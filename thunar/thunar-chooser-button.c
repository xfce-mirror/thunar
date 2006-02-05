/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>.
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

#include <thunar/thunar-chooser-button.h>
#include <thunar/thunar-chooser-dialog.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-pango-extensions.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
};



static void thunar_chooser_button_class_init      (ThunarChooserButtonClass *klass);
static void thunar_chooser_button_init            (ThunarChooserButton      *chooser_button);
static void thunar_chooser_button_finalize        (GObject                  *object);
static void thunar_chooser_button_get_property    (GObject                  *object,
                                                   guint                     prop_id,
                                                   GValue                   *value,
                                                   GParamSpec               *pspec);
static void thunar_chooser_button_set_property    (GObject                  *object,
                                                   guint                     prop_id,
                                                   const GValue             *value,
                                                   GParamSpec               *pspec);
static void thunar_chooser_button_activate        (ThunarChooserButton      *chooser_button,
                                                   GtkWidget                *item);
static void thunar_chooser_button_activate_other  (ThunarChooserButton      *chooser_button);
static void thunar_chooser_button_file_changed    (ThunarChooserButton      *chooser_button,
                                                   ThunarFile               *file);
static void thunar_chooser_button_pressed         (ThunarChooserButton      *chooser_button,
                                                   GtkWidget                *button);



struct _ThunarChooserButtonClass
{
  GtkHBoxClass __parent__;
};

struct _ThunarChooserButton
{
  GtkHBox __parent__;

  GtkWidget             *image;
  GtkWidget             *label;
  GtkWidget             *button;

  ThunarFile            *file;

  GtkTooltips           *tooltips;

  ThunarVfsMimeDatabase *database;
};



static GObjectClass *thunar_chooser_button_parent_class;



GType
thunar_chooser_button_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarChooserButtonClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_chooser_button_class_init,
        NULL,
        NULL,
        sizeof (ThunarChooserButton),
        0,
        (GInstanceInitFunc) thunar_chooser_button_init,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_HBOX, I_("ThunarChooserButton"), &info, 0);
    }

  return type;
}



static void
thunar_chooser_button_class_init (ThunarChooserButtonClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_chooser_button_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_chooser_button_finalize;
  gobject_class->get_property = thunar_chooser_button_get_property;
  gobject_class->set_property = thunar_chooser_button_set_property;

  /**
   * ThunarChooserButton:file:
   *
   * The #ThunarFile for which a preferred application should
   * be chosen.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file", "file", "file",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_chooser_button_init (ThunarChooserButton *chooser_button)
{
  GtkWidget *separator;
  GtkWidget *arrow;
  GtkWidget *hbox;

  /* grab a reference on the mime database */
  chooser_button->database = thunar_vfs_mime_database_get_default ();

  /* allocate tooltips */
  chooser_button->tooltips = gtk_tooltips_new ();
  exo_gtk_object_ref_sink (GTK_OBJECT (chooser_button->tooltips));

  gtk_widget_push_composite_child ();

  chooser_button->button = gtk_button_new ();
  g_signal_connect_swapped (G_OBJECT (chooser_button->button), "pressed", G_CALLBACK (thunar_chooser_button_pressed), chooser_button);
  gtk_box_pack_start (GTK_BOX (chooser_button), chooser_button->button, TRUE, TRUE, 0);
  gtk_widget_show (chooser_button->button);

  hbox = gtk_hbox_new (FALSE, 4);
  gtk_container_add (GTK_CONTAINER (chooser_button->button), hbox);
  gtk_widget_show (hbox);

  chooser_button->image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (hbox), chooser_button->image, FALSE, FALSE, 0);
  gtk_widget_show (chooser_button->image);

  chooser_button->label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, "yalign", 0.0f, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), chooser_button->label, TRUE, TRUE, 0);
  gtk_widget_show (chooser_button->label);

  separator = g_object_new (GTK_TYPE_VSEPARATOR, "height-request", 16, NULL);
  gtk_box_pack_start (GTK_BOX (hbox), separator, FALSE, FALSE, 0);
  gtk_widget_show (separator);

  arrow = gtk_arrow_new (GTK_ARROW_DOWN, GTK_SHADOW_NONE);
  gtk_box_pack_start (GTK_BOX (hbox), arrow, FALSE, FALSE, 0);
  gtk_widget_show (arrow);

  gtk_widget_pop_composite_child ();
}



static void
thunar_chooser_button_finalize (GObject *object)
{
  ThunarChooserButton *chooser_button = THUNAR_CHOOSER_BUTTON (object);

  /* reset the "file" property */
  thunar_chooser_button_set_file (chooser_button, NULL);

  /* disconnect from the mime database */
  g_object_unref (G_OBJECT (chooser_button->database));

  /* release the tooltips */
  g_object_unref (G_OBJECT (chooser_button->tooltips));

  (*G_OBJECT_CLASS (thunar_chooser_button_parent_class)->finalize) (object);
}



static void
thunar_chooser_button_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ThunarChooserButton *chooser_button = THUNAR_CHOOSER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, thunar_chooser_button_get_file (chooser_button));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_chooser_button_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ThunarChooserButton *chooser_button = THUNAR_CHOOSER_BUTTON (object);

  switch (prop_id)
    {
    case PROP_FILE:
      thunar_chooser_button_set_file (chooser_button, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_chooser_button_activate (ThunarChooserButton *chooser_button,
                                GtkWidget           *item)
{
  ThunarVfsMimeApplication *application;
  ThunarVfsMimeInfo        *info;
  GError                   *error = NULL;

  g_return_if_fail (THUNAR_IS_CHOOSER_BUTTON (chooser_button));
  g_return_if_fail (GTK_IS_MENU_ITEM (item));

  /* verify that we still have a valid file */
  if (G_UNLIKELY (chooser_button->file == NULL))
    return;

  /* determine the application that was set for the item */
  application = g_object_get_data (G_OBJECT (item), "thunar-vfs-mime-application");
  if (G_UNLIKELY (application == NULL))
    return;

  /* determine the mime info for the file */
  info = thunar_file_get_mime_info (chooser_button->file);

  /* try to set application as default for these kind of file */
  if (!thunar_vfs_mime_database_set_default_application (chooser_button->database, info, application, &error))
    {
      /* tell the user that it didn't work */
      thunar_dialogs_show_error (GTK_WIDGET (chooser_button), error, _("Failed to set default application for \"%s\""),
                                 thunar_file_get_display_name (chooser_button->file));
      g_error_free (error);
    }
  else
    {
      /* emit "changed" on the file, so everybody updates its state */
      thunar_file_changed (chooser_button->file);
    }
}



static void
thunar_chooser_button_activate_other (ThunarChooserButton *chooser_button)
{
  GtkWidget *toplevel;
  GtkWidget *dialog;

  g_return_if_fail (THUNAR_IS_CHOOSER_BUTTON (chooser_button));

  /* determine the toplevel window for the chooser */
  toplevel = gtk_widget_get_toplevel (GTK_WIDGET (chooser_button));
  if (G_UNLIKELY (toplevel == NULL))
    return;

  /* popup the application chooser dialog */
  dialog = g_object_new (THUNAR_TYPE_CHOOSER_DIALOG, "open", FALSE, NULL);
  exo_binding_new (G_OBJECT (chooser_button), "file", G_OBJECT (dialog), "file");
  gtk_window_set_transient_for (GTK_WINDOW (dialog), GTK_WINDOW (toplevel));
  gtk_window_set_modal (GTK_WINDOW (dialog), TRUE);
  gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);
}



static void
thunar_chooser_button_file_changed (ThunarChooserButton *chooser_button,
                                    ThunarFile          *file)
{
  ThunarVfsMimeApplication *application;
  ThunarVfsMimeInfo        *info;
  ThunarIconFactory        *icon_factory;
  GtkIconTheme             *icon_theme;
  const gchar              *icon_name;
  GdkPixbuf                *icon = NULL;
  gchar                    *text;
  gint                      icon_size;

  g_return_if_fail (THUNAR_IS_CHOOSER_BUTTON (chooser_button));
  g_return_if_fail (chooser_button->file == file);
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* determine the mime info for the file */
  info = thunar_file_get_mime_info (file);

  /* determine the default application for that mime info */
  application = thunar_vfs_mime_database_get_default_application (chooser_button->database, info);
  if (G_LIKELY (application != NULL))
    {
      /* determine the icon size for menus */
      gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_size, &icon_size);

      /* setup the image for the application */
      icon_factory = thunar_icon_factory_get_default ();
      icon_theme = thunar_icon_factory_get_icon_theme (icon_factory);
      icon_name = thunar_vfs_mime_handler_lookup_icon_name (THUNAR_VFS_MIME_HANDLER (application), icon_theme);
      if (G_LIKELY (icon_name != NULL))
        icon = thunar_icon_factory_load_icon (icon_factory, icon_name, icon_size, NULL, FALSE);
      gtk_image_set_from_pixbuf (GTK_IMAGE (chooser_button->image), icon);
      g_object_unref (G_OBJECT (icon_factory));
      if (G_LIKELY (icon != NULL))
        g_object_unref (G_OBJECT (icon));

      /* setup the label for the application */
      gtk_label_set_attributes (GTK_LABEL (chooser_button->label), NULL);
      gtk_label_set_text (GTK_LABEL (chooser_button->label), thunar_vfs_mime_handler_get_name (THUNAR_VFS_MIME_HANDLER (application)));

      /* cleanup */
      g_object_unref (G_OBJECT (application));
    }
  else
    {
      /* no default application specified */
      gtk_label_set_attributes (GTK_LABEL (chooser_button->label), thunar_pango_attr_list_italic ());
      gtk_label_set_text (GTK_LABEL (chooser_button->label), _("No application selected"));
      gtk_image_set_from_pixbuf (GTK_IMAGE (chooser_button->image), NULL);
    }

  /* setup a useful tooltip and ATK description */
  text = g_strdup_printf (_("The selected application is used to open "
                            "this and other files or type \"%s\"."),
                          thunar_vfs_mime_info_get_comment (info));
  atk_object_set_name (gtk_widget_get_accessible (chooser_button->button), text);
  gtk_tooltips_set_tip (chooser_button->tooltips, chooser_button->button, text, NULL);
  g_free (text);
}



static void
menu_position (GtkMenu  *menu,
               gint     *x,
               gint     *y,
               gboolean *push_in,
               gpointer  chooser)
{
  GtkRequisition chooser_request;
  GtkRequisition menu_request;
  GdkRectangle   geometry;
  GdkScreen     *screen;
  GtkWidget     *toplevel = gtk_widget_get_toplevel (chooser);
  gint           monitor;
  gint           x0;
  gint           y0;

  gtk_widget_translate_coordinates (GTK_WIDGET (chooser), toplevel, 0, 0, &x0, &y0);

  gtk_widget_size_request (GTK_WIDGET (chooser), &chooser_request);
  gtk_widget_size_request (GTK_WIDGET (menu), &menu_request);

  gdk_window_get_position (GTK_WIDGET (chooser)->window, x, y);

  *y += y0;
  *x += x0;

  /* verify the the menu is on-screen */
  screen = gtk_widget_get_screen (GTK_WIDGET (chooser));
  if (G_LIKELY (screen != NULL))
    {
      monitor = gdk_screen_get_monitor_at_point (screen, *x, *y);
      gdk_screen_get_monitor_geometry (screen, monitor, &geometry);
      if (*y + menu_request.height > geometry.y + geometry.height)
        *y -= menu_request.height - chooser_request.height;
    }

  *push_in = TRUE;
}



static void
thunar_chooser_button_pressed (ThunarChooserButton *chooser_button,
                               GtkWidget           *button)
{
  ThunarVfsMimeApplication *default_application;
  ThunarVfsMimeInfo        *info;
  ThunarIconFactory        *icon_factory;
  GtkIconTheme             *icon_theme;
  const gchar              *icon_name;
  GdkPixbuf                *icon;
  GMainLoop                *loop;
  GtkWidget                *image;
  GtkWidget                *item;
  GtkWidget                *menu;
  GList                    *applications;
  GList                    *lp;
  gint                      icon_size;

  g_return_if_fail (THUNAR_IS_CHOOSER_BUTTON (chooser_button));
  g_return_if_fail (chooser_button->button == button);
  g_return_if_fail (GTK_IS_BUTTON (button));

  /* verify that we have a valid file */
  if (G_UNLIKELY (chooser_button->file == NULL))
    return;

  /* determine the mime info for the file */
  info = thunar_file_get_mime_info (chooser_button->file);

  /* determine the default application */
  default_application = thunar_vfs_mime_database_get_default_application (chooser_button->database, info);
  if (G_UNLIKELY (default_application == NULL))
    {
      /* no default application, just popup the application chooser */
      thunar_chooser_button_activate_other (chooser_button);
      return;
    }

  /* determine all applications that claim to be able to handle the file */
  applications = thunar_vfs_mime_database_get_applications (chooser_button->database, info);

  /* make sure the default application comes first */
  lp = g_list_find (applications, default_application);
  if (G_LIKELY (lp != NULL))
    {
      applications = g_list_delete_link (applications, lp);
      g_object_unref (G_OBJECT (default_application));
    }
  applications = g_list_prepend (applications, default_application);

  /* allocate a new popup menu */
  menu = gtk_menu_new ();
  exo_gtk_object_ref_sink (GTK_OBJECT (menu));
  gtk_menu_set_screen (GTK_MENU (menu), gtk_widget_get_screen (button));

  /* determine the icon size for menus */
  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_size, &icon_size);

  /* determine the icon factory for our screen */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (button));
  icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

  /* add the other possible applications */
  for (lp = applications; lp != NULL; lp = lp->next)
    {
      item = gtk_image_menu_item_new_with_label (thunar_vfs_mime_handler_get_name (lp->data));
      g_object_set_data_full (G_OBJECT (item), I_("thunar-vfs-mime-application"), lp->data, g_object_unref);
      g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_chooser_button_activate), chooser_button);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* setup the icon for the application */
      icon_name = thunar_vfs_mime_handler_lookup_icon_name (lp->data, icon_theme);
      icon = thunar_icon_factory_load_icon (icon_factory, icon_name, icon_size, NULL, FALSE);
      image = gtk_image_new_from_pixbuf (icon);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
      gtk_widget_show (image);
      if (G_LIKELY (icon != NULL))
        g_object_unref (icon);
    }

  /* append a separator */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* release the applications list */
  g_list_free (applications);

  /* add the "Other Application..." choice */
  item = gtk_image_menu_item_new_with_mnemonic (_("_Other Application..."));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_chooser_button_activate_other), chooser_button);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* allocate a new loop and connect it to the menu */
  loop = g_main_loop_new (NULL, FALSE);
  g_signal_connect_swapped (G_OBJECT (menu), "deactivate", G_CALLBACK (g_main_loop_quit), loop);

  /* make sure the menu has atleast the same width as the chooser */
  if (menu->allocation.width < button->allocation.width)
    gtk_widget_set_size_request (menu, button->allocation.width, -1);

  /* run the menu */
  gtk_grab_add (menu);
  gtk_menu_popup (GTK_MENU (menu), NULL, NULL, menu_position, button, 0, gtk_get_current_event_time ());
  g_main_loop_run (loop);
  gtk_grab_remove (menu);
  g_main_loop_unref (loop);

  /* cleanup */
  g_object_unref (G_OBJECT (icon_factory));
  g_object_unref (G_OBJECT (menu));

  /* yeppa, that's a requirement */
  gtk_button_released (GTK_BUTTON (button));
}



/**
 * thunar_chooser_button_new:
 *
 * Allocates a new #ThunarChooserButton instance.
 *
 * Return value: the newly allocated #ThunarChooserButton.
 **/
GtkWidget*
thunar_chooser_button_new (void)
{
  return g_object_new (THUNAR_TYPE_CHOOSER_BUTTON, NULL);
}



/**
 * thunar_chooser_button_get_file:
 * @chooser_button : a #ThunarChooserButton instance.
 *
 * Returns the #ThunarFile associated with @chooser_button.
 *
 * Return value: the file associated with @chooser_button.
 **/
ThunarFile*
thunar_chooser_button_get_file (ThunarChooserButton *chooser_button)
{
  g_return_val_if_fail (THUNAR_IS_CHOOSER_BUTTON (chooser_button), NULL);
  return chooser_button->file;
}



/**
 * thunar_chooser_button_set_file:
 * @chooser_button : a #ThunarChooserButton instance.
 * @file           : a #ThunarFile or %NULL.
 *
 * Associates @chooser_button with the specified @file.
 **/
void
thunar_chooser_button_set_file (ThunarChooserButton *chooser_button,
                                ThunarFile          *file)
{
  g_return_if_fail (THUNAR_IS_CHOOSER_BUTTON (chooser_button));
  g_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  /* check if we already use that file */
  if (G_UNLIKELY (chooser_button->file == file))
    return;

  /* disconnect from the previous file */
  if (G_UNLIKELY (chooser_button->file != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (chooser_button->file), thunar_chooser_button_file_changed, chooser_button);
      g_object_unref (G_OBJECT (chooser_button->file));
    }

  /* activate the new file */
  chooser_button->file = file;

  /* connect to the new file */
  if (G_LIKELY (file != NULL))
    {
      /* take a reference */
      g_object_ref (G_OBJECT (file));

      /* stay informed about changes */
      g_signal_connect_swapped (G_OBJECT (file), "changed", G_CALLBACK (thunar_chooser_button_file_changed), chooser_button);

      /* update our state now */
      thunar_chooser_button_file_changed (chooser_button, file);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (chooser_button), "file");
}



