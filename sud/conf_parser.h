
/* A Bison parser, made by GNU Bison 2.4.1.  */

/* Skeleton interface for Bison's Yacc-like parsers in C
   
      Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005, 2006
   Free Software Foundation, Inc.
   
   This program is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.
   
   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.
   
   You should have received a copy of the GNU General Public License
   along with this program.  If not, see <http://www.gnu.org/licenses/>.  */

/* As a special exception, you may create a larger work that contains
   part or all of the Bison parser skeleton and distribute that work
   under terms of your choice, so long as that work isn't itself a
   parser generator using the skeleton or a modified version thereof
   as a parser skeleton.  Alternatively, if you modify or redistribute
   the parser skeleton itself, you may (at your option) remove this
   special exception, which will cause the skeleton and the resulting
   Bison output files to be licensed under the GNU General Public
   License without this special exception.
   
   This special exception was added by the Free Software Foundation in
   version 2.2 of Bison.  */


/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     CFGNAME = 258,
     DEF = 259,
     OPTIONS = 260,
     DAEMONIZE = 261,
     EMERGENCY = 262,
     ETRIES = 263,
     AUTHGROUP = 264,
     MODE = 265,
     LOG = 266,
     NCLIENTS = 267,
     PIDFILE = 268,
     SEP = 269,
     SOCKFILE = 270,
     SUIPFILE = 271,
     TERMINAL = 272,
     TIMEOUT = 273,
     SETEGROUP = 274,
     SETEUSER = 275,
     SETGROUP = 276,
     SETUSER = 277,
     UTMP = 278,
     UTMPHOST = 279,
     UTMPUSER = 280,
     YES = 281,
     NO = 282,
     STR = 283,
     STRING = 284,
     NUM = 285
   };
#endif
/* Tokens.  */
#define CFGNAME 258
#define DEF 259
#define OPTIONS 260
#define DAEMONIZE 261
#define EMERGENCY 262
#define ETRIES 263
#define AUTHGROUP 264
#define MODE 265
#define LOG 266
#define NCLIENTS 267
#define PIDFILE 268
#define SEP 269
#define SOCKFILE 270
#define SUIPFILE 271
#define TERMINAL 272
#define TIMEOUT 273
#define SETEGROUP 274
#define SETEUSER 275
#define SETGROUP 276
#define SETUSER 277
#define UTMP 278
#define UTMPHOST 279
#define UTMPUSER 280
#define YES 281
#define NO 282
#define STR 283
#define STRING 284
#define NUM 285




#if ! defined YYSTYPE && ! defined YYSTYPE_IS_DECLARED
typedef union YYSTYPE
{

/* Line 1676 of yacc.c  */
#line 55 "conf_parser.y"

	char 	*strtype;
	int	num;



/* Line 1676 of yacc.c  */
#line 119 "conf_parser.h"
} YYSTYPE;
# define YYSTYPE_IS_TRIVIAL 1
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
#endif

extern YYSTYPE yylval;


