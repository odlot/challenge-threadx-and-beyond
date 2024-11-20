/* 
 * Copyright (c) Microsoft
 * Copyright (c) 2024 Eclipse Foundation
 * 
 *  This program and the accompanying materials are made available 
 *  under the terms of the MIT license which is available at
 *  https://opensource.org/license/mit.
 * 
 *  SPDX-License-Identifier: MIT
 * 
 *  Contributors: 
 *     Microsoft         - Initial version
 *     Frédéric Desbiens - 2024 version.
 */

#include <stdio.h>
#include <stdbool.h>

#include "tx_api.h"

#include "board_init.h"
#include "cmsis_utils.h"
#include "screen.h"
#include "sntp_client.h"
#include "wwd_networking.h"

#include "cloud_config.h"

#include "nx_api.h"
#include "nxd_mqtt_client.h"

#define ECLIPSETX_THREAD_STACK_SIZE 4096
#define ECLIPSETX_THREAD_PRIORITY   4

TX_THREAD eclipsetx_thread;
ULONG eclipsetx_thread_stack[ECLIPSETX_THREAD_STACK_SIZE / sizeof(ULONG)];

#define DEMO_STACK_SIZE 2048
#define CLIENT_ID_STRING "mytestclient"
#define MQTT_CLIENT_STACK_SIZE 4096
#define STRLEN(p) (sizeof(p) - 1)
#define MQTT_THREAD_PRIORITY 2
#define MQTT_KEEP_ALIVE_TIMER 300
#define QOS0 0
#define QOS1 1
#define LOCAL_SERVER_ADDRESS (IP_ADDRESS(10, 42, 0, 1))
#define TOPIC_NAME "mqtt_data"
#define MESSAGE_STRING "This is a message. "
#define DEMO_MESSAGE_EVENT 1
#define DEMO_ALL_EVENTS 3

//static UCHAR message_buffer[NXD_MQTT_MAX_MESSAGE_LENGTH];
//static UCHAR topic_buffer[NXD_MQTT_MAX_TOPIC_NAME_LENGTH];

static ULONG mqtt_client_stack[MQTT_CLIENT_STACK_SIZE / sizeof(ULONG)];
static NXD_MQTT_CLIENT mqtt_client;
TX_EVENT_FLAGS_GROUP mqtt_app_flag;

static VOID my_disconnect_func(NXD_MQTT_CLIENT *client_ptr)
{
    printf("client disconnected from server\n");
}

/*static VOID my_notify_func(NXD_MQTT_CLIENT* client_ptr, UINT number_of_messages)
{
    tx_event_flags_set(&mqtt_app_flag, DEMO_MESSAGE_EVENT, TX_OR);
    return;
}*/

INT value = 50;
INT button_A_was_pressed = 0;
INT button_B_was_pressed = 0;

static void eclipsetx_thread_entry(ULONG parameter)
{
    UINT status;

    printf("Starting Eclipse ThreadX thread\r\n\r\n");

    printf("%s\r\n", WIFI_SSID);

    // Initialize the network
    if ((status = wwd_network_init(WIFI_SSID, WIFI_PASSWORD, WIFI_MODE)))
    {
        screen_print("Error", L0);
        printf("ERROR: Failed to initialize the network (0x%08x)\r\n", status);
    } else {
        screen_print("Ok", L0);
    }

    status = wwd_network_connect();
    printf("network connect status (0x%08x)\r\n", status);

    NX_IP *test = &nx_ip;
    if (test == NULL) {
        screen_print("E2", L3);
    }

    status = nxd_mqtt_client_create(&mqtt_client, "my_client",
        CLIENT_ID_STRING, STRLEN(CLIENT_ID_STRING), &nx_ip, &nx_pool[0],
        (VOID*)mqtt_client_stack, sizeof(mqtt_client_stack),
        MQTT_THREAD_PRIORITY, NX_NULL, 0);

    NXD_ADDRESS server_ip;
    /*ULONG events;
    UINT topic_length, message_length;*/
    nxd_mqtt_client_disconnect_notify_set(&mqtt_client, my_disconnect_func);

    status = tx_event_flags_create(&mqtt_app_flag, "my app event");
    server_ip.nxd_ip_version = 4;
    server_ip.nxd_ip_address.v4 = LOCAL_SERVER_ADDRESS;

    /* Start the connection to the server. */
    status = nxd_mqtt_client_connect(&mqtt_client, &server_ip, NXD_MQTT_PORT, MQTT_KEEP_ALIVE_TIMER, 0, NX_WAIT_FOREVER);

    if (status != TX_SUCCESS) {
        printf("nxd_mqtt_client_connect (0x%08x)\r\n", status);
    } else {
        bool onChange = false;
        char snum[5];
        itoa(value, snum, 10);
        nxd_mqtt_client_publish(&mqtt_client, TOPIC_NAME,
            STRLEN(TOPIC_NAME), (CHAR*)snum, 
            STRLEN(MESSAGE_STRING), 0, QOS1, NX_WAIT_FOREVER);
        printf("Publish a message with QoS level 1\n");
        while (1) {
            if (BUTTON_A_IS_PRESSED) {
                button_B_was_pressed = 0;
                button_A_was_pressed = button_A_was_pressed + 1 >= 5 ? 5 : button_A_was_pressed + 1;
                value = (value - button_A_was_pressed) <= 0 ? 0 : value - button_A_was_pressed;
                onChange = true;
            }
            if (BUTTON_B_IS_PRESSED) {
                button_A_was_pressed = 0;
                button_B_was_pressed = button_B_was_pressed + 1 >= 5 ? 5 : button_B_was_pressed + 1;
                value = (value + button_B_was_pressed) >= 100 ? 100 : value + button_B_was_pressed;
                onChange = true;
            }
            if (onChange) {
                onChange = false;
                printf("my value: %d\r\n", value);
                itoa(value, snum, 10);
                nxd_mqtt_client_publish(&mqtt_client, TOPIC_NAME,
                    STRLEN(TOPIC_NAME), (CHAR*)snum, 
                    STRLEN(MESSAGE_STRING), 0, QOS1, NX_WAIT_FOREVER);
                printf("Publish a message with QoS level 1: %s\n", snum);
                tx_thread_sleep(0.1 * TX_TIMER_TICKS_PER_SECOND);
            }
        }
    }

    /* Subscribe to the topic with QoS level 0. */
    // nxd_mqtt_client_subscribe(&mqtt_client, TOPIC_NAME, STRLEN(TOPIC_NAME),
    //     QOS0);

    /* Set the receive notify function. */
    // nxd_mqtt_client_receive_notify_set(&mqtt_client, my_notify_func);

    /* Now wait for the broker to publish the message. */
    // tx_event_flags_get(&mqtt_app_flag, DEMO_ALL_EVENTS,
    //     TX_OR_CLEAR, &events, TX_WAIT_FOREVER);

    /*if(events & DEMO_MESSAGE_EVENT)
    {
        nxd_mqtt_client_message_get(&mqtt_client, topic_buffer,
            sizeof(topic_buffer), &topic_length, message_buffer,
            sizeof(message_buffer), &message_length);

        topic_buffer[topic_length] = 0;

        message_buffer[message_length] = 0;

        printf("topic = %s, message = %s\n", topic_buffer, message_buffer);
    }*/

    /* Now unsubscribe the topic. */
    //nxd_mqtt_client_unsubscribe(&mqtt_client, TOPIC_NAME,
    //    STRLEN(TOPIC_NAME));

    /* Disconnect from the broker. */
    nxd_mqtt_client_disconnect(&mqtt_client);

    /* Delete the client instance, release all the resources. */
    nxd_mqtt_client_delete(&mqtt_client);
    printf("BYE!\r\n");
}

void tx_application_define(void* first_unused_memory)
{
    systick_interval_set(TX_TIMER_TICKS_PER_SECOND);

    // Create ThreadX thread
    UINT status = tx_thread_create(&eclipsetx_thread,
        "Eclipse ThreadX Thread",
        eclipsetx_thread_entry,
        0,
        eclipsetx_thread_stack,
        ECLIPSETX_THREAD_STACK_SIZE,
        ECLIPSETX_THREAD_PRIORITY,
        ECLIPSETX_THREAD_PRIORITY,
        TX_NO_TIME_SLICE,
        TX_AUTO_START);

    if (status != TX_SUCCESS)
    {
        printf("ERROR: Eclipse ThreadX thread creation failed\r\n");
    }
}

int main(void)
{
    // Initialize the board
    board_init();

    // Enter the ThreadX kernel
    tx_kernel_enter();

    return 0;
}
