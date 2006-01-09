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

#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-location-entry.h>
#include <thunar/thunar-path-entry.h>



enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
};



static void        thunar_location_entry_class_init            (ThunarLocationEntryClass *klass);
static void        thunar_location_entry_navigator_init        (ThunarNavigatorIface     *iface);
static void        thunar_location_entry_location_bar_init     (ThunarLocationBarIface   *iface);
static void        thunar_location_entry_init                  (ThunarLocationEntry      *location_entry);
static void        thunar_location_entry_finalize              (GObject                  *object);
static void        thunar_location_entry_get_property          (GObject                  *object,
                                                                guint                     prop_id,
                                                                GValue                   *value,
                                                                GParamSpec               *pspec);
static void        thunar_location_entry_set_property          (GObject                  *object,
                                                                guint                     prop_id,
                                                                const GValue             *value,
                                                                GParamSpec               *pspec);
static ThunarFile *thunar_location_entry_get_current_directory (ThunarNavigator          *navigator);
static void        thunar_location_entry_set_current_directory (ThunarNavigator          *navigator,
                                                                ThunarFile               *current_directory);
static gboolean    thunar_location_entry_accept_focus          (ThunarLocationBar        *location_bar);
static void        thunar_location_entry_activate              (ThunarLocationEntry      *location_entry);



struct _ThunarLocationEntryClass
{
  GtkHBoxClass __parent__;
};

struct _ThunarLocationEntry
{
  GtkHBox __parent__;

  ThunarFile *current_directory;
  GtkWidget  *path_entry;
};



static GObjectClass *thunar_location_entry_parent_class;



GType
thunar_location_entry_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarLocationEntryClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_location_entry_class_init,
        NULL,
        NULL,
        sizeof (ThunarLocationEntry),
        0,
        (GInstanceInitFunc) thunar_location_entry_init,
        NULL,
      };

      static const GInterfaceInfo navigator_info =
      {
        (GInterfaceInitFunc) thunar_location_entry_navigator_init,
        NULL,
        NULL,
      };

      static const GInterfaceInfo location_bar_info =
      {
        (GInterfaceInitFunc) thunar_location_entry_location_bar_init,
        NULL,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_HBOX, I_("ThunarLocationEntry"), &info, 0);
      g_type_add_interface_static (type, THUNAR_TYPE_NAVIGATOR, &navigator_info);
      g_type_add_interface_static (type, THUNAR_TYPE_LOCATION_BAR, &location_bar_info);
    }

  return type;
}



static void
thunar_location_entry_class_init (ThunarLocationEntryClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_location_entry_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_location_entry_finalize;
  gobject_class->get_property = thunar_location_entry_get_property;
  gobject_class->set_property = thunar_location_entry_set_property;

  g_object_class_override_property (gobject_class,
                                    PROP_CURRENT_DIRECTORY,
                                    "current-directory");
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



static gboolean
transform_object_to_boolean (const GValue *src_value,
                             GValue       *dst_value,
                             gpointer      user_data)
{
  g_value_set_boolean (dst_value, (g_value_get_object (src_value) != NULL));
  return TRUE;
}



static void
thunar_location_entry_init (ThunarLocationEntry *location_entry)
{
  GtkWidget *button;
  GtkWidget *image;

  gtk_box_set_spacing (GTK_BOX (location_entry), 2);

  location_entry->path_entry = thunar_path_entry_new ();
  g_signal_connect_swapped (G_OBJECT (location_entry->path_entry), "activate",
                            G_CALLBACK (thunar_location_entry_activate), location_entry);
  gtk_box_pack_start (GTK_BOX (location_entry), location_entry->path_entry, TRUE, TRUE, 0);
  gtk_widget_show (location_entry->path_entry);

  button = g_object_new (GTK_TYPE_BUTTON, "relief", GTK_RELIEF_NONE, NULL);
  g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (thunar_location_entry_activate), location_entry);
  gtk_box_pack_start (GTK_BOX (location_entry), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  image = gtk_image_new_from_stock (GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU);
  gtk_container_add (GTK_CONTAINER (button), image);
  gtk_widget_show (image);

  /* the "Go" button is only sensitive if a valid file is entered */
  exo_binding_new_full (G_OBJECT (location_entry->path_entry), "current-file",
                        G_OBJECT (button), "sensitive",
                        transform_object_to_boolean, NULL, NULL);
}



static void
thunar_location_entry_finalize (GObject *object)
{
  ThunarLocationEntry *location_entry = THUNAR_LOCATION_ENTRY (object);

  if (G_LIKELY (location_entry->current_directory != NULL))
    g_object_unref (G_OBJECT (location_entry->current_directory));

  G_OBJECT_CLASS (thunar_location_entry_parent_class)->finalize (object);
}



static void
thunar_location_entry_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ThunarNavigator *navigator = THUNAR_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (navigator));
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
  ThunarNavigator *navigator = THUNAR_NAVIGATOR (object);

  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (navigator, g_value_get_object (value));
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

  if (G_LIKELY (location_entry->current_directory != NULL))
    g_object_unref (G_OBJECT (location_entry->current_directory));

  location_entry->current_directory = current_directory;

  if (G_LIKELY (current_directory != NULL))
    g_object_ref (G_OBJECT (current_directory));

  thunar_path_entry_set_current_file (THUNAR_PATH_ENTRY (location_entry->path_entry), current_directory);

  g_object_notify (G_OBJECT (location_entry), "current-directory");
}



static gboolean
thunar_location_entry_accept_focus (ThunarLocationBar *location_bar)
{
  ThunarLocationEntry *location_entry = THUNAR_LOCATION_ENTRY (location_bar);

  /* select the whole path in the path entry */
  gtk_editable_select_region (GTK_EDITABLE (location_entry->path_entry), 0, -1);

  /* give the keyboard focus to the path entry */
  gtk_widget_grab_focus (location_entry->path_entry);

  return TRUE;
}



static void
thunar_location_entry_activate (ThunarLocationEntry *location_entry)
{
  ThunarFile *file;
  GError     *error = NULL;

  /* determine the current file from the path entry */
  file = thunar_path_entry_get_current_file (THUNAR_PATH_ENTRY (location_entry->path_entry));
  if (G_LIKELY (file != NULL))
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
          if (!thunar_file_launch (file, GTK_WIDGET (location_entry), &error))
            {
              thunar_dialogs_show_error (GTK_WIDGET (location_entry), error, _("Failed to launch `%s'"), thunar_file_get_display_name (file));
              g_error_free (error);
            }

          /* be sure to reset the current file of the path entry */
          if (G_LIKELY (location_entry->current_directory != NULL))
            thunar_path_entry_set_current_file (THUNAR_PATH_ENTRY (location_entry->path_entry), location_entry->current_directory);
        }
    }
}



/**
 * thunar_location_entry_new:
 *
 * Allocates a new #ThunarLocationEntry instance.
 *
 * Return value: the newly allocated #ThunarLocationEntry.
 **/
GtkWidget*
thunar_location_entry_new (void)
{
  return g_object_new (THUNAR_TYPE_LOCATION_ENTRY, NULL);
}



