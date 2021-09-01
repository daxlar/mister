#include "core2forAWS.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#define PIR_SENSING_PIN GPIO_NUM_32
#define GPIO_QUEUE_SIZE 3
#define MEETING_RESPONSE_QUEUE_SIZE 2

static xQueueHandle gpio_evt_queue = NULL;
static xQueueHandle meeting_end_response_queue;
static char TAG[] = "pir";

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

xQueueHandle getPirQueueHandle()
{
    return meeting_end_response_queue;
}

void pir_init()
{
    gpio_config_t pGPIOConfig;
    pGPIOConfig.pin_bit_mask = (1ULL << PIR_SENSING_PIN);
    pGPIOConfig.mode = GPIO_MODE_INPUT;
    pGPIOConfig.pull_up_en = 0;
    pGPIOConfig.pull_down_en = 1;
    pGPIOConfig.intr_type = GPIO_INTR_POSEDGE;

    gpio_config(&pGPIOConfig);

    gpio_evt_queue = xQueueCreate(GPIO_QUEUE_SIZE, sizeof(uint32_t));
    meeting_end_response_queue = xQueueCreate(MEETING_RESPONSE_QUEUE_SIZE, sizeof(uint32_t));

    ESP_LOGI(TAG, "pir init complete");
}

void pir_task(void *params)
{

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIR_SENSING_PIN, gpio_isr_handler, (void *)PIR_SENSING_PIN);
    ESP_LOGI(TAG, "pir start");

    uint32_t io_num;
    uint32_t meetingFinished;

    rtc_date_t dateTime;

    while (1)
    {
        if (xQueueReceive(meeting_end_response_queue, &meetingFinished, portMAX_DELAY))
        {
            BM8563_GetTime(&dateTime);
            int meetingFinishedInitialTime = dateTime.minute;
            int delayTimeinMs = 60 * 1000; // wait for movment for 1 minute
            bool delay = true;

            // calculate the delay here to wait for movement, if movement, back off, then wait again until the meething threshold
            while (delay)
            {
                if (xQueueReceive(gpio_evt_queue, &io_num, pdMS_TO_TICKS(delayTimeinMs)))
                {
                    ESP_LOGI(TAG, "received interrupt on pin number: %d", io_num);
                    // make a noise
                    // display UI to back off again
                    // if none of those work, then spray
                }
                else
                {
                    BM8563_GetTime(&dateTime);
                    int currentMinute = dateTime.minute;
                    if ((currentMinute < meetingFinishedInitialTime) || ((currentMinute - meetingFinishedInitialTime) > 15))
                    {
                        // the meeting unit has begun
                        // TODO: mark current meeting unit as a valid meeting since the room was used for a complete meeting unit
                        delay = false;
                    }
                }
            }
        }
    }
}