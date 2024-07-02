#include "SocketSctp.h"
#include "SctpGetAddrs.h"
#include "SocketConfiguration.h"
#include "SocketErrorChecks.h"
#include "SocketIO.h"

static void logLaddrsSctp(FD fd)
{
    logLaddrs(fd, ignore_assoc);
}

static bool isNotification(int msg_flags)
{
    return msg_flags & MSG_NOTIFICATION;
}

static const char* sctpNotificationToString(const union sctp_notification* sn)
{
    switch (sn->sn_header.sn_type)
    {
        case SCTP_ASSOC_CHANGE:
        {
            switch (sn->sn_assoc_change.sac_state)
            {
                case SCTP_COMM_UP: return "SCTP_ASSOC_CHANGE - SCTP_COMM_UP";
                case SCTP_COMM_LOST: return "SCTP_ASSOC_CHANGE - SCTP_COMM_LOST";
                case SCTP_RESTART: return "SCTP_ASSOC_CHANGE - SCTP_RESTART";
                case SCTP_SHUTDOWN_COMP: return "SCTP_ASSOC_CHANGE - SCTP_SHUTDOWN_COMP";
                case SCTP_CANT_STR_ASSOC: return "SCTP_ASSOC_CHANGE - SCTP_CANT_STR_ASSOC";
                default: return "SCTP_ASSOC_CHANGE - UNKNOWN";
            }
        }
        case SCTP_PEER_ADDR_CHANGE:
        {
            switch (sn->sn_paddr_change.spc_state)
            {
                case SCTP_ADDR_AVAILABLE: return "SCTP_ADDR_AVAILABLE"; break;
                case SCTP_ADDR_UNREACHABLE: return "SCTP_ADDR_UNREACHABLE"; break;
                case SCTP_ADDR_REMOVED: return "SCTP_ADDR_REMOVED"; break;
                case SCTP_ADDR_ADDED: return "SCTP_ADDR_ADDED"; break;
                case SCTP_ADDR_MADE_PRIM: return "SCTP_ADDR_MADE_PRIM"; break;
                case SCTP_ADDR_CONFIRMED: return "SCTP_ADDR_CONFIRMED"; break;
                default: return "UNKNOWN";
            }
        }
        case SCTP_REMOTE_ERROR:
            return "SCTP_REMOTE_ERROR";
            // case SCTP_SEND_FAILED_EVENT: return "SCTP_SEND_FAILED_EVENT";
        case SCTP_SHUTDOWN_EVENT: return "SCTP_SHUTDOWN_EVENT";
        case SCTP_ADAPTATION_INDICATION: return "SCTP_ADAPTATION_INDICATION";
        case SCTP_PARTIAL_DELIVERY_EVENT:
            return "SCTP_PARTIAL_DELIVERY_EVENT";
            // case SCTP_AUTHENTICATION_EVENT: return "SCTP_AUTHENTICATION_EVENT";
        case SCTP_SENDER_DRY_EVENT: return "SCTP_SENDER_DRY_EVENT";
        default: return "UNKNOWN SCTP NOTIFICATION";
    }
}

static bool isAssocChange(const union sctp_notification* sn)
{
    return sn->sn_header.sn_type == SCTP_ASSOC_CHANGE;
}

static void logPaddrsSctp(FD fd)
{
    logPaddrs(fd, ignore_assoc);
}

static void logPeerInfo(FD fd, AssocId assoc_id)
{
    struct sctp_status status = {0};
    socklen_t len = sizeof(status);
    checkOptinfo(sctp_opt_info(fd, ignore_assoc, SCTP_STATUS, &status, &len));
    const struct sctp_paddrinfo* paddr_info = &status.sstat_primary;

    struct sctp_paddrparams paddr_params = {0};
    len = sizeof(paddr_params);
    checkOptinfo(sctp_opt_info(fd, ignore_assoc, SCTP_PEER_ADDR_PARAMS, &paddr_params, &len));

    DEBUG_LOG("Peeled off fd = %d, assoc_id = %d, state = %s, cwnd = %d, rto = %d, mto = %d, fragmentation_point = %d, "
              "path_mtu = %d",
              fd,
              assoc_id,
              statusToString((enum sctp_spinfo_state)paddr_info->spinfo_state),
              paddr_info->spinfo_cwnd,
              paddr_info->spinfo_rto,
              paddr_info->spinfo_mtu,
              status.sstat_fragmentation_point,
              paddr_params.spp_pathmtu);
    DEBUG_LOG("Peer addresses:");
    logPaddrsSctp(fd);
}

static void peelOff(Socket* socket, AssocId assoc_id)
{
    if (socket->peer_count >= peers_max_count)
    {
        WARN_LOG("Cannot %s, because there can't be any more peers", __func__);
        return;
    }

    const int peeloff_result = sctp_peeloff(socket->fd, assoc_id);
    checkPeeloff(peeloff_result);
    logPeerInfo(peeloff_result, assoc_id);
    socket->peers[socket->peer_count].assoc_id = assoc_id;
    socket->peers[socket->peer_count].fd = peeloff_result;
    ++socket->peer_count;
}

static void handleCommUp(Socket* socket, AssocId assoc_id)
{
    peelOff(socket, assoc_id);
}

static void handleEstablishmentFailure(Socket* socket, const IPSocket* remote)
{
    scheduleReestablishment(socket, remote);
}

static Peer* findPeerByAssocId(Socket* socket, AssocId assoc_id)
{
    for (Count i = 0; i < socket->peer_count; ++i)
        if (socket->peers[i].assoc_id == assoc_id)
            return socket->peers + i;
    return NULL;
}

static void removeAssoc(Socket* socket, AssocId assoc_id)
{
    removePeer(socket, findPeerByAssocId(socket, assoc_id));
    DEBUG_LOG("Removed assoc_id = %d", assoc_id);
}

static void handleGracefulShutdown(Socket* socket, AssocId assoc_id)
{
    removeAssoc(socket, assoc_id);
}

static void handleCommLost(Socket* socket, AssocId assoc_id, const IPSocket* remote)
{
    removeAssoc(socket, assoc_id);
    scheduleReestablishment(socket, remote);
}

static void handleNotification(Socket* socket, const IPSocket* from, const union sctp_notification* notification)
{
    INFO_LOG("Received notification: %s from %s:%u", sctpNotificationToString(notification), from->addr, from->port);

    if (isAssocChange(notification))
    {
        const struct sctp_assoc_change* sac = &notification->sn_assoc_change;
        const short state = sac->sac_state;
        const AssocId assoc_id = sac->sac_assoc_id;

        switch (state)
        {
            case SCTP_COMM_UP: handleCommUp(socket, assoc_id); return;
            case SCTP_CANT_STR_ASSOC: handleEstablishmentFailure(socket, from); return;
            case SCTP_SHUTDOWN_COMP: handleGracefulShutdown(socket, assoc_id); return;
            case SCTP_COMM_LOST: handleCommLost(socket, assoc_id, from); return;
        }

        /*if (state == SCTP_COMM_UP)
        {
            DEBUG_LOG("COMM_UP");
        }
        else
        {
            DEBUG_LOG("Not a COMM_UP");
        }*/
    }
}

///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

void configureSocketSctp(Socket* socket, FD fd)
{
    configureNoDelay(fd);
    configureReceivingEvents(fd);
    configureInitParams(fd, (InitMaxAttempts)5, (InitMaxTimeoutInMs)5000);
    configureRto(fd, (RtoInitialInMs)1000, (RtoMinInMs)1000, (RtoMaxInMs)5000, all_associations);
    configureSack(fd, (SackFreq)2, (SackDelay)200, all_associations);
    configureMaxRetrans(fd, 5, all_associations);
    configureIPv4AddressMapping(fd);
    configurePeerAddressParams(
        fd, (HbInterval)10000, (PathMaxRetrans)3, (PathMtu)1500, socket->family, all_associations);
    configureMaxSeg(fd, 1500, all_associations);
}

void bindSocketSctp(Socket* socket, const LocalIPSockets* locals)
{
    Sockaddrs saddrs = ipSocketsToSockaddrs(locals);
    for (Count i = 0; i < locals->count; ++i)
    {
        const IPSocket* current_ip_socket = locals->sockets + i;
        INFO_LOG("Binding socket: fd = %d, addr = %s:%u", socket->fd, current_ip_socket->addr, current_ip_socket->port);
    }
    checkBind(sctp_bindx(socket->fd, saddrs, locals->count, SCTP_BINDX_ADD_ADDR));
    free(saddrs);
    DEBUG_LOG("Bound addresses:");
    logLaddrsSctp(socket->fd);
}

void connectSocketSctp(Socket* socket, const RemoteIPSockets* remotes)
{
    for (Count i = 0; i < remotes->count; ++i)
        connectSingleSocket(socket->fd, remotes->sockets + i);
}

void connectMultihomedSocket(Socket* socket, const RemoteIPSockets* remotes)
{
    if (socket->protocol != SCTP)
        WARN_LOG("Impossible on this socket type");

    if (not remotes->count)
        return;

    const Sockaddrs saddrs = ipSocketsToSockaddrs(remotes);
    for (Count i = 0; i < remotes->count; ++i)
    {
        const IPSocket* current_ip_socket = remotes->sockets + i;
        INFO_LOG("Connecting to %s:%u with multihoming", current_ip_socket->addr, current_ip_socket->port);
    }
    checkConnect(sctp_connectx(socket->fd, (struct sockaddr*)&saddrs, remotes->count, ignore_assoc));
    free(saddrs);
}

void handleMessageSctp(Socket* socket, FD fd)
{
    struct sockaddr_storage from_storage = {0};
    struct sockaddr* from = (struct sockaddr*)&from_storage;
    socklen_t from_len = sizeof(from_storage);
    struct sctp_sndrcvinfo sndrcvinfo = {0};
    int flags = 0;

    const int receive_result =
        sctp_recvmsg(fd, socket->msg_buffer, msg_buffer_size, from, &from_len, &sndrcvinfo, &flags);
    checkReceive(receive_result);
    socket->msg_buffer[receive_result] = 0;

    if (isNotification(flags))
    {
        const union sctp_notification* notification = (const union sctp_notification*)socket->msg_buffer;
        const IPSocket ip_socket = sockaddrStoragetoIPSocket(&from_storage);
        handleNotification(socket, &ip_socket, notification);
    }
    else
        INFO_LOG("Received message: %s (size = %Zu)", socket->msg_buffer, receive_result);
}

void sendSocketSctp(Socket* socket, ChatMessage msg)
{
    const size_t msg_size = strlen(msg);

    for (Count i = 0; i < socket->peer_count; ++i)
    {
        const Peer* peer = socket->peers + i;
        if (peer->fd)
        {
            INFO_LOG(
                "Sending message: %s (size = %Zu) on fd = %d, assoc_id = %d", msg, msg_size, peer->fd, peer->assoc_id);
            checkSend(send(peer->fd, msg, msg_size, ignore_flags));
        }
    }
}
