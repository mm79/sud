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
#include <stdlib.h>
#include <unistd.h>
#include <syslog.h>

#include "session.h"


void
s_head_init(struct shead *sh)
{
	LIST_INIT(sh);
} 

struct sctl *
s_alloc(void)
{
	struct sctl *sp;

	sp = (struct sctl *)calloc(1, sizeof(struct sctl));

	return sp;
}

void
s_dealloc(struct sctl *sc)
{
	if (sc)
		free(sc);
}

void
s_ins(struct shead *sh, struct sctl *sc)
{
	LIST_INSERT_HEAD(sh, sc, s_lentry);
}

int
s_rm_with_pid(struct shead *sh, pid_t pid)
{
	struct sctl *sp;

	LIST_FOREACH(sp, sh, s_lentry)
		if (sp->s_pid == pid) {
			(void)close(sp->s_pipedesc);
			LIST_REMOVE(sp, s_lentry);
			s_dealloc(sp);
			return 1;
		} 	

	return 0;
}
