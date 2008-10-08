if COND_SINGLE_COMPILATION_UNIT
PGSQL_SOURCES = single-compilation-unit.cc
else
PGSQL_SOURCES = pgsql.cc QorePGConnection.cc QorePGMapper.cc
endif

lib_LTLIBRARIES = pgsql.la
pgsql_la_SOURCES = $(PGSQL_SOURCES)
pgsql_la_LDFLAGS = -module -avoid-version ${PGSQL_LIBS}

INCLUDES = -I$(top_srcdir)/include $(PGSQL_INCLUDES)

AM_CPPFLAGS=$(QORE_CPPFLAGS)

if COND_PROFILE
AM_CXXFLAGS=-g -pg
else
if COND_DEBUG
AM_CXXFLAGS=-g
endif
endif

install-exec-hook:
	mv $(DESTDIR)$(libdir)/pgsql.$(MODULE_SUFFIX) $(DESTDIR)$(libdir)/pgsql.qmod
	rm -f $(DESTDIR)$(libdir)/pgsql.la


