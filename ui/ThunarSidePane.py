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
from ThunarTreePane import ThunarTreePane
from ThunarBookmarksPane import ThunarBookmarksPane

class ThunarSidePane(gtk.VBox):
    DISABLED = 0
    TREE = 1
    SHORTCUTS = 2

    def __init__(self):
        gtk.VBox.__init__(self)
        self.set_spacing(0)

        self.frame = gtk.Frame()
        self.frame.set_border_width(0)
        self.frame.set_shadow_type(gtk.SHADOW_ETCHED_IN)
        self.pack_start(self.frame, False, False, 0)
        self.frame.show()

        hbox = gtk.HBox(False, 6)
        hbox.set_border_width(0)
        self.frame.add(hbox)
        hbox.show()

        self.label = gtk.Label('')
        self.label.set_alignment(0.0, 0.5)
        hbox.pack_start(self.label, True, True, 0)
        self.label.show()

        button = gtk.Button()
        button.set_relief(gtk.RELIEF_NONE)
        button.set_border_width(0)
        image = gtk.image_new_from_stock(gtk.STOCK_CLOSE, gtk.ICON_SIZE_MENU)
        button.add(image)
        image.show()
        button.connect('clicked', lambda btn: self.emit('hide-sidepane'))
        hbox.pack_start(button, False, False, 0)
        button.show()

        self.swin = gtk.ScrolledWindow(None, None)
        self.pack_start(self.swin, True, True, 0)
        self.swin.show()

        self.child = None
        self.set_state(self.SHORTCUTS)


    def _directory_changed(self, info):
        self.emit('directory-changed', info)


    def set_gtkfilechooser_like(self, value):
        if value:
            self.frame.hide()
            self.swin.set_shadow_type(gtk.SHADOW_ETCHED_IN)
        else:
            self.frame.show()
            self.swin.set_shadow_type(gtk.SHADOW_NONE)


    def set_state(self, state):
        if self.child:
            self.child.destroy()
            self.child = None

        if state == self.TREE:
            self.child = ThunarTreePane()
            self.handler_id = self.child.connect('directory-changed0', lambda tree, info: self._directory_changed(info))
            self.swin.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
            self.label.set_label(' Tree')
        elif state == self.SHORTCUTS:
            self.child = ThunarBookmarksPane()
            self.handler_id = self.child.connect('directory-changed1', lambda bm, info: self._directory_changed(info))
            self.swin.set_policy(gtk.POLICY_NEVER, gtk.POLICY_AUTOMATIC)
            self.label.set_label(' Shortcuts')
        else:
            self.hide()
            return

        self.swin.add(self.child)
        self.child.show()
        self.show()


    def select_by_info(self, info):
        if self.child:
            self.child.handler_block(self.handler_id)
            self.child.select_by_info(info)
            self.child.handler_unblock(self.handler_id)



gobject.type_register(ThunarSidePane)
gobject.signal_new('directory-changed', ThunarSidePane, \
                   gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, \
                   [ThunarFileInfo])
gobject.signal_new('hide-sidepane', ThunarSidePane, \
                   gobject.SIGNAL_RUN_LAST, gobject.TYPE_NONE, [])

