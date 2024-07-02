#include "NetworkTask.h"
#include <tools_c/StringEquals.h>
#include <tools_c/TimeUtils.h>
#include <tools_c/ZeroMemory.h>
#include "Constants.h"
#include "IoTask.h"
#include "StartTask.h"

static bool fillNow(struct timeval* now)
{
    void* const ignore_timezone = NULL;
    return gettimeofday(now, ignore_timezone) == 0;
}

int networkTask(void* arg)
{
    NetworkTaskParams* p = (NetworkTaskParams*)arg;
    startTask(p->name, p->delay);

    CREATE_ZEROED(ScheduledTasks, tasks);

    Socket socket =
        createSocket(&p->config.locals, p->config.protocol, &tasks, isUnix(p->config.protocol) ? AF_UNIX : AF_UNSPEC);
    listenSocket(&socket, (BacklogCount)10);
    connectSocket(&socket, &p->config.remotes);

    struct timeval now, time_to_die;

    const bool should_live_forever = isZero(&p->lifetime);
    if (not should_live_forever)
    {
        const long lifetime_usec = p->lifetime.tv_nsec / 1000;

        DEBUG_LOG("This task will die in %ds:%dus", p->lifetime.tv_sec, lifetime_usec);

        fillNow(&now);
        time_to_die.tv_sec = now.tv_sec + p->lifetime.tv_sec;
        time_to_die.tv_usec = now.tv_usec + lifetime_usec;
    }

    while (should_live_forever or (fillNow(&now) and compareTime(&now, &time_to_die) <= 0))
    {
        launchScheduledTasks(&tasks);
        receiveSocket(&socket);

        ChatMessage send_msg = chat(NULL);
        if (not empty(send_msg))
        {
            if (not equals(send_msg, quit_msg))
                sendSocket(&socket, send_msg);
            else
            {
                chat(quit_msg);
                break;
            }
        }
    }

    cleanUpSocket(&socket);

    return 0;
}
