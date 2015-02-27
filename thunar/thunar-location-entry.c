/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2010 Jannis Pohlmann <jannis@xfce.org>
 *
 * This program is free software; you can redistribute it and/or 
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of 
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public 
 * License along with this program; if not, write to the Free 
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <gdk/gdkkeysyms.h>

#include <thunar/thunar-browser.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-location-entry.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-path-entry.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-shortcuts-model.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SELECTED_FILES,
  PROP_UI_MANAGER,
};



static void        thunar_location_entry_component_init           (ThunarComponentIface     *iface);
static void        thunar_location_entry_navigator_init           (ThunarNavigatorIface     *iface);
static void        thunar_location_entry_location_bar_init        (ThunarLocationBarIface   *iface);
static void        thunar_location_entry_finalize                 (GObject                  *object);
static void        thunar_location_entry_get_property             (GObject                  *object,
                                                                   guint                     prop_id,
                                                                   GValue                   *value,
                                                                   GParamSpec               *pspec);
static void        thunar_location_entry_set_property             (GObject                  *object,
                                                                   guint                     prop_id,
                                                                   const GValue             *value,
                                                                   GParamSpec               *pspec);
static ThunarFile *thunar_location_entry_get_current_directory    (ThunarNavigator          *navigator);
static void        thunar_location_entry_set_current_directory    (ThunarNavigator          *navigator,
                                                                   ThunarFile               *current_directory);
static void        thunar_location_entry_component_set_ui_manager (ThunarComponent          *component,
                                                                   GtkUIManager             *ui_manager);
static gboolean    thunar_location_entry_accept_focus             (ThunarLocationBar        *location_bar,
                                                                   const gchar              *initial_text);
static void        thunar_location_entry_activate                 (GtkWidget                *path_entry,
                                                                   ThunarLocationEntry      *location_entry);
static gboolean    thunar_location_entry_reset                    (ThunarLocationEntry      *location_entry);
static void        thunar_location_entry_reload                   (GtkEntry                 *entry,
                                                                   GtkEntryIconPosition      icon_pos,
                                                                   GdkEvent                 *event,
                                                                   ThunarLocationEntry      *location_entry);



struct _ThunarLocationEntryClass
{
  GtkHBoxClass __parent__;

  /* internal action signals */
  gboolean (*reset) (ThunarLocationEntry *location_entry);
};

struct _ThunarLocationEntry
{
  GtkHBox __parent__;

  ThunarFile   *current_directory;
  GtkWidget    *path_entry;
  GtkUIManager *ui_manager;
};



G_DEFINE_TYPE_WITH_CODE (ThunarLocationEntry, thunar_location_entry, GTK_TYPE_HBOX,
  G_IMPLEMENT_INTERFACE (THUNAR_TYPE_BROWSER, NULL)
  G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_location_entry_navigator_init)
  G_IMPLEMENT_INTERFACE (THUNAR_TYPE_COMPONENT, thunar_location_entry_component_init)
  G_IMPLEMENT_INTERFACE (THUNAR_TYPE_LOCATION_BAR, thunar_location_entry_location_bar_init))



static void
thunar_location_entry_class_init (ThunarLocationEntryClass *klass)
{
  GtkBindingSet *binding_set;
  GObjectClass  *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_location_entry_finalize;
  gobject_class->get_property = thunar_location_entry_get_property;
  gobject_class->set_property = thunar_location_entry_set_property;

  klass->reset = thunar_location_entry_reset;

  /* override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

  /* override ThunarComponent's properties */
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
                G_STRUCT_OFFSET (ThunarLocationEntryClass, reset),
                g_signal_accumulator_true_handled, NULL,
                _thunar_marshal_BOOLEAN__VOID,
                G_TYPE_BOOLEAN, 0);

  /* setup the key bindings for the location entry */
  binding_set = gtk_binding_set_by_class (klass);
  gtk_binding_entry_add_signal (binding_set, GDK_Escape, 0, "reset", 0);
}



static void
thunar_location_entry_component_init (ThunarComponentIface *iface)
{
  iface->get_selected_files = (gpointer) exo_noop_null;
  iface->set_selected_files = (gpointer) exo_noop;
  iface->get_ui_manager = (gpointer) exo_noop_null;
  iface->set_ui_manager = thunar_location_entry_component_set_ui_manager;
}



static void
thunar_location_entry_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_location_entry_get_current_directory;
  iface->set_current_directory = thunar_location_entry_set_current_directory;
}



static void
thunar_location_entry_location_bar_init (ThunarLocationBarIface *iface)
{
  iface->accept_focus = thunar_location_entry_accept_focus;
  iface->is_standalone = (gpointer) exo_noop_false;
}



static void
thunar_location_entry_init (ThunarLocationEntry *location_entry)
{
  gtk_box_set_spacing (GTK_BOX (location_entry), 0);

  location_entry->path_entry = thunar_path_entry_new ();
  exo_binding_new (G_OBJECT (location_entry), "current-directory", G_OBJECT (location_entry->path_entry), "current-file");
  g_signal_connect_after (G_OBJECT (location_entry->path_entry), "activate", G_CALLBACK (thunar_location_entry_activate), location_entry);
  gtk_box_pack_start (GTK_BOX (location_entry), location_entry->path_entry, TRUE, TRUE, 0);
  gtk_widget_show (location_entry->path_entry);

  /* put reload button in entry */
  gtk_entry_set_icon_from_icon_name (GTK_ENTRY (location_entry->path_entry),
                                     GTK_ENTRY_ICON_SECONDARY, "view-refresh");
  gtk_entry_set_icon_tooltip_text (GTK_ENTRY (location_entry->path_entry),
                                   GTK_ENTRY_ICON_SECONDARY, _("Reload the current folder"));
  g_signal_connect (G_OBJECT (location_entry->path_entry), "icon-release",
                    G_CALLBACK (thunar_location_entry_reload), location_entry);
}



static void
thunar_location_entry_finalize (GObject *object)
{
  /* disconnect from the selected files and the UI manager */
  thunar_component_set_selected_files (THUNAR_COMPONENT (object), NULL);
  thunar_component_set_ui_manager (THUNAR_COMPONENT (object), NULL);

  /* disconnect from the current directory */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), NULL);

  (*G_OBJECT_CLASS (thunar_location_entry_parent_class)->finalize) (object);
}



static void
thunar_location_entry_get_property (GObject    *object,
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
thunar_location_entry_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ThunarLocationEntry *entry = THUNAR_LOCATION_ENTRY (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      thunar_path_entry_set_working_directory (THUNAR_PATH_ENTRY (entry->path_entry), 
                                               entry->current_directory);
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
thunar_location_entry_get_current_directory (ThunarNavigator *navigator)
{
  return THUNAR_LOCATION_ENTRY (navigator)->current_directory;
}



static void
thunar_location_entry_set_current_directory (ThunarNavigator *navigator,
                                             ThunarFile      *current_directory)
{
  ThunarLocationEntry *location_entry = THUNAR_LOCATION_ENTRY (navigator);

  /* disconnect from the previous directory */
  if (G_LIKELY (location_entry->current_directory != NULL))
    g_object_unref (G_OBJECT (location_entry->current_directory));

  /* activate the new directory */
  location_entry->current_directory = current_directory;

  /* connect to the new directory */
  if (G_LIKELY (current_directory != NULL))
    g_object_ref (G_OBJECT (current_directory));

  /* notify listeners */
  g_object_notify (G_OBJECT (location_entry), "current-directory");
}



static void
thunar_location_entry_component_set_ui_manager (ThunarComponent *component,
                                                GtkUIManager    *ui_manager)
{
  ThunarLocationEntry *location_entry = THUNAR_LOCATION_ENTRY (component);

  if (location_entry->ui_manager != NULL)
    {
      g_object_unref (location_entry->ui_manager);
      location_entry->ui_manager = NULL;
    }

  if (ui_manager != NULL)
    location_entry->ui_manager = g_object_ref (ui_manager);
}



static gboolean
thunar_location_entry_accept_focus (ThunarLocationBar *location_bar,
                                    const gchar       *initial_text)
{
  ThunarLocationEntry *location_entry = THUNAR_LOCATION_ENTRY (location_bar);

  /* give the keyboard focus to the path entry */
  gtk_widget_grab_focus (location_entry->path_entry);

  /* check if we have an initial text for the location bar */
  if (G_LIKELY (initial_text != NULL))
    {
      /* setup the new text */
      gtk_entry_set_text (GTK_ENTRY (location_entry->path_entry), initial_text);

      /* move the cursor to the end of the text */
      gtk_editable_set_position (GTK_EDITABLE (location_entry->path_entry), -1);
    }
  else
    {
      /* select the whole path in the path entry */
      gtk_editable_select_region (GTK_EDITABLE (location_entry->path_entry), 0, -1);
    }

  return TRUE;
}



static void
thunar_location_entry_open_or_launch (ThunarLocationEntry *location_entry,
                                      ThunarFile          *file)
{
  GError *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_ENTRY (location_entry));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* check if the file is mounted */
  if (thunar_file_is_mounted (file))
    {
      /* check if we have a new directory or a file to launch */
      if (thunar_file_is_directory (file))
        {
          /* open the new directory */
          thunar_navigator_change_directory (THUNAR_NAVIGATOR (location_entry), file);
        }
      else
        {
          /* try to launch the selected file */
          thunar_file_launch (file, location_entry->path_entry, NULL, &error);

          /* be sure to reset the current file of the path entry */
          if (G_LIKELY (location_entry->current_directory != NULL))
            {
              thunar_path_entry_set_current_file (THUNAR_PATH_ENTRY (location_entry->path_entry), 
                                                  location_entry->current_directory);
            }
        }
    }
  else
    {
      g_set_error (&error, G_IO_ERROR, G_IO_ERROR_NOT_FOUND, _("File does not exist"));
    }

  /* check if we need to display an error dialog */
  if (error != NULL)
    {
      thunar_dialogs_show_error (location_entry->path_entry, error, 
                                 _("Failed to open \"%s\""), 
                                 thunar_file_get_display_name (file));
      g_error_free (error);
    }
}



static void
thunar_location_entry_poke_file_finish (ThunarBrowser *browser,
                                        ThunarFile    *file,
                                        ThunarFile    *target_file,
                                        GError        *error,
                                        gpointer       ignored)
{
  _thunar_return_if_fail (THUNAR_IS_LOCATION_ENTRY (browser));
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  if (error == NULL)
    {
      /* try to open or launch the target file */
      thunar_location_entry_open_or_launch (THUNAR_LOCATION_ENTRY (browser), 
                                            target_file);
    }
  else
    {
      /* display an error explaining why we couldn't open/mount the file */
      thunar_dialogs_show_error (THUNAR_LOCATION_ENTRY (browser)->path_entry,
                                 error, _("Failed to open \"%s\""), 
                                 thunar_file_get_display_name (file));
    }
}



static void
thunar_location_entry_activate (GtkWidget           *path_entry,
                                ThunarLocationEntry *location_entry)
{
  ThunarFile *file;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_ENTRY (location_entry));
  _thunar_return_if_fail (location_entry->path_entry == path_entry);

  /* determine the current file from the path entry */
  file = thunar_path_entry_get_current_file (THUNAR_PATH_ENTRY (path_entry));
  if (G_LIKELY (file != NULL))
    {
      thunar_browser_poke_file (THUNAR_BROWSER (location_entry), file, path_entry,
                                thunar_location_entry_poke_file_finish, NULL);
    }
}



static gboolean
thunar_location_entry_reset (ThunarLocationEntry *location_entry)
{
  /* just reset the path entry to our current directory... */
  thunar_path_entry_set_current_file (THUNAR_PATH_ENTRY (location_entry->path_entry), location_entry->current_directory);

  /* ...and select the whole text again */
  gtk_editable_select_region (GTK_EDITABLE (location_entry->path_entry), 0, -1);

  return TRUE;
}



static void
thunar_location_entry_reload (GtkEntry            *entry,
                              GtkEntryIconPosition icon_pos,
                              GdkEvent            *event,
                              ThunarLocationEntry *location_entry)
{
  GtkAction *action;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_ENTRY (location_entry));

  if (icon_pos == GTK_ENTRY_ICON_SECONDARY
      && location_entry->ui_manager != NULL)
    {
      action = gtk_ui_manager_get_action (location_entry->ui_manager, "/main-menu/view-menu/reload");
      _thunar_return_if_fail (GTK_IS_ACTION (action));
      gtk_action_activate (action);
    }
}
