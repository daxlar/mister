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

void meeting_init();
xQueueHandle get_meetingEnd_evt_queue();
void meeting_task(void *params);

#endif