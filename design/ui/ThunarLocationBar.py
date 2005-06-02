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

class ThunarLocationBar(gtk.HBox):
    def __init__(self):
        gtk.HBox.__init__(self)
        self.set_spacing(2)

#        label = gtk.Label('Location:')
#        self.pack_start(label, False, False, 0)
#        label.show()

        self.__entry = gtk.Entry()
        self.__entry.connect('activate', lambda e: self.__activated())
        self.pack_start(self.__entry, True, True, 0)
        self.__entry.show()

        button = gtk.Button()
        button.set_relief(gtk.RELIEF_NONE)
        button.connect('clicked', lambda b: self.__entry.activate())
        self.pack_start(button, False, False, 0)
        button.show()

        image = gtk.image_new_from_stock(gtk.STOCK_JUMP_TO, gtk.ICON_SIZE_MENU)
        button.add(image)
        image.show()


    def __activated(self):
        self.emit('directory-changed', self.get_info())


    def get_info(self):
        return ThunarFileInfo(self.__entry.get_text())


    def set_info(self, info):
        self.__entry.set_text(info.get_path())
        self.__entry.set_position(-1)
        self.__entry.select_region(0, 0)


    def focus(self):
        self.__entry.set_position(-1)
        self.__entry.select_region(0, -1)
        self.__entry.grab_focus()



gobject.type_register(ThunarLocationBar)
gobject.signal_new('directory-changed', ThunarLocationBar, \
                   gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, \
                   [ThunarFileInfo])

