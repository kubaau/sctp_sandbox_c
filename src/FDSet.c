#include "FDSet.h"
#include <tools_c/Repeat.h>
#include "SocketErrorChecks.h"

void resetFdSet(FDSet* fdset)
{
    fdset->selected_fds_count = 0;
    fdset->fd_max = 1;
}

void setFd(FDSet* fdset, FD fd)
{
    FD_SET(fd, &fdset->fds);
    if (fd >= fdset->fd_max)
        fdset->fd_max = fd + 1;
}

static struct timeval msToTimeval(Milliseconds ms)
{
    struct timeval ret;
    ret.tv_sec = 0;
    ret.tv_usec = ms * 1000;
    return ret;
}

static int selectImpl(FDSet* fdset, Timeout timeout)
{
    fd_set* write_fds = NULL;
    fd_set* except_fds = NULL;
    struct timeval timeout_timeval = msToTimeval(timeout);
    return checkedSelect(select(fdset->fd_max, &fdset->fds, write_fds, except_fds, &timeout_timeval));
}

void selectFdSet(FDSet* fdset, Timeout timeout)
{
    fdset->selected_fds_count = 0;
    if (selectImpl(fdset, timeout))
        repeat(fdset->fd_max) if (fdset->selected_fds_count < selected_fds_max_count and FD_ISSET(i, &fdset->fds))
            fdset->selected_fds[fdset->selected_fds_count++] = i;
}
