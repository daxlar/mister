#ifndef PIR_H
#define PIR_H

void pir_init();
void pir_start();
void pir_task(void *params);
xQueueHandle getPirQueueHandle();

#endif