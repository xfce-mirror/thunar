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

class ThunarPathBar(gtk.HBox):
    def __init__(self):
        gtk.HBox.__init__(self)
        self.set_spacing(2)

        self.__info = None
        self.__rebuild_disabled = False


    def __clicked(self, info):
        self.__rebuild(self.__info, info)
        self.__rebuild_disabled = True
        self.emit('directory-changed', info)
        self.__rebuild_disabled = False


    def __rebuild(self, info, selected_info):
        if self.__rebuild_disabled:
            return

        # remove all existing buttons
        self.foreach(lambda child: child.destroy())

        icon_size = gtk.icon_size_lookup(gtk.ICON_SIZE_BUTTON)[0]

        align = gtk.Alignment(1.0, 1.0, 1.0, 1.0)
        align.set_property('width-request', 16)
        self.pack_start(align, False, False, 0)
        align.show()

        self.__info = info
        while info:
            if info.get_path() == selected_info.get_path():
                button = gtk.ToggleButton()
                button.set_active(True)
            else:
                button = gtk.Button()
            button.set_data('thunar-info', info)
            button.connect('clicked', lambda button: self.__clicked(button.get_data('thunar-info')))
            self.pack_start(button, False, False, 0)
            self.reorder_child(button, 0)
            button.show()

            if info.get_path() == '/':
                image = gtk.Image()
                image.set_from_pixbuf(info.render_icon(icon_size))
                button.add(image)
                image.show()
                break

            hbox = gtk.HBox(False, 2)
            button.add(hbox)
            hbox.show()

            image = gtk.Image()
            image.set_from_pixbuf(info.render_icon(icon_size))
            hbox.pack_start(image, False, False, 0)
            image.show()

            if info.is_home():
                name = 'Home'
            elif info.is_desktop():
                name = 'Desktop'
            else:
                name = info.get_visible_name()

            if info.get_path() == selected_info.get_path():
                text = '<b>%s</b>' % name.replace('&', '&amp;')
            else:
                text = name.replace('&', '&amp;')

            label = gtk.Label(text)
            label.set_use_markup(True)
            hbox.pack_start(label, True, True, 0)
            label.show()

            if info.is_home() or info.is_desktop():
                break

            info = info.get_parent()


    def get_info(self, info):
        return self.__info


    def set_info(self, info):
        self.__rebuild(info, info)



gobject.type_register(ThunarPathBar)
gobject.signal_new('directory-changed', ThunarPathBar, \
                   gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, \
                   [ThunarFileInfo])

