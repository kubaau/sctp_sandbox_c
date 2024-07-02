#include "StartTask.h"
#include <tools_c/Logging.h>
#include <tools_c/TimeUtils.h>

void startTask(const char* name, Duration delay)
{
    threadName(name);

    if (not isZero(&delay))
    {
        DEBUG_LOG("Task delayed for %ds:%dns", delay.tv_sec, delay.tv_nsec);
        nanosleep(&delay, NULL);
    }

    DEBUG_LOG("Starting task %s", name);
}
