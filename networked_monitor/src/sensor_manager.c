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

    if (!device_is_ready(bme280)) {
        LOG_ERR("BME280 device not ready");
        return;
    }

    while (1) {
        sensor_sample_fetch(bme280);
        sensor_channel_get(bme280, SENSOR_CHAN_AMBIENT_TEMP, &data.temp);
        sensor_channel_get(bme280, SENSOR_CHAN_PRESS, &data.press);
        sensor_channel_get(bme280, SENSOR_CHAN_HUMIDITY, &data.humidity);

        if (k_mutex_lock(&data_mutex, K_MSEC(500)) == 0) {
            latest_reading = data;
            k_mutex_unlock(&data_mutex);
        }

        k_sleep(K_SECONDS(5));
    }
}
