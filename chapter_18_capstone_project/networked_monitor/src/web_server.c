#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <stdio.h>
#include <string.h>
#include "sensor_manager.h"

LOG_MODULE_REGISTER(web_server, LOG_LEVEL_INF);

extern struct sensor_reading latest_reading;
extern struct k_mutex data_mutex;

#define WEB_SERVER_PORT 80

static void handle_client(int client)
{
    struct sensor_reading data;
    char response[256];

    // Get the latest sensor data
    if (k_mutex_lock(&data_mutex, K_MSEC(500)) == 0) {
        data = latest_reading;
        k_mutex_unlock(&data_mutex);
    } else {
        // Could not get lock, send error or old data
        const char *error_response = "HTTP/1.1 503 Service Unavailable\r\n\r\n";
        send(client, error_response, strlen(error_response), 0);
        close(client);
        return;
    }

    // Create the HTTP response
    snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n\r\n"
             "<html><head><title>Weather Station</title></head>"
             "<body><h1>Weather Station</h1>"
             "<p>Temperature: %d.%06d C</p>"
             "<p>Pressure: %d.%06d kPa</p>"
             "<p>Humidity: %d.%06d %%</p>"
             "</body></html>",
             data.temp.val1, data.temp.val2,
             data.press.val1, data.press.val2,
             data.humidity.val1, data.humidity.val2);

    send(client, response, strlen(response), 0);
    close(client);
}

void web_server_thread(void)
{
    int server_sock;
    struct sockaddr_in addr;

    server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

    addr.sin_family = AF_INET;
    addr.sin_port = htons(WEB_SERVER_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_sock, 5);

    LOG_INF("Web server listening on port %d", WEB_SERVER_PORT);

    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);

        if (client_sock >= 0) {
            handle_client(client_sock);
        }
    }
}
