dnl Process this file with autoconf to produce a configure script.

# get rid of that stupid cache mechanism
rm -f config.cache

AC_INIT(keybuf, [0.0.0], [takebi@laafc.net])
AC_PREREQ(2.59)
AC_CONFIG_SRCDIR([configure.ac])
AC_CONFIG_MACRO_DIR([m4])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST

AM_INIT_AUTOMAKE([1.8 foreign])
AM_CONFIG_HEADER([config.h])
m4_ifdef([AM_SILENT_RULES], [AM_SILENT_RULES([yes])])
AC_GNU_SOURCE

AC_PROG_CC
AM_PROG_CC_STDC
AC_HEADER_STDC
AC_C___ATTRIBUTE__

define([AC_LIBTOOL_LANG_CXX_CONFIG], [:])
define([AC_LIBTOOL_LANG_GCJ_CONFIG], [:])
define([AC_LIBTOOL_LANG_F77_CONFIG], [:])
AC_DISABLE_STATIC
AC_PROG_LIBTOOL

AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION(0.14)

PKG_CHECK_MODULES(E, [enlightenment])

release=$(pkg-config --variable=release enlightenment)
MODULE_ARCH="$host_os-$host_cpu-$release"
AC_SUBST(MODULE_ARCH)
AC_DEFINE_UNQUOTED(MODULE_ARCH, "$MODULE_ARCH", "Module architecture")

# Find edje_cc
PKG_CHECK_MODULES(EDJE, [edje])
AC_ARG_WITH(edje-cc,
  AC_HELP_STRING([--with-edje-cc=PATH], [specify a specific path to edje_cc]),
  [
    v=$withval;
    EDJE_CC=$v
  ],[
    EDJE_CC=$(pkg-config --variable=prefix edje)/bin/edje_cc
  ])
AC_SUBST(EDJE_CC)
AC_MSG_CHECKING([Which edje_cc to use])
AC_MSG_RESULT(${EDJE_CC})

datadir=$(pkg-config --variable=modules enlightenment)/${PACKAGE}
AC_ARG_ENABLE(homedir-install,
  AS_HELP_STRING([--enable-homedir-install], [Install module in homedir]),
  [ datadir="${HOME}/.e/e/modules/${PACKAGE}" ]
)

AC_ARG_WITH(theme-image-source-dir,
  AC_HELP_STRING([--with-theme-image-source-dir=PATH],
    [specify a specific path to a theme image dir]),
  [
    v=$withval;
    THEME_IMAGE_SRC_DIR=$v
  ],[
    THEME_IMAGE_SRC_DIR="./images"
  ])
AC_SUBST(THEME_IMAGE_SRC_DIR)
AC_MSG_CHECKING([Where to a theme image source dir])
AC_MSG_RESULT(${THEME_IMAGE_SRC_DIR})

AC_OUTPUT([
Makefile
module.desktop
po/Makefile.in
src/Makefile
data/Makefile
data/themes/Makefile
], [])
