#pragma once

#include <tools_c/TaskScheduler.h>
#include <mqueue.h>
#include <semaphore.h>
#include "FDSet.h"
#include "NetworkProtocol.h"

enum
{
    msg_buffer_size = 64 * 1024,
    peers_max_count = 10,
    path_max_length = 10
};

typedef char MsgBuffer[msg_buffer_size];

typedef bool ShouldReestablish;
typedef char Path[path_max_length];
typedef struct
{
    AssocId assoc_id;
    FD fd;
    RemoteIPSocket remote;
    ShouldReestablish should_reestablish;
    ScheduledTask* ping_task;
    ScheduledTask* comm_lost_task;
    Path path;
} Peer;

typedef sem_t Semaphore;
typedef struct
{
    int i;
    Semaphore sem;
} SharedMemory;

typedef pid_t ProcessId;
typedef mqd_t MessageQueue;
typedef int SharedMemoryId;
typedef struct
{
    ProcessId parent;
    FD pipe;
    FD named_pipe;
    MessageQueue mq;
    MessageQueue peer_mq;
    FD shm;
    SharedMemoryId shm_id;
    SharedMemory* shm_mmap;
    SharedMemory* shm_shmat;
    Semaphore* sem_named;
    Semaphore* sem_unnamed;
} Forked;

typedef struct
{
    FD fd;
    Family family;

    NetworkProtocol protocol;
    IPSocket local;

    MsgBuffer msg_buffer;

    Peer peers[peers_max_count];
    Count peer_count;

    FDSet fd_set;

    Forked forked;

    ScheduledTasks* tasks;
} Socket;

typedef int BacklogCount;
Socket createSocket(const LocalIPSockets*, NetworkProtocol, ScheduledTasks*, Family);
void configureSocket(Socket*, FD);
void bindSocket(Socket*, FD, const LocalIPSockets*);
void listenSocket(Socket*, BacklogCount);
void connectSocket(Socket*, const RemoteIPSockets*);
void connectMultihomedSocket(Socket*, const RemoteIPSockets*);
void connectSingleSocket(FD, const RemoteIPSocket*);
void receiveSocket(Socket*);
void sendSocket(Socket*, ChatMessage);
void cleanUpSocket(Socket*);
void closeFd(FD*);

typedef struct
{
    Socket* socket;
    IPSocket remote;
} RemoteParams;

void scheduleReestablishment(Socket*, const IPSocket*);
void removePeer(Socket*, Peer*);
