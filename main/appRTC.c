#include "core2forAWS.h"

#include "string.h"
#include "sys/time.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_sntp.h"

char strftime_buf[64];
static const char *TAG = "rtc";

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

static void parseNTPStringToRTCTime(char strftime_buf[], int bufferSize, rtc_date_t *dateTime)
{
    int strLength = 0;
    for (strLength = 0; strLength < bufferSize && strftime_buf[strLength] != '\0'; strLength++)
    {
        if (strftime_buf[strLength] == ':')
        {
            strftime_buf[strLength] = ' ';
        }
    }
    char timeUnits[7][5] = {"\0\0\0\0\0",
                            "\0\0\0\0\0",
                            "\0\0\0\0\0",
                            "\0\0\0\0\0",
                            "\0\0\0\0\0",
                            "\0\0\0\0\0",
                            "\0\0\0\0\0"};

#define WEEKDAY 0
#define MONTH 1
#define DAY 2
#define HOUR 3
#define MINUTE 4
#define SECOND 5
#define YEAR 6

    int bufferIndex = 0;
    int timeUnitIndex = 0;

    while (bufferIndex < strLength)
    {
        int timeUnitCharIndex = 0;
        while (bufferIndex < strLength && strftime_buf[bufferIndex] != ' ')
        {
            timeUnits[timeUnitIndex][timeUnitCharIndex] = strftime_buf[bufferIndex];
            bufferIndex++;
            timeUnitCharIndex++;
        }
        timeUnitIndex++;
        bufferIndex++;
    }

    for (int i = 0; i < 7; i++)
    {
        ESP_LOGI(TAG, "%s", timeUnits[i]);
    }

    char monthLookupTable[12][4] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 0; i < 12; i++)
    {
        if (strcmp(timeUnits[MONTH], monthLookupTable[i]) == 0)
        {
            dateTime->month = i;
        }
    }

    dateTime->day = (timeUnits[DAY][0] - '0') * 10 + (timeUnits[DAY][1] - '0');
    dateTime->hour = (timeUnits[HOUR][0] - '0') * 10 + (timeUnits[HOUR][1] - '0');
    dateTime->minute = (timeUnits[MINUTE][0] - '0') * 10 + (timeUnits[MINUTE][1] - '0');
    dateTime->second = (timeUnits[SECOND][0] - '0') * 10 + (timeUnits[SECOND][1] - '0');
    dateTime->year = (timeUnits[YEAR][0] - '0') * 1000 + (timeUnits[YEAR][1] - '0') * 100 + (timeUnits[YEAR][2] - '0') * 10 + (timeUnits[YEAR][3] - '0');
}

void app_rtc_init()
{
    int i = 0;

    time_t now;
    struct tm timeinfo;

    ESP_LOGI(TAG, "Initializing SNTP");
    sntp_setoperatingmode(SNTP_OPMODE_POLL);
    sntp_setservername(0, "pool.ntp.org");
    sntp_set_time_sync_notification_cb(time_sync_notification_cb);
    sntp_init();

    while (i < 15)
    {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        i++;
    }

    int retry = 0;
    const int retry_count = 10;
    while (sntp_get_sync_status() == SNTP_SYNC_STATUS_RESET && ++retry < retry_count)
    {
        ESP_LOGI(TAG, "Waiting for system time to be set... (%d/%d)", retry, retry_count);
        vTaskDelay(2000 / portTICK_PERIOD_MS);
    }

    time(&now);
    localtime_r(&now, &timeinfo);

    // Set timezone to Cental Standard Time and print local time
    setenv("TZ", "CST6CDT", 1);
    tzset();
    localtime_r(&now, &timeinfo);
    strftime(strftime_buf, sizeof(strftime_buf), "%c", &timeinfo);
    ESP_LOGI(TAG, "The current date/time in CST is: %s", strftime_buf);

    rtc_date_t dateTime;

    parseNTPStringToRTCTime(strftime_buf, sizeof(strftime_buf) / sizeof(char), &dateTime);
    BM8563_SetTime(&dateTime);

    rtc_date_t datetime;
    BM8563_GetTime(&datetime);
    ESP_LOGI(TAG, "Current Date: %d-%02d-%02d  Time: %02d:%02d:%02d",
             datetime.year, datetime.month, datetime.day, datetime.hour, datetime.minute, datetime.second);

    sntp_stop();
}