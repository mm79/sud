/*
 * Copyright (c) 2012 Matteo Mazzarella <matteo@dancingbear.it>
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
#include <sys/uio.h>

#include <grp.h>
#include <pwd.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "conf.h"
#include "extern.h"


#define set_id(cond, param, value) \
        ((cond) == USEMYID)  ? (param) = (value) : 0;

void
sud_process(struct conf *cfp, int fd, int pipefd) {
#if defined(HAVE_GETPEEREID) || defined(SO_PEERCRED)
        uid_t euid;
        gid_t egid;
#endif

#if defined(HAVE_GETPEEREID)
        if (getpeereid(fd, &euid, &egid) < 0) {
                syslog(LOG_ERR, "getpeereid failed: %m");
                if (cfp->haveauthgroup)
                        _exit(1);
        }
#endif

#if defined(SO_PEERCRED) && !defined(HAVE_GETPEEREID)
        struct ucred peercred;
#ifdef HAVE_SOCKLEN_T
	socklen_t olen = sizeof(struct ucred);
#else
        int olen = sizeof(struct ucred);
#endif

        if (getsockopt(fd, SOL_SOCKET, SO_PEERCRED, &peercred, &olen) < 0) {
                syslog(LOG_ERR, "can't get credential via SO_PEERCRED");
                if (cfp->haveauthgroup)
                        _exit(1);
        }

        euid = peercred.uid;
        egid = peercred.gid;
#endif

#if defined(HAVE_GETPEEREID) || defined(SO_PEERCRED)
        /*
         * XXX: LOG_INFO should be LOG_AUTH
         */
        if (cfp->haveauthgroup && is_group(euid, cfp->authgroup) != 1) {
                syslog(LOG_INFO, "[%s] auth failed u: %s [%lu] g: %s [%lu]",
                        cfp->session, get_pwentry(euid)->pw_name, 
			(unsigned long)euid, get_group(egid)->gr_name,
			(unsigned long)egid);
                _exit(1);
        }

	if (cfp->log)
        	syslog(LOG_INFO,"[%s] auth req u: %s [%lu] g: %s [%lu]",
				cfp->session, get_pwentry(euid)->pw_name, 
				(unsigned long)euid, get_group(egid)->gr_name,
				(unsigned long)egid);

        set_id(cfp->havesetuser,   cfp->setuser,   euid);
        set_id(cfp->haveseteuser,  cfp->seteuser,  euid);
        set_id(cfp->havesetgroup,  cfp->setgroup,  egid);
        set_id(cfp->havesetegroup, cfp->setegroup, egid);
#endif
	if (cfp->terminal)
		(void)setenv("TERM", cfp->terminal, 1);
	else
		(void)setenv("TERM", default_term ? default_term : "vt100",
				default_term ? 1 : 0);

	switch (cfp->mode) {
	case INTERACTIVE:
		exec_shell(cfp, fd, pipefd);
		break;
	case READ:
	case BLIND:
	case READWRITE:
		exec_command(cfp, fd, pipefd);
		break;
	}
	
	_exit(1);
}
