#include "core2forAWS.h"
#include "powered_by_aws_logo.c"
#include "driver/gpio.h"

#define PIR_SENSING_PIN GPIO_NUM_32

static xQueueHandle gpio_evt_queue = NULL;
static char * tempString = "hello world!";
static lv_obj_t* tab_view;
static const char * btns[] ={"Apply", "Close", ""};

volatile int interruptFlag = 0;

static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void* arg)
{
    uint32_t io_num;
    for(;;) {
        if(xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY)) {
            printf("GPIO[%d] intr, val: %d\n", io_num, gpio_get_level(io_num));
        }
    }
}

static void event_handler(lv_obj_t * obj, lv_event_t event)
{
    if(event == LV_EVENT_VALUE_CHANGED) {
        printf("Button: %s\n", lv_msgbox_get_active_btn_text(obj));
    }
}

void app_main(void)
{

    int counter = 0;

    Core2ForAWS_Init();
    Core2ForAWS_Display_SetBrightness(80);
    
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    lv_obj_t * mbox1 = lv_msgbox_create(lv_scr_act(), NULL);
    lv_msgbox_set_text(mbox1, "A message box with two buttons.");
    lv_msgbox_add_btns(mbox1, btns);
    lv_obj_set_width(mbox1, 200);
    lv_obj_set_event_cb(mbox1, event_handler);
    lv_obj_align(mbox1, NULL, LV_ALIGN_CENTER, 0, 0); 
    xSemaphoreGive(xGuiSemaphore);
    
    gpio_config_t pGPIOConfig;
    pGPIOConfig.pin_bit_mask = (1ULL << PIR_SENSING_PIN);
    pGPIOConfig.mode = GPIO_MODE_INPUT;
    pGPIOConfig.pull_up_en = 0;
    pGPIOConfig.pull_down_en = 1;
    pGPIOConfig.intr_type = GPIO_INTR_POSEDGE;

    gpio_config(&pGPIOConfig);

    gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
    xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

    gpio_install_isr_service(0);
    gpio_isr_handler_add(PIR_SENSING_PIN, gpio_isr_handler, (void*) PIR_SENSING_PIN);
    char val = tempString[2];
    while (1)
    {
        for (int i = 0; i < 10000; i++)
        {
        }
        counter = (counter + 1) % 10000;
        vTaskDelay(1000);
        lv_msgbox_set_text(mbox1, tempString);
    }
}