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
import gobject
import gtk

class ThunarView(gtk.TreeView):
  def __init__(self):
    gtk.TreeView.__init__(self)
    self.set_model(self.create_model())

    column = gtk.TreeViewColumn('Name')
    renderer = gtk.CellRendererPixbuf()
    column.pack_start(renderer, False)
    column.add_attribute(renderer, 'pixbuf', 0)
    renderer = gtk.CellRendererText()
    column.pack_start(renderer, True)
    column.add_attribute(renderer, 'text', 1)
    self.append_column(column)
    self.set_expander_column(column)

    column = gtk.TreeViewColumn('Size')
    renderer = gtk.CellRendererText()
    column.pack_start(renderer, False)
    column.add_attribute(renderer, 'text', 2)
    self.append_column(column)

    column = gtk.TreeViewColumn('Owner')
    renderer = gtk.CellRendererText()
    column.pack_start(renderer, False)
    column.add_attribute(renderer, 'text', 3)
    self.append_column(column)

    column = gtk.TreeViewColumn('Permissions')
    renderer = gtk.CellRendererText()
    column.pack_start(renderer, False)
    column.add_attribute(renderer, 'text', 4)
    self.append_column(column)

    self.set_rules_hint(True)
    self.set_headers_clickable(True)

  def create_model(self):
    model = gtk.ListStore(gtk.gdk.Pixbuf, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_STRING, gobject.TYPE_STRING)

    model.append([
      self.render_icon(gtk.STOCK_NEW, gtk.ICON_SIZE_MENU),
      'test1.txt',
      '2.0 KB',
      'Benedikt Meurer',
      '-rw-r--r--',
    ])
    
    model.append([
      self.render_icon(gtk.STOCK_NEW, gtk.ICON_SIZE_MENU),
      'test2.txt',
      '4.5 KB',
      'Benedikt Meurer',
      '-rw-r--r--',
    ])

    return model
