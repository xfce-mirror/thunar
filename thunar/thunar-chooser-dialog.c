/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>.
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

#include <thunar/thunar-chooser-dialog.h>
#include <thunar/thunar-chooser-model.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILE,
  PROP_OPEN,
};



static void     thunar_chooser_dialog_class_init      (ThunarChooserDialogClass *klass);
static void     thunar_chooser_dialog_init            (ThunarChooserDialog      *dialog);
static void     thunar_chooser_dialog_dispose         (GObject                  *object);
static void     thunar_chooser_dialog_get_property    (GObject                  *object,
                                                       guint                     prop_id,
                                                       GValue                   *value,
                                                       GParamSpec               *pspec);
static void     thunar_chooser_dialog_set_property    (GObject                  *object,
                                                       guint                     prop_id,
                                                       const GValue             *value,
                                                       GParamSpec               *pspec);
static void     thunar_chooser_dialog_realize         (GtkWidget                *widget);
static void     thunar_chooser_dialog_response        (GtkDialog                *widget,
                                                       gint                      response);
static gboolean thunar_chooser_dialog_selection_func  (GtkTreeSelection         *selection,
                                                       GtkTreeModel             *model,
                                                       GtkTreePath              *path,
                                                       gboolean                  path_currently_selected,
                                                       gpointer                  user_data);
static void     thunar_chooser_dialog_update_accept   (ThunarChooserDialog      *dialog);
static void     thunar_chooser_dialog_update_header   (ThunarChooserDialog      *dialog);
static void     thunar_chooser_dialog_browse          (GtkWidget                *button,
                                                       ThunarChooserDialog      *dialog);
static void     thunar_chooser_dialog_notify_expanded (GtkExpander              *expander,
                                                       GParamSpec               *pspec,
                                                       ThunarChooserDialog      *dialog);
static void     thunar_chooser_dialog_notify_loading  (ThunarChooserModel       *model,
                                                       GParamSpec               *pspec,
                                                       ThunarChooserDialog      *dialog);
static void     thunar_chooser_dialog_row_activated   (GtkTreeView              *treeview,
                                                       GtkTreePath              *path,
                                                       GtkTreeViewColumn        *column,
                                                       ThunarChooserDialog      *dialog);



struct _ThunarChooserDialogClass
{
  GtkDialogClass __parent__;
};

struct _ThunarChooserDialog
{
  GtkDialog __parent__;

  ThunarFile *file;
  gboolean    open;

  GtkWidget  *header_image;
  GtkWidget  *header_label;
  GtkWidget  *tree_view;
  GtkWidget  *custom_expander;
  GtkWidget  *custom_entry;
  GtkWidget  *cancel_button;
  GtkWidget  *accept_button;
};



G_DEFINE_TYPE (ThunarChooserDialog, thunar_chooser_dialog, GTK_TYPE_DIALOG);



static void
thunar_chooser_dialog_class_init (ThunarChooserDialogClass *klass)
{
  GtkDialogClass *gtkdialog_class;
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_chooser_dialog_dispose;
  gobject_class->get_property = thunar_chooser_dialog_get_property;
  gobject_class->set_property = thunar_chooser_dialog_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->realize = thunar_chooser_dialog_realize;

  gtkdialog_class = GTK_DIALOG_CLASS (klass);
  gtkdialog_class->response = thunar_chooser_dialog_response;

  /**
   * ThunarChooserDialog::file:
   *
   * The #ThunarFile for which an application should be chosen.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        _("File"),
                                                        _("The file for which an application should be chosen"),
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarChooserDialog::open:
   *
   * Whether the chooser should open the specified file.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_OPEN,
                                   g_param_spec_boolean ("open",
                                                         _("Open"),
                                                         _("Whether the chooser should open the specified file"),
                                                         FALSE,
                                                         G_PARAM_CONSTRUCT | EXO_PARAM_READWRITE));
}



static void
thunar_chooser_dialog_init (ThunarChooserDialog *dialog)
{
  GtkTreeViewColumn *column;
  GtkTreeSelection  *selection;
  GtkCellRenderer   *renderer;
  GtkWidget         *header;
  GtkWidget         *button;
  GtkWidget         *image;
  GtkWidget         *hbox;
  GtkWidget         *vbox;
  GtkWidget         *box;
  GtkWidget         *swin;

  /* setup basic window properties */
  gtk_dialog_set_has_separator (GTK_DIALOG (dialog), FALSE);
  gtk_window_set_destroy_with_parent (GTK_WINDOW (dialog), TRUE);
  gtk_window_set_resizable (GTK_WINDOW (dialog), FALSE);
  gtk_window_set_title (GTK_WINDOW (dialog), _("Open With"));

  /* create the main widget box */
  vbox = g_object_new (GTK_TYPE_VBOX, "border-width", 6, "spacing", 12, NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  /* create the header box */
  header = gtk_hbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), header, FALSE, FALSE, 0);
  gtk_widget_show (header);

  /* create the header image */
  dialog->header_image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (header), dialog->header_image, FALSE, FALSE, 0);
  gtk_widget_show (dialog->header_image);

  /* create the header label */
  dialog->header_label = gtk_label_new ("");
  gtk_misc_set_alignment (GTK_MISC (dialog->header_label), 0.0f, 0.5f);
  gtk_label_set_line_wrap (GTK_LABEL (dialog->header_label), TRUE);
  gtk_widget_set_size_request (dialog->header_label, 350, -1);
  gtk_box_pack_start (GTK_BOX (header), dialog->header_label, FALSE, FALSE, 0);
  gtk_widget_show (dialog->header_label);

  /* create the view box */
  box = gtk_vbox_new (FALSE, 6);
  gtk_box_pack_start (GTK_BOX (vbox), box, TRUE, TRUE, 0);
  gtk_widget_show (box);

  /* create the scrolled window for the tree view */
  swin = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_size_request (swin, -1, 275);
  gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (swin), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
  gtk_scrolled_window_set_shadow_type (GTK_SCROLLED_WINDOW (swin), GTK_SHADOW_IN);
  gtk_box_pack_start (GTK_BOX (box), swin, TRUE, TRUE, 0);
  gtk_widget_show (swin);

  /* create the tree view */
  dialog->tree_view = g_object_new (GTK_TYPE_TREE_VIEW, "headers-visible", FALSE, NULL);
  g_signal_connect (G_OBJECT (dialog->tree_view), "row-activated", G_CALLBACK (thunar_chooser_dialog_row_activated), dialog);
  gtk_container_add (GTK_CONTAINER (swin), dialog->tree_view);
  gtk_widget_show (dialog->tree_view);

  /* append the tree view column */
  column = g_object_new (GTK_TYPE_TREE_VIEW_COLUMN, "expand", TRUE, NULL);
  renderer = gtk_cell_renderer_pixbuf_new ();
  gtk_tree_view_column_pack_start (column, renderer, FALSE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "pixbuf", THUNAR_CHOOSER_MODEL_COLUMN_ICON,
                                       NULL);
  renderer = gtk_cell_renderer_text_new ();
  gtk_tree_view_column_pack_start (column, renderer, TRUE);
  gtk_tree_view_column_set_attributes (column, renderer,
                                       "style", THUNAR_CHOOSER_MODEL_COLUMN_STYLE,
                                       "style-set", THUNAR_CHOOSER_MODEL_COLUMN_STYLE_SET,
                                       "text", THUNAR_CHOOSER_MODEL_COLUMN_NAME,
                                       NULL);
  gtk_tree_view_column_set_sort_column_id (column, THUNAR_CHOOSER_MODEL_COLUMN_NAME);
  gtk_tree_view_append_column (GTK_TREE_VIEW (dialog->tree_view), column);

  /* create the "Custom command" expand */
  dialog->custom_expander = gtk_expander_new_with_mnemonic (_("Use a _custom command:"));
  exo_binding_new_with_negation (G_OBJECT (dialog->custom_expander), "expanded", G_OBJECT (dialog->tree_view), "sensitive");
  g_signal_connect (G_OBJECT (dialog->custom_expander), "notify::expanded", G_CALLBACK (thunar_chooser_dialog_notify_expanded), dialog);
  gtk_box_pack_start (GTK_BOX (box), dialog->custom_expander, FALSE, FALSE, 0);
  gtk_widget_show (dialog->custom_expander);

  /* create the "Custom command" box */
  hbox = gtk_hbox_new (FALSE, 6);
  gtk_container_add (GTK_CONTAINER (dialog->custom_expander), hbox);
  gtk_widget_show (hbox);

  /* create the "Custom command" entry */
  dialog->custom_entry = g_object_new (GTK_TYPE_ENTRY, "activates-default", TRUE, NULL);
  g_signal_connect_swapped (G_OBJECT (dialog->custom_entry), "changed", G_CALLBACK (thunar_chooser_dialog_update_accept), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), dialog->custom_entry, TRUE, TRUE, 0);
  gtk_widget_show (dialog->custom_entry);

  /* create the "Custom command" button */
  button = gtk_button_new_with_mnemonic (_("_Browse"));
  g_signal_connect (G_OBJECT (button), "clicked", G_CALLBACK (thunar_chooser_dialog_browse), dialog);
  gtk_box_pack_start (GTK_BOX (hbox), button, FALSE, FALSE, 0);
  gtk_widget_show (button);

  /* create the image for the browse button */
  image = gtk_image_new_from_stock (GTK_STOCK_OPEN, GTK_ICON_SIZE_BUTTON);
  gtk_button_set_image (GTK_BUTTON (button), image);
  gtk_widget_show (image);

  /* add the "Cancel" button */
  dialog->cancel_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);

  /* add the "Ok"/"Open" button */
  dialog->accept_button = gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_OK, GTK_RESPONSE_ACCEPT);
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, FALSE);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);

  /* update the "Ok"/"Open" button whenever the tree selection changes */
  selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
  gtk_tree_selection_set_select_function (selection, thunar_chooser_dialog_selection_func, dialog, NULL);
  g_signal_connect_swapped (G_OBJECT (selection), "changed", G_CALLBACK (thunar_chooser_dialog_update_accept), dialog);
}



static void
thunar_chooser_dialog_dispose (GObject *object)
{
  ThunarChooserDialog *dialog = THUNAR_CHOOSER_DIALOG (object);

  /* drop the reference on the file */
  thunar_chooser_dialog_set_file (dialog, NULL);

  (*G_OBJECT_CLASS (thunar_chooser_dialog_parent_class)->dispose) (object);
}



static void
thunar_chooser_dialog_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ThunarChooserDialog *dialog = THUNAR_CHOOSER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, thunar_chooser_dialog_get_file (dialog));
      break;

    case PROP_OPEN:
      g_value_set_boolean (value, thunar_chooser_dialog_get_open (dialog));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_chooser_dialog_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ThunarChooserDialog *dialog = THUNAR_CHOOSER_DIALOG (object);

  switch (prop_id)
    {
    case PROP_FILE:
      thunar_chooser_dialog_set_file (dialog, g_value_get_object (value));
      break;

    case PROP_OPEN:
      thunar_chooser_dialog_set_open (dialog, g_value_get_boolean (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_chooser_dialog_realize (GtkWidget *widget)
{
  ThunarChooserDialog *dialog = THUNAR_CHOOSER_DIALOG (widget);
  GtkTreeModel        *model;
  GdkCursor           *cursor;

  /* let the GtkWindow class realize the dialog */
  (*GTK_WIDGET_CLASS (thunar_chooser_dialog_parent_class)->realize) (widget);

  /* update the dialog header */
  thunar_chooser_dialog_update_header (dialog);

  /* setup a watch cursor if we're currently loading the model */
  model = gtk_tree_view_get_model (GTK_TREE_VIEW (dialog->tree_view));
  if (thunar_chooser_model_get_loading (THUNAR_CHOOSER_MODEL (model)))
    {
      cursor = gdk_cursor_new (GDK_WATCH);
      gdk_window_set_cursor (widget->window, cursor);
      gdk_cursor_unref (cursor);
    }
}



static void
thunar_chooser_dialog_response (GtkDialog *widget,
                                gint       response)
{
  ThunarVfsMimeApplication *application = NULL;
  ThunarVfsMimeDatabase    *mime_database;
  ThunarChooserDialog      *dialog = THUNAR_CHOOSER_DIALOG (widget);
  ThunarVfsMimeInfo        *mime_info;
  GtkTreeSelection         *selection;
  GtkTreeModel             *model;
  GtkTreeIter               iter;
  const gchar              *exec;
  GtkWidget                *message;
  gboolean                  succeed;
  GError                   *error = NULL;
  gchar                    *name;
  gchar                    *s;
  GList                     list;

  /* no special processing for non-accept responses */
  if (G_UNLIKELY (response != GTK_RESPONSE_ACCEPT))
    return;

  /* grab a reference on the mime database */
  mime_database = thunar_vfs_mime_database_get_default ();

  /* determine the mime info for the file */
  mime_info = thunar_file_get_mime_info (dialog->file);

  /* determine the application that was chosen by the user */
  if (!gtk_expander_get_expanded (GTK_EXPANDER (dialog->custom_expander)))
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        gtk_tree_model_get (model, &iter, THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, &application, -1);
    }
  else
    {
      /* determine the command line for the custom command */
      exec = gtk_entry_get_text (GTK_ENTRY (dialog->custom_entry));

      /* determine the name for the custom command */
      name = g_strdup (exec);
      s = strchr (name, ' ');
      if (G_UNLIKELY (s != NULL))
        *s = '\0';

      /* try to add an application for the custom command */
      application = thunar_vfs_mime_database_add_application (mime_database, mime_info, name, exec, &error);

      /* verify the application */
      if (G_UNLIKELY (application == NULL))
        {
          message = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                            GTK_DIALOG_DESTROY_WITH_PARENT
                                            | GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            _("Failed to add new application %s."), name);
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s.", error->message);
          gtk_dialog_run (GTK_DIALOG (message));
          gtk_widget_destroy (message);
          g_error_free (error);
        }

      /* cleanup */
      g_free (name);
    }

  /* verify that we have a valid application */
  if (G_UNLIKELY (application == NULL))
    goto cleanup;

  /* remember the application as default for these kind of file */
  succeed = thunar_vfs_mime_database_set_default_application (mime_database, mime_info, application, &error);

  /* verify that we were successfull */
  if (G_UNLIKELY (!succeed))
    {
      message = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                        GTK_DIALOG_DESTROY_WITH_PARENT
                                        | GTK_DIALOG_MODAL,
                                        GTK_MESSAGE_ERROR,
                                        GTK_BUTTONS_CLOSE,
                                        _("Failed to set default application for \"%s\"."),
                                        thunar_file_get_display_name (dialog->file));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s.", error->message);
      gtk_dialog_run (GTK_DIALOG (message));
      gtk_widget_destroy (message);
      g_error_free (error);
    }
  else if (G_LIKELY (dialog->open))
    {
      /* open the file using the specified application */
      list.data = thunar_file_get_uri (dialog->file); list.next = list.prev = NULL;
      if (!thunar_vfs_mime_application_exec (application, gtk_widget_get_screen (GTK_WIDGET (dialog)), &list, &error))
        {
          message = gtk_message_dialog_new (GTK_WINDOW (dialog),
                                            GTK_DIALOG_DESTROY_WITH_PARENT
                                            | GTK_DIALOG_MODAL,
                                            GTK_MESSAGE_ERROR,
                                            GTK_BUTTONS_CLOSE,
                                            _("Failed execute %s."),
                                            thunar_vfs_mime_application_get_name (application));
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s.", error->message);
          gtk_dialog_run (GTK_DIALOG (message));
          gtk_widget_destroy (message);
          g_error_free (error);
        }
    }

  /* cleanup */
  thunar_vfs_mime_application_unref (application);
cleanup:
  g_object_unref (G_OBJECT (mime_database));
  thunar_vfs_mime_info_unref (mime_info);
}



static gboolean
thunar_chooser_dialog_selection_func (GtkTreeSelection *selection,
                                      GtkTreeModel     *model,
                                      GtkTreePath      *path,
                                      gboolean          path_currently_selected,
                                      gpointer          user_data)
{
  GtkTreeIter iter;
  gboolean    permitted = TRUE;
  GValue      value = { 0, };

  /* we can always change the selection if the path is already selected */
  if (G_UNLIKELY (!path_currently_selected))
    {
      /* check if there's an application for the path */
      gtk_tree_model_get_iter (model, &iter, path);
      gtk_tree_model_get_value (model, &iter, THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, &value);
      permitted = (g_value_get_boxed (&value) != NULL);
      g_value_unset (&value);
    }

  return permitted;
}



static void
thunar_chooser_dialog_update_accept (ThunarChooserDialog *dialog)
{
  GtkTreeSelection *selection;
  GtkTreeModel     *model;
  GtkTreeIter       iter;
  const gchar      *text;
  gboolean          sensitive = FALSE;
  GValue            value = { 0, };

  g_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));

  if (gtk_expander_get_expanded (GTK_EXPANDER (dialog->custom_expander)))
    {
      /* check if the user entered a valid custom command */
      text = gtk_entry_get_text (GTK_ENTRY (dialog->custom_entry));
      sensitive = (text != NULL && *text != '\0');
    }
  else
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
      if (gtk_tree_selection_get_selected (selection, &model, &iter))
        {
          /* check if the selected row refers to a valid application */
          gtk_tree_model_get_value (model, &iter, THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, &value);
          sensitive = (g_value_get_boxed (&value) != NULL);
          g_value_unset (&value);
        }
    }

  /* update the "Ok"/"Open" button sensitivity */
  gtk_dialog_set_response_sensitive (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT, sensitive);
}



static void
thunar_chooser_dialog_update_header (ThunarChooserDialog *dialog)
{
  ThunarVfsMimeInfo *mime_info;
  ThunarIconFactory *icon_factory;
  GtkIconTheme      *icon_theme;
  const gchar       *icon_name;
  GdkPixbuf         *icon;
  gchar             *text;

  g_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));
  g_return_if_fail (GTK_WIDGET_REALIZED (dialog));

  /* check if we have a valid file set */
  if (G_UNLIKELY (dialog->file == NULL))
    {
#if GTK_CHECK_VERSION(2,8,0)
      gtk_image_clear (GTK_IMAGE (dialog->header_image));
#endif
      gtk_label_set_text (GTK_LABEL (dialog->header_label), NULL);
    }
  else
    {
      /* determine the mime info for the file */
      mime_info = thunar_file_get_mime_info (dialog->file);

      /* determine the icon theme/factory for the widget */
      icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (dialog)));
      icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

      /* update the header image with the icon for the mime type */
      icon_name = thunar_vfs_mime_info_lookup_icon_name (mime_info, icon_theme);
      icon = thunar_icon_factory_load_icon (icon_factory, icon_name, 48, NULL, FALSE);
      gtk_image_set_from_pixbuf (GTK_IMAGE (dialog->header_image), icon);
      if (G_LIKELY (icon != NULL))
        g_object_unref (G_OBJECT (icon));

      /* update the header label */
      text = g_strdup_printf (_("Open <i>%s</i> and other files of type \"%s\" with:"),
                              thunar_file_get_display_name (dialog->file),
                              thunar_vfs_mime_info_get_comment (mime_info));
      gtk_label_set_markup (GTK_LABEL (dialog->header_label), text);
      g_free (text);

      /* cleanup */
      g_object_unref (G_OBJECT (icon_factory));
      thunar_vfs_mime_info_unref (mime_info);
    }
}



static void
thunar_chooser_dialog_browse (GtkWidget           *button,
                              ThunarChooserDialog *dialog)
{
  GtkFileFilter *filter;
  GtkWidget     *chooser;
  gchar         *filename;
  gchar         *s;

  chooser = gtk_file_chooser_dialog_new (_("Select an Application"),
                                         GTK_WINDOW (dialog),
                                         GTK_FILE_CHOOSER_ACTION_OPEN,
                                         GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                         GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                                         NULL);
  gtk_file_chooser_set_local_only (GTK_FILE_CHOOSER (chooser), TRUE);

  /* add file chooser filters */
  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("All Files"));
  gtk_file_filter_add_pattern (filter, "*");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Executable Files"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-executable");
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);
  gtk_file_chooser_set_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Perl Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-perl");
  gtk_file_filter_add_pattern (filter, "*.pl");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Python Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-python");
  gtk_file_filter_add_pattern (filter, "*.py");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Ruby Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-ruby");
  gtk_file_filter_add_pattern (filter, "*.rb");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  filter = gtk_file_filter_new ();
  gtk_file_filter_set_name (filter, _("Shell Scripts"));
  gtk_file_filter_add_mime_type (filter, "application/x-csh");
  gtk_file_filter_add_mime_type (filter, "application/x-shellscript");
  gtk_file_filter_add_pattern (filter, "*.sh");
  gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (chooser), filter);

  /* use the bindir as default folder */
  gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (chooser), BINDIR);

  /* setup the currently selected file */
  filename = gtk_editable_get_chars (GTK_EDITABLE (dialog->custom_entry), 0, -1);
  if (G_LIKELY (filename != NULL))
    {
      /* use only the first argument */
      s = strchr (filename, ' ');
      if (G_UNLIKELY (s != NULL))
        *s = '\0';

      if (G_LIKELY (*filename != '\0'))
        gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (chooser), filename);
      g_free (filename);
    }

  /* run the chooser dialog */
  if (gtk_dialog_run (GTK_DIALOG (chooser)) == GTK_RESPONSE_ACCEPT)
    {
      filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (chooser));
      gtk_entry_set_text (GTK_ENTRY (dialog->custom_entry), filename);
      g_free (filename);
    }

  gtk_widget_destroy (chooser);
}



static void
thunar_chooser_dialog_notify_expanded (GtkExpander         *expander,
                                       GParamSpec          *pspec,
                                       ThunarChooserDialog *dialog)
{
  GtkTreeSelection *selection;

  g_return_if_fail (GTK_IS_EXPANDER (expander));
  g_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));

  /* clear the application selection whenever the expander
   * is expanded to avoid confusion for the user.
   */
  if (gtk_expander_get_expanded (expander))
    {
      selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (dialog->tree_view));
      gtk_tree_selection_unselect_all (selection);
    }

  /* update the sensitivity of the "Ok"/"Open" button */
  thunar_chooser_dialog_update_accept (dialog);
}



static void
thunar_chooser_dialog_notify_loading (ThunarChooserModel  *model,
                                      GParamSpec          *pspec,
                                      ThunarChooserDialog *dialog)
{
  GtkTreePath *path;
  GtkTreeIter  iter;

  g_return_if_fail (THUNAR_IS_CHOOSER_MODEL (model));
  g_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));

  /* expand the first tree view row (the recommended applications) */
  if (G_LIKELY (gtk_tree_model_get_iter_first (GTK_TREE_MODEL (model), &iter)))
    {
      path = gtk_tree_model_get_path (GTK_TREE_MODEL (model), &iter);
      gtk_tree_view_expand_to_path (GTK_TREE_VIEW (dialog->tree_view), path);
      gtk_tree_path_free (path);
    }

  /* reset the cursor */
  if (G_LIKELY (GTK_WIDGET_REALIZED (dialog)))
    gdk_window_set_cursor (GTK_WIDGET (dialog)->window, NULL);

  /* grab focus to the tree view widget */
  if (G_LIKELY (GTK_WIDGET_REALIZED (dialog->tree_view)))
    gtk_widget_grab_focus (dialog->tree_view);
}



static void
thunar_chooser_dialog_row_activated (GtkTreeView         *treeview,
                                     GtkTreePath         *path,
                                     GtkTreeViewColumn   *column,
                                     ThunarChooserDialog *dialog)
{
  GtkTreeModel *model;
  GtkTreeIter   iter;
  GValue        value = { 0, };

  g_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));
  g_return_if_fail (GTK_IS_TREE_VIEW (treeview));

  /* determine the current chooser model */
  model = gtk_tree_view_get_model (treeview);
  if (G_UNLIKELY (model == NULL))
    return;

  /* determine the application for the tree path */
  gtk_tree_model_get_iter (model, &iter, path);
  gtk_tree_model_get_value (model, &iter, THUNAR_CHOOSER_MODEL_COLUMN_APPLICATION, &value);

  /* check if the row refers to a valid application */
  if (G_LIKELY (g_value_get_boxed (&value) != NULL))
    {
      /* emit the accept dialog response */
      gtk_dialog_response (GTK_DIALOG (dialog), GTK_RESPONSE_ACCEPT);
    }
  else if (gtk_tree_view_row_expanded (treeview, path))
    {
      /* collapse the path that were double clicked */
      gtk_tree_view_collapse_row (treeview, path);
    }
  else
    {
      /* expand the path that were double clicked */
      gtk_tree_view_expand_to_path (treeview, path);
    }

  /* cleanup */
  g_value_unset (&value);
}



/**
 * thunar_chooser_dialog_new:
 * @parent : transient parent of the dialog or %NULL.
 * @file   : the #ThunarFile for which an application should be chosen.
 * @open   : whether to also open the @file.
 *
 * Allocates a new #ThunarChooserDialog with the given parameters.
 *
 * Return value: the newly allocated #ThunarChooserDialog.
 **/
GtkWidget*
thunar_chooser_dialog_new (GtkWindow  *parent,
                           ThunarFile *file,
                           gboolean    open)
{
  GtkWidget *dialog;

  g_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), NULL);
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  dialog = g_object_new (THUNAR_TYPE_CHOOSER_DIALOG,
                         "file", file,
                         "open", open,
                         NULL);

  if (G_LIKELY (parent != NULL))
    gtk_window_set_transient_for (GTK_WINDOW (dialog), parent);

  return dialog;
}



/**
 * thunar_chooser_dialog_get_file:
 * @dialog : a #ThunarChooserDialog.
 *
 * Returns the #ThunarFile currently associated with @dialog or
 * %NULL if there's no such association.
 *
 * Return value: the #ThunarFile associated with @dialog.
 **/
ThunarFile*
thunar_chooser_dialog_get_file (ThunarChooserDialog *dialog)
{
  g_return_val_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog), NULL);
  return dialog->file;
}



/**
 * thunar_chooser_dialog_set_file:
 * @dialog : a #ThunarChooserDialog.
 * @file   : a #ThunarFile or %NULL.
 *
 * Associates @dialog with @file.
 **/
void
thunar_chooser_dialog_set_file (ThunarChooserDialog *dialog,
                                ThunarFile          *file)
{
  ThunarChooserModel *model;
  ThunarVfsMimeInfo  *mime_info;

  g_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));
  g_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  /* disconnect from the previous file */
  if (G_LIKELY (dialog->file != NULL))
    {
      /* unset the chooser model */
      gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->tree_view), NULL);

      /* disconnect us from the file */
      g_signal_handlers_disconnect_by_func (G_OBJECT (dialog->file), gtk_widget_destroy, dialog);
      thunar_file_unwatch (THUNAR_FILE (dialog->file));
      g_object_unref (G_OBJECT (dialog->file));
    }

  /* activate the new file */
  dialog->file = file;

  /* connect to the new file */
  if (G_LIKELY (file != NULL))
    {
      /* take a reference on the file */
      g_object_ref (G_OBJECT (file));

      /* watch the file for changes */
      thunar_file_watch (dialog->file);

      /* destroy the chooser dialog if the file is deleted */
      g_signal_connect_swapped (G_OBJECT (file), "destroy", G_CALLBACK (gtk_widget_destroy), dialog);

      /* allocate the new chooser model */
      mime_info = thunar_file_get_mime_info (file);
      model = thunar_chooser_model_new (mime_info);
      gtk_tree_view_set_model (GTK_TREE_VIEW (dialog->tree_view), GTK_TREE_MODEL (model));
      g_signal_connect (G_OBJECT (model), "notify::loading", G_CALLBACK (thunar_chooser_dialog_notify_loading), dialog);
      thunar_vfs_mime_info_unref (mime_info);
      g_object_unref (G_OBJECT (model));
    }

  /* update the header */
  if (GTK_WIDGET_REALIZED (dialog))
    thunar_chooser_dialog_update_header (dialog);

  /* notify listeners */
  g_object_notify (G_OBJECT (dialog), "file");
}



/**
 * thunar_chooser_dialog_get_open:
 * @dialog : a #ThunarChooserDialog.
 *
 * Tells whether the chooser @dialog should open the file.
 *
 * Return value: %TRUE if @dialog should open the file.
 **/
gboolean
thunar_chooser_dialog_get_open (ThunarChooserDialog *dialog)
{
  g_return_val_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog), FALSE);
  return dialog->open;
}



/**
 * thunar_chooser_dialog_set_open:
 * @dialog : a #ThunarChooserDialog.
 * @open   : %TRUE if the chooser @dialog should open the file.
 *
 * Sets whether the chooser @dialog should open the file.
 **/
void
thunar_chooser_dialog_set_open (ThunarChooserDialog *dialog,
                                gboolean             open)
{
  g_return_if_fail (THUNAR_IS_CHOOSER_DIALOG (dialog));

  /* apply the new state */
  dialog->open = open;

  /* change the accept button label text */
  gtk_button_set_label (GTK_BUTTON (dialog->accept_button), open ? GTK_STOCK_OPEN : GTK_STOCK_OK);

  /* notify listeners */
  g_object_notify (G_OBJECT (dialog), "open");
}


