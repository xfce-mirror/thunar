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

signals_registered = False

class ThunarParentSelector(gtk.Button):
    def __init__(self, dir_info):
        gtk.Button.__init__(self)

        # register signals
        global signals_registered
        if not signals_registered:
            gobject.signal_new('activated', self, gobject.SIGNAL_RUN_LAST, \
                               gobject.TYPE_NONE, [ThunarFileInfo])
            signals_registered = True

        self.unset_flags(gtk.CAN_FOCUS)
        self.set_relief(gtk.RELIEF_NONE)
        self.connect('pressed', lambda self: self._selector_clicked())
        self.file_info = dir_info

        box = gtk.HBox(False, 6)
        box.set_border_width(0)
        self.add(box)
        box.show()

        mime_info = self.file_info.get_mime_info()
        self.image = gtk.Image()
        self.image.set_from_pixbuf(dir_info.render_icon(16))
        box.pack_start(self.image, False, False, 0)
        self.image.show()

        self.label = gtk.Label(self.file_info.get_visible_name())
        box.pack_start(self.label, False, False, 0)
        self.label.show()

        arrow = gtk.Arrow(gtk.ARROW_DOWN, gtk.SHADOW_NONE)
        box.pack_start(arrow, False, False, 0)
        arrow.show()


    def _popup_pos(self, menu):
        menu_w, menu_h = menu.size_request()
        self_w, self_h = self.size_request()

        x0, y0 = self.translate_coordinates(self.get_toplevel(), 0, 0)

        x, y = self.window.get_position()

        y -= menu_h - self_h
        y += y0
        x += x0

        return (x, y, True)


    def _selector_clicked(self):
        first = None
        menu = gtk.Menu()
        file_info = self.file_info
        while file_info:
            name = file_info.get_visible_name()
            icon = file_info.render_icon(16)

            image = gtk.Image()
            image.set_from_pixbuf(icon)

            item = gtk.ImageMenuItem(name)
            item.set_image(image)
            item.set_data('info', file_info)
            item.connect('activate', lambda item: self.emit('activated', item.get_data('info')))
            menu.prepend(item)
            item.show()
            if not first: first = item

            file_info = file_info.get_parent()

        loop = gobject.MainLoop()

        menu.connect('deactivate', lambda menu, loop: loop.quit(), loop)

        menu.grab_add()
        menu.popup(None, None, lambda menu: self._popup_pos(menu), 0, gtk.get_current_event_time())
        menu.select_item(first)
        loop.run()
        menu.grab_remove()
        self.released()
