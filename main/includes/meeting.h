#ifndef MEETING_H
#define MEETING_H

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "esp_log.h"

struct MeetingInterval
{
    int beginInMinutes;
    int endInMinutes;
};

#define MEETING_DEAD_ZONE_IN_MINUTES 3 // the time before the meeting that if someone is still in the room, it will count as the next meeting

void meeting_init();
xQueueHandle get_meetingEnd_evt_queue();
void meeting_task(void *params);
void mark_meeting_unit(int meetingUnitIndex);
void mark_meeting_unit_interval(int intervalStartIdx, int intervalEndIdx);

#endif