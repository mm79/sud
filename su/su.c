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
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/poll.h>
#include <sys/un.h>
#include <sys/uio.h>
#include <sys/ioctl.h>
#include <sys/wait.h>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <termios.h>
#include <unistd.h>

#include "control.h"
#include "extern.h"


struct winsize winsize;
int pipewinch[2] = { -1, -1 };
int interactive = 1;

int
get_window_size(int fd, struct winsize *wp)
{
	return ioctl(fd, TIOCGWINSZ, wp);
}

void
sendwindow(int ctrls)
{
	struct schck schk;

	memset(&schk, 0, sizeof schk);
	schk.s_type = S_CHGWIN;
	schk.s_wsiz = winsize;

	if (ctrls > 0) 
		(void)write(ctrls, &schk, sizeof(struct schck));
}

RETSIGTYPE
sig_winch()
{
	int saved_errno = errno;

	if (pipewinch[1] != -1)
		(void)write(pipewinch[1], "", 1);
	
	errno = saved_errno;
}

void
mode(int f)
{
	static struct termios deftty;
	struct termios tty;

        switch (f) {
        case 0:
                (void)tcsetattr(0, TCSANOW, &deftty);
                break;
        case 1:
                (void)tcgetattr(0, &deftty);
                tty = deftty;
                tty.c_lflag &= ~(ECHO|ICANON|ISIG|IEXTEN);
                tty.c_iflag &= ~ICRNL;
                tty.c_oflag &= ~OPOST;
                tty.c_cc[VMIN] = 1;
                tty.c_cc[VTIME] = 0;
                (void)tcsetattr(0, TCSANOW, &tty);
                break;
        default:
                return;
        }
}

int
non_blocking(int fd)
{
	int fval = fcntl(fd, F_GETFL, 0);

	if (fval < 0) {
		perror("fcntl(F_GETFL)");
		exit(1);
	}

	if ((fval & O_NONBLOCK) == 0) {
		fval |= O_NONBLOCK;
		(void)fcntl(fd, F_SETFL, fval);
	}
	
	return fval;
}

ssize_t
writen(int fd, const void *msg, size_t n)
{
        ssize_t nwr;
        size_t nleft = n;
        const char *ptr = msg;

        while (nleft > 0) {
                if ((nwr = write(fd, ptr, nleft)) < 0) {
                        if (errno == EWOULDBLOCK || errno == EINTR)
                                continue;
			else
                                return ((ssize_t)-1);
                }

                nleft -= nwr;
                ptr += nwr;
        }

        return ((ssize_t)n);
}

int 
select_fd(int fd, int ctrls)
{
	(void)non_blocking(fd);

	if (interactive) {
		if (!isatty(STDOUT_FILENO))
			fprintf(stderr, "warning stdout is not a tty\n");

		if (!isatty(STDIN_FILENO))
			fprintf(stderr, "warning stdin is not a tty\n");

		/*
	 	 * pipe to manage SIGWINCH in select
	 	 */
		if (pipe(pipewinch) < 0) {
			perror("pipe");
			return -1;
	 	}

		(void)non_blocking(pipewinch[0]);
		(void)non_blocking(pipewinch[1]);
		(void)non_blocking(ctrls);

		/* forcing a pipewinch write */
		(void)raise(SIGWINCH);
	}

        for (;;) {
		char buf[BUFSIZ];
		int nfd;
		struct pollfd pollfds[nfd = interactive ? 3 : 2];
		ssize_t n;
		int ret;

		bzero(pollfds, sizeof pollfds);
		pollfds[0].fd = STDIN_FILENO;
		pollfds[0].events = POLLIN;
		pollfds[1].fd = fd;
		pollfds[1].events = POLLIN;
		if (interactive) {
			pollfds[2].fd = pipewinch[0];
			pollfds[2].events = POLLIN;
		}

		ret = poll(pollfds, nfd, -1);
		if (ret == 0)
			continue;
 		if (ret < 0) {
			if (errno == EINTR)
				continue;
			else
				return -1;
		}

		if (pollfds[0].revents) {
			if ((n = read(STDIN_FILENO, buf, sizeof buf)) < 0) {
				if (errno == EINTR)
					continue;
				else
					return -1;
			}
			if (n == 0)
				(void)shutdown(fd, SHUT_WR);
			else if (writen(fd, buf, (size_t)n) < 0)
				return -1;
		}
	
		if (pollfds[1].revents) {
			if ((n = read(fd, buf, sizeof buf)) < 0) {
				if (errno == EWOULDBLOCK)
					continue;
				else
					return -1;
			}
			if (n == 0) 
				return 0;
			if (writen(STDOUT_FILENO, buf, (size_t)n) < 0)
				return -1;
		}

		if (interactive && pollfds[2].revents) {
			struct winsize ws;
                        char c;

			if (read(pipewinch[0], &c, 1) < 0) {
				/* 
				 * Errors (like EWOULDBLOCK) shouldn't happen 
				 * since we are the only reader and the reading
				 * is atomic for a buffer <= PIPE_BUF
				 */
				if (errno != EWOULDBLOCK) {
					pollfds[2].fd = -1;
                		        pollfds[2].events = 0;
					interactive = 0;
					nfd--;
				}
				continue;
			}

                        if ((get_window_size(STDIN_FILENO, &ws) != -1) &&
                                        memcmp(&ws, &winsize, sizeof(ws))) {
                                winsize = ws;
                                sendwindow(ctrls);
                        }
		}	
	} 

	/*
	 * never reached
	 */
	return -1;
}

ssize_t
read_fd(int fd, void *ptr, size_t nbytes, int *recvfd)
{
        struct msghdr msg;
        struct cmsghdr *cmptr;
        struct iovec iov[1];
        char control[CMSG_SPACE(sizeof(int))];
        ssize_t n;

        iov[0].iov_base = ptr;
        iov[0].iov_len  = nbytes;

	memset(&msg, 0, sizeof msg);
        msg.msg_control = (caddr_t)control;
        msg.msg_controllen = sizeof(control);
        msg.msg_iov = iov;
        msg.msg_iovlen = 1;

        if ((n = recvmsg(fd, &msg, 0)) <= 0)
                return (n);

        if ((cmptr = CMSG_FIRSTHDR(&msg)) != NULL &&
                cmptr->cmsg_len == CMSG_LEN(sizeof(int))) {

                if (cmptr->cmsg_level != SOL_SOCKET) {
                        printf("!= SOL_SOCKET\n");
                        exit(1);
                }

                if (cmptr->cmsg_type != SCM_RIGHTS) {
                        printf("!= SCM_RIGHTS\n");
                        exit(1);
                }

                *recvfd = *((int *) CMSG_DATA(cmptr));
        } else
                *recvfd = -1;

        return (n);
}

ssize_t 
su_control(int from, int *pfd)
{
        char c;

	return read_fd(from, &c, 1, pfd);
}

void
usage(char *prog)
{
	printf(	"usage: %s\n\n"
		"-i for interactive mode\n"
		"-n for non-interactive mode\n"
		"-p sockfile\n\n", prog);

	exit(0);
}

int
main(int argc, char *argv[])
{
	struct sockaddr_un s_un;
	char *to_name = NULL;
	int s, ctrls;
	int ch;

	while ((ch = getopt(argc, argv, "hinp:")) != -1)
		switch (ch) {
                case 'i':
                        interactive = 1;
                        break;
                case 'n':
                        interactive = 0;
                        break;
		case 'p':
			if ((to_name = strdup(optarg)) == NULL) {
				fprintf(stderr, "Out of memory\n");
				exit(1);
			}
			break;
		default:
			usage(argv[0]);
		}

	if (to_name == NULL && (to_name = strdup(SOCK_NAME)) == NULL) {
		fprintf(stderr, "Out of memory\n");
		exit(1);
	}

	memset(&s_un, 0, sizeof(s_un));
	s_un.sun_family = AF_UNIX;

#ifdef HAVE_STRLCPY
        (void)strlcpy(s_un.sun_path, to_name, sizeof(s_un.sun_path));
#else
        (void)strncpy(s_un.sun_path, to_name, sizeof(s_un.sun_path)-1);
        s_un.sun_path[sizeof(s_un.sun_path)-1] = '\0';
#endif

	if ((s = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
               	perror("socket");
		exit(1);
	} 

	if (connect(s, (const struct sockaddr *)&s_un, SUN_LEN(&s_un))) {
		perror("connect");
		exit(1);
	}	

	if (interactive && su_control(s, &ctrls) == 1) {
		int ret_status = 0;

		(void)signal(SIGWINCH, sig_winch);

		mode(1);
		if (select_fd(s, ctrls) < 0)
			ret_status = 1;
		mode(0);
		
		return (ret_status); 	
	} else
		return select_fd(s, -1) < 0 ? 1 : 0; 

	/*
	 * not reached
	 */
	return(1);
}
