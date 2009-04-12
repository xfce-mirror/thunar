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
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-private.h>



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
  const gchar *content_type;
  GAppInfo    *app_info;
  GError      *error = NULL;

  _thunar_return_if_fail (THUNAR_IS_CHOOSER_BUTTON (chooser_button));
  _thunar_return_if_fail (GTK_IS_MENU_ITEM (item));

  /* verify that we still have a valid file */
  if (G_UNLIKELY (chooser_button->file == NULL))
    return;

  /* determine the application that was set for the item */
  app_info = g_object_get_data (G_OBJECT (item), "app-info");
  if (G_UNLIKELY (app_info == NULL))
    return;

  /* determine the mime info for the file */
  content_type = thunar_file_get_content_type (chooser_button->file);

  /* try to set application as default for these kind of file */
  if (!g_app_info_set_as_default_for_type (app_info, content_type, &error))
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

  _thunar_return_if_fail (THUNAR_IS_CHOOSER_BUTTON (chooser_button));

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
  const gchar *content_type;
  GAppInfo    *app_info;
  gchar       *description;

  _thunar_return_if_fail (THUNAR_IS_CHOOSER_BUTTON (chooser_button));
  _thunar_return_if_fail (chooser_button->file == file);
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* determine the content type of the file */
  content_type = thunar_file_get_content_type (file);

  /* determine the default application for that content type */
  app_info = g_app_info_get_default_for_type (content_type, FALSE);
  if (G_LIKELY (app_info != NULL))
    {
      /* setup the image for the application */
      gtk_image_set_from_gicon (GTK_IMAGE (chooser_button->image), 
                                g_app_info_get_icon (app_info),
                                GTK_ICON_SIZE_MENU);

      /* setup the label for the application */
      gtk_label_set_attributes (GTK_LABEL (chooser_button->label), NULL);
      gtk_label_set_text (GTK_LABEL (chooser_button->label), g_app_info_get_name (app_info));

      /* cleanup */
      g_object_unref (app_info);
    }
  else
    {
      /* no default application specified */
      gtk_label_set_attributes (GTK_LABEL (chooser_button->label), thunar_pango_attr_list_italic ());
      gtk_label_set_text (GTK_LABEL (chooser_button->label), _("No application selected"));
      gtk_image_set_from_pixbuf (GTK_IMAGE (chooser_button->image), NULL);
    }

  /* setup a useful tooltip for the button */
  description = g_content_type_get_description (content_type);
  thunar_gtk_widget_set_tooltip (chooser_button->button,
                                 _("The selected application is used to open "
                                   "this and other files of type \"%s\"."),
                                 description);
  g_free (description);
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
  const gchar *content_type;
  GtkWidget   *image;
  GtkWidget   *item;
  GtkWidget   *menu;
  GAppInfo    *default_app_info;
  GList       *app_infos;
  GList       *lp;

  _thunar_return_if_fail (THUNAR_IS_CHOOSER_BUTTON (chooser_button));
  _thunar_return_if_fail (chooser_button->button == button);
  _thunar_return_if_fail (GTK_IS_BUTTON (button));

  /* verify that we have a valid file */
  if (G_UNLIKELY (chooser_button->file == NULL))
    return;

  /* determine the content type for the file */
  content_type = thunar_file_get_content_type (chooser_button->file);

  /* determine the default application */
  default_app_info = g_app_info_get_default_for_type (content_type, FALSE);
  if (G_UNLIKELY (default_app_info == NULL))
    {
      /* no default application, just popup the application chooser */
      thunar_chooser_button_activate_other (chooser_button);
      g_object_unref (default_app_info);
      return;
    }

  /* determine all applications that claim to be able to handle the file */
  app_infos = g_app_info_get_all_for_type (content_type);

  /* allocate a new popup menu */
  menu = gtk_menu_new ();

  /* add the other possible applications */
  for (lp = app_infos; lp != NULL; lp = lp->next)
    {
      item = gtk_image_menu_item_new_with_label (g_app_info_get_name (lp->data));
      g_object_set_data_full (G_OBJECT (item), I_("app-info"), lp->data, g_object_unref);
      g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_chooser_button_activate), chooser_button);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* setup the icon for the application */
      image = gtk_image_new_from_gicon (g_app_info_get_icon (lp->data), GTK_ICON_SIZE_MENU);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
      gtk_widget_show (image);
    }

  /* append a separator */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* release the applications list */
  g_list_free (app_infos);

  /* add the "Other Application..." choice */
  item = gtk_image_menu_item_new_with_mnemonic (_("_Other Application..."));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (thunar_chooser_button_activate_other), chooser_button);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* make sure the menu has atleast the same width as the chooser */
  if (menu->allocation.width < button->allocation.width)
    gtk_widget_set_size_request (menu, button->allocation.width, -1);

  /* run the menu on the button's screen (takes over the floating reference of menu) */
  thunar_gtk_menu_run (GTK_MENU (menu), button, menu_position, button, 0, gtk_get_current_event_time ());

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
  _thunar_return_val_if_fail (THUNAR_IS_CHOOSER_BUTTON (chooser_button), NULL);
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
  _thunar_return_if_fail (THUNAR_IS_CHOOSER_BUTTON (chooser_button));
  _thunar_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

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



