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

signals_registered = False

class ThunarTreePane(gtk.TreeView):
    COLUMN_NAME = 0
    COLUMN_ICON = 1
    COLUMN_BOLD = 2
    COLUMN_INFO = 3

    ICON_SIZE = 24

    def __init__(self):
        gtk.TreeView.__init__(self)

        # register signals
        global signals_registered
        if not signals_registered:
            gobject.signal_new('directory-changed', self, gobject.SIGNAL_RUN_LAST, \
                               gobject.TYPE_NONE, [ThunarFileInfo])
            signals_registered = True

        self.model = gtk.TreeStore(gobject.TYPE_STRING, gtk.gdk.Pixbuf, gobject.TYPE_INT, ThunarFileInfo)

        info = ThunarFileInfo(os.getenv('HOME'))
        row = self.model.append(None, ['Home Folder', info.render_icon(ThunarTreePane.ICON_SIZE), 800, info])
        self.model.append(row, [None, None, 0, None])
        self.home_path = self.model.get_path(row)

        theme = gtk.icon_theme_get_default()
        try:
            icon = theme.load_icon('gnome-dev-cdrom', ThunarTreePane.ICON_SIZE, 0)
        except:
            icon = None
        row = self.model.append(None, ['Removable Media', icon, 800, None])
        self.model.append(row, [None, None, 0, None])

        info = ThunarFileInfo('/')
        row = self.model.append(None, ['Filesystem', info.render_icon(ThunarTreePane.ICON_SIZE), 800, info])
        self.model.append(row, [None, None, 0, None])
        self.fs_path = self.model.get_path(row)

        self.set_model(self.model)

        column = gtk.TreeViewColumn('Name')
        renderer = gtk.CellRendererPixbuf()
        column.pack_start(renderer, False)
        column.add_attribute(renderer, 'pixbuf', ThunarTreePane.COLUMN_ICON)
        renderer = gtk.CellRendererText()
        column.pack_start(renderer, True)
        column.add_attribute(renderer, 'text', ThunarTreePane.COLUMN_NAME)
        column.add_attribute(renderer, 'weight', ThunarTreePane.COLUMN_BOLD)
        self.append_column(column)
        self.set_expander_column(column)

        self.set_rules_hint(False)
        self.set_headers_visible(False)

        self.get_selection().connect('changed', lambda ign: self._selection_changed())
        self.connect('row-expanded', lambda self, iter, path: self._row_expanded(iter, path))


    def _row_expanded(self, iter, path):
        if self.model.iter_n_children(iter) != 1:
            return
        info = self.model.get(iter, ThunarTreePane.COLUMN_INFO)[0]
        if not info:
            return
        child = self.model.iter_nth_child(iter, 0)
        info = self.model.get(child, ThunarTreePane.COLUMN_INFO)[0]
        if info:
            return
        parent_info = self.model.get(iter, ThunarTreePane.COLUMN_INFO)[0]
        names = dircache.listdir(parent_info.get_path())[:]
        names.sort()
        for name in names:
            if name.startswith('.'):
                continue
            
            file_info = ThunarFileInfo(os.path.join(parent_info.get_path(), name))
            if not file_info.is_directory():
                continue

            row = self.model.append(iter, [file_info.get_visible_name(), file_info.render_icon(ThunarTreePane.ICON_SIZE), 400, file_info])
            self.model.append(row, [None, None, 0, None])

        self.model.remove(child)


    def _selection_changed(self):
        model, iter = self.get_selection().get_selected()
        if iter:
            info = self.model.get(iter, self.COLUMN_INFO)[0]
            self.emit('directory-changed', info)
        

    def select_by_info(self, info):
        tpath = self.fs_path

        components = []
        tmp = info
        while tmp:
            if tmp.get_path() == ThunarFileInfo(os.getenv('HOME')).get_path():
                tpath = self.home_path
                break
            elif tmp.get_path() == '/':
                break
            components.append(tmp)
            tmp = tmp.get_parent()

        components.reverse()

        for c in components:
            self.expand_to_path(tpath)
            parent_iter = self.model.get_iter(tpath)
            child_iter = self.model.iter_children(parent_iter)
            while child_iter:
                child_info = self.model.get(child_iter, self.COLUMN_INFO)[0]
                if child_info.get_path() == c.get_path():
                    break
                child_iter = self.model.iter_next(child_iter)

            if not child_iter:
                return
            tpath = self.model.get_path(child_iter)

        self.get_selection().select_path(tpath)
        self.scroll_to_cell(tpath)
