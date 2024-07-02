#include <tools_c/AsyncTask.h>
#include <tools_c/PrintBacktrace.h>
#include <string.h>
#include "IoTask.h"
#include "NetworkTask.h"

ENABLE_LOGGING

static NetworkTaskParams network_params;
static NetworkTaskParams mme_params;
static NetworkTaskParams enb1_params;
static NetworkTaskParams enb2_params;

static void runPredefinedConfig(const NetworkConfiguration* config, AsyncTasks* tasks)
{
    const char* localhost = "127.0.0.1";
    // const char* nat = "10.0.2.15";
    const Port s1_port = 36412;
    const Port x2_port = 36422;

    ZERO_MEMORY(&mme_params);
    mme_params.name = "MME";
    mme_params.config = *config;
    mme_params.config.locals.sockets[0] = createIPSocket(localhost, s1_port);
    // mme_params.config.locals.sockets[1] = createIPSocket(nat, s1_port);
    mme_params.config.locals.count = 1;
    mme_params.config.remotes.count = 0;

    ZERO_MEMORY(&enb1_params);
    enb1_params.name = "eNB1";
    enb1_params.delay.tv_nsec = 100 * 1000 * 1000;
    enb1_params.config = *config;
    enb1_params.config.locals.sockets[0] = createIPSocket(localhost, x2_port);
    enb1_params.config.remotes.sockets[0] = mme_params.config.locals.sockets[0];
    enb1_params.config.locals.count = 1;
    enb1_params.config.remotes.count = 1;

    ZERO_MEMORY(&enb2_params);
    enb2_params.name = "eNB2";
    enb2_params.delay.tv_nsec = 200 * 1000 * 1000;
    enb2_params.lifetime.tv_sec = 13;
    enb2_params.config = *config;
    enb2_params.config.locals.sockets[0] = createIPSocket(localhost, x2_port + 1);
    enb2_params.config.remotes.sockets[0] = mme_params.config.locals.sockets[0];
    enb2_params.config.remotes.sockets[1] = mme_params.config.locals.sockets[0];
    enb2_params.config.remotes.sockets[1].port += 17;
    enb2_params.config.locals.count = 1;
    enb2_params.config.remotes.count = 2;

    startTask(tasks, networkTask, &mme_params);
    startTask(tasks, networkTask, &enb1_params);
    startTask(tasks, networkTask, &enb2_params);
}

static void runNetwork(const NetworkConfiguration* config, AsyncTasks* tasks)
{
    if (config->locals.count or isUnix(config->protocol))
    {
        ZERO_MEMORY(&network_params);
        network_params.name = "network";
        network_params.config = *config;
        startTask(tasks, networkTask, &network_params);
    }
    else
        runPredefinedConfig(config, tasks);
}

int main(int argc, char* argv[])
{
    installSigaction(SIGTERM, printBacktrace);
    installSigaction(SIGSEGV, printBacktrace);

    Args args;
    args.argc = argc;
    args.argv = argv;
    // if (argsContains(&args, "-debug"))
    enable_debug_logs = true;
    debugArgs(&args);

    threadName("12345678901234567890");

    AsyncTasks tasks = createAsyncTasks();
    startTask(&tasks, ioTask, (TaskParams)NULL);
    const NetworkConfiguration config = createNetworkConfiguration(&args);
    runNetwork(&config, &tasks);
    join(&tasks);
}
