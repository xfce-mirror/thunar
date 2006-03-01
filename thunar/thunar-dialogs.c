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

#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif

#include <exo/exo.h>

#include <thunar/thunar-dialogs.h>



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
  GtkWidget *window = NULL;
  GdkScreen *screen;
  va_list    args;
  gchar     *primary_text;

  g_return_if_fail (parent == NULL || GDK_IS_SCREEN (parent) || GTK_IS_WIDGET (parent));

  /* determine the proper parent */
  if (parent == NULL)
    {
      /* just use the default screen then */
      screen = gdk_screen_get_default ();
    }
  else if (GDK_IS_SCREEN (parent))
    {
      /* yep, that's a screen */
      screen = GDK_SCREEN (parent);
    }
  else
    {
      /* parent is a widget, so let's determine the toplevel window */
      window = gtk_widget_get_toplevel (GTK_WIDGET (parent));
      if (GTK_WIDGET_TOPLEVEL (window))
        {
          /* make sure the toplevel window is shown */
          gtk_widget_show_now (window);
        }
      else
        {
          /* no toplevel, not usable then */
          window = NULL;
        }

      /* determine the screen for the widget */
      screen = gtk_widget_get_screen (GTK_WIDGET (parent));
    }

  /* determine the primary error text */
  va_start (args, format);
  primary_text = g_strdup_vprintf (format, args);
  va_end (args);

  /* allocate the error dialog */
  dialog = gtk_message_dialog_new ((GtkWindow *) window,
                                   GTK_DIALOG_DESTROY_WITH_PARENT
                                   | GTK_DIALOG_MODAL,
                                   GTK_MESSAGE_ERROR,
                                   GTK_BUTTONS_CLOSE,
                                   _("%s."), primary_text);

  /* move the dialog to the appropriate screen */
  if (window == NULL && screen != NULL)
    gtk_window_set_screen (GTK_WINDOW (dialog), screen);

  /* set secondary text if an error is provided */
  if (G_LIKELY (error != NULL))
    gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), _("%s."), error->message);

  /* display the dialog */
  gtk_dialog_run (GTK_DIALOG (dialog));

  /* cleanup */
  gtk_widget_destroy (dialog);
  g_free (primary_text);
}



/**
 * thunar_dialogs_show_help:
 * @parent : a #GtkWidget on which the user manual should be shown, or a #GdkScreen
 *           if no #GtkWidget is known. May also be %NULL, in which case the default
 *           #GdkScreen will be used.
 * @page   : the name of the page of the user manual to display or %NULL to display
 *           the overview page.
 * @offset : the offset of the topic in the @page to display or %NULL to just display
 *           @page.
 *
 * Displays the Thunar user manual. If @page is not %NULL it specifies the basename
 * of the HTML file to display. @offset may refer to a anchor in the @page.
 **/
void
thunar_dialogs_show_help (gpointer     parent,
                          const gchar *page,
                          const gchar *offset)
{
  GdkScreen *screen;
  GError    *error = NULL;
  gchar     *command;
  gchar     *tmp;

  /* determine the screen on which to launch the help */
  if (G_UNLIKELY (parent == NULL))
    screen = gdk_screen_get_default ();
  else if (GDK_IS_SCREEN (parent))
    screen = GDK_SCREEN (parent);
  else
    screen = gtk_widget_get_screen (GTK_WIDGET (parent));

  /* generate the command for the documentation browser */
  command = g_strdup (LIBEXECDIR "/ThunarHelp");

  /* check if a page is given */
  if (G_UNLIKELY (page != NULL))
    {
      /* append page as second parameter */
      tmp = g_strconcat (command, " ", page, NULL);
      g_free (command);
      command = tmp;

      /* check if an offset is given */
      if (G_UNLIKELY (offset != NULL))
        {
          /* append offset as third parameter */
          tmp = g_strconcat (command, " ", offset, NULL);
          g_free (command);
          command = tmp;
        }
    }

  /* try to run the documentation browser */
  if (!gdk_spawn_command_line_on_screen (screen, command, &error))
    {
      /* display an error message to the user */
      thunar_dialogs_show_error (parent, error, _("Failed to open the documentation browser"));
      g_error_free (error);
    }

  /* cleanup */
  g_free (command);
}


