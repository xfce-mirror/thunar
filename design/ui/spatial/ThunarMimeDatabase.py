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


class ThunarMimeDatabase:
    def match(self, path):
        type = rox.mime.get_type(path)
        return ThunarMimeInfo(type)


class ThunarMimeInfo:
    def __init__(self, type):
        self.theme = gtk.icon_theme_get_default()
        self.type = type

    def get_comment(self):
        return self.type.get_comment()

    def render_icon(self, size):
        type = '%s' % self.type
        try:
            name = 'mime-' + type.replace('/', ':')
            icon = self.theme.load_icon(name, size, 0)
        except gobject.GError:
            try:
                name = 'gnome-mime-' + type.replace('/', '-')
                icon = self.theme.load_icon(name, size, 0)
            except gobject.GError:
                try:
                    name = 'mime-' + type.split('/')[0]
                    icon = self.theme.load_icon(name, size, 0)
                except gobject.GError:
                    try:
                        name = 'gnome-mime-' + type.split('/')[0]
                        icon = self.theme.load_icon(name, size, 0)
                    except gobject.GError:
                        name = 'gnome-mime-application-octet-stream'
                        icon = self.theme.load_icon(name, size, 0)
        return icon
