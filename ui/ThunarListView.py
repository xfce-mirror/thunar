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
from ThunarView import ThunarView

try:
    import pyexo
    pyexo.require('0.3')
    import exo
    exo_supported = True
except ImportError:
    exo_supported = False

signals_registered = False

class ThunarListView(gtk.TreeView, ThunarView):
    def __init__(self, dir_info):
        gtk.TreeView.__init__(self)
        ThunarView.__init__(self)

        # register signals
        global signals_registered
        if not signals_registered:
            try:
                gobject.signal_new('activated', self, gobject.SIGNAL_RUN_LAST, \
                                   gobject.TYPE_NONE, [ThunarFileInfo])
            except:
                pass
            try:
                gobject.signal_new('context-menu', self, gobject.SIGNAL_RUN_LAST, \
                                   gobject.TYPE_NONE, [])
            except:
                pass
            try:
                gobject.signal_new('selection-changed', self, gobject.SIGNAL_RUN_LAST, \
                                   gobject.TYPE_NONE, [])
            except:
                pass
            signals_registered = True

        self.set_model(ThunarModel(dir_info))

        column = gtk.TreeViewColumn('Name')
        renderer = gtk.CellRendererPixbuf()
        column.pack_start(renderer, False)
        column.add_attribute(renderer, 'pixbuf', ThunarModel.COLUMN_ICON)
        if exo_supported:
            renderer = exo.CellRendererEllipsizedText()
            renderer.set_property('ellipsize', exo.PANGO_ELLIPSIZE_END)
            renderer.set_property('ellipsize-set', True)
        else:
            renderer = gtk.CellRendererText()
        column.pack_start(renderer, True)
        column.add_attribute(renderer, 'text', ThunarModel.COLUMN_NAME)
        column.set_resizable(True)
        column.set_expand(True)
        column.set_sizing(gtk.TREE_VIEW_COLUMN_AUTOSIZE)
        self.append_column(column)
        self.set_expander_column(column)

        column = gtk.TreeViewColumn('Size')
        renderer = gtk.CellRendererText()
        renderer.set_property('xalign', 1.0)
        column.pack_start(renderer, True)
        column.add_attribute(renderer, 'text', ThunarModel.COLUMN_SIZE)
        column.set_resizable(True)
        column.set_expand(False)
        self.append_column(column)

        column = gtk.TreeViewColumn('Modified')
        renderer = gtk.CellRendererText()
        column.pack_start(renderer, False)
        column.add_attribute(renderer, 'text', ThunarModel.COLUMN_MTIME)
        column.set_resizable(True)
        column.set_expand(False)
        self.append_column(column)

        column = gtk.TreeViewColumn('Kind')
        renderer = gtk.CellRendererText()
        column.pack_start(renderer, False)
        column.add_attribute(renderer, 'text', ThunarModel.COLUMN_KIND)
        column.set_resizable(True)
        column.set_expand(False)
        self.append_column(column)

        self.set_rules_hint(True)
        self.set_headers_clickable(True)

        self.get_selection().set_mode(gtk.SELECTION_MULTIPLE)

        self.connect('row-activated', lambda tree, path, column: self._activated(path))
        self.connect('button-press-event', lambda tree, event: self._button_press_event(event))
        self.get_selection().connect('changed', lambda selection: self.selection_changed())


    def get_selected_files(self):
        selection = self.get_selection()
        model, paths = selection.get_selected_rows()
        list = []
        for path in paths:
            iter = model.get_iter(path)
            list.append(model.get(iter, ThunarModel.COLUMN_FILEINFO)[0])
        return list


    def select_all(self):
        self.get_selection().select_all()


    def _activated(self, path):
        iter = self.get_model().get_iter(path)
        info = self.get_model().get(iter, ThunarModel.COLUMN_FILEINFO)[0]
        if info.is_directory():
            self.activated(info)


    def _button_press_event(self, event):
        if event.button == 3 and event.type == gtk.gdk.BUTTON_PRESS:
            path, column, x, y = self.get_path_at_pos(int(event.x), int(event.y))
            if path:
                selection = self.get_selection()
                if not selection.path_is_selected(path):
                    selection.unselect_all()
                    selection.select_path(path)
                self.grab_focus()
                self.context_menu()
            return True
        elif (event.button == 1 or event.button == 2) and event.type == gtk.gdk._2BUTTON_PRESS:
            path, column, x, y = self.get_path_at_pos(int(event.x), int(event.y))
            if path:
                iter = self.get_model().get_iter(path)
                if event.button == 1:
                    selection = self.get_selection()
                    selection.unselect_all()
                    selection.select_path(path)
                info = self.get_model().get(iter, ThunarModel.COLUMN_FILEINFO)[0]
                if info.is_directory():
                    self.activated(info)
                if event.button == 2 or (event.state & gtk.gdk.SHIFT_MASK) != 0:
                    self.get_toplevel().destroy()
            return True
        return False
