#!/usr/bin/env python
# vi:set ts=4 sw=4 et ai nocindent:
#
# $Id$
#
# Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
# All rights reserved.
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
# 02111-1307, USA.
#

import pygtk
pygtk.require('2.0')
import gobject
import gtk

__ALL__ = [ 'get_default' ]


default_loader = None
def get_default():
    global default_loader
    if not default_loader:
        default_loader = ThunarImageLoader()
    return default_loader

    
class ThunarImageLoader:
    def __init__(self):
        self.__theme = gtk.icon_theme_get_default()
        self.__cache = {}


    def load_icon(self, name, size):
        key = '%s@%sx%s' % (name, size, size)
        if self.__cache.has_key(key):
            icon = self.__cache[key]
        else:
            icon = self.__theme.load_icon(name, size, 0)
            self.__cache[key] = icon
        return icon
