#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "driver/i2c.h"
#include "esp_check.h"
#include "esp_err.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#define I2C_PORT I2C_NUM_0
#define I2C_SDA_IO 21
#define I2C_SCL_IO 22
#define I2C_FREQ_HZ 100000

#define DS3231_ADDR    0x68
#define AT24C32_ADDR   0x57  /* HW-111: A0/A1/A2 pulled high */

#define DS3231_REG_SECONDS 0x00
#define DS3231_REG_MINUTES 0x01
#define DS3231_REG_HOURS 0x02
#define DS3231_REG_DAY 0x03
#define DS3231_REG_DATE 0x04
#define DS3231_REG_MONTH_CENTURY 0x05
#define DS3231_REG_YEAR 0x06
#define DS3231_REG_ALARM1 0x07
#define DS3231_REG_ALARM2 0x0B
#define DS3231_REG_CONTROL 0x0E
#define DS3231_REG_STATUS 0x0F
#define DS3231_REG_AGING_OFFSET 0x10
#define DS3231_REG_TEMP_MSB 0x11
#define DS3231_REG_TEMP_LSB 0x12

static const char *TAG = "DS3231_TEST";

typedef struct {
	uint8_t second;
	uint8_t minute;
	uint8_t hour;
	uint8_t day_of_week;
	uint8_t day_of_month;
	uint8_t month;
	uint16_t year;
} ds3231_datetime_t;

typedef enum {
	DS3231_SQW_1HZ = 0,
	DS3231_SQW_1024HZ,
	DS3231_SQW_4096HZ,
	DS3231_SQW_8192HZ,
} ds3231_sqw_freq_t;

static uint8_t dec_to_bcd(uint8_t val)
{
	return (uint8_t)(((val / 10U) << 4U) | (val % 10U));
}

static uint8_t bcd_to_dec(uint8_t val)
{
	return (uint8_t)(((val >> 4U) * 10U) + (val & 0x0FU));
}

static esp_err_t i2c_init(void)
{
	i2c_config_t cfg = {
		.mode = I2C_MODE_MASTER,
		.sda_io_num = I2C_SDA_IO,
		.scl_io_num = I2C_SCL_IO,
		.sda_pullup_en = GPIO_PULLUP_ENABLE,
		.scl_pullup_en = GPIO_PULLUP_ENABLE,
		.master.clk_speed = I2C_FREQ_HZ,
	};

	ESP_ERROR_CHECK(i2c_param_config(I2C_PORT, &cfg));
	return i2c_driver_install(I2C_PORT, cfg.mode, 0, 0, 0);
}

static esp_err_t ds3231_write_bytes(uint8_t reg, const uint8_t *data, size_t len)
{
	uint8_t buffer[16];
	if (len > (sizeof(buffer) - 1U)) {
		return ESP_ERR_INVALID_SIZE;
	}

	buffer[0] = reg;
	memcpy(&buffer[1], data, len);
	return i2c_master_write_to_device(I2C_PORT, DS3231_ADDR, buffer, len + 1U, pdMS_TO_TICKS(100));
}

static esp_err_t ds3231_read_bytes(uint8_t reg, uint8_t *data, size_t len)
{
	return i2c_master_write_read_device(I2C_PORT, DS3231_ADDR, &reg, 1, data, len, pdMS_TO_TICKS(100));
}

static esp_err_t ds3231_read_u8(uint8_t reg, uint8_t *value)
{
	return ds3231_read_bytes(reg, value, 1);
}

static esp_err_t ds3231_write_u8(uint8_t reg, uint8_t value)
{
	return ds3231_write_bytes(reg, &value, 1);
}

static esp_err_t ds3231_update_bits(uint8_t reg, uint8_t mask, uint8_t value)
{
	uint8_t current = 0;
	ESP_RETURN_ON_ERROR(ds3231_read_u8(reg, &current), TAG, "read reg 0x%02X failed", reg);
	current = (uint8_t)((current & (uint8_t)(~mask)) | (value & mask));
	return ds3231_write_u8(reg, current);
}

static esp_err_t ds3231_set_datetime(const ds3231_datetime_t *dt)
{
	if (dt == NULL || dt->year < 2000 || dt->year > 2199 || dt->month < 1 || dt->month > 12 || dt->day_of_month < 1 ||
		dt->day_of_month > 31 || dt->hour > 23 || dt->minute > 59 || dt->second > 59 || dt->day_of_week < 1 ||
		dt->day_of_week > 7) {
		return ESP_ERR_INVALID_ARG;
	}

	uint8_t year_low = (uint8_t)(dt->year % 100U);
	uint8_t century_bit = (dt->year >= 2100) ? 0x80U : 0x00U;

	uint8_t raw[7] = {
		dec_to_bcd(dt->second),
		dec_to_bcd(dt->minute),
		dec_to_bcd(dt->hour),
		dec_to_bcd(dt->day_of_week),
		dec_to_bcd(dt->day_of_month),
		(uint8_t)(dec_to_bcd(dt->month) | century_bit),
		dec_to_bcd(year_low),
	};

	return ds3231_write_bytes(DS3231_REG_SECONDS, raw, sizeof(raw));
}

static esp_err_t ds3231_get_datetime(ds3231_datetime_t *dt)
{
	if (dt == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	uint8_t raw[7] = {0};
	ESP_RETURN_ON_ERROR(ds3231_read_bytes(DS3231_REG_SECONDS, raw, sizeof(raw)), TAG, "read datetime failed");

	dt->second = bcd_to_dec((uint8_t)(raw[0] & 0x7FU));
	dt->minute = bcd_to_dec((uint8_t)(raw[1] & 0x7FU));
	dt->hour = bcd_to_dec((uint8_t)(raw[2] & 0x3FU));
	dt->day_of_week = bcd_to_dec((uint8_t)(raw[3] & 0x07U));
	dt->day_of_month = bcd_to_dec((uint8_t)(raw[4] & 0x3FU));
	dt->month = bcd_to_dec((uint8_t)(raw[5] & 0x1FU));

	uint16_t century = (raw[5] & 0x80U) ? 2100U : 2000U;
	dt->year = (uint16_t)(century + bcd_to_dec(raw[6]));

	return ESP_OK;
}

static esp_err_t ds3231_get_temperature(float *temp_c)
{
	if (temp_c == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	uint8_t raw[2] = {0};
	ESP_RETURN_ON_ERROR(ds3231_read_bytes(DS3231_REG_TEMP_MSB, raw, sizeof(raw)), TAG, "read temp failed");

	int8_t msb = (int8_t)raw[0];
	uint8_t frac = (uint8_t)((raw[1] >> 6U) & 0x03U);
	*temp_c = (float)msb + ((float)frac * 0.25f);
	return ESP_OK;
}

static esp_err_t ds3231_force_temperature_conversion(void)
{
	ESP_RETURN_ON_ERROR(ds3231_update_bits(DS3231_REG_CONTROL, 0x20U, 0x20U), TAG, "set CONV bit failed");

	for (int i = 0; i < 10; i++) {
		uint8_t status = 0;
		ESP_RETURN_ON_ERROR(ds3231_read_u8(DS3231_REG_STATUS, &status), TAG, "read status failed");
		if ((status & 0x04U) == 0U) {
			return ESP_OK;
		}
		vTaskDelay(pdMS_TO_TICKS(50));
	}

	return ESP_ERR_TIMEOUT;
}

static esp_err_t ds3231_set_aging_offset(int8_t offset)
{
	return ds3231_write_u8(DS3231_REG_AGING_OFFSET, (uint8_t)offset);
}

static esp_err_t ds3231_get_aging_offset(int8_t *offset)
{
	if (offset == NULL) {
		return ESP_ERR_INVALID_ARG;
	}

	uint8_t raw = 0;
	ESP_RETURN_ON_ERROR(ds3231_read_u8(DS3231_REG_AGING_OFFSET, &raw), TAG, "read aging failed");
	*offset = (int8_t)raw;
	return ESP_OK;
}

static esp_err_t ds3231_set_32khz_output(bool enable)
{
	return ds3231_update_bits(DS3231_REG_STATUS, 0x08U, enable ? 0x08U : 0x00U);
}

static esp_err_t ds3231_clear_oscillator_stop_flag(void)
{
	return ds3231_update_bits(DS3231_REG_STATUS, 0x80U, 0x00U);
}

static esp_err_t ds3231_enable_square_wave(ds3231_sqw_freq_t freq)
{
	uint8_t rs = 0;
	switch (freq) {
	case DS3231_SQW_1HZ:
		rs = 0x00U;
		break;
	case DS3231_SQW_1024HZ:
		rs = 0x08U;
		break;
	case DS3231_SQW_4096HZ:
		rs = 0x10U;
		break;
	case DS3231_SQW_8192HZ:
		rs = 0x18U;
		break;
	default:
		return ESP_ERR_INVALID_ARG;
	}

	return ds3231_update_bits(DS3231_REG_CONTROL, 0x1CU, rs);
}

static esp_err_t ds3231_set_alarm1_example(void)
{
	uint8_t a1[4] = {
		dec_to_bcd(30),
		dec_to_bcd(0),
		dec_to_bcd(0),
		dec_to_bcd(1),
	};
	ESP_RETURN_ON_ERROR(ds3231_write_bytes(DS3231_REG_ALARM1, a1, sizeof(a1)), TAG, "write alarm1 failed");

	ESP_RETURN_ON_ERROR(ds3231_update_bits(DS3231_REG_STATUS, 0x01U, 0x00U), TAG, "clear A1F failed");
	ESP_RETURN_ON_ERROR(ds3231_update_bits(DS3231_REG_CONTROL, 0x04U | 0x01U, 0x04U | 0x01U), TAG,
						"enable A1 interrupt failed");
	return ESP_OK;
}

static esp_err_t ds3231_set_alarm2_example(void)
{
	uint8_t a2[3] = {
		dec_to_bcd(1),
		dec_to_bcd(0),
		dec_to_bcd(1),
	};
	ESP_RETURN_ON_ERROR(ds3231_write_bytes(DS3231_REG_ALARM2, a2, sizeof(a2)), TAG, "write alarm2 failed");

	ESP_RETURN_ON_ERROR(ds3231_update_bits(DS3231_REG_STATUS, 0x02U, 0x00U), TAG, "clear A2F failed");
	ESP_RETURN_ON_ERROR(ds3231_update_bits(DS3231_REG_CONTROL, 0x04U | 0x02U, 0x04U | 0x02U), TAG,
						"enable A2 interrupt failed");
	return ESP_OK;
}

static esp_err_t ds3231_disable_alarm_interrupts(void)
{
	return ds3231_update_bits(DS3231_REG_CONTROL, 0x03U, 0x00U);
}

static esp_err_t ds3231_probe(void)
{
	uint8_t value = 0;
	return ds3231_read_u8(DS3231_REG_STATUS, &value);
}

static void i2c_scan(void)
{
	ESP_LOGI(TAG, "Scanning I2C bus (SDA=%d SCL=%d)...", I2C_SDA_IO, I2C_SCL_IO);
	int found = 0;
	for (uint8_t addr = 0x08; addr <= 0x77; addr++) {
		i2c_cmd_handle_t cmd = i2c_cmd_link_create();
		i2c_master_start(cmd);
		i2c_master_write_byte(cmd, (uint8_t)(addr << 1) | I2C_MASTER_WRITE, true);
		i2c_master_stop(cmd);
		esp_err_t ret = i2c_master_cmd_begin(I2C_PORT, cmd, pdMS_TO_TICKS(50));
		i2c_cmd_link_delete(cmd);
		if (ret == ESP_OK) {
			ESP_LOGI(TAG, "  Found device at 0x%02X%s", addr,
					 addr == DS3231_ADDR ? "  <-- DS3231" : "");
			found++;
		}
	}
	if (found == 0) {
		ESP_LOGW(TAG, "  No I2C devices found. Check wiring and pull-ups.");
	} else {
		ESP_LOGI(TAG, "  Scan complete: %d device(s) found.", found);
	}
}

static void log_datetime(const ds3231_datetime_t *dt)
{
	ESP_LOGI(TAG, "RTC: %04u-%02u-%02u (DOW=%u) %02u:%02u:%02u", dt->year, dt->month, dt->day_of_month,
			 dt->day_of_week, dt->hour, dt->minute, dt->second);
}

static esp_err_t run_ds3231_tests(void)
{
	esp_err_t err;

	ESP_LOGI(TAG, "----- DS3231 test sequence start -----");

	uint8_t control = 0;
	uint8_t status = 0;
	err = ds3231_read_u8(DS3231_REG_CONTROL, &control);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Read CONTROL failed: %s", esp_err_to_name(err));
		return err;
	}
	err = ds3231_read_u8(DS3231_REG_STATUS, &status);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Read STATUS failed: %s", esp_err_to_name(err));
		return err;
	}
	ESP_LOGI(TAG, "Initial control=0x%02X status=0x%02X", control, status);

	err = ds3231_update_bits(DS3231_REG_CONTROL, 0x80U, 0x00U);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Disable EOSC failed: %s", esp_err_to_name(err));
		return err;
	}
	err = ds3231_clear_oscillator_stop_flag();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Clear OSF failed: %s", esp_err_to_name(err));
		return err;
	}

	ds3231_datetime_t set_dt = {
		.second = 0,
		.minute = 0,
		.hour = 12,
		.day_of_week = 5,
		.day_of_month = 20,
		.month = 3,
		.year = 2026,
	};
	err = ds3231_set_datetime(&set_dt);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Set datetime failed: %s", esp_err_to_name(err));
		return err;
	}

	ds3231_datetime_t read_dt = {0};
	err = ds3231_get_datetime(&read_dt);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Read datetime failed: %s", esp_err_to_name(err));
		return err;
	}
	ESP_LOGI(TAG, "Time set/get test complete");
	log_datetime(&read_dt);

	err = ds3231_force_temperature_conversion();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Force temp conversion failed: %s", esp_err_to_name(err));
		return err;
	}
	float temp_c = 0.0f;
	err = ds3231_get_temperature(&temp_c);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Read temperature failed: %s", esp_err_to_name(err));
		return err;
	}
	ESP_LOGI(TAG, "Temperature test: %.2f C", temp_c);

	ESP_LOGI(TAG, "Square-wave test: 1Hz, 1024Hz, 4096Hz, 8192Hz (observe SQW pin)");
	err = ds3231_enable_square_wave(DS3231_SQW_1HZ);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Set SQW 1Hz failed: %s", esp_err_to_name(err));
		return err;
	}
	vTaskDelay(pdMS_TO_TICKS(1000));
	err = ds3231_enable_square_wave(DS3231_SQW_1024HZ);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Set SQW 1024Hz failed: %s", esp_err_to_name(err));
		return err;
	}
	vTaskDelay(pdMS_TO_TICKS(500));
	err = ds3231_enable_square_wave(DS3231_SQW_4096HZ);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Set SQW 4096Hz failed: %s", esp_err_to_name(err));
		return err;
	}
	vTaskDelay(pdMS_TO_TICKS(500));
	err = ds3231_enable_square_wave(DS3231_SQW_8192HZ);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Set SQW 8192Hz failed: %s", esp_err_to_name(err));
		return err;
	}
	vTaskDelay(pdMS_TO_TICKS(500));

	err = ds3231_set_alarm1_example();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Set Alarm1 failed: %s", esp_err_to_name(err));
		return err;
	}
	err = ds3231_set_alarm2_example();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Set Alarm2 failed: %s", esp_err_to_name(err));
		return err;
	}
	ESP_LOGI(TAG, "Alarm1/Alarm2 configured; observe INT/SQW pin and status flags.");

	int8_t original_aging = 0;
	err = ds3231_get_aging_offset(&original_aging);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Read aging offset failed: %s", esp_err_to_name(err));
		return err;
	}
	ESP_LOGI(TAG, "Aging offset read test: %d", original_aging);

	err = ds3231_set_aging_offset(5);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Write aging offset failed: %s", esp_err_to_name(err));
		return err;
	}
	int8_t new_aging = 0;
	err = ds3231_get_aging_offset(&new_aging);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Re-read aging offset failed: %s", esp_err_to_name(err));
		return err;
	}
	ESP_LOGI(TAG, "Aging offset write/read test: %d", new_aging);
	err = ds3231_set_aging_offset(original_aging);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Restore aging offset failed: %s", esp_err_to_name(err));
		return err;
	}

	err = ds3231_set_32khz_output(true);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Enable 32kHz output failed: %s", esp_err_to_name(err));
		return err;
	}
	ESP_LOGI(TAG, "32kHz output enabled (observe 32kHz pin)");
	vTaskDelay(pdMS_TO_TICKS(1000));
	err = ds3231_set_32khz_output(false);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Disable 32kHz output failed: %s", esp_err_to_name(err));
		return err;
	}
	ESP_LOGI(TAG, "32kHz output disabled");

	err = ds3231_disable_alarm_interrupts();
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "Disable alarm interrupts failed: %s", esp_err_to_name(err));
		return err;
	}
	ESP_LOGI(TAG, "Alarm interrupts disabled; test sequence completed.");

	ESP_LOGI(TAG, "----- DS3231 test sequence end -----");
	return ESP_OK;
}

/* ---------------------------------------------------------------------------
 * AT24C32 EEPROM helpers (HW-111 on-board EEPROM, address 0x57)
 * AT24C32 uses 2-byte word address before data.
 * ---------------------------------------------------------------------------*/
static esp_err_t at24c32_write_byte(uint16_t word_addr, uint8_t data)
{
	uint8_t buf[3] = {
		(uint8_t)(word_addr >> 8),
		(uint8_t)(word_addr & 0xFFU),
		data,
	};
	esp_err_t err = i2c_master_write_to_device(I2C_PORT, AT24C32_ADDR, buf, sizeof(buf), pdMS_TO_TICKS(100));
	if (err == ESP_OK) {
		vTaskDelay(pdMS_TO_TICKS(10));  /* EEPROM write cycle time */
	}
	return err;
}

static esp_err_t at24c32_read_byte(uint16_t word_addr, uint8_t *data)
{
	uint8_t addr_buf[2] = {
		(uint8_t)(word_addr >> 8),
		(uint8_t)(word_addr & 0xFFU),
	};
	return i2c_master_write_read_device(I2C_PORT, AT24C32_ADDR, addr_buf, sizeof(addr_buf), data, 1,
										pdMS_TO_TICKS(100));
}

static void run_at24c32_tests(void)
{
	ESP_LOGI(TAG, "----- AT24C32 EEPROM test (HW-111) -----");

	/* probe */
	uint8_t probe_val = 0;
	esp_err_t err = at24c32_read_byte(0x0000, &probe_val);
	if (err != ESP_OK) {
		ESP_LOGW(TAG, "AT24C32 not found at 0x%02X: %s", AT24C32_ADDR, esp_err_to_name(err));
		return;
	}
	ESP_LOGI(TAG, "AT24C32 detected at 0x%02X", AT24C32_ADDR);

	/* write/read-back test at address 0x0010 */
	const uint16_t test_addr = 0x0010;
	const uint8_t  test_val  = 0xA5;
	uint8_t original = 0;

	err = at24c32_read_byte(test_addr, &original);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "EEPROM read failed: %s", esp_err_to_name(err));
		return;
	}
	ESP_LOGI(TAG, "EEPROM addr 0x%04X original value: 0x%02X", test_addr, original);

	err = at24c32_write_byte(test_addr, test_val);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "EEPROM write failed: %s", esp_err_to_name(err));
		return;
	}

	uint8_t readback = 0;
	err = at24c32_read_byte(test_addr, &readback);
	if (err != ESP_OK) {
		ESP_LOGE(TAG, "EEPROM read-back failed: %s", esp_err_to_name(err));
		return;
	}

	if (readback == test_val) {
		ESP_LOGI(TAG, "EEPROM write/read PASS: wrote 0x%02X, read 0x%02X", test_val, readback);
	} else {
		ESP_LOGE(TAG, "EEPROM write/read FAIL: wrote 0x%02X, read 0x%02X", test_val, readback);
	}

	/* restore original value */
	at24c32_write_byte(test_addr, original);
	ESP_LOGI(TAG, "EEPROM original value restored.");
	ESP_LOGI(TAG, "----- AT24C32 EEPROM test end -----");
}

void app_main(void)
{
	ESP_ERROR_CHECK(i2c_init());
	i2c_scan();

	bool ds3231_ready = false;
	esp_err_t err = ds3231_probe();
	if (err == ESP_OK) {
		err = run_ds3231_tests();
		ds3231_ready = (err == ESP_OK);
	} else {
		ESP_LOGW(TAG, "DS3231 probe failed: %s", esp_err_to_name(err));
	}

	run_at24c32_tests();

	if (!ds3231_ready) {
		ESP_LOGW(TAG, "RTC not ready. Check wiring and pull-ups: SDA=%d SCL=%d, address=0x%02X", I2C_SDA_IO, I2C_SCL_IO,
				 DS3231_ADDR);
	}

	while (true) {
		if (!ds3231_ready) {
			err = ds3231_probe();
			if (err == ESP_OK) {
				ESP_LOGI(TAG, "DS3231 detected, running tests...");
				err = run_ds3231_tests();
				ds3231_ready = (err == ESP_OK);
			}
			if (!ds3231_ready) {
				ESP_LOGW(TAG, "Waiting for DS3231... (%s)", esp_err_to_name(err));
			}
		} else {
			ds3231_datetime_t dt = {0};
			err = ds3231_get_datetime(&dt);
			if (err == ESP_OK) {
				log_datetime(&dt);
			} else {
				ds3231_ready = false;
				ESP_LOGW(TAG, "Lost DS3231 communication: %s", esp_err_to_name(err));
			}
		}
		vTaskDelay(pdMS_TO_TICKS(2000));
	}
}