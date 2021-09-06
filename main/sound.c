#include <stdio.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

#include "core2forAWS.h"

extern const unsigned char music[120264];

void sound_init()
{
    Speaker_Init();
}

void play_sound()
{
    Speaker_Init();
    Core2ForAWS_Speaker_Enable(1);
    Speaker_WriteBuff((uint8_t *)music, 120264, portMAX_DELAY);
    Core2ForAWS_Speaker_Enable(0);
    Speaker_Deinit();
}

void sound_task(void *arg)
{
    Speaker_Init();
    Core2ForAWS_Speaker_Enable(1);
    Speaker_WriteBuff((uint8_t *)music, 120264, portMAX_DELAY);
    Core2ForAWS_Speaker_Enable(0);
    Speaker_Deinit();
    vTaskDelete(NULL); // Deletes the current task from FreeRTOS task list and the FreeRTOS idle task will remove from memory.
}
