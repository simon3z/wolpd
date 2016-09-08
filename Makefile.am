
EXTRA_DIST		= wolpd.x wolpd.spec wolpd.sysconfig

SUFFIXES		= .x .8
HELP2MAN		= help2man

ACLOCAL_AMFLAGS		= -I m4

sbin_PROGRAMS		= wolpd
wolpd_SOURCES		= wolpd.c
bin_SCRIPTS		= wolpd.init.centos wolpd.init.sles

dist_man8_MANS		= wolpd.8

wolpd.8:
	$(HELP2MAN) -s 8 -N -I wolpd.x -o $@ $(top_srcdir)/wolpd

.PHONY: rpmbuild

rpm_builddir = $(abs_builddir)/rpmbuild
RPMBUILD = rpmbuild --define '_topdir $(rpm_builddir)'

rpmbuild: dist-gzip
	$(MKDIR_P) $(rpm_builddir)
	$(RPMBUILD) -ta $(distdir).tar.gz

