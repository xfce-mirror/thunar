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

import dircache, fnmatch, os

import pygtk
pygtk.require('2.0')
import gobject
import gtk

from ThunarFileInfo import ThunarFileInfo

class ThunarModel(gtk.ListStore):
    COLUMN_ICON     = 0
    COLUMN_ICONHUGE = 1
    COLUMN_NAME     = 2
    COLUMN_SIZE     = 3
    COLUMN_MTIME    = 4
    COLUMN_KIND     = 5
    COLUMN_FILEINFO = 6

    def __init__(self, dir_info):
        gtk.ListStore.__init__(self, gtk.gdk.Pixbuf,\
                               gtk.gdk.Pixbuf, \
                               gobject.TYPE_STRING, \
                               gobject.TYPE_STRING, \
                               gobject.TYPE_STRING, \
                               gobject.TYPE_STRING, \
                               ThunarFileInfo)

        self.set_default_sort_func(self._compare)
        self.set_sort_column_id(-1, gtk.SORT_ASCENDING)

        for name in dircache.listdir(dir_info.get_path()):
            if name.startswith('.'):
                continue

            file_info = ThunarFileInfo(os.path.join(dir_info.get_path(), name))
            mime_info = file_info.get_mime_info()

            self.append([
                file_info.render_icon(16),
                file_info.render_icon(48),
                file_info.get_visible_name(),
                file_info.get_size(),
                file_info.get_mtime(),
                mime_info.get_comment(),
                file_info
            ])


    def match(self, pattern):
        result = []
        iter = self.get_iter_first()
        while iter:
            info = self.get(iter, self.COLUMN_FILEINFO)[0]
            if fnmatch.fnmatch(info.get_visible_name(), pattern):
                result.append(iter)
            iter = self.iter_next(iter)
        return result


    def _compare(self, model, iter1, iter2):
        info1 = self.get(iter1, self.COLUMN_FILEINFO)[0]
        info2 = self.get(iter2, self.COLUMN_FILEINFO)[0]

        if info1 and not info2:
            return -1
        elif not info1 and info2:
            return 1

        if info1.is_directory() and not info2.is_directory():
            return -1
        elif info2.is_directory() and not info1.is_directory():
            return 1

        if info1.get_visible_name() < info2.get_visible_name():
            return -1
        elif info1.get_visible_name() > info2.get_visible_name():
            return 1

        return 0
