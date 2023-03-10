#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT([idrivebmr-server], [2.2.11.2193], [martin@urbackup.org])
AC_CONFIG_SRCDIR([AcceptThread.cpp])
AC_CONFIG_HEADER([config.h])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_SYSTEM
AM_INIT_AUTOMAKE([subdir-objects tar-ustar])
AC_ARG_WITH([mail],
     AS_HELP_STRING([--without-mail], [Disables server mailing features. Use this to remove the dependency on libcurl.]))
AC_ARG_WITH([crypto],
     AS_HELP_STRING([--without-crypto], [Disables cryptography functions. Internet mode will not be available. Use this to remove the dependency on libcrypto++.]))
AC_ARG_ENABLE([install_initd],
     AS_HELP_STRING([--enable-install_initd], [Enables installing of supplied init.d file into /etc/init.d]))
AC_ARG_ENABLE([packaging],
     AS_HELP_STRING([--enable-packaging], [Will be installed for packaging.]))
AC_ARG_WITH([mountvhd],
     AS_HELP_STRING([--with-mountvhd], [Enable mounting of VHD files via fuse.]))
AC_ARG_ENABLE([assertions],
     AS_HELP_STRING([--enable-assertions], [Enable assertions (bug finding).]))
AM_CONDITIONAL(WITH_ASSERTIONS, test "x$enable_assertions" = xyes)

AC_ARG_ENABLE([fortify],
     AS_HELP_STRING([--disable-fortify], [Disable GCC/runtime fortification]))
AM_CONDITIONAL(WITH_FORTIFY, test "x$enable_fortify" != xno)

AC_ARG_ENABLE([embedded-cryptopp],
     AS_HELP_STRING([--enable-embedded-cryptopp], [Compile and use Crypto++ 5.6.3 included with the source distribution.]))
AM_CONDITIONAL(EMBEDDED_CRYPTOPP, test "x$enable_embedded_cryptopp" = xyes)
AM_CONDITIONAL(TARGET_CPU_IS_X86, test "x$TARGET_CPU" = xx86)


# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O

AC_CXX_HAVE_SSTREAM
AC_CXX_HAVE_STRSTREAM
AC_CXX_HAVE_LONG_LONG

AX_PTHREAD
if !($HAVE_PTHREAD)
then
	echo "Sorry, your system needs the pthread library."
	echo "Either install it or give up."
	exit 1
fi

AC_LANG([C++])

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([pthread.h arpa/inet.h fcntl.h netdb.h netinet/in.h stdlib.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_C_INLINE
AC_TYPE_SIZE_T
AC_HEADER_TIME
AC_STRUCT_TM

AX_CHECK_COMPILE_FLAG(-fstack-protector-strong --param=ssp-buffer-size=4 -Wformat -Werror=format-security -fPIE,
	[], [AM_CONDITIONAL(WITH_FORTIFY, test xyes = xno)])
	
AX_CHECK_LINK_FLAG([-Wl,-z,relro],
	[], [AM_CONDITIONAL(WITH_FORTIFY, test xyes = xno)])

AX_LIB_SOCKET_NSL
AX_CHECK_ZLIB

AC_MSG_CHECKING([for operating system])
case "$host_os" in
freebsd*)
        opsys="freebsd"
        ;;
linux*)
        opsys="linux"
        ;;
*)
        opsys=""
        ;;
esac
AM_CONDITIONAL([FREEBSD], test x"$opsys" = x"freebsd")
AM_CONDITIONAL([LINUX], test x"$opsys" = x"linux")

# Checks for library functions.
AC_FUNC_SELECT_ARGTYPES
AC_FUNC_STRFTIME
AC_CHECK_FUNCS([gettimeofday memset select socket strstr syncfs fallocate64 utimensat accept4])
AC_CHECK_LIB(dl, dlopen, [DLOPEN_LIBS="-ldl"])
AC_SUBST([DLOPEN_LIBS])

P_SUBDIRS="snapshot_helper fsimageplugin httpserver idrivebmrserver"

AM_CONDITIONAL(WITH_URLPLUGIN, test "x$with_mail" != "xno")
AM_CONDITIONAL(WITH_CRYPTOPLUGIN, test "x$with_crypto" != "xno")
AM_CONDITIONAL(WITH_FUSEPLUGIN, test "x$with_mountvhd" = "xyes")
AM_CONDITIONAL(PACKAGING, test "x$enable_packaging" = xyes)
AM_CONDITIONAL(INSTALL_INITD, test "x$enable_install_initd" = xyes)

if test "x$with_mail" != "xno"
then
	LIBCURL_CHECK_CONFIG(,[7.20.0],,[
       AC_MSG_ERROR([Your system lacks of libcurl >= 7.20.0])
     ])
fi

if test "x$with_crypto" != "xno" && test "x$enable_embedded_cryptopp" != "xyes"
then
	MULE_CHECK_CRYPTOPP([5.1], [], [echo "Crypto++ not found. Please install (cryptopp-devel/libcrypto++-dev) or run configure with --enable-embedded-cryptopp."; exit 44;])
fi

WITH_FUSEPLUGIN_CXXFLAGS=""
if test "x$with_mountvhd" = "xyes"
then
	PKG_CHECK_MODULES([FUSE], [fuse >= 2.8.0], [
			  AC_SUBST([FUSE_CFLAGS])
              AC_SUBST([FUSE_LIBS])
	])
	WITH_FUSEPLUGIN_CXXFLAGS="-DWITH_FUSEPLUGIN"
fi
AC_SUBST([WITH_FUSEPLUGIN_CXXFLAGS])

AC_SEARCH_LIBS([clock_gettime], [rt posix4],
	[test "$ac_cv_search_clock_gettime" = "none required" || LIBS="$LIBS $ac_cv_search_clock_gettime"],
	[AC_MSG_FAILURE([No library for clock_gettime found])] )

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
