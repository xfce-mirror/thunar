/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
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

#include <libxfce4ui/libxfce4ui.h>
#include <tex-open-terminal/tex-open-terminal.h>



static void   tex_open_terminal_menu_provider_init (ThunarxMenuProviderIface *iface);
static GList *tex_open_terminal_get_file_actions   (ThunarxMenuProvider      *provider,
                                                    GtkWidget                *window,
                                                    GList                    *files);
static GList *tex_open_terminal_get_folder_actions (ThunarxMenuProvider      *provider,
                                                    GtkWidget                *window,
                                                    ThunarxFileInfo          *folder);
static void   tex_open_terminal_activated          (GtkAction                *action,
                                                    GtkWidget                *window);



struct _TexOpenTerminalClass
{
  GObjectClass __parent__;
};

struct _TexOpenTerminal
{
  GObject __parent__;
};



THUNARX_DEFINE_TYPE_WITH_CODE (TexOpenTerminal,
                               tex_open_terminal,
                               G_TYPE_OBJECT,
                               THUNARX_IMPLEMENT_INTERFACE (THUNARX_TYPE_MENU_PROVIDER,
                                                            tex_open_terminal_menu_provider_init));



static void
tex_open_terminal_class_init (TexOpenTerminalClass *klass)
{
  /* nothing to do here */
}



static void
tex_open_terminal_init (TexOpenTerminal *open_terminal)
{
  /* nothing to do here */
}



static void
tex_open_terminal_menu_provider_init (ThunarxMenuProviderIface *iface)
{
  iface->get_file_actions = tex_open_terminal_get_file_actions;
  iface->get_folder_actions = tex_open_terminal_get_folder_actions;
}



static GList*
tex_open_terminal_get_file_actions (ThunarxMenuProvider *provider,
                                    GtkWidget           *window,
                                    GList               *files)
{
  /* check if we have a directory here */
  if (G_LIKELY (files != NULL && files->next == NULL && thunarx_file_info_is_directory (files->data)))
    return tex_open_terminal_get_folder_actions (provider, window, files->data);

  return NULL;
}



static GList*
tex_open_terminal_get_folder_actions (ThunarxMenuProvider *provider,
                                      GtkWidget           *window,
                                      ThunarxFileInfo     *folder)
{
  GtkAction *action = NULL;
  gchar     *scheme;
  gchar     *path;
  gchar     *uri;

  /* determine the uri scheme of the folder and check if we support it */
  scheme = thunarx_file_info_get_uri_scheme (folder);
  if (G_LIKELY (strcmp (scheme, "file") == 0))
    {
      /* determine the local path to the folder */
      uri = thunarx_file_info_get_uri (folder);
      path = g_filename_from_uri (uri, NULL, NULL);
      g_free (uri);

      /* check if we have a valid path here */
      if (G_LIKELY (path != NULL))
        {
          action = gtk_action_new ("TexOpenTerminal::open-terminal-here", "Open Terminal Here", "Open Terminal in this folder", NULL);
          g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (tex_open_terminal_activated), window);
          g_object_set_data_full (G_OBJECT (action), "open-terminal-here-path", path, g_free);
        }
    }
  g_free (scheme);

  return (action != NULL) ? g_list_prepend (NULL, action) : NULL;
}



static void
tex_open_terminal_activated (GtkAction *action,
                             GtkWidget *window)
{
  const gchar *path;
  GError      *error = NULL;
  gchar       *command;

  /* determine the folder path */
  path = g_object_get_data (G_OBJECT (action), "open-terminal-here-path");
  if (G_UNLIKELY (path == NULL))
    return;
  
  /* build up the command line for the terminal */
  command = g_strdup_printf ("Terminal --working-directory \"%s\"", path);

  /* try to run the terminal command */
  if (!xfce_spawn_command_line_on_screen (gtk_widget_get_screen (window), command, FALSE, FALSE, &error))
    {
      /* display an error dialog */
      xfce_dialog_show_error (GTK_WINDOW (window), error, "Failed to open terminal in folder %s.", path);
      g_error_free (error);
    }

  /* cleanup */
  g_free (command);
}


