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

from ThunarFileInfo import ThunarFileInfo
from ThunarModel import ThunarModel

signals_registered = False

class ThunarView(gtk.TreeView):
    def __init__(self, dir_info):
        gtk.TreeView.__init__(self)

        # register signals
        global signals_registered
        if not signals_registered:
            gobject.signal_new('selected', self, gobject.SIGNAL_RUN_LAST, \
                               gobject.TYPE_NONE, [ThunarFileInfo])
            signals_registered = True

        self.set_model(ThunarModel(dir_info))

        column = gtk.TreeViewColumn('Name')
        renderer = gtk.CellRendererPixbuf()
        column.pack_start(renderer, False)
        column.add_attribute(renderer, 'pixbuf', ThunarModel.COLUMN_ICON)
        renderer = gtk.CellRendererText()
        column.pack_start(renderer, True)
        column.add_attribute(renderer, 'text', ThunarModel.COLUMN_NAME)
        self.append_column(column)
        self.set_expander_column(column)

        column = gtk.TreeViewColumn('Size')
        renderer = gtk.CellRendererText()
        column.pack_start(renderer, False)
        column.add_attribute(renderer, 'text', ThunarModel.COLUMN_SIZE)
        self.append_column(column)

        column = gtk.TreeViewColumn('Modified')
        renderer = gtk.CellRendererText()
        column.pack_start(renderer, False)
        column.add_attribute(renderer, 'text', ThunarModel.COLUMN_MTIME)
        self.append_column(column)

        column = gtk.TreeViewColumn('Kind')
        renderer = gtk.CellRendererText()
        column.pack_start(renderer, False)
        column.add_attribute(renderer, 'text', ThunarModel.COLUMN_KIND)
        self.append_column(column)

        self.set_rules_hint(True)
        self.set_headers_clickable(True)

        self.connect('row-activated', lambda tree, path, column: tree._activated(path))


    def _activated(self, path):
        iter = self.get_model().get_iter(path)
        info = self.get_model().get(iter, ThunarModel.COLUMN_FILEINFO)[0]
        if info.is_directory():
            self.emit('selected', info)
