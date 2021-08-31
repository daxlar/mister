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
#include "meeting.h"

#define STARTING_MEETING_INTERVAL "0000-0000"
#define MEETING_START_TIME_NUM_DIGITS 4
#define STARTING_ACKNOWLEDGE false
#define MAX_LENGTH_OF_UPDATE_JSON_BUFFER 200

static const char *TAG = "iot";

/* CA Root certificate */
extern const uint8_t aws_root_ca_pem_start[] asm("_binary_aws_root_ca_pem_start");
extern const uint8_t aws_root_ca_pem_end[] asm("_binary_aws_root_ca_pem_end");
/* Default MQTT HOST URL is pulled from the aws_iot_config.h */
char HostAddress[255] = AWS_IOT_MQTT_HOST;
/* Default MQTT port is pulled from the aws_iot_config.h */
uint32_t port = AWS_IOT_MQTT_PORT;

char meetingInterval[] = STARTING_MEETING_INTERVAL;
bool acknowledgement = STARTING_ACKNOWLEDGE;

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

    //ESP_LOGI(TAG, "Shadow update callback: %s", pReceivedJsonDocument);

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

void meetingInterval_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext)
{
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    char *status = (char *)(pContext->pData);

    //ESP_LOGI(TAG, "meetingInterval callback pJsonString: %s", pJsonString);

    if (pContext != NULL)
    {
        ESP_LOGI(TAG, "**********************************************");
        ESP_LOGI(TAG, "Delta - meetingInterval state changed to %s", status);
        ESP_LOGI(TAG, "**********************************************");

        char meetingStartTimeStrSegment[MEETING_START_TIME_NUM_DIGITS];
        char meetingEndTimeStrSegment[MEETING_START_TIME_NUM_DIGITS];

        int strIndex = 0;
        for (; strIndex < MEETING_START_TIME_NUM_DIGITS; strIndex++)
        {
            meetingStartTimeStrSegment[strIndex] = status[strIndex] - '0';
        }
        strIndex++;
        int offset = strIndex;
        for (; status[strIndex] != '\0'; strIndex++)
        {
            meetingEndTimeStrSegment[strIndex - offset] = status[strIndex] - '0';
        }

        int meetingTimeStart = meetingStartTimeStrSegment[0] * 1000 + meetingStartTimeStrSegment[1] * 100 + meetingStartTimeStrSegment[2] * 10 + meetingStartTimeStrSegment[3];
        int meetingTimeEnd = meetingEndTimeStrSegment[0] * 1000 + meetingEndTimeStrSegment[1] * 100 + meetingEndTimeStrSegment[2] * 10 + meetingEndTimeStrSegment[3];

        struct MeetingInterval meeting;
        meeting.beginInMinutes = meetingTimeStart;
        meeting.endInMinutes = meetingTimeEnd;

        ESP_LOGI(TAG, "Meeting time start: %d", meetingTimeStart);
        ESP_LOGI(TAG, "Meeting time end %d", meetingTimeEnd);
        ESP_LOGI(TAG, "Sending through the queue!");

        xQueueSend(get_meetingEnd_evt_queue(), &meeting, 0);
    }
}

void acknowledgement_Callback(const char *pJsonString, uint32_t JsonStringDataLen, jsonStruct_t *pContext)
{
    IOT_UNUSED(pJsonString);
    IOT_UNUSED(JsonStringDataLen);

    if (pContext != NULL)
    {
        ESP_LOGI(TAG, "Delta - acknowledgement state changed to %d", *(bool *)(pContext->pData));
    }
}

void aws_iot_task(void *param)
{

    IoT_Error_t rc = FAILURE;

    char JsonDocumentBuffer[MAX_LENGTH_OF_UPDATE_JSON_BUFFER];
    size_t sizeOfJsonDocumentBuffer = sizeof(JsonDocumentBuffer) / sizeof(JsonDocumentBuffer[0]);

    jsonStruct_t meetingIntervalActuator;
    meetingIntervalActuator.cb = meetingInterval_Callback;
    meetingIntervalActuator.pKey = "meetingInterval";
    meetingIntervalActuator.pData = &meetingInterval;
    meetingIntervalActuator.type = SHADOW_JSON_STRING;
    meetingIntervalActuator.dataLength = strlen(meetingInterval) + 1;

    jsonStruct_t acknowledgementActuator;
    acknowledgementActuator.cb = acknowledgement_Callback;
    acknowledgementActuator.pKey = "acknowledgement";
    acknowledgementActuator.pData = &acknowledgement;
    acknowledgementActuator.type = SHADOW_JSON_BOOL;
    acknowledgementActuator.dataLength = sizeof(bool);

    ESP_LOGI(TAG, "AWS IoT SDK Version %d.%d.%d-%s", VERSION_MAJOR, VERSION_MINOR, VERSION_PATCH, VERSION_TAG);

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

    /* Wait for WiFI to show as connected */
    xEventGroupWaitBits(wifi_event_group, CONNECTED_BIT,
                        false, true, portMAX_DELAY);

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

    // register delta callback for acknowledgement
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &acknowledgementActuator);
    if (SUCCESS != rc)
    {
        ESP_LOGE(TAG, "Shadow Register Delta Error");
    }

    // register delta callback for hvacStatus
    rc = aws_iot_shadow_register_delta(&iotCoreClient, &meetingIntervalActuator);
    if (SUCCESS != rc)
    {
        ESP_LOGE(TAG, "Shadow Register Delta Error");
    }

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

        /*
        ESP_LOGI(TAG, "*****************************************************************************************");
        ESP_LOGI(TAG, "On Device: roomOccupancy %s", acknowledgement ? "true" : "false");
        ESP_LOGI(TAG, "On Device: hvacStatus %s", meetingInterval);
        */
        acknowledgement = true;

        rc = aws_iot_shadow_init_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
        if (SUCCESS == rc)
        {
            rc = aws_iot_shadow_add_reported(JsonDocumentBuffer, sizeOfJsonDocumentBuffer, 2, &acknowledgementActuator, &meetingIntervalActuator);
            if (SUCCESS == rc)
            {
                rc = aws_iot_finalize_json_document(JsonDocumentBuffer, sizeOfJsonDocumentBuffer);
                if (SUCCESS == rc)
                {
                    //ESP_LOGI(TAG, "Update Shadow: %s", JsonDocumentBuffer);
                    rc = aws_iot_shadow_update(&iotCoreClient, client_id, JsonDocumentBuffer,
                                               ShadowUpdateStatusCallback, NULL, 4, true);
                    shadowUpdateInProgress = true;
                }
            }
        }
        /*
        ESP_LOGI(TAG, "*****************************************************************************************");
        ESP_LOGI(TAG, "Stack remaining for task '%s' is %d bytes", pcTaskGetTaskName(NULL), uxTaskGetStackHighWaterMark(NULL));
        */
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