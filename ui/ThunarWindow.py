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

import os

import pygtk
pygtk.require('2.0')
import gtk

from ThunarModel import ThunarModel
from ThunarPathBar import ThunarPathBar
from ThunarFileInfo import ThunarFileInfo
from ThunarListView import ThunarListView
from ThunarSidePane import ThunarSidePane
from ThunarPropertiesDialog import ThunarPropertiesDialog

# the icon view requires libexo-0.3
try:
    from ThunarIconView import ThunarIconView
    icon_view_support = True
except ImportError:
    icon_view_support = False

class ThunarWindow(gtk.Window):
    def __init__(self, info):
        gtk.Window.__init__(self, gtk.WINDOW_TOPLEVEL)
        self.set_title(info.get_path())
        self.set_icon(info.render_icon(48))
        self.set_default_size(650,550)


        set = gtk.IconSet(gtk.gdk.pixbuf_new_from_file('shortcuts.png'))
        factory = gtk.IconFactory()
        factory.add('thunar-shortcuts', set)
        factory.add_default()

        self.bookmarks = []

        self.info = info

        self.action_group = gtk.ActionGroup('thunar-window')
        self.action_group.add_actions([
            ('file-menu', None, '_File'),
            ('get-info', gtk.STOCK_PROPERTIES, 'Get _Info...', '<Alt>Return', None, lambda ign, self: self._action_get_info()),
            ('close-window', gtk.STOCK_CLOSE, '_Close', '<Control>W', None, lambda ign, self: self.destroy()),
        ], self)
        self.action_group.add_actions([
            ('edit-menu', None, '_Edit'),
            ('copy-files', gtk.STOCK_COPY, '_Copy Files', '<Control>C'),
            ('cut-files', gtk.STOCK_CUT, 'Cu_t Files', '<Control>X'),
            ('paste-files', gtk.STOCK_PASTE, '_Paste Files', '<Control>V'),
            ('select-all-files', None, 'Select _All Files', '<Control>S', None, lambda ign, self: self.view.select_all()),
            ('select-by-pattern', None, 'Select by Pattern', None),
            ('edit-toolbars', None, '_Edit Toolbars...', None),
            ('preferences', gtk.STOCK_PREFERENCES, 'Prefere_nces...', None),
        ], self)
        self.action_group.add_actions([
            ('view-menu', None, '_View'),
            ('view-sidebar-menu', None, '_Sidebar'),
            ('reload', gtk.STOCK_REFRESH, '_Reload', '<Control>R', None, lambda ign, self: self._internal_open_dir(self.info)),
        ], self)
        self.action_group.add_radio_actions([
            ('sidebar-tree', gtk.STOCK_OPEN, 'Tree', None, 'Display the tree pane on the left', 1),
            ('sidebar-shortcuts', 'thunar-shortcuts', 'Shortcuts', None, 'Display the shortcuts on the left', 2),
            ('sidebar-disabled', None, 'Hidden', None, 'Hide the sidebar', 3),
        ], 2, lambda action, whatever, self: self._action_sidebar_toggled(), self)
        self.action_group.add_toggle_actions([
            ('view-gtkfilechooser', None, 'GtkFileChooser-like', None, None, lambda ign, self: self._action_gtkfilechooser_like(), True),
            ('view-toolbars', None, 'Show Toolbars', None, None, lambda ign, self: self._action_show_toolbars(), False),
        ], self)
        self.action_group.add_radio_actions([
            ('view-as-icons', None, 'View as _Icons', None, None, 1),
            ('view-as-list', None, 'View as _List'),
        ], 1, lambda action, whatever, self: self._action_view_toggled(), self)
        self.action_group.add_actions([
            ('go-menu', None, '_Go'),
            ('go-up', gtk.STOCK_GO_UP, '_Up', '<Alt>Up', None, lambda ign, self: self._action_open_dir(self.info.get_parent())),
            ('go-back', gtk.STOCK_GO_BACK, '_Back', '<Alt>Left', None, lambda ign, self: self._action_go_back()),
            ('go-forward', gtk.STOCK_GO_FORWARD, '_Forward', '<Alt>Right', None, lambda ign, self: self._action_go_forward()),
            ('go-home', gtk.STOCK_HOME, '_Home Folder', None, None, lambda ign, self: self._action_open_dir(ThunarFileInfo(os.getenv('HOME')))),
            ('go-location', None, '_Location...', '<Control>L', None, lambda ign, self: self._action_open_location()),
        ], self)
        self.action_group.add_actions([
            ('bookmarks-menu', None, '_Bookmarks'),
            ('add-bookmark', gtk.STOCK_ADD, '_Add Bookmark', '<Control>D', None, lambda ign, self: self._action_add_bookmark()),
        ], self)
        self.action_group.add_actions([
            ('help-menu', None, '_Help'),
            ('contents', gtk.STOCK_HELP, '_Contents', 'F1'),
            ('report-bug', None, '_Report bug'),
            ('about', gtk.STOCK_DIALOG_INFO, '_About'),
        ], self)

        self.action_group.get_action('sidebar-tree').set_property('is-important', True)
        self.action_group.get_action('sidebar-shortcuts').set_property('is-important', True)

        self.action_group.get_action('copy-files').set_property('sensitive', False)
        self.action_group.get_action('cut-files').set_property('sensitive', False)
        self.action_group.get_action('paste-files').set_property('sensitive', False)
        self.action_group.get_action('select-by-pattern').set_property('sensitive', False)
        self.action_group.get_action('edit-toolbars').set_property('sensitive', False)
        self.action_group.get_action('preferences').set_property('sensitive', False)
        self.action_group.get_action('go-back').set_property('sensitive', False)
        self.action_group.get_action('go-forward').set_property('sensitive', False)
        self.action_group.get_action('contents').set_property('sensitive', False)
        self.action_group.get_action('report-bug').set_property('sensitive', False)
        self.action_group.get_action('about').set_property('sensitive', False)

        self.ui_manager = gtk.UIManager()
        self.ui_manager.insert_action_group(self.action_group, 0)
        self.ui_manager.add_ui_from_file('thunar.ui')
        self.add_accel_group(self.ui_manager.get_accel_group())

        self.main_vbox = gtk.VBox(False, 0)
        self.add(self.main_vbox)
        self.main_vbox.show()

        menu_bar = self.ui_manager.get_widget('/main-menu')
        self.main_vbox.pack_start(menu_bar, False, False, 0)
        menu_bar.show()

        tool_bar = self.ui_manager.get_widget('/main-toolbar')
        tool_bar.set_style(gtk.TOOLBAR_BOTH_HORIZ)
        self.main_vbox.pack_start(tool_bar, False, False, 0)
        tool_bar.hide()

        self.main_hbox = gtk.HPaned()
        self.main_hbox.set_border_width(6)
        self.main_vbox.pack_start(self.main_hbox, True, True, 0)
        self.main_hbox.show()

        self.sidepane = ThunarSidePane()
        self.main_hbox.set_position(self.sidepane.size_request()[0])
        self.sidepane.select_by_info(self.info)
        self.sidepane.set_gtkfilechooser_like(True)
        self.sidepane_selection_id = self.sidepane.connect('directory-changed', lambda ign, info: self._action_open_dir(info))
        self.sidepane_selection_id = self.sidepane.connect('hide-sidepane', lambda ign: self.action_group.get_action('sidebar-disabled').activate())
        self.main_hbox.pack1(self.sidepane, False, False)
        self.sidepane.show()

        vbox = gtk.VBox(False, 6)
        self.main_hbox.pack2(vbox, True, False)
        vbox.show()

        self.pathbar = ThunarPathBar()
        self.pathbar.set_info(self.info)
        self.pathbar.connect('directory-changed', lambda history, info: self._action_open_dir(info))
        vbox.pack_start(self.pathbar, False, False, 0)
        self.pathbar.show()

        self.swin = gtk.ScrolledWindow(None, None)
        self.swin.set_policy(gtk.POLICY_AUTOMATIC, gtk.POLICY_AUTOMATIC)
        self.swin.set_shadow_type(gtk.SHADOW_IN)
        vbox.pack_start(self.swin, True, True, 0)
        self.swin.show()

        if icon_view_support:
            self.view = ThunarIconView(self.info)
            self.action_group.get_action('view-as-icons').set_active(True)
        else:
            self.view = ThunarListView(self.info)
            self.action_group.get_action('view-as-list').set_active(True)
        if not icon_view_support:
            self.action_group.get_action('view-as-icons').set_property('sensitive', False)
        self.view.connect('context-menu', lambda view: self._context_menu())
        self.view.connect('activated', lambda widget, info: self._action_open_dir(info))
        self.view.connect('selection-changed', lambda widget: self._selection_changed())
        self.swin.add(self.view)
        self.view.grab_focus()
        self.view.show()

        self.status_bar = gtk.Statusbar()
        self.status_id = self.status_bar.get_context_id('Selection state')
        self.main_vbox.pack_start(self.status_bar, False, False, 0)
        self.status_bar.show()

        self.back_list = []
        self.forward_list = []

        # get initial state right
        self._selection_changed()


    def _action_view_toggled(self):
        other = self.swin.get_child()
        if not other:
            return
        self.view = None
        other.destroy()
        if self.action_group.get_action('view-as-icons').get_active():
            self.view = ThunarIconView(self.info)
        else:
            self.view = ThunarListView(self.info)
        self.view.connect('context-menu', lambda view: self._context_menu())
        self.view.connect('activated', lambda widget, info: self._action_open_dir(info))
        self.view.connect('selection-changed', lambda widget: self._selection_changed())
        self.swin.add(self.view)
        self.view.show()


    def _action_gtkfilechooser_like(self):
        value = self.action_group.get_action('view-gtkfilechooser').get_active()
        self.sidepane.set_gtkfilechooser_like(value)
        if value:
            self.pathbar.show()
            self.main_hbox.set_border_width(6)
        else:
            self.pathbar.hide()
            self.main_hbox.set_border_width(0)


    def _action_sidebar_toggled(self):
        if self.action_group.get_action('sidebar-tree').get_active():
            self.sidepane.set_state(ThunarSidePane.TREE)
        elif self.action_group.get_action('sidebar-shortcuts').get_active():
            self.sidepane.set_state(ThunarSidePane.SHORTCUTS)
        else:
            self.sidepane.set_state(ThunarSidePane.DISABLED)
        self.sidepane.select_by_info(self.info)


    def _action_show_toolbars(self):
        if self.action_group.get_action('view-toolbars').get_active():
            self.ui_manager.get_widget('/main-toolbar').show()
        else:
            self.ui_manager.get_widget('/main-toolbar').hide()


    def _action_get_info(self):
        infos = self.view.get_selected_files()
        for info in infos:
            dialog = ThunarPropertiesDialog(self, info)
            dialog.show()


    def _action_add_bookmark(self):
        menu = self.ui_manager.get_widget('/main-menu/bookmarks-menu').get_submenu()
        if not self.bookmarks:
            item = gtk.SeparatorMenuItem()
            menu.append(item)
            item.show()
        for info in self.bookmarks:
            if info.get_path() == self.info.get_path():
                return
        info = self.info
        self.bookmarks.append(info)
        item = gtk.ImageMenuItem(info.get_visible_name())
        image = gtk.Image()
        image.set_from_pixbuf(info.render_icon(16))
        item.set_image(image)
        item.connect('activate', lambda ign: self._action_open_dir(info))
        image.show()
        menu.append(item)
        item.show()


    def _action_open_dir(self, info):
        self.back_list.append(self.info)
        self.forward_list = []
        self.action_group.get_action('go-back').set_property('sensitive', True)
        self.action_group.get_action('go-forward').set_property('sensitive', False)
        self._internal_open_dir(info)


    def _internal_open_dir(self, info):
        model = ThunarModel(info)
        self.view.set_model(model)
        self.info = info
        self.action_group.get_action('go-up').set_property('sensitive', (info.get_parent() != None))
        self._selection_changed()
        self.set_title(info.get_path())
        self.set_icon(info.render_icon(48))

        self.sidepane.handler_block(self.sidepane_selection_id)
        self.sidepane.select_by_info(info)
        self.sidepane.handler_unblock(self.sidepane_selection_id)

        self.pathbar.set_info(info)

        # scroll to (0,0)
        self.swin.get_hadjustment().set_value(0)
        self.swin.get_vadjustment().set_value(0)


    def _action_go_back(self):
        if not self.back_list:
            return
        info = self.back_list.pop(len(self.back_list) - 1)
        self.forward_list.append(self.info)
        self.action_group.get_action('go-back').set_property('sensitive', (self.back_list != []))
        self.action_group.get_action('go-forward').set_property('sensitive', True)
        self._internal_open_dir(info)


    def _action_go_forward(self):
        if not self.forward_list:
            return
        info = self.forward_list.pop(len(self.forward_list) - 1)
        self.back_list.append(self.info)
        self.action_group.get_action('go-back').set_property('sensitive', True)
        self.action_group.get_action('go-forward').set_property('sensitive', (self.forward_list != []))
        self._internal_open_dir(info)


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
        entry.set_text(self.info.get_path())
        entry.set_activates_default(True)
        hbox.pack_start(entry)
        entry.show()

        response = dialog.run()
        if response == gtk.RESPONSE_OK:
            try:
                info = ThunarFileInfo(entry.get_text())
                self._action_open_dir(info)
            except:
                pass
        dialog.destroy()


    def _action_open_in_new_window(self, info):
        window = ThunarWindow(info)
        window.show()


    def _context_menu(self):
        files = self.view.get_selected_files()
        menu = gtk.Menu()
        if len(files) == 1:
            info = files[0]

            if info.is_directory():
                item = gtk.ImageMenuItem('Open')
                image = gtk.image_new_from_stock(gtk.STOCK_OPEN, gtk.ICON_SIZE_MENU)
                item.set_image(image)
                image.show()
                item.connect('activate', lambda ign: self._action_open_dir(info))
                menu.append(item)
                item.show()

                item = gtk.MenuItem('Open in New Window')
                item.connect('activate', lambda ign: self._action_open_in_new_window(info))
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

        item = gtk.MenuItem('Get _Info...')
        item.connect('activate', lambda ign: self._action_get_info())
        menu.append(item)
        item.show()

        menu.popup(None, None, None, 3, gtk.get_current_event_time())


    def _selection_changed(self):
        infos = self.view.get_selected_files()
        self.action_group.get_action('get-info').set_property('sensitive', len(infos) > 0)
        self.status_bar.pop(self.status_id)
        if infos:
            self.status_bar.push(self.status_id, '%d items selected' % len(infos))
        else:
            model = self.view.get_model()
            self.status_bar.push(self.status_id, '%d items' % model.iter_n_children(None))

