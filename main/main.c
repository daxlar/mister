#include "core2forAWS.h"
#include "driver/gpio.h"

#include "string.h"
#include "sys/time.h"
#include "esp_system.h"
#include "esp_event.h"
#include "esp_log.h"
#include "esp_attr.h"
#include "esp_sleep.h"
#include "nvs_flash.h"
#include "esp_sntp.h"

#include "wifi.h"

#define PIR_SENSING_PIN GPIO_NUM_32

static const char *TAG = "mister";

static xQueueHandle gpio_evt_queue = NULL;
static char *tempString = "hello world!";
static lv_obj_t *tab_view;
static const char *btns[] = {"Apply", "Close", ""};

volatile int interruptFlag = 0;

static void time_sync_notification_cb(struct timeval *tv)
{
    ESP_LOGI(TAG, "Notification of a time synchronization event");
}

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

void parseNTPStringToRTCTime(char strftime_buf[], int bufferSize, rtc_date_t *dateTime)
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
    for (int i = 0; i < 7; i++)
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

void app_main(void)
{

    int counter = 0;

    Core2ForAWS_Init();
    Core2ForAWS_Display_SetBrightness(80);

    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    lv_obj_t *mbox1 = lv_msgbox_create(lv_scr_act(), NULL);
    lv_msgbox_set_text(mbox1, "A message box with two buttons.");
    lv_msgbox_add_btns(mbox1, btns);
    lv_obj_set_width(mbox1, 200);
    lv_obj_set_event_cb(mbox1, event_handler);
    lv_obj_align(mbox1, NULL, LV_ALIGN_CENTER, 0, 0);
    xSemaphoreGive(xGuiSemaphore);

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
    char val = tempString[2];

    initialise_wifi();

    int i = 0;
    while (i < 10)
    {
        vTaskDelay(2000 / portTICK_PERIOD_MS);
        i++;
    }

    i = 0;

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

    char strftime_buf[64];

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

    while (1)
    {
        for (int i = 0; i < 10000; i++)
        {
        }
        counter = (counter + 1) % 10000;
        vTaskDelay(1000);
        lv_msgbox_set_text(mbox1, tempString);
    }
}