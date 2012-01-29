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
#include <sys/stat.h>

#include <fcntl.h>
#include <paths.h>
#include <stdio.h>
#include <syslog.h>
#include <unistd.h>

#include "extern.h"

/*
 * Some systems have a symbolic link in sh and some shells don't permit to
 * keep privileges dropping our wanted IDs by default 
 */
int
sud_popen(const char *command, int *pipefd, int mode)
{
        int pdes_r[2] = { -1, -1 };
	int pdes_w[2] = { -1, -1 };
#ifdef __NetBSD__
	char *argv[4];
#else
	char *argv[5];
#endif
	int i, fd, nfd;

	pipefd[0] = pipefd[1] = -1;

        if (mode != BLIND && pipe(pdes_r) < 0)
		return -1;

	if (mode != READ && pipe(pdes_w) < 0)
		return -1;

/*
 * XXX: fix me perhaps by checking for -p shell support in autoconf ?
 */
#ifdef __NetBSD__	
        argv[0] = "sh";
        argv[1] = "-c";
        argv[2] = (char *)command;
        argv[3] = NULL;
#else
        argv[0] = "sh";
	argv[1] = "-p";
	argv[2] = "-c";
	argv[3] = (char *)command;
	argv[4] = NULL;
#endif

        switch (vfork()) {
        case -1: 
		syslog(LOG_ERR, "sud_popen() vfork %m");
		_exit(1);
	case 0:
		/*
		 * not to close this descriptor could be useful for debugging
		 * but for security reason is better to detach it from our
		 * terminal: a user could alter the output if sud is invoked
		 * with -n option
		 */ 
		if ((fd = open(_PATH_DEVNULL, O_RDWR, 0)) < 0) {
			syslog(LOG_ERR, "unable to open /dev/null: %m");
			_exit(1);
		}

		(void)dup2(fd, STDERR_FILENO);
	
		/*
		 * this could be closed later when we call getDtabsize
		 */ 	
		if (fd > 2)
			(void)close(fd);
 
		if (mode != BLIND) {
			(void)close(pdes_r[0]);
			if (pdes_r[1] != STDOUT_FILENO) {
                		(void)dup2(pdes_r[1], STDOUT_FILENO);	
				(void)close(pdes_r[1]);	
			}
		}

		if (mode != READ) {
			(void)close(pdes_w[1]);
			if (pdes_w[0] != STDIN_FILENO) {
				(void)dup2(pdes_w[0], STDIN_FILENO);
				(void)close(pdes_w[0]);
			}
		}

		nfd = (int)getDtabsize();
		for (i = 3; i < nfd; i++) 
			(void)close(i);

                (void)execv(_PATH_BSHELL, argv);
                _exit(127);
	default:
		if (mode != BLIND) {
			(void)close(pdes_r[1]);
			pipefd[0] = pdes_r[0];
		}	

		if (mode != READ) {
			(void)close(pdes_w[0]);
			pipefd[1] = pdes_w[1];
		}
	}	

	return 0;
}
