#include "Socket.h"
#include <signal.h>
#include "NetworkConfiguration.h"
#include "SctpGetAddrs.h"
#include "SocketConfiguration.h"
#include "SocketDccp.h"
#include "SocketErrorChecks.h"
#include "SocketIO.h"
#include "SocketSctp.h"
#include "SocketTcp.h"
#include "SocketUdp.h"
#include "SocketUnix.h"
#include "SocketUnixForked.h"

static Family chooseSocketFamily(const LocalIPSockets* locals)
{
    for (Count i = 0; i < locals->count; ++i)
        if (locals->sockets[i].family == AF_INET6)
            return AF_INET6;
    return AF_INET;
}

static bool shouldListen(SocketType type)
{
    return type != SOCK_DGRAM;
}

static void reestablish(ScheduledTaskParams params)
{
    RemoteParams* p = (RemoteParams*)params;
    DEBUG_LOG("Reestablishing connection to %s:%d", p->remote.addr, p->remote.port);
    RemoteIPSockets remotes;
    remotes.sockets[0] = p->remote;
    remotes.count = 1;
    connectSocket(p->socket, &remotes);
}

static void selectFds(Socket* socket)
{
    FD_ZERO(&socket->fd_set.fds);
    socket->fd_set.fd_max = 0;

    setFd(&socket->fd_set, socket->fd);
    if (not isUdp(socket->protocol))
        for (Count i = 0; i < socket->peer_count; ++i)
            if (socket->peers[i].fd)
                setFd(&socket->fd_set, socket->peers[i].fd);

    selectFdSet(&socket->fd_set, (Milliseconds)10);
}

static void disableTasks(Peer* peer)
{
    DEBUG_LOG("Disabling tasks for %s:%d", peer->remote.addr, peer->remote.port);

    if (peer->ping_task)
        disableTask(peer->ping_task);

    if (peer->comm_lost_task)
        disableTask(peer->comm_lost_task);

    peer->ping_task = NULL;
    peer->comm_lost_task = NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

Socket createSocket(const LocalIPSockets* locals, NetworkProtocol protocol, ScheduledTasks* tasks, Family family)
{
    Socket s = {0};
    s.family = family == AF_UNSPEC ? chooseSocketFamily(locals) : family;
    s.protocol = protocol;
    s.local = locals->sockets[0];
    s.local.port = any_port;
    s.tasks = tasks;

    if (protocol != UnixForked)
    {
        s.fd = socket(s.family, typeFromNetworkProtocol(protocol), protocolFromNetworkProtocol(protocol));
        checkSocket(s.fd);
        DEBUG_LOG("Created socket: fd = %d, family = %s", s.fd, familyToString(s.family));

        configureSocket(&s, s.fd);
        DEBUG_LOG("Configured fd = %d", s.fd);

        bindSocket(&s, s.fd, locals);
    }
    else
        initForkedSocket(&s);

    return s;
}

void configureSocket(Socket* socket, FD fd)
{
    configureReuseAddr(fd);
    configureNonBlockingMode(fd);
    if (socket->family != AF_UNIX)
    {
        configureDscp(fd, 63, socket->family);
        configureTTL(fd, 255);
    }

    if (socket->protocol == SCTP)
        configureSocketSctp(socket, fd);
    else if (socket->protocol == TCP)
        configureKeepAlive(fd, (KeepAliveTimeInS)1, (KeepAliveIntervalInS)1, (KeepAliveProbes)3);
}

void bindSocket(Socket* socket, FD fd, const LocalIPSockets* locals)
{
    switch (socket->protocol)
    {
        case TCP:
        case UDP:
        case UDP_Lite:
        case DCCP: bindSocketTcp(fd, locals->sockets); return;
        case Unix: bindSocketUnix(fd); return;
        case UnixForked: return;
        default: bindSocketSctp(socket, locals);
    }
}

void listenSocket(Socket* socket, BacklogCount backlog_count)
{
    if (shouldListen(typeFromNetworkProtocol(socket->protocol)))
    {
        checkListen(listen(socket->fd, backlog_count));
        DEBUG_LOG("Listening: fd = %d, backlog_count = %d", socket->fd, backlog_count);
    }
}

void connectSocket(Socket* socket, const RemoteIPSockets* remotes)
{
    switch (socket->protocol)
    {
        case TCP:
        case DCCP: connectSocketTcp(socket, remotes); return;
        case UDP:
        case UDP_Lite: connectSocketUdp(socket, remotes); return;
        case Unix: connectSocketUnix(socket, remotes); return;
        case UnixForked: return;
        default: connectSocketSctp(socket, remotes);
    }
}

void connectSingleSocket(FD fd, const RemoteIPSocket* remote)
{
    const struct sockaddr_storage saddr = ipSocketToSockaddrStorage(remote);
    INFO_LOG("Connecting to %s:%u", remote->addr, remote->port);
    checkConnect(connect(fd, (const struct sockaddr*)&saddr, sizeof(saddr)));
}

void receiveSocket(Socket* socket)
{
    selectFds(socket);

    for (Count i = 0; i < socket->fd_set.selected_fds_count; ++i)
    {
        const FD fd = socket->fd_set.selected_fds[i];
        DEBUG_LOG("Receiving message on fd = %d", fd);

        switch (socket->protocol)
        {
            case TCP: handleMessageTcp(socket, fd); break;
            case UDP:
            case UDP_Lite: handleMessageUdp(socket, fd); break;
            case DCCP: handleMessageDccp(socket, fd); break;
            case Unix: handleMessageUnix(socket, fd); break;
            case UnixForked: handleMessageUnixForked(socket, fd); break;
            default: handleMessageSctp(socket, fd);
        }
    }

    if (socket->protocol == UnixForked)
        receiveUnixForked(socket);
}

void sendSocket(Socket* socket, ChatMessage msg)
{
    switch (socket->protocol)
    {
        case TCP: sendSocketTcp(socket, msg); return;
        case UDP:
        case UDP_Lite: sendSocketUdp(socket, msg); return;
        case DCCP: sendSocketDccp(socket, msg); return;
        case Unix: sendSocketUnix(socket, msg); return;
        case UnixForked: sendSocketUnixForked(socket, msg); return;
        default: sendSocketSctp(socket, msg);
    }
}

void cleanUpSocket(Socket* socket)
{
    if (socket->protocol == DCCP or isUdp(socket->protocol) or socket->protocol == Unix)
        sendSocket(socket, quit_msg);

    for (Count i = 0; i < socket->peer_count; ++i)
        if (socket->peers[i].fd)
            closeFd(&socket->peers[i].fd);
    closeFd(&socket->fd);

    closeFd(&socket->forked.pipe);
    closeFd(&socket->forked.named_pipe);
    closeFd(&socket->forked.shm);

    cleanUpMessageQueues(socket);
    cleanUpSemaphores(socket);
    cleanUpSharedMemory(socket);

    if (isChild(socket))
    {
        INFO_LOG("Child is dying - notifying parent %d", socket->forked.parent);
        kill(socket->forked.parent, SIGCHLD);
    }

    cleanUpGlobals();
}

void closeFd(FD* fd)
{
    if (*fd)
    {
        DEBUG_LOG("Closing fd = %d", *fd);
        close(*fd);
        *fd = 0;
    }
}

void scheduleReestablishment(Socket* socket, const IPSocket* remote)
{
    Duration delay = {0};
    delay.tv_sec = 5;
    ScheduledTask task = createScheduledTask(reestablish, &delay, (Repetitions)1);
    RemoteParams* params = malloc(sizeof(RemoteParams));
    params->socket = socket;
    params->remote = *remote;
    DEBUG_LOG("Scheduling delayed reestablishment to remote %s:%d", remote->addr, remote->port);
    scheduleTask(socket->tasks, &task, params);
}

void removePeer(Socket* socket, Peer* peer)
{
    if (isUdp(socket->protocol))
        disableTasks(peer);
    closeFd(&peer->fd);
    peer->assoc_id = 0;
    peer->remote.port = 0;
    peer->path[0] = 0;
}
