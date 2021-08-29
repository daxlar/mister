#include "core2forAWS.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_log.h"

#define PIR_SENSING_PIN GPIO_NUM_32

static xQueueHandle gpio_evt_queue = NULL;
static char TAG[] = "pir";

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
    uint32_t gpio_num = (uint32_t)arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void *arg)
{
    uint32_t io_num;
    for (;;)
    {
        if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
        {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

void pir_init()
{
    gpio_config_t pGPIOConfig;
    pGPIOConfig.pin_bit_mask = (1ULL << PIR_SENSING_PIN);
    pGPIOConfig.mode = GPIO_MODE_INPUT;
    pGPIOConfig.pull_up_en = 0;
    pGPIOConfig.pull_down_en = 1;
    pGPIOConfig.intr_type = GPIO_INTR_POSEDGE;

    gpio_config(&pGPIOConfig);

    ESP_LOGI(TAG, "pir init complete");
}

void pir_start()
{
    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIR_SENSING_PIN, gpio_isr_handler, (void *)PIR_SENSING_PIN);

    ESP_LOGI(TAG, "pir start");
}