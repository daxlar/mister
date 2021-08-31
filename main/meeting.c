#include "core2forAWS.h"
#include "meeting.h"

#define TOTAL_MEETING_INTERVALS 32
#define MEETING_MINIMUM_INTERVAL 15
// duration to wait after the end of a meeting to see if another meeting will be scheduled
#define MEETING_BUFFER_DURATION_MINUTES 3
#define EIGHT_AM_IN_MEETING_UNITS 32
#define FOUR_PM_IN_MEETING_UINTS 64

static xQueueHandle meetingEnd_evt_queue = NULL;
static char TAG[] = "meeting";

int meetingUnits[TOTAL_MEETING_INTERVALS];

void meeting_init()
{
    meetingEnd_evt_queue = xQueueCreate(10, sizeof(struct MeetingInterval));
}

xQueueHandle get_meetingEnd_evt_queue()
{
    return meetingEnd_evt_queue;
}

void meeting_task(void *params)
{
    ESP_LOGI(TAG, "starting the meeting task!");
    rtc_date_t dateTime;
    int currentHour;
    int currentMinute;
    int meetingStartIdx;
    int meetingEndIdx;

    // preemptively try to get all the available meeting intervals
    // check to see if there is meeting scheduled right now
    // if there is a meeting scheduled right now, wait until 3 minutes after meeting ends and check if there were more meeting scheduled
    // if there are immediate meetings scheduled right after, keep looping
    // if there is no meeting scheduled right now, but there was one previously activate system. then loop

    while (1)
    {
        struct MeetingInterval meeting;
        while (xQueueReceive(meetingEnd_evt_queue, &meeting, pdMS_TO_TICKS(10)) == pdPASS)
        {
            ESP_LOGI(TAG, "meeting start from queue: %d", meeting.beginInMinutes);
            ESP_LOGI(TAG, "meeting end from queue: %d", meeting.endInMinutes);
            meetingStartIdx = meeting.beginInMinutes / MEETING_MINIMUM_INTERVAL;
            meetingEndIdx = meeting.endInMinutes / MEETING_MINIMUM_INTERVAL;
            ESP_LOGI(TAG, "Indexs: %d, %d", meetingStartIdx, meetingEndIdx);

            for (int i = meetingStartIdx; i < meetingEndIdx && ((i > EIGHT_AM_IN_MEETING_UNITS) && (i < FOUR_PM_IN_MEETING_UINTS)); i++)
            {
                meetingUnits[i] = 1;
            }
        }

        BM8563_GetTime(&dateTime);

        currentHour = dateTime.hour;
        currentMinute = dateTime.minute;
        int currentMeetingUnitsIndex = (currentHour * 60 + currentMinute) / 15;

        if (meetingUnits[currentMeetingUnitsIndex] == 1)
        {
            //since there can only be multiples of 15 minute meeting intervals
            int timeToDelayInMinutes = MEETING_BUFFER_DURATION_MINUTES + MEETING_MINIMUM_INTERVAL - (currentMinute % MEETING_MINIMUM_INTERVAL);
            ESP_LOGI(TAG, "time to wait: %d minutes", timeToDelayInMinutes);
            TickType_t xLastWakeTime = xTaskGetTickCount();
            // wait until the meeting ends + buffer time to check if system needs to activate
            vTaskDelayUntil(&xLastWakeTime, pdMS_TO_TICKS(timeToDelayInMinutes * 60 * 1000));
            continue;
        }

        // start system if coming out of a meeting and currently no meeting
        if (meetingEndIdx > 0 && ((meetingUnits[currentMeetingUnitsIndex - 1] == 0) && (meetingUnits[currentMeetingUnitsIndex] == 1)))
        {
            // start system now!
            while (1)
            {
                vTaskDelay(2000 / portTICK_PERIOD_MS);
                ESP_LOGI(TAG, "start the system now!");
            }
        }
    }
}