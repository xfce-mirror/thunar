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

class ThunarPropertiesDialog(gtk.Dialog):
  def __init__(self, parent):
    gtk.Dialog.__init__(self, 'foo.mp3 Properties', parent, gtk.DIALOG_DESTROY_WITH_PARENT,
                        (gtk.STOCK_HELP, gtk.RESPONSE_HELP, gtk.STOCK_CLOSE, gtk.RESPONSE_CLOSE))
    self.set_has_separator(False)
    self.connect('response', lambda self, response: self.destroy())

    tooltips = gtk.Tooltips()

    notebook = gtk.Notebook()
    self.vbox.pack_start(notebook, True, True, 0)
    notebook.show()


    ###
    ### General
    ###
    table = gtk.Table(2, 4, False)
    table.set_col_spacings(10)
    table.set_row_spacings(6)
    table.set_border_width(6)
    label = gtk.Label('General')
    notebook.append_page(table, label)
    label.show()
    table.show()

    box = gtk.HBox(False, 6)
    table.attach(box, 0, 1, 0, 1, gtk.FILL, gtk.FILL)
    box.show()

    image = gtk.image_new_from_stock(gtk.STOCK_NEW, gtk.ICON_SIZE_DIALOG)
    box.pack_start(image, False, False, 0)
    image.show()

    label = gtk.Label('<b>Name:</b>')
    label.set_use_markup(True)
    label.set_alignment(1.0, 0.5)
    box.pack_start(label, True, True, 0)
    label.show()

    entry = gtk.Entry()
    entry.set_text('foo.mp3')
    table.attach(entry, 1, 2, 0, 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
    entry.show()

    label = gtk.Label('<b>Type:</b>')
    label.set_use_markup(True)
    label.set_alignment(1.0, 0.5)
    table.attach(label, 0, 1, 1, 2, gtk.FILL, gtk.FILL)
    label.show()

    ebox = gtk.EventBox()
    tooltips.set_tip(ebox, 'audio/x-mp3')
    table.attach(ebox, 1, 2, 1, 2, gtk.EXPAND | gtk.FILL, gtk.FILL)
    ebox.show()

    label = gtk.Label('MP3 audio')
    label.set_alignment(0.0, 0.5)
    label.set_selectable(True)
    ebox.add(label)
    label.show()

    label = gtk.Label('<b>Size:</b>')
    label.set_use_markup(True)
    label.set_alignment(1.0, 0.5)
    table.attach(label, 0, 1, 2, 3, gtk.FILL, gtk.FILL)
    label.show()

    label = gtk.Label('3.5 MB')
    label.set_alignment(0.0, 0.5)
    label.set_selectable(True)
    table.attach(label, 1, 2, 2, 3, gtk.EXPAND | gtk.FILL, gtk.FILL)
    label.show()

    label = gtk.Label('<b>Location:</b>')
    label.set_use_markup(True)
    label.set_alignment(1.0, 0.5)
    table.attach(label, 0, 1, 3, 4, gtk.FILL, gtk.FILL)
    label.show()

    label = gtk.Label('file:///usr/home/bar')
    label.set_alignment(0.0, 0.5)
    label.set_selectable(True)
    table.attach(label, 1, 2, 3, 4, gtk.EXPAND | gtk.FILL, gtk.FILL)
    label.show()



    ###
    ### Permissions
    ###
    table = gtk.Table(4, 5, False)
    table.set_col_spacings(12)
    table.set_row_spacings(6)
    table.set_border_width(6)
    label = gtk.Label('Permissions')
    notebook.append_page(table, label)
    label.show()
    table.show()

    label = gtk.Label('<b>File Owner:</b>')
    label.set_use_markup(True)
    label.set_alignment(1.0, 0.5)
    table.attach(label, 0, 1, 0, 1, gtk.FILL, gtk.FILL)
    label.show()

    label = gtk.Label('Benedikt Meurer (bmeurer)')
    label.set_alignment(0.0, 0.5)
    table.attach(label, 1, 4, 0, 1, gtk.FILL, gtk.FILL)
    label.show()

    label = gtk.Label('<b>File Group:</b>')
    label.set_use_markup(True)
    label.set_alignment(1.0, 0.5)
    table.attach(label, 0, 1, 1, 2, gtk.FILL, gtk.FILL)
    label.show()

    align = gtk.Alignment(0.0, 0.5, 0.0, 0.0)
    table.attach(align, 1, 4, 1, 2, gtk.FILL, gtk.FILL)
    align.show()

    combo = gtk.combo_box_new_text()
    combo.append_text('staff')
    combo.append_text('users')
    combo.append_text('wheel')
    combo.append_text('www')
    combo.set_active(1)
    align.add(combo)
    combo.show()

    sep = gtk.HSeparator()
    table.attach(sep, 0, 4, 2, 3, gtk.EXPAND | gtk.FILL, gtk.FILL)
    sep.show()

    label = gtk.Label('<b>Owner:</b>')
    label.set_use_markup(True)
    label.set_alignment(1.0, 0.5)
    table.attach(label, 0, 1, 3, 4, gtk.FILL, gtk.FILL)
    label.show()

    button = gtk.CheckButton('Read')
    table.attach(button, 1, 2, 3, 4, gtk.FILL, gtk.FILL)
    button.show()

    button = gtk.CheckButton('Write')
    table.attach(button, 2, 3, 3, 4, gtk.FILL, gtk.FILL)
    button.show()

    button = gtk.CheckButton('Execute')
    table.attach(button, 3, 4, 3, 4, gtk.FILL, gtk.FILL)
    button.show()

    label = gtk.Label('<b>Group:</b>')
    label.set_use_markup(True)
    label.set_alignment(1.0, 0.5)
    table.attach(label, 0, 1, 4, 5, gtk.FILL, gtk.FILL)
    label.show()

    button = gtk.CheckButton('Read')
    table.attach(button, 1, 2, 4, 5, gtk.FILL, gtk.FILL)
    button.show()

    button = gtk.CheckButton('Write')
    table.attach(button, 2, 3, 4, 5, gtk.FILL, gtk.FILL)
    button.show()

    button = gtk.CheckButton('Execute')
    table.attach(button, 3, 4, 4, 5, gtk.FILL, gtk.FILL)
    button.show()

    label = gtk.Label('<b>Others:</b>')
    label.set_use_markup(True)
    label.set_alignment(1.0, 0.5)
    table.attach(label, 0, 1, 5, 6, gtk.FILL, gtk.FILL)
    label.show()

    button = gtk.CheckButton('Read')
    table.attach(button, 1, 2, 5, 6, gtk.FILL, gtk.FILL)
    button.show()

    button = gtk.CheckButton('Write')
    table.attach(button, 2, 3, 5, 6, gtk.FILL, gtk.FILL)
    button.show()

    button = gtk.CheckButton('Execute')
    table.attach(button, 3, 4, 5, 6, gtk.FILL, gtk.FILL)
    button.show()

    sep = gtk.HSeparator()
    table.attach(sep, 0, 4, 6, 7, gtk.EXPAND | gtk.FILL, gtk.FILL)
    sep.show()

    label = gtk.Label('<b>Special flags:</b>')
    label.set_use_markup(True)
    label.set_alignment(1.0, 0.5)
    table.attach(label, 0, 1, 7, 8, gtk.FILL, gtk.FILL)
    label.show()

    button = gtk.CheckButton('Set user ID')
    table.attach(button, 1, 4, 7, 8, gtk.FILL, gtk.FILL)
    button.show()

    button = gtk.CheckButton('Set group ID')
    table.attach(button, 1, 4, 8, 9, gtk.FILL, gtk.FILL)
    button.show()

    button = gtk.CheckButton('Sticky')
    table.attach(button, 1, 4, 9, 10, gtk.FILL, gtk.FILL)
    button.show()

    sep = gtk.HSeparator()
    table.attach(sep, 0, 4, 10, 11, gtk.EXPAND | gtk.FILL, gtk.FILL)
    sep.show()

    label = gtk.Label('<b>Text view:</b>')
    label.set_use_markup(True)
    label.set_alignment(1.0, 0.5)
    table.attach(label, 0, 1, 11, 12, gtk.FILL, gtk.FILL)
    label.show()

    label = gtk.Label('-rw-r--r--')
    label.set_alignment(0.0, 0.5)
    table.attach(label, 1, 4, 11, 12, gtk.EXPAND | gtk.FILL, gtk.FILL)
    label.show()

    label = gtk.Label('<b>Number view:</b>')
    label.set_use_markup(True)
    label.set_alignment(1.0, 0.5)
    table.attach(label, 0, 1, 12, 13, gtk.FILL, gtk.FILL)
    label.show()

    label = gtk.Label('0644')
    label.set_alignment(0.0, 0.5)
    table.attach(label, 1, 4, 12, 13, gtk.EXPAND | gtk.FILL, gtk.FILL)
    label.show()



    ###
    ### Application
    ###
    vbox = gtk.VBox(False, 6)
    vbox.set_border_width(6)
    label = gtk.Label('Application')
    notebook.append_page(vbox, label)
    label.show()
    vbox.show()

    label = gtk.Label('Select an application to open <i>foo.mp3</i> and all other files of type "MP3 audio"')
    label.set_use_markup(True)
    label.set_line_wrap(True)
    vbox.pack_start(label, False, True, 0)
    label.show()

    model = gtk.ListStore(gobject.TYPE_BOOLEAN, gobject.TYPE_STRING)
    model.append([True, 'Xfce Media Player'])
    model.append([False, 'X Multimedia System'])
    model.append([False, 'beep-media-player'])

    swin = gtk.ScrolledWindow()
    swin.set_policy(gtk.POLICY_NEVER, gtk.POLICY_AUTOMATIC)
    swin.set_shadow_type(gtk.SHADOW_ETCHED_IN)
    vbox.pack_start(swin, True, True, 0)
    swin.show()

    treeview = gtk.TreeView(model)
    treeview.set_headers_visible(False)
    swin.add(treeview)
    treeview.show()
    
    column = gtk.TreeViewColumn()
    renderer = gtk.CellRendererToggle()
    renderer.set_radio(True)
    column.pack_start(renderer, False)
    column.add_attribute(renderer, 'active', 0)
    renderer = gtk.CellRendererText()
    column.pack_start(renderer, True)
    column.add_attribute(renderer, 'text', 1)
    treeview.append_column(column)

    box = gtk.HButtonBox()
    box.set_layout(gtk.BUTTONBOX_END)
    box.set_spacing(6)
    vbox.pack_start(box, False, False, 6)
    box.show()

    button = gtk.Button(None, gtk.STOCK_ADD)
    box.pack_start(button, False, False, 0)
    button.show()

    button = gtk.Button(None, gtk.STOCK_REMOVE)
    box.pack_start(button, False, False, 0)
    button.show()


