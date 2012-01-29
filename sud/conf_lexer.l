%{
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
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>

#include "conf.h"
#include "conf_parser.h"
#include "extern.h"
#include "queue.h"

extern int nline;

/* 
 * Push and pop parser state: currently only a pair of { } is allowed for a
 * session
 */
static int stack[1];
static int states;

struct conf *curr_cfg;

#define PUSH_STATE()							\
	do {								\
		if (states) {						\
			syslog(LOG_ERR, "too many push");		\
			exit(1);					\
		}							\
		stack[states++] = YY_START;				\
	} while(0)

#define POP_STATE()							\
	do {								\
		if (!states) {						\
			syslog(LOG_ERR, "too many pops");		\
			exit(1);					\
		}							\
		BEGIN(stack[--states]);					\
	} while(0)
%}

%x PARAM DEFAULT DAEMONOPT

%%
<*>\n			nline++;
<*>"\#".*\n         	nline++;

<*>\t+             	;
<*>" "+            	;
<*>\\+             	;
<*>=			return SEP;
<*>yes			return YES;
<*>no			return NO;

<DAEMONOPT>daemonize		return DAEMONIZE;
<DAEMONOPT>emergency		return EMERGENCY;
<DAEMONOPT>emergencytries	return ETRIES;

<PARAM,DEFAULT,DAEMONOPT>pidfile  		return PIDFILE;
<PARAM,DEFAULT,DAEMONOPT>terminal		return TERMINAL;

options		return OPTIONS;
default		{
			if (default_conf == NULL &&
				(default_conf = (struct conf *)calloc(1,
					sizeof(struct conf))) == NULL) {
				syslog(LOG_ERR, "Out of memory on %s", yytext);
				exit(1);
			}	
	
			curr_cfg = default_conf;

			if (curr_cfg->session == NULL &&
				(curr_cfg->session = strdup(yytext)) == NULL) {
				syslog(LOG_ERR, "Out of memory on %s", yytext); 
				exit(1);
			}	

			return DEF; 
		}
[a-zA-Z1-9]*	{
			struct conf *cfp;

			LIST_FOREACH(cfp, &confhead, lentry)
				if (strcmp(cfp->session, yytext) == 0) {
					syslog(LOG_INFO, 
						"%s session is already set",
						yytext);
					sud_daemonize();
					emergency_session();
					/*
					 * not reached
					 */
					exit(1);
				}

			curr_cfg=(struct conf *)calloc(1, sizeof(struct conf));
			if (curr_cfg == NULL) {
				syslog(LOG_ERR, "malloc conf structure %m");
				exit(1);
			}  

			if ((curr_cfg->session = strdup(yytext)) == NULL) {
				syslog(LOG_ERR, "Out of memory on %s", yytext);
                                exit(1);
                        }

			return CFGNAME;
		}

\{		{
			if (curr_cfg == NULL || curr_cfg->session == NULL) {
				PUSH_STATE();
				BEGIN(DAEMONOPT);
			} else {
				if (setconf(curr_cfg) == -1)
					exit(1);

				PUSH_STATE();
				if (curr_cfg == default_conf)
					BEGIN(DEFAULT);
				else
					BEGIN(PARAM);
			}

			return *yytext;
		}

<*>\}	        {
			POP_STATE();
			if (curr_cfg == NULL) 
				return *yytext;
			else if (curr_cfg != default_conf) {
				LIST_INSERT_HEAD(&confhead, curr_cfg, lentry);
				nservices++;
			}

			curr_cfg = NULL;

			return *yytext;
                }

<PARAM,DEFAULT>authgroup	return AUTHGROUP;
<PARAM,DEFAULT>dologin		return UTMP;
<PARAM,DEFAULT>log		return LOG;
<PARAM,DEFAULT>mode		return MODE;
<PARAM,DEFAULT>nclients		return NCLIENTS;
<PARAM,DEFAULT>setegroup	return SETEGROUP;
<PARAM,DEFAULT>seteuser		return SETEUSER;
<PARAM,DEFAULT>setgroup		return SETGROUP;
<PARAM,DEFAULT>setuser		return SETUSER;
<PARAM,DEFAULT>sockfile		return SOCKFILE;
<PARAM,DEFAULT>suipfile		return SUIPFILE;
<PARAM,DEFAULT>timeout		return TIMEOUT;
<PARAM,DEFAULT>uthost		return UTMPHOST;
<PARAM,DEFAULT>utname		return UTMPUSER;

<*>[0-9]+                   	{ yylval.num = atoi(yytext);    return NUM; }
<*>[0-9a-zA-Z\_\.\-\/\$]+       { yylval.strtype = yytext;      return STR; }
<*>\".*\"     			{
                       			char *p = yytext + 1;

					yylval.strtype = p;

					while (*p && *p != '"')
						++p;

					*p = 0;

					return(STRING);
	       			}
%%

#ifndef yywrap
int yywrap(void)
{
  return 1;
}
#endif
