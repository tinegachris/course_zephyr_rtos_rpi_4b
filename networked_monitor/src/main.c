#include <zephyr/kernel.h>
#include "sensor_manager.h"
#include "web_server.h"

/* Shared data and mutex */
struct sensor_reading latest_reading;
K_MUTEX_DEFINE(data_mutex);

K_THREAD_DEFINE(sensor_tid, 1024, sensor_thread, NULL, NULL, NULL, 7, 0, 0);
K_THREAD_DEFINE(web_server_tid, 2048, web_server_thread, NULL, NULL, NULL, 7, 0, 0);

void main(void)
{
    /* The threads are started automatically by K_THREAD_DEFINE */
}
