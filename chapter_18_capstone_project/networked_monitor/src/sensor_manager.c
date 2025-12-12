#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/sensor.h>
#include <zephyr/logging/log.h>
#include "sensor_manager.h"

LOG_MODULE_REGISTER(sensor_manager, LOG_LEVEL_INF);

extern struct sensor_reading latest_reading;
extern struct k_mutex data_mutex;

static const struct device *const bme280 = DEVICE_DT_GET_ANY(bosch_bme280);

void sensor_thread(void)
{
    struct sensor_reading data;
    int ret;

    if (!device_is_ready(bme280)) {
        LOG_ERR("BME280 device not ready");
        return;
    }

    LOG_INF("Sensor thread started, polling every 5 seconds");

    while (1) {
        /* Fetch sensor sample */
        ret = sensor_sample_fetch(bme280);
        if (ret < 0) {
            LOG_ERR("Failed to fetch sensor sample: %d", ret);
            k_sleep(K_SECONDS(5));
            continue;
        }

        /* Get temperature reading */
        ret = sensor_channel_get(bme280, SENSOR_CHAN_AMBIENT_TEMP, &data.temp);
        if (ret < 0) {
            LOG_ERR("Failed to get temperature: %d", ret);
            k_sleep(K_SECONDS(5));
            continue;
        }

        /* Get pressure reading */
        ret = sensor_channel_get(bme280, SENSOR_CHAN_PRESS, &data.press);
        if (ret < 0) {
            LOG_ERR("Failed to get pressure: %d", ret);
            k_sleep(K_SECONDS(5));
            continue;
        }

        /* Get humidity reading */
        ret = sensor_channel_get(bme280, SENSOR_CHAN_HUMIDITY, &data.humidity);
        if (ret < 0) {
            LOG_ERR("Failed to get humidity: %d", ret);
            k_sleep(K_SECONDS(5));
            continue;
        }

        /* Update shared data with mutex protection */
        ret = k_mutex_lock(&data_mutex, K_MSEC(500));
        if (ret == 0) {
            latest_reading = data;
            k_mutex_unlock(&data_mutex);
            
            LOG_DBG("Sensor readings updated - Temp: %d.%06d C, Press: %d.%06d kPa, Humidity: %d.%06d %%",
                    data.temp.val1, data.temp.val2,
                    data.press.val1, data.press.val2,
                    data.humidity.val1, data.humidity.val2);
        } else {
            LOG_WRN("Failed to acquire mutex, skipping data update");
        }

        k_sleep(K_SECONDS(5));
    }
}
