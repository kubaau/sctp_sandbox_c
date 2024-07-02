#include "SocketUnix.h"
#include <tools_c/StringEquals.h>
#include "Constants.h"
#include "SocketErrorChecks.h"

static void handleCommUpUnix(Socket* socket, const Path path)
{
    if (socket->peer_count >= peers_max_count)
    {
        WARN_LOG("Cannot %s, because there can't be any more peers", __func__);
        return;
    }

    INFO_LOG("New path %s", path);
    Peer* peer = socket->peers + socket->peer_count++;
    strcpy(peer->path, path);
}

static Peer* findPeerByPath(Socket* socket, const Path path)
{
    for (Count i = 0; i < socket->peer_count; ++i)
        if (equals(socket->peers[i].path, path))
            return socket->peers + i;
    return NULL;
}

static void handleGracefulShutdownUnix(Socket* socket, const Path path)
{
    INFO_LOG("Graceful shutdown on fd = %d, path %s", socket->fd, path);
    removePeer(socket, findPeerByPath(socket, path));
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void bindSocketUnix(FD fd)
{
    struct sockaddr_un saddr = {0};
    saddr.sun_family = AF_UNIX;
    checkBind(bind(fd, (const struct sockaddr*)&saddr, sizeof(Family)));

    socklen_t saddr_len = sizeof(saddr);
    checkGetsockname(getsockname(fd, (struct sockaddr*)&saddr, &saddr_len));
    saddr.sun_path[0] = '#';
    INFO_LOG("Bound socket: fd = %d, path = %s", fd, saddr.sun_path);
}

void connectSocketUnix(Socket* socket, const RemoteIPSockets* remotes)
{
    for (Count i = 0; i < remotes->count; ++i)
        handleCommUpUnix(socket, remotes->sockets[i].addr);
}

void handleMessageUnix(Socket* socket, FD fd)
{
    struct sockaddr_storage from_storage = {0};
    socklen_t from_len = sizeof(from_storage);

    const int result = checkedReceive(
        recvfrom(fd, socket->msg_buffer, msg_buffer_size, ignore_flags, (struct sockaddr*)&from_storage, &from_len));
    socket->msg_buffer[result] = 0;

    char* sun_path = ((struct sockaddr_un*)&from_storage)->sun_path;
    sun_path[0] = '#';

    INFO_LOG("Received message: %s (size = %Zu) from %s", socket->msg_buffer, result, sun_path);

    char* path = sun_path + 1;

    if (equals(socket->msg_buffer, quit_msg))
    {
        handleGracefulShutdownUnix(socket, path);
        return;
    }

    if (not findPeerByPath(socket, path))
    {
        handleCommUpUnix(socket, path);
        return;
    }
}

void sendSocketUnix(Socket* socket, ChatMessage msg)
{
    const Size msg_size = strlen(msg);

    for (Count i = 0; i < socket->peer_count; ++i)
    {
        const Peer* peer = socket->peers + i;
        if (not empty(peer->path))
        {
            struct sockaddr_un saddr = {0};
            saddr.sun_family = socket->family;
            strcpy(saddr.sun_path + 1, peer->path);

            saddr.sun_path[0] = '#';
            INFO_LOG(
                "Sending message: %s (size = %Zu) on fd = %d, path = %s", msg, msg_size, socket->fd, saddr.sun_path);
            saddr.sun_path[0] = 0;
            checkSend(sendto(socket->fd,
                             msg,
                             msg_size,
                             ignore_flags,
                             (const struct sockaddr*)&saddr,
                             sizeof(Family) + strlen(peer->path) + 1));
        }
    }
}
