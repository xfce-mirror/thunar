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

import rox, rox.mime

import ThunarImageLoader


__ALL__ = [ 'get_default' ]


default_database = None
def get_default():
    global default_database
    if not default_database:
        default_database = ThunarMimeDatabase()
    return default_database



class ThunarMimeDatabase:
    def __init__(self):
        self.__cache = {}


    def match(self, path):
        type = rox.mime.get_type(path)
        name = '%s' % type
        if self.__cache.has_key(name):
            type = self.__cache[name]
        else:
            type = ThunarMimeInfo(type)
            self.__cache[name] = type
        return type



class ThunarMimeInfo:
    def __init__(self, type):
        self.__loader = ThunarImageLoader.get_default()
        self.__type = type


    def get_comment(self):
        return self.__type.get_comment()


    def render_icon(self, size):
        type = '%s' % self.__type
        try:
            name = 'mime-' + type.replace('/', ':')
            icon = self.__loader.load_icon(name, size)
        except gobject.GError:
            try:
                name = 'gnome-mime-' + type.replace('/', '-')
                icon = self.__loader.load_icon(name, size)
            except gobject.GError:
                try:
                    name = 'mime-' + type.split('/')[0]
                    icon = self.__loader.load_icon(name, size)
                except gobject.GError:
                    try:
                        name = 'gnome-mime-' + type.split('/')[0]
                        icon = self.__loader.load_icon(name, size)
                    except gobject.GError:
                        try:
                            name = 'mime-application:octet-stream'
                            icon = self.__loader.load_icon(name, size)
                        except gobject.GError:
                            try:
                                name = 'gnome-mime-application-octet-stream'
                                icon = self.__loader.load_icon(name, size)
                            except gobject.GError:
                                icon = gtk.gdk.pixbuf_new_from_file_at_size('fallback.svg', size, size)
        return icon
