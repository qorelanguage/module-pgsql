MNAME=pgsql

if COND_MINGW
QPP=qpp_native
else
QPP=qpp
endif

.qpp.cpp:
	$(QPP) -V $<

GENERATED_SRC = ql_pgsql.cpp
CLEANFILES = $(GENERATED_SRC)

if COND_SINGLE_COMPILATION_UNIT
single-compilation-unit.cpp: $(GENERATED_SRC)
PGSQL_SOURCES = single-compilation-unit.cpp
else
PGSQL_SOURCES = pgsql.cpp QorePGConnection.cpp QorePGMapper.cpp
nodist_pgsql_la_SOURCES = $(GENERATED_SRC)
endif

lib_LTLIBRARIES = pgsql.la
pgsql_la_SOURCES = $(PGSQL_SOURCES)
pgsql_la_LDFLAGS = -module -avoid-version ${PGSQL_LIBS} ${MODULE_LDFLAGS}

INCLUDES = -I$(top_srcdir)/include $(PGSQL_INCLUDES)

AM_CPPFLAGS=$(QORE_CPPFLAGS)

if COND_PROFILE
AM_CXXFLAGS=-g -pg
else
if COND_DEBUG
AM_CXXFLAGS=-g
endif
endif

if COND_MINGW
RM_OTHER=$(DESTDIR)$(libdir)/$(MNAME).dll.a
endif

install-exec-hook:
	mv $(DESTDIR)$(libdir)/$(MNAME).$(MODULE_SUFFIX) $(DESTDIR)$(libdir)/$(MNAME)-api-$(MODULE_API).qmod
	rm -f $(DESTDIR)$(libdir)/$(MNAME).la $(DESTDIR)$(libdir)/$(MNAME).a $(RM_OTHER)
