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

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <syslog.h>
#include <unistd.h>

#include "conf.h"
#include "control.h"
#include "extern.h"


int nodaemon, nservices;
int emergency_maxtries = 10;
char *sud_emergency, *default_term, *fileconf, *sud_pidfile;

static volatile sig_atomic_t have_sigchld, have_sighup;
static int *pipes;
static int saved_argc;
static int lockfd = -1;
static char **saved_argv;
static char *program_fullpath;

static void free_pipes(int);
static void save_args(int, char **);
static void sud_opts(int, char **);
static void usage(char *);
static int sud_dispatch(int);
static RETSIGTYPE sud_sigchld();
static RETSIGTYPE sud_sighup();

static RETSIGTYPE
sud_sigchld()
{
	have_sigchld = 1;
}

static RETSIGTYPE
sud_sighup()
{
	have_sighup++;
}

static void
save_args(int argc, char **argv)
{
	char buf[MAXPATHLEN];
	int i;

	snprintf(buf, sizeof(buf), "%s/sbin/sud", SUD_PATH_PREFIX);
	program_fullpath = strdup(buf);
	if (program_fullpath == NULL) {
		syslog(LOG_ERR, "Out of memory strdup(%s)", buf);
		exit(1);
	}

        saved_argc = argc;
        saved_argv = calloc(argc + 1, sizeof(*saved_argv));
        for (i = 0; i < argc; i++) {
                if ((saved_argv[i] = strdup(argv[i])) == NULL) {
                        syslog(LOG_ERR, "Out of memory strdup(argv[%d])", i);
                        exit(1);
                }
        }
        saved_argv[argc] = NULL;
}

static void
usage(char *name)
{
	printf(	"%s %s usage: "
		"%s -f [configfile] -p [pidfile] -n -t terminal -v\n",
			name, PACKAGE_VERSION, name);
	exit(0);
}  

static void
free_pipes(int n)
{
	int i;

	if (pipes) {
		for (i = 0; i < n; i++)
			(void)close(pipes[i]);

		free(pipes);
		pipes = NULL;
	}
}

/*
 * this is safer than exiting
 */
void 
emergency_session(void)
{
	static int tries;
	struct conf *cfg = NULL;

	syslog(LOG_INFO, "*** EMERGENCY SESSION ***"); 

	while ((cfg = LIST_FIRST(&confhead))) {
		LIST_REMOVE(cfg, lentry);
		delconf(cfg);
	}
	
	initconf();

	do {
		if (cfg)
			delconf(cfg);

		while ((cfg = (struct conf *)calloc(1, sizeof(struct conf)))
				== NULL) { 
			syslog(LOG_ERR, 
				"cannot allocate a struct for emergency");
			sleep(2);
		}

		while ((cfg->session= strdup("emergency")) == NULL) {
			syslog(LOG_ERR, "strdup problem");
			sleep(2);
		}
	
		/*
	 	 * this prevents the overwriting of our suipfile
	 	 * it's very important to define emergency first of possible
	 	 * mistakes.. else setconf() will set a default emergency 
	 	 * service for us
	 	 */
		if (sud_emergency)
			while ((cfg->suipfile = strdup(sud_emergency)) == NULL)
			{ 
				syslog(LOG_ERR, "strdup sud_emergency");
				sleep(2);
			}
	} while (setconf(cfg) < 0) ;

        nservices = 1;
	LIST_INSERT_HEAD(&confhead, cfg, lentry);
	while (sud_dispatch(1) < 0) { 
		if (++tries > emergency_maxtries) {
			syslog(LOG_INFO, 
				"emergency: maxtries (%d) reached exiting",
				emergency_maxtries);
			exit(1);
		}
		sleep(2);
	}
}

static void
sud_opts(int argc, char *argv[])
{
	int ch;

	while ((ch = getopt(argc, argv, "f:np:t:v")) != -1)
		switch (ch) {
		case 'f':
			if (fileconf)
				free(fileconf);

			if ((fileconf = strdup(optarg)) == NULL) {
				fprintf(stderr,"Out of Memory on %s\n", 
						optarg);
				exit(1);
			}
			break;
                case 'n':
                        nodaemon = 1;
                        break;
		case 'p':
			if (sud_pidfile)
				free(sud_pidfile);

			if ((sud_pidfile = strdup(optarg)) == NULL) {
				fprintf(stderr,"Out of Memory on %s\n",
						optarg);
				exit(1);
			}
			break;
		case 't':
			if (default_term)
				free(default_term);

			if ((default_term = strdup(optarg)) == NULL) {
				fprintf(stderr, "Out of memory on %s\n",
						optarg);
				exit(1);
			}
			break;	
		case 'v':
			printf("%s version: %s\n", PACKAGE_NAME, 
				PACKAGE_VERSION);
			exit(0);
		default:
			usage(argv[0]);
		}
}

int
main(int argc, char *argv[])
{
	sud_opts(argc, argv);
	save_args(argc, argv);
	if (sud_dispatch(0) < 0)
		return 1;

	return 0;
}

void
sud_daemonize(void)
{
	if (!nodaemon) {
		if (daemon(0, 0) < 0) {
			perror("daemon");
			exit(1);
		}
		
		(void)signal(SIGHUP, sud_sighup);
	}

	if ((lockfd = write_pidfile(sud_pidfile ? sud_pidfile : \
                                "/var/run/sud.pid", 1)) < 0)
		exit(1);
}

static int
sud_dispatch(int emergency)
{
	struct conf *cfg;
	sigset_t defmask, suspmask;
	int lservices = 0;	/* loaded services */

        (void)sigemptyset(&defmask);
	if (!nodaemon)
        	(void)sigaddset(&defmask, SIGHUP);
	(void)sigaddset(&defmask, SIGUSR1);
	(void)sigaddset(&defmask, SIGCHLD);

        if (sigprocmask(SIG_BLOCK, &defmask, NULL) < 0) {
                syslog(LOG_ERR, "sigprocmask: %m");
		return -1;        	
	}

	if (!emergency) {
		initconf();
		if (fileconf && openconf(fileconf) < 0) {
			fprintf(stderr, "unable to open %s\n", fileconf);
			return -1;
		} else if (fileconf == NULL)
			(void)openconf("/etc/sud.conf");

		sud_daemonize();
#ifdef DEBUG
		LIST_FOREACH(cfg, &confhead, lentry)
			printconf(cfg);
#endif
	} else if (have_sighup)
		have_sighup = 0; 

	openlog("sud",
#ifdef LOG_PERROR
		nodaemon ? LOG_PID | LOG_PERROR : LOG_PID
#else
		LOG_PID
#endif
		, LOG_INFO); 
			
	(void)signal(SIGCHLD, sud_sigchld);
	(void)signal(SIGUSR1, sud_sighup);

	if (!nservices) {
		syslog(LOG_INFO, "no services found in sud.conf");
		emergency_session();
		/*
		 * not reached
		 */
		return -1;
	}	
		
	if ((pipes = (int *)malloc(nservices * sizeof(int))) == NULL) {
		syslog(LOG_ERR, "malloc(pipes) %m");
		return -1;
	}
		
	LIST_FOREACH(cfg, &confhead, lentry) {
		int pipefd[2];
			
		if (pipe(pipefd) < 0) {
			syslog(LOG_ERR, "creating pipe: %m");
			return -1;
		}	
			
		switch (fork()) {
		case -1:
			syslog(LOG_ERR, "fork() at main: %m");
			/*
			 * close pipe descriptors
			 * this is important if sud_dispatch() was called for
			 * the emergency service
			 */
			(void)close(pipefd[0]);
			(void)close(pipefd[1]);
			return -1;
		case 0:
		{
			int k;

			for (k = 0; k <= lservices; k++)
				(void)close(pipes[k]);
				
			(void)close(lockfd);
			(void)close(pipefd[1]);
			
			(void)signal(SIGHUP,  SIG_DFL);
			(void)signal(SIGUSR1, SIG_DFL);
			(void)sigprocmask(SIG_UNBLOCK, &defmask, NULL);

			if (service_init(cfg) == -1)
				_exit(1);
			if (service_dispatch(cfg, pipefd[0]) == -1) 
				_exit(1);
		}
		default:
			(void)close(pipefd[0]);
			(void)non_blocking(pipefd[1]);
			pipes[lservices++] = pipefd[1]; 
		}
	}

	(void)sigemptyset(&suspmask);

	for (;;) {
		(void)sigsuspend(&suspmask);

		if (have_sigchld) {
			pid_t pid;
			int status;

			have_sigchld = 0;

			while ((pid = waitpid(-1, &status, WNOHANG|WUNTRACED))) 
			{ 
               			if (pid < 0) {
					if (errno == EINTR)
						continue;
					else
						break;
				}
          				
				if (WIFSTOPPED(status)) {
			       		(void)kill(pid, SIGCONT);
					continue;
				}

				nservices--;
			}

			if (nservices <= 0) {
				/*
				 * call free_pipes() in order to close our
				 * descriptors
				 */
				free_pipes(lservices);
				syslog(LOG_INFO, "restarting sud");
				(void)execv(program_fullpath, saved_argv); 
				/*
				 * argh
				 */
				return -1;
			}
		}

		/*
		 * do not try to close descriptors if a sighup was already
		 * caught (very difficult to happen but not impossible)
		 */					
		if (have_sighup == 1) 
			free_pipes(lservices);
	}
}
