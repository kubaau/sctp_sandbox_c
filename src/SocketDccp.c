#include "SocketDccp.h"
#include <tools_c/StringEquals.h>
#include "Constants.h"
#include "SocketErrorChecks.h"
#include "SocketTcp.h"

void handleMessageDccp(Socket* socket, FD fd)
{
    if (fd == socket->fd)
    {
        handleCommUpTcp(socket, fd);
        return;
    }

    const int receive_result = recv(fd, socket->msg_buffer, msg_buffer_size, ignore_flags);
    checkReceive(receive_result);
    socket->msg_buffer[receive_result] = 0;

    if (not socket->msg_buffer[0])
    {
        handleCommLostTcp(socket, fd);
        return;
    }

    INFO_LOG("Received message: %s (size = %Zu)", socket->msg_buffer, receive_result);
    logPaddrsTcp(fd);

    if (equals(socket->msg_buffer, quit_msg))
    {
        handleGracefulShutdownTcp(socket, fd);
        return;
    }
}

void sendSocketDccp(Socket* socket, ChatMessage msg)
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

            if (failedSend(send(peer->fd, msg, msg_size, ignore_flags)))
                WARN_LOG("Could not send");
        }
    }
}
