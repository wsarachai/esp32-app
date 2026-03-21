#include <stdio.h>
#include "driver/gpio.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_log.h"

// DHT22 sensor pin (GPIO 32)
#define DHT_PIN GPIO_NUM_32
#define DHT_RESPONSE_TIMEOUT_US 200
#define DHT_BIT_TIMEOUT_US 150
#define DHT_START_SIGNAL_MS 18
#define DHT_RELEASE_TIME_US 30
#define DHT_SAMPLE_TIME_US 45
#define DHT_DATA_BITS 40
#define DHT_READ_RETRIES 3
#define DHT_RETRY_DELAY_MS 20

static const char *TAG = "DHT22";
static portMUX_TYPE s_dht_lock = portMUX_INITIALIZER_UNLOCKED;

typedef enum
{
    DHT_READ_OK = 0,
    DHT_READ_NO_RESPONSE_LOW,
    DHT_READ_NO_RESPONSE_HIGH,
    DHT_READ_NO_DATA_START_LOW,
    DHT_READ_BIT_START_TIMEOUT,
    DHT_READ_BIT_END_TIMEOUT,
    DHT_READ_CHECKSUM_MISMATCH,
} dht_read_status_t;

typedef struct
{
    dht_read_status_t status;
    int failed_bit;
    uint8_t data[5];
    uint8_t checksum_calc;
} dht_read_failure_t;

static const char *dht_status_to_string(dht_read_status_t status)
{
    switch (status)
    {
    case DHT_READ_OK:
        return "ok";
    case DHT_READ_NO_RESPONSE_LOW:
        return "no response low";
    case DHT_READ_NO_RESPONSE_HIGH:
        return "no response high";
    case DHT_READ_NO_DATA_START_LOW:
        return "no data start low";
    case DHT_READ_BIT_START_TIMEOUT:
        return "bit start timeout";
    case DHT_READ_BIT_END_TIMEOUT:
        return "bit end timeout";
    case DHT_READ_CHECKSUM_MISMATCH:
        return "checksum mismatch";
    default:
        return "unknown";
    }
}

static bool dht_wait_while_level(int level, int timeout_us)
{
    while (gpio_get_level(DHT_PIN) == level && timeout_us-- > 0)
    {
        esp_rom_delay_us(1);
    }

    return timeout_us > 0;
}

/**
 * Read 8 bits from DHT22
 * Returns false if timeout, true if success
 */
static dht_read_status_t dht_read_bits(uint8_t *bits, int num_bits, int *failed_bit)
{
    for (int i = 0; i < num_bits; i++)
    {
        // Wait for LOW -> HIGH transition (start of bit)
        if (!dht_wait_while_level(0, DHT_BIT_TIMEOUT_US))
        {
            *failed_bit = i;
            return DHT_READ_BIT_START_TIMEOUT;
        }

        // Sample after the 0-bit pulse width but before a 1-bit pulse ends.
        esp_rom_delay_us(DHT_SAMPLE_TIME_US);
        uint8_t bit = gpio_get_level(DHT_PIN);
        bits[i / 8] <<= 1;
        bits[i / 8] |= bit;

        // After the last bit, the bus may stay HIGH (idle) instead of
        // producing another LOW phase, so do not require a falling edge.
        if (i == (num_bits - 1))
        {
            continue;
        }

        // Wait for HIGH -> LOW transition (end of bit)
        if (!dht_wait_while_level(1, DHT_BIT_TIMEOUT_US))
        {
            *failed_bit = i;
            return DHT_READ_BIT_END_TIMEOUT;
        }
    }

    return DHT_READ_OK;
}

/**
 * Read temperature and humidity from DHT22
 * Returns true if successful
 */
static bool dht22_read_once(float *humidity, float *temperature, dht_read_failure_t *failure)
{
    if (humidity == NULL || temperature == NULL)
    {
        return false;
    }

    if (failure != NULL)
    {
        failure->status = DHT_READ_OK;
        failure->failed_bit = -1;
        failure->checksum_calc = 0;
        for (int i = 0; i < 5; ++i)
        {
            failure->data[i] = 0;
        }
    }

    uint8_t data[5] = {0};

    // Prepare GPIO as open-drain output.
    gpio_config_t io_conf = {
        .pin_bit_mask = (1ULL << DHT_PIN),
        .mode = GPIO_MODE_OUTPUT_OD,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&io_conf);

    // Send start signal: pull LOW for 18ms
    gpio_set_level(DHT_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(DHT_START_SIGNAL_MS));
    gpio_set_level(DHT_PIN, 1);
    esp_rom_delay_us(DHT_RELEASE_TIME_US);

    // Switch to input mode to read
    gpio_set_direction(DHT_PIN, GPIO_MODE_INPUT);
    gpio_set_pull_mode(DHT_PIN, GPIO_PULLUP_ONLY);

    dht_read_status_t status = DHT_READ_OK;
    int failed_bit = -1;

    // The DHT22 waveform is short enough that a brief critical section
    // prevents scheduler and interrupt jitter from corrupting bit timings.
    portENTER_CRITICAL(&s_dht_lock);

    // Wait for DHT22 to respond (LOW pulse)
    if (!dht_wait_while_level(1, DHT_RESPONSE_TIMEOUT_US))
    {
        status = DHT_READ_NO_RESPONSE_LOW;
    }
    // Wait for response HIGH pulse
    else if (!dht_wait_while_level(0, DHT_RESPONSE_TIMEOUT_US))
    {
        status = DHT_READ_NO_RESPONSE_HIGH;
    }
    // Wait for data transmission to start
    else if (!dht_wait_while_level(1, DHT_RESPONSE_TIMEOUT_US))
    {
        status = DHT_READ_NO_DATA_START_LOW;
    }
    else
    {
        status = dht_read_bits(data, DHT_DATA_BITS, &failed_bit);
    }

    portEXIT_CRITICAL(&s_dht_lock);

    if (status != DHT_READ_OK)
    {
        if (failure != NULL)
        {
            failure->status = status;
            failure->failed_bit = failed_bit;
        }
        return false;
    }

    // Verify checksum
    uint8_t checksum = (data[0] + data[1] + data[2] + data[3]) & 0xFF;
    if (checksum != data[4])
    {
        if (failure != NULL)
        {
            failure->status = DHT_READ_CHECKSUM_MISMATCH;
            failure->checksum_calc = checksum;
            for (int i = 0; i < 5; ++i)
            {
                failure->data[i] = data[i];
            }
        }
        return false;
    }

    // Extract humidity (16-bit, high byte first)
    *humidity = ((data[0] << 8) | data[1]) / 10.0f;

    // Extract temperature (16-bit, high byte first, with sign bit)
    int16_t temp_raw = ((data[2] & 0x7F) << 8) | data[3];
    if (data[2] & 0x80)
    {
        temp_raw = -temp_raw; // Negative temperature
    }
    *temperature = temp_raw / 10.0f;

    return true;
}

bool dht22_read(float *humidity, float *temperature)
{
    if (humidity == NULL || temperature == NULL)
    {
        return false;
    }

    dht_read_failure_t last_failure = {
        .status = DHT_READ_OK,
        .failed_bit = -1,
        .data = {0},
        .checksum_calc = 0,
    };

    for (int attempt = 1; attempt <= DHT_READ_RETRIES; ++attempt)
    {
        if (dht22_read_once(humidity, temperature, &last_failure))
        {
            if (attempt > 1)
            {
                ESP_LOGI(TAG, "Read recovered on retry %d/%d", attempt, DHT_READ_RETRIES);
            }
            return true;
        }

        if (attempt < DHT_READ_RETRIES)
        {
            vTaskDelay(pdMS_TO_TICKS(DHT_RETRY_DELAY_MS));
        }
    }

    if (last_failure.status == DHT_READ_CHECKSUM_MISMATCH)
    {
        ESP_LOGW(TAG,
                 "Read failed after %d attempts: %s sensor=%u calc=%u raw=[%u,%u,%u,%u,%u]",
                 DHT_READ_RETRIES,
                 dht_status_to_string(last_failure.status),
                 last_failure.data[4],
                 last_failure.checksum_calc,
                 last_failure.data[0],
                 last_failure.data[1],
                 last_failure.data[2],
                 last_failure.data[3],
                 last_failure.data[4]);
    }
    else if (last_failure.status == DHT_READ_BIT_START_TIMEOUT ||
             last_failure.status == DHT_READ_BIT_END_TIMEOUT)
    {
        ESP_LOGW(TAG,
                 "Read failed after %d attempts: %s at bit %d",
                 DHT_READ_RETRIES,
                 dht_status_to_string(last_failure.status),
                 last_failure.failed_bit);
    }
    else
    {
        ESP_LOGW(TAG,
                 "Read failed after %d attempts: %s",
                 DHT_READ_RETRIES,
                 dht_status_to_string(last_failure.status));
    }

    return false;
}
