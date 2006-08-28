/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <thunar/thunar-application.h>
#include <thunar/thunar-clipboard-manager.h>
#include <thunar/thunar-create-dialog.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-navigation-bar.h>
#include <thunar/thunar-navigation-bar-ui.h>
#include <thunar/thunar-path-bar.h>
#include <thunar/thunar-path-entry.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-properties-dialog.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SELECTED_FILES,
  PROP_UI_MANAGER,
};



static void           thunar_navigation_bar_class_init                (ThunarNavigationBarClass *klass);
static void           thunar_navigation_bar_component_init            (ThunarComponentIface     *iface);
static void           thunar_navigation_bar_navigator_init            (ThunarNavigatorIface     *iface);
static void           thunar_navigation_bar_init                      (ThunarNavigationBar      *navigation_bar);
static void           thunar_navigation_bar_finalize                  (GObject                  *object);
static void           thunar_navigation_bar_get_property              (GObject                  *object,
                                                                       guint                     prop_id,
                                                                       GValue                   *value,
                                                                       GParamSpec               *pspec);
static void           thunar_navigation_bar_set_property              (GObject                  *object,
                                                                       guint                     prop_id,
                                                                       const GValue             *value,
                                                                       GParamSpec               *pspec);
static ThunarFile    *thunar_navigation_bar_get_current_directory     (ThunarNavigator          *navigator);
static void           thunar_navigation_bar_set_current_directory     (ThunarNavigator          *navigator,
                                                                       ThunarFile               *current_directory);
static GtkUIManager  *thunar_navigation_bar_get_ui_manager            (ThunarComponent          *component);
static void           thunar_navigation_bar_set_ui_manager            (ThunarComponent          *component,
                                                                       GtkUIManager             *ui_manager);
static gboolean       thunar_navigation_bar_reset                     (ThunarNavigationBar      *navigation_bar);
static void           thunar_navigation_bar_action_create_folder      (GtkAction                *action,
                                                                       ThunarNavigationBar      *navigation_bar);
static void           thunar_navigation_bar_action_down_folder        (GtkAction                *action,
                                                                       ThunarNavigationBar      *navigation_bar);
static void           thunar_navigation_bar_action_empty_trash        (GtkAction                *action,
                                                                       ThunarNavigationBar      *navigation_bar);
static void           thunar_navigation_bar_action_open               (GtkAction                *action,
                                                                       ThunarNavigationBar      *navigation_bar);
static void           thunar_navigation_bar_action_open_in_new_window (GtkAction                *action,
                                                                       ThunarNavigationBar      *navigation_bar);
static void           thunar_navigation_bar_action_paste_into_folder  (GtkAction                *action,
                                                                       ThunarNavigationBar      *navigation_bar);
static void           thunar_navigation_bar_action_properties         (GtkAction                *action,
                                                                       ThunarNavigationBar      *navigation_bar);
static void           thunar_navigation_bar_activate                  (GtkWidget                *path_entry,
                                                                       ThunarNavigationBar      *navigation_bar);
static void           thunar_navigation_bar_changed                   (GtkWidget                *path_entry,
                                                                       ThunarNavigationBar      *navigation_bar);
static void           thunar_navigation_bar_context_menu              (ThunarNavigationBar      *navigation_bar,
                                                                       GdkEventButton           *event,
                                                                       ThunarFile               *file);
static void           thunar_navigation_bar_label_size_request        (GtkWidget                *align,
                                                                       GtkRequisition           *requisition,
                                                                       ThunarNavigationBar      *navigation_bar);
static void           thunar_navigation_bar_toggled                   (GtkWidget                *button,
                                                                       ThunarNavigationBar      *navigation_bar);



struct _ThunarNavigationBarClass
{
  GtkHBoxClass __parent__;

  /* internal action signals */
  gboolean (*reset) (ThunarNavigationBar *navigation_bar);
};

struct _ThunarNavigationBar
{
  GtkHBox __parent__;

  ThunarPreferences *preferences;

  GtkActionGroup    *action_group;
  GtkUIManager      *ui_manager;
  guint              ui_merge_id;

  ThunarFile        *current_directory;

  GtkWidget         *button;
  GtkWidget         *path_bar;
  GtkWidget         *path_entry;
  GtkWidget         *path_entry_box;
  GtkWidget         *path_entry_label;

  /* TRUE if the path_entry is shown temporarily */
  gboolean           temporary_path_entry;
};



static const GtkActionEntry action_entries[] =
{
  { "navbar-down-folder", NULL, "Down Folder", "<alt>Down", NULL, G_CALLBACK (thunar_navigation_bar_action_down_folder), },
  { "navbar-context-menu", NULL, "Context Menu", NULL, NULL, NULL, },
  { "navbar-open", GTK_STOCK_OPEN, N_("_Open"), NULL, NULL, G_CALLBACK (thunar_navigation_bar_action_open), },
  { "navbar-open-in-new-window", NULL, N_("Open in New Window"), NULL, NULL, G_CALLBACK (thunar_navigation_bar_action_open_in_new_window), },
  { "navbar-create-folder", NULL, N_("Create _Folder..."), NULL, NULL, G_CALLBACK (thunar_navigation_bar_action_create_folder), },
  { "navbar-empty-trash", NULL, N_("_Empty Trash"), NULL, N_("Delete all files and folders in the Trash"), G_CALLBACK (thunar_navigation_bar_action_empty_trash), },
  { "navbar-paste-into-folder", GTK_STOCK_PASTE, N_("Paste Into Folder"), NULL, NULL, G_CALLBACK (thunar_navigation_bar_action_paste_into_folder), },
  { "navbar-properties", GTK_STOCK_PROPERTIES, N_("_Properties..."), NULL, NULL, G_CALLBACK (thunar_navigation_bar_action_properties), },
};

static GObjectClass *thunar_navigation_bar_parent_class;



GType
thunar_navigation_bar_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarNavigationBarClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_navigation_bar_class_init,
        NULL,
        NULL,
        sizeof (ThunarNavigationBar),
        0,
        (GInstanceInitFunc) thunar_navigation_bar_init,
        NULL,
      };

      static const GInterfaceInfo component_info =
      {
        (GInterfaceInitFunc) thunar_navigation_bar_component_init,
        NULL,
        NULL,
      };

      static const GInterfaceInfo navigator_info =
      {
        (GInterfaceInitFunc) thunar_navigation_bar_navigator_init,
        NULL,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_HBOX, I_("ThunarNavigationBar"), &info, 0);
      g_type_add_interface_static (type, THUNAR_TYPE_NAVIGATOR, &navigator_info);
      g_type_add_interface_static (type, THUNAR_TYPE_COMPONENT, &component_info);
    }

  return type;
}



static void
thunar_navigation_bar_class_init (ThunarNavigationBarClass *klass)
{
  GtkBindingSet *binding_set;
  GObjectClass  *gobject_class;

  /* determine the parent type class */
  thunar_navigation_bar_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_navigation_bar_finalize;
  gobject_class->get_property = thunar_navigation_bar_get_property;
  gobject_class->set_property = thunar_navigation_bar_set_property;

  klass->reset = thunar_navigation_bar_reset;

  /* Override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

  /* Override ThunarComponent's properties */
  g_object_class_override_property (gobject_class, PROP_SELECTED_FILES, "selected-files");
  g_object_class_override_property (gobject_class, PROP_UI_MANAGER, "ui-manager");

  /**
   * ThunarLocationEntry::reset:
   * @location_entry : a #ThunarLocationEntry.
   *
   * Emitted by @location_entry whenever the user requests to
   * reset the @location_entry contents to the current directory.
   * This is an internal signal used to bind the action to keys.
   **/
  g_signal_new (I_("reset"),
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
                G_STRUCT_OFFSET (ThunarNavigationBarClass, reset),
                g_signal_accumulator_true_handled, NULL,
                _thunar_marshal_BOOLEAN__VOID,
                G_TYPE_BOOLEAN, 0);

  /* setup the key bindings for the location entry */
  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0, "reset", 0);
}



static void
thunar_navigation_bar_component_init (ThunarComponentIface *iface)
{
  iface->get_selected_files = (gpointer) exo_noop_null;
  iface->set_selected_files = (gpointer) exo_noop;
  iface->get_ui_manager = thunar_navigation_bar_get_ui_manager;
  iface->set_ui_manager = thunar_navigation_bar_set_ui_manager;
}



static void
thunar_navigation_bar_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_navigation_bar_get_current_directory;
  iface->set_current_directory = thunar_navigation_bar_set_current_directory;
}



static void
thunar_navigation_bar_init (ThunarNavigationBar *navigation_bar)
{
  AtkRelationSet *relations;
  AtkRelation    *relation;
  AtkObject      *object;
  GtkWidget      *align;
  GtkWidget      *image;
  gboolean        active;

  /* connect to the preferences */
  navigation_bar->preferences = thunar_preferences_get ();

  /* setup the action group for the navigation bar */
  navigation_bar->action_group = gtk_action_group_new ("ThunarNavigationBar");
  gtk_action_group_set_translation_domain (navigation_bar->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (navigation_bar->action_group, action_entries, G_N_ELEMENTS (action_entries), navigation_bar);

  gtk_box_set_spacing (GTK_BOX (navigation_bar), 12);

  navigation_bar->button = gtk_toggle_button_new ();
  g_signal_connect (G_OBJECT (navigation_bar->button), "toggled", G_CALLBACK (thunar_navigation_bar_toggled), navigation_bar);
  thunar_gtk_widget_set_tooltip (navigation_bar->button, _("Switch to path entry mode"));
  gtk_box_pack_start (GTK_BOX (navigation_bar), navigation_bar->button, FALSE, FALSE, 0);
  gtk_widget_show (navigation_bar->button);

  image = gtk_image_new_from_stock (GTK_STOCK_EDIT, GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (navigation_bar->button), image);
  gtk_widget_show (image);

  navigation_bar->path_entry_box = gtk_hbox_new (FALSE, 8);
  gtk_box_pack_start (GTK_BOX (navigation_bar), navigation_bar->path_entry_box, TRUE, TRUE, 0);

  align = gtk_alignment_new (0.5f, 0.5f, 1.0f, 1.0f);
  g_signal_connect (G_OBJECT (align), "size-request", G_CALLBACK (thunar_navigation_bar_label_size_request), navigation_bar);
  gtk_box_pack_start (GTK_BOX (navigation_bar->path_entry_box), align, FALSE, TRUE, 0);
  gtk_widget_show (align);

  navigation_bar->path_entry_label = gtk_label_new (_("Location:"));
  gtk_misc_set_alignment (GTK_MISC (navigation_bar->path_entry_label), 1.0f, 0.5f);
  gtk_container_add (GTK_CONTAINER (align), navigation_bar->path_entry_label);
  gtk_widget_show (navigation_bar->path_entry_label);

  navigation_bar->path_entry = thunar_path_entry_new ();
  exo_binding_new (G_OBJECT (navigation_bar), "current-directory", G_OBJECT (navigation_bar->path_entry), "current-file");
  g_signal_connect_after (G_OBJECT (navigation_bar->path_entry), "activate", G_CALLBACK (thunar_navigation_bar_activate), navigation_bar);
  g_signal_connect_after (G_OBJECT (navigation_bar->path_entry), "changed", G_CALLBACK (thunar_navigation_bar_changed), navigation_bar);
  gtk_box_pack_start (GTK_BOX (navigation_bar->path_entry_box), navigation_bar->path_entry, TRUE, TRUE, 0);
  gtk_widget_show (navigation_bar->path_entry);

  /* set Atk label relation for the entry */
  object = gtk_widget_get_accessible (navigation_bar->path_entry);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (navigation_bar->path_entry_label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));

  navigation_bar->path_bar = thunar_path_bar_new ();
  exo_binding_new (G_OBJECT (navigation_bar), "current-directory", G_OBJECT (navigation_bar->path_bar), "current-directory");
  g_signal_connect_swapped (G_OBJECT (navigation_bar->path_bar), "context-menu", G_CALLBACK (thunar_navigation_bar_context_menu), navigation_bar);
  g_signal_connect_swapped (G_OBJECT (navigation_bar->path_bar), "change-directory", G_CALLBACK (thunar_navigator_change_directory), navigation_bar);
  gtk_box_pack_start (GTK_BOX (navigation_bar), navigation_bar->path_bar, TRUE, TRUE, 0);
  gtk_widget_show (navigation_bar->path_bar);

  /* use the default setting for the entry */
  g_object_get (G_OBJECT (navigation_bar->preferences), "last-navigation-bar-entry", &active, NULL);
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (navigation_bar->button), active);
}



static void
thunar_navigation_bar_finalize (GObject *object)
{
  ThunarNavigationBar *navigation_bar = THUNAR_NAVIGATION_BAR (object);

  /* disconnect from the selected files and the UI manager */
  thunar_component_set_selected_files (THUNAR_COMPONENT (navigation_bar), NULL);
  thunar_component_set_ui_manager (THUNAR_COMPONENT (navigation_bar), NULL);

  /* disconnect from the current_directory */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (navigation_bar), NULL);

  /* release our action group */
  g_object_unref (G_OBJECT (navigation_bar->action_group));

  /* disconnect from the preferences */
  g_object_unref (G_OBJECT (navigation_bar->preferences));

  (*G_OBJECT_CLASS (thunar_navigation_bar_parent_class)->finalize) (object);
}



static void
thunar_navigation_bar_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    case PROP_SELECTED_FILES:
      g_value_set_boxed (value, thunar_component_get_selected_files (THUNAR_COMPONENT (object)));
      break;

    case PROP_UI_MANAGER:
      g_value_set_object (value, thunar_component_get_ui_manager (THUNAR_COMPONENT (object)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_navigation_bar_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    case PROP_SELECTED_FILES:
      thunar_component_set_selected_files (THUNAR_COMPONENT (object), g_value_get_boxed (value));
      break;

    case PROP_UI_MANAGER:
      thunar_component_set_ui_manager (THUNAR_COMPONENT (object), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_navigation_bar_get_current_directory (ThunarNavigator *navigator)
{
  return THUNAR_NAVIGATION_BAR (navigator)->current_directory;
}



static void
thunar_navigation_bar_set_current_directory (ThunarNavigator *navigator,
                                             ThunarFile      *current_directory)
{
  ThunarNavigationBar *navigation_bar = THUNAR_NAVIGATION_BAR (navigator);

  _thunar_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));
  _thunar_return_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar));

  /* disconnect from the previous current_directory */
  if (G_LIKELY (navigation_bar->current_directory != NULL))
    g_object_unref (G_OBJECT (navigation_bar->current_directory));

  /* activate the new current_directory */
  navigation_bar->current_directory = current_directory;

  /* connect to the new current_directory */
  if (G_LIKELY (current_directory != NULL))
    {
      /* take a reference on the new directory */
      g_object_ref (G_OBJECT (current_directory));

      /* check if the path_entry is temporarily shown */
      if (G_UNLIKELY (navigation_bar->temporary_path_entry))
        {
          /* switch back to the path_bar (also resets the temporary flag for the path_entry) */
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (navigation_bar->button), FALSE);
        }
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (navigation_bar), "current-directory");
}



static GtkUIManager*
thunar_navigation_bar_get_ui_manager (ThunarComponent *component)
{
  return THUNAR_NAVIGATION_BAR (component)->ui_manager;
}



static void
thunar_navigation_bar_set_ui_manager (ThunarComponent *component,
                                      GtkUIManager    *ui_manager)
{
  ThunarNavigationBar *navigation_bar = THUNAR_NAVIGATION_BAR (component);
  GError              *error = NULL;

  /* disconnect from the previous ui manager */
  if (G_UNLIKELY (navigation_bar->ui_manager != NULL))
    {
      /* drop our action group from the previous UI manager */
      gtk_ui_manager_remove_action_group (navigation_bar->ui_manager, navigation_bar->action_group);

      /* unmerge our ui controls from the previous UI manager */
      gtk_ui_manager_remove_ui (navigation_bar->ui_manager, navigation_bar->ui_merge_id);

      /* drop our reference on the previous UI manager */
      g_object_unref (G_OBJECT (navigation_bar->ui_manager));
    }

  /* activate the new UI manager */
  navigation_bar->ui_manager = ui_manager;

  /* connect to the new UI manager */
  if (G_LIKELY (ui_manager != NULL))
    {
      /* we keep a reference on the new manager */
      g_object_ref (G_OBJECT (ui_manager));

      /* add our action group to the new manager */
      gtk_ui_manager_insert_action_group (ui_manager, navigation_bar->action_group, -1);

      /* merge our UI control items with the new manager */
      navigation_bar->ui_merge_id = gtk_ui_manager_add_ui_from_string (ui_manager, thunar_navigation_bar_ui, thunar_navigation_bar_ui_length, &error);
      if (G_UNLIKELY (navigation_bar->ui_merge_id == 0))
        {
          g_error ("Failed to merge ThunarNavigationBar menus: %s", error->message);
          g_error_free (error);
        }
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (navigation_bar), "ui-manager");
}



static gboolean
thunar_navigation_bar_reset (ThunarNavigationBar *navigation_bar)
{
  _thunar_return_val_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar), FALSE);

  /* check if the path_entry was shown temporarily */
  if (G_LIKELY (navigation_bar->temporary_path_entry))
    {
      /* switch back to the path_bar (also resets the temporary flag for the path_entry) */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (navigation_bar->button), FALSE);
    }

  /* be sure to reset the current file of the path entry */
  if (G_LIKELY (navigation_bar->current_directory != NULL))
    thunar_path_entry_set_current_file (THUNAR_PATH_ENTRY (navigation_bar->path_entry), navigation_bar->current_directory);

  return TRUE;
}



static void
thunar_navigation_bar_action_create_folder (GtkAction           *action,
                                            ThunarNavigationBar *navigation_bar)
{
  ThunarVfsMimeDatabase *mime_database;
  ThunarVfsMimeInfo     *mime_info;
  ThunarApplication     *application;
  ThunarFile            *directory;
  GList                  path_list;
  gchar                 *name;

  _thunar_return_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar));
  _thunar_return_if_fail (GTK_IS_ACTION (action));

  /* determine the directory for the action */
  directory = g_object_get_data (G_OBJECT (action), "thunar-file");
  if (G_LIKELY (directory != NULL))
    {
      /* lookup "inode/directory" mime info */
      mime_database = thunar_vfs_mime_database_get_default ();
      mime_info = thunar_vfs_mime_database_get_info (mime_database, "inode/directory");

      /* ask the user to enter a name for the new folder */
      name = thunar_show_create_dialog (GTK_WIDGET (navigation_bar), mime_info, _("New Folder"), _("Create New Folder"));
      if (G_LIKELY (name != NULL))
        {
          /* fake the path list */
          path_list.data = thunar_vfs_path_relative (thunar_file_get_path (directory), name);
          path_list.next = path_list.prev = NULL;

          /* launch the operation */
          application = thunar_application_get ();
          thunar_application_mkdir (application, GTK_WIDGET (navigation_bar), &path_list, NULL);
          g_object_unref (G_OBJECT (application));

          /* release the path */
          thunar_vfs_path_unref (path_list.data);

          /* release the file name */
          g_free (name);
        }

      /* cleanup */
      g_object_unref (G_OBJECT (mime_database));
      thunar_vfs_mime_info_unref (mime_info);
    }
}



static void
thunar_navigation_bar_action_down_folder (GtkAction           *action,
                                          ThunarNavigationBar *navigation_bar)
{
  _thunar_return_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar));
  _thunar_return_if_fail (GTK_IS_ACTION (action));

  /* check if the path_bar is currently active (down-folder doesn't make sense for path_entry) */
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (navigation_bar->button)))
    {
      /* tell the path_bar to go down one folder */
      thunar_path_bar_down_folder (THUNAR_PATH_BAR (navigation_bar->path_bar));
    }
}



static void
thunar_navigation_bar_action_empty_trash (GtkAction           *action,
                                          ThunarNavigationBar *navigation_bar)
{
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar));
  _thunar_return_if_fail (GTK_IS_ACTION (action));

  /* launch the operation */
  application = thunar_application_get ();
  thunar_application_empty_trash (application, GTK_WIDGET (navigation_bar));
  g_object_unref (G_OBJECT (application));
}



static void
thunar_navigation_bar_action_open (GtkAction           *action,
                                   ThunarNavigationBar *navigation_bar)
{
  ThunarFile *directory;

  _thunar_return_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar));
  _thunar_return_if_fail (GTK_IS_ACTION (action));

  /* determine the directory for the action */
  directory = g_object_get_data (G_OBJECT (action), "thunar-file");
  if (G_LIKELY (directory != NULL && thunar_file_is_directory (directory)))
    {
      /* open the folder in this window */
      thunar_navigator_change_directory (THUNAR_NAVIGATOR (navigation_bar), directory);
    }
}



static void
thunar_navigation_bar_action_open_in_new_window (GtkAction           *action,
                                                 ThunarNavigationBar *navigation_bar)
{
  ThunarApplication *application;
  ThunarFile        *directory;

  _thunar_return_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar));
  _thunar_return_if_fail (GTK_IS_ACTION (action));

  /* determine the directory for the action */
  directory = g_object_get_data (G_OBJECT (action), "thunar-file");
  if (G_LIKELY (directory != NULL && thunar_file_is_directory (directory)))
    {
      /* open the folder in a new window */
      application = thunar_application_get ();
      thunar_application_open_window (application, directory, gtk_widget_get_screen (GTK_WIDGET (navigation_bar)));
      g_object_unref (G_OBJECT (application));
    }
}



static void
thunar_navigation_bar_action_paste_into_folder (GtkAction           *action,
                                                ThunarNavigationBar *navigation_bar)
{
  ThunarClipboardManager *clipboard;
  ThunarFile             *directory;

  _thunar_return_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar));
  _thunar_return_if_fail (GTK_IS_ACTION (action));

  /* determine the directory for the action */
  directory = g_object_get_data (G_OBJECT (action), "thunar-file");
  if (G_LIKELY (directory != NULL && thunar_file_is_directory (directory)))
    {
      /* paste files from the clipboard to the folder represented by this ThunarFile */
      clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (GTK_WIDGET (navigation_bar)));
      thunar_clipboard_manager_paste_files (clipboard, thunar_file_get_path (directory), GTK_WIDGET (navigation_bar), NULL);
      g_object_unref (G_OBJECT (clipboard));
    }
}



static void
thunar_navigation_bar_action_properties (GtkAction           *action,
                                         ThunarNavigationBar *navigation_bar)
{
  ThunarFile *directory;
  GtkWidget  *toplevel;
  GtkWidget  *dialog;

  _thunar_return_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar));
  _thunar_return_if_fail (GTK_IS_ACTION (action));

  /* determine the directory for the action */
  directory = g_object_get_data (G_OBJECT (action), "thunar-file");
  if (G_LIKELY (directory != NULL))
    {
      /* determine the toplevel window */
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (navigation_bar));
      if (G_LIKELY (toplevel != NULL && GTK_WIDGET_TOPLEVEL (toplevel)))
        {
          /* popup the properties dialog */
          dialog = thunar_properties_dialog_new ();
          gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
          thunar_properties_dialog_set_file (THUNAR_PROPERTIES_DIALOG (dialog), directory);
          gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
          gtk_widget_show (dialog);
        }
    }
}



static void
thunar_navigation_bar_activate (GtkWidget           *path_entry,
                                ThunarNavigationBar *navigation_bar)
{
  ThunarFile *file;
  GError     *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar));
  _thunar_return_if_fail (THUNAR_IS_PATH_ENTRY (path_entry));

  /* determine the current file from the path entry */
  file = thunar_path_entry_get_current_file (THUNAR_PATH_ENTRY (path_entry));
  if (G_LIKELY (file != NULL))
    {
      /* check if we have a new directory or a file to launch */
      if (thunar_file_is_directory (file))
        {
          /* open the new directory */
          thunar_navigator_change_directory (THUNAR_NAVIGATOR (navigation_bar), file);
        }
      else
        {
          /* try to launch the selected file */
          if (!thunar_file_launch (file, path_entry, &error))
            {
              thunar_dialogs_show_error (path_entry, error, _("Failed to launch \"%s\""), thunar_file_get_display_name (file));
              g_error_free (error);
            }

          /* be sure to reset the current file of the path entry */
          if (G_LIKELY (navigation_bar->current_directory != NULL))
            thunar_path_entry_set_current_file (THUNAR_PATH_ENTRY (path_entry), navigation_bar->current_directory);
        }

      /* check if the path_entry was only shown temporarily */
      if (G_UNLIKELY (navigation_bar->temporary_path_entry))
        {
          /* switch back to path_bar (also resets the temporary flag for the path_entry) */
          gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (navigation_bar->button), FALSE);
        }

      /* be sure to update the path_entry label */
      thunar_navigation_bar_changed (path_entry, navigation_bar);
    }
}



static void
thunar_navigation_bar_changed (GtkWidget           *path_entry,
                               ThunarNavigationBar *navigation_bar)
{
  ThunarVfsPath *path;
  ThunarFile    *file;

  _thunar_return_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar));
  _thunar_return_if_fail (navigation_bar->path_entry == path_entry);

  /* determine the current path in the path_entry */
  path = thunar_vfs_path_new (gtk_entry_get_text (GTK_ENTRY (path_entry)), NULL);
  
  /* determine the cached file for the path (may be NULL) */
  file = (path != NULL) ? thunar_file_cache_lookup (path) : NULL;

  /* check if this is equal to the current_directory */
  if (G_LIKELY (navigation_bar->current_directory == file))
    {
      /* use the "Location:" label, as this is really the current location */
      gtk_label_set_text (GTK_LABEL (navigation_bar->path_entry_label), _("Location:"));
    }
  else
    {
      /* the user is trying to change the location, use the "Go To:" label */
      gtk_label_set_text (GTK_LABEL (navigation_bar->path_entry_label), _("Go To:"));
    }

  /* cleanup */
  thunar_vfs_path_unref (path);
}



static void
thunar_navigation_bar_context_menu (ThunarNavigationBar *navigation_bar,
                                    GdkEventButton      *event,
                                    ThunarFile          *file)
{
  ThunarClipboardManager *clipboard;
  const gchar            *display_name;
  GtkAction              *action;
  GMainLoop              *loop;
  GtkWidget              *menu;
  guint                   signal_id;

  _thunar_return_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));
  _thunar_return_if_fail (event != NULL);

  /* we cannot popup the context menu without an ui manager */
  if (G_LIKELY (navigation_bar->ui_manager != NULL))
    {
      /* determine the display name of the file */
      display_name = thunar_file_get_display_name (file);

      /* be sure to keep a reference on the navigation bar */
      g_object_ref (G_OBJECT (navigation_bar));

      /* grab a reference on the clipboard manager for this display */
      clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (GTK_WIDGET (navigation_bar)));

      /* setup the "Open" action */
      action = gtk_action_group_get_action (navigation_bar->action_group, "navbar-open");
      thunar_gtk_action_set_tooltip (action, _("Open \"%s\" in this window"), display_name);
      g_object_set_data_full (G_OBJECT (action), I_("thunar-file"), g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);
      gtk_action_set_sensitive (action, (file != navigation_bar->current_directory));

      /* setup the "Open in New Window" action */
      action = gtk_action_group_get_action (navigation_bar->action_group, "navbar-open-in-new-window");
      thunar_gtk_action_set_tooltip (action, _("Open \"%s\" in a new window"), display_name);
      g_object_set_data_full (G_OBJECT (action), I_("thunar-file"), g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);

      /* setup the "Create Folder..." action */
      action = gtk_action_group_get_action (navigation_bar->action_group, "navbar-create-folder");
      thunar_gtk_action_set_tooltip (action, _("Create a new folder in \"%s\""), display_name);
      g_object_set_data_full (G_OBJECT (action), I_("thunar-file"), g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);
      gtk_action_set_sensitive (action, thunar_file_is_writable (file));
      gtk_action_set_visible (action, !thunar_file_is_trashed (file));

      /* setup the "Empty Trash" action */
      action = gtk_action_group_get_action (navigation_bar->action_group, "navbar-empty-trash");
      gtk_action_set_visible (action, (thunar_file_is_root (file) && thunar_file_is_trashed (file)));
      gtk_action_set_sensitive (action, (thunar_file_get_size (file) > 0));

      /* setup the "Paste Into Folder" action */
      action = gtk_action_group_get_action (navigation_bar->action_group, "navbar-paste-into-folder");
      thunar_gtk_action_set_tooltip (action, _("Move or copy files previously selected by a Cut or Copy command into \"%s\""), display_name);
      g_object_set_data_full (G_OBJECT (action), I_("thunar-file"), g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);
      gtk_action_set_sensitive (action, thunar_clipboard_manager_get_can_paste (clipboard));

      /* setup the "Properties..." action */
      action = gtk_action_group_get_action (navigation_bar->action_group, "navbar-properties");
      thunar_gtk_action_set_tooltip (action, _("View the properties of the folder \"%s\""), display_name);
      g_object_set_data_full (G_OBJECT (action), I_("thunar-file"), g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);

      /* determine the menu widget */
      menu = gtk_ui_manager_get_widget (navigation_bar->ui_manager, "/navbar-context-menu");
      gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (GTK_WIDGET (navigation_bar)));
      exo_gtk_object_ref_sink (GTK_OBJECT (menu));

      /* run an internal main loop */
      gtk_grab_add (menu);
      loop = g_main_loop_new (NULL, FALSE);
      signal_id = g_signal_connect_swapped (G_OBJECT (menu), "deactivate", G_CALLBACK (g_main_loop_quit), loop);
      gtk_menu_popup (GTK_MENU (menu), NULL, NULL, NULL, NULL, event->button, event->time);
      g_main_loop_run (loop);
      g_signal_handler_disconnect (G_OBJECT (menu), signal_id);
      g_main_loop_unref (loop);
      gtk_grab_remove (menu);

      /* cleanup */
      g_object_unref (G_OBJECT (navigation_bar));
      g_object_unref (G_OBJECT (clipboard));
      g_object_unref (G_OBJECT (menu));
    }
}



static void
thunar_navigation_bar_label_size_request (GtkWidget           *align,
                                          GtkRequisition      *requisition,
                                          ThunarNavigationBar *navigation_bar)
{
  PangoLayout *layout;
  gint         location_height;
  gint         location_width;
  gint         goto_height;
  gint         goto_width;

  /* determine the "Location:" pixel size */
  layout = gtk_widget_create_pango_layout (navigation_bar->path_entry_label, _("Location:"));
  pango_layout_get_pixel_size (layout, &location_width, &location_height);

  /* determine the "Go To:" pixel size */
  pango_layout_set_text (layout, _("Go To:"), -1);
  pango_layout_get_pixel_size (layout, &goto_width, &goto_height);

  /* request the maximum of the pixel sizes, to make sure
   * the label always requests the same size.
   */
  requisition->width = MAX (location_width, goto_width);
  requisition->height = MAX (location_height, goto_height);

  /* cleanup */
  g_object_unref (G_OBJECT (layout));
}



static void
thunar_navigation_bar_toggled (GtkWidget           *button,
                               ThunarNavigationBar *navigation_bar)
{
  gboolean active;

  _thunar_return_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar));
  _thunar_return_if_fail (GTK_IS_TOGGLE_BUTTON (button));

  /* reset the temporary flag for the path_entry */
  navigation_bar->temporary_path_entry = FALSE;

  /* update the visibility of path_bar and path_entry_box */
  active = gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (button));
  if (G_UNLIKELY (active))
    {
      /* hide the path_bar/show the path_entry_box */
      gtk_widget_hide (navigation_bar->path_bar);
      gtk_widget_show (navigation_bar->path_entry_box);

      /* update the tooltip of the location mode button */
      thunar_gtk_widget_set_tooltip (navigation_bar->button, _("Switch to path bar mode"));
    }
  else
    {
      /* show the path_bar/hide the path_entry_box */
      gtk_widget_show (navigation_bar->path_bar);
      gtk_widget_hide (navigation_bar->path_entry_box);

      /* update the tooltip of the location mode button */
      thunar_gtk_widget_set_tooltip (navigation_bar->button, _("Switch to path entry mode"));
    }

  /* remember the setting as default */
  g_object_set (G_OBJECT (navigation_bar->preferences), "last-navigation-bar-entry", active, NULL);
}



/**
 * thunar_navigation_bar_new:
 *
 * Allocates a new #ThunarNavigationBar that is not associated with any path.
 *
 * Return value: the newly allocated #ThunarNavigationBar.
 **/
GtkWidget*
thunar_navigation_bar_new (void)
{
  return g_object_new (THUNAR_TYPE_NAVIGATION_BAR, NULL);
}



/**
 * thunar_navigation_bar_open_location:
 * @navigation_bar : a #ThunarNavigationBar.
 * @initial_text   : the initial text for the location bar, or %NULL.
 *
 * Gives focus to the location entry widget and puts the @initial_text, if not
 * %NULL, into the entry.
 *
 * If the path bar is currently visible, it will be temporarily hidden until
 * the user is done with the editing.
 **/
void
thunar_navigation_bar_open_location (ThunarNavigationBar *navigation_bar,
                                     const gchar         *initial_text)
{
  _thunar_return_if_fail (THUNAR_IS_NAVIGATION_BAR (navigation_bar));

  /* check if we not already display the path_entry */
  if (!gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (navigation_bar->button)))
    {
      /* switch to the path_entry */
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (navigation_bar->button), TRUE);

      /* the path_entry is shown only temporarily */
      navigation_bar->temporary_path_entry = TRUE;
    }

  /* check if we have an initial text for the location bar */
  if (G_LIKELY (initial_text != NULL))
    {
      /* setup the new text */
      gtk_entry_set_text (GTK_ENTRY (navigation_bar->path_entry), initial_text);

      /* move the cursor to the end of the text */
      gtk_editable_set_position (GTK_EDITABLE (navigation_bar->path_entry), -1);
    }
  else
    {
      /* select the whole path in the path entry */
      gtk_editable_select_region (GTK_EDITABLE (navigation_bar->path_entry), 0, -1);
    }

  /* grab focus to the path_entry widget */
  gtk_widget_grab_focus (navigation_bar->path_entry);
}



