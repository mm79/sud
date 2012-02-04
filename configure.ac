AC_INIT(sud, 1.3, [matteo@dancingbear.it])
AC_CONFIG_AUX_DIR(config)
AC_CONFIG_SRCDIR(sud/main.c)
AM_INIT_AUTOMAKE(sud, 1.3)
AC_CONFIG_HEADER(autoconf.h)
AC_PREFIX_DEFAULT(/usr)

AM_PROG_CC_C_O
AC_PROG_INSTALL
AC_GNU_SOURCE
AC_PROG_YACC
AM_PROG_LEX

AC_SEARCH_LIBS(forkpty, util lutil)
AC_SEARCH_LIBS(crypt, crypt lcrypt)
AC_CHECK_HEADERS(pty.h util.h libutil.h)
AC_CHECK_HEADERS([lastlog.h utmp.h utmpx.h sys/param.h])
AC_CHECK_HEADERS([termios.h sys/termios.h])
AC_TYPE_SIGNAL
AC_TYPE_UID_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T


AC_DEFINE_UNQUOTED(SUD_PATH_PREFIX, "$ac_default_prefix", "sud prefix installation")

AC_CHECK_TYPE(socklen_t,
   	      [AC_DEFINE(HAVE_SOCKLEN_T, 1, [Define if you have socklen_t.])],
	      [], [#include <sys/socket.h>])

AC_CHECK_FUNCS(daemon forkpty)
AC_CHECK_FUNCS(getpeereid strlcpy)
AC_CHECK_FUNCS(setlogin setproctitle setresuid setresgid)

AC_CONFIG_FILES([Makefile su/Makefile sud/Makefile login/Makefile])
AC_OUTPUT
