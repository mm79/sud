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
#include <sys/types.h>
#include <sys/stat.h>

#include <grp.h>
#include <pwd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <unistd.h>

#include "conf.h"
#include "extern.h"


int nline = 1;

int	yyerror(char *);
%}

%union {
	char 	*strtype;
	int	num;
}

%token	CFGNAME DEF OPTIONS
%token	DAEMONIZE EMERGENCY ETRIES
%token  AUTHGROUP MODE LOG NCLIENTS PIDFILE SEP SOCKFILE SUIPFILE
	TERMINAL TIMEOUT
%token 	SETEGROUP SETEUSER SETGROUP SETUSER 
%token	UTMP UTMPHOST UTMPUSER
%token	YES NO
%token	<strtype>	STR STRING
%token	<num> 		NUM  

%type	<num>	val
%%
	cfgs	:
		| cfgs statement
		;

	statement:
		CFGNAME       	'{' exps '}'
		| DEF		'{' exps '}'
		| OPTIONS	'{' options '}'
		;

	options :
		| options option
		;

        option:
                DAEMONIZE SEP val
                {
                        /*
                         * if nodaemon is already set do not consider this 
			 * option in the configuration file
                         */
                        if (nodaemon == 0)
                                nodaemon = !$3;
                }
                | PIDFILE SEP STR
                {
                        if (sud_pidfile == NULL &&
                                ((sud_pidfile = strdup($3)) == NULL)) {
                                syslog(LOG_ERR,"out of memory on dup pidfile");
                                exit(1);
                        }
                }
		| EMERGENCY SEP STR
		{
			if (sud_emergency == NULL &&
				((sud_emergency = strdup($3)) == NULL)) {
				syslog(LOG_ERR, "out of memory on dup emerg");
				exit(1);
			}
		}
		| EMERGENCY SEP STRING
		{
                        if (sud_emergency == NULL &&
                                ((sud_emergency = strdup($3)) == NULL)) {
                                syslog(LOG_ERR, "out of memory on dup emerg");
                                exit(1);
                        }
                }
          	| ETRIES SEP NUM
		{
			emergency_maxtries = $3;
		}			
                ;

	exps	:
		| exps exp
		;

	exp	:
		SUIPFILE SEP STR 
		{
			struct stat st;

			if (lstat($3, &st) == -1) { 
				syslog(LOG_ERR, 
					"sorry unable to stat: %s file\n", $3);	
				yyerror("stat problem");
			}
						
			if (curr_cfg->suipfile)
				free(curr_cfg->suipfile);

			if ((curr_cfg->suipfile = strdup($3)) == NULL) {
				syslog(LOG_ERR, "out of memory on %s", $3);
				exit(1);
			}	
		}
		| SUIPFILE SEP STRING	
		{
			if (curr_cfg->suipfile)
				free(curr_cfg->suipfile);

			if ((curr_cfg->suipfile = strdup($3)) == NULL) { 
                                syslog(LOG_ERR, "out of memory on %s", $3);
                                exit(1);
			}
		}	
		| SOCKFILE SEP STR	
		{
			if (curr_cfg->sockfile)
				free(curr_cfg->sockfile);

			if ((curr_cfg->sockfile = strdup($3)) == NULL) {
				syslog(LOG_ERR, "out of memory on %s", $3);
				exit(1);
			}
		}
		| MODE SEP STR 
		{
			if (strcasecmp($3, "readwrite") == 0) {
				curr_cfg->mode = READWRITE;
				break;
			}

			if (strcasecmp($3, "write") == 0 ||
			    strcasecmp($3, "blind") == 0) {
				curr_cfg->mode = BLIND;
				break;
			}

			if (strcasecmp($3, "read") == 0 ||
			    strcasecmp($3, "command") == 0) {
				curr_cfg->mode = READ;
				break;
			}

			if (strcasecmp($3, "interactive") == 0) {
				curr_cfg->mode = INTERACTIVE;
				break;
			}

			(void)yyerror("unknown mode");
		}
                | PIDFILE SEP STR	
		{
			if (curr_cfg->pidfile)
				free(curr_cfg->pidfile);
                                               
			if ((curr_cfg->pidfile = strdup($3)) == NULL) {
                               	syslog(LOG_ERR, "out of memory on %s", $3);
				exit(1);
                        }
                }
		| TERMINAL SEP STR
		{
			if (curr_cfg->terminal)
				free(curr_cfg->terminal);

			if ((curr_cfg->terminal = strdup($3)) == NULL) {
				syslog(LOG_ERR, "out of memory on %s", $3);
				exit(1);
			}
		}
		| AUTHGROUP SEP STR		
		{
			struct group *grp;

			/*
			 * XXX: if you have noauth group, it will be ignored
			 */
			if (strcmp($3, "noauth") == 0) {
				curr_cfg->haveauthgroup = 0;
				break;
			}

			if ((grp = getgrnam($3)) == NULL) {
				syslog(LOG_ERR, "unable to find %s group", $3);
				yyerror("group problem");
			}

			curr_cfg->authgroup = grp->gr_gid;
			curr_cfg->haveauthgroup = 1;
		}	
		| AUTHGROUP SEP NUM	
		{
			struct group *grp;

			if ((grp = getgrgid((gid_t)$3)) == NULL) {
				syslog(LOG_ERR, "unable to find %lu group", 
					(unsigned long)$3);
				yyerror("group problem");
			}
					
			curr_cfg->authgroup = grp->gr_gid;
			curr_cfg->haveauthgroup = 1;
		}
                | SETEGROUP SEP STR      
		{
                	struct group *grp;

#if defined(HAVE_GETPEEREID) || defined(SO_PEERCRED)
			if (strcmp($3, "$mygid") == 0) {
                        	curr_cfg->havesetegroup = USEMYID;
                        	break;
                        }
#endif

                       	if ((grp = getgrnam($3)) == NULL) {
                        	syslog(LOG_ERR, "unable to find %s group", $3);
                                yyerror("group problem");
                        }

                        curr_cfg->havesetegroup = USEID;
                        curr_cfg->setegroup = grp->gr_gid;
                }
		| SETEGROUP SEP NUM
	      	{
                	struct group *grp;

                        if ((grp = getgrgid((gid_t)$3)) == NULL) {
                       		syslog(LOG_ERR, "unable to find %lu group", 
					(unsigned long)$3);
                                yyerror("group problem");
                        }

                        curr_cfg->havesetegroup = USEID;
                        curr_cfg->setegroup = grp->gr_gid;
                }
                | SETGROUP SEP STR      
		{
               		struct group *grp;

#if defined(HAVE_GETPEEREID) || defined(SO_PEERCRED)
                       	if (strcmp($3, "$mygid") == 0) {
                        	curr_cfg->havesetgroup = USEMYID;
                                break;
                        }
#endif

                        if ((grp = getgrnam($3)) == NULL) {
                        	syslog(LOG_ERR, "unable to find %s group", $3);
                                yyerror("group problem");
                        }

			curr_cfg->havesetgroup = USEID;
                        curr_cfg->setgroup = grp->gr_gid;
		}
		| SETGROUP SEP NUM	
		{
                	struct group *grp;

                       	if ((grp = getgrgid((gid_t)$3)) == NULL) {
                       		syslog(LOG_ERR, "unable to find %lu group", 
					(unsigned long)$3);
         			yyerror("group problem");
                        }

			curr_cfg->havesetgroup = USEID;
                        curr_cfg->setgroup = grp->gr_gid;
		}
		| NCLIENTS SEP NUM	
		{
			curr_cfg->nclients = $3;	
		}
		| TIMEOUT SEP NUM	
		{
			curr_cfg->timeout = (time_t)$3;
		}
		| SETEUSER SEP NUM	
		{
			struct passwd *pw;

                        if ((pw = getpwuid((uid_t)$3)) == NULL) {
               	        	syslog(LOG_ERR, "unable to find %lu uid", 
					(unsigned long)$3);
				yyerror("user problem");
			}

			curr_cfg->haveseteuser = USEID;
			curr_cfg->seteuser = pw->pw_uid;
		}
                | SETEUSER SEP STR       
		{
			struct passwd *pw;
                     
#if defined(HAVE_GETPEEREID) || defined(SO_PEERCRED)
			if (strcmp($3, "$myuid") == 0) {
				curr_cfg->haveseteuser = USEMYID;
				break;
			}
#endif
			if ((pw = getpwnam($3)) == NULL) {
				syslog(LOG_ERR, "unable to find %s user", $3);
				yyerror("user problem");
			}

			curr_cfg->haveseteuser = USEID;
			curr_cfg->seteuser = pw->pw_uid;
                                        
		}
		| SETUSER SEP NUM	
		{
			struct passwd *pw;
			
			if ((pw = getpwuid((uid_t)$3)) == NULL) {
				syslog(LOG_ERR, "unable to find %lu uid", 
					(unsigned long)$3);
				yyerror("user problem");
			}

			curr_cfg->havesetuser = USEID;
			curr_cfg->setuser = pw->pw_uid;
		} 
		| SETUSER SEP STR	
		{
			struct passwd *pw;

#if defined(HAVE_GETPEEREID) || defined(SO_PEERCRED)
			if (strcmp($3, "$myuid") == 0) {
				curr_cfg->havesetuser = USEMYID;
				break;
			}
#endif
			if ((pw = getpwnam($3)) == NULL) {
				syslog(LOG_ERR, "unable to find %s user", $3);
				yyerror("user problem");
			}

			curr_cfg->havesetuser = USEID;
			curr_cfg->setuser = pw->pw_uid;
		}
                | UTMPUSER SEP STR      
		{
			if (curr_cfg->utname)
				free(curr_cfg->utname);

			if ((curr_cfg->utname = strdup($3)) == NULL) {
				syslog(LOG_ERR, "out of memory on %s", $3);
				exit(1);
			}
		}
		| UTMPHOST SEP STR	
		{
			if (curr_cfg->uthost)
				free(curr_cfg->uthost);

			if ((curr_cfg->uthost = strdup($3)) == NULL) {
				syslog(LOG_ERR, "out of memory on %s", $3);
				exit(1);
			}
		}
		| UTMPHOST SEP STRING	
		{
			if (curr_cfg->uthost)
				free(curr_cfg->uthost);

			if ((curr_cfg->uthost = strdup($3)) == NULL) {
				syslog(LOG_ERR, "out of memory on %s", $3);
				exit(1);
			}
		}
		| LOG SEP val
		{
			curr_cfg->log = $3;
		} 
		| UTMP SEP val		
		{
			curr_cfg->utmp = $3;
		}
		;
        
	val	:
		YES	
		{
			$$ = 1;
		}
		|	
		NO	
		{
			$$ = 0;
		}
		; 

%%

int 
yyerror(char *s)
{
	extern FILE *yyin;
	
	if (yyin)
		(void)fclose(yyin);
	
	syslog(LOG_ERR, "%s in line %d", s, nline);

	sud_daemonize();
	emergency_session();

	/*
	 * never reached
	 */
	return 1;
}
