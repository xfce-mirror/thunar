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

import dircache, os

import pygtk
pygtk.require('2.0')
import gobject
import gtk

from ThunarFileInfo import ThunarFileInfo

class ThunarModel(gtk.ListStore):
    COLUMN_ICON     = 0
    COLUMN_NAME     = 1
    COLUMN_SIZE     = 2
    COLUMN_MTIME    = 3
    COLUMN_KIND     = 4
    COLUMN_FILEINFO = 5

    def __init__(self, dir_info):
        gtk.ListStore.__init__(self, gtk.gdk.Pixbuf,\
                               gobject.TYPE_STRING, \
                               gobject.TYPE_STRING, \
                               gobject.TYPE_STRING, \
                               gobject.TYPE_STRING, \
                               ThunarFileInfo)

        for name in dircache.listdir(dir_info.get_path()):
            if name.startswith('.'):
                continue

            file_info = ThunarFileInfo(os.path.join(dir_info.get_path(), name))
            mime_info = file_info.get_mime_info()

            self.append([
                mime_info.render_icon(16),
                file_info.get_visible_name(),
                file_info.get_size(),
                file_info.get_mtime(),
                mime_info.get_comment(),
                file_info
            ])



