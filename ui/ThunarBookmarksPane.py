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

import os

import pygtk
pygtk.require('2.0')
import gobject
import gtk

from ThunarFileInfo import ThunarFileInfo

signals_registered = False

class ThunarBookmarksPane(gtk.TreeView):
    COLUMN_NAME = 0
    COLUMN_ICON = 1
    COLUMN_INFO = 2

    ICON_SIZE = 32


    def __init__(self):
        gtk.TreeView.__init__(self, self._create_model())

        # register signals
        global signals_registered
        if not signals_registered:
            gobject.signal_new('directory-changed1', self, gobject.SIGNAL_RUN_LAST, \
                               gobject.TYPE_NONE, [ThunarFileInfo])
            signals_registered = True

        column = gtk.TreeViewColumn('Name')
        renderer = gtk.CellRendererPixbuf()
        column.pack_start(renderer, False)
        column.add_attribute(renderer, 'pixbuf', self.COLUMN_ICON)
        renderer = gtk.CellRendererText()
        column.pack_start(renderer, True)
        column.add_attribute(renderer, 'text', self.COLUMN_NAME)
        self.append_column(column)
        self.set_expander_column(column)

        self.get_selection().set_mode(gtk.SELECTION_BROWSE)
        self.get_selection().unselect_all()
        self.get_selection().connect('changed', lambda selection: self._selection_changed())

        self.set_headers_visible(False)
        self.set_rules_hint(False)


    def _create_model(self):
        self.model = gtk.ListStore(gobject.TYPE_STRING, gtk.gdk.Pixbuf, ThunarFileInfo)

        home = os.getenv('HOME')

        for path in [home, os.path.join(home, 'Desktop'), '/']:
            try:
                info = ThunarFileInfo(path)
                self.model.append([info.get_visible_name(), info.render_icon(self.ICON_SIZE), info])
            except:
                pass
        
        try:
            fp = file(os.path.join(home, '.gtk-bookmarks'))
            for line in fp.readlines():
                line = line[:-1]
                if not line.startswith('file://'):
                    continue
                path = line[7:]
                try:
                    info = ThunarFileInfo(path)
                    self.model.append([info.get_visible_name(), info.render_icon(self.ICON_SIZE), info])
                except:
                    pass
        except IOError:
            pass
        return self.model


    def _selection_changed(self):
        selection = self.get_selection()
        model, iter = selection.get_selected()
        if iter:
            info = model.get(iter, self.COLUMN_INFO)[0]
            self.emit('directory-changed1', info)


    def select_by_info(self, info):
        self.get_selection().unselect_all()
        iter = self.model.iter_children(None)
        while iter:
            bm_info = self.model.get(iter, self.COLUMN_INFO)[0]
            if bm_info.get_path() == info.get_path():
                path = self.model.get_path(iter)
                self.get_selection().select_path(path)
                return
            iter = self.model.iter_next(iter)
