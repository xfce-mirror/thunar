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

class ThunarView:
    def __init__(self):
        return

    def get_selected_files(self):
        pass


    def select_all(self):
        pass


    def select_by_pattern(self, pattern):
        pass


    def invert_selection(self):
        pass


    def activated(self, info, new_window = False):
        self.emit('activated', info, new_window)


    def context_menu(self):
        self.emit('context-menu')


    def selection_changed(self):
        self.emit('selection-changed')

