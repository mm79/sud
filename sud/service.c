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
#include <sys/wait.h>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>

#include <errno.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "conf.h"
#include "extern.h"
#include "queue.h"
#include "session.h"


static int 	sun_desc;
static int 	pipechld[2] 	= { -1, -1 };

static RETSIGTYPE	service_sigchld();

int
service_init(struct conf *cfp)
{
	struct sockaddr_un s_un;

	memset(&s_un,  0, sizeof(struct sockaddr_un));

	if (cfp->pidfile)
                (void)write_pidfile(cfp->pidfile, 0);
 
	s_un.sun_family = AF_UNIX;
#ifdef HAVE_STRLCPY
	(void)strlcpy(s_un.sun_path, cfp->sockfile, sizeof(s_un.sun_path));
#else
        (void)strncpy(s_un.sun_path, cfp->sockfile, 
		sizeof(s_un.sun_path)-1);
        s_un.sun_path[sizeof(s_un.sun_path)-1] = '\0';
#endif
        (void)unlink(cfp->sockfile);

        if ((sun_desc = socket(AF_UNIX, SOCK_STREAM, 0)) < 0) {
                syslog(LOG_ERR, "socket(AF_UNIX, SOCK_STREAM, 0): %m)");
                return -1;
        }

        if (bind(sun_desc, (struct sockaddr *)&s_un, SUN_LEN(&s_un)) < 0) {
                syslog(LOG_ERR, "bind: %m");
		return -1;
        }

        if (chmod(cfp->sockfile, cfp->haveauthgroup ? S_IRWXU|S_IRWXG : \
                        S_IRWXU|S_IRWXG|S_IRWXO) < 0) {
                syslog(LOG_ERR, "chmod: %m");
                return -1;
        }

        if (chown(cfp->sockfile, 0, cfp->haveauthgroup ? cfp->authgroup: \
                        0) < 0) {
                syslog(LOG_ERR, "chown: %m");
                return -1;
        }

	if (listen(sun_desc, cfp->nclients) < 0) {
                syslog(LOG_ERR, "listen: %m");
                return -1;
        }

	return 0;
}

static RETSIGTYPE
service_sigchld()
{
	int saved_errno = errno;

	if (pipechld[1] != -1)
		(void)write(pipechld[1], "", 1);

	errno = saved_errno;
}

int
service_dispatch(struct conf *confp, int pipefd)
{
	struct shead shead;
	struct sockaddr_un cliaddr;
	socklen_t clilen;
	int s_newreq, maxfd;
	int servergone  = 0;

#ifdef HAVE_SETPROCTITLE
	setproctitle("%s service", confp->session);
#endif

	s_head_init(&shead);

	if (pipe(pipechld) < 0) {
		syslog(LOG_ERR, "service_dispatch pipe_chld: %m");
		return -1;
	}

	(void)non_blocking(sun_desc);
	(void)non_blocking(pipefd);
	(void)non_blocking(pipechld[0]);
	(void)non_blocking(pipechld[1]);
	
	maxfd = pipefd > sun_desc ? pipefd : sun_desc;
	if (maxfd < pipechld[0])
		maxfd = pipechld[0];
	maxfd += 1;

	if (signal(SIGCHLD, service_sigchld) == SIG_ERR) {
		syslog(LOG_ERR, "signal() %m");
		return -1;
	}
	
        for (;;) {
		fd_set rset;

		FD_ZERO(&rset);

		if (!servergone) {
			FD_SET(sun_desc, &rset);
			FD_SET(pipefd, &rset);
		} else {
			/*
			 * do not accept other sessions
			 * set pipechld if we need to read wait status  
			 */
			if (LIST_EMPTY(&shead))
				_exit(0);
		}
 
		FD_SET(pipechld[0], &rset);

		if (select(maxfd, &rset, NULL, NULL, NULL) == -1) {
			if (errno == EINTR)
				continue;
			else {
				syslog(LOG_ERR, "service select %m");
				_exit(1);
			}
		}

		if (FD_ISSET(pipechld[0], &rset)) {
			pid_t pid;
			int status;
			char c;

			if (pipechld[0] != -1)
				(void)read(pipechld[0], &c, 1);

			while ((pid = waitpid(-1, &status, 
					WNOHANG|WUNTRACED))) {
				if (pid < 0) {
					if (errno == EINTR)
						continue;
					else
						break;

					if (WIFSTOPPED(status)) {
						(void)kill(pid, SIGCONT);
						continue;
					}
				}
	
				s_rm_with_pid(&shead, pid);
			}
		}	

		if (FD_ISSET(sun_desc, &rset)) {
			int ctlpipe[2];
			pid_t pid;

			clilen = sizeof(struct sockaddr_un);

			if ((s_newreq = accept(sun_desc,
				(struct sockaddr *)&cliaddr, &clilen)) < 0) {
                        	if (errno == EINTR) 
			       		continue;
				else {
                                	syslog(LOG_ERR, "accept %m");
                                	return -1;
                        	}
                	}

			if (pipe(ctlpipe) < 0) {
				syslog(LOG_ERR, "creating ctlpipe: %m");
				return -1;
			}

 	               	switch ((pid = fork())) {
        	       	case -1:
				syslog(LOG_ERR, "accept fork(): %m");
				(void)close(ctlpipe[0]);
				(void)close(ctlpipe[1]);
                	        (void)close(s_newreq);
                        	sleep(1);
                        	continue;
			case 0:
#ifdef HAVE_SETPROCTITLE
      				setproctitle("%s child", confp->session); 
#endif
				(void)close(sun_desc);
				(void)close(pipefd);
 				(void)close(pipechld[0]);
				(void)close(pipechld[1]);
				(void)close(ctlpipe[1]);
	
				sud_process(confp, s_newreq, ctlpipe[0]);
                        	/*
                        	 * never reached
                        	 */
                        	break;
                	default:
			{
				struct sctl *sctl;

				if ((sctl = s_alloc()) == NULL) {
					syslog(LOG_ERR, "s_alloc %m");
					return -1;
				}

				sctl->s_pid = pid;
				sctl->s_pipedesc = ctlpipe[1];
				(void)non_blocking(ctlpipe[1]);
				s_ins(&shead, sctl);

				(void)close(ctlpipe[0]);
                        	(void)close(s_newreq);
                	}}
		}

		if (FD_ISSET(pipefd, &rset)) {
			char buf[PIPE_BUF];
			ssize_t n;

			n = read(pipefd, buf, sizeof(buf));
			if (n <= 0) {
				struct sctl *sctl;

				(void)close(sun_desc);
				servergone = 1;
				/*
				 * send notify to our sessions
				 */
				LIST_FOREACH(sctl, &shead, s_lentry)
					(void)write(sctl->s_pipedesc, "", 1);
			}
		}
        }

	/*
	 * never reached
	 */
	return -1;
}
