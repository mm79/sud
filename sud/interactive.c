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
#include <sys/time.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <sys/stat.h>
#include <sys/wait.h>

#include <errno.h>
#ifdef HAVE_LIBUTIL_H
#include <libutil.h>
#endif
#include <paths.h>
#ifdef HAVE_PTY_H
#include <pty.h>
#endif
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <time.h>
#include <unistd.h>
#ifdef HAVE_UTIL_H
#include <util.h>
#endif
#include <utmp.h>

#include "conf.h"
#include "control.h"
#include "extern.h"


static int pipechld[2] = { -1, -1 };

static int select_fd(struct conf *, int, int, int, int);
static RETSIGTYPE sig_chld(); 

static RETSIGTYPE
sig_chld()
{
	int saved_errno = errno;

	if (pipechld[1] != -1)
		(void)write(pipechld[1], "", 1);
        
	errno = saved_errno;
}

static int
select_fd(struct conf *cfp, int sock, int master, int ctrls, int parentfd)
{
	char sockbuf[1024], ptybuf[1024];
	struct timeval timeout, *tp = NULL;
	char *pbp = NULL, *sbp = NULL;
	ssize_t pcc = 0, scc = 0, n;
	int rsel, maxfd;

	(void)non_blocking(sock);
	(void)non_blocking(master);
	(void)non_blocking(ctrls);
	(void)non_blocking(parentfd);

	(void)signal(SIGTTOU, SIG_IGN);

	maxfd = sock    > master  	? sock    	: master;
	if (maxfd < ctrls)
		maxfd = ctrls;
	if (maxfd < pipechld[0])
		maxfd = pipechld[0];
	if (maxfd < parentfd)
		maxfd = parentfd;
	maxfd += 1;

	for (;;) {
		fd_set rset, wset, *omask;	

		FD_ZERO(&rset);
		FD_ZERO(&wset);

		omask = (fd_set *) NULL;

		if (scc) {
			FD_SET(master, &wset);
			omask = &wset;
		} else
			FD_SET(sock, &rset);

		if (pcc >= 0) {
			if (pcc) {
				FD_SET(sock, &wset);
				omask = &wset;
			} else
				FD_SET(master, &rset);
		}

		FD_SET(ctrls, &rset);
		FD_SET(pipechld[0], &rset);
		FD_SET(parentfd, &rset);
	
		/*
		 * Posix.1g defines timeout parameter to const
		 * Linux uses a value-result timeout
		 */
		if (cfp->timeout) {
			timeout.tv_sec  = cfp->timeout;
			timeout.tv_usec = 0;
			tp = &timeout;
        	}

		if ((rsel = select(maxfd, &rset, omask, NULL, tp)) == -1) {
			if (errno == EINTR)
				continue;
			else {
				syslog(LOG_ERR, "selectfd %m"); 
				return -1;
			}
		}

		if (rsel == 0) {
			if (tp) {
				syslog(LOG_INFO, 
					"[%s] session time expired for %s",
					cfp->session,
					get_pwentry(cfp->setuser)->pw_name);
				/*
				 * giving a chance to know timeout
				 * XXX: writen()?
				 */
				(void)write(sock, "\r\nsession timeout\r\n", 
					(size_t)19); 
				return 0;
			} else 
				continue;
		}

		if (FD_ISSET(parentfd, &rset)) {
			char c;

			(void)read(parentfd, &c, 1);

			return 0;
		}

		if (FD_ISSET(pipechld[0], &rset)) {
			pid_t pid;
			int st;
			char c;

			if (pipechld[0] != -1)
				(void)read(pipechld[0], &c, 1);

			while ((pid = waitpid(-1, &st, WNOHANG|WUNTRACED))) {
				if (pid < 0) {
					if (errno == EINTR)
						continue;
					else
						break;
				}

				if (WIFSTOPPED(st)) {
					(void)kill(pid, SIGCONT);
					continue;
				}
			}
		} 

		if (FD_ISSET(ctrls, &rset)) {
			struct schck sck;
	
			n = read(ctrls, &sck, sizeof(sck));
		
			if (n == sizeof(sck))
				(void)control_check(ctrls, &sck, master);
                }

		if (FD_ISSET(sock, &rset)) {
			scc = read(sock, sockbuf, sizeof(sockbuf));
			if (scc < 0 && errno == EWOULDBLOCK)
				scc = 0;
			else {
				if (scc <= 0)
					return (scc) ? -1 : 0;
	
				sbp = sockbuf;
			
				FD_SET(master, &wset);
			}			
		}

		if (FD_ISSET(master, &wset) && scc > 0) {
			n = write(master, sbp, (size_t)scc);
		
			if (n < 0 && errno == EWOULDBLOCK) 
				continue;
			else if (n < 0) 
				return -1;

			if (n > 0) 
				scc -= n, sbp += n;
		}

		if (FD_ISSET(master, &rset)) {
			pcc = read(master, ptybuf, sizeof(ptybuf));
			if (pcc < 0 && errno == EWOULDBLOCK) 
				pcc = 0;
			else {
				if (pcc <= 0) 
					return (pcc) ? -1 : 0;

				pbp = ptybuf;

				FD_SET(sock, &wset);
			}
		}

		if (FD_ISSET(sock, &wset) && pcc > 0) {
			n = write(sock, pbp, (size_t)pcc);

			if (n < 0 && errno == EWOULDBLOCK)
				continue;
			else if (n < 0) 
				return -1;

			if (n > 0) 
				pcc -= n, pbp += n;
		}
	}

	/*
	 * never reached
	 */
	return -1;
}

void
exec_shell(struct conf *cfp, int fd, int parentfd)
{
	int master, slave;
	pid_t pid;
	char line[MAXPATHLEN];	
	char *tty;
	struct utmp ut;

	memset(&ut, 0, sizeof(ut));

	if (pipe(pipechld) < 0) {
		syslog(LOG_ERR, "exec_shell() pipe %m");
		_exit(1);
	}

	(void)non_blocking(pipechld[0]);
	(void)non_blocking(pipechld[1]);

	if (openpty(&master, &slave, line, NULL, NULL) == -1) {
		syslog(LOG_ERR, "openpty %m");
		_exit(1) ;
	}

	/*
	 * pts/x compatible
	 */
	if ((tty = strstr(line, "/dev/"))) 
        	tty += 5;
       	else
       		tty = line;

	if (cfp->utmp) {
		if (cfp->utname)
                	(void)strncpy(ut.ut_name, cfp->utname, 
				sizeof(ut.ut_name));
		else {
			struct passwd *pw;
			pw = get_pwentry(cfp->havesetuser ? cfp->setuser : \
				0);

			(void)strncpy(ut.ut_name, pw->pw_name, 
				sizeof(ut.ut_name)); 
		}
    
		(void)strncpy(ut.ut_line, tty, sizeof(ut.ut_line));

		if (cfp->uthost)
			(void)strncpy(ut.ut_host, cfp->uthost, 
				sizeof(ut.ut_host));
	
		(void)time(&ut.ut_time);	
	}

        /*
         * overwriting signal disposition
         */
        (void)signal(SIGCHLD, sig_chld);

	switch (pid = fork()) {
	case -1:
		syslog(LOG_ERR, "forkpty: %m");
		_exit(1);	
	case 0:
		(void)close(parentfd);
		(void)close(pipechld[0]);
		(void)close(pipechld[1]);
		(void)close(master);
               	(void)close(fd);	
		(void)login_tty(slave);
		login(&ut);

		set_privileges(cfp);		

		/*
		 * SUIP PROGRAM HERE
		 */
#ifdef __NetBSD__
		(void)execl(_PATH_BSHELL,"sh", "-c",cfp->suipfile,(char *)NULL);
#else
		(void)execl(_PATH_BSHELL, "sh", "-p", "-c", cfp->suipfile, 
			(char *)NULL);
#endif
		_exit(127);
	default:
	{
		int ctrls;	
		int exit_status = 0;
	
		(void)close(slave);
	
		/*
		 * trying to open a control channel
		 * control_create() returns the number of bytes which were 
		 * written
		 * select_fd() returns -1 if errors exist you can check errno
		 */
		if (control_create(&ctrls, fd) == 1) { 
			if (select_fd(cfp, fd, master, ctrls, parentfd) < 0)
				exit_status = 1; 
		} else {
			syslog(LOG_ERR, "can't open ctrl chan");
			exit_status = 1;
		}

		if (cfp->utmp) {
			if (!logout(tty)) { 
				syslog(LOG_ERR, "unable to logout on %s", tty);
				exit_status = 1;
			} else
				logwtmp(tty, "", "");
		}

		cleanup(line);
		_exit(exit_status);
	}}	

	/*
	 * never reached
	 */
	_exit(1);
}
