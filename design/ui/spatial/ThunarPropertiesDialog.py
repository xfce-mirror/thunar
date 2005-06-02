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

class ThunarPropertiesDialog(gtk.Dialog):
    def __init__(self, parent, info):
        gtk.Dialog.__init__(self, info.get_visible_name() + ' Info', parent,
                            gtk.DIALOG_DESTROY_WITH_PARENT,
                            (gtk.STOCK_HELP, gtk.RESPONSE_HELP,
                             gtk.STOCK_CLOSE, gtk.RESPONSE_CLOSE))
        self.set_has_separator(False)
        self.set_icon(info.render_icon(48))
        self.connect('response', lambda self, response: self.destroy())

        tooltips = gtk.Tooltips()

        notebook = gtk.Notebook()
        self.vbox.pack_start(notebook, True, True, 0)
        notebook.show()


        ###
        ### General
        ###
        table = gtk.Table(2, 4, False)
        table.set_col_spacings(12)
        table.set_row_spacings(6)
        table.set_border_width(6)
        label = gtk.Label('General')
        notebook.append_page(table, label)
        label.show()
        table.show()

        row = 0

        ### {
        box = gtk.HBox(False, 6)
        table.attach(box, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        box.show()

        image = gtk.Image()
        image.set_from_pixbuf(info.render_icon(48))
        image.set_alignment(0.5, 0.5)
        box.pack_start(image, False, True, 0)
        image.show()

        label = gtk.Label('<b>Name:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        box.pack_start(label, True, True, 0)
        label.show()

        entry = gtk.Entry()
        entry.set_text(info.get_visible_name())
        table.attach(entry, 1, 2, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
        entry.show()
        row += 1 ### }

        ### {
        align = gtk.Alignment(1.0, 1.0, 1.0, 1.0)
        align.set_size_request(-1, 12)
        table.attach(align, 0, 1, row, row + 1, 0, 0)
        align.show()
        row += 1 ### }

        ### {
        label = gtk.Label('<b>Kind:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        label = gtk.Label(info.get_mime_info().get_comment())
        label.set_alignment(0.0, 0.5)
        label.set_selectable(True)
        table.attach(label, 1, 2, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
        label.show()
        row += 1 ### }

        if not info.is_directory():
            ### {
            label = gtk.Label('<b>Open With:</b>')
            label.set_use_markup(True)
            label.set_alignment(1.0, 0.5)
            table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
            label.show()

            hbox = gtk.HBox(False, 6)
            table.attach(hbox, 1, 2, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
            hbox.show()

            image = gtk.image_new_from_stock(gtk.STOCK_CDROM, gtk.ICON_SIZE_MENU)
            hbox.pack_start(image, False, False, 0)
            image.show()

            label = gtk.Label('Xfce Media Player')
            label.set_alignment(0.0, 0.5)
            label.set_selectable(True)
            hbox.pack_start(label, False, True, 0)
            label.show()

            button = gtk.Button('Ch_ange')
            hbox.pack_end(button, False, False, 0)
            button.show()
            row += 1 ### }

        ### {
        align = gtk.Alignment(1.0, 1.0, 1.0, 1.0)
        align.set_size_request(-1, 12)
        table.attach(align, 0, 1, row, row + 1, 0, 0)
        align.show()
        row += 1 ### }

        ### {
        label = gtk.Label('<b>Location:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        label = gtk.Label(info.get_parent().get_visible_name())
        label.set_alignment(0.0, 0.5)
        label.set_selectable(True)
        table.attach(label, 1, 2, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
        label.show()
        row += 1 ### }

        if not info.is_directory():
            ### {
            label = gtk.Label('<b>Size:</b>')
            label.set_use_markup(True)
            label.set_alignment(1.0, 0.5)
            table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
            label.show()

            label = gtk.Label(info.get_size())
            label.set_alignment(0.0, 0.5)
            label.set_selectable(True)
            table.attach(label, 1, 2, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
            label.show()
            row += 1 ### }

        ### {
        label = gtk.Label('<b>Permissions:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        label = gtk.Label(info.get_permissions())
        label.set_alignment(0.0, 0.5)
        label.set_selectable(True)
        table.attach(label, 1, 2, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
        label.show()
        row += 1 ### }

        ### {
        align = gtk.Alignment(1.0, 1.0, 1.0, 1.0)
        align.set_size_request(-1, 12)
        table.attach(align, 0, 1, row, row + 1, 0, 0)
        align.show()
        row += 1 ### }

        ### {
        label = gtk.Label('<b>Modified:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        label = gtk.Label(info.get_mtime())
        label.set_alignment(0.0, 0.5)
        label.set_selectable(True)
        table.attach(label, 1, 2, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
        label.show()
        row += 1 ### }

        ### {
        label = gtk.Label('<b>Accessed:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        label = gtk.Label(info.get_atime())
        label.set_alignment(0.0, 0.5)
        label.set_selectable(True)
        table.attach(label, 1, 2, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
        label.show()
        row += 1 ### }



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

        row = 0

        ### {
        label = gtk.Label('<b>File Owner:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        label = gtk.Label('Benedikt Meurer (bmeurer)')
        label.set_alignment(0.0, 0.5)
        table.attach(label, 1, 4, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()
        row += 1 ### }

        ### {
        label = gtk.Label('<b>File Group:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        align = gtk.Alignment(0.0, 0.5, 0.0, 0.0)
        table.attach(align, 1, 4, row, row + 1, gtk.FILL, gtk.FILL)
        align.show()

        combo = gtk.combo_box_new_text()
        combo.append_text('staff')
        combo.append_text('users')
        combo.append_text('wheel')
        combo.append_text('www')
        combo.set_active(1)
        align.add(combo)
        combo.show()
        row += 1 ### }

        ### {
        sep = gtk.HSeparator()
        table.attach(sep, 0, 4, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
        sep.show()
        row += 1 ### }

        ### {
        label = gtk.Label('<b>Owner:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        button = gtk.CheckButton('Read')
        table.attach(button, 1, 2, row, row + 1, gtk.FILL, gtk.FILL)
        button.show()

        button = gtk.CheckButton('Write')
        table.attach(button, 2, 3, row, row + 1, gtk.FILL, gtk.FILL)
        button.show()

        button = gtk.CheckButton('Execute')
        table.attach(button, 3, 4, row, row + 1, gtk.FILL, gtk.FILL)
        button.show()
        row += 1 ### }

        ### {
        label = gtk.Label('<b>Group:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        button = gtk.CheckButton('Read')
        table.attach(button, 1, 2, row, row + 1, gtk.FILL, gtk.FILL)
        button.show()

        button = gtk.CheckButton('Write')
        table.attach(button, 2, 3, row, row + 1, gtk.FILL, gtk.FILL)
        button.show()

        button = gtk.CheckButton('Execute')
        table.attach(button, 3, 4, row, row + 1, gtk.FILL, gtk.FILL)
        button.show()
        row += 1 ### }

        ### {
        label = gtk.Label('<b>Others:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        button = gtk.CheckButton('Read')
        table.attach(button, 1, 2, row, row + 1, gtk.FILL, gtk.FILL)
        button.show()

        button = gtk.CheckButton('Write')
        table.attach(button, 2, 3, row, row + 1, gtk.FILL, gtk.FILL)
        button.show()

        button = gtk.CheckButton('Execute')
        table.attach(button, 3, 4, row, row + 1, gtk.FILL, gtk.FILL)
        button.show()
        row += 1 ### }

        ### {
        sep = gtk.HSeparator()
        table.attach(sep, 0, 4, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
        sep.show()
        row += 1 ### }
     
        ### {
        label = gtk.Label('<b>Special flags:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        button = gtk.CheckButton('Set user ID')
        table.attach(button, 1, 4, row, row + 1, gtk.FILL, gtk.FILL)
        button.show()
        row += 1 ### }

        ### {
        button = gtk.CheckButton('Set group ID')
        table.attach(button, 1, 4, row, row + 1, gtk.FILL, gtk.FILL)
        button.show()
        row += 1 ### }

        ### {
        button = gtk.CheckButton('Sticky')
        table.attach(button, 1, 4, row, row + 1, gtk.FILL, gtk.FILL)
        button.show()
        row += 1 ### }

        ### {
        sep = gtk.HSeparator()
        table.attach(sep, 0, 4, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
        sep.show()
        row += 1 ### }

        ### {
        label = gtk.Label('<b>Text view:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        label = gtk.Label('-rw-r--r--')
        label.set_alignment(0.0, 0.5)
        table.attach(label, 1, 4, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
        label.show()
        row += 1 ### }

        ### {
        label = gtk.Label('<b>Number view:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        label = gtk.Label('0644')
        label.set_alignment(0.0, 0.5)
        table.attach(label, 1, 4, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
        label.show()
        row += 1 ### }

        ### {
        label = gtk.Label('<b>Last changed:</b>')
        label.set_use_markup(True)
        label.set_alignment(1.0, 0.5)
        table.attach(label, 0, 1, row, row + 1, gtk.FILL, gtk.FILL)
        label.show()

        label = gtk.Label('2005-02-15 17:55')
        label.set_alignment(0.0, 0.5)
        table.attach(label, 1, 4, row, row + 1, gtk.EXPAND | gtk.FILL, gtk.FILL)
        label.show()
        row += 1 ### }
