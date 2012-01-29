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

/*
 * This is a generic login program
 * The usage of your system login is preferable 
 */

#include "autoconf.h"

/* --- */
#ifdef __linux__
#define _BSD_SOURCE
#define _XOPEN_SOURCE
#endif

#include <sys/param.h>
#include <sys/time.h>
#include <sys/resource.h>

#include <errno.h>
#include <fcntl.h>
#include <paths.h>
#include <pwd.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

extern char **environ;

#ifndef __linux__
int
authenticated(char *password)
{
	struct passwd *pwd;
	size_t plen;
	char *salt;
	uid_t uid;

	uid = getuid();
	if ((pwd = getpwuid(uid)) == NULL)
		return 0;
		
	salt = pwd->pw_passwd;
	salt = (char *)crypt(password, salt);

	plen = strlen(password);
	memset(password, 0, plen);

	/*
         * XXX: LOG_INFO should be LOG_AUTH
         */
	if (!pwd || strcmp(salt, pwd->pw_passwd) != 0 ||
	    (*pwd->pw_passwd == '\0' && plen > 0))
		return (0);

	return (1);
}
#endif

RETSIGTYPE
sig_int()
{
	return;
}

void
showfile(const char *filename)
{
	char buffer[8192];
	struct sigaction osa, sa;
	ssize_t n;
	int fd;
    
	if ((fd = open(filename, O_RDONLY)) < 0)
		return ;
		
	/*
	 * signal() sets SA_RESTART on some systems (BSD, SVR4)
	 * SunOS uses SA_INTERRUPT to prevent interrupted system calls 
	 * from being restarted 
	 */
	memset(&sa, 0, sizeof(sa));
		
	sa.sa_handler = sig_int;
	sigemptyset(&sa.sa_mask);
#ifdef SA_INTERRUPT
	sa.sa_flags = SA_INTERRUPT;
#else
	sa.sa_flags = 0;                /* !SA_RESTART */
#endif
	(void)sigaction(SIGINT, &sa, &osa);
                
	while((n = read(fd, buffer, sizeof(buffer))) > 0 && 
		write(STDOUT_FILENO, buffer, (size_t)n) == n)	
			;
	        
	(void)sigaction(SIGINT, &osa, NULL);
	(void)close(fd);
}

int
main()
{
#ifndef __linux__
	char *ptr;
	int prio;
#endif
	struct passwd *pw;
	int auth;
	uid_t uid;

	showfile("/etc/issue.suz");
	uid = getuid();
	pw = getpwuid(uid);
	if (pw == NULL) {
		syslog(LOG_ERR, "i don't know who is %lu", (u_long)uid);
                exit(1);
        }

#ifdef __linux__
        auth = 1;
#else
	printf("realname> %s\n", pw->pw_name);	
	if ((ptr = getpass("password> ")) == NULL) {
		fprintf(stderr, "error getting password\n");
		exit(1);
	}

	errno = 0;
	prio = getpriority(PRIO_PROCESS, 0);
        if (errno)
		prio = 0;

	setpriority(PRIO_PROCESS, 0, -2);
	auth = authenticated(ptr);
	setpriority(PRIO_PROCESS, 0, prio);
#endif	
	if (auth) {
		char terminal[64];
		char pathname[MAXPATHLEN];

		syslog(LOG_INFO, "%s login on %s: success", pw->pw_name, 
			ttyname(0));

		showfile("/etc/motd.suz");

		if (chdir(pw->pw_dir) == -1 && chdir("/") == -1) { 
			perror("chdir(pw->pw_dir)");
			exit(1);
		}

		if (getcwd(pathname, MAXPATHLEN) == NULL) {
			perror("getcwd");
			exit(1);
		}
		
		printf("WARNING your current pathname is: %s\n", pathname);
		printf("terminal type: ");
		scanf("%63s", terminal);
		terminal[sizeof(terminal)-1] = '\0';

		if ((environ = calloc(1, sizeof (char *))) == NULL) {
                        perror("calloc");
			exit(1);
		}

                (void)setenv("TERM", 	terminal, 1);
                (void)setenv("PWD", 	pathname, 1);
		(void)setenv("HOME", 	pw->pw_dir, 1);
		(void)setenv("SHELL",	pw->pw_shell, 1);
		(void)setenv("LOGNAME",	pw->pw_name, 1);
		(void)setenv("USER",	pw->pw_name, 1);
		(void)setenv("PATH",	uid == 0 ? _PATH_STDPATH : \
			_PATH_DEFPATH, 1);

		if (execl(pw->pw_shell, pw->pw_shell, (char *)NULL) < 0) {
			perror("execl");
			exit(1);
		}
	} else {
		syslog(LOG_ERR, "%s on %s bad password", pw->pw_name, 
			ttyname(0));
		fprintf(stderr, "wrong password\n");
		exit(1);
	}

	return 0;
}
