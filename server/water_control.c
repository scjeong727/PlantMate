
#include <string.h>
#include <dirent.h>
#include <stdio.h>


void water_control_init(WaterControl* control)
{
    int i;

    if (!control) return;

    memset(control, 0, sizeof(WaterControl));
    pthread_mutex_init(&control->mutex, NULL);

    control->count = 0;

    for (i = 0; i < MAX_WATER_RESOURCES; ++i) {
        control->resources[i].index = i;
        control->resources[i].device_path[0] = '\0';
        control->resources[i].in_use = 0;
        control->resources[i].owner_sock = -1;
    }

    water_control_refresh_device_list(control);
}

void water_control_cleanup(WaterControl* control)
{
    if (!control) return;
    pthread_mutex_destroy(&control->mutex);
}

void water_control_set_device_path(WaterControl* control, int index, const char* path)
{
    if (!control || !path) return;
    if (index < 0 || index >= MAX_WATER_RESOURCES) return;

    pthread_mutex_lock(&control->mutex);

    if (index >= control->count) {
        control->count = index + 1;
    }

    strncpy(control->resources[index].device_path, path,
            sizeof(control->resources[index].device_path) - 1);
    control->resources[index].device_path[sizeof(control->resources[index].device_path) - 1] = '\0';

    pthread_mutex_unlock(&control->mutex);
}

int water_control_get_device_path(WaterControl* control, int index, char* out, size_t out_size)
{
    if (!control || !out || out_size == 0) return 0;
    if (index < 0 || index >= MAX_WATER_RESOURCES) return 0;

    pthread_mutex_lock(&control->mutex);

    if (control->resources[index].device_path[0] == '\0') {
        pthread_mutex_unlock(&control->mutex);
        return 0;
    }

    strncpy(out, control->resources[index].device_path, out_size - 1);
    out[out_size - 1] = '\0';

    pthread_mutex_unlock(&control->mutex);
    return 1;
}

int water_control_acquire(WaterControl* control, int index, int owner_sock)
{
    if (!control) return 0;
    if (index < 0 || index >= control->count) return 0;
    if (owner_sock < 0) return 0;

    pthread_mutex_lock(&control->mutex);

    if (control->resources[index].in_use) {
        pthread_mutex_unlock(&control->mutex);
        return 0;
    }

    control->resources[index].in_use = 1;
    control->resources[index].owner_sock = owner_sock;

    pthread_mutex_unlock(&control->mutex);
    return 1;
}

int water_control_release_by_owner(WaterControl* control, int owner_sock)
{
    int i;
    int released = 0;

    if (!control) return 0;

    pthread_mutex_lock(&control->mutex);

    for (i = 0; i < control->count; ++i) {
        if (control->resources[i].in_use &&
            control->resources[i].owner_sock == owner_sock) {
            control->resources[i].in_use = 0;
            control->resources[i].owner_sock = -1;
            released = 1;
        }
    }

    pthread_mutex_unlock(&control->mutex);
    return released;
}

int water_control_is_available(WaterControl* control, int index)
{
    int available;

    if (!control) return 0;
    if (index < 0 || index >= control->count) return 0;

    pthread_mutex_lock(&control->mutex);
    available = (control->resources[index].in_use == 0);
    pthread_mutex_unlock(&control->mutex);

    return available;
}

void water_control_refresh_device_list(WaterControl* control)
{
    DIR* dir;
    struct dirent* ent;
    WaterResource old_resources[MAX_WATER_RESOURCES];
    int old_count;
    int i, j;

    if (!control) return;

    pthread_mutex_lock(&control->mutex);

    old_count = control->count;
    for (i = 0; i < old_count; ++i) {
        old_resources[i] = control->resources[i];
    }

    for (i = 0; i < MAX_WATER_RESOURCES; ++i) {
        control->resources[i].index = i;
        control->resources[i].device_path[0] = '\0';
        control->resources[i].in_use = 0;
        control->resources[i].owner_sock = -1;
    }
    control->count = 0;

    dir = opendir("/dev");
    if (!dir) {
        pthread_mutex_unlock(&control->mutex);
        return;
    }

    while ((ent = readdir(dir)) != NULL && control->count < MAX_WATER_RESOURCES) {
        char full_path[256];
        int found_old = -1;

        if (strncmp(ent->d_name, "ttyACM", 6) != 0 &&
            strncmp(ent->d_name, "ttyUSB", 6) != 0)
            continue;

        if (snprintf(full_path, sizeof(full_path), "/dev/%s", ent->d_name) >= (int)sizeof(full_path))
            continue;

        for (j = 0; j < old_count; ++j) {
            if (strcmp(old_resources[j].device_path, full_path) == 0) {
                found_old = j;
                break;
            }
        }

        {
            size_t len = strlen(full_path);
            if (len >= sizeof(control->resources[control->count].device_path))
                len = sizeof(control->resources[control->count].device_path) - 1;

            memcpy(control->resources[control->count].device_path, full_path, len);
            control->resources[control->count].device_path[len] = '\0';
        }
        control->resources[control->count].index = control->count;
        if (found_old >= 0) {
            control->resources[control->count].in_use = old_resources[found_old].in_use;
            control->resources[control->count].owner_sock = old_resources[found_old].owner_sock;
        }

        control->count++;
    }

    closedir(dir);
    pthread_mutex_unlock(&control->mutex);
}

int water_control_acquire_by_path(WaterControl* control, const char* device_path, int owner_sock)
{
    int i, index = -1;

    if (!control || !device_path || device_path[0] == '\0' || owner_sock < 0)
        return 0;

    pthread_mutex_lock(&control->mutex);

    for (i = 0; i < control->count; ++i) {
        if (strcmp(control->resources[i].device_path, device_path) == 0) {
            index = i;
            break;
        }
    }

    if (index < 0) {
        pthread_mutex_unlock(&control->mutex);
        return 0;
    }

    if (control->resources[index].in_use &&
        control->resources[index].owner_sock != owner_sock) {
        pthread_mutex_unlock(&control->mutex);
        return 0;
    }

    for (i = 0; i < control->count; ++i) {
        if (i == index) continue;
        if (control->resources[i].in_use &&
            control->resources[i].owner_sock == owner_sock) {
            control->resources[i].in_use = 0;
            control->resources[i].owner_sock = -1;
        }
    }

    control->resources[index].in_use = 1;
    control->resources[index].owner_sock = owner_sock;

    pthread_mutex_unlock(&control->mutex);
    return 1;
}