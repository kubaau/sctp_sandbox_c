#include "SocketTcp.h"
#include "Constants.h"
#include "SocketErrorChecks.h"
#include "SocketIO.h"

static void logLaddrsTcp(FD fd)
{
    struct sockaddr_storage saddr_storage;
    socklen_t saddr_len = sizeof(struct sockaddr_storage);
    checkGetsockname(getsockname(fd, (struct sockaddr*)&saddr_storage, &saddr_len));
    const IPSocket local = sockaddrStoragetoIPSocket(&saddr_storage);
    DEBUG_LOG("%s:%d", local.addr, local.port);
}

static FD createConnectSocket(Socket* s)
{
    const FD fd = socket(s->family, typeFromNetworkProtocol(s->protocol), protocolFromNetworkProtocol(s->protocol));
    checkSocket(fd);
    DEBUG_LOG("Created socked: fd = %d, family = %s for a new connection", fd, familyToString(s->family));

    configureSocket(s, fd);
    DEBUG_LOG("Configured fd = %d", fd);

    LocalIPSockets locals;
    locals.sockets[0] = s->local;
    locals.count = 1;
    bindSocket(s, fd, &locals);

    return fd;
}

static Peer* findPeerByFd(Socket* socket, FD fd)
{
    for (Count i = 0; i < socket->peer_count; ++i)
        if (socket->peers[i].fd == fd)
            return socket->peers + i;
    return NULL;
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void bindSocketTcp(FD fd, const LocalIPSocket* local)
{
    struct sockaddr_storage saddr = ipSocketToSockaddrStorage(local);
    INFO_LOG("Binding socket: fd = %d, addr = %s:%u", fd, local->addr, local->port);
    checkBind(bind(fd, (const struct sockaddr*)&saddr, sizeof(saddr)));
    DEBUG_LOG("Bound addresses:");
    logLaddrsTcp(fd);
}

void connectSocketTcp(Socket* socket, const RemoteIPSockets* remotes)
{
    for (Count i = 0; i < remotes->count; ++i)
    {
        if (socket->peer_count >= peers_max_count)
        {
            WARN_LOG("Cannot %s, because there can't be any more peers", __func__);
            return;
        }

        const FD fd = createConnectSocket(socket);
        connectSingleSocket(fd, remotes->sockets + i);
        Peer* new_peer = socket->peers + socket->peer_count++;
        new_peer->fd = fd;
        new_peer->remote = remotes->sockets[i];
        new_peer->should_reestablish = true;
    }
}

void handleMessageTcp(Socket* socket, FD fd)
{
    if (fd == socket->fd)
    {
        handleCommUpTcp(socket, fd);
        return;
    }

    const int receive_result = recv(fd, socket->msg_buffer, msg_buffer_size, ignore_flags);
    if (failedReceive(receive_result))
    {
        handleCommLostTcp(socket, fd);
        return;
    }
    socket->msg_buffer[receive_result] = 0;

    if (not socket->msg_buffer[0])
    {
        handleGracefulShutdownTcp(socket, fd);
        return;
    }

    INFO_LOG("Received message: %s (size = %Zu)", socket->msg_buffer, receive_result);
    logPaddrsTcp(fd);
}

void handleCommUpTcp(Socket* socket, FD fd)
{
    if (socket->peer_count >= peers_max_count)
    {
        WARN_LOG("Cannot %s, because there can't be any more peers", __func__);
        return;
    }

    DEBUG_LOG("Accepting: fd = %d", fd);
    struct sockaddr_storage saddr_storage;
    socklen_t saddr_len = sizeof(struct sockaddr_storage);
    const int accept_result = accept(fd, (struct sockaddr*)&saddr_storage, &saddr_len);
    checkAccept(accept_result);
    const IPSocket remote = sockaddrStoragetoIPSocket(&saddr_storage);
    DEBUG_LOG("Accepted remote fd = %d, peer address = %s:%d", accept_result, remote.addr, remote.port);

    Peer* peer = socket->peers + socket->peer_count++;
    peer->fd = accept_result;
    peer->remote = remote;
    peer->should_reestablish = false;
}

void handleCommLostTcp(Socket* socket, FD fd)
{
    Peer* peer = findPeerByFd(socket, fd);
    INFO_LOG("No connection on fd = %d towards %s:%d", fd, peer->remote.addr, peer->remote.port);
    if (peer->should_reestablish)
        scheduleReestablishment(socket, &peer->remote);
    removePeer(socket, peer);
}

void handleGracefulShutdownTcp(Socket* socket, FD fd)
{
    Peer* peer = findPeerByFd(socket, fd);
    INFO_LOG("Graceful shutdown on fd = %d, peer %s:%d", fd, peer->remote.addr, peer->remote.port);
    removePeer(socket, peer);
}

void sendSocketTcp(Socket* socket, ChatMessage msg)
{
    const Size msg_size = strlen(msg);

    for (Count i = 0; i < socket->peer_count; ++i)
    {
        const Peer* peer = socket->peers + i;
        if (peer->fd)
        {
            INFO_LOG("Sending message: %s (size = %Zu) on fd = %d, remote = %s:%d",
                     msg,
                     msg_size,
                     peer->fd,
                     peer->remote.addr,
                     peer->remote.port);

            checkSend(send(peer->fd, msg, msg_size, ignore_flags));
        }
    }
}

void logPaddrsTcp(FD fd)
{
    struct sockaddr_storage saddr_storage;
    socklen_t saddr_len = sizeof(struct sockaddr_storage);
    checkGetsockname(getpeername(fd, (struct sockaddr*)&saddr_storage, &saddr_len));
    const IPSocket local = sockaddrStoragetoIPSocket(&saddr_storage);
    DEBUG_LOG("%s:%d", local.addr, local.port);
}
