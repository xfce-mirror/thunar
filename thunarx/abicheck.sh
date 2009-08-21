#!/bin/sh
#
# Copyright (c) 2004 The GLib Development Team.
# Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>.
#
# This library is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public
# License as published by the Free Software Foundation; either
# version 2 of the License, or (at your option) any later version.
#
# This library is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
# Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# License along with this library; if not, write to the
# Free Software Foundation, Inc., 59 Temple Place - Suite 330,
# Boston, MA 02111-1307, USA.
#

cpp -P -DALL_FILES ${srcdir:-.}/thunarx.symbols | sed -e '/^$/d' -e 's/ G_GNUC.*$//' -e 's/ PRIVATE//' | sort > expected-abi
nm -D .libs/libthunarx-2.so | grep " T\|R " | cut -d ' ' -f 3 | grep -v '^_.*' | sort > actual-abi
diff -u expected-abi actual-abi && rm expected-abi actual-abi
