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

import dircache, os, stat, time

import pygtk
pygtk.require('2.0')
import gobject
import gtk

from ThunarMimeDatabase import ThunarMimeDatabase

class ThunarFileInfo(gobject.GObject):
    def __init__(self, path):
        gobject.GObject.__init__(self)
        self.mimedb = ThunarMimeDatabase()

        # build up a normalized path
        self.path = ''
        for name in path.split('/'):
            if name:
                self.path += '/' + name
        if not self.path:
            self.path = '/'

        self.stat = os.stat(self.path)


    def get_mime_info(self):
        return self.mimedb.match(self.path)

    def get_name(self):
        name = os.path.basename(self.path)
        if not name:
            name = '/'
        return name

    def get_path(self):
        return self.path

    def get_visible_name(self):
        name = self.get_name()
        if name == '/':
            name = 'Filesystem'
        return name

    def get_size(self):
        return _humanize_size(self.stat[stat.ST_SIZE])

    def get_mtime(self):
        return time.strftime('%x %X', time.localtime(self.stat[stat.ST_MTIME]))

    def is_directory(self):
        return stat.S_ISDIR(self.stat[stat.ST_MODE])

    def get_parent(self):
        if self.path == '/':
            return None
        else:
            return ThunarFileInfo(self.path[0:self.path.rfind('/')])



def _humanize_size(size):
    if size > 1024 * 1024 * 1024:
        return '%.1f GB' % (size / (1024 * 1024 * 1024))
    elif size > 1024 * 1024:
        return '%.1f MB' % (size / (1024 * 1024))
    elif size > 1024:
        return '%.1f KB' % (size / 1024)
    else:
        return '%d B' % size



