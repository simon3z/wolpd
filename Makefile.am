
EXTRA_DIST = \
	wolpd.service \
	wolpd.spec \
	wolpd.sysconfig \
	wolpd.x

SUFFIXES = .x .8
HELP2MAN = help2man

ACLOCAL_AMFLAGS = -I m4

sbin_PROGRAMS = wolpd
wolpd_SOURCES = wolpd.c

dist_man8_MANS = wolpd.8

wolpd.8: wolpd
	$(HELP2MAN) -s 8 -N -I wolpd.x -o $@ $(top_srcdir)/wolpd
