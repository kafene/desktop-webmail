AC_PREREQ(2.59)
AC_INIT(desktop-webmail, 002, asac@canonical.com)
AC_CONFIG_SRCDIR([src/desktop-webmail.c])
AC_CONFIG_HEADER([config.h])
AM_INIT_AUTOMAKE
AC_LIBTOOL_DLOPEN
AC_PROG_LIBTOOL
IT_PROG_INTLTOOL

ALL_LINGUAS="de nl hu"
AC_SUBST([ALL_LINGUAS])

GETTEXT_PACKAGE=desktop-webmail
AC_SUBST(GETTEXT_PACKAGE)
AC_DEFINE_UNQUOTED(GETTEXT_PACKAGE, "$GETTEXT_PACKAGE",[description])
AC_DEFINE_UNQUOTED(LOCALEDIR, "${prefix}/${DATADIRNAME}/locale", [Name of gettext locale directory])

AC_CONFIG_MACRO_DIR([m4])

# Checks for programs.
AC_PROG_CXX
AC_PROG_CC
AM_PROG_CC_C_O

# Checks for libraries.

PKG_CHECK_MODULES(UI, gtk+-2.0)

# Checks for header files.

# Checks for typedefs, structures, and compiler characteristics.

# Checks for library functions.
AC_CONFIG_FILES([Makefile data/Makefile src/Makefile po/Makefile.in])
AC_OUTPUT

