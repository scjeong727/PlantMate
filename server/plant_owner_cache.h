#ifndef PLANT_OWNER_CACHE_H
#define PLANT_OWNER_CACHE_H

void plant_owner_cache_init(void);
void plant_owner_cache_set(int plant_id, int user_id);
void plant_owner_cache_remove(int plant_id);
int plant_owner_cache_exists_by_user(int plant_id, int user_id);

#endif
