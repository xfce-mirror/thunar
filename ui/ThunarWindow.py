#!/usr/bin/env python
# vi:set ts=2 sw=2 et ai nocindent:
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
import gtk

from ThunarPropertiesDialog import ThunarPropertiesDialog
from ThunarView import ThunarView

class ThunarWindow(gtk.Window):
  def __init__(self):
    gtk.Window.__init__(self, gtk.WINDOW_TOPLEVEL)
    self.set_title('Thunar')
    self.set_default_size(400,350)

    action_group = gtk.ActionGroup('thunar-window')
    action_group.add_actions([
      ('file-menu', None, '_File'),
      ('properties', gtk.STOCK_PROPERTIES, '_Properties...', '<Alt>Return', None, lambda ign, self: ThunarPropertiesDialog(self).show()),
      ('close-window', gtk.STOCK_CLOSE, '_Close', '<Control>W', None, lambda ign, self: self.destroy()),
    ], self)
    action_group.add_actions([
      ('edit-menu', None, '_Edit'),
      ('copy-files', gtk.STOCK_COPY, '_Copy Files', '<Control>C'),
      ('cut-files', gtk.STOCK_CUT, 'Cu_t Files', '<Control>X'),
      ('paste-files', gtk.STOCK_PASTE, '_Paste Files', '<Control>V'),
      ('select-all-files', None, 'Select _All Files', '<Control>S'),
      ('select-by-pattern', None, 'Select by Pattern', None),
      ('edit-toolbars', None, '_Edit Toolbars...', None),
      ('preferences', gtk.STOCK_PREFERENCES, 'Prefere_nces...', None),
    ], self)
    action_group.add_actions([
      ('view-menu', None, '_View'),
      ('reload', gtk.STOCK_REFRESH, '_Reload', '<Control>R'),
    ], self)
    action_group.add_radio_actions([
      ('view-as-icons', None, 'View as _Icons'),
      ('view-as-list', None, 'View as _List'),
    ], 0, None, self)
    action_group.add_actions([
      ('go-menu', None, '_Go'),
      ('go-up', gtk.STOCK_GO_UP, '_Up', '<Alt>Up'),
      ('go-back', gtk.STOCK_GO_BACK, '_Back', '<Alt>Left'),
      ('go-forward', gtk.STOCK_GO_FORWARD, '_Forward', '<Alt>Right'),
      ('go-home', gtk.STOCK_HOME, '_Home Folder'),
      ('go-location', None, '_Location...'),
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

    self.main_vbox = gtk.VBox(False, 0)
    self.add(self.main_vbox)
    self.main_vbox.show()

    menu_bar = self.ui_manager.get_widget('/main-menu')
    self.main_vbox.pack_start(menu_bar, False, False, 0)
    menu_bar.show()

    tool_bar = self.ui_manager.get_widget('/main-toolbar')
    self.main_vbox.pack_start(tool_bar, False, False, 0)
    tool_bar.show()

    self.main_hbox = gtk.HPaned()
    self.main_vbox.pack_start(self.main_hbox, True, True, 0)
    self.main_hbox.show()


    swin = gtk.ScrolledWindow(None, None)
    swin.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
    self.main_hbox.add2(swin)
    swin.show()

    self.view = ThunarView()
    swin.add(self.view)
    self.view.show()
