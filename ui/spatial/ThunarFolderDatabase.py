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

import xml.dom.minidom

db = None
def get_folder_db():
    global db
    if not db:
        db = ThunarFolderDatabase()
    return db


class ThunarFolderProperties:
    def __init__(self, geometry, iconview):
        self.geometry = geometry
        self.iconview = iconview


class ThunarFolderDatabase:
    def __init__(self):
        self.folders = {}

        try:
            self._load()
        except:
            pass


    def lookup(self, path):
        if self.folders.has_key(path):
            return self.folders[path]
        return None


    def insert(self, path, geometry, iconview):
        if self.folders.has_key(path):
            del self.folders[path]
        self.folders[path] = ThunarFolderProperties(geometry, iconview)


    def sync(self):
        fp = open('folders.xml', 'w')
        fp.write("""<?xml version="1.0" encoding="UTF-8"?>
<folders>
""")
        for key in self.folders.keys():
            folder = self.folders[key]
            fp.write('<folder path="%s" geometry="%s" iconview="%s" />' \
                % (key, folder.geometry, int(folder.iconview)))

        fp.write("""
</folders>
""")
        fp.close()


    def _load(self):
        doc = xml.dom.minidom.parse('folders.xml')
        assert doc.documentElement.tagName == 'folders'
        for node in doc.getElementsByTagName('folder'):
            path = '%s' % node.getAttribute('path')
            geometry = '%s' % node.getAttribute('geometry')
            iconview = int(node.getAttribute('iconview'))
            self.folders[path] = ThunarFolderProperties(geometry, iconview)

