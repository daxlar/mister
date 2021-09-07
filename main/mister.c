#include "core2forAWS.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"

#define MISTER_PIN GPIO_NUM_26
#define OFF 0
#define ON 1
static char TAG[] = "mister";

static int mister_state;

void mister_init()
{
    gpio_config_t pGPIOConfig;
    pGPIOConfig.pin_bit_mask = (1ULL << MISTER_PIN);
    pGPIOConfig.mode = GPIO_MODE_OUTPUT;
    pGPIOConfig.pull_up_en = 0;
    pGPIOConfig.pull_down_en = 0;
    pGPIOConfig.intr_type = GPIO_INTR_DISABLE;
    gpio_config(&pGPIOConfig);

    mister_state = OFF;

    ESP_LOGI(TAG, "mister init complete");

    gpio_set_level(MISTER_PIN, 0);
}

void mister_spray_on()
{
    if (mister_state == ON)
    {
        return;
    }
    gpio_set_level(MISTER_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(2000));
    mister_state = ON;
    ESP_LOGI(TAG, "mister turned on");
}

void mister_spray_off()
{
    if (mister_state == OFF)
    {
        return;
    }
    gpio_set_level(MISTER_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level(MISTER_PIN, 1);
    vTaskDelay(pdMS_TO_TICKS(2000));
    gpio_set_level(MISTER_PIN, 0);
    vTaskDelay(pdMS_TO_TICKS(2000));
    mister_state = OFF;
    ESP_LOGI(TAG, "mister turned off");
}