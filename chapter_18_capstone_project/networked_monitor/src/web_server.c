#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/net/socket.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "sensor_manager.h"

LOG_MODULE_REGISTER(web_server, LOG_LEVEL_INF);

extern struct sensor_reading latest_reading;
extern struct k_mutex data_mutex;

#define WEB_SERVER_PORT 80
#define RECV_BUF_SIZE 512

static void handle_client(int client)
{
    struct sensor_reading data;
    char response[512];
    char recv_buf[RECV_BUF_SIZE];
    int ret;

    /* Receive HTTP request (basic implementation) */
    ret = recv(client, recv_buf, sizeof(recv_buf) - 1, 0);
    if (ret < 0) {
        LOG_ERR("Failed to receive data from client: %d", errno);
        close(client);
        return;
    }
    recv_buf[ret] = '\0';
    LOG_DBG("Received request: %.*s", (ret > 50 ? 50 : ret), recv_buf);

    /* Get the latest sensor data */
    ret = k_mutex_lock(&data_mutex, K_MSEC(500));
    if (ret == 0) {
        data = latest_reading;
        k_mutex_unlock(&data_mutex);
    } else {
        /* Could not get lock, send error response */
        LOG_WRN("Failed to acquire mutex for sensor data");
        const char *error_response = 
            "HTTP/1.1 503 Service Unavailable\r\n"
            "Content-Type: text/html\r\n\r\n"
            "<html><body><h1>503 Service Unavailable</h1>"
            "<p>Sensor data temporarily unavailable</p></body></html>";
        
        ret = send(client, error_response, strlen(error_response), 0);
        if (ret < 0) {
            LOG_ERR("Failed to send error response: %d", errno);
        }
        close(client);
        return;
    }

    /* Create the HTTP response */
    ret = snprintf(response, sizeof(response),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Connection: close\r\n\r\n"
             "<!DOCTYPE html>"
             "<html><head><title>Weather Station</title>"
             "<meta http-equiv=\"refresh\" content=\"5\">"
             "</head>"
             "<body style=\"font-family: Arial, sans-serif; margin: 40px;\">"
             "<h1>Environmental Monitor</h1>"
             "<div style=\"background: #f0f0f0; padding: 20px; border-radius: 8px;\">"
             "<p><strong>Temperature:</strong> %d.%06d °C</p>"
             "<p><strong>Pressure:</strong> %d.%06d kPa</p>"
             "<p><strong>Humidity:</strong> %d.%06d %%</p>"
             "</div>"
             "<p style=\"color: #666; font-size: 0.9em;\">Page auto-refreshes every 5 seconds</p>"
             "</body></html>",
             data.temp.val1, data.temp.val2,
             data.press.val1, data.press.val2,
             data.humidity.val1, data.humidity.val2);

    if (ret < 0 || ret >= sizeof(response)) {
        LOG_ERR("Failed to format HTTP response");
        close(client);
        return;
    }

    /* Send the response */
    ret = send(client, response, strlen(response), 0);
    if (ret < 0) {
        LOG_ERR("Failed to send response: %d", errno);
    } else {
        LOG_DBG("Response sent successfully (%d bytes)", ret);
    }

    close(client);
}

void web_server_thread(void)
{
    int server_sock;
    struct sockaddr_in addr;
    int ret;
    int optval = 1;

    /* Create socket */
    server_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_sock < 0) {
        LOG_ERR("Failed to create socket: %d", errno);
        return;
    }

    /* Set socket options to reuse address */
    ret = setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
    if (ret < 0) {
        LOG_WRN("Failed to set SO_REUSEADDR: %d", errno);
    }

    /* Bind socket */
    addr.sin_family = AF_INET;
    addr.sin_port = htons(WEB_SERVER_PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    ret = bind(server_sock, (struct sockaddr *)&addr, sizeof(addr));
    if (ret < 0) {
        LOG_ERR("Failed to bind socket to port %d: %d", WEB_SERVER_PORT, errno);
        close(server_sock);
        return;
    }

    /* Listen for connections */
    ret = listen(server_sock, 5);
    if (ret < 0) {
        LOG_ERR("Failed to listen on socket: %d", errno);
        close(server_sock);
        return;
    }

    LOG_INF("Web server listening on port %d", WEB_SERVER_PORT);
    LOG_INF("Connect to http://<device-ip>:%d to view sensor data", WEB_SERVER_PORT);

    /* Accept and handle client connections */
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_addr_len = sizeof(client_addr);
        
        int client_sock = accept(server_sock, (struct sockaddr *)&client_addr, &client_addr_len);

        if (client_sock >= 0) {
            LOG_INF("Client connected from %d.%d.%d.%d:%d",
                    (client_addr.sin_addr.s_addr >> 0) & 0xFF,
                    (client_addr.sin_addr.s_addr >> 8) & 0xFF,
                    (client_addr.sin_addr.s_addr >> 16) & 0xFF,
                    (client_addr.sin_addr.s_addr >> 24) & 0xFF,
                    ntohs(client_addr.sin_port));
            
            handle_client(client_sock);
        } else {
            LOG_ERR("Failed to accept connection: %d", errno);
            k_sleep(K_MSEC(100)); /* Brief delay to prevent tight loop on persistent errors */
        }
    }

    /* Cleanup (unreachable in this implementation) */
    close(server_sock);
}
