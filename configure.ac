# Process this file with autoconf to produce a configure script.

# AC_PREREQ(2.59)
AC_INIT([qore-pgsql-module], [3.2.0],
        [David Nichols <david@qore.org>],
        [qore-pgsql-module])
AM_INIT_AUTOMAKE([no-dist-gzip dist-bzip2 tar-ustar])
AC_CONFIG_HEADER([config.h])

AC_LANG(C++)
AC_PROG_CXX
AC_PROG_CC
AC_DISABLE_STATIC
AC_LIBTOOL_DLOPEN
AM_PROG_LIBTOOL
AC_PROG_INSTALL

AC_C_BIGENDIAN

# fix for building with older autotools
if test ! -d m4; then mkdir m4; fi

AC_CONFIG_MACRO_DIR([m4])

# set system variables
OS=`uname -s`
if test "$OS" = "HP-UX"; then
   if test "$host_cpu" = "ia64"; then
      ARCH=itanium
   else
      ARCH=PA-RISC
   fi
else
   ARCH=`uname -p`
   if test $ARCH = unknown; then
      ARCH=`uname -m`
   fi
   if test $ARCH = i686 -o $ARCH = i586 -o $ARCH = athlon; then
      ARCH=i386
   fi
fi

# set find prefix
if test -n "$prefix" -a "$prefix" != "NONE"; then
   find_prefix="$prefix"
fi

#AC_CANONICAL_HOST
# get CPU family
case "${host_cpu}" in
   i[[3456]]86) cpu_family=i386 ;;
   amd64)       cpu_family=x86_64 ;;
   *)           cpu_family=${host_cpu} ;;
esac

# set OS-specific variables
SHLIB_SUFFIX=so
#echo host_os=${host_os}sss
case "${host_os}" in
     *linux*)   # set compile flags for Linux
		CPPFLAGS="${CPPFLAGS} -D_GNU_SOURCE"
		SHLIB_SUFFIX=so
		AC_DEFINE(LINUX, 1, if compiling on Linux)
		;;
     *solaris*) # for checking for some network functions on Solaris
		LDFLAGS="$LDFLAGS -lnsl"
		# assume we are using CC if not using g++, add -norunpath to link flags in this case
		if test "$GXX" != "yes"; then
		   CXXFLAGS="${CXXFLAGS} -mt"
		   LDFLAGS="$LDFLAGS -norunpath -mt"
		fi
		AC_DEFINE(SOLARIS, 1, if compiling on Solaris)
		# add -ldl to libs if on Solaris 8
		if test "${host_os}" = "solaris2.8"; then
		   OTHER_LIBS="$OTHER_LIBS -ldl"
		fi
		solaris=yes
	        ;;
     *darwin*)  SHLIB_SUFFIX=dylib
		# libtool 1.5.* is creating modules on Darwin 8.* (OS/X 10.4.*) with a .so suffix for some reason
		MODULE_SUFFIX=so
		AC_DEFINE(DARWIN, 1, [if compiling on Darwin])
		# use gnutar on darwin to avoid getting those pesky resource files
		AMTAR=gnutar
		darwin=true
		;;
     *hpux*)    if test "$host_cpu" = "ia64"; then
     		   SHLIB_SUFFIX=so
		else  # assume hppa (PA-RISC)
     		   SHLIB_SUFFIX=sl
		   AC_DEFINE(SHLIB_SUFFIX, "sl", shared library suffix)
		fi
		# add "-AA" and -D__STDC_EXT__ to aCC commandline to enable a clean compile
		if test -n "`echo $CXX|grep aCC`"; then
		   CXXFLAGS="$CXXFLAGS -AA -D__STDC_EXT__ -D_RWSTD_MULTI_THREAD"
		fi
		AC_DEFINE(HPUX, 1, if compiling on HP-UX)
		;;
     *cygwin*)  SHLIB_SUFFIX=dll
     		CPPFLAGS=-mwin32
		windows=yes
     		;;
     *mingw32*) SHLIB_SUFFIX=dll
		windows=yes
		mingw=yes
		#CPPFLAGS="$CPPFLAGS -D_WIN32_WINNT=0x0501"
		LDFLAGS="$LDFLAGS -lwsock32 -lws2_32 -lgdi32 -lz -lsecur32"
		#make_dll=yes
		CPPFLAGS="$CPPFLAGS -DBUILDING_DLL=1"
		MODULE_LDFLAGS="-no-undefined"
		OS=Windows
		# set LIB_PREFIX if not already set
		if test -z "$LIB_PREFIX"; then
   		   AC_MSG_CHECKING([for mingw32 library prefix])
		   cc_loc=`which $CXX|head -1`
		   mingw_base=`dirname $cc_loc|sed 's/\/@<:@^/@:>@*$//'`
		   cc_targ=`echo $CXX|sed s/-g++$//`
		   if test -d $mingw_base/$cc_targ; then
		      LIB_PREFIX=$mingw_base/$cc_targ
		      AC_MSG_RESULT([$LIB_PREFIX])
		   else
		      AC_MSG_RESULT([not found])
		   fi
		fi
		;;
     *mingw64*) SHLIB_SUFFIX=dll
		windows=yes
		mingw=yes
		#CPPFLAGS="$CPPFLAGS -D_WIN32_WINNT=0x0501"
		LDFLAGS="$LDFLAGS -lwsock32 -lws2_32 -lgdi32 -lz -lsecur32"
		#make_dll=yes
		CPPFLAGS="$CPPFLAGS -DBUILDING_DLL=1"
		MODULE_LDFLAGS="-no-undefined"
		OS=Windows
		# set LIB_PREFIX if not already set
		if test -z "$LIB_PREFIX"; then
   		   AC_MSG_CHECKING([for mingw64 library prefix])
		   cc_loc=`which $CXX|head -1`
		   mingw_base=`dirname $cc_loc|sed 's/\/@<:@^/@:>@*$//'`
		   cc_targ=`echo $CXX|sed s/-g++$//`
		   if test -d $mingw_base/$cc_targ; then
		      LIB_PREFIX=$mingw_base/$cc_targ
		      AC_MSG_RESULT([$LIB_PREFIX])
		   else
		      AC_MSG_RESULT([not found])
		   fi
		fi
		;;
     *)         AC_WARN(Unknown platform ${host_os})
                ;;
esac
AC_SUBST(SHLIB_SUFFIX)
if test -z "$MODULE_SUFFIX" ; then
   MODULE_SUFFIX=$SHLIB_SUFFIX
fi
AC_SUBST(MODULE_SUFFIX)

AC_ARG_ENABLE([64bit],
  [AS_HELP_STRING([--enable-64bit],
		  [enable 64bit support (default: auto)])],
  [case "${enable_64bit}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_64bit} for --enable-64bit) ;;
      esac],
  [enable_64bit=auto])

if test "$enable_64bit" = "auto"; then
   # turn on 64-bit by default if compiling on a 64-bit platform
   case "${host_cpu}" in
       *64*)
                 enable_64bit=yes
                 ;;
       s390x)
                 enable_64bit=yes
                 ;;
       *)        case "${host_os}" in
                     darwin*)   mv=`uname -r|cut -f1 -d.`
 		                if test $mv -ge 10 -a "${host_cpu}" = "i386"; then
		         	    enable_64bit=yes
		      		fi
		     ;;
		     solaris*)  if test "${host_cpu}" = "i386"; then
		     	            # if we are running on a 64-bit machine, then enable a 64-bit build
				    if test -n "`isainfo -v 2>/dev/null| grep 64-bit`"; then
		         	        enable_64bit=yes
		     		    fi
		     	        fi
		     ;;
       		 esac
		 if test "$enable_64bit" = "auto"; then
		     enable_64bit=no
		 fi
                 ;;
    esac
fi

# allow users to explicitly set libsuffix
AC_ARG_WITH([libsuffix],
  [AS_HELP_STRING([--with-libsuffix@<:@=LIBSUFFIX@:>@],
                  [explitily set libsuffix])],
  [LIBSUFFIX=$with_libsuffix; LIBSUFFIX_SET=yes;AC_SUBST(LIBSUFFIX)],
  [])

# see if we know how to set flags properly for different hosts and architectures
# FIXME: add test on HP-UX if the compiler can generate the appropriate binaries
if test "$enable_64bit" = "yes"; then
   case "${host_os}" in
      *linux*)	if test "$GXX" = "yes" -a "$ARCH" != "aarch64"; then
                    CXXFLAGS="$CXXFLAGS -m64"
                fi
                # multiarch
                if test "$LIBSUFFIX_SET" != "yes"; then
                    if test -d /usr/lib64; then
                        LIBSUFFIX=64
                    fi
                    if test "$GXX" = "yes"; then
                        multiarch=`${CXX} --print-multiarch 2> /dev/null`
                        if test -n "${multiarch}" -a -d /usr/lib/${multiarch}; then
                            LIBSUFFIX=/${multiarch}
                        fi
                    fi
                    AC_SUBST(LIBSUFFIX)
                fi
                ;;
      *hpux*)	if test "$host_cpu" = "ia64"; then
                    if test -n "`echo $CXX|grep aCC`"; then
                        CXXFLAGS="$CXXFLAGS +DD64"
                    elif test "$GXX" = "yes"; then
                        CXXFLAGS="$CXXFLAGS -mlp64"
                    fi
                else
                    if test -n "`echo $CXX|grep aCC`"; then
                        CXXFLAGS="$CXXFLAGS +DA2.0W"
                    elif test "$GXX" = "yes"; then
                        CXXFLAGS="$CXXFLAGS -march=2.0"
    	    	    fi
                fi
                ;;
     *solaris*) if test "$GXX" = "yes"; then
		    if test "$host_cpu" = "sparc"; then
		        CXXFLAGS="$CXXFLAGS -mcpu=ultrasparc3 -m64"  # NOTE we compile for minimum ultrasparc 3 in 64-bit mode
		    else
		        CXXFLAGS="$CXXFLAGS -m64"
		    fi
		else
		    CC_MAJOR=`$CXX -V 2>&1 | head -1| cut -f1 -d.|cut -d\  -f4`
		    CC_MINOR=`$CXX -V 2>&1 | head -1| cut -f2 -d.|cut -d\  -f1`
		    if test "$CC_MAJOR" -gt 5 -o \( "$CC_MAJOR" -eq 5 -a "$CC_MINOR" -ge 10 \); then
		        CXXFLAGS="$CXXFLAGS -m64"
		    else
			CXXFLAGS="$CXXFLAGS -xarch=generic64"
		    fi
		fi
		;;
     *darwin*)	if test "$GXX" = "yes"; then
		   CXXFLAGS="$CXXFLAGS -m64"
		fi
		# set ARCH to x86_64 to be consistent with other intel 64-bit builds
		if test "$host_cpu" = "i386"; then
		   ARCH=x86_64
		fi
		;;
   esac
   SYBLIBSUFFIX=64
   RVLIBSUFFIX=64
   bits=64
else
   case "${host_os}" in
     *linux*)	if test "$GXX" = "yes"; then
                    case "$host_cpu" in
                        arm*) ;;
                        *) CXXFLAGS="$CXXFLAGS -m32" ;;
                    esac
                fi
                # multiarch
                if test "$LIBSUFFIX_SET" != "yes"; then
                    if test "$host_cpu" = "x86_64" -a -d /usr/lib32; then
                        LIBSUFFIX=32
                    fi
                    if test "$GXX" = "yes"; then
                        multiarch=`${CXX} --print-multiarch 2> /dev/null`
                        if test -n "${multiarch}" -a -d /usr/lib/${multiarch}; then
                            LIBSUFFIX=/${multiarch}
                        fi
                    fi
                    AC_SUBST(LIBSUFFIX)
                fi
                ;;
     *darwin*)  if test "$GXX" = "yes"; then
                    case "$host_cpu" in
                        arm*) ;;
                        *) CXXFLAGS="$CXXFLAGS -m32" ;;
                    esac
                fi
                ;;
     *hpux*)	if test "$host_cpu" != "ia64"; then
		    if test "$GXX" = "yes"; then
		        CXXFLAGS="$CXXFLAGS -march=2.0"
		    elif test -n "`echo $CXX|grep aCC`" -a "$host_cpu" != "ia64"; then
		        CXXFLAGS="$CXXFLAGS +DA2.0N"   # NOTE we compile for PA-RISC 2.0 32-bit, not PA-RISC 1.1
		    fi
		fi
		;;
     *solaris*) if test "$GXX" = "yes"; then
		    CXXFLAGS="$CXXFLAGS -m32"
		else
		    CXXFLAGS="$CXXFLAGS -xarch=generic"
		fi
		;;
   esac
   if test "$host_cpu" = "x86_64"; then
      cpu_family=i386
   fi
   bits=32
fi

# test for C++11
AX_CXX_COMPILE_STDCXX_11([noext], [mandatory])
if test "$HAVE_CXX11" = "1"; then
    AM_CXXFLAGS=-std=gnu++11
fi

# checks for how to build threads
ACX_PTHREAD
if test "$acx_pthread_ok" = "no"; then
   AC_MSG_ERROR(POSIX threads do not seem to be supported on this platform, aborting)
fi
CXXFLAGS="$CXXFLAGS $PTHREAD_CFLAGS"
#LDFLAGS="$LDFLAGS $PTHREAD_LIBS"

# allow the user to set a generic search prefix for libraries, includes, etc
AC_ARG_WITH([lib-prefix],
  [AS_HELP_STRING([--with-lib-prefix@<:@=DIR@:>@],
                  [generic search prefix for libraries, includes, etc])],
  [if test ! -d "${with_lib_prefix}"; then AC_MSG_ERROR([bad value ${with_lib_prefix} for --with-lib-prefix]); unset with_lib_prefix; fi],
  [with_lib_prefix="$LIB_PREFIX"])

AC_ARG_ENABLE([single-compilation-unit],
  [AS_HELP_STRING([--enable-single-compilation-unit],
                  [enable compilation as a single unit (default: on)])],
  [case "${enable_single_compilation_unit}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_single_compilation_unit} for --enable-single-compilation-unit) ;;
      esac],
  [enable_single_compilation_unit=yes])

AC_ARG_WITH([doxygen],
    [AS_HELP_STRING([--with-doxygen@<:@=PATH@:>@],
                    [path to doxygen binary])],
    [if test "${with_doxygen}" != "no" -a ! -x "${with_doxygen}"; then AC_MSG_ERROR(${with_doxygen} is not an executable in --with-doxygen); unset with_doxygen; fi ],
    [with_doxygen=auto])

if test "${with_doxygen}" = "auto"; then
   AC_MSG_CHECKING([for doxygen to build API documentation])
   # check for doxygen
   with_doxygen=`which doxygen 2>/dev/null | grep -v "no doxygen in.*"`
   if test -n "$with_doxygen"; then
      doxy_version=`$with_doxygen --version`
      AC_MSG_RESULT([found $with_doxygen $doxy_version])
   else
      AC_MSG_RESULT([not found])
   fi
fi

if test -n "${with_doxygen}"; then
   AC_SUBST(DOXYGEN_CMD, $with_doxygen)
fi

AC_ARG_WITH([pgsql],
    [AS_HELP_STRING([--with-pgsql@<:@=DIR@:>@],
                    [PostgreSQL base directory])],
    [if test ! -d "${with_pgsql}"; then AC_MSG_ERROR(directory ${with_pgsql} does not exist for --with-pgsql); unset with_pgsql; fi])

check_pgsql() {
    a="$1"
    if test \( "$a" = "/" -o -z "$a" \) -a -h "$a/lib${LIBSUFFIX}"; then
        a=/usr
    fi

    inc="$a/include"
    if test "$a" = "/"; then
       inc=/usr/include
    fi
    pdir=`ls $inc/postgres*/libpq-fe.h 2>/dev/null | tail -1`
    if test -n "$pdir"; then
       inc=`dirname $pdir`
    fi
    #echo DBG: checking $a inc=$inc alt_inc=$alt_inc
    if test -f "$inc/libpq-fe.h"; then
       with_pgsql_includes=$inc
    elif test -n "$alt_inc" -a -f "$alt_inc/libpq-fe.h"; then
       with_pgsql_includes=$alt_inc
    elif test -f "$inc/postgresql/libpq-fe.h"; then
       with_pgsql_includes=$inc/postgresql
    elif test -f "$inc/pgsql/libpq-fe.h"; then
       with_pgsql_includes=$inc/pgsql
    fi
    if test -z "$with_pgsql_includes"; then
       #echo DBG: pgsql $a $inc not found
       return
    else
        # check for server include files
        if test -f "$with_pgsql_includes/utils/nabstime.h"; then
            with_pgsql_server_includes="$with_pgsql_includes"
        elif test -d "$with_pgsql_includes/server"; then
	        with_pgsql_server_includes="$with_pgsql_includes/server"
        elif test -d "$with_pgsql_includes/pgsql/server"; then
	        with_pgsql_server_includes="$with_pgsql_includes/pgsql/server"
        elif test -d "$with_pgsql_includes/postgresql/server"; then
	        with_pgsql_server_includes="$with_pgsql_includes/postgresql/server"
       fi
    fi

    lib="$a/lib"

    pdir=`ls -d $lib/postgres* 2>/dev/null |tail -1`
    if test -n "$pdir"; then
       alt_lib=$pdir
    fi

    #echo DGB lib: $lib alt_lib: $alt_lib LIBSUFFIX: ${LIBSUFFIX} SHLIB_SUFFIX: ${SHLIB_SUFFIX}

    if test "$mingw" = yes -a -f "$lib/libpq.a"; then
       with_pgsql_libs=$lib
       pgsql_static=1
    elif test -f "$lib/libpq.${SHLIB_SUFFIX}" -o -f "$lib/libpq.so" -o -f "$lib/libpq.${SHLIB_SUFFIX}.a"; then
       with_pgsql_libs=$lib
    elif test -f "$lib${LIBSUFFIX}/libpq.${SHLIB_SUFFIX}" -o -f "$lib${LIBSUFFIX}/libpq.so"; then
       with_pgsql_libs=$lib${LIBSUFFIX}
    elif test -f "$lib/postgresql/libpq.${SHLIB_SUFFIX}"; then
       with_pgsql_libs=$lib/postgresql
    elif test -f "$lib${LIBSUFFIX}/postgresql/libpq.${SHLIB_SUFFIX}"; then
       with_pgsql_libs=$lib${LIBSUFFIX}/postgresql
    elif test -f "$lib/pgsql/libpq.${SHLIB_SUFFIX}"; then
       with_pgsql_libs=$lib/pgsql
    elif test -f "$lib${LIBSUFFIX}/pgsql/libpq.${SHLIB_SUFFIX}"; then
       with_pgsql_libs=$lib${LIBSUFFIX}/pgsql
    elif test -f "$lib/libpq.a"; then
       with_pgsql_libs=$lib
       pgsql_static=1
    elif test -f "$lib${LIBSUFFIX}/libpq.a"; then
       with_pgsql_libs=$lib${LIBSUFFIX}
       pgsql_static=1
    elif test -f "$lib/postgresql/libpq.a"; then
       with_pgsql_libs=$lib/postgresql
       pgsql_static=1
    elif test -f "$lib${LIBSUFFIX}/postgresql/libpq.a"; then
       with_pgsql_libs=$lib${LIBSUFFIX}/postgresql
       pgsql_static=1
    elif test -f "$lib/pgsql/libpq.a"; then
       with_pgsql_libs=$lib/pgsql
       pgsql_static=1
    elif test -f "$lib${LIBSUFFIX}/pgsql/libpq.a"; then
       with_pgsql_libs=$lib${LIBSUFFIX}/pgsql
       pgsql_static=1
    elif test -n "$alt_lib"; then
       if test -f "$alt_lib/libpq.${SHLIB_SUFFIX}" -o -f "$alt_lib/libpq.so"; then
          with_pgsql_libs=$alt_lib
       elif test -f "$alt_lib/postgresql/libpq.${SHLIB_SUFFIX}"; then
          with_pgsql_libs=$alt_lib/postgresql
       elif test -f "$alt_lib/pgsql/libpq.${SHLIB_SUFFIX}"; then
          with_pgsql_libs=$alt_lib/pgsql
       elif test -f "$alt_lib/libpq.a"; then
          with_pgsql_libs=$alt_lib
          pgsql_static=1
       elif test -f "$alt_lib/postgresql/libpq.a"; then
          with_pgsql_libs=$alt_lib/postgresql
          pgsql_static=1
       elif test -f "$alt_lib/pgsql/libpq.a"; then
          with_pgsql_libs=$alt_lib/pgsql
          pgsql_static=1
       fi
    fi

    if test -z "$with_pgsql_libs"; then
       #echo DBG pgsql $a libs not found
       unset with_pgsql_includes
       unset with_pgsql_server_includes
       return
    fi
    if test "$with_pgsql_includes" != "/usr/include"; then
       pgsql_app_includes=true
    fi
    if test "$with_pgsql_libs" != "/lib${LIBSUFFIX}"; then
       pgsql_app_libs=true
    fi

    # check postgresql version
    if test -n "$with_pgsql_server_includes"; then
       pg_config=`ls $with_pgsql_server_includes/pg_config*.h 2>/dev/null`
    else
       pg_config=`ls $with_pgsql_includes/pg_config*.h 2>/dev/null`
    fi
    if test -z "$pg_config"; then
       AC_MSG_ERROR([can't determine PostgreSQL version])
    fi
    version=`grep "PG_VERSION " $pg_config|cut -f2 -d\"`
    version_major=`echo $version|cut -f1 -d.`
    if test -z "$version_major"; then
       AC_MSG_ERROR([can't determine PostgreSQL version])
    fi
    if test $version_major -lt 8; then
       AC_MSG_ERROR([PostgreSQL version $version is not supported (must be >= 8)])
    fi
    AC_DEFINE_UNQUOTED([POSTGRES_VERSION_MAJOR], $version_major, [postgresql major version number])
}

# see if we can figure out where the pgsql include files and libraries are
if test "${with_pgsql}" != "no"; then
   AC_MSG_CHECKING([for PostgreSQL includes and libraries])

    if test -n "$with_pgsql"; then
        check_pgsql "$with_pgsql"
        if test -z "$with_pgsql_includes"; then
            AC_MSG_ERROR([not found in $with_pgsql])
        fi
    else
        for dir in $PGSQL_DIR "${with_lib_prefix}" "${find_prefix}" / /usr /usr/local /opt/pgsql /usr/pgsql /sw /opt/local /opt/sfw /usr/sfw /opt/gnu; do
            if test -z "$dir"; then
                continue;
            fi
            check_pgsql $dir
            if test -n "$with_pgsql_includes"; then
                break;
            fi
        done
    fi

    if test -n "$with_pgsql_includes"; then
        if test -n "$pgsql_static"; then
            AC_MSG_RESULT([includes=$with_pgsql_includes, libs=$with_pgsql_libs (static)])
        else
            AC_MSG_RESULT([includes=$with_pgsql_includes, libs=$with_pgsql_libs (shared)])
        fi
        with_pgsql=yes
    else
        AC_MSG_ERROR([not found])
    fi
else
   AC_MSG_ERROR([cannot build module without PostgreSQL support])
fi

if test "${with_pgsql}" = yes; then
    if test -n "$with_pgsql_includes"; then
        # only set include path if necessary
        if test -n "$pgsql_app_includes"; then
            # do not use server includes since it will break the build now
            #   if test -n "$with_pgsql_server_includes"; then
            #      PGSQL_INCLUDES="-I$with_pgsql_includes -I$with_pgsql_server_includes"
            #      CPPFLAGS="$CPPFLAGS -DPOSTGRESQL_SERVER_INCLUDES=1"
            #	else
                PGSQL_INCLUDES="-I$with_pgsql_includes"
            #	fi
            #elif test -n "$with_pgsql_server_includes"; then
            #   PGSQL_INCLUDES="-I$with_pgsql_server_includes"
            #   CPPFLAGS="$CPPFLAGS -DPOSTGRESQL_SERVER_INCLUDES=1"
        fi

        if test -n "$pgsql_static"; then
            PGSQL_LIBS="$with_pgsql_libs/libpq.a"
            if test "$mingw" = "yes"; then
                PGSQL_LIBS="$PGSQL_LIBS -lssl -lcrypto -lintl -lwldap32"
            fi
        else
            if test -n "$pgsql_app_libs"; then
                PGSQL_LIBS="-L$with_pgsql_libs -lpq"
            else
                PGSQL_LIBS="-lpq"
            fi
        fi
        # check if C++ compiler can link with PGSQL libs
        SAVE_CXXFLAGS="$CXXFLAGS"
        SAVE_LDFLAGS="$LDFLAGS"
        CXXFLAGS="$CXXFLAGS $PGSQL_INCLUDES"
        LDFLAGS="$LDFLAGS $PGSQL_LIBS"
        AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <libpq-fe.h>
	 ]],[[
PQconnectdb(NULL);
	]])], pgsql_links=yes, pgsql_links=no)

     CXXFLAGS="$SAVE_CXXFLAGS"
     LDFLAGS="$SAVE_LDFLAGS"

     if test "$pgsql_links" = "no"; then
        AC_MSG_ERROR([could not compile a test PostgreSQL program])
     else
        AC_SUBST(PGSQL_INCLUDES)
	AC_SUBST(PGSQL_LIBS)
        with_pgsql=yes
     fi

     # check if the client library has PQlibVersion
     AC_MSG_CHECKING([for PQlibVersion])
     SAVE_CXXFLAGS="$CXXFLAGS"
     SAVE_LDFLAGS="$LDFLAGS"
     CXXFLAGS="$CXXFLAGS $PGSQL_INCLUDES"
     LDFLAGS="$LDFLAGS $PGSQL_LIBS"
     AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <libpq-fe.h>
	 ]],[[
int ver = PQlibVersion();
	]])], pgsql_hascver=yes, pgsql_hascver=no)

     CXXFLAGS="$SAVE_CXXFLAGS"
     LDFLAGS="$SAVE_LDFLAGS"

     if test "$pgsql_hascver" = "yes"; then
        AC_MSG_RESULT([yes])
        AC_DEFINE(HAVE_PQLIBVERSION, 1, define if the postgresql client lib supports PQlibVersion)
     else
        AC_MSG_RESULT([no])
     fi
  else
     AC_MSG_ERROR([no PostgreSQL libraries or includes found])
  fi
fi

set_qore_cppflags() {
    QORE_INC_DIR=$1
    if test "$1" != "/usr/include"; then
        QORE_CPPFLAGS=-I$1
    fi
}

find_qore() {
    a="$1"
    if test \( "$a" = "/" -o -z "$a" \) -a -h "/lib" -a -e "/usr/lib${LIBSUFFIX}"; then
        a=/usr
    fi
    lib=$a/lib${LIBSUFFIX}

    # if we only want the static qore libraries
    if test -n "$qore_static"; then
        if test -f "$lib/libqore.a"; then
            QORE_LDFLAGS="$lib/libqore.a"
            AC_MSG_RESULT([$lib (forced static)])
        fi
    else
        if test -f "$lib/libqore.${SHLIB_SUFFIX}"; then
            if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                QORE_LDFLAGS="-L$lib"
            fi
            QORE_LDFLAGS="$QORE_LDFLAGS -lqore"
            AC_MSG_RESULT([$lib (shared)])
        elif test "$mingw" = yes -a -f "$lib/libqore.${SHLIB_SUFFIX}.a"; then
            if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                QORE_LDFLAGS="-L$lib"
            fi
            QORE_LDFLAGS="$QORE_LDFLAGS -lqore"
            AC_MSG_RESULT([$lib (shared)])
        elif test -f "$lib/libqore.a"; then
            if test "$lib" != "/lib" -a "$lib" != "/usr/lib"; then
                QORE_LDFLAGS="-L$lib"
            fi
            QORE_LDFLAGS="$QORE_LDFLAGS -lqore"
            AC_MSG_RESULT([$lib (static)])
        fi
    fi
    if test -z "$QORE_LDFLAGS"; then
        return
    fi

    AC_MSG_CHECKING([for qore include files])
    # try to find include files
    if test "$a" = "/"; then
        inc=/usr/include
    else
        inc=$a/include
    fi
    if test -f "$inc/qore/Qore.h"; then
        AC_MSG_RESULT([found: $inc])
        set_qore_cppflags $inc
    else
        AC_MSG_ERROR([not found])
    fi

    AC_SUBST(QORE_LDFLAGS)
    AC_SUBST(QORE_CPPFLAGS)
}

# see if we can figure out where the qore library is
AC_ARG_WITH([qore-dir],
  [AS_HELP_STRING([--with-qore-dir@<:@=DIR@:>@],
                  [Qore directory])],
  [if test ! -d "${with_qore_dir}"; then AC_MSG_ERROR(bad value ${with_qore_dir} for --with-qore-dir); unset with_qore_dir; fi],
  [with_qore_dir="$QORE_DIR"])

AC_MSG_CHECKING([for qore libraries and header files])
for dir in "${with_qore_dir}" "${with_lib_prefix}" "${find_prefix}" /usr / /usr/local /opt/gnu /opt/qore /usr/local/qore /opt/local /sw /usr/sfw /opt/sfw; do
    find_qore $dir
    if test -n "$QORE_LDFLAGS"; then
        break
    fi
done
if test -z "$QORE_LDFLAGS"; then
    AC_MSG_ERROR([no qore library found])
else
   # test qore version
   AC_MSG_CHECKING([qore library version])

   # get qore version code
   qvc=`grep QORE_VERSION_CODE $QORE_INC_DIR/qore/qore-version.h|cut -f3 -d\  2>/dev/null`

   # get qore version string
   qore_version=`grep QORE_VERSION.\" $QORE_INC_DIR/qore/qore-version.h|cut -f3 -d\ |sed s/\"//g 2>/dev/null`

   # if version code not present, may be qore 0.7.0 or earlier
   if test -z "$qvc"; then
      qv_major=`grep QORE_VERSION_MAJOR $QORE_INC_DIR/qore/qore-version.h|cut -f3 -d\  2>/dev/null`
      if test -z "$qv_major"; then
         AC_MSG_ERROR([cannot determine; check Qore header files in $QORE_INC_DIR])
      fi

      qv_minor=`grep QORE_VERSION_MINOR $QORE_INC_DIR/qore/qore-version.h|cut -f3 -d\  2>/dev/null`
      qv_sub=`grep QORE_VERSION_SUB $QORE_INC_DIR/qore/qore-version.h|cut -f3 -d\  2>/dev/null`

      if test "$qv_major" -gt 0; then
         qvc=`printf "%d%02d%02d" $qv_major $qv_minor $qv_sub`
      else
         qvc=`printf "%d%02d" $qv_major $qv_minor $qv_sub`
      fi
      AC_DEFINE_UNQUOTED([QORE_VERSION_CODE], $qvc, [define when missing in Qore headers])
   fi

   if test $qvc -lt 900; then
      AC_MSG_ERROR([found $qore_version; this module requires at least version 0.9])
   fi

   AC_MSG_RESULT([${qore_version} OK])

   MODULE_API=`qore --latest-module-api`
   mapi_major=`qore --latest-module-api|cut -f1 -d.`
   mapi_minor=`qore --latest-module-api|cut -f2 -d.`

   AC_SUBST(MODULE_API)

   if test -z "${prefix}" -o "${prefix}" = "NONE"; then
      libdir=`qore --module-dir`
      userlibdir=`qore --user-module-dir 2>/dev/null`
      if test -z "${userlibdir}"; then
         userlibdir="${libdir}"
      fi
      prefix=`which qore|sed s/.bin.qore//`
   else
      libdir="$prefix/lib${LIBSUFFIX}/qore-modules"
      qlibdir=`qore --module-dir`
      if test "$libdir" != "$qlibdir"; then
         AC_MSG_WARN([warning, installed qore's module dir is $qlibdir, this module will be installed in $libdir])
      fi
      if test -z "${datarootdir}"; then
         datarootdir="$prefix/share";
         AC_SUBST([datarootdir])
      fi
      if test -n "echo ${datarootdir}|grep \$"; then
         userlibdir="`eval echo ${datarootdir}`/qore-modules"
      else
         userlibdir="${datarootdir}/qore-modules"
      fi
      qlibdir=`qore --user-module-dir`
      if test "$userlibdir" != "$qlibdir"; then
         AC_MSG_WARN([warning, installed qore's user module dir is $qlibdir, this module will be installed in $userlibdir])
      fi
   fi
fi
AC_SUBST([userlibdir])
usermoddir="`echo ${userlibdir}|sed s/-modules.*//`"
AC_SUBST([usermoddir])

AC_ARG_ENABLE([profile],
     [AS_HELP_STRING([--enable-profile],
		     [turn on profiling support (default=no)])],
     [case "${enable_profile}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_profile} for --enable-profile) ;;
      esac],
     [enable_profile=no])

AC_ARG_ENABLE([debug],
     [AS_HELP_STRING([--enable-debug],
		     [turn on debugging (default=no)])],
     [case "${enable_debug}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_debug} for --enable-debug) ;;
      esac],
     [enable_debug=yes])

if test "${enable_debug}" = yes; then
   AC_DEFINE(DEBUG, 1, Define if debugging support should be included)
   # remove -O2 from CXXFLAGS so valgrind can report properly
   CXXFLAGS=`echo $CXXFLAGS | sed 's/\-O2//'`
else
   AC_DEFINE(NDEBUG, 1, Define if assert() declarations should be suppressed)
fi

if test "${enable_profile}" = yes; then
   AC_DEFINE(PROFILE, 1, Define if profiling support should be included)
fi

AC_ARG_ENABLE([optimization],
     [AS_HELP_STRING([--enable-optimization],
		     [turn on optimization (default=auto (yes unless debugging is enabled)])],
     [case "${enable_optimization}" in
       yes|no) ;;
       *)      AC_MSG_ERROR(bad value ${enable_optimization} for --enable-optimization) ;;
      esac],
     [enable_optimization=auto])

if test "$enable_optimization" = "auto"; then
   if test "${enable_debug}" = yes; then
      enable_optimization=no
   else
      enable_optimization=yes
   fi
fi

# check for gcc visibility support
AC_MSG_CHECKING([for gcc visibility support])
if test "$GXX" = "yes"; then
   gcc_version=`$CXX -dumpversion`
   if test "$gcc_version" \> "4"; then
      AC_DEFINE(HAVE_GCC_VISIBILITY, 1, if compiling with g++ and visibility support is available)
      AC_MSG_RESULT([yes, $CXX $gcc_version])
      # we no longer set visibility to hidden by default because it makes RTTI symbols invisible
      # all functions in the library are tagged anyway
      #CXXFLAGS="$CXXFLAGS -fvisibility=hidden"
   else
      AC_MSG_RESULT([no, gcc $gcc_version])
   fi
else
   AC_MSG_RESULT([no, $CXX])
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_CHECK_MEMBERS([struct stat.st_blksize])
AC_STRUCT_ST_BLOCKS
AC_CHECK_MEMBERS([struct stat.st_rdev])
AC_HEADER_TIME
AC_STRUCT_TM
AC_C_VOLATILE

AC_CHECK_HEADERS([arpa/inet.h])
AC_CHECK_HEADERS([netinet/in.h])

# turn on optimizations if we know how
if test "$enable_optimization" = "yes"; then
    if test "$GXX" = "yes"; then
        CXXFLAGS="$CXXFLAGS -O3"  # gcc
    elif test "$CXX" = "CC" -a "`echo $host_os|cut -b-7`" = "solaris"; then
        # remove -g with Solaris CC as it reduces optimization levels
	CXXFLAGS="`echo $CXXFLAGS| sed s/-g//` -xO5" # Solaris CC
    elif test -n "`echo $CXX|grep aCC`"; then
        # remove -g with HP-UX aCC as it reduces optimization levels
	CXXFLAGS="`echo $CXXFLAGS| sed s/-g//` +O4" # HP-UX aCC
    fi
else
    if test "$GXX" = "yes"; then
        CXXFLAGS="`echo $CXXFLAGS| sed s/-O2//`"  # gcc
    fi
fi

# turn on some build flags for non-debugging, non-profiling builds
#if test "$enable_debug" = "no" -a "$enable_profile" = "no" -a "$enable_optimization" = "yes" ; then
    # we can no longer strip binaries and libraries with non-debugging, non-profiling builds on Darwin with -Wl,-x because it breaks linking :(
#fi

# turn on warnings if we know how
if test "$GXX" = "yes"; then
   CXXFLAGS="$CXXFLAGS -Wall"  # gcc
else
   if test "$solaris" = yes; then
      # turn on all warnings for the oracle sun studio compiler
      CXXFLAGS="$CXXFLAGS +w"
   fi
fi

AC_SUBST(OTHER_LIBS)

# if compiling for mingw32, we have to add qore ldflags to the module's ldflags
if test "$mingw" = yes; then
    MODULE_LDFLAGS="$MODULE_LDFLAGS $QORE_LDFLAGS"
    AC_SUBST(MODULE_LDFLAGS)
fi

# save more version information in config.h
MODULE_VERSION_MAJOR=`echo $PACKAGE_VERSION | cut -f3 -d\  | sed s/\"//g | cut -f1 -d.`
MODULE_VERSION_MINOR=`echo $PACKAGE_VERSION | cut -f3 -d\  | sed s/\"//g | cut -f2 -d.`
MODULE_VERSION_SUB=`echo $PACKAGE_VERSION | cut -f3 -d\  | sed s/\"//g | cut -f3 -d.`
QORE_MODULE_PACKAGE_VERSION="$PACKAGE_VERSION"

# set version information
AC_DEFINE_UNQUOTED([MODULE_VERSION_MAJOR], $MODULE_VERSION_MAJOR, [major version number])
AC_DEFINE_UNQUOTED([MODULE_VERSION_MINOR], $MODULE_VERSION_MINOR, [minor version number])
AC_DEFINE_UNQUOTED([MODULE_VERSION_SUB], $MODULE_VERSION_SUB, [sub version number])
AC_DEFINE_UNQUOTED([MODULE_TARGET_ARCH], "$ARCH", [host type])
AC_DEFINE_UNQUOTED([MODULE_TARGET_OS], "$OS", [host type])
AC_DEFINE_UNQUOTED([MODULE_TARGET_BITS], $bits, [32 or 64 bit build])
AC_DEFINE_UNQUOTED([QORE_MODULE_PACKAGE_VERSION], "$QORE_MODULE_PACKAGE_VERSION", [qore module package version])

AM_CONDITIONAL([COND_DOXYGEN],    [test -n "$with_doxygen"])
AM_CONDITIONAL([COND_DEBUG],      [test "$enable_debug" = yes])
AM_CONDITIONAL([COND_PROFILE],    [test "$enable_profile" = yes])
AM_CONDITIONAL([COND_SINGLE_COMPILATION_UNIT], [test "$enable_single_compilation_unit" = yes])
AM_CONDITIONAL([COND_MINGW],      [test "$mingw" = yes])
AC_CONFIG_FILES([Makefile src/Makefile])
AC_OUTPUT

if test -n "$with_doxygen"; then enable_doxygen=yes; fi

echo "*** MODULE FEATURES ***"
printf "%-24s: %s %s" module "$OS" "$cpu_family"
if test "$enable_64bit" = "yes"; then
   printf " (64-bit)\n"
else
   printf " (32-bit)\n"
fi

show_library_feature() {
   printf "%-24s: " "$1"
   if test "$2" = "yes"; then
      printf "%-8s" yes
   else
      printf disabled
   fi
   if test -n "$3"; then
      printf " (%s)" $3
   fi
   echo
}

show_library_feature optimizations $enable_optimization
show_library_feature debug $enable_debug
show_library_feature profiling $enable_profile
show_library_feature doxygen $enable_doxygen

# hack to use our libtool if on mingw to enable static libs to be linked in to the module
if test "$mingw" = yes; then
   # backup generated libtool
   if test ! -e libtool.orig ; then cp libtool libtool.orig; fi
   sed -e 's/deplibs_check_method=.*/deplibs_check_method=pass_all/g' -e 's/allow_undefined_flag=.*/allow_undefined_flag=yes/g' libtool > libtool.tmp
   mv libtool.tmp libtool
fi
