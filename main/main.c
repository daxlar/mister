/*
 * AWS IoT EduKit - Core2 for AWS IoT EduKit
 * Smart Thermostat v1.2.0
 * main.c
 * 
 * Copyright 2010-2015 Amazon.com, Inc. or its affiliates. All Rights Reserved.
 * Additions Copyright 2016 Espressif Systems (Shanghai) PTE LTD
 *
 * Licensed under the Apache License, Version 2.0 (the "License").
 * You may not use this file except in compliance with the License.
 * A copy of the License is located at
 *
 *  http://aws.amazon.com/apache2.0
 *
 * or in the "license" file accompanying this file. This file is distributed
 * on an "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either
 * express or implied. See the License for the specific language governing
 * permissions and limitations under the License.
 */
/**
 * @file main.c
 * @brief simple MQTT publish, subscribe, and device shadows for use with AWS IoT EduKit reference hardware.
 *
 * This example takes the parameters from the build configuration and establishes a connection to AWS IoT Core over MQTT.
 *
 * Some configuration is required. Visit https://edukit.workshop.aws
 *
 */
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "aws_iot_config.h"
#include "aws_iot_log.h"
#include "aws_iot_version.h"
#include "aws_iot_mqtt_client_interface.h"
#include "aws_iot_shadow_interface.h"

#include "core2forAWS.h"

#include "wifi.h"
#include "fft.h"
#include "ui.h"
#include "rtcTime.h"

static const char *TAG = "MAIN";

#define HEATING "HEATING"
#define COOLING "COOLING"
#define STANDBY "STANDBY"

#define STARTING_ROOMTEMPERATURE 0.0f
#define STARTING_SOUNDLEVEL 0x00
#define STARTING_HVACSTATUS STANDBY
#define STARTING_ROOMOCCUPANCY false

// Number of slices to split the microphone sample into
#define AUDIO_TIME_SLICES 60

#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 200

/* CA Root certificate */
extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");

/* Default MQTT HOST URL is pulled from the aws_iot_config.h */
char HostAddress[255] = AWS_IOT_MQTT_HOST;

/* Default MQTT port is pulled from the aws_iot_config.h */
uint32_t port = AWS_IOT_MQTT_PORT;

#define PIR_SENSING_PIN GPIO_NUM_32

static xQueueHandle gpio_evt_queue = NULL;
static lv_obj_t *tab_view;
static const char *btns[] = {"Apply", "Close", ""};

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void *arg)
{
    uint32_t io_num;
    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

static void event_handler(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        printf("Button: %s\n", lv_msgbox_get_active_btn_text(obj));
    }
}

void iot_subscribe_callback_handler(AWS_IoT_Client *pClient, char *topicName, uint16_t topicNameLen,
                                    IoT_Publish_Message_Params *params, void *pData)
{
    ESP_LOGI(TAG, "Subscribe callback");
    ESP_LOGI(TAG, "%.*s\t%.*s", topicNameLen, topicName, (int)params->payloadLen, (char *)params->payload);
}

void disconnect_callback_handler(AWS_IoT_Client *pClient, void *data)
{
    ESP_LOGW(TAG, "MQTT Disconnect");
    //ui_textarea_add("Disconnected from AWS IoT Core...", NULL, 0);

    IoT_Error_t rc = FAILURE;

    if (NULL == pClient)
    {
        return;
    }

    if (aws_iot_is_autoreconnect_enabled(pClient))
    {
        ESP_LOGI(TAG, "Auto Reconnect is enabled, Reconnecting attempt will start now");
    }
    else
    {
        ESP_LOGW(TAG, "Auto Reconnect not enabled. Starting manual reconnect...");
        rc = aws_iot_mqtt_attempt_reconnect(pClient);
        if (NETWORK_RECONNECTED == rc)
        {
            ESP_LOGW(TAG, "Manual Reconnect Successful");
        }
        else
        {
            ESP_LOGW(TAG, "Manual Reconnect Failed - %d", rc);
        }
    }
}

static bool shadowUpdateInProgress;

void ShadowUpdateStatusCallback(const char *pThingName, ShadowActions_t action, Shadow_Ack_Status_t status,
                                const char *pReceivedJsonDocument, void *pContextData)
{
    IOT_UNUSED(pThingName);
    IOT_UNUSED(action);
    IOT_UNUSED(pReceivedJsonDocument);
    IOT_UNUSED(pContextData);

    ESP_LOGI(TAG, "Shadow update callback: %s", pReceivedJsonDocument);

    shadowUpdateInProgress = false;

    if (SHADOW_ACK_TIMEOUT == status)
    {
        ESP_LOGE(TAG, "Update timed out");
    }
    else if (SHADOW_ACK_REJECTED == status)
    {
        ESP_LOGE(TAG, "Update rejected");
    }
    else if (SHADOW_ACK_ACCEPTED == status)
    {
        ESP_LOGI(TAG, "Update accepted");
    }
}

void hvac_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext)
{
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    char *status = (char *)(pContext->pData);

    ESP_LOGI(TAG, "hvac callback pJsonString: %s", pJsonString);

    if (pContext != NULL)
    {
        ESP_LOGI(TAG, "Delta - hvacStatus state changed to %s", status);
    }

    if (strcmp(status, HEATING) == 0)
    {
        ESP_LOGI(TAG, "setting side LEDs to red");
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, 0xFF0000);
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, 0xFF0000);
        Core2ForAWS_Sk6812_Show();
    }
    else if (strcmp(status, COOLING) == 0)
    {
        ESP_LOGI(TAG, "setting side LEDs to blue");
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_LEFT, 0x0000FF);
        Core2ForAWS_Sk6812_SetSideColor(SK6812_SIDE_RIGHT, 0x0000FF);
        Core2ForAWS_Sk6812_Show();
    }
    else
    {
        ESP_LOGI(TAG, "clearing side LEDs");
        Core2ForAWS_Sk6812_Clear();
        Core2ForAWS_Sk6812_Show();
    }
}

void occupancy_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext)
{
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    if (pContext != NULL)
    {
        ESP_LOGI(TAG, "Delta - roomOccupancy state changed to %d", *(bool *)(pContext->pData));
    }
}

void config_gpio()
{
    gpio_config_t pGPIOConfig;
    pGPIOConfig.pin_bit_mask = (1ULL << PIR_SENSING_PIN);
    pGPIOConfig.mode = GPIO_MODE_INPUT;
    pGPIOConfig.pull_up_en = 0;
    pGPIOConfig.pull_down_en = 1;
    pGPIOConfig.intr_type = GPIO_INTR_POSEDGE;

    gpio_config(&pGPIOConfig);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIR_SENSING_PIN, gpio_isr_handler, (void *)PIR_SENSING_PIN);

    ESP_LOGI(TAG, "done with gpio config!");
    ESP_LOGI(TAG, "******************************");
}

char hvacStatus[7] = STARTING_HVACSTATUS;
bool roomOccupancy = STARTING_ROOMOCCUPANCY;

void aws_iot_task(void *param)
{
    IoT_Error_t rc = FAILURE;

    char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
    size_t sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);

    jsonStruct_t hvacStatusActuator;
    hvacStatusActuator.cb = hvac_Callback;
    hvacStatusActuator.pKey = "hvacStatus";
    hvacStatusActuator.pData = &hvacStatus;
    hvacStatusActuator.type = SHADOW_JSON_STRING;
    hvacStatusActuator.dataLength = strlen(hvacStatus) + 1;

    jsonStruct_t roomOccupancyActuator;
    roomOccupancyActuator.cb = occupancy_Callback;
    roomOccupancyActuator.pKey = "roomOccupancy";
    roomOccupancyActuator.pData = &roomOccupancy;
    roomOccupancyActuator.type = SHADOW_JSON_BOOL;
    roomOccupancyActuator.dataLength = sizeof(bool);

    ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

    // initialize the mqtt client
    AWS_IoT_Client iotCoreClient;

    ShadowInitParameters_t sp = ShadowInitParametersDefault;
    sp.pHost = HostAddress;
    sp.port = port;
    sp.enableAutoReconnect = false;
    sp.disconnectHandler = disconnect_callback_handler;

    sp.pRootCA = (const char *)aws_root_ca_pem_start;
    sp.pClientCRT = "#";
    sp.pClientKey = "#0";

#define CLIENT_ID_LEN (ATCA_SERIAL_NUM_SIZE * 2)
    char *client_id = malloc(CLIENT_ID_LEN + 1);
    ATCA_STATUS ret = Atecc608_GetSerialString(client_id);
    if (ret != ATCA_SUCCESS)
    {
        ESP_LOGE(TAG, "Failed to get device serial from secure element. Error: %i", ret);
        abort();
    }

    //ui_textarea_add("\n\nDevice client Id:\n>> %s <<\n", client_id, CLIENT_ID_LEN);

    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);

    setSNTP();

    int i = 0;
    while (i < 10)
    {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        i++;
    }

    ESP_LOGI(TAG, "Shadow Init");

    rc = aws_iot_shadow_init(&iotCoreClient, &sp);
    if (SUCCESS != rc)
    {
        ESP_LOGE(TAG, "aws_iot_shadow_init returned error %d, aborting...", rc);
        abort();
    }

    ShadowConnectParameters_t scp = ShadowConnectParametersDefault;
    scp.pMyThingName = client_id;
    scp.pMqttClientId = client_id;
    scp.mqttClientIdLen = CLIENT_ID_LEN;

    ESP_LOGI(TAG, "Shadow Connect");
    rc = aws_iot_shadow_connect(&iotCoreClient, &scp);
    if (SUCCESS != rc)
    {
        ESP_LOGE(TAG, "aws_iot_shadow_connect returned error %d, aborting...", rc);
        abort();
    }
    //ui_textarea_add("Connected to AWS IoT Device Shadow service", NULL, 0);

    /*
     * Enable Auto Reconnect functionality. Minimum and Maximum time of Exponential backoff are set in aws_iot_config.h
     *  #AWS_IOT_MQTT_MIN_RECONNECT_WAIT_INTERVAL
     *  #AWS_IOT_MQTT_MAX_RECONNECT_WAIT_INTERVAL
     */
    rc = aws_iot_shadow_set_autoreconnect_status(&iotCoreClient, true);
    if (SUCCESS != rc)
    {
        ESP_LOGE(TAG, "Unable to set Auto Reconnect to true - %d, aborting...", rc);
        abort();
    }

    // register delta callback for roomOccupancy
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &roomOccupancyActuator);
    if (SUCCESS != rc)
    {
        ESP_LOGE(TAG, "Shadow Register Delta Error");
    }

    // register delta callback for hvacStatus
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &hvacStatusActuator);
    if (SUCCESS != rc)
    {
        ESP_LOGE(TAG, "Shadow Register Delta Error");
    }

    config_gpio();

    // loop and publish changes
    while (NETWORK_ATTEMPTING_RECONNECT == rc || NETWORK_RECONNECTED == rc || SUCCESS == rc)
    {
        rc = aws_iot_shadow_yield(&iotCoreClient, 200);
        if (NETWORK_ATTEMPTING_RECONNECT == rc || shadowUpdateInProgress)
        {
            rc = aws_iot_shadow_yield(&iotCoreClient, 1000);
            // If the client is attempting to reconnect, or already waiting on a shadow update,
            // we will skip the rest of the loop.
            continue;
        }

        ESP_LOGI(TAG, "*****************************************************************************************");
        ESP_LOGI(TAG, "On Device: roomOccupancy %s", roomOccupancy ? "true" : "false");
        ESP_LOGI(TAG, "On Device: hvacStatus %s", hvacStatus);

        roomOccupancy = true;

        rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
        if (SUCCESS == rc)
        {
            rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 2, &roomOccupancyActuator, &hvacStatusActuator);
            if (SUCCESS == rc)
            {
                rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
                if (SUCCESS == rc)
                {
                    ESP_LOGI(TAG, "Update Shadow: %s", JsonDocumentBuffer);
                    rc = aws_iot_shadow_update(&iotCoreClient, client_id, JsonDocumentBuffer,
                                               ShadowUpdateStatusCallback, NULL, 4, true);
                    shadowUpdateInProgress = true;
                }
            }
        }
        ESP_LOGI(TAG, "*****************************************************************************************");
        ESP_LOGI(TAG, "Stack remaining for task '%s' is %d bytes", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));

        vTaskDelay(pdMS_TO_TICKS(1000));
    }

    if (SUCCESS != rc)
    {
        ESP_LOGE(TAG, "An error occurred in the loop %d", rc);
    }

    ESP_LOGI(TAG, "Disconnecting");
    rc = aws_iot_shadow_disconnect(&iotCoreClient);

    if (SUCCESS != rc)
    {
        ESP_LOGE(TAG, "Disconnect error %d", rc);
    }

    vTaskDelete(NULL);
}

void app_main()
{
    Core2ForAWS_Init();
    Core2ForAWS_Display_SetBrightness(80);
    Core2ForAWS_LED_Enable(1);

    ui_init();
    /*
     ported UI
    */

    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    lv_obj_t *mbox1 = lv_msgbox_create(lv_scr_act(), NULL);
    lv_msgbox_set_text(mbox1, "A message box with two buttons.");
    lv_msgbox_add_btns(mbox1, btns);
    lv_obj_set_width(mbox1, 200);
    lv_obj_set_event_cb(mbox1, event_handler);
    lv_obj_align(mbox1, NULL, LV_ALIGN_CENTER, 0, 0);
    xSemaphoreGive(xGuiSemaphore);

    initialise_wifi();

    xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 4096 * 2, NULL, 5, NULL, 1);
}
