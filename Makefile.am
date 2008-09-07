AUTOMAKE_OPTIONS = foreign

SUBDIRS = src

noinst_HEADERS = src/pgsql.h \
	src/QorePGConnection.h \
	src/QorePGMapper.h

EXTRA_DIST = COPYING ChangeLog AUTHORS README \
	test/db-test.q \
	pgsql-module-doc.html \
	qore-pgsql-module.spec

libtool: $(LIBTOOL_DEPS)
	$(SHELL) ./config.status --recheck
