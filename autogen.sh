#!/bin/sh
#
# Copyright (c) 2002-2010 Xfce Development Team
#

(type xdt-autogen) >/dev/null 2>&1 || {
  cat >&2 <<EOF
autogen.sh: You don't seem to have the Xfce development tools installed on
            your system, which are required to build this software.
            Please install the xfce4-dev-tools package first, it is available
            from http://www.xfce.org/.
EOF
  exit 1
}

# portability for awk
awk_tests="gawk mawk nawk awk"
if test -z "$AWK"; then
  for a in $awk_tests; do
    if type $a >/dev/null 2>&1; then
      AWK=$a
      break
    fi
  done
else
  if ! type $AWK >/dev/null 2>/dev/null; then
    unset AWK
  fi
fi
if test -z "$AWK"; then
  echo "autogen.sh: The 'awk' program (one of $awk_tests) is" >&2
  echo "            required, but cannot be found." >&2
  exit 1
fi

# substitute revision
if test -d .git/svn; then
    revision=`git svn find-rev trunk 2>/dev/null ||
              git svn find-rev origin/trunk 2>/dev/null ||
              git svn find-rev HEAD 2>/dev/null ||
              git svn find-rev master 2>/dev/null`
elif test -d .git; then
    revision=`git rev-parse --short HEAD`
elif test -d .svn; then
    revision=`LC_ALL=C svn info $0 | $AWK '/^Revision: / {printf "%05d\n", $2}'`
fi
if test "x$revision" = "x"; then
    revision="UNKNOWN"
fi

# substitute the linguas
linguas=`cd "po" 2>/dev/null && ls *.po 2>/dev/null | $AWK 'BEGIN { FS="."; ORS=" " } { print $1 }'`
if test "x$linguas" = "x"; then
    echo "autogen.sh: No po files were found, aborting." >&2
    exit 1
fi

sed -e "s/@LINGUAS@/${linguas}/g" \
    -e "s/@REVISION@/${revision}/g" \
    < "configure.in.in" > "configure.in"

exec xdt-autogen $@

# xdt-autogen clean does not remove all generated files
(test x"clean" = x"$1") && {
  rm -f configure.in
  rm -f INSTALL
} || true

# vi:set ts=2 sw=2 et ai:
