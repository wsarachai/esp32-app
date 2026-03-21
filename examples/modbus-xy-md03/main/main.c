#include <stdio.h>
#include <stdint.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "XY_MD03";

/*
 * XY-MD03 defaults (common for many modules):
 * - Slave address: 1
 * - Function: 0x03 (Read Holding Registers)
 * - Register 0x0001: Temperature (x10, signed)
 * - Register 0x0002: Humidity (x10)
 *
 * If your module is configured differently, change the constants below.
 */
#define MB_SLAVE_ADDR              0x01
#define MB_FUNC_READ_HOLDING       0x03

#define MB_REG_TEMPERATURE         0x0001
#define MB_REG_HUMIDITY            0x0002

#define MB_UART_PORT               UART_NUM_2
#define MB_UART_BAUD_RATE          9600
#define MB_UART_TXD_PIN            GPIO_NUM_17
#define MB_UART_RXD_PIN            GPIO_NUM_16
#define MB_UART_RTS_PIN            GPIO_NUM_4

#define MB_RX_BUF_SIZE             256
#define MB_POLL_PERIOD_MS          2000
#define MB_RESPONSE_TIMEOUT_MS     300

static uint16_t modbus_crc16(const uint8_t *data, size_t len)
{
	uint16_t crc = 0xFFFF;

	for (size_t pos = 0; pos < len; pos++) {
		crc ^= data[pos];
		for (int i = 0; i < 8; i++) {
			if (crc & 0x0001) {
				crc >>= 1;
				crc ^= 0xA001;
			} else {
				crc >>= 1;
			}
		}
	}

	return crc;
}

static esp_err_t modbus_read_holding_registers(uint8_t slave,
											   uint16_t start_reg,
											   uint16_t quantity,
											   uint16_t *out_regs)
{
	uint8_t request[8];
	uint8_t response[64];

	if (quantity == 0 || out_regs == NULL || quantity > 20) {
		return ESP_ERR_INVALID_ARG;
	}

	request[0] = slave;
	request[1] = MB_FUNC_READ_HOLDING;
	request[2] = (uint8_t)(start_reg >> 8);
	request[3] = (uint8_t)(start_reg & 0xFF);
	request[4] = (uint8_t)(quantity >> 8);
	request[5] = (uint8_t)(quantity & 0xFF);

	uint16_t crc = modbus_crc16(request, 6);
	request[6] = (uint8_t)(crc & 0xFF);       // CRC low byte first
	request[7] = (uint8_t)((crc >> 8) & 0xFF);

	uart_flush_input(MB_UART_PORT);
	uart_write_bytes(MB_UART_PORT, (const char *)request, sizeof(request));

	// Expected response length: addr + func + byte_count + data + crc(2)
	const int expected_len = 3 + (quantity * 2) + 2;
	int rx_len = uart_read_bytes(MB_UART_PORT,
								 response,
								 expected_len,
								 pdMS_TO_TICKS(MB_RESPONSE_TIMEOUT_MS));

	if (rx_len < expected_len) {
		ESP_LOGW(TAG, "Timeout/short response: got %d, expected %d", rx_len, expected_len);
		return ESP_ERR_TIMEOUT;
	}

	if (response[0] != slave) {
		ESP_LOGW(TAG, "Unexpected slave address in response: 0x%02X", response[0]);
		return ESP_FAIL;
	}

	if (response[1] == (MB_FUNC_READ_HOLDING | 0x80)) {
		ESP_LOGE(TAG, "Modbus exception code: 0x%02X", response[2]);
		return ESP_FAIL;
	}

	if (response[1] != MB_FUNC_READ_HOLDING) {
		ESP_LOGW(TAG, "Unexpected function in response: 0x%02X", response[1]);
		return ESP_FAIL;
	}

	if (response[2] != (quantity * 2)) {
		ESP_LOGW(TAG, "Unexpected byte count: %u", response[2]);
		return ESP_FAIL;
	}

	uint16_t crc_rx = (uint16_t)response[expected_len - 2] |
					  ((uint16_t)response[expected_len - 1] << 8);
	uint16_t crc_calc = modbus_crc16(response, expected_len - 2);

	if (crc_rx != crc_calc) {
		ESP_LOGW(TAG, "CRC mismatch: rx=0x%04X calc=0x%04X", crc_rx, crc_calc);
		return ESP_FAIL;
	}

	for (uint16_t i = 0; i < quantity; i++) {
		uint8_t hi = response[3 + (i * 2)];
		uint8_t lo = response[4 + (i * 2)];
		out_regs[i] = (uint16_t)((hi << 8) | lo);
	}

	return ESP_OK;
}

static void rs485_uart_init(void)
{
	const uart_config_t uart_config = {
		.baud_rate = MB_UART_BAUD_RATE,
		.data_bits = UART_DATA_8_BITS,
		.parity = UART_PARITY_DISABLE,
		.stop_bits = UART_STOP_BITS_1,
		.flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
		.source_clk = UART_SCLK_DEFAULT,
	};

	ESP_ERROR_CHECK(uart_driver_install(MB_UART_PORT, MB_RX_BUF_SIZE, 0, 0, NULL, 0));
	ESP_ERROR_CHECK(uart_param_config(MB_UART_PORT, &uart_config));
	ESP_ERROR_CHECK(uart_set_pin(MB_UART_PORT,
								 MB_UART_TXD_PIN,
								 MB_UART_RXD_PIN,
								 MB_UART_RTS_PIN,
								 UART_PIN_NO_CHANGE));

	// In half-duplex mode, RTS controls the transceiver DE/RE pin.
	ESP_ERROR_CHECK(uart_set_mode(MB_UART_PORT, UART_MODE_RS485_HALF_DUPLEX));

	ESP_LOGI(TAG,
			 "RS485 UART ready: port=%d, baud=%d, TX=%d, RX=%d, RTS(DE/RE)=%d",
			 MB_UART_PORT,
			 MB_UART_BAUD_RATE,
			 MB_UART_TXD_PIN,
			 MB_UART_RXD_PIN,
			 MB_UART_RTS_PIN);
}

void app_main(void)
{
	rs485_uart_init();

	while (1) {
		uint16_t regs[2] = {0};
		esp_err_t err = modbus_read_holding_registers(MB_SLAVE_ADDR,
													  MB_REG_TEMPERATURE,
													  2,
													  regs);

		if (err == ESP_OK) {
			int16_t temp_raw = (int16_t)regs[0];
			uint16_t hum_raw = regs[1];

			float temp_c = (float)temp_raw / 10.0f;
			float hum_rh = (float)hum_raw / 10.0f;

			ESP_LOGI(TAG,
					 "Temperature: %.1f C, Humidity: %.1f %%RH",
					 (double)temp_c,
					 (double)hum_rh);
		} else {
			ESP_LOGW(TAG, "Read failed: %s", esp_err_to_name(err));
		}

		vTaskDelay(pdMS_TO_TICKS(MB_POLL_PERIOD_MS));
	}
}