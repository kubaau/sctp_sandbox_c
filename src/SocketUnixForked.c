#include "SocketUnixForked.h"
#include <tools_c/RandomContainers.h>
#include <tools_c/Sigaction.h>
#include <tools_c/StringEquals.h>
#include <tools_c/Swap.h>
#include <sys/mman.h>
#include <sys/prctl.h>
#include <sys/shm.h>
#include <sys/stat.h>
#include "Constants.h"
#include "SocketErrorChecks.h"

static const char* parent_mq_name = "/parent";
static const char* child_mq_name = "/child";

static void createMessageQueues(Socket* socket)
{
    const int mq_flags = O_RDWR | O_CREAT | O_EXCL | O_NONBLOCK | O_CLOEXEC;
    const int mode = S_IRUSR | S_IWUSR;
    void* const default_attr = NULL;
    mq_unlink(parent_mq_name);
    mq_unlink(child_mq_name);
    socket->forked.mq = checkedMqOpen(mq_open(parent_mq_name, mq_flags, mode, default_attr));
    socket->forked.peer_mq = checkedMqOpen(mq_open(child_mq_name, mq_flags, mode, default_attr));
}

static const char* shm_name = "/shm";
static const int permissions = 0666;
static void* const ignore_mapping_hint = NULL;
static const size_t shm_size = sizeof(SharedMemory);

static void createPosixSharedMemory(Socket* socket)
{
    socket->forked.shm = checkedShmopen(shm_open(shm_name, O_RDWR | O_CREAT | O_EXCL, permissions));
    checkTruncate(ftruncate(socket->forked.shm, shm_size));
    const int offset = 0;
    socket->forked.shm_mmap = (SharedMemory*)checkedMmap(
        mmap(ignore_mapping_hint, shm_size, PROT_READ | PROT_WRITE, MAP_SHARED, socket->forked.shm, offset));
}

static void createSysVSharedMemory(Socket* socket)
{
    socket->forked.shm_id = checkedShmget(shmget(IPC_PRIVATE, shm_size, permissions | IPC_CREAT));
    const int shmat_flags = 0;
    socket->forked.shm_shmat =
        (SharedMemory*)checkedShmat(shmat(socket->forked.shm_id, ignore_mapping_hint, shmat_flags));
}

static const char* sem_name = "/sem";
static const int initial_sem_value = 0;

static void createNamedSemaphore(Socket* socket)
{
    socket->forked.sem_named =
        (Semaphore*)checkedSemopen(sem_open(sem_name, O_CREAT | O_EXCL, permissions, initial_sem_value));
}

static void createUnnamedSemaphore(Socket* socket)
{
    socket->forked.sem_unnamed = &socket->forked.shm_mmap->sem;
    const bool shared_between_processes = true;
    checkSeminit(sem_init(socket->forked.sem_unnamed, shared_between_processes, initial_sem_value));
}

typedef struct
{
    FD first, second;
} FdPair;

static FdPair createSocketPair(Socket* socket)
{
    FdPair fds;
    checkSocketpair(socketpair(socket->family,
                               typeFromNetworkProtocol(socket->protocol),
                               protocolFromNetworkProtocol(socket->protocol),
                               (int*)&fds));
    return fds;
}

static const int pipe_flags = O_CLOEXEC | O_NONBLOCK;

static FdPair createPipe(void)
{
    FdPair fds;
    checkPipe(pipe2((int*)&fds, pipe_flags));
    return fds;
}

static const char* pipe_name = "/tmp/named_pipe";

static FdPair createNamedPipe(void)
{
    checkRemove(remove(pipe_name));
    checkMkfifo(mkfifo(pipe_name, permissions));
    FdPair fds = {checkedOpen(open(pipe_name, O_RDONLY | pipe_flags)),
                  checkedOpen(open(pipe_name, O_WRONLY | pipe_flags))};
    return fds;
}

static SigActionSignature(childDied)
{
    DEBUG_LOG("Oh no, my child is dead ;(");
    cleanUpGlobals();
    exit(EXIT_SUCCESS);
}

static SigActionSignature(parentDied)
{
    DEBUG_LOG("Oh no, my parent is dead ;(");
    cleanUpGlobals();
    exit(EXIT_SUCCESS);
}

static void killParentOnChildDeath(void)
{
    installSigaction(SIGCHLD, childDied);
}

static void forkParent(Socket* socket, FdPair socket_pair_fds, FdPair pipe_fds, FdPair named_pipe_fds)
{
    threadName("parent");
    killParentOnChildDeath();
    // pipefd[1] refers to the write end of the pipe.  pipefd[0] refers to the read end of the pipe.
    socket->fd = socket_pair_fds.second;
    socket->forked.pipe = pipe_fds.second;
    socket->forked.named_pipe = named_pipe_fds.second;
    closeFd(&socket_pair_fds.first);
    closeFd(&pipe_fds.first);
    closeFd(&named_pipe_fds.first);
}

static void checkParentAlive(Socket* socket)
{
    const int current_ppid = getppid();
    if (current_ppid != socket->forked.parent)
    {
        DEBUG_LOG(
            "Parent died during child creation :( parent = %d current_ppid = %d", socket->forked.parent, current_ppid);
        cleanUpGlobals();
        exit(EXIT_SUCCESS);
    }
}

static void killChildOnParentDeath(Socket* socket)
{
    installSigaction(SIGTERM, parentDied);
    checkPrctl(prctl(PR_SET_PDEATHSIG, SIGTERM));
    checkParentAlive(socket);
}

static void forkChild(Socket* socket, FdPair socket_pair_fds, FdPair pipe_fds, FdPair named_pipe_fds)
{
    threadName("child");
    killChildOnParentDeath(socket);
    socket->fd = socket_pair_fds.first;
    socket->forked.pipe = pipe_fds.first;
    socket->forked.named_pipe = named_pipe_fds.first;
    closeFd(&socket_pair_fds.second);
    closeFd(&pipe_fds.second);
    closeFd(&named_pipe_fds.second);
    SWAP(MessageQueue, socket->forked.mq, socket->forked.peer_mq);
}

static void forkSocket(Socket* socket, FdPair socket_pair_fds, FdPair pipe_fds, FdPair named_pipe_fds)
{
    const int pid = getpid();
    DEBUG_LOG("pid = %d", pid);

    const int fork_result = fork();
    if (fork_result)
    {
        checkedFork(fork_result);
        DEBUG_LOG("fork_result = %d", fork_result);
        forkParent(socket, socket_pair_fds, pipe_fds, named_pipe_fds);
    }
    else
    {
        socket->forked.parent = pid;
        forkChild(socket, socket_pair_fds, pipe_fds, named_pipe_fds);
    }
}

static bool isSemaphoreReady(Semaphore* sem)
{
    return checkedSemtrywait(sem_trywait(sem)) == 0;
}

static void printSemaphoreValues(Socket* socket)
{
    int value;
    checkSemgetvalue(sem_getvalue(socket->forked.sem_named, &value));
    DEBUG_LOG("sem_named = %d", value);
    checkSemgetvalue(sem_getvalue(socket->forked.sem_unnamed, &value));
    DEBUG_LOG("sem_unnamed = %d", value);
}

static void readPosixSharedMemory(Socket* socket)
{
    if (isSemaphoreReady(socket->forked.sem_named))
    {
        INFO_LOG("Shared mmap integer is: %d", socket->forked.shm_mmap->i);
        printSemaphoreValues(socket);
    }
}

static void readSysVSharedMemory(Socket* socket)
{
    if (isSemaphoreReady(socket->forked.sem_unnamed))
    {
        INFO_LOG("Shared shmat integer is: %d", socket->forked.shm_shmat->i);
        printSemaphoreValues(socket);
    }
}

static void receiveFromPipe(Socket* socket)
{
    const int result = checkedRead(read(socket->forked.pipe, socket->msg_buffer, msg_buffer_size));
    if (result > 0)
    {
        socket->msg_buffer[result] = 0;
        INFO_LOG("Received message: %s (size = %Zu) on pipe = %d", socket->msg_buffer, result, socket->forked.pipe);
    }
}

static void receiveFromNamedPipe(Socket* socket)
{
    const int result = checkedRead(read(socket->forked.named_pipe, socket->msg_buffer, msg_buffer_size));
    if (result > 0)
    {
        socket->msg_buffer[result] = 0;
        INFO_LOG("Received message: %s (size = %Zu) on named_pipe = %d",
                 socket->msg_buffer,
                 result,
                 socket->forked.named_pipe);
    }
}

static void receiveFromMessageQueue(Socket* socket)
{
    Duration sleep = {0};
    sleep.tv_nsec = 10 * 1000 * 1000;
    nanosleep(&sleep, NULL);

    unsigned prio;
    const int result = checkedMqReceive(mq_receive(socket->forked.mq, socket->msg_buffer, msg_buffer_size, &prio));
    if (result > 0)
    {
        socket->msg_buffer[result] = 0;
        INFO_LOG("Received message: %s (size = %Zu) with prio = %d", socket->msg_buffer, result, prio);
    }
}

static void sendOnSocketPair(Socket* socket, ChatMessage msg)
{
    const Size msg_size = strlen(msg);
    INFO_LOG("Sending message: %s (size = %Zu) on fd = %d", msg, msg_size, socket->fd);
    checkSend(send(socket->fd, msg, msg_size, ignore_flags));
}

static void sendOnPipe(Socket* socket, ChatMessage msg)
{
    const Size msg_size = strlen(msg);
    INFO_LOG("Sending message: %s (size = %Zu) on pipe = %d", msg, msg_size, socket->forked.pipe);
    checkWrite(write(socket->forked.pipe, msg, msg_size));
}

static void sendOnNamedPipe(Socket* socket, ChatMessage msg)
{
    const Size msg_size = strlen(msg);
    INFO_LOG("Sending message: %s (size = %Zu) on named_pipe = %d", msg, msg_size, socket->forked.named_pipe);
    checkWrite(write(socket->forked.named_pipe, msg, msg_size));
}

static void sendOnMessageQueue(Socket* socket, ChatMessage msg)
{
    const Size msg_size = strlen(msg);
    unsigned* const prios = shuffledIndexes(5);
    repeat(5)
    {
        INFO_LOG("Sending message: %s (size = %Zu) with prio = %d", msg, msg_size, prios[i]);
        checkMqSend(mq_send(socket->forked.peer_mq, msg, msg_size, prios[i]));
    }
    free(prios);
}

static void setPosixSharedMemory(Socket* socket, int v)
{
    INFO_LOG("Setting mmap shared integer to: %d", v);
    socket->forked.shm_mmap->i = v;
}

static void postNamedSemaphore(Socket* socket)
{
    checkSempost(sem_post(socket->forked.sem_named));
    printSemaphoreValues(socket);
}

static void setSysVSharedMemory(Socket* socket, int v)
{
    INFO_LOG("Setting shmat shared integer to: %d", v);
    socket->forked.shm_shmat->i = v;
}

static void postUnnamedSemaphore(Socket* socket)
{
    checkSempost(sem_post(socket->forked.sem_unnamed));
    printSemaphoreValues(socket);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void initForkedSocket(Socket* socket)
{
    cleanUpGlobals();
    createMessageQueues(socket);
    createPosixSharedMemory(socket);
    createSysVSharedMemory(socket);
    createNamedSemaphore(socket);
    createUnnamedSemaphore(socket);
    forkSocket(socket, createSocketPair(socket), createPipe(), createNamedPipe());
    configureSocket(socket, socket->fd);
    DEBUG_LOG("Configured fd = %d", socket->fd);
}

void handleMessageUnixForked(Socket* socket, FD fd)
{
    const int result = checkedReceive(recv(fd, socket->msg_buffer, msg_buffer_size, ignore_flags));
    socket->msg_buffer[result] = 0;

    INFO_LOG("Received message: %s (size = %Zu)", socket->msg_buffer, result);

    if (equals(socket->msg_buffer, quit_msg))
    {
        cleanUpGlobals();
        exit(EXIT_SUCCESS);
    }
}

void receiveUnixForked(Socket* socket)
{
    readPosixSharedMemory(socket);
    readSysVSharedMemory(socket);

    if (isChild(socket))
    {
        receiveFromPipe(socket);
        receiveFromNamedPipe(socket);
    }

    receiveFromMessageQueue(socket);
}

void sendSocketUnixForked(Socket* socket, ChatMessage msg)
{
    sendOnSocketPair(socket, msg);
    sendOnPipe(socket, msg);
    sendOnNamedPipe(socket, msg);
    sendOnMessageQueue(socket, msg);
    setPosixSharedMemory(socket, strlen(msg));
    postNamedSemaphore(socket);
    setSysVSharedMemory(socket, strlen(msg));
    postUnnamedSemaphore(socket);
}

bool isChild(Socket* socket)
{
    return socket->forked.parent;
}

void cleanUpMessageQueues(Socket* socket)
{
    mq_close(socket->forked.mq);
    mq_close(socket->forked.peer_mq);
}

void cleanUpSemaphores(Socket* socket)
{
    sem_close(socket->forked.sem_named);
    sem_close(socket->forked.sem_unnamed);
    sem_destroy(socket->forked.sem_unnamed);
}

void cleanUpSharedMemory(Socket* socket)
{
    munmap(socket->forked.shm_mmap, shm_size);
    shmdt(socket->forked.shm_shmat);
}

void cleanUpGlobals(void)
{
    mq_unlink(parent_mq_name);
    mq_unlink(child_mq_name);
    sem_unlink(sem_name);
    shm_unlink(shm_name);
}
