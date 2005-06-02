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

from ThunarFileInfo import ThunarFileInfo
from ThunarModel import ThunarModel
from ThunarView import ThunarView

class ThunarIconView(exo.IconView, ThunarView):
    def __init__(self, dir_info):
        exo.IconView.__init__(self, ThunarModel(dir_info))
        ThunarView.__init__(self)

        self.set_text_column(ThunarModel.COLUMN_NAME)
        self.set_pixbuf_column(ThunarModel.COLUMN_ICONHUGE)

        self.set_selection_mode(gtk.SELECTION_MULTIPLE)

        self.connect('item-activated', lambda self, path: self._activated(path))
        self.connect('button-press-event', lambda self, event: self._button_press_event(event))
        

    def get_selected_files(self):
        paths = self.get_selected_items()
        list = []
        for path in paths:
            iter = self.get_model().get_iter(path)
            list.append(self.get_model().get(iter, ThunarModel.COLUMN_FILEINFO)[0])
        return list


    def select_by_pattern(self, pattern):
        self.unselect_all()
        for iter in self.get_model().match(pattern):
            path = self.get_model().get_path(iter)
            self.select_path(path)


    def invert_selection(self):
        model = self.get_model()
        iter = model.get_iter_first()
        while iter:
            path = model.get_path(iter)
            if self.path_is_selected(path):
                self.unselect_path(path)
            else:
                self.select_path(path)
            iter = model.iter_next(iter)


    def _activated(self, path):
        iter = self.get_model().get_iter(path)
        info = self.get_model().get(iter, ThunarModel.COLUMN_FILEINFO)[0]
        if info.is_directory():
            self.activated(info)


    def _button_press_event(self, event):
        if event.button == 3 and event.type == gtk.gdk.BUTTON_PRESS:
            path = self.get_path_at_pos(int(event.x), int(event.y))
            if path:
                if not self.path_is_selected(path):
                    self.unselect_all()
                    self.select_path(path)
                self.grab_focus()
                self.context_menu()
            return True
        elif event.button == 2 and event.type == gtk.gdk.BUTTON_PRESS:
            path = self.get_path_at_pos(int(event.x), int(event.y))
            if path:
                iter = self.get_model().get_iter(path)
                info = self.get_model().get(iter, ThunarModel.COLUMN_FILEINFO)[0]
                if info.is_directory():
                    self.unselect_all()
                    self.select_path(path)
                    self.activated(info, True)
            return True
        elif event.button == 1 and event.type == gtk.gdk._2BUTTON_PRESS:
            path = self.get_path_at_pos(int(event.x), int(event.y))
            if path:
                iter = self.get_model().get_iter(path)
                if event.button == 1:
                    self.unselect_all()
                    self.select_path(path)
                info = self.get_model().get(iter, ThunarModel.COLUMN_FILEINFO)[0]
                if info.is_directory():
                    self.activated(info)
            return True
        return False



gobject.type_register(ThunarIconView)
gobject.signal_new('activated', ThunarIconView, gobject.SIGNAL_RUN_LAST, \
                   gobject.TYPE_NONE, [ThunarFileInfo, gobject.TYPE_BOOLEAN])
gobject.signal_new('context-menu', ThunarIconView, gobject.SIGNAL_RUN_LAST, \
                   gobject.TYPE_NONE, [])
