#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "freertos/event_groups.h"

#include "esp_log.h"

#include "core2forAWS.h"
#include "ui.h"

#define MAX_TEXTAREA_LENGTH 1024

static lv_obj_t *out_txtarea;
static lv_obj_t *wifi_label;

static char *TAG = "UI";
static char timeOutString[] = "Time left to confirm activity: 30";
static const char *btns[] = {"Confirm", ""};
static SemaphoreHandle_t confirmationSemaphore;

volatile bool confirmation = false;

static void event_handler(lv_obj_t *obj, lv_event_t event)
{
    if (event == LV_EVENT_VALUE_CHANGED)
    {
        printf("Button: %s\n", lv_msgbox_get_active_btn_text(obj));

        xSemaphoreTake(confirmationSemaphore, portMAX_DELAY);
        confirmation = true;
        xSemaphoreGive(confirmationSemaphore);
    }
}

static void ui_textarea_prune(size_t new_text_length)
{
    const char *current_text = lv_textarea_get_text(out_txtarea);
    size_t current_text_len = strlen(current_text);
    if (current_text_len + new_text_length >= MAX_TEXTAREA_LENGTH)
    {
        for (int i = 0; i < new_text_length; i++)
        {
            lv_textarea_set_cursor_pos(out_txtarea, 0);
            lv_textarea_del_char_forward(out_txtarea);
        }
        lv_textarea_set_cursor_pos(out_txtarea, LV_TEXTAREA_CURSOR_LAST);
    }
}

void ui_textarea_add(char *baseTxt, char *param, size_t paramLen)
{
    if (baseTxt != NULL)
    {
        xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
        if (param != NULL && paramLen != 0)
        {
            size_t baseTxtLen = strlen(baseTxt);
            ui_textarea_prune(paramLen);
            size_t bufLen = baseTxtLen + paramLen;
            char buf[(int)bufLen];
            sprintf(buf, baseTxt, param);
            lv_textarea_add_text(out_txtarea, buf);
        }
        else
        {
            lv_textarea_add_text(out_txtarea, baseTxt);
        }
        xSemaphoreGive(xGuiSemaphore);
    }
    else
    {
        ESP_LOGE(TAG, "Textarea baseTxt is NULL!");
    }
}

void ui_wifi_label_update(bool state)
{
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    if (state == false)
    {
        lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    }
    else
    {
        char buffer[25];
        sprintf(buffer, "#0000ff %s #", LV_SYMBOL_WIFI);
        lv_label_set_text(wifi_label, buffer);
    }
    xSemaphoreGive(xGuiSemaphore);
}

void boot_ui_init()
{
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    wifi_label = lv_label_create(lv_scr_act(), NULL);
    lv_obj_align(wifi_label, NULL, LV_ALIGN_IN_TOP_RIGHT, 0, 6);
    lv_label_set_text(wifi_label, LV_SYMBOL_WIFI);
    lv_label_set_recolor(wifi_label, true);

    out_txtarea = lv_textarea_create(lv_scr_act(), NULL);
    lv_obj_set_size(out_txtarea, 300, 180);
    lv_obj_align(out_txtarea, NULL, LV_ALIGN_IN_BOTTOM_MID, 0, -12);
    lv_textarea_set_max_length(out_txtarea, MAX_TEXTAREA_LENGTH);
    lv_textarea_set_text_sel(out_txtarea, false);
    lv_textarea_set_cursor_hidden(out_txtarea, true);
    lv_textarea_set_text(out_txtarea, "Starting mister\n");
    xSemaphoreGive(xGuiSemaphore);
}

// must be called within a task!
bool occupancy_ui_init()
{
    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    lv_obj_clean(lv_scr_act());
    lv_obj_t *mbox1 = lv_msgbox_create(lv_scr_act(), NULL);
    lv_msgbox_set_text(mbox1, timeOutString);
    lv_msgbox_add_btns(mbox1, btns);
    lv_obj_set_width(mbox1, 200);
    lv_obj_set_event_cb(mbox1, event_handler);
    lv_obj_align(mbox1, NULL, LV_ALIGN_CENTER, 0, 0);
    xSemaphoreGive(xGuiSemaphore);

    confirmationSemaphore = xSemaphoreCreateMutex();

    int secondsTimer = 30;
    int timeOutStringLen = strlen(timeOutString);
    int firstDigitIndex = timeOutStringLen - 2;
    int secondDigitIndex = timeOutStringLen - 1;

    int firstDigitOffset = 3;
    int secondDigitOffset = 0;

    for (int i = secondsTimer; i >= 0; i--)
    {
        vTaskDelay(pdMS_TO_TICKS(1000));
        firstDigitOffset = i / 10;
        secondDigitOffset = i % 10;
        if (firstDigitOffset == 0)
        {
            timeOutString[firstDigitIndex] = secondDigitOffset + '0';
            timeOutString[secondDigitIndex] = ' ';
        }
        else
        {
            timeOutString[firstDigitIndex] = firstDigitOffset + '0';
            timeOutString[secondDigitIndex] = secondDigitOffset + '0';
        }

        bool getConfirmVal = false;
        xSemaphoreTake(confirmationSemaphore, portMAX_DELAY);
        getConfirmVal = confirmation;
        xSemaphoreGive(confirmationSemaphore);

        if (getConfirmVal)
        {
            break;
        }

        lv_msgbox_set_text(mbox1, timeOutString);
    }

    bool haveConfirmed = false;
    xSemaphoreTake(confirmationSemaphore, portMAX_DELAY);
    haveConfirmed = confirmation;
    xSemaphoreGive(confirmationSemaphore);

    if (haveConfirmed)
    {
        xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
        lv_obj_clean(lv_scr_act());
        lv_obj_t *mbox2 = lv_msgbox_create(lv_scr_act(), NULL);
        lv_msgbox_set_text(mbox2, "Thank you for confirming!");
        lv_obj_set_width(mbox2, 200);
        lv_obj_align(mbox2, NULL, LV_ALIGN_CENTER, 0, 0);
        xSemaphoreGive(xGuiSemaphore);

        xSemaphoreTake(confirmationSemaphore, portMAX_DELAY);
        confirmation = false;
        xSemaphoreGive(confirmationSemaphore);

        return haveConfirmed;
    }

    xSemaphoreTake(xGuiSemaphore, portMAX_DELAY);
    lv_obj_clean(lv_scr_act());
    lv_obj_t *mbox2 = lv_msgbox_create(lv_scr_act(), NULL);
    lv_msgbox_set_text(mbox2, "Starting the misting robot!");
    lv_obj_set_width(mbox2, 200);
    lv_obj_align(mbox2, NULL, LV_ALIGN_CENTER, 0, 0);
    xSemaphoreGive(xGuiSemaphore);

    return haveConfirmed;
}
