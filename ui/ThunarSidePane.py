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

signals_registered = False

class ThunarSidePane(gtk.Notebook):
    def __init__(self):
        gtk.Notebook.__init__(self)

        # register signals
        global signals_registered
        if not signals_registered:
            gobject.signal_new('directory-changed', self, gobject.SIGNAL_RUN_LAST, \
                               gobject.TYPE_NONE, [ThunarFileInfo])
            signals_registered = True

        swin = gtk.ScrolledWindow(None, None)
        swin.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        label = gtk.Label('Tree')
        self.append_page(swin, label)
        label.show()
        swin.show()

        self.tree = ThunarTreePane()
        self.tree_handler_id = self.tree.connect('directory-changed0', lambda tree, info: self._directory_changed(info))
        swin.add(self.tree)
        self.tree.show()

        swin = gtk.ScrolledWindow(None, None)
        swin.set_policy(gtk.POLICY_NEVER, gtk.POLICY_AUTOMATIC)
        swin.set_shadow_type(gtk.SHADOW_IN)
        label = gtk.Label('Bookmarks')
        self.append_page(swin, label)
        label.show()
        swin.show()

        self.bm = ThunarBookmarksPane()
        self.bm_handler_id = self.bm.connect('directory-changed1', lambda bm, info: self._directory_changed(info))
        swin.add(self.bm)
        self.bm.show()


    def _directory_changed(self, info):
        self.emit('directory-changed', info)


    def select_by_info(self, info):
        self.tree.handler_block(self.tree_handler_id)
        self.tree.select_by_info(info)
        self.tree.handler_unblock(self.tree_handler_id)

        self.bm.handler_block(self.bm_handler_id)
        self.bm.select_by_info(info)
        self.bm.handler_unblock(self.bm_handler_id)
