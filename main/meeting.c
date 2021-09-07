#include "core2forAWS.h"
#include "meeting.h"
#include "pir.h"

#define TOTAL_MEETING_INTERVALS 96        // 96 intervals every 24 hours
#define MEETING_BUFFER_DURATION_MINUTES 3 // duration to wait after the end of a meeting to see if another meeting will be scheduled
#define EIGHT_AM_IN_MEETING_UNITS 32
#define FOUR_PM_IN_MEETING_UINTS 64

static SemaphoreHandle_t meetingUnitSemaphore;
static xQueueHandle meetingEnd_evt_queue = NULL;
static char TAG[] = "meeting";

int meetingUnits[TOTAL_MEETING_INTERVALS];

int binaryTestFlag = 0;

void meeting_init()
{
    // should be number of meetingUnits
    meetingEnd_evt_queue = xQueueCreate(10, sizeof(struct MeetingInterval));
    meetingUnitSemaphore = xSemaphoreCreateMutex();
    for (int i = 0; i < TOTAL_MEETING_INTERVALS; i++)
    {
        meetingUnits[i] = 0;
    }
}

xQueueHandle get_meetingEnd_evt_queue()
{
    return meetingEnd_evt_queue;
}

void mark_meeting_unit(int meetingUnitIndex)
{
    ESP_LOGI(TAG, "trying to mark meeting units!");
    xSemaphoreTake(meetingUnitSemaphore, portMAX_DELAY);
    meetingUnits[meetingUnitIndex] = 1;
    xSemaphoreGive(meetingUnitSemaphore);
    ESP_LOGI(TAG, "finished marking meeting units");
}

void mark_meeting_unit_interval(int intervalStartIdx, int intervalEndIdx)
{
    ESP_LOGI(TAG, "start index: %d, end index: %d", intervalStartIdx, intervalEndIdx);
    for (int i = intervalStartIdx; i < intervalEndIdx; i++)
    {
        mark_meeting_unit(i);
    }
}

static void delay_until_next_meeting_unit()
{
    rtc_date_t dateTime;
    BM8563_GetTime(&dateTime);
    int timeToDelayInMinutes = MEETING_MINIMUM_INTERVAL - (dateTime.minute % MEETING_MINIMUM_INTERVAL);
    ESP_LOGI(TAG, "time to wait: %d minutes, starting from: %d", timeToDelayInMinutes, dateTime.minute);
    TickType_t xLastWakeTime = xTaskGetTickCount();
    vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(timeToDelayInMinutes * 60 * 1000));
}

void meeting_task(void *params)
{
    ESP_LOGI(TAG, "starting the meeting task!");
    rtc_date_t dateTime;
    int currentMeetingUnitsIndex;
    int currentMeetingUnitState;
    int previousMeetingUnitState;
    bool activeFreeMeetingEdge;

    // preemptively try to get all the available meeting intervals
    // check to see if there is meeting scheduled right now
    // if there is a meeting scheduled right now, wait until 3 minutes after meeting ends and check if there were more meeting scheduled
    // if there are immediate meetings scheduled right after, keep looping
    // if there is no meeting scheduled right now, but there was one previously activate system. then loop

    while (1)
    {
        BM8563_GetTime(&dateTime);
        currentMeetingUnitsIndex = (dateTime.hour * 60 + dateTime.minute) / MEETING_MINIMUM_INTERVAL;
        currentMeetingUnitState = 0;
        previousMeetingUnitState = 0;
        activeFreeMeetingEdge = false;

        xSemaphoreTake(meetingUnitSemaphore, portMAX_DELAY);
        currentMeetingUnitState = meetingUnits[currentMeetingUnitsIndex];
        if (currentMeetingUnitsIndex > 0)
        {
            previousMeetingUnitState = meetingUnits[currentMeetingUnitsIndex - 1];
            if (currentMeetingUnitState == 0 && previousMeetingUnitState == 1)
            {
                activeFreeMeetingEdge = true;
            }
        }
        xSemaphoreGive(meetingUnitSemaphore);

        if (currentMeetingUnitState == 1)
        {
            ESP_LOGI(TAG, "currently active meeting %d", currentMeetingUnitsIndex);
            delay_until_next_meeting_unit();
            ESP_LOGI(TAG, "coming from active meeting %d", currentMeetingUnitsIndex);
            continue;
        }

        // start system if coming out of a meeting and currently no meeting
        if (activeFreeMeetingEdge)
        {
            uint32_t start = 1;
            ESP_LOGI(TAG, "starting the system!");
            xQueueSend(getPirQueueHandle(), &start, 0);
            delay_until_next_meeting_unit();
        }
    }
}