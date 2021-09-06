#ifndef ROBOT_H
#define ROBOT_H

#define ROBOT_FINISHED 0
#define ROBOT_DELAYED 1

void robot_init();
int get_robot_status();
void robot_task(void *params);

#endif