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

#include <sys/types.h>
#include <sys/socket.h>

#include <stdio.h>
#include <signal.h>
#include <string.h>


#ifndef PIPE_BUF
#define PIPE_BUF 512
#endif

#ifndef SUN_LEN
#define SUN_LEN(su) \
        (sizeof(*(su)) - sizeof((su)->sun_path) + strlen((su)->sun_path))
#endif

#define SOCK_NAME "/var/run/sud.unix"

/* modes */
#define INTERACTIVE	1
#define	READWRITE	2
#define READ		3
#define BLIND		4

extern char *sud_pidfile;
extern char *sud_emergency, *default_term;
extern char *fileconf;
extern struct conf *curr_cfg;
extern int emergency_maxtries;
extern int nservices;
extern int nodaemon;

/*
 * prototypes for these functions are not defined in linux
 */
#if defined(__linux__) && HAVE_SETRESUID
int setresuid(uid_t, uid_t, uid_t);
#endif
#if defined(__linux__) && HAVE_SETRESGID
int setresgid(gid_t, gid_t, gid_t);
#endif

int     yyparse(void);
int     yylex(void);

/* conf.c */
struct conf ;
int 	setconf(struct conf *);
void	delconf(struct conf *);
void	initconf(void);
int     openconf(char *);
#ifdef DEBUG
void    printconf(struct conf *);
#endif

/* control.c */
struct schck ; 

ssize_t control_create(int *, int);
int     control_check(int, const struct schck *, int);

/* interactive.c */
void exec_shell(struct conf *, int, int);

/* main.c */
int     main(int, char **);
void	emergency_session(void);
void	sud_daemonize(void);

/* noninteractive.c */
void exec_command(struct conf *, int, int);

/* popen.c */
int sud_popen(const char *, int *, int);

/* request.c */
void sud_process(struct conf *, int, int);

/* session.c */
struct sctl;
struct shead;
struct sctl *s_alloc(void);
void s_ins(struct shead *, struct sctl *);
void s_dealloc(struct sctl *);
void s_head_init(struct shead *);
int  s_rm_with_pid(struct shead *, pid_t);

/* service.c */
int service_init(struct conf *);
int service_dispatch(struct conf *, int);

/* utility.c */
#if defined(HAVE_GETPEEREID) || defined(SO_PEERCRED)
int is_group(uid_t, gid_t);
#endif
#ifndef HAVE_DAEMON
int daemon(int, int);
#endif
long getDtabsize(void);
int non_blocking(int);
int write_pidfile(char *, int);
void cleanup(char *);
struct group *get_group(gid_t);
struct passwd *get_pwentry(uid_t);
void set_allgroups(struct conf *, int);
void set_privileges(struct conf *);
