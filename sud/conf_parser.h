#ifndef BISON_Y_TAB_H
# define BISON_Y_TAB_H

#ifndef YYSTYPE
typedef union {
	char 	*strtype;
	int	num;
} yystype;
# define YYSTYPE yystype
# define YYSTYPE_IS_TRIVIAL 1
#endif
# define	CFGNAME	257
# define	DEF	258
# define	OPTIONS	259
# define	DAEMONIZE	260
# define	EMERGENCY	261
# define	ETRIES	262
# define	AUTHGROUP	263
# define	MODE	264
# define	LOG	265
# define	NCLIENTS	266
# define	PIDFILE	267
# define	SEP	268
# define	SOCKFILE	269
# define	SUIPFILE	270
# define	TERMINAL	271
# define	TIMEOUT	272
# define	SETEGROUP	273
# define	SETEUSER	274
# define	SETGROUP	275
# define	SETUSER	276
# define	UTMP	277
# define	UTMPHOST	278
# define	UTMPUSER	279
# define	YES	280
# define	NO	281
# define	STR	282
# define	STRING	283
# define	NUM	284


extern YYSTYPE yylval;

#endif /* not BISON_Y_TAB_H */
