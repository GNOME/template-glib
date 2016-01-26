AC_PREREQ([2.69])


dnl ***********************************************************************
dnl Define Versioning Information
dnl ***********************************************************************
m4_define([tmpl_major_version],[3])
m4_define([tmpl_minor_version],[19])
m4_define([tmpl_micro_version],[4])
m4_define([tmpl_version],[tmpl_major_version.tmpl_minor_version.tmpl_micro_version])
m4_define([tmpl_interface_age],[0])
m4_define([tmpl_bugreport_url],
	  [https://bugzilla.gnome.org/enter_bug.cgi?product=template-glib])
m4_define([tmpl_debug_default],
          [m4_if(m4_eval(tmpl_minor_version % 2), [1], [yes], [minimum])])


dnl ***********************************************************************
dnl Initialize Autoconf
dnl ***********************************************************************
AC_INIT([template-glib],[tmpl_version],[tmpl_bugreport_url])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_SRCDIR([src/main.c])
AC_CONFIG_MACRO_DIR([build-aux])
AC_CONFIG_AUX_DIR([build-aux])
AC_SUBST(ACLOCAL_AMFLAGS, "-I build-aux")
AC_CANONICAL_HOST


dnl ***********************************************************************
dnl Make Version Info Available to .in Files
dnl ***********************************************************************
MAJOR_VERSION=tmpl_major_version
MINOR_VERSION=tmpl_minor_version
MICRO_VERSION=tmpl_micro_version
AC_SUBST(MAJOR_VERSION)
AC_SUBST(MINOR_VERSION)
AC_SUBST(MICRO_VERSION)


dnl ***********************************************************************
dnl Initialize Automake
dnl ***********************************************************************
AM_SILENT_RULES([yes])
AM_INIT_AUTOMAKE([1.11 foreign subdir-objects tar-ustar no-dist-gzip dist-xz])
AM_MAINTAINER_MODE([enable])


dnl ***********************************************************************
dnl Internationalization
dnl ***********************************************************************
GETTEXT_PACKAGE=AC_PACKAGE_TARNAME
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED([GETTEXT_PACKAGE], ["$GETTEXT_PACKAGE"], [GETTEXT package name])
AM_GNU_GETTEXT_VERSION([0.19.2])
AM_GNU_GETTEXT([external])


dnl ***********************************************************************
dnl Check for Required Programs
dnl ***********************************************************************
AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_LEX
AC_PROG_YACC
AM_PROG_VALAC
AC_PATH_PROG([GLIB_GENMARSHAL], [glib-genmarshal])
AC_PATH_PROG([GLIB_MKENUMS], [glib-mkenums])
AC_PATH_PROG([GLIB_COMPILE_RESOURCES], [glib-compile-resources])
PKG_PROG_PKG_CONFIG([0.22])
GOBJECT_INTROSPECTION_CHECK([1.42.0])
GLIB_GSETTINGS
VAPIGEN_CHECK


dnl ***********************************************************************
dnl Enable support for debuggin
dnl ***********************************************************************
AC_ARG_ENABLE(tracing,
              AS_HELP_STRING([--enable-tracing=@<:@no/yes@:>@],
                             [add extra debugging information @<:@default=no@:>@]),
              ,
              enable_tracing=no)
AS_IF([test "x$enable_tracing" = "xyes"],[enable_debug=yes ENABLE_TRACING=1],[ENABLE_TRACING=0])
AC_SUBST(ENABLE_TRACING)
AC_ARG_ENABLE(debug,
              AS_HELP_STRING([--enable-debug=@<:@no/minimum/yes@:>@],
                             [turn on debugging @<:@default=tmpl_debug_default@:>@]),
              ,
              enable_debug=tmpl_debug_default)
AS_CASE(["$enable_debug"],
        [yes],[
            DEBUG_CFLAGS="$DEBUG_CFLAGS -O0"
            DEBUG_CFLAGS="$DEBUG_CFLAGS -g"
        ],
        [minimum],[
            DEBUG_CFLAGS="$DEBUG_CFLAGS -DG_DISABLE_CAST_CHECKS"
        ],
        [no],[
            DEBUG_CFLAGS="$DEBUG_CFLAGS -DG_DISABLE_ASSERT"
            DEBUG_CFLAGS="$DEBUG_CFLAGS -DG_DISABLE_CHECKS"
            DEBUG_CFLAGS="$DEBUG_CFLAGS -DG_DISABLE_CAST_CHECKS"
        ],
        [])
AC_SUBST(DEBUG_CFLAGS)


dnl ***********************************************************************
dnl Check for Required Packages
dnl ***********************************************************************
PKG_CHECK_MODULES(TMPL, [gio-2.0 >= 2.47.4
			 gobject-introspection-1.0 >= 1.47.1])


dnl ***********************************************************************
dnl Initialize Libtool
dnl ***********************************************************************
LT_PREREQ([2.2])
LT_INIT


dnl ***********************************************************************
dnl Additional C Compiler Flags
dnl ***********************************************************************
AX_CHECK_COMPILE_FLAG([-Werror=unknown-warning-option], [
	ax_compiler_flags_test="-Werror=unknown-warning-option"
], [
	ax_compiler_flags_test=""
])
AX_APPEND_COMPILE_FLAGS([ \
	-Wall \
	-Wcast-align \
	-Wdeclaration-after-statement \
	-Wempty-body \
	-Werror=format-security \
	-Werror=format=2 \
	-Wextra \
	-Wformat \
	-Wformat-nonliteral \
	-Wformat-security \
	-Winit-self \
	-Wmisleading-indentation \
	-Wmissing-include-dirs \
	-Wshift-negative-value \
	-Wnested-externs \
	-Wno-missing-field-initializers \
	-Wno-sign-compare \
	-Wno-strict-aliasing \
	-Wno-uninitialized \
	-Wno-unused-parameter \
	-Wpointer-arith \
	-Wredundant-decls \
	-Wreturn-type \
	-Wshadow \
	-Wswitch-default \
	-Wswitch-enum \
	-Wundef \
	-Wuninitialized \
], [], [$ax_compiler_flags_test])
AC_C_CONST


dnl ***********************************************************************
dnl Use --enable-tests to always build tests
dnl ***********************************************************************
AC_ARG_ENABLE([tests],
              AS_HELP_STRING([--enable-tests],
                             [Enable always building tests during 'make all' [default=no]]),
              [enable_tests=$enableval], [enable_tests=no])

AM_CONDITIONAL([ENABLE_TESTS], test "$enable_tests" = "yes")


dnl ***********************************************************************
dnl Support for gtk-doc Documentation Engine
dnl ***********************************************************************
GTK_DOC_CHECK([1.11],[--flavour no-tmpl])
AM_CONDITIONAL(ENABLE_GTK_DOC, test "x$enable_gtk_doc" = "xyes")
AC_ARG_ENABLE(doc-cross-references,
              AS_HELP_STRING([--disable-doc-cross-references],
                             [cross reference symbols from other libraries @<:@default=yes@:>@]),
              enable_doc_cross_references=$enableval,
              enable_doc_cross_references=yes)
AM_CONDITIONAL(ENABLE_DOC_CROSS_REFERENCES, test x$enable_doc_cross_references != xno)


dnl ***********************************************************************
dnl Enable optimizations such as -Bsymbolic
dnl ***********************************************************************
AC_MSG_CHECKING([whether to enable optimized builds])
AC_ARG_ENABLE(optimizations, 
              AC_HELP_STRING([--enable-optimizations],
                             [turn on build-time optimizations [default=yes]]),
              [enable_optimizations=$enableval],
              [AS_IF([test "x$enable_debug" = "xyes"],
		     [enable_optimizations="no"],
		     [enable_optimizations="yes"])])
AC_MSG_RESULT([$enable_optimizations])
OPTIMIZE_CFLAGS=""
OPTIMIZE_LDFLAGS=""
# Enable -Bsymbolic
AS_IF([test "$enable_optimizations" != "no"],
      [AX_CHECK_LINK_FLAG([-Wl,-Bsymbolic],
                          [OPTIMIZE_LDFLAGS="$OPTIMIZE_LDFLAGS -Wl,-Bsymbolic"])
       AX_CHECK_LINK_FLAG([-fno-plt],
                          [OPTIMIZE_LDFLAGS="$OPTIMIZE_LDFLAGS -fno-plt"])])
AC_SUBST(OPTIMIZE_CFLAGS)
AC_SUBST(OPTIMIZE_LDFLAGS)


dnl ***********************************************************************
dnl Check if we should instrument our targets
dnl ***********************************************************************
AC_MSG_CHECKING([for --enable-profiling])
AC_ARG_ENABLE(profiling,
              AC_HELP_STRING([--enable-profiling],
                             [turn on runtime instrumentation [default=no]]),
              [enable_profiling=$enableval],
	      [enable_profiling=no])
AS_IF([test "$enable_profiling" = "yes"],[
	ac_save_cflags="$CFLAGS"
	CFLAGS="$CFLAGS -pg"
	AC_LINK_IFELSE([AC_LANG_SOURCE([[int main() { return 0; }]])],
	               [PROFILING_CFLAGS="-pg"],
		       [enable_profiling=no])
	CFLAGS="$ac_save_cflags $PROFILING_CFLAGS"
])
AC_MSG_RESULT([$enable_profiling])



dnl ***********************************************************************
dnl Enable GObject Introspection bindings
dnl ***********************************************************************
GOBJECT_INTROSPECTION_CHECK([1.42.0])


dnl ***********************************************************************
dnl Process .in Files
dnl ***********************************************************************
AC_CONFIG_FILES([
	Makefile

	data/Makefile
	data/template-glib-1.0.pc

	examples/Makefile

	doc/Makefile
	doc/version.xml

	po/Makefile.in

	src/Makefile
	src/tmpl-debug.h

	tests/Makefile
])
AC_OUTPUT


echo ""
echo " ${PACKAGE} - ${VERSION}"
echo ""
echo " Options"
echo ""
echo "  Prefix ............................... : ${prefix}"
echo "  Library Directory .................... : ${libdir}"
echo "  Optimized Build ...................... : ${enable_optimizations}"
echo ""
echo " Development Options"
echo ""
echo "  Enable Debug ......................... : ${enable_debug}"
echo "  Enable Profiling (-pg)................ : ${enable_profiling}"
echo "  Build Test Suite ..................... : ${enable_tests}"
echo "  Build API reference .................. : ${enable_gtk_doc}"
echo "  Use cross-referencing in API docs .... : ${enable_doc_cross_references}"
echo "  Vala support ......................... : ${enable_vala}"
echo ""

