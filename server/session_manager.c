#include "session_manager.h"
#include <pthread.h>
#include <string.h>

#define MAX_LOGIN_SESSIONS 128

typedef struct {
    int in_use;
    int client_sock;
    int user_id;
    char login_id[64];
} LoginSession;

static LoginSession g_sessions[MAX_LOGIN_SESSIONS];
static pthread_mutex_t g_sessions_mutex = PTHREAD_MUTEX_INITIALIZER;

bool session_try_bind_login(int client_sock, const char* login_id, int user_id)
{
    int i, empty_idx = -1;

    pthread_mutex_lock(&g_sessions_mutex);

    for (i = 0; i < MAX_LOGIN_SESSIONS; ++i) {
        if (g_sessions[i].in_use &&
            strcmp(g_sessions[i].login_id, login_id) == 0) {
            pthread_mutex_unlock(&g_sessions_mutex);
            return false;
        }
        if (!g_sessions[i].in_use && empty_idx == -1)
            empty_idx = i;
    }

    if (empty_idx == -1) {
        pthread_mutex_unlock(&g_sessions_mutex);
        return false;
    }

    g_sessions[empty_idx].in_use = 1;
    g_sessions[empty_idx].client_sock = client_sock;
    g_sessions[empty_idx].user_id = user_id;
    strncpy(g_sessions[empty_idx].login_id, login_id, sizeof(g_sessions[empty_idx].login_id) - 1);
    g_sessions[empty_idx].login_id[sizeof(g_sessions[empty_idx].login_id) - 1] = '\0';

    pthread_mutex_unlock(&g_sessions_mutex);
    return true;
}

void session_remove_by_sock(int client_sock)
{
    int i;

    pthread_mutex_lock(&g_sessions_mutex);
    for (i = 0; i < MAX_LOGIN_SESSIONS; ++i) {
        if (g_sessions[i].in_use && g_sessions[i].client_sock == client_sock) {
            memset(&g_sessions[i], 0, sizeof(g_sessions[i]));
            break;
        }
    }
    pthread_mutex_unlock(&g_sessions_mutex);
}

bool session_is_login_active(const char* login_id)
{
    int i;
    bool found = false;

    pthread_mutex_lock(&g_sessions_mutex);

    for (i = 0; i < MAX_LOGIN_SESSIONS; ++i) {
        if (g_sessions[i].in_use &&
            strcmp(g_sessions[i].login_id, login_id) == 0) {
            found = true;
            break;
        }
    }

    pthread_mutex_unlock(&g_sessions_mutex);
    return found;
}

int session_get_user_id_by_sock(int client_sock)
{
    int i;
    int user_id = -1;

    pthread_mutex_lock(&g_sessions_mutex);

    for (i = 0; i < MAX_LOGIN_SESSIONS; ++i) {
        if (g_sessions[i].in_use && g_sessions[i].client_sock == client_sock) {
            user_id = g_sessions[i].user_id;
            break;
        }
    }

    pthread_mutex_unlock(&g_sessions_mutex);
    return user_id;
}
