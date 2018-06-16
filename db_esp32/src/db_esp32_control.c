/*
 *   This file is part of DroneBridge: https://github.com/seeul8er/DroneBridge
 *
 *   Copyright 2018 Wolfgang Christl
 *
 *   Licensed under the Apache License, Version 2.0 (the "License");
 *   you may not use this file except in compliance with the License.
 *   You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 *   Unless required by applicable law or agreed to in writing, software
 *   distributed under the License is distributed on an "AS IS" BASIS,
 *   WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *   See the License for the specific language governing permissions and
 *   limitations under the License.
 *
 */

#include <sys/fcntl.h>
#include <msp_serial.h>
#include <db_protocol.h>
#include <string.h>
#include <sys/param.h>
#include "esp_log.h"
#include "esp_vfs.h"
#include "esp_vfs_dev.h"
#include "lwip/sockets.h"
#include "driver/uart.h"
#include "globals.h"
#include "db_esp32_control.h"


uint16_t app_port_proxy = APP_PORT_PROXY;
uint16_t app_port_telem = APP_PORT_TELEMETRY;
struct sockaddr_in client_telem_addr, client_proxy_addr, server_addr;
char const *TAG = "DB_CONTROL: ";
int udp_socket = -1, serial_socket = -1;
uint8_t msp_message_buffer[UART_BUF_SIZE];


int get_socket_error_code(int socket)
{
    int result;
    u32_t optlen = sizeof(int);
    if(getsockopt(socket, SOL_SOCKET, SO_ERROR, &result, &optlen) == -1) {
        ESP_LOGE(TAG, "getsockopt failed");
        return -1;
    }
    return result;

}

int show_socket_error_reason(int socket)
{
    int err = get_socket_error_code(socket);
    ESP_LOGE(TAG, "socket error %d %s", err, strerror(err));
    return err;
}

int open_udp_socket() {
    // set up UDP socket remote address to forward proxy traffic (MSP) to
    client_proxy_addr.sin_family = AF_INET;
    client_proxy_addr.sin_addr.s_addr = inet_addr(DEST_IP);
    client_proxy_addr.sin_port = htons(app_port_proxy);
    // set up UDP socket remote address to forward telemetry traffic (MAVLink, LTM) to
    client_telem_addr.sin_family = AF_INET;
    client_telem_addr.sin_addr.s_addr = inet_addr(DEST_IP);
    client_telem_addr.sin_port = htons(app_port_telem);

    // local server port we bind to
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(app_port_proxy);

    ESP_LOGI(TAG, "bind_udp_server port:%d", app_port_proxy);
    udp_socket = lwip_socket(PF_INET, SOCK_DGRAM, IPPROTO_IP);
    if (udp_socket < 0) {
        show_socket_error_reason(udp_socket);
        return ESP_FAIL;
    }
    if (lwip_bind(udp_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        show_socket_error_reason(udp_socket);
        close(udp_socket);
        return ESP_FAIL;
    }
    return 1;
}

int open_serial_socket() {
    uart_config_t uart_config = {
            .baud_rate = DB_UART_BAUD_RATE,
            .data_bits = UART_DATA_8_BITS,
            .parity    = UART_PARITY_DISABLE,
            .stop_bits = UART_STOP_BITS_1,
            .flow_ctrl = UART_HW_FLOWCTRL_DISABLE
    };
    ESP_ERROR_CHECK(uart_param_config(UART_NUM_2, &uart_config));
    ESP_ERROR_CHECK(uart_set_pin(UART_NUM_2, DB_UART_PIN_TX, DB_UART_PIN_RX, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));
    ESP_ERROR_CHECK(uart_driver_install(UART_NUM_2, 256, 0, 0, NULL, 0));
    if ((serial_socket = open("/dev/uart/2", O_RDWR | O_NONBLOCK)) == -1) {
        printf("%sCannot open UART2\n", TAG);
        ESP_LOGE(TAG, "Cannot open UART2");
        close(serial_socket); serial_socket = -1; uart_driver_delete(UART_NUM_2);
    }
    esp_vfs_dev_uart_use_driver(2);
    return 1;
}

/**
 * @brief Parses & sends complete MSP & LTM messages
 */
void parse_send_msp_ltm_message(){
    uint8_t serial_byte;
    mspPort_t db_msp_port;
    bool continue_reading = true;
    uint serial_read_bytes = 0;
    while (continue_reading){
        if (read(serial_socket, &serial_byte, 1) > 0) {
        //if (uart_read_bytes(UART_NUM_2, &serial_byte, 1, 20 / portTICK_RATE_MS) > 0) {
            serial_read_bytes++;
            printf("%sRead one byte form UART!\n", TAG);
            // if MSP parser returns false stop reading from serial. We are reading shit or started
            // reading during the middle of a message
            if (mspSerialProcessReceivedData(&db_msp_port, serial_byte)){
                msp_message_buffer[(serial_read_bytes-1)] = serial_byte;
                if (db_msp_port.c_state == MSP_COMMAND_RECEIVED){
                    continue_reading = false;
                    printf("%sGot MSP message!\n", TAG);
                    if (client_connected)
                        lwip_sendto(udp_socket, msp_message_buffer, (size_t) serial_read_bytes, 0,
                                (struct sockaddr *) &client_proxy_addr, sizeof (client_proxy_addr));
                } else if (db_msp_port.c_state == LTM_COMMAND_RECEIVED){
                    continue_reading = false;
                    printf("%sGot LTM message!\n", TAG);
                    if (client_connected)
                        lwip_sendto(udp_socket, msp_message_buffer, (size_t) serial_read_bytes, 0,
                                (struct sockaddr *) &client_telem_addr, sizeof (client_telem_addr));
                }
            } else {
                continue_reading = 0;
            }
        } else {
            printf("%sCould not read form UART!\n", TAG);
        }
    }
}

/**
 * @brief DroneBridge control module implementation for a ESP32 device. Bi-directional link between FC and ground. Can
 * handle MSPv1, MSPv2, LTM and MAVLink.
 * MSP & LTM is parsed and sent packet/frame by frame to ground
 * MAVLink is passed through (fully transparent). Can be used with any protocol.
 */
void control_module(void *pvParameter){
    int setup_success = 1, recv_len, select_return;
    char udp_buffer[UDP_BUF_SIZE];
    memset(udp_buffer, 0, UDP_BUF_SIZE);
    //uint8_t *uart_buffer = (uint8_t *) malloc(UART_BUF_SIZE);
    socklen_t proxy_addr_length = sizeof(client_proxy_addr);

    if (open_udp_socket() == ESP_FAIL) setup_success = 0;
    if (open_serial_socket() == ESP_FAIL) setup_success = 0;
    if (setup_success){
        printf("%ssetup complete!\n", TAG);
    }
//    for (int i = 0; i < 1000; ++i) {
//        char buf;
//        if (read(serial_socket, &buf, 1) > 0) {
//            ESP_LOGI(TAG, "Received: %c", buf);
//        }
//    }
    while(setup_success){
        fd_set fd_socket_set;
        FD_ZERO (&fd_socket_set);
        FD_SET(udp_socket, &fd_socket_set);
        FD_SET(serial_socket, &fd_socket_set);
        struct timeval tv = {
                .tv_sec = 2,
                .tv_usec = 0,
        };
        select_return = select(MAX(serial_socket, udp_socket) + 1, &fd_socket_set, NULL, NULL, &tv);
        if(select_return < 0) {
            ESP_LOGE(TAG, "select() returned error: %s", strerror(errno));
        }else if (select_return == 0){
            ESP_LOGW(TAG, "select timeout!");
        }else if (select_return > 0){
            if (FD_ISSET(udp_socket, &fd_socket_set)){
                recv_len = lwip_recvfrom_r(udp_socket, udp_buffer, UDP_BUF_SIZE, 0,
                        (struct sockaddr *) &client_proxy_addr, &proxy_addr_length);
                if (recv_len < 0)
                {
                    ESP_LOGE(TAG,"recvfrom");
                    break;
                }
                printf("%sGot UDP packet\n", TAG);
                if ((recv_len + 1) < UDP_BUF_SIZE) udp_buffer[recv_len + 1] = '\0';
                printf("%swrote %i bytes to UART\n", TAG, uart_write_bytes(UART_NUM_2, udp_buffer, (size_t) recv_len));
            }
            if (FD_ISSET(serial_socket, &fd_socket_set)){
                //int uart_recv_length = uart_read_bytes(UART_NUM_2, uart_buffer, UART_BUF_SIZE, 20 / portTICK_RATE_MS);
                printf("%sGot some from serial socket!\n", TAG);
                char buf;
                if (read(serial_socket, &buf, 1) > 0) {
                    ESP_LOGI(TAG, "Received: %c", buf);
                } else {
                    ESP_LOGE(TAG, "UART read error");
                    break;
                }
//                switch (SERIAL_PROTOCOL){
//                    default:
//                    case 1:
//                    case 2:
//                        printf(TAG, "Got MSP on serial port!\n");
//                        parse_send_msp_ltm_message();
//                        break;
//                    case 3:
//                    case 4:
//                    case 5:
//                        // transparent buffer
//                        break;
//                }
            }
        }
    }
    close(udp_socket);
    close(serial_socket);
    vTaskDelete(NULL);
}
