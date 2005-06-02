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
from ThunarParentSelector import ThunarParentSelector

signals_registered = False

class ThunarStatusBar(gtk.Statusbar):
    def __init__(self, dir_info):
        gtk.Statusbar.__init__(self)

        # register signals
        global signals_registered
        if not signals_registered:
            gobject.signal_new('activated', self, gobject.SIGNAL_RUN_LAST, \
                               gobject.TYPE_NONE, [ThunarFileInfo])
            signals_registered = True

        frame = gtk.Frame()
        frame.set_border_width(0)
        shadow_type = self.style_get_property('shadow_type')
        frame.set_property('shadow_type', shadow_type)
        self.pack_start(frame, False, False, 0)
        self.reorder_child(frame, 0)
        frame.show()

        selector = ThunarParentSelector(dir_info)
        selector.connect('activated', lambda widget, info: self.emit('activated', info))
        frame.add(selector)
        selector.show()
