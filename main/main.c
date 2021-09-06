#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "core2forAWS.h"

#include "wifi.h"
#include "ui.h"
#include "iot.h"
#include "appRTC.h"
#include "meeting.h"
#include "pir.h"
#include "servo.h"
#include "sound.h"
#include "mister.h"
#include "robot.h"

void app_main()
{
    Core2ForAWS_Init();
    Core2ForAWS_Display_SetBrightness(80);
    Core2ForAWS_LED_Enable(1);

    boot_ui_init();
    wifi_init();
    app_rtc_init();
    meeting_init();
    pir_init();
    servo_init();
    mister_init();
    robot_init();

    xTaskCreatePinnedToCore(&pir_task, "pir_task", 4096 * 2, NULL, 4, NULL, 0);
    xTaskCreatePinnedToCore(&meeting_task, "meeting_task", 4096 * 2, NULL, 3, NULL, 0);
    xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 4096 * 2, NULL, 5, NULL, 1);
}
