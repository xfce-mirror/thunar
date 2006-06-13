/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_SIGNAL_H
#include <signal.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>

#include <thunar-vfs/thunar-vfs.h>



/* well known archive types */
static const char *TSE_MIME_TYPES[] = {
  "application/x-ar",
  "application/x-arj",
  "application/x-bzip",
  "application/x-bzip-compressed-tar",
  "application/x-compress",
  "application/x-compressed-tar",
  "application/x-deb",
  "application/x-gtar",
  "application/x-gzip",
  "application/x-lha",
  "application/x-lhz",
  "application/x-rar",
  "application/x-rar-compressed",
  "application/x-tar",
  "application/x-zip",
  "application/x-zip-compressed",
  "application/zip",
  "multipart/x-zip",
  "application/x-rpm",
  "application/x-jar",
  "application/x-java-archive",
  "application/x-lzop",
  "application/x-zoo",
  "application/x-cd-image",
};



/* compress response ids */
enum
{
  TSE_RESPONSE_COMPRESS,
  TSE_RESPONSE_PLAIN,
};



static void
tse_error (GError      *error,
           const gchar *format,
           ...)
{
  GtkWidget *dialog;
  va_list    args;
  gchar     *primary_text;

  /* determine the primary error text */
  va_start (args, format);
  primary_text = g_strdup_vprintf (format, args);
  va_end (args);

  /* allocate the error dialog */
  dialog = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s.", primary_text);
  gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);

  /* display the dialog */
  gtk_dialog_run (GTK_DIALOG (dialog));

  /* cleanup */
  gtk_widget_destroy (dialog);
  g_free (primary_text);
}



static gint
tse_ask_compress (GList *infos)
{
  ThunarVfsFileSize total_size = 0;
  ThunarVfsInfo    *info;
  GtkWidget        *message;
  GList            *lp;
  gint              response = TSE_RESPONSE_PLAIN;
  gint              n_archives = 0;
  gint              n_infos = 0;
  gint              n;

  /* check the file infos */
  for (lp = infos; lp != NULL; lp = lp->next, ++n_infos)
    {
      /* need to compress if we have any directories */
      info = (ThunarVfsInfo *) lp->data;
      if (info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY)
        return TSE_RESPONSE_COMPRESS;

      /* check if the single file is already an archive */
      for (n = 0; n < G_N_ELEMENTS (TSE_MIME_TYPES); ++n)
        {
          /* check if this mime type matches */
          if (strcmp (thunar_vfs_mime_info_get_name (info->mime_info), TSE_MIME_TYPES[n]) == 0)
            {
              /* yep, that's a match then */
              ++n_archives;
              break;
            }
        }

      /* add file size to total */
      total_size += info->size;
    }

  /* check if the total size is larger than 200KiB, or we have more than
   * one file, and atleast one of the files is not already an archive.
   */
  if ((n_infos > 1 || total_size > 200 * 1024) && n_infos != n_archives)
    {
      /* check if we have more than one file */
      if (G_LIKELY (n_infos == 1))
        {
          /* ask the user whether to compress the file */
          info = (ThunarVfsInfo *) infos->data;
          message = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                            _("Send \"%s\" as compressed archive?"), info->display_name);
          gtk_dialog_add_button (GTK_DIALOG (message), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
          gtk_dialog_add_button (GTK_DIALOG (message), _("Send _directly"), TSE_RESPONSE_PLAIN);
          gtk_dialog_add_button (GTK_DIALOG (message), _("Send com_pressed"), TSE_RESPONSE_COMPRESS);
          gtk_dialog_set_default_response (GTK_DIALOG (message), TSE_RESPONSE_COMPRESS);
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), _("When sending a file via email, you can either choose to "
                                                                                    "send the file directly, as is, or compress the file before "
                                                                                    "attaching it to an email. It is highly recommended to "
                                                                                    "compress large files before sending them."));
          response = gtk_dialog_run (GTK_DIALOG (message));
          gtk_widget_destroy (message);
        }
      else
        {
          /* ask the user whether to send files as archive */
          message = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_QUESTION, GTK_BUTTONS_NONE,
                                            ngettext ("Send %d file as compressed archive?",
                                                      "Send %d files as compressed archive?",
                                                      n_infos),
                                            n_infos);
          gtk_dialog_add_button (GTK_DIALOG (message), GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL);
          gtk_dialog_add_button (GTK_DIALOG (message), _("Send _directly"), TSE_RESPONSE_PLAIN);
          gtk_dialog_add_button (GTK_DIALOG (message), _("Send as _archive"), TSE_RESPONSE_COMPRESS);
          gtk_dialog_set_default_response (GTK_DIALOG (message), TSE_RESPONSE_COMPRESS);
          gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), _("When sending multiple files via email, you can either choose "
                                                                                    "to send the files directly, attaching multiple files to an "
                                                                                    "email, or send all files compressed into a single archive "
                                                                                    "file and attach the archive. It is highly recommended to "
                                                                                    "send multiple large files as archive."));
          response = gtk_dialog_run (GTK_DIALOG (message));
          gtk_widget_destroy (message);
        }
    }

  return response;
}



static void
tse_child_watch (GPid     pid,
                 gint     status,
                 gpointer user_data)
{
  g_object_set_data (G_OBJECT (user_data), I_("tse-child-status"), GINT_TO_POINTER (status));
  gtk_dialog_response (GTK_DIALOG (user_data), GTK_RESPONSE_YES);
}



static gboolean
tse_progress (const gchar *working_directory,
              gchar      **argv,
              GError     **error)
{
  GtkWidget *progress;
  GtkWidget *dialog;
  GtkWidget *image;
  GtkWidget *label;
  GtkWidget *hbox;
  GtkWidget *vbox;
  gboolean   succeed = FALSE;
  GPid       pid;
  gint       pulse_timer_id;
  gint       watch_id;
  gint       response;
  gint       status;

  /* try to run the command */
  if (!g_spawn_async (working_directory, argv, NULL, G_SPAWN_DO_NOT_REAP_CHILD | G_SPAWN_SEARCH_PATH, NULL, NULL, &pid, error))
    return FALSE;

  /* allocate the progress dialog */
  dialog = gtk_dialog_new_with_buttons (_("Compressing files..."),
                                        NULL, GTK_DIALOG_NO_SEPARATOR,
                                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                                        NULL);
  gtk_dialog_set_default_response (GTK_DIALOG (dialog), GTK_RESPONSE_CANCEL);
  gtk_window_set_default_size (GTK_WINDOW (dialog), 300, -1);

  /* setup the hbox */
  hbox = gtk_hbox_new (FALSE, 12);
  gtk_container_set_border_width (GTK_CONTAINER (hbox), 8);
  gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), hbox, TRUE, TRUE, 0);
  gtk_widget_show (hbox);
  
  /* setup the image */
  image = gtk_image_new_from_icon_name ("gnome-package", GTK_ICON_SIZE_DIALOG);
  gtk_misc_set_alignment (GTK_MISC (image), 0.5f, 0.0f);
  gtk_box_pack_start (GTK_BOX (hbox), image, FALSE, FALSE, 0);
  gtk_widget_show (image);

  /* setup the vbox */
  vbox = gtk_vbox_new (FALSE, 12);
  gtk_box_pack_start (GTK_BOX (hbox), vbox, TRUE, TRUE, 0);
  gtk_widget_show (vbox);

  /* setup the label */
  label = gtk_label_new (_("Compressing files..."));
  gtk_misc_set_alignment (GTK_MISC (label), 0.0f, 0.5f);
  gtk_box_pack_start (GTK_BOX (vbox), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* setup the progress bar */
  progress = gtk_progress_bar_new ();
  gtk_box_pack_start (GTK_BOX (vbox), progress, FALSE, FALSE, 0);
  gtk_widget_show (progress);

  /* start the child watch */
  watch_id = g_child_watch_add (pid, tse_child_watch, dialog);

  /* start the pulse timer */
  pulse_timer_id = g_timeout_add (125, (GSourceFunc) gtk_progress_bar_pulse, progress);

  /* run the dialog */
  response = gtk_dialog_run (GTK_DIALOG (dialog));
  if (response == GTK_RESPONSE_YES)
    {
      /* check if the command failed */
      status = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (dialog), I_("tse-child-status")));
      if (!WIFEXITED (status) || WEXITSTATUS (status) != 0)
        {
          /* tell the user that the command failed */
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("ZIP command terminated with error %d"), status);
        }
      else
        {
          /* the command succeed */
          succeed = TRUE;
        }
    }
  else
    {
      /* terminate the ZIP command */
      kill (pid, SIGQUIT);
    }

  /* cleanup */
  g_source_remove (pulse_timer_id);
  gtk_widget_destroy (dialog);
  g_source_remove (watch_id);

  return succeed;
}



static gboolean
tse_compress (GList  *infos,
              gchar **zipfile_return)
{
  ThunarVfsPath *parent;
  ThunarVfsInfo *info;
  gboolean       succeed = TRUE;
  GError        *error = NULL;
  gchar        **argv;
  gchar         *zipfile;
  gchar         *tmppath;
  gchar         *tmpdir;
  gchar         *path;
  gchar         *dot;
  GList         *lp;
  gint           n;

  /* create a temporary directory */
  tmpdir = g_strdup ("/tmp/thunar-sendto-email.XXXXXX");
  if (G_UNLIKELY (mkdtemp (tmpdir) == NULL))
    {
      /* tell the user that we failed to create a temporary directory */
      error = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
      tse_error (error, _("Failed to create temporary directory"));
      g_error_free (error);
      return FALSE;
    }

  /* check if we have only a single file/folder to compress */
  if (G_LIKELY (infos != NULL && infos->next == NULL))
    {
      /* determine the name for the ZIP file */
      info = (ThunarVfsInfo *) infos->data;
      path = g_strdup (info->display_name);
      dot = strrchr (path, '.');
      if (G_LIKELY (dot != NULL))
        *dot = '\0';
      zipfile = g_strconcat (tmpdir, G_DIR_SEPARATOR_S, path, ".zip", NULL);
      g_free (path);
    }
  else
    {
      /* determine the name for the ZIP file */
      info = (ThunarVfsInfo *) infos->data;
      parent = thunar_vfs_path_get_parent (info->path);
      if (!thunar_vfs_path_is_root (parent))
        {
          /* use the parent directory's name */
          zipfile = g_strconcat (tmpdir, G_DIR_SEPARATOR_S, thunar_vfs_path_get_name (parent), ".zip", NULL);
        }
      else
        {
          /* use the first file's name */
          zipfile = g_strconcat (tmpdir, G_DIR_SEPARATOR_S, info->display_name, ".zip", NULL);
        }
    }

  /* generate the argument list for the ZIP command */
  argv = g_new0 (gchar *, g_list_length (infos) + 5);
  argv[0] = g_strdup ("zip");
  argv[1] = g_strdup ("-q");
  argv[2] = g_strdup ("-r");
  argv[3] = g_strdup (zipfile);
  for (lp = infos, n = 4; lp != NULL && succeed; lp = lp->next, ++n)
    {
      /* create a symlink for the file to the tmp dir */
      info = (ThunarVfsInfo *) lp->data;
      path = thunar_vfs_path_dup_string (info->path);
      tmppath = g_build_filename (tmpdir, thunar_vfs_path_get_name (info->path), NULL);
      succeed = (symlink (path, tmppath) == 0);
      if (G_UNLIKELY (!succeed))
        {
          /* tell the user that we failed to create a symlink for this file */
          error = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
          tse_error (error, _("Failed to create symbolic link for \"%s\""), info->display_name);
          g_error_free (error);
        }
      else
        {
          /* add the file to the ZIP command line */
          argv[n] = g_path_get_basename (tmppath);
        }
      g_free (tmppath);
      g_free (path);
    }

  /* check if we succeed up to this point */
  if (G_LIKELY (succeed))
    {
      /* try to run the ZIP command */
      succeed = tse_progress (tmpdir, argv, &error);
      if (G_UNLIKELY (!succeed))
        {
          /* check if we failed or the user cancelled */
          if (G_LIKELY (error != NULL))
            {
              /* tell the user that we failed to compress the file(s) */
              tse_error (error, ngettext ("Failed to compress %d file",
                                          "Failed to compress %d files",
                                          g_list_length (infos)),
                         g_list_length (infos));
              g_error_free (error);
            }

          /* delete the temporary directory */
          dot = g_strdup_printf ("rm -rf '%s'", tmpdir);
          g_spawn_command_line_sync (dot, NULL, NULL, NULL, NULL);
          g_free (dot);
        }
      else
        {
          /* return the path to the compressed file */
          *zipfile_return = zipfile;
          zipfile = NULL;
        }
    }

  /* cleanup */
  g_strfreev (argv);
  g_free (zipfile);
  g_free (tmpdir);

  return succeed;
}



int
main (int argc, char **argv)
{
  ThunarVfsInfo *info;
  ThunarVfsPath *path;
  GString       *mailto;
  GError        *error = NULL;
  GList         *infos = NULL;
  gchar        **attachments = NULL;
  gchar         *zipfile;
  GList         *lp;
  gint           response;
  gint           n;

  /* setup translation domain */
  xfce_textdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR, "UTF-8");

#ifdef G_ENABLE_DEBUG
  /* Do NOT remove this line for now, If something doesn't work,
   * fix your code instead!
   */
  g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif

  /* initialize Gtk+ */
  gtk_init (&argc, &argv);

  /* initialize the ThunarVFS library */
  thunar_vfs_init ();

  /* set default icon for dialogs */
  gtk_window_set_default_icon_name ("internet-mail");

  /* verify that atleast one file was supplied */
  if (G_UNLIKELY (argc == 1))
    {
      g_fprintf (stderr, "Usage: thunar-sendto-email [FILES...]\n");
      return EXIT_FAILURE;
    }

  /* determine the ThunarVfsInfos for the files */
  for (n = 1; n < argc; ++n)
    {
      /* try to transform to a ThunarVfsPath */
      path = thunar_vfs_path_new (argv[n], &error);
      if (G_UNLIKELY (path == NULL))
        {
invalid_argument:
          g_fprintf (stderr, "thunar-sendto-email: Invalid argument \"%s\": %s\n", argv[n], error->message);
          thunar_vfs_info_list_free (infos);
          g_error_free (error);
          return EXIT_FAILURE;
        }

      /* verify that we're not trying to send root */
      if (thunar_vfs_path_is_root (path))
        {
          error = g_error_new_literal (G_FILE_ERROR, g_file_error_from_errno (EFBIG), g_strerror (EFBIG));
          thunar_vfs_path_unref (path);
          goto invalid_argument;
        }

      /* try to determine the info */
      info = thunar_vfs_info_new_for_path (path, &error);
      thunar_vfs_path_unref (path);

      /* check if we failed */
      if (G_UNLIKELY (info == NULL))
        goto invalid_argument;

      /* add to our list of infos */
      infos = g_list_append (infos, info);
    }

  /* check whether to compress the files */
  response = tse_ask_compress (infos);
  if (response == TSE_RESPONSE_COMPRESS)
    {
      /* try to compress the files */
      if (tse_compress (infos, &zipfile))
        {
          /* we have only a single attachment now */
          attachments = g_new0 (gchar *, 2);
          attachments[0] = g_filename_to_uri (zipfile, NULL, NULL);
          g_free (zipfile);
        }
    }
  else if (response == TSE_RESPONSE_PLAIN)
    {
      /* attach the files one by one */
      attachments = g_new0 (gchar *, g_list_length (infos) + 1);
      for (lp = infos, n = 0; lp != NULL; lp = lp->next, ++n)
        {
          info = (ThunarVfsInfo *) lp->data;
          attachments[n] = thunar_vfs_path_dup_uri (info->path);
        }
    }

  /* check if we have anything to attach */
  if (G_LIKELY (attachments != NULL))
    {
      /* no specific email address */
      mailto = g_string_new ("?");
      for (n = 0; attachments[n] != NULL; ++n)
        g_string_append_printf (mailto, "attach=%s&", attachments[n]);
      g_strfreev (attachments);

      /* open the mail composer */
      if (!exo_execute_preferred_application ("MailReader", mailto->str, NULL, NULL, &error))
        {
          /* tell the user that we failed */
          tse_error (error, _("Failed to compose new email"));
          g_error_free (error);
        }

      /* cleanup */
      g_string_free (mailto, TRUE);
    }

  /* cleanup */
  thunar_vfs_info_list_free (infos);

  /* shutdown the ThunarVFS library */
  thunar_vfs_shutdown ();

  return EXIT_SUCCESS;
}

