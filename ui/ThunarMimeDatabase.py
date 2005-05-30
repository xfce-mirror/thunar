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

import pyexo
pyexo.require('0.3')
import exo


__ALL__ = [ 'get_default' ]


default_database = None
def get_default():
    global default_database
    if not default_database:
        default_database = ThunarMimeDatabase()
    return default_database



class ThunarMimeDatabase:
    def __init__(self):
        self.__db = exo.mime_database_get_default()


    def match(self, path):
        return ThunarMimeInfo(self.__db.get_info_for_file(path))


    def get_info(self, type):
        return ThunarMimeInfo(self.__db.get_info(type))



class ThunarMimeInfo:
    def __init__(self, type):
        self.__loader = gtk.icon_theme_get_default()
        self.__type = type


    def get_comment(self):
        return self.__type.get_comment()


    def render_icon(self, size):
        icon = self.__type.lookup_icon(self.__loader, size, 0)
        if icon:
            return icon.load_icon()
        else:
            return gtk.gdk.pixbuf_new_from_file_at_size('fallback.svg', size, size)
