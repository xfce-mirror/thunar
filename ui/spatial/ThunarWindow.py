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

import weakref, gc

import pygtk
pygtk.require('2.0')
import gtk

from ThunarStatusBar import ThunarStatusBar
from ThunarView import ThunarView

windows = weakref.WeakValueDictionary()

class ThunarWindow(gtk.Window):
    def destroyed(self):
        del windows[self.dir_info.get_path()] 
        gc.collect()
        if len(windows) == 0:
            gtk.main_quit()

    def __init__(self, dir_info):
        gtk.Window.__init__(self, gtk.WINDOW_TOPLEVEL)
        self.connect('destroy', lambda self: self.destroyed())

        global windows
        self.dir_info = dir_info
        windows[dir_info.get_path()] = self

        self.set_title(dir_info.get_visible_name())
        self.set_default_size(400,350)

        action_group = gtk.ActionGroup('thunar-window')
        action_group.add_actions([
          ('file-menu', None, '_File'),
          ('close-parent-folders', None, 'Close _Parent Folders', '<Control><Shift>W', None, lambda ign, self: self._close_parent_folders()),
          ('close-window', gtk.STOCK_CLOSE, '_Close', '<Control>W', None, lambda ign, self: self.destroy()),
        ], self)
        action_group.add_actions([
          ('window-menu', None, '_Window'),
        ], self)
        action_group.add_actions([
          ('help-menu', None, '_Help'),
          ('contents', gtk.STOCK_HELP, '_Contents', 'F1'),
          ('report-bug', None, '_Report bug'),
          ('about', gtk.STOCK_DIALOG_INFO, '_About'),
        ], self)

        self.ui_manager = gtk.UIManager()
        self.ui_manager.insert_action_group(action_group, 0)
        self.ui_manager.add_ui_from_file('thunar.ui')
        self.add_accel_group(self.ui_manager.get_accel_group())

        main_vbox = gtk.VBox(False, 0)
        self.add(main_vbox)
        main_vbox.show()

        menu_bar = self.ui_manager.get_widget('/main-menu')
        main_vbox.pack_start(menu_bar, False, False, 0)
        menu_bar.show()

        swin = gtk.ScrolledWindow(None, None)
        swin.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        main_vbox.pack_start(swin, True, True, 0)
        swin.show()

        self.view = ThunarView(dir_info)
        self.view.connect('selected', lambda widget, info: self.open_dir(info))
        swin.add(self.view)
        self.view.show()

        self.status_bar = ThunarStatusBar(dir_info)
        self.status_bar.connect('selected', lambda widget, info: self.open_dir(info))
        main_vbox.pack_start(self.status_bar, False, False, 0)
        self.status_bar.show()


    def _close_parent_folders(self):
        dir_info = self.dir_info.get_parent()
        while dir_info:
            if windows.has_key(dir_info.get_path()):
                windows[dir_info.get_path()].destroy()
            dir_info = dir_info.get_parent()


    def open_dir(self, dir_info):
        gc.collect()
        try:
            window = windows[dir_info.get_path()]
            window.present()
        except:
            window = ThunarWindow(dir_info)
            window.show()
