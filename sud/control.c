/*
 * Copyright (c) 2003 Matteo Mazzarella <mm@cydonia.vpn.cuore.org>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * 3. The name of the author may not be used to endorse or promote products
 *    derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY
 * AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL
 * THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;
 * OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
 * OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF
 * ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "autoconf.h"

/* --- */
#include <sys/param.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <sys/uio.h>

#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "control.h"
#include "extern.h"


static ssize_t write_fd(int, void *, size_t, int);


static ssize_t
write_fd(int fd, void *ptr, size_t nbytes, int sendfd)
{
        struct msghdr msg;
        struct iovec iov[1];
        struct cmsghdr *cmptr;
        char control[CMSG_SPACE(sizeof(int))];

        iov[0].iov_base = ptr;
        iov[0].iov_len  = nbytes;

	memset(&msg, 0, sizeof msg);
        msg.msg_control	= (caddr_t)control;
        msg.msg_controllen = CMSG_LEN(sizeof(int));
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        cmptr = CMSG_FIRSTHDR(&msg);
        cmptr->cmsg_len = CMSG_LEN(sizeof(int));
        cmptr->cmsg_level = SOL_SOCKET;
        cmptr->cmsg_type = SCM_RIGHTS;
        *((int *) CMSG_DATA(cmptr)) = sendfd;

        return(sendmsg(fd, &msg, 0));
}

ssize_t
control_create(int *new, int to)
{
	int fds[2];
	ssize_t n;
	
	if (socketpair(AF_UNIX, SOCK_DGRAM, 0, fds) == -1) { 
		syslog(LOG_ERR, "socketpair %m");
		return -1;
	}	
	
	/*
	 * send fd
	 */
	if ((n = write_fd(to, "", 1, fds[1])) == -1)
		syslog(LOG_ERR, "write_fd %m");

	(void)close(fds[1]);
	*new = fds[0]; 

	return n;
}

int
control_check(int fd, const struct schck *sck, int master)
{
	switch (sck->s_type) {
	case S_CHGWIN:
		if (ioctl(master, TIOCSWINSZ, &sck->s_wsiz) == -1)
			syslog(LOG_ERR, "TIOCSWINSZ %m"); 
	default:
		/*
		 * not implemented
		 */
		return -1;
	}

	return 0;
}
