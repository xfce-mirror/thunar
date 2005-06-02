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

import dircache, pwd, os, stat, time

import pygtk
pygtk.require('2.0')
import gobject
import gtk

import ThunarImageLoader

import ThunarMimeDatabase

class ThunarFileInfo(gobject.GObject):
    def __init__(self, path):
        gobject.GObject.__init__(self)
        self.mimedb = ThunarMimeDatabase.get_default()

        # build up a normalized path
        self.path = ''
        for name in path.split('/'):
            if name:
                self.path += '/' + name
        if not self.path:
            self.path = '/'

        try:
            self.stat = os.stat(self.path)
        except OSError:
            self.stat = os.lstat(self.path)


    def __try_thumbnail(self, size):
        import md5
        uri = 'file://' + self.path
        name = md5.new(uri).hexdigest() + '.png'
        path = os.path.join(os.getenv('HOME'), '.thumbnails', 'normal', name)
        return gtk.gdk.pixbuf_new_from_file_at_size(path, size, size)


    def render_icon(self, size):
        loader = ThunarImageLoader.get_default()
        icon = None
        try:
            if self.is_home(): icon = loader.load_icon('gnome-fs-home', size)
        except:
            pass
        try:
            if not icon and self.is_desktop(): icon = loader.load_icon('gnome-fs-desktop', size)
        except:
            pass
        try:
            if not icon and self.path == '/': icon = loader.load_icon('gnome-dev-harddisk', size)
        except:
            pass
        if not icon:
            try:
                icon = self.__try_thumbnail(size)
            except:
                pass
        if not icon:
            icon = self.get_mime_info().render_icon(size)
        return icon

    def is_home(self):
        home = pwd.getpwuid(os.getuid()).pw_dir
        return os.path.samefile(home, self.path)

    def is_desktop(self):
        home = pwd.getpwuid(os.getuid()).pw_dir
        return os.path.samefile(os.path.join(home, 'Desktop'), self.path)

    def get_mime_info(self):
        if stat.S_ISDIR(self.stat[stat.ST_MODE]):
            return self.mimedb.get_info("inode/directory")
        elif stat.S_ISCHR(self.stat[stat.ST_MODE]):
            return self.mimedb.get_info("inode/chardevice")
        elif stat.S_ISBLK(self.stat[stat.ST_MODE]):
            return self.mimedb.get_info("inode/blockdevice")
        elif stat.S_ISFIFO(self.stat[stat.ST_MODE]):
            return self.mimedb.get_info("inode/fifo")
        elif stat.S_ISSOCK(self.stat[stat.ST_MODE]):
            return self.mimedb.get_info("inode/socket")
        return self.mimedb.match(self.path)

    def get_name(self):
        name = os.path.basename(self.path)
        if not name:
            name = '/'
        return name

    def get_path(self):
        return self.path

    def get_visible_name(self):
        name = self.get_name()
        if name == '/':
            name = 'Filesystem'
        return name

    def get_size(self):
        if self.is_directory():
            return '-'
        return _humanize_size(self.stat[stat.ST_SIZE])

    def get_full_size(self):
        result = ''
        size = self.stat[stat.ST_SIZE]
        while size > 0:
            if result != '':
                result = ',' + result
            if size > 1000:
                result = '%03d%s' % ((size % 1000), result)
            else:
                result = '%d%s' % ((size % 1000), result)
            size = size / 1000
        return '%s (%s Bytes)' % (_humanize_size(self.stat[stat.ST_SIZE]), result)

    def get_atime(self):
        return time.strftime('%x %X', time.localtime(self.stat[stat.ST_ATIME]))

    def get_ctime(self):
        return time.strftime('%x %X', time.localtime(self.stat[stat.ST_CTIME]))

    def get_mtime(self):
        return time.strftime('%x %X', time.localtime(self.stat[stat.ST_MTIME]))

    def is_directory(self):
        return stat.S_ISDIR(self.stat[stat.ST_MODE])

    def get_permissions_mode(self):
        return self.stat[stat.ST_MODE]

    def get_permissions(self):
        from stat import S_ISDIR, S_IMODE
        mode = self.stat[stat.ST_MODE]
        if S_ISDIR(mode):   result = 'd'
        else:               result = '-'
        mode = S_IMODE(mode)
        if mode & 0400: result += 'r'
        else:           result += '-'
        if mode & 0200: result += 'w'
        else:           result += '-'
        if mode & 0100: result += 'x'
        else:           result += '-'
        if mode & 0040: result += 'r'
        else:           result += '-'
        if mode & 0020: result += 'w'
        else:           result += '-'
        if mode & 0010: result += 'x'
        else:           result += '-'
        if mode & 0004: result += 'r'
        else:           result += '-'
        if mode & 0002: result += 'w'
        else:           result += '-'
        if mode & 0001: result += 'x'
        else:           result += '-'
        return result

    def get_parent(self):
        if self.path == '/':
            return None
        else:
            return ThunarFileInfo(self.path[0:self.path.rfind('/')])



def _humanize_size(size):
    if size > 1024 * 1024 * 1024:
        return '%.1f GB' % (size / (1024 * 1024 * 1024))
    elif size > 1024 * 1024:
        return '%.1f MB' % (size / (1024 * 1024))
    elif size > 1024:
        return '%.1f KB' % (size / 1024)
    else:
        return '%d B' % size



