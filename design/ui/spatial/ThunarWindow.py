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

import dircache, os
import weakref, gc
import shelve, pickle

import pygtk
pygtk.require('2.0')
import gobject
import gtk

from ThunarFileInfo import ThunarFileInfo
from ThunarFolderDatabase import get_folder_db
from ThunarListView import ThunarListView
from ThunarPropertiesDialog import ThunarPropertiesDialog
from ThunarStatusBar import ThunarStatusBar

# the icon view requires libexo-0.3
try:
    from ThunarIconView import ThunarIconView
    icon_view_support = True
except ImportError:
    icon_view_support = False

windows = weakref.WeakValueDictionary()

class ThunarWindow(gtk.Window):
    def _destroyed(self):
        del windows[self.dir_info.get_path()] 
        gc.collect()
        if len(windows) == 0:
            gtk.main_quit()


    def _save_state(self):
        db = get_folder_db()
        x, y = self.get_position()
        w, h = self.get_size()
        geometry = '%sx%s+%s+%s' % (w, h, x, y)
        iconview = self.action_group.get_action('view-as-icons').get_active()
        db.insert(self.dir_info.get_path(), geometry, iconview)
        folder = db.lookup(self.dir_info.get_path())


    def __init__(self, dir_info, use_icon_view = True):
        gtk.Window.__init__(self, gtk.WINDOW_TOPLEVEL)
        self.set_icon(dir_info.render_icon(48))
        self.connect('destroy', lambda self: self._destroyed())

        global windows
        self.dir_info = dir_info
        windows[dir_info.get_path()] = self

        self.set_title(dir_info.get_visible_name())

        foldersdb = get_folder_db()
        folder = foldersdb.lookup(dir_info.get_path())
        if folder:
            use_icon_view = folder.iconview
            self.set_geometry_hints(None, -1, -1, -1, -1, 0, 0, 1, 1)
            self.parse_geometry(folder.geometry)
        else:
            self.set_default_size(400,350)

        self.action_group = gtk.ActionGroup('thunar-window')
        self.action_group.add_actions([
            ('file-menu', None, '_File'),
            ('open-parent', None, 'Open _Parent', '<Alt>Up', None, lambda ign, self: self._action_open_parent()),
            ('open-location', None, 'Open _Location', '<Ctrl>L', None, lambda ign, self: self._action_open_location()),
            ('get-info', gtk.STOCK_PROPERTIES, 'Get _Info...', '<Alt>Return', None, lambda ign, self: self._action_get_info()),
            ('close-parent-folders', None, 'Close P_arent Folders', '<Control><Shift>W', None, lambda ign, self: self._action_close_parent_folders()),
            ('close-all-folders', None, 'Clos_e All Folders', '<Control>Q', None, lambda ign, self: gtk.main_quit()),
            ('close-window', gtk.STOCK_CLOSE, '_Close', '<Control>W', None, lambda ign, self: self.destroy()),
        ], self)
        self.action_group.add_actions([
            ('view-menu', None, '_View'),
            ('select-all', None, 'Select _All', '<Control>A', None, lambda ign, self: self.view.select_all()),
        ], self)
        self.action_group.add_radio_actions([
            ('view-as-icons', None, 'View as _Icons'),
            ('view-as-list', None, 'View as _List'),
        ], 0, lambda action, whatever, self: self._view_toggled(), self)
        self.action_group.add_actions([
            ('help-menu', None, '_Help'),
            ('contents', gtk.STOCK_HELP, '_Contents', 'F1'),
            ('report-bug', None, '_Report bug'),
            ('about', gtk.STOCK_DIALOG_INFO, '_About'),
        ], self)

        self.ui_manager = gtk.UIManager()
        self.ui_manager.insert_action_group(self.action_group, 0)
        self.ui_manager.add_ui_from_file('thunar.ui')
        self.add_accel_group(self.ui_manager.get_accel_group())

        self.action_group.get_action('get-info').set_property('sensitive', False)
        self.action_group.get_action('open-parent').set_property('sensitive', dir_info.get_parent() != None)

        main_vbox = gtk.VBox(False, 0)
        self.add(main_vbox)
        main_vbox.show()

        menu_bar = self.ui_manager.get_widget('/main-menu')
        main_vbox.pack_start(menu_bar, False, False, 0)
        menu_bar.show()

        self.swin = gtk.ScrolledWindow(None, None)
        self.swin.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        main_vbox.pack_start(self.swin, True, True, 0)
        self.swin.show()

        if icon_view_support and use_icon_view:
            self.view = ThunarIconView(dir_info)
            self.action_group.get_action('view-as-icons').set_active(True)
        else:
            self.view = ThunarListView(dir_info)
            self.action_group.get_action('view-as-list').set_active(True)
        if not icon_view_support:
            self.action_group.get_action('view-as-icons').set_property('sensitive', False)
        self.view.connect('context-menu', lambda view: self._context_menu())
        self.view.connect('activated', lambda widget, info: self.open_dir(info))
        self.view.connect('selection-changed', lambda widget: self._selection_changed())
        self.swin.add(self.view)
        self.view.show()

        self.status_bar = ThunarStatusBar(dir_info)
        self.status_bar.connect('activated', lambda widget, info: self.open_dir(info))
        main_vbox.pack_start(self.status_bar, False, False, 0)
        self.status_bar.show()
        self.status_id = self.status_bar.get_context_id('Selection state')

        # get initial state right
        self._selection_changed()


        self.connect('unrealize', lambda self: self._save_state())
        self.connect_after('configure-event', lambda self, ev: self._save_state())


    def _action_close_parent_folders(self):
        dir_info = self.dir_info.get_parent()
        while dir_info:
            if windows.has_key(dir_info.get_path()):
                windows[dir_info.get_path()].destroy()
            dir_info = dir_info.get_parent()


    def _action_get_info(self):
        infos = self.view.get_selected_files()
        for info in infos:
            dialog = ThunarPropertiesDialog(self, info)
            dialog.show()

    
    def _action_open_parent(self):
        dir_info = self.dir_info.get_parent()
        if dir_info:
            self.open_dir(dir_info)


    def _action_open_location(self):
        dialog = gtk.Dialog('Open Location', self, gtk.DIALOG_DESTROY_WITH_PARENT | gtk.DIALOG_NO_SEPARATOR | gtk.DIALOG_MODAL, (gtk.STOCK_CANCEL, gtk.RESPONSE_CANCEL, gtk.STOCK_OPEN, gtk.RESPONSE_OK))
        dialog.set_default_response(gtk.RESPONSE_OK)
        dialog.set_default_size(390, 50)

        hbox = gtk.HBox(False, 12)
        hbox.set_border_width(12)
        dialog.vbox.pack_start(hbox, True, True, 0)
        hbox.show()

        label = gtk.Label('Location:')
        hbox.pack_start(label, False, False, 0)
        label.show()

        entry = gtk.Entry()
        entry.set_text(self.dir_info.get_path())
        entry.set_activates_default(True)
        hbox.pack_start(entry)
        entry.show()

        response = dialog.run()
        if response == gtk.RESPONSE_OK:
            try:
                info = ThunarFileInfo(entry.get_text())
                self.open_dir(info)
            except:
                pass
        dialog.destroy()


    def _selection_changed(self):
        infos = self.view.get_selected_files()
        self.action_group.get_action('get-info').set_property('sensitive', len(infos) > 0)

        self.status_bar.pop(self.status_id)
        if infos:
            self.status_bar.push(self.status_id, '%d items selected' % len(infos))
        else:
            model = self.view.get_model()
            self.status_bar.push(self.status_id, '%d items' % model.iter_n_children(None))


    def _fill_submenu(self, menu, info):
        if menu.get_children():
            return

        item = gtk.MenuItem('Open this folder')
        item.connect('activate', lambda widget: self.open_dir(info))
        menu.append(item)
        item.show()

        separator = None

        names = dircache.listdir(info.get_path())[:]
        names.sort()
        for name in names:
            if name.startswith('.'):
                continue

            child = ThunarFileInfo(os.path.join(info.get_path(), name))
            if not child.is_directory():
                continue

            if not separator:
                separator = gtk.SeparatorMenuItem()
                menu.append(separator)
                separator.show()

            item = self._create_dir_item(child)
            menu.append(item)
            item.show()


    def _create_dir_item(self, info):
        item = gtk.ImageMenuItem()
        label = gtk.Label(info.get_visible_name())
        label.set_alignment(0.0, 0.5)
        item.add(label)
        label.show()
        image = gtk.Image()
        image.set_from_pixbuf(info.render_icon(16))
        image.show()
        item.set_image(image)

        if info.is_directory():
            submenu = gtk.Menu()
            item.set_submenu(submenu)
            submenu.connect('show', lambda menu: self._fill_submenu(menu, info))

        return item


    def _context_menu(self):
        files = self.view.get_selected_files()
        menu = gtk.Menu()
        if len(files) == 1:
            info = files[0]

            if info.is_directory():
                item = self._create_dir_item(info)
                menu.append(item)
                item.show()

                item = gtk.SeparatorMenuItem()
                menu.append(item)
                item.show()

            item = gtk.MenuItem('Rename')
            item.set_sensitive(False)
            menu.append(item)
            item.show()

            item = gtk.SeparatorMenuItem()
            menu.append(item)
            item.show()

        item = self.action_group.get_action('get-info').create_menu_item()
        menu.append(item)
        item.show()

        menu.popup(None, None, None, 3, gtk.get_current_event_time())


    def _view_toggled(self):
        other = self.swin.get_child()
        if not other:
            return
        self.view = None
        other.destroy()
        if self.action_group.get_action('view-as-icons').get_active():
            self.view = ThunarIconView(self.dir_info)
        else:
            self.view = ThunarListView(self.dir_info)
        self.view.connect('context-menu', lambda view: self._context_menu())
        self.view.connect('activated', lambda widget, info: self.open_dir(info))
        self.view.connect('selection-changed', lambda widget: self._selection_changed())
        self.swin.add(self.view)
        self.view.show()

        self._save_state()


    def open_dir(self, dir_info):
        gc.collect()
        try:
            window = windows[dir_info.get_path()]
            window.present()
        except:
            window = ThunarWindow(dir_info, self.action_group.get_action('view-as-icons').get_active())
            window.show()
