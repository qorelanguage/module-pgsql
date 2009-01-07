AUTOMAKE_OPTIONS = foreign

SUBDIRS = src

noinst_HEADERS = src/pgsql.h \
	src/QorePGConnection.h \
	src/QorePGMapper.h

EXTRA_DIST = COPYING ChangeLog AUTHORS README \
	RELEASE-NOTES \
	test/db-test.q \
	docs/pgsql-module-doc.html \
	qore-pgsql-module.spec

ACLOCAL_AMFLAGS=-I m4

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
