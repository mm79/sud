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
#include <sys/stat.h>

#include <errno.h>
#include <fcntl.h>
#include <grp.h>
#include <pwd.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "conf.h"
#include "control.h"
#include "extern.h"


#if defined(HAVE_GETPEEREID) || defined(SO_PEERCRED)
int
is_group(uid_t uid, gid_t gid)
{
	struct passwd *pw;	
	struct group *pgrp;
	char **p;

	if ((pw = getpwuid(uid)) == NULL) {
		syslog(LOG_ERR, "getpwuid failed: %m");
		return (-1);
	}

	if ((pgrp = getgrgid(gid)) == NULL) {
		syslog(LOG_ERR, "getgrgid failed: %m");
		return (-1);
	}

	if (strcmp(pgrp->gr_name, pw->pw_name) == 0)
		return 1;

	for (p = pgrp->gr_mem; *p; p++) 
		if(strcmp(pw->pw_name, *p) == 0)
			return 1;

	return 0;
}
#endif

void
set_privileges(struct conf *cfp)
{
	if (cfp->havesetgroup) {
		struct group *gr;
                        
		set_allgroups(cfp, 0);
		gr = get_group(cfp->setgroup);

		if (cfp->havesetegroup) {
			gid_t rgid = gr->gr_gid;

			gr = get_group(cfp->setegroup);
#ifdef HAVE_SETRESGID
			if (setresgid(rgid, gr->gr_gid, gr->gr_gid) < 0) {
				syslog(LOG_ERR, "setresgid failed: %m");
				_exit(1);
			}
#else
			if (setregid(rgid, gr->gr_gid) < 0) {
				syslog(LOG_ERR, "setregid failed: %m");
				_exit(1);
			}
#endif
		} else
			(void)setgid(gr->gr_gid);
	} else
		set_allgroups(cfp, 1);
                
	if (cfp->havesetuser) {
		struct passwd *pw = get_pwentry(cfp->setuser);

#ifdef HAVE_SETLOGIN
		/*
		 * login_tty() calls setsid()
		 */
		if (cfp->mode == INTERACTIVE)
			(void)setlogin(cfp->utname ? cfp->utname : \
                        	        pw->pw_name);
#endif

		if (cfp->haveseteuser) {
			uid_t ruid = pw->pw_uid;

			pw = get_pwentry(cfp->seteuser);

#ifdef HAVE_SETRESUID
			if (setresuid(ruid, pw->pw_uid, pw->pw_uid) < 0) {
				syslog(LOG_ERR, "setresuid failed %m");
				_exit(1);
			}
#else
			if (setreuid(ruid, pw->pw_uid) < 0) {
				syslog(LOG_ERR, "setreuid failed %m");
				_exit(1);
			}
#endif
		} else {
			/*
			 * drop privileges
			 * we're root so real, effective
			 * and saved uid are changed
			 */
			if (setuid(pw->pw_uid) < 0) {
				syslog(LOG_ERR, "setuid failed? %m");
				_exit(1);
			}
		}
	} else {        /* !havesetuser */
		if (setuid(0) < 0) {
			syslog(LOG_ERR, "setuid(0) %m");
			_exit(1);
		}

#ifdef HAVE_SETLOGIN
		if (cfp->mode == INTERACTIVE)
                	(void)setlogin(cfp->utname ? cfp->utname : "root");
#endif
	}
}

void
cleanup(char *line)
{
	if (line) {
		/*
		 * XXX: this is not necessary on some systems 
		 */ 
		(void) chmod(line, 0666);
		(void) chown(line, 0, 0);
	}
}

int
non_blocking(int fd)
{
        int fval = fcntl(fd, F_GETFL, 0);

        if (fval < 0) {
                syslog(LOG_ERR, "non_blocking:fcntl(%d, F_GETFL, 0): %m", fd);
		return -1;	
	}

        if ((fval & O_NONBLOCK) == 0) {
                fval |= O_NONBLOCK;
                (void)fcntl(fd, F_SETFL, fval);
        } 

        return fval;
}

#ifndef HAVE_DAEMON

#ifndef _PATH_DEVNULL
#define _PATH_DEVNULL "/dev/null"
#endif

int
daemon(nochdir, noclose)
        int nochdir, noclose;
{
        int fd;

        switch (fork()) {
        case -1:
                return (-1);
        case 0:
                break;
        default:
                _exit(0);
        }

        if (setsid() == -1)
                return (-1);

        if (!nochdir)
                (void)chdir("/");

        if (!noclose && (fd = open(_PATH_DEVNULL, O_RDWR, 0)) != -1) {
                (void)dup2(fd, STDIN_FILENO);
                (void)dup2(fd, STDOUT_FILENO);
                (void)dup2(fd, STDERR_FILENO);
                if (fd > 2)
                        (void)close(fd);
        }

        return (0);
}
#endif /* HAVE_DAEMON */

struct passwd *
get_pwentry(uid_t uid)
{
	struct passwd *pw;

	if ((pw = getpwuid(uid)) == NULL) {
		syslog(LOG_ERR,  "unable to get pw entry for id %lu",
			(unsigned long)uid);
		_exit(1);
	}
	
	return pw;
}	

struct group *
get_group(gid_t gid)
{
	struct group *gr;

	if ((gr = getgrgid(gid)) == NULL) {
		syslog(LOG_ERR, "unable to get group entry for id %lu",
			(unsigned long)gid);
		_exit(1);
	}	
	
	return gr;
}	

void
set_allgroups(struct conf *cfp, int flag)
{
	struct passwd *pw = get_pwentry(cfp->havesetuser ? \
			cfp->setuser : 0);

	(void)initgroups(pw->pw_name, pw->pw_gid);
	if (flag)
		/*
		 * we are root so all three IDS are changed
		 */ 
		(void)setgid(pw->pw_gid);
}

long
getDtabsize(void)
{
	return sysconf(_SC_OPEN_MAX);
}

int
write_pidfile(char *pidfile, int locking)
{
        char line[255];  
        struct flock lock;
        int fd, fval;

	if (!locking) 
		(void)unlink(pidfile);

        if ((fd = open(pidfile, O_RDWR|O_CREAT, 0644)) < 0) {
                syslog(LOG_ERR, "%s: %m", pidfile);
		return -1;
        }

	if (locking) {                
        	if ((fval = fcntl(fd, F_GETFD, 0)) < 0) {
                	syslog(LOG_ERR, "fcntl(F_GETFD) %s: %m", pidfile);
                	return -1;
        	}
        
        	fval |= FD_CLOEXEC;
  	      
		if (fcntl(fd, F_SETFD, fval) < 0) {
			syslog(LOG_ERR, "fcntl(F_SETFD) %s: %m", pidfile);
			return -1;
        	}
               
        	lock.l_type = F_WRLCK;
        	lock.l_start = 0;
        	lock.l_len = 0;
        	lock.l_whence = SEEK_SET;
                 
        	if (fcntl(fd, F_SETLK, &lock) < 0) {
                	if (errno == EAGAIN && fcntl(fd, F_GETLK, &lock) > -1)
                        	syslog(LOG_ERR, 
					"service already runs at pid %ld",
                                        (long)lock.l_pid);
                	else
                        	syslog(LOG_ERR, "%s: %m", pidfile);

                	return -1;
        	}
	}
        
        (void)snprintf(line, sizeof(line), "%ld\n", (long)getpid());
        if (write(fd, line, strlen(line)) < 0) {
                syslog(LOG_ERR, "writing pidfile: %m");
                return -1;
        }

	return (locking ? fd : close(fd));
}
