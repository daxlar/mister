#ifndef UI_H
#define UI_h

void ui_textarea_add(char *txt, char *param, size_t paramLen);
void ui_wifi_label_update(bool state);
void boot_ui_init();
bool occupancy_ui_init();

#endif