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

import os

import pygtk
pygtk.require('2.0')
import gtk

from ThunarFileInfo import ThunarFileInfo
from ThunarWindow import ThunarWindow

class Main:
  def __init__(self):
    self.window = ThunarWindow(ThunarFileInfo(os.getcwd()))
    self.window.connect('destroy', gtk.main_quit)

  def run(self):
    self.window.show()
    gtk.main()


if __name__ == "__main__":
  main = Main()
  main.run()

