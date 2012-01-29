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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "conf.h"
#include "extern.h"

#define setstr(x, val)	{					\
	if ((x = strdup(val)) == NULL) {			\
		syslog(LOG_ERR, "Out of Memory on %s\n", val); 	\
		return -1;					\
	}							\
} 	

struct confhead confhead;
struct conf *default_conf;

void
initconf(void)
{
	LIST_INIT(&confhead);
	nservices = 0;
}

int
setconf(struct conf *cp)
{
	char buf[MAXPATHLEN];

	/*
	 * our rule is not the default one but default rule exists
	 */
	if (default_conf && cp != default_conf) {
		char *savedname = cp->session;
		*cp = *default_conf;
		cp->session = savedname;

		setstr(cp->suipfile, default_conf->suipfile); 
		setstr(cp->terminal, default_conf->terminal);

		(void)snprintf(buf, sizeof(buf), "/var/run/%s.unix", 
				cp->session);
		setstr(cp->sockfile, buf);

		(void)snprintf(buf, sizeof(buf), "/var/run/%s.pid",
				cp->session);
		setstr(cp->pidfile, buf);

		return 0;
	} else	if (default_conf == NULL) { 
		/*
                 * We don't need to set these fields for default_conf
                 */
		(void)snprintf(buf, sizeof(buf), "/var/run/%s.unix",
                                cp->session);
                setstr(cp->sockfile, buf);

                (void)snprintf(buf, sizeof(buf), "/var/run/%s.pid",
                                cp->session);
                setstr(cp->pidfile, buf);
        }

	/*
	 * default_rule or simple rule with default_rule == NULL
	 */

	if (cp->suipfile == NULL) {
		(void)snprintf(buf, sizeof(buf), "%s/sbin/ilogin", 
			SUD_PATH_PREFIX);
		setstr(cp->suipfile, buf);
	}

	if (cp->terminal == NULL) {
		(void)snprintf(buf, sizeof(buf), "%s", default_term ? \
			default_term : "vt100");
		setstr(cp->terminal, buf);
	}

	cp->authgroup = 0;
	cp->log = 0;
	cp->nclients = 10;
	cp->timeout = 0;
	cp->utmp = 0;
        cp->utname = NULL;
	cp->uthost = NULL;
	cp->haveseteuser = 0;
	cp->havesetuser = 0;
	cp->havesetgroup = 0;
	cp->havesetegroup = 0;
	cp->haveauthgroup = 1;
	cp->mode = 1;
	cp->setuser = 0;
	cp->seteuser = 0;
	cp->setgroup = 0;
	cp->setegroup = 0;

	return 0;
}

void
delconf(struct conf *cp)
{
	if (cp->session)
		free(cp->session);
	if (cp->suipfile)
		free(cp->suipfile);
	if (cp->sockfile)
		free(cp->sockfile);
	if (cp->terminal)
		free(cp->terminal);
	if (cp->pidfile)
		free(cp->pidfile);

	free(cp);
}

int
openconf(char *filename)
{
        extern FILE *yyin;
	
        if ((yyin = fopen(filename, "r")) == NULL)
                return -1;

        (void)yyparse();
	(void)fclose(yyin);

        return 0;
}

#ifdef DEBUG
void
printconf(struct conf *cp)
{
	printf("[%s]\n",   cp->session);
	printf("interactive= %d\n",cp->mode);
	printf("suipfile = %s\n",  cp->suipfile);
	printf("sockfile = %s\n",  cp->sockfile);
	printf("pidfile = %s\n",   cp->pidfile);
	printf("log = %d\n",	   cp->log);
	printf("nclients = %d\n",  cp->nclients);	
	printf("dologin = %d\n",   cp->utmp);
        printf("utname = %s\n",	   cp->utname);
	printf("uthost = %s\n",	   cp->uthost);
	printf("terminal = %s\n",  cp->terminal);
	printf("timeout = %ld\n",  (long)cp->timeout);
	printf("authgroup = %lu (%d)\n", (unsigned long)cp->authgroup,
		cp->haveauthgroup);
	printf("setuser = %lu (%d)\n",   
		(unsigned long)cp->setuser, cp->havesetuser);
	printf("seteuser = %lu (%d)\n",	 
		(unsigned long)cp->seteuser,cp->haveseteuser);	
	printf("setgroup = %lu (%d)\n",  
		(unsigned long)cp->setgroup, cp->havesetgroup);
	printf("setegroup= %lu (%d)\n",  
		(unsigned long)cp->setegroup, cp->havesetegroup);
}
#endif
