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

#include <errno.h>
#include <stdio.h>
#include <pwd.h>
#include <syslog.h>
#include <unistd.h>

#include "conf.h"
#include "extern.h"

static int select_mode(struct conf *, int, int *, int);

static int
select_mode(struct conf *cfp, int sock, int *pipefd, int parentfd)
{
	char sockbuf[BUFSIZ], pipebuf[PIPE_BUF];
	struct timeval timeout, *tp = NULL;
	ssize_t pcc = 0, scc = 0, n;
	char *pbp = NULL, *sbp = NULL;
	int rsel, maxfd;
	int bye = 0;
	
	(void)non_blocking(sock);
	(void)non_blocking(parentfd);

	maxfd = sock > parentfd ? sock : parentfd;

	if (pipefd[0] != -1) {
		(void)non_blocking(pipefd[0]);
		if (pipefd[0] > maxfd)
			maxfd = pipefd[0];
	}		

	if (pipefd[1] != -1) {
		(void)non_blocking(pipefd[1]);
		if (pipefd[1] > maxfd)
			maxfd = pipefd[1];
	}

	maxfd += 1;

	if (cfp->mode == READ)
                (void)shutdown(sock, SHUT_RD);
        if (cfp->mode == BLIND)
                (void)shutdown(sock, SHUT_WR);

	for (;;) {
		fd_set rset, wset, *omask;

		FD_ZERO(&rset);
		FD_ZERO(&wset);

		omask = (fd_set *) NULL;

		switch (cfp->mode) {
		case READWRITE:
			if (!bye) {
				if (scc) {
					FD_SET(pipefd[1], &wset);
					omask = &wset;
				} else	 
					FD_SET(sock, &rset);
			}

                	if (pcc >= 0) {
                        	if (pcc) {
                                	FD_SET(sock, &wset);
                                	omask = &wset;
                        	} else
                        		FD_SET(pipefd[0], &rset);
			}
			break;
		case BLIND:
			if (bye)
				return 0;

			if (scc) {
				FD_SET(pipefd[1], &wset);
				omask = &wset;
			} else 
				FD_SET(sock, &rset);

			break;
		case READ:
			if (pcc >= 0) {
				if (pcc) {
					FD_SET(sock, &wset);
					omask = &wset;
				} else
					FD_SET(pipefd[0], &rset);
			}
			break;
		default:
			return -1;	
		}

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
                                return 0;
                        } else
                                continue;
                }

                if (FD_ISSET(parentfd, &rset)) {
                        char c;

                        (void)read(parentfd, &c, 1);

                        return 0;
                }

                if (cfp->mode != BLIND && FD_ISSET(pipefd[0], &rset)) {
                        pcc = read(pipefd[0], pipebuf, sizeof(pipebuf));
		    
			if (pcc < 0 && errno == EWOULDBLOCK)
                                pcc = 0;
                        else {
                                if (pcc <= 0)
                                        return (pcc) ? -1 : 0;

                                pbp = pipebuf;

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
		
                if (cfp->mode != READ && FD_ISSET(sock, &rset)) {
                        scc = read(sock, sockbuf, sizeof(sockbuf));

                        if (scc < 0 && errno == EWOULDBLOCK)
                               	scc = 0;
                        else {
                                if (scc < 0)
                                        return -1;

				/*
				 * client write-shutdown or disconnection
				 */
				if (scc == 0) {
					bye = 1;
					(void)close(pipefd[1]);
					continue;
				} 

                                sbp = sockbuf;

                                FD_SET(pipefd[1], &wset);
                        }
                }

                if (cfp->mode != READ && FD_ISSET(pipefd[1], &wset)) {
                        n = write(pipefd[1], sbp, (size_t)scc);

                        if (n < 0 && errno == EWOULDBLOCK)
                                continue;
                        else if (n < 0)
                                return -1;

                        if (n > 0)
                                scc -= n, sbp += n;
                }
        }
}

void
exec_command(struct conf *cfp, int sockfd, int parentfd)
{
	int pipefd[2];

	set_privileges(cfp);

	if (sud_popen(cfp->suipfile, pipefd, cfp->mode) < 0) {
		syslog(LOG_ERR, "sud_popen error");
		_exit(1);
	}

	_exit(select_mode(cfp, sockfd, pipefd, parentfd) < 0 ? 1 : 0);
}
