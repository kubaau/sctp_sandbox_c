#include "SocketUdp.h"
#include <tools_c/StringEquals.h>
#include "Constants.h"
#include "SocketErrorChecks.h"

static const Count keep_alive_period_s = 1;
static const Count keep_alive_probes = 3;
static const char* keep_alive_msg = "ka";
static const char* keep_alive_ack_msg = "kack";

static void sendSocketUdpToRemote(Socket* socket, ChatMessage msg, const RemoteIPSocket* remote)
{
    const Size msg_size = strlen(msg);

    INFO_LOG("Sending message: %s (size = %Zu) on fd = %d, remote = %s:%d",
             msg,
             msg_size,
             socket->fd,
             remote->addr,
             remote->port);

    const struct sockaddr_storage saddr = ipSocketToSockaddrStorage(remote);
    checkSend(sendto(socket->fd, msg, msg_size, ignore_flags, (const struct sockaddr*)&saddr, sizeof(saddr)));
}

static void ping(ScheduledTaskParams params)
{
    RemoteParams* p = (RemoteParams*)params;
    sendSocketUdpToRemote(p->socket, keep_alive_msg, &p->remote);
}

static ScheduledTask* schedulePing(Socket* socket, const RemoteIPSocket* remote)
{
    Duration delay = {0};
    delay.tv_sec = keep_alive_period_s;
    ScheduledTask task = createScheduledTask(ping, &delay, (Repetitions)keep_alive_probes);
    RemoteParams* params = malloc(sizeof(RemoteParams));
    params->socket = socket;
    params->remote = *remote;
    return scheduleTask(socket->tasks, &task, params);
}

static Peer* findPeerByRemote(Socket* socket, const RemoteIPSocket* remote)
{
    for (Count i = 0; i < socket->peer_count; ++i)
        if (equals(socket->peers[i].remote.addr, remote->addr) and socket->peers[i].remote.port == remote->port)
            return socket->peers + i;
    return NULL;
}

static void handleCommLostUdp(Socket* socket, const RemoteIPSocket* remote)
{
    INFO_LOG("No connection on fd = %d towards %s:%d", socket->fd, remote->addr, remote->port);
    scheduleReestablishment(socket, remote);
    removePeer(socket, findPeerByRemote(socket, remote));
}

static void commLost(ScheduledTaskParams params)
{
    RemoteParams* p = (RemoteParams*)params;
    handleCommLostUdp(p->socket, &p->remote);
}

static ScheduledTask* scheduleCommLost(Socket* socket, const RemoteIPSocket* remote)
{
    Duration delay = {0};
    delay.tv_sec = keep_alive_period_s * (keep_alive_probes + 1);
    ScheduledTask task = createScheduledTask(commLost, &delay, (Repetitions)1);
    RemoteParams* params = malloc(sizeof(RemoteParams));
    params->socket = socket;
    params->remote = *remote;
    return scheduleTask(socket->tasks, &task, params);
}

static void handleCommUpUdp(Socket* socket, const RemoteIPSocket* remote)
{
    if (socket->peer_count >= peers_max_count)
    {
        WARN_LOG("Cannot %s, because there can't be any more peers", __func__);
        return;
    }

    INFO_LOG("New peer %s:%d", remote->addr, remote->port);
    Peer* peer = socket->peers + socket->peer_count++;
    peer->remote = *remote;
    peer->ping_task = schedulePing(socket, remote);
    peer->comm_lost_task = scheduleCommLost(socket, remote);
}

static void handleGracefulShutdownUdp(Socket* socket, const RemoteIPSocket* remote)
{
    INFO_LOG("Graceful shutdown on fd = %d, peer %s:%d", socket->fd, remote->addr, remote->port);
    removePeer(socket, findPeerByRemote(socket, remote));
}

static void resetTasks(Peer* peer)
{
    resetTask(peer->ping_task);
    resetTask(peer->comm_lost_task);
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void connectSocketUdp(Socket* socket, const RemoteIPSockets* remotes)
{
    for (Count i = 0; i < remotes->count; ++i)
        handleCommUpUdp(socket, remotes->sockets + i);
}

void handleMessageUdp(Socket* socket, FD fd)
{
    struct sockaddr_storage from_storage;
    socklen_t from_len = sizeof(from_storage);
    const int receive_result =
        recvfrom(fd, socket->msg_buffer, msg_buffer_size, ignore_flags, (struct sockaddr*)&from_storage, &from_len);
    checkReceive(receive_result);
    socket->msg_buffer[receive_result] = 0;

    const RemoteIPSocket remote = sockaddrStoragetoIPSocket(&from_storage);

    INFO_LOG(
        "Received message: %s (size = %Zu) from %s:%d", socket->msg_buffer, receive_result, remote.addr, remote.port);

    if (equals(socket->msg_buffer, quit_msg))
    {
        handleGracefulShutdownUdp(socket, &remote);
        return;
    }

    if (equals(socket->msg_buffer, keep_alive_msg))
        sendSocketUdpToRemote(socket, keep_alive_ack_msg, &remote);

    Peer* peer = findPeerByRemote(socket, &remote);
    if (peer)
        resetTasks(peer);
    else
    {
        handleCommUpUdp(socket, &remote);
        return;
    }
}

void sendSocketUdp(Socket* socket, ChatMessage msg)
{
    for (Count i = 0; i < socket->peer_count; ++i)
    {
        const Peer* peer = socket->peers + i;
        if (peer->remote.port)
            sendSocketUdpToRemote(socket, msg, &peer->remote);
    }
}
