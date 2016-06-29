/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2011 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
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
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-io-jobs.h>
#include <thunar/thunar-job.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-util.h>



/**
 * thunar_dialogs_show_rename_file:
 * @parent : a #GtkWidget on which the error dialog should be shown, or a #GdkScreen
 *           if no #GtkWidget is known. May also be %NULL, in which case the default
 *           #GdkScreen will be used.
 * @file   : the #ThunarFile we're going to rename.
 *
 * Displays the Thunar rename dialog for a single file rename.
 *
 * Return value: The #ThunarJob responsible for renaming the file or
 *               %NULL if there was no renaming required.
 **/
ThunarJob *
thunar_dialogs_show_rename_file (gpointer    parent,
                                 ThunarFile *file)
{
  ThunarIconFactory *icon_factory;
  GtkIconTheme      *icon_theme;
  const gchar       *filename;
  const gchar       *text;
  ThunarJob         *job = NULL;
  GtkWidget         *dialog;
  GtkWidget         *entry;
  GtkWidget         *label;
  GtkWidget         *image;
  GtkWidget         *table;
  GtkWindow         *window;
  GdkPixbuf         *icon;
  GdkScreen         *screen;
  glong              offset;
  gchar             *title;
  gint               response;
  PangoLayout       *layout;
  gint               layout_width;
  gint               layout_offset;
  gint               parent_width = 500;

  _thunar_return_val_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WINDOW (parent), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);

  /* parse the parent window and screen */
  screen = thunar_util_parse_parent (parent, &window);

  /* get the filename of the file */
  filename = thunar_file_get_display_name (file);

  /* create a new dialog window */
  title = g_strdup_printf (_("Rename \"%s\""), filename);
  dialog = gtk_dialog_new_with_buttons (title,
                                        window,
                                        GTK_DIALOG_MODAL
                                        | GTK_DIALOG_NO_SEPARATOR
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        _("_Rename"), GTK_RESPONSE_OK,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_OK);
  g_free (title);

  /* move the dialog to the appropriate screen */
  if (G_UNLIKELY (window == NULL && screen != NULL))
    gtk_window_set_screen (GTK_WINDOW (dialog), screen);

  table = g_object_new (GTK_TYPE_TABLE, "border-width", 6, "column-spacing", 6, "row-spacing", 3, NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);

  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (dialog));
  icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);
  icon = thunar_icon_factory_load_file_icon (icon_factory, file, THUNAR_FILE_ICON_STATE_DEFAULT, 48);
  g_object_unref (G_OBJECT (icon_factory));

  image = gtk_image_new_from_pixbuf (icon);
  gtk_misc_set_padding (GTK_MISC (image), 6, 6);
  gtk_table_attach (GTK_TABLE (table), image, 0, 1, 0, 2, GTK_FILL, GTK_FILL, 0, 0);
  g_object_unref (G_OBJECT (icon));
  gtk_widget_show (image);

  label = gtk_label_new (_("Enter the new name:"));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_table_attach (GTK_TABLE (table), label, 1, 2, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);

  entry = gtk_entry_new ();
  gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
  gtk_table_attach (GTK_TABLE (table), entry, 1, 2, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (entry);

  /* setup the old filename */
  gtk_entry_set_text (GTK_ENTRY (entry), filename);

  /* check if we don't have a directory here */
  if (!thunar_file_is_directory (file))
    {
      /* check if the filename contains an extension */
      text = thunar_util_str_get_extension (filename);
      if (G_LIKELY (text != NULL))
        {
          /* grab focus to the entry first, else the selection will be altered later */
          gtk_widget_grab_focus (entry);

          /* determine the UTF-8 char offset */
          offset = g_utf8_pointer_to_offset (filename, text);

          /* select the text prior to the dot */
          if (G_LIKELY (offset > 0))
            gtk_editable_select_region (GTK_EDITABLE (entry), 0, offset);
        }
    }

  /* get the size the entry requires to render the full text */
  layout = gtk_entry_get_layout (GTK_ENTRY (entry));
  pango_layout_get_pixel_size (layout, &layout_width, NULL);
  gtk_entry_get_layout_offsets (GTK_ENTRY (entry), &layout_offset, NULL);
  layout_width += (layout_offset * 2) + (12 * 4) + 48; /* 12px free space in entry */

  /* parent window width */
  if (G_LIKELY (window != NULL))
    {
      /* keep below 90% of the parent window width */
      gtk_window_get_size (GTK_WINDOW (window), &parent_width, NULL);
      parent_width *= 0.90f;
    }

  /* resize the dialog to make long names fit as much as possible */
  gtk_window_set_default_size (GTK_WINDOW (dialog), CLAMP (layout_width, 300, parent_width), -1);

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (G_LIKELY (response == GTK_RESPONSE_OK))
    {
      /* hide the dialog */
      gtk_widget_hide (dialog);
      
      /* determine the new filename */
      text = gtk_entry_get_text (GTK_ENTRY (entry));

      /* check if we have a new name here */
      if (G_LIKELY (!exo_str_is_equal (filename, text)))
        {
          /* try to rename the file */
          job = thunar_io_jobs_rename_file (file, text);
        }
    }

  /* cleanup */
  gtk_widget_destroy (dialog);

  return job;
}



/**
 * thunar_dialogs_show_about:
 * @parent : the parent #GtkWindow or %NULL.
 * @title  : the software title.
 * @format : the printf()-style format for the main text in the about dialog.
 * @...    : argument list for the @format.
 *
 * Displays the Thunar about dialog with @format as main text.
 **/
void
thunar_dialogs_show_about (GtkWindow   *parent,
                           const gchar *title,
                           const gchar *format,
                           ...)
{
  static const gchar *artists[] =
  {
    "Young Hahn <youngjin.hahn@gmail.com>",
    NULL,
  };

  static const gchar *authors[] =
  {
    "Benedikt Meurer <benny@xfce.org>",
    "Jannis Pohlmann <jannis@xfce.org>",
    "Nick Schermer <nick@xfce.org>",
    NULL,
  };
  
  static const gchar *documenters[] =
  {
    "Benedikt Meurer <benny@xfce.org>",
    "Jannis Pohlmann <jannis@xfce.org>",
    "Nick Schermer <nick@xfce.org>",
    NULL,
  };

  GdkPixbuf *logo;
  va_list    args;
  gchar     *comments;

  _thunar_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));

  /* determine the comments */
  va_start (args, format);
  comments = g_strdup_vprintf (format, args);
  va_end (args);

  
  /* try to load the about logo */
  logo = gdk_pixbuf_new_from_file (DATADIR "/pixmaps/Thunar/Thunar-about-logo.png", NULL);

  /* open the about dialog */
  gtk_show_about_dialog (parent,
                         "artists", artists,
                         "authors", authors,
                         "comments", comments,
                         "copyright", "Copyright \302\251 2004-2011 Benedikt Meurer\n"
                                      "Copyright \302\251 2009-2011 Jannis Pohlmann\n"
                                      "Copyright \302\251 2009-2012 Nick Schermer",
                         "destroy-with-parent", TRUE,
                         "documenters", documenters,
                         "license", XFCE_LICENSE_GPL,
                         "logo", logo,
                         "program-name", title,
                         "translator-credits", _("translator-credits"),
                         "version", PACKAGE_VERSION,
                         "website", "http://thunar.xfce.org/",
                         NULL);

  /* cleanup */
  if (G_LIKELY (logo != NULL))
    g_object_unref (G_OBJECT (logo));
  g_free (comments);
}



/**
 * thunar_dialogs_show_error:
 * @parent : a #GtkWidget on which the error dialog should be shown, or a #GdkScreen
 *           if no #GtkWidget is known. May also be %NULL, in which case the default
 *           #GdkScreen will be used.
 * @error  : a #GError, which gives a more precise description of the problem or %NULL.
 * @format : the printf()-style format for the primary problem description.
 * @...    : argument list for the @format.
 *
 * Displays an error dialog on @widget using the @format as primary message and optionally
 * displaying @error as secondary error text.
 *
 * If @widget is not %NULL and @widget is part of a #GtkWindow, the function makes sure
 * that the toplevel window is visible prior to displaying the error dialog.
 **/
void
thunar_dialogs_show_error (gpointer      parent,
                           const GError *error,
                           const gchar  *format,
                           ...)
{
  GtkWidget *dialog;
  GtkWindow *window;
  GdkScreen *screen;
  va_list    args;
  gchar     *primary_text;

  _thunar_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));

  /* do not display error dialog for already handled errors */
  if (error->code == G_IO_ERROR_FAILED_HANDLED)
    return;

  /* parse the parent pointer */
  screen = thunar_util_parse_parent (parent, &window);

  /* determine the primary error text */
  va_start (args, format);
  primary_text = g_strdup_vprintf (format, args);
  va_end (args);

  /* allocate the error dialog */
  dialog = gtk_message_dialog_new (window,
                                   GTK_DIALOG_DESTROY_WITH_PARENT
                                   | GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   "%s.", primary_text);

  /* move the dialog to the appropriate screen */
  if (G_UNLIKELY (window == NULL && screen != NULL))
    gtk_window_set_screen (GTK_WINDOW (dialog), screen);

  /* set secondary text if an error is provided */
  if (G_LIKELY (error != NULL))
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);

  /* display the dialog */
  gtk_dialog_run (GTK_DIALOG (dialog));

  /* cleanup */
  gtk_widget_destroy (dialog);
  g_free (primary_text);
}



/**
 * thunar_dialogs_show_job_ask:
 * @parent   : the parent #GtkWindow or %NULL.
 * @question : the question text.
 * @choices  : possible responses.
 *
 * Utility function to display a question dialog for the ThunarJob::ask
 * signal.
 *
 * Return value: the #ThunarJobResponse.
 **/
ThunarJobResponse
thunar_dialogs_show_job_ask (GtkWindow        *parent,
                             const gchar      *question,
                             ThunarJobResponse choices)
{
  const gchar *separator;
  const gchar *mnemonic;
  GtkWidget   *message;
  GtkWidget   *button;
  GString     *secondary = g_string_sized_new (256);
  GString     *primary = g_string_sized_new (256);
  gint         response;
  gint         n;
  gboolean     has_cancel = FALSE;

  _thunar_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (g_utf8_validate (question, -1, NULL), THUNAR_JOB_RESPONSE_CANCEL);

  /* try to separate the question into primary and secondary parts */
  separator = strstr (question, ": ");
  if (G_LIKELY (separator != NULL))
    {
      /* primary is everything before the colon, plus a dot */
      g_string_append_len (primary, question, separator - question);
      g_string_append_c (primary, '.');

      /* secondary is everything after the colon (skipping whitespace) */
      do
        ++separator;
      while (g_ascii_isspace (*separator));
      g_string_append (secondary, separator);
    }
  else
    {
      /* otherwise separate based on the \n\n */
      separator = strstr (question, "\n\n");
      if (G_LIKELY (separator != NULL))
        {
          /* primary is everything before the newlines */
          g_string_append_len (primary, question, separator - question);

          /* secondary is everything after the newlines (skipping whitespace) */
          while (g_ascii_isspace (*separator))
            ++separator;
          g_string_append (secondary, separator);
        }
      else
        {
          /* everything is primary */
          g_string_append (primary, question);
        }
    }

  /* allocate the question message dialog */
  message = gtk_message_dialog_new (parent,
                                    GTK_DIALOG_MODAL |
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_QUESTION,
                                    GTK_BUTTONS_NONE,
                                    "%s", primary->str);
  if (G_LIKELY (*secondary->str != '\0'))
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s", secondary->str);

  /* add the buttons based on the possible choices */
  for (n = 6; n >= 0; --n)
    {
      /* check if the response is set */
      response = choices & (1 << n);
      if (response == 0)
        continue;

      switch (response)
        {
        case THUNAR_JOB_RESPONSE_YES:
          mnemonic = _("_Yes");
          break;

        case THUNAR_JOB_RESPONSE_YES_ALL:
          mnemonic = _("Yes to _all");
          break;

        case THUNAR_JOB_RESPONSE_NO:
          mnemonic = _("_No");
          break;

        case THUNAR_JOB_RESPONSE_NO_ALL:
          mnemonic = _("N_o to all");
          break;

        case THUNAR_JOB_RESPONSE_RETRY:
          mnemonic = _("_Retry");
          break;

        case THUNAR_JOB_RESPONSE_FORCE:
          mnemonic = _("Copy _Anyway");
          break;

        case THUNAR_JOB_RESPONSE_CANCEL:
          /* cancel is always the last option */
          has_cancel = TRUE;
          continue;

        default:
          g_assert_not_reached ();
          break;
        }

      button = gtk_button_new_with_mnemonic (mnemonic);
      gtk_widget_set_can_default (button, TRUE);
      gtk_dialog_add_action_widget (GTK_DIALOG (message), button, response);
      gtk_widget_show (button);

      gtk_dialog_set_default_response (GTK_DIALOG (message), response);
    }

  if (has_cancel)
    {
      button = gtk_button_new_with_mnemonic (_("_Cancel"));
      gtk_widget_set_can_default (button, TRUE);
      gtk_dialog_add_action_widget (GTK_DIALOG (message), button, GTK_RESPONSE_CANCEL);
      gtk_widget_show (button);
      gtk_dialog_set_default_response (GTK_DIALOG (message), GTK_RESPONSE_CANCEL);
    }

  /* run the question dialog */
  response = gtk_dialog_run (GTK_DIALOG (message));
  gtk_widget_destroy (message);

  /* transform the result as required */
  if (G_UNLIKELY (response <= 0))
    response = THUNAR_JOB_RESPONSE_CANCEL;

  /* cleanup */
  g_string_free (secondary, TRUE);
  g_string_free (primary, TRUE);

  return response;
}



/**
 * thunar_dialogs_show_job_ask_replace:
 * @parent   : the parent #GtkWindow or %NULL.
 * @src_file : the #ThunarFile of the source file.
 * @dst_file : the #ThunarFile of the destination file that
 *             may be replaced with the source file.
 *
 * Asks the user whether to replace the destination file with the
 * source file identified by @src_file.
 *
 * Return value: the selected #ThunarJobResponse.
 **/
ThunarJobResponse
thunar_dialogs_show_job_ask_replace (GtkWindow  *parent,
                                     ThunarFile *src_file,
                                     ThunarFile *dst_file)
{
  ThunarIconFactory *icon_factory;
  ThunarPreferences *preferences;
  ThunarDateStyle    date_style;
  GtkIconTheme      *icon_theme;
  GtkWidget         *dialog;
  GtkWidget         *table;
  GtkWidget         *image;
  GtkWidget         *label;
  GdkPixbuf         *icon;
  gchar             *date_string;
  gchar             *size_string;
  gchar             *text;
  gint               response;
  gboolean           file_size_binary;

  _thunar_return_val_if_fail (parent == NULL || GTK_IS_WINDOW (parent), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (src_file), THUNAR_JOB_RESPONSE_CANCEL);
  _thunar_return_val_if_fail (THUNAR_IS_FILE (dst_file), THUNAR_JOB_RESPONSE_CANCEL);

  /* determine the style used to format dates */
  preferences = thunar_preferences_get ();
  g_object_get (G_OBJECT (preferences), "misc-date-style", &date_style, NULL);
  g_object_get (G_OBJECT (preferences), "misc-file-size-binary", &file_size_binary, NULL);
  g_object_unref (G_OBJECT (preferences));

  /* setup the confirmation dialog */
  dialog = gtk_dialog_new_with_buttons (_("Confirm to replace files"),
                                        parent,
                                        GTK_DIALOG_MODAL
                                        | GTK_DIALOG_NO_SEPARATOR
                                        | GTK_DIALOG_DESTROY_WITH_PARENT,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        _("S_kip All"), THUNAR_JOB_RESPONSE_NO_ALL,
                                        _("_Skip"), THUNAR_JOB_RESPONSE_NO,
                                        _("Replace _All"), THUNAR_JOB_RESPONSE_YES_ALL,
                                        _("_Replace"), THUNAR_JOB_RESPONSE_YES,
                                        NULL);
  gtk_dialog_set_alternative_button_order (GTK_DIALOG (dialog),
                                           THUNAR_JOB_RESPONSE_YES,
                                           THUNAR_JOB_RESPONSE_YES_ALL,
                                           THUNAR_JOB_RESPONSE_NO,
                                           THUNAR_JOB_RESPONSE_NO_ALL,
                                           GTK_RESPONSE_CANCEL,
                                           -1);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), THUNAR_JOB_RESPONSE_YES);

  /* determine the icon factory to use */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (dialog));
  icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

  table = g_object_new (GTK_TYPE_TABLE,
                        "border-width", 10,
                        "n-columns", 3,
                        "n-rows", 5,
                        "row-spacing", 6,
                        "column-spacing", 5,
                        NULL);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), table, TRUE, TRUE, 0);
  gtk_widget_show (table);

  image = gtk_image_new_from_icon_name ("stock_folder-copy", GTK_ICON_SIZE_BUTTON);
  gtk_misc_set_alignment (GTK_MISC (image), 0.5f, 0.0f);
  gtk_misc_set_padding (GTK_MISC (image), 6, 6);
  gtk_table_attach (GTK_TABLE (table), image, 0, 1, 0, 1, GTK_FILL, GTK_EXPAND | GTK_FILL, 0, 0);
  gtk_widget_show (image);

  if (thunar_file_is_symlink (dst_file))
    {
      text = g_strdup_printf (_("This folder already contains a symbolic link \"%s\"."), 
                              thunar_file_get_display_name (dst_file));
    }
  else if (thunar_file_is_directory (dst_file))
    {
      text = g_strdup_printf (_("This folder already contains a folder \"%s\"."),
                              thunar_file_get_display_name (dst_file));
    }
  else
    { 
      text = g_strdup_printf (_("This folder already contains a file \"%s\"."), 
                              thunar_file_get_display_name (dst_file));
    }

  label = gtk_label_new (text);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_label_set_attributes (GTK_LABEL (label), thunar_pango_attr_list_big ());
  gtk_table_attach (GTK_TABLE (table), label, 1, 3, 0, 1, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);
  g_free (text);

  if (thunar_file_is_symlink (dst_file))
    text = g_strdup_printf (Q_("ReplaceDialogPart1|Do you want to replace the link"));
  else if (thunar_file_is_directory (dst_file))
    text = g_strdup_printf (Q_("ReplaceDialogPart1|Do you want to replace the existing folder"));
  else
    text = g_strdup_printf (Q_("ReplaceDialogPart1|Do you want to replace the existing file"));

  label = gtk_label_new (text);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_table_attach (GTK_TABLE (table), label, 1, 3, 1, 2, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);
  g_free (text);

  icon = thunar_icon_factory_load_file_icon (icon_factory, dst_file, THUNAR_FILE_ICON_STATE_DEFAULT, 48);
  image = gtk_image_new_from_pixbuf (icon);
  gtk_misc_set_padding (GTK_MISC (image), 6, 6);
  gtk_table_attach (GTK_TABLE (table), image, 1, 2, 2, 3, GTK_FILL, GTK_FILL, 0, 0);
  g_object_unref (G_OBJECT (icon));
  gtk_widget_show (image);

  size_string = thunar_file_get_size_string_formatted (dst_file, file_size_binary);
  date_string = thunar_file_get_date_string (dst_file, THUNAR_FILE_DATE_MODIFIED, date_style);
  text = g_strdup_printf ("%s %s\n%s %s", _("Size:"), size_string, _("Modified:"), date_string);
  label = gtk_label_new (text);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_table_attach (GTK_TABLE (table), label, 2, 3, 2, 3, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);
  g_free (size_string);
  g_free (date_string);
  g_free (text);

  if (thunar_file_is_symlink (src_file))
    text = g_strdup_printf (Q_("ReplaceDialogPart2|with the following link?"));
  else if (thunar_file_is_directory (src_file))
    text = g_strdup_printf (Q_("ReplaceDialogPart2|with the following folder?"));
  else
    text = g_strdup_printf (Q_("ReplaceDialogPart2|with the following file?"));

  label = gtk_label_new (text);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_table_attach (GTK_TABLE (table), label, 1, 3, 3, 4, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);
  g_free (text);

  icon = thunar_icon_factory_load_file_icon (icon_factory, src_file, THUNAR_FILE_ICON_STATE_DEFAULT, 48);
  image = gtk_image_new_from_pixbuf (icon);
  gtk_misc_set_padding (GTK_MISC (image), 6, 6);
  gtk_table_attach (GTK_TABLE (table), image, 1, 2, 4, 5, GTK_FILL, GTK_FILL, 0, 0);
  g_object_unref (G_OBJECT (icon));
  gtk_widget_show (image);

  size_string = thunar_file_get_size_string_formatted (src_file, file_size_binary);
  date_string = thunar_file_get_date_string (src_file, THUNAR_FILE_DATE_MODIFIED, date_style);
  text = g_strdup_printf ("%s %s\n%s %s", _("Size:"), size_string, _("Modified:"), date_string);
  label = gtk_label_new (text);
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_table_attach (GTK_TABLE (table), label, 2, 3, 4, 5, GTK_EXPAND | GTK_FILL, GTK_FILL, 0, 0);
  gtk_widget_show (label);
  g_free (size_string);
  g_free (date_string);
  g_free (text);

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  /* cleanup */
  g_object_unref (G_OBJECT (icon_factory));

  /* translate GTK responses */
  if (G_UNLIKELY (response < 0))
    response = THUNAR_JOB_RESPONSE_CANCEL;

  return response;
}



/**
 * thunar_dialogs_show_job_error:
 * @parent : the parent #GtkWindow or %NULL.
 * @error  : the #GError provided by the #ThunarJob.
 *
 * Utility function to display a message dialog for the
 * ThunarJob::error signal.
 **/
void
thunar_dialogs_show_job_error (GtkWindow *parent,
                               GError    *error)
{
  const gchar *separator;
  GtkWidget   *message;
  GString     *secondary = g_string_sized_new (256);
  GString     *primary = g_string_sized_new (256);

  _thunar_return_if_fail (parent == NULL || GTK_IS_WINDOW (parent));
  _thunar_return_if_fail (error != NULL && error->message != NULL);

  /* try to separate the message into primary and secondary parts */
  separator = strstr (error->message, ": ");
  if (G_LIKELY (separator > error->message))
    {
      /* primary is everything before the colon, plus a dot */
      g_string_append_len (primary, error->message, separator - error->message);
      g_string_append_c (primary, '.');

      /* secondary is everything after the colon (plus a dot) */
      do
        ++separator;
      while (g_ascii_isspace (*separator));
      g_string_append (secondary, separator);
      if (separator[strlen (separator - 1)] != '.')
        g_string_append_c (secondary, '.');
    }
  else
    {
      /* primary is everything, secondary is empty */
      g_string_append (primary, error->message);
    }

  /* allocate and display the error message dialog */
  message = gtk_message_dialog_new (parent,
                                    GTK_DIALOG_MODAL |
                                    GTK_DIALOG_DESTROY_WITH_PARENT,
                                    GTK_MESSAGE_ERROR,
                                    GTK_BUTTONS_NONE,
                                    "%s", primary->str);
  if (G_LIKELY (*secondary->str != '\0'))
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s", secondary->str);
  gtk_dialog_add_button (GTK_DIALOG (message), GTK_STOCK_CLOSE, GTK_RESPONSE_CANCEL);
  gtk_dialog_run (GTK_DIALOG (message));
  gtk_widget_destroy (message);

  /* cleanup */
  g_string_free (secondary, TRUE);
  g_string_free (primary, TRUE);
}



gboolean
thunar_dialogs_show_insecure_program (gpointer     parent,
                                      const gchar *primary,
                                      ThunarFile  *file,
                                      const gchar *command)
{
  GdkScreen      *screen;
  GtkWindow      *window;
  gint            response;
  GtkWidget      *dialog;
  GString        *secondary;
  ThunarFileMode  old_mode;
  ThunarFileMode  new_mode;
  GFileInfo      *info;
  GError         *err = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  _thunar_return_val_if_fail (g_utf8_validate (command, -1, NULL), FALSE);

  /* parse the parent window and screen */
  screen = thunar_util_parse_parent (parent, &window);

  /* secondary text */
  secondary = g_string_new (NULL);
  g_string_append_printf (secondary, _("The desktop file \"%s\" is in an insecure location "
                                       "and not marked as executable. If you do not trust "
                                       "this program, click Cancel."),
                                       thunar_file_get_display_name (file));
  g_string_append (secondary, "\n\n");
  if (exo_str_looks_like_an_uri (command))
    g_string_append_printf (secondary, G_KEY_FILE_DESKTOP_KEY_URL"=%s", command);
  else
    g_string_append_printf (secondary, G_KEY_FILE_DESKTOP_KEY_EXEC"=%s", command);

  /* allocate and display the error message dialog */
  dialog = gtk_message_dialog_new (window,
                                   GTK_DIALOG_MODAL |
                                   GTK_DIALOG_DESTROY_WITH_PARENT,
                                   GTK_MESSAGE_WARNING,
                                   GTK_BUTTONS_NONE,
                                   "%s", primary);
  gtk_dialog_add_button (GTK_DIALOG (dialog), _("_Launch Anyway"), GTK_RESPONSE_OK);
  if (thunar_file_is_chmodable (file))
    gtk_dialog_add_button (GTK_DIALOG (dialog), _("Mark _Executable"), GTK_RESPONSE_APPLY);
  gtk_dialog_add_button (GTK_DIALOG (dialog), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  if (screen != NULL && window == NULL)
    gtk_window_set_screen (GTK_WINDOW (dialog), screen);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s", secondary->str);
  g_string_free (secondary, TRUE);
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  gtk_widget_destroy (dialog);

  /* check if we should make the file executable */
  if (response == GTK_RESPONSE_APPLY)
    {
      /* try to query information about the file */
      info = g_file_query_info (thunar_file_get_file (file),
                                G_FILE_ATTRIBUTE_UNIX_MODE,
                                G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                NULL, &err);

      if (G_LIKELY (info != NULL))
        {
          if (g_file_info_has_attribute (info, G_FILE_ATTRIBUTE_UNIX_MODE))
            {
              /* determine the current mode */
              old_mode = g_file_info_get_attribute_uint32 (info, G_FILE_ATTRIBUTE_UNIX_MODE);

              /* generate the new mode */
              new_mode = old_mode | THUNAR_FILE_MODE_USR_EXEC | THUNAR_FILE_MODE_GRP_EXEC | THUNAR_FILE_MODE_OTH_EXEC;

              if (old_mode != new_mode)
                {
                  g_file_set_attribute_uint32 (thunar_file_get_file (file),
                                               G_FILE_ATTRIBUTE_UNIX_MODE, new_mode,
                                               G_FILE_QUERY_INFO_NOFOLLOW_SYMLINKS,
                                               NULL, &err);
                }
            }
          else
            {
              g_warning ("No %s attribute found", G_FILE_ATTRIBUTE_UNIX_MODE);
            }

          g_object_unref (info);
        }

      if (err != NULL)
        {
          thunar_dialogs_show_error (parent, err, ("Unable to mark launcher executable"));
          g_error_free (err);
        }

      /* just launch */
      response = GTK_RESPONSE_OK;
    }

  return (response == GTK_RESPONSE_OK);
}
