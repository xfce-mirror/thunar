/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-properties-dialog.h>



enum
{
  PROP_0,
  PROP_FILE,
};



static void     thunar_properties_dialog_class_init           (ThunarPropertiesDialogClass *klass);
static void     thunar_properties_dialog_init                 (ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_dispose              (GObject                     *object);
static void     thunar_properties_dialog_finalize             (GObject                     *object);
static void     thunar_properties_dialog_get_property         (GObject                     *object,
                                                               guint                        prop_id,
                                                               GValue                      *value,
                                                               GParamSpec                  *pspec);
static void     thunar_properties_dialog_set_property         (GObject                     *object,
                                                               guint                        prop_id,
                                                               const GValue                *value,
                                                               GParamSpec                  *pspec);
static gboolean thunar_properties_dialog_key_press_event      (GtkWidget                   *widget,
                                                               GdkEventKey                 *event);
static void     thunar_properties_dialog_response             (GtkDialog                   *dialog,
                                                               gint                         response);
static void     thunar_properties_dialog_activate             (GtkWidget                   *entry,
                                                               ThunarPropertiesDialog      *dialog);
static gboolean thunar_properties_dialog_focus_out_event      (GtkWidget                   *entry,
                                                               GdkEventFocus               *event,
                                                               ThunarPropertiesDialog      *dialog);
static void     thunar_properties_dialog_update               (ThunarPropertiesDialog      *dialog);
static gboolean thunar_properties_dialog_rename_idle          (gpointer                     user_data);
static void     thunar_properties_dialog_rename_idle_destroy  (gpointer                     user_data);



struct _ThunarPropertiesDialogClass
{
  GtkDialogClass __parent__;
};

struct _ThunarPropertiesDialog
{
  GtkDialog __parent__;

  ThunarVfsVolumeManager *volume_manager;
  ThunarFile             *file;

  GtkWidget   *notebook;
  GtkWidget   *icon_image;
  GtkWidget   *name_entry;
  GtkWidget   *kind_label;
  GtkWidget   *modified_label;
  GtkWidget   *accessed_label;
  GtkWidget   *volume_image;
  GtkWidget   *volume_label;
  GtkWidget   *size_label;

  gint         rename_idle_id;
};



G_DEFINE_TYPE (ThunarPropertiesDialog, thunar_properties_dialog, GTK_TYPE_DIALOG);



static void
thunar_properties_dialog_class_init (ThunarPropertiesDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_properties_dialog_dispose;
  gobject_class->finalize = thunar_properties_dialog_finalize;
  gobject_class->get_property = thunar_properties_dialog_get_property;
  gobject_class->set_property = thunar_properties_dialog_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->key_press_event = thunar_properties_dialog_key_press_event;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_properties_dialog_response;

  /**
   * ThunarPropertiesDialog:file:
   *
   * The #ThunarFile whose properties are currently displayed by
   * this #ThunarPropertiesDialog. This property may also be %NULL
   * in which case nothing is displayed.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        _("File"),
                                                        _("The file displayed by the dialog"),
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));
}



static void
thunar_properties_dialog_init (ThunarPropertiesDialog *dialog)
{
  GtkWidget *table;
  GtkWidget *label;
  GtkWidget *box;
  GtkWidget *spacer;
  gchar     *text;
  gint       row = 0;

  dialog->volume_manager = thunar_vfs_volume_manager_get_default ();
  dialog->rename_idle_id = -1;

  gtk_dialog_add_buttons (GTK_DIALOG (dialog),
                          GTK_STOCK_HELP, GTK_RESPONSE_HELP,
                          GTK_STOCK_CLOSE, GTK_RESPONSE_CLOSE,
                          NULL);
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);

  dialog->notebook = gtk_notebook_new ();
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), dialog->notebook, TRUE, TRUE, 0);
  gtk_widget_show (dialog->notebook);

  table = g_object_new (GTK_TYPE_TABLE,
                        "border-width", 6,
                        "column-spacing", 12,
                        "row-spacing", 6,
                        NULL);
  label = gtk_label_new (_("General"));
  gtk_notebook_append_page (GTK_NOTEBOOK (dialog->notebook), table, label);
  gtk_widget_show (label);
  gtk_widget_show (table);


  /*
     First box (icon, name)
   */
  box = gtk_hbox_new (FALSE, 6);
  gtk_table_attach (GTK_TABLE (table), box, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (box);

  dialog->icon_image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (box), dialog->icon_image, FALSE, TRUE, 0);
  gtk_widget_show (dialog->icon_image);

  text = g_strdup_printf ("<b>%s</b>", _("Name:"));
  label = g_object_new (GTK_TYPE_LABEL, "label", text, "use-markup", TRUE, "xalign", 1.0f, NULL);
  gtk_box_pack_start (GTK_BOX (box), label, TRUE, TRUE, 0);
  gtk_widget_show (label);
  g_free (text);

  dialog->name_entry = g_object_new (GTK_TYPE_ENTRY, "editable", FALSE, NULL);
  g_signal_connect (G_OBJECT (dialog->name_entry), "activate", G_CALLBACK (thunar_properties_dialog_activate), dialog);
  g_signal_connect (G_OBJECT (dialog->name_entry), "focus-out-event", G_CALLBACK (thunar_properties_dialog_focus_out_event), dialog);
  gtk_table_attach (GTK_TABLE (table), dialog->name_entry, 1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (dialog->name_entry);

  ++row;


  spacer = g_object_new (GTK_TYPE_ALIGNMENT, "height-request", 12, NULL);
  gtk_table_attach (GTK_TABLE (table), spacer, 0, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (spacer);

  ++row;


  /*
     Second box (kind)
   */
  text = g_strdup_printf ("<b>%s</b>", _("Kind:"));
  label = g_object_new (GTK_TYPE_LABEL, "label", text, "use-markup", TRUE, "xalign", 1.0f, NULL);
  exo_binding_new (G_OBJECT (label), "visible", G_OBJECT (spacer), "visible");
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);
  g_free (text);

  dialog->kind_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  exo_binding_new (G_OBJECT (dialog->kind_label), "visible", G_OBJECT (label), "visible");
  gtk_table_attach (GTK_TABLE (table), dialog->kind_label, 1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (dialog->kind_label);

  ++row;


  spacer = g_object_new (GTK_TYPE_ALIGNMENT, "height-request", 12, NULL);
  gtk_table_attach (GTK_TABLE (table), spacer, 0, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (spacer);

  ++row;


  /*
     Third box (modified, accessed)
   */
  text = g_strdup_printf ("<b>%s</b>", _("Modified:"));
  label = g_object_new (GTK_TYPE_LABEL, "label", text, "use-markup", TRUE, "xalign", 1.0f, NULL);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);
  g_free (text);

  dialog->modified_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  exo_binding_new (G_OBJECT (dialog->modified_label), "visible", G_OBJECT (label), "visible");
  gtk_table_attach (GTK_TABLE (table), dialog->modified_label, 1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (dialog->modified_label);

  ++row;

  text = g_strdup_printf ("<b>%s</b>", _("Accessed:"));
  label = g_object_new (GTK_TYPE_LABEL, "label", text, "use-markup", TRUE, "xalign", 1.0f, NULL);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);
  g_free (text);

  dialog->accessed_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  exo_binding_new (G_OBJECT (dialog->accessed_label), "visible", G_OBJECT (label), "visible");
  gtk_table_attach (GTK_TABLE (table), dialog->accessed_label, 1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (dialog->accessed_label);

  ++row;


  spacer = g_object_new (GTK_TYPE_ALIGNMENT, "height-request", 12, NULL);
  gtk_table_attach (GTK_TABLE (table), spacer, 0, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (spacer);

  ++row;


  /*
     Fourth box (volume, size)
   */
  text = g_strdup_printf ("<b>%s</b>", _("Volume:"));
  label = g_object_new (GTK_TYPE_LABEL, "label", text, "use-markup", TRUE, "xalign", 1.0f, NULL);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);
  g_free (text);

  box = gtk_hbox_new (FALSE, 6);
  exo_binding_new (G_OBJECT (box), "visible", G_OBJECT (label), "visible");
  gtk_table_attach (GTK_TABLE (table), box, 1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (box);

  dialog->volume_image = gtk_image_new ();
  exo_binding_new (G_OBJECT (dialog->volume_image), "visible", G_OBJECT (box), "visible");
  gtk_box_pack_start (GTK_BOX (box), dialog->volume_image, FALSE, TRUE, 0);
  gtk_widget_show (dialog->volume_image);

  dialog->volume_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  exo_binding_new (G_OBJECT (dialog->volume_label), "visible", G_OBJECT (dialog->volume_image), "visible");
  gtk_box_pack_start (GTK_BOX (box), dialog->volume_label, TRUE, TRUE, 0);
  gtk_widget_show (dialog->volume_label);

  ++row;

  text = g_strdup_printf ("<b>%s</b>", _("Size:"));
  label = g_object_new (GTK_TYPE_LABEL, "label", text, "use-markup", TRUE, "xalign", 1.0f, NULL);
  gtk_table_attach (GTK_TABLE (table), label, 0, 1, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);
  g_free (text);

  dialog->size_label = g_object_new (GTK_TYPE_LABEL, "xalign", 0.0f, NULL);
  exo_binding_new (G_OBJECT (dialog->size_label), "visible", G_OBJECT (label), "visible");
  gtk_table_attach (GTK_TABLE (table), dialog->size_label, 1, 2, row, row + 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (dialog->size_label);

  ++row;


  spacer = g_object_new (GTK_TYPE_ALIGNMENT, "height-request", 12, NULL);
  gtk_table_attach (GTK_TABLE (table), spacer, 0, 2, row, row + 1, GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (spacer);

  ++row;
}



static void
thunar_properties_dialog_dispose (GObject *object)
{
  ThunarPropertiesDialog *dialog = THUNAR_PROPERTIES_DIALOG (object);

  /* reset the file displayed by the dialog */
  thunar_properties_dialog_set_file (dialog, NULL);

  (*G_OBJECT_CLASS (thunar_properties_dialog_parent_class)->dispose) (object);
}



static void
thunar_properties_dialog_finalize (GObject *object)
{
  ThunarPropertiesDialog *dialog = THUNAR_PROPERTIES_DIALOG (object);

  /* drop the reference on the volume manager */
  g_object_unref (G_OBJECT (dialog->volume_manager));

  /* be sure to cancel any pending rename idle source */
  if (G_UNLIKELY (dialog->rename_idle_id >= 0))
    g_source_remove (dialog->rename_idle_id);

  (*G_OBJECT_CLASS (thunar_properties_dialog_parent_class)->finalize) (object);
}



static void
thunar_properties_dialog_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ThunarPropertiesDialog *dialog = THUNAR_PROPERTIES_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, thunar_properties_dialog_get_file (dialog));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_properties_dialog_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ThunarPropertiesDialog *dialog = THUNAR_PROPERTIES_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILE:
      thunar_properties_dialog_set_file (dialog, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_properties_dialog_key_press_event (GtkWidget   *widget,
                                          GdkEventKey *event)
{
  if (event->keyval == GDK_Escape)
    {
      gtk_widget_destroy (widget);
      return TRUE;
    }

  return GTK_WIDGET_CLASS (thunar_properties_dialog_parent_class)->key_press_event (widget, event);
}



static void
thunar_properties_dialog_response (GtkDialog *dialog,
                                   gint       response)
{
  if (response == GTK_RESPONSE_CLOSE)
    {
      gtk_widget_destroy (GTK_WIDGET (dialog));
    }
  else if (GTK_DIALOG_CLASS (thunar_properties_dialog_parent_class)->response != NULL)
    {
      (*GTK_DIALOG_CLASS (thunar_properties_dialog_parent_class)->response) (dialog, response);
    }
}



static void
thunar_properties_dialog_activate (GtkWidget              *entry,
                                   ThunarPropertiesDialog *dialog)
{
  if (G_LIKELY (dialog->rename_idle_id < 0))
    {
      dialog->rename_idle_id = g_idle_add_full (G_PRIORITY_DEFAULT, thunar_properties_dialog_rename_idle,
                                                dialog, thunar_properties_dialog_rename_idle_destroy);
    }
}



static gboolean
thunar_properties_dialog_focus_out_event (GtkWidget              *entry,
                                          GdkEventFocus          *event,
                                          ThunarPropertiesDialog *dialog)
{
  if (G_LIKELY (dialog->rename_idle_id < 0))
    {
      dialog->rename_idle_id = g_idle_add_full (G_PRIORITY_DEFAULT, thunar_properties_dialog_rename_idle,
                                                dialog, thunar_properties_dialog_rename_idle_destroy);
    }

  return FALSE;
}



static void
thunar_properties_dialog_update (ThunarPropertiesDialog *dialog)
{
  ThunarIconFactory *icon_factory;
  ThunarVfsFileSize  size;
  ThunarVfsMimeInfo *info;
  ThunarVfsVolume   *volume;
  GtkIconTheme      *icon_theme;
  const gchar       *icon_name;
  const gchar       *name;
  GdkPixbuf         *icon;
  glong              offset;
  gchar             *size_string;
  gchar             *str;

  g_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));
  g_return_if_fail (THUNAR_IS_FILE (dialog->file));

  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (dialog)));
  icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

  /* update the icon */
  icon = thunar_file_load_icon (dialog->file, THUNAR_FILE_ICON_STATE_DEFAULT, icon_factory, 48);
  gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->icon_image), icon);
  gtk_window_set_icon (GTK_WINDOW (dialog), icon);
  if (G_LIKELY (icon != NULL))
    g_object_unref (G_OBJECT (icon));

  /* update the name */
  name = thunar_file_get_display_name (dialog->file);
  gtk_entry_set_editable (GTK_ENTRY (dialog->name_entry), thunar_file_is_renameable (dialog->file));
  gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), name);
  str = g_strdup_printf (_("%s Info"), name);
  gtk_window_set_title (GTK_WINDOW (dialog), str);
  g_free (str);

  /* grab the input focus to the name entry */
  gtk_widget_grab_focus (dialog->name_entry);

  /* select the pre-dot part of the name */
  str = strrchr (name, '.');
  if (G_LIKELY (str != NULL))
    {
      /* calculate the offset */
      offset = g_utf8_pointer_to_offset (name, str);

      /* select the region */
      if (G_LIKELY (offset > 0))
        gtk_entry_select_region (GTK_ENTRY (dialog->name_entry), 0, offset);
    }

  /* update the mime type */
  info = thunar_file_get_mime_info (dialog->file);
  gtk_label_set_text (GTK_LABEL (dialog->kind_label), thunar_vfs_mime_info_get_comment (info));
  gtk_widget_show (dialog->kind_label);
  thunar_vfs_mime_info_unref (info);

  /* update the modified time */
  str = thunar_file_get_date_string (dialog->file, THUNAR_FILE_DATE_MODIFIED);
  if (G_LIKELY (str != NULL))
    {
      gtk_label_set_text (GTK_LABEL (dialog->modified_label), str);
      gtk_widget_show (dialog->modified_label);
      g_free (str);
    }
  else
    {
      gtk_widget_hide (dialog->modified_label);
    }

  /* update the accessed time */
  str = thunar_file_get_date_string (dialog->file, THUNAR_FILE_DATE_ACCESSED);
  if (G_LIKELY (str != NULL))
    {
      gtk_label_set_text (GTK_LABEL (dialog->accessed_label), str);
      gtk_widget_show (dialog->accessed_label);
      g_free (str);
    }
  else
    {
      gtk_widget_hide (dialog->accessed_label);
    }

  /* update the volume */
  volume = thunar_file_get_volume (dialog->file, dialog->volume_manager);
  if (G_LIKELY (volume != NULL))
    {
      icon_name = thunar_vfs_volume_lookup_icon_name (volume, icon_theme);
      icon = thunar_icon_factory_load_icon (icon_factory, icon_name, 16, NULL, FALSE);
      gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->volume_image), icon);
      if (G_LIKELY (icon != NULL))
        g_object_unref (G_OBJECT (icon));

      name = thunar_vfs_volume_get_name (volume);
      gtk_label_set_text (GTK_LABEL (dialog->volume_label), name);
      gtk_widget_show (dialog->volume_label);
    }
  else
    {
      gtk_widget_hide (dialog->volume_label);
    }

  /* update the size */
  size_string = thunar_file_get_size_string (dialog->file);
  if (G_LIKELY (size_string != NULL))
    {
      if (G_LIKELY (thunar_file_get_size (dialog->file, &size)))
        {
          str = g_strdup_printf (_("%s (%u Bytes)"), size_string, (guint) size);
          gtk_label_set_text (GTK_LABEL (dialog->size_label), str);
          g_free (str);
        }
      else
        {
          gtk_label_set_text (GTK_LABEL (dialog->size_label), size_string);
        }

      gtk_widget_show (dialog->size_label);
      g_free (size_string);
    }
  else
    {
      gtk_widget_hide (dialog->size_label);
    }

  /* cleanup */
  g_object_unref (G_OBJECT (icon_factory));
}



static gboolean
thunar_properties_dialog_rename_idle (gpointer user_data)
{
  ThunarPropertiesDialog *dialog = THUNAR_PROPERTIES_DIALOG (user_data);
  const gchar            *old_name;
  GtkWidget              *message;
  GError                 *error = NULL;
  gchar                  *new_name;

  /* check if we still have a valid file and if the user is allowed to rename */
  if (G_UNLIKELY (dialog->file == NULL || !GTK_WIDGET_SENSITIVE (dialog->name_entry)))
    return FALSE;

  GDK_THREADS_ENTER ();

  /* determine new and old name */
  new_name = gtk_editable_get_chars (GTK_EDITABLE (dialog->name_entry), 0, -1);
  old_name = thunar_file_get_display_name (dialog->file);
  if (g_utf8_collate (new_name, old_name) != 0)
    {
      /* try to rename the file to the new name */
      if (!thunar_file_rename (dialog->file, new_name, &error))
        {
          /* reset the entry widget to the old name */
          gtk_entry_set_text (GTK_ENTRY (dialog->name_entry), old_name);

          /* display an error message */
          message = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                            GTK_DIALOG_DESTROY_WITH_PARENT
                                            | GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            _("Failed to rename %s."),
                                            old_name);
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message),
                                                    "%s.", error->message);
          gtk_dialog_run (GTK_DIALOG (message));
          gtk_widget_destroy (message);
          g_error_free (error);
        }
    }
  g_free (new_name);

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_properties_dialog_rename_idle_destroy (gpointer user_data)
{
  THUNAR_PROPERTIES_DIALOG (user_data)->rename_idle_id = -1;
}



/**
 * thunar_properties_dialog_new:
 *
 * Allocates a new #ThunarPropertiesDialog instance,
 * that is not associated with any #ThunarFile.
 *
 * Return value: the newly allocated #ThunarPropertiesDialog
 *               instance.
 **/
GtkWidget*
thunar_properties_dialog_new (void)
{
  return g_object_new (THUNAR_TYPE_PROPERTIES_DIALOG, NULL);
}



/**
 * thunar_properties_dialog_get_file:
 * @dialog : a #ThunarPropertiesDialog.
 *
 * Returns the #ThunarFile currently being displayed
 * by @dialog or %NULL if @dialog doesn't display
 * any file right now.
 *
 * Return value: the #ThunarFile displayed by @dialog
 *               or %NULL.
 **/
ThunarFile*
thunar_properties_dialog_get_file (ThunarPropertiesDialog *dialog)
{
  g_return_val_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog), NULL);
  return dialog->file;
}



/**
 * thunar_properties_dialog_set_file:
 * @dialog : a #ThunarPropertiesDialog.
 * @file   : a #ThunarFile or %NULL.
 *
 * Sets the #ThunarFile that is displayed by @dialog
 * to @file.
 **/
void
thunar_properties_dialog_set_file (ThunarPropertiesDialog *dialog,
                                   ThunarFile             *file)
{
  g_return_if_fail (THUNAR_IS_PROPERTIES_DIALOG (dialog));
  g_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  /* disconnect from any previously set file */
  if (dialog->file != NULL)
    {
      /* unregister our file watch */
      thunar_file_unwatch (dialog->file);

      /* unregister handlers */
      g_signal_handlers_disconnect_by_func (G_OBJECT (dialog->file), thunar_properties_dialog_update, dialog);
      g_signal_handlers_disconnect_by_func (G_OBJECT (dialog->file), gtk_widget_destroy, dialog);

      g_object_unref (G_OBJECT (dialog->file));
    }

  /* activate the new file */
  dialog->file = file;

  /* connect to the new file */
  if (file != NULL)
    {
      g_object_ref (G_OBJECT (file));

      /* watch the file for changes */
      thunar_file_watch (file);

      /* install signal handlers */
      g_signal_connect_swapped (G_OBJECT (file), "changed", G_CALLBACK (thunar_properties_dialog_update), dialog);
      g_signal_connect_swapped (G_OBJECT (file), "destroy", G_CALLBACK (gtk_widget_destroy), dialog);

      /* update the UI for the new file */
      thunar_properties_dialog_update (dialog);
    }

  /* tell everybody that we have a new file here */
  g_object_notify (G_OBJECT (dialog), "file");
}



