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


class ThunarColumnsView(gtk.Viewport, ThunarView):
    def __init__(self, info):
        gtk.Viewport.__init__(self)
        ThunarView.__init__(self)

        self.set_model(ThunarModel(info))


    def set_model(self, model):
        # remove the old view (if any)
        child = self.get_child()
        if child:
            child.destroy()

        self.__model = model

        box = gtk.HPaned()
        self.add(box)
        box.show()

        column = self.__create_column(model)
        box.pack1(column, False, False)
        column.show()

        expander = self.__create_column(None)
        box.pack2(expander, True, True)
        expander.show()

        column.set_data('thunar-box', box)
        column.set_data('thunar-next', expander)
        

    def get_model(self):
        return self.__model


    def get_selected_files(self):
        return []


    def __selection_changed(self, view):
        selection = view.get_selection()
        model, iter = selection.get_selected()
        if not iter:
            return

        column = view.get_parent()
        box = column.get_data('thunar-box')
        next = column.get_data('thunar-next')
        if next:
            next.destroy()

        info = model.get(iter, ThunarModel.COLUMN_FILEINFO)[0]
        if info.is_directory():
            box2 = gtk.HPaned()
            box.pack2(box2, True, True)
            box2.show()

            column.set_data('thunar-next', box2)

            column = self.__create_column(ThunarModel(info))
            box2.pack1(column, False, False)
            column.show()

            expander = self.__create_column(None)
            box2.pack2(expander, True, True)
            expander.show()

            column.set_data('thunar-box', box2)
            column.set_data('thunar-next', expander)
        else:
            ebox = gtk.EventBox()
            ebox.modify_bg(gtk.STATE_NORMAL, gtk.gdk.color_parse('White'))
            column.set_data('thunar-next', ebox)
            box.pack2(ebox, True, False)
            ebox.show()

            alignment = gtk.Alignment(0.5, 0.5, 0, 0)
            ebox.add(alignment)
            alignment.show()

            vbox = gtk.VBox(False, 12)
            alignment.add(vbox)
            vbox.show()

            image = gtk.Image()
            image.set_from_pixbuf(info.render_icon(128))
            image.set_property('width-request', 168)
            vbox.pack_start(image, False, False, 0)
            image.show()

            label = gtk.Label(info.get_visible_name())
            vbox.pack_start(label, False, False, 0)
            label.show()

            align = gtk.Alignment(0.5, 0.5)
            vbox.pack_start(align, False, False, 0)
            align.show()

            table = gtk.Table(3, 2, False)
            table.set_col_spacings(6)
            table.set_row_spacings(3)
            align.add(table)
            table.show()

            label = gtk.Label('<small>Kind:</small>')
            label.set_use_markup(True)
            label.set_alignment(1.0, 0.5)
            table.attach(label, 0, 1, 0, 1, gtk.FILL, gtk.FILL)
            label.show()

            label = gtk.Label('<small>%s</small>' % info.get_mime_info().get_comment())
            label.set_use_markup(True)
            label.set_alignment(0.0, 0.5)
            table.attach(label, 1, 2, 0, 1, gtk.FILL, gtk.FILL)
            label.show()

            label = gtk.Label('<small>Size:</small>')
            label.set_use_markup(True)
            label.set_alignment(1.0, 0.5)
            table.attach(label, 0, 1, 1, 2, gtk.FILL, gtk.FILL)
            label.show()

            label = gtk.Label('<small>%s</small>' % info.get_size())
            label.set_use_markup(True)
            label.set_alignment(0.0, 0.5)
            table.attach(label, 1, 2, 1, 2, gtk.FILL, gtk.FILL)
            label.show()

        self.get_hadjustment().set_value(self.get_hadjustment().upper)


    def __create_column(self, model):
        view = gtk.TreeView()
        view.set_model(model)

        column = gtk.TreeViewColumn('Name')
        renderer = gtk.CellRendererPixbuf()
        column.pack_start(renderer, False)
        column.add_attribute(renderer, 'pixbuf', ThunarModel.COLUMN_ICON)
        renderer = gtk.CellRendererText()
        column.pack_start(renderer, True)
        column.add_attribute(renderer, 'text', ThunarModel.COLUMN_NAME)
        column.set_resizable(True)
        column.set_expand(True)
        column.set_sizing(gtk.TREE_VIEW_COLUMN_AUTOSIZE)
        view.append_column(column)
        view.set_expander_column(column)

        view.set_rules_hint(False)
        view.set_headers_visible(False)

        swin = gtk.ScrolledWindow()
        swin.set_policy(gtk.POLICY_NEVER, gtk.POLICY_AUTOMATIC)
        swin.add(view)
        view.show()

        view.connect('button-press-event', lambda view, event: self.__button_pressed(view, event))

        selection = view.get_selection()
        selection.connect('changed', lambda selection: self.__selection_changed(selection.get_tree_view()))

        return swin


    def __button_pressed(self, view, event):
        if event.button == 3 and event.type == gtk.gdk.BUTTON_PRESS:
            path, column, x, y = view.get_path_at_pos(int(event.x), int(event.y))
            if path:
                selection = view.get_selection()
                if not selection.path_is_selected(path):
                    selection.unselect_all()
                    selection.select_path(path)
                view.grab_focus()
                self.context_menu()
            return True
        return False


gobject.type_register(ThunarColumnsView)
gobject.signal_new('activated', ThunarColumnsView, gobject.SIGNAL_RUN_LAST, \
                   gobject.TYPE_NONE, [ThunarFileInfo])
gobject.signal_new('context-menu', ThunarColumnsView, \
                   gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, [])
gobject.signal_new('selection-changed', ThunarColumnsView, \
                   gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, [])
