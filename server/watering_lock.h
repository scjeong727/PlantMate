#ifndef WATERING_LOCK_H
#define WATERING_LOCK_H

void watering_lock_init(void);
int watering_try_begin(int plant_id);
void watering_end(int plant_id);

#endif
