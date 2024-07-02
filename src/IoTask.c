#include "IoTask.h"
#include <tools_c/ErrorChecks.h>
#include <tools_c/Logging.h>
#include <tools_c/StringEquals.h>
#include "Chat.h"
#include "Constants.h"
#include "StartTask.h"

static void readLine(ChatBuffer msg)
{
    checkFgets(fgets(msg, chat_buffer_size, stdin));
    msg[strcspn(msg, "\r\n")] = 0;
}

static void chatFromStdin(ChatBuffer msg)
{
    readLine(msg);
    chat(msg);
}

int ioTask(void* arg)
{
    (void)arg;

    Duration no_delay = {0};
    startTask("io", no_delay);

    ChatBuffer msg;
    while (not equals(msg, quit_msg))
        chatFromStdin(msg);

    return 0;
}
