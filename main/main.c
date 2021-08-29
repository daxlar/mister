#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <unistd.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "core2forAWS.h"
#include "esp_log.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#include "wifi.h"
#include "ui.h"
#include "appRTC.h"
#include "iot.h"
#include "pir.h"
#include "servo.h"

void app_main()
{
    Core2ForAWS_Init();
    Core2ForAWS_Display_SetBrightness(80);
    Core2ForAWS_LED_Enable(1);

    magic_ui_init();
    ui_init();
    wifi_init();
    pir_init();
    servo_init();
    app_rtc_init();
    iot_init();

    xTaskCreatePinnedToCore(&aws_iot_task, "aws_iot_task", 4096 * 2, NULL, 5, NULL, 1);
}
