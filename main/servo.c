#include "core2forAWS.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "driver/mcpwm.h"

#define SERVO_OUTPUT_IO GPIO_NUM_33
#define SERVO_MIN_PULSEWIDTH_US (1000) // Minimum pulse width in microsecond
#define SERVO_MAX_PULSEWIDTH_US (2000) // Maximum pulse width in microsecond
#define SERVO_MAX_DEGREE (90)          // Maximum angle in degree upto which servo can rotate

static char TAG[] = "servo";

void servo_init()
{
    mcpwm_gpio_init(MCPWM_UNIT_0, MCPWM0A, SERVO_OUTPUT_IO); // To drive a RC servo, one MCPWM generator is enough

    mcpwm_config_t pwm_config = {
        .frequency = 50, // frequency = 50Hz, i.e. for every servo motor time period should be 20ms
        .cmpr_a = 0,     // duty cycle of PWMxA = 0
        .counter_mode = MCPWM_UP_COUNTER,
        .duty_mode = MCPWM_DUTY_MODE_0,
    };
    mcpwm_init(MCPWM_UNIT_0, MCPWM_TIMER_0, &pwm_config);
}

static void servo_start()
{
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 1000);
}

static void servo_stop()
{
    mcpwm_set_duty_in_us(MCPWM_UNIT_0, MCPWM_TIMER_0, MCPWM_OPR_A, 0);
}

void servo_rotate()
{
    servo_start();
    vTaskDelay(pdMS_TO_TICKS(200)); // 200 is magic number
    servo_stop();
    ESP_LOGI(TAG, "servo stopped");
}
