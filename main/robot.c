
#include "core2forAWS.h"

#include "robot.h"
#include "sound.h"
#include "ui.h"
#include "servo.h"
#include "mister.h"

extern const unsigned char music[120264];
static SemaphoreHandle_t robotStatusSemaphore;
static char TAG[] = "robot";

int robot_status = ROBOT_DELAYED;

void robot_init()
{
    robotStatusSemaphore = xSemaphoreCreateMutex();
    ESP_LOGI(TAG, "robot init");
}

int get_robot_status()
{
    int ret_val;
    xSemaphoreTake(robotStatusSemaphore, portMAX_DELAY);
    ret_val = robot_status;
    xSemaphoreGive(robotStatusSemaphore);
    ESP_LOGI(TAG, "finished getting status");
    return ret_val;
}

void robot_task(void *params)
{
    xSemaphoreTake(robotStatusSemaphore, portMAX_DELAY);
    play_sound();
    bool hasConfirmed = occupancy_ui_init();
    ESP_LOGI(TAG, "confirmationStatus: %d", hasConfirmed);
    if (hasConfirmed)
    {
        robot_status = ROBOT_DELAYED;
        xSemaphoreGive(robotStatusSemaphore);
        vTaskDelete(NULL);
    }

    mister_spray_on();
    servo_full_rotation();
    mister_spray_off();

    robot_status = ROBOT_FINISHED;
    xSemaphoreGive(robotStatusSemaphore);
    vTaskDelete(NULL);
}