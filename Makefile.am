
MAINTAINERCLEANFILES	= $(dist_man8_MANS) $(DIST_ARCHIVES) \
			  aclocal.m4 configure depcomp ltmain.sh \
			  config.guess config.sub missing install-sh \
			  compile config.h.in config.h.in~ \
			  Makefile.in

EXTRA_DIST		= wolpd.x wolpd.spec wolpd.sysconfig

ACLOCAL_AMFLAGS		= -I m4
AM_CPPFLAGS		= -Wall -Werror -Wshadow -O2 # -Wconversion

sbin_PROGRAMS		= wolpd
dist_man8_MANS		= wolpd.8

wolpd_SOURCES		= wolpd.c

SUFFIXES		= .x .8
HELP2MAN		= help2man

.x.8:
	@if test ! -f $*; then \
	  $(MAKE) $(AM_MAKEFLAGS) $*; \
	else :; fi
	$(HELP2MAN) -i $< -o $@ $(srcdir)/$*

