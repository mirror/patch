/* utility functions for `patch' */

/* $Id: util.h,v 1.6 1997/04/07 01:07:00 eggert Exp $ */

void ask PARAMS ((char const *, ...)) __attribute__ ((format (printf, 1, 2)));
void say PARAMS ((char const *, ...)) __attribute__ ((format (printf, 1, 2)));

void fatal PARAMS ((char const *, ...))
	__attribute__ ((noreturn, format (printf, 1, 2)));
void pfatal PARAMS ((char const *, ...))
	__attribute__ ((noreturn, format (printf, 1, 2)));

char *fetchname PARAMS ((char *, int));
char *savebuf PARAMS ((char const *, size_t));
char *savestr PARAMS ((char const *));
void Fseek PARAMS ((FILE *, long, int));
void copy_file PARAMS ((char const *, char const *));
void exit_with_signal PARAMS ((int)) __attribute__ ((noreturn));
void ignore_signals PARAMS ((void));
void makedirs PARAMS ((char *));
void memory_fatal PARAMS ((void));
void move_file PARAMS ((char const *, char const *, int));
void read_fatal PARAMS ((void));
void remove_prefix PARAMS ((char *, size_t));
void set_signals PARAMS ((int));
void write_fatal PARAMS ((void));
