#include "Chat.h"
#include <tools_c/Logging.h>

thread_local ChatBuffer old_chat_msg;

static mtx_t createMutex(void)
{
    DEBUG_LOG("Initializing mutex");
    mtx_t mtx;
    mtx_init(&mtx, mtx_plain);
    return mtx;
}

static void initChatMutex(mtx_t* chat_mtx)
{
    static bool chat_mtx_init = false;
    if (not chat_mtx_init)
    {
        *chat_mtx = createMutex();
        chat_mtx_init = true;
    }
}

static void exchangeChatMessages(ChatMessage new_chat_msg)
{
    static ChatBuffer chat_msg;
    strcpy(old_chat_msg, chat_msg);
    if (new_chat_msg)
        strcpy(chat_msg, new_chat_msg);
    else
        chat_msg[0] = 0;
}

ChatMessage chat(ChatMessage new_chat_msg)
{
    // static mtx_t chat_mtx = PTHREAD_MUTEX_INITIALIZER;
    static mtx_t chat_mtx;
    initChatMutex(&chat_mtx);

    mtx_lock(&chat_mtx);
    exchangeChatMessages(new_chat_msg);
    mtx_unlock(&chat_mtx);

    return old_chat_msg;
}
