#include "core2forAWS.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "meeting.h"
#include "sound.h"
#include "ui.h"
#include "robot.h"

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

    gpio_intr_disable(PIR_SENSING_PIN);
    xQueueReset(gpio_evt_queue);
    ESP_LOGI(TAG, "pir init complete");
}

static bool check_meeting_spillover(int meetingFinishedInitialTime)
{
    rtc_date_t dateTime;
    BM8563_GetTime(&dateTime);
    bool delaySpillover = dateTime.minute < meetingFinishedInitialTime;      // checking the spillover from 59 -> 00
    bool delayOverlap = (dateTime.minute - meetingFinishedInitialTime) > 15; // checking the spill over into another meeting unit
    return (delayOverlap || delaySpillover);
}

void pir_task(void *params)
{
    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIR_SENSING_PIN, gpio_isr_handler, (void *)PIR_SENSING_PIN);
    ESP_LOGI(TAG, "pir start");

    uint32_t io_num;
    uint32_t meetingFinished;
    int meetingFinishedInitialTime;
    int meetingUnitIndex;
    int delayTimeinMs;
    bool delay;
    rtc_date_t dateTime;

    while (1)
    {
        if (xQueueReceive(meeting_end_response_queue, &meetingFinished, portMAX_DELAY))
        {
            ESP_LOGI(TAG, "got something from the queue");
            BM8563_GetTime(&dateTime);
            meetingFinishedInitialTime = dateTime.minute;
            meetingUnitIndex = (dateTime.hour * 60 + dateTime.minute) / 15;
            delayTimeinMs = 2 * 60 * 1000; // wait for movement for 2 minute
            delay = true;
            // calculate the delay here to wait for movement, if movement, back off, then wait again until the meething threshold
            while (delay)
            {
                gpio_intr_enable(PIR_SENSING_PIN);

                if (xQueueReceive(gpio_evt_queue, &io_num, pdMS_TO_TICKS(delayTimeinMs)))
                {
                    ESP_LOGI(TAG, "received interrupt on pin number: %d", io_num);
                    if (check_meeting_spillover(meetingFinishedInitialTime))
                    {
                        mark_meeting_unit(meetingUnitIndex); // delayed long enough that it counts as a new meeting unit
                        break;
                    }
                    continue;
                }

                if (check_meeting_spillover(meetingFinishedInitialTime))
                {
                    mark_meeting_unit(meetingUnitIndex); // delayed long enough that it counts as a new meeting unit
                    break;
                }

                xTaskCreatePinnedToCore(&robot_task, "robotTask", 4096 * 2, NULL, 6, NULL, 1);
                gpio_intr_disable(PIR_SENSING_PIN);
                xQueueReset(gpio_evt_queue);

                if (get_robot_status() == ROBOT_FINISHED) // this should be an event group
                {
                    ESP_LOGI(TAG, "robot finished!");
                    break;
                }
                ESP_LOGI(TAG, "robot terminated");
            }
        }
    }
}