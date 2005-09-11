/* $Id$ */
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

#include <thunarx/thunarx.h>



/* make sure to export the required public functions */
#ifdef HAVE_GNUC_VISIBILITY
extern void thunar_extension_initialize (GTypeModule *module) __attribute__((visibility("default")));
extern void thunar_extension_shutdown (void) __attribute__((visibility("default")));
extern void thunar_extension_list_types (const GType **types, gint *n_types) __attribute__((visibility("default")));
#else
extern G_MODULE_EXPORT void thunar_extension_initialize (GTypeModule *module);
extern G_MODULE_EXPORT void thunar_extension_shutdown (void);
extern G_MODULE_EXPORT void thunar_extension_list_types (const GType **types, gint *n_types);
#endif



static GType type_list[1];



typedef struct _OpenTerminalHereClass OpenTerminalHereClass;
typedef struct _OpenTerminalHere      OpenTerminalHere;



static void   open_terminal_here_register_type      (GTypeModule              *module);
static void   open_terminal_here_menu_provider_init (ThunarxMenuProviderIface *iface);
static GList *open_terminal_here_get_file_actions   (ThunarxMenuProvider      *provider,
                                                     GtkWidget                *window,
                                                     GList                    *files);
static GList *open_terminal_here_get_folder_actions (ThunarxMenuProvider      *provider,
                                                     GtkWidget                *window,
                                                     ThunarxFileInfo          *folder);
static void   open_terminal_here_activated          (GtkAction                *action,
                                                     GtkWidget                *window);



struct _OpenTerminalHereClass
{
  GObjectClass __parent__;
};

struct _OpenTerminalHere
{
  GObject __parent__;
};



static void
open_terminal_here_register_type (GTypeModule *module)
{
  static const GTypeInfo info =
  {
    sizeof (OpenTerminalHereClass),
    NULL,
    NULL,
    NULL,
    NULL,
    NULL,
    sizeof (OpenTerminalHere),
    0,
    NULL,
    NULL,
  };

  static const GInterfaceInfo menu_provider_info =
  {
    (GInterfaceInitFunc) open_terminal_here_menu_provider_init,
    NULL,
    NULL,
  };

  type_list[0] = g_type_module_register_type (module, G_TYPE_OBJECT, "OpenTerminalHere", &info, 0);
  g_type_module_add_interface (module, type_list[0], THUNARX_TYPE_MENU_PROVIDER, &menu_provider_info);
}



static void
open_terminal_here_menu_provider_init (ThunarxMenuProviderIface *iface)
{
  iface->get_file_actions = open_terminal_here_get_file_actions;
  iface->get_folder_actions = open_terminal_here_get_folder_actions;
}



static GList*
open_terminal_here_get_file_actions (ThunarxMenuProvider *provider,
                                     GtkWidget           *window,
                                     GList               *files)
{
  /* check if we have a directory here */
  if (G_LIKELY (files != NULL && files->next == NULL && thunarx_file_info_is_directory (files->data)))
    return open_terminal_here_get_folder_actions (provider, window, files->data);

  return NULL;
}



static GList*
open_terminal_here_get_folder_actions (ThunarxMenuProvider *provider,
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
          action = gtk_action_new ("OpenTerminalHere::open-terminal-here", "Open Terminal Here", "Open Terminal in this folder", NULL);
          g_signal_connect (G_OBJECT (action), "activate", G_CALLBACK (open_terminal_here_activated), window);
          g_object_set_data_full (G_OBJECT (action), "open-terminal-here-path", path, g_free);
        }
    }
  g_free (scheme);

  return (action != NULL) ? g_list_prepend (NULL, action) : NULL;
}



static void
open_terminal_here_activated (GtkAction *action,
                              GtkWidget *window)
{
  const gchar *path;
  GtkWidget   *dialog;
  GError      *error = NULL;
  gchar       *command;

  /* determine the folder path */
  path = g_object_get_data (G_OBJECT (action), "open-terminal-here-path");
  if (G_UNLIKELY (path == NULL))
    return;
  
  /* build up the command line for the terminal */
  command = g_strdup_printf ("Terminal --working-directory \"%s\"", path);

  /* try to run the terminal command */
  if (!gdk_spawn_command_line_on_screen (gtk_widget_get_screen (window), command, &error))
    {
      /* display an error dialog */
      dialog = gtk_message_dialog_new (GTK_WINDOW (window),
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_ERROR,
                                       GTK_BUTTONS_CLOSE,
                                       "Failed to open terminal in folder %s.",
                                       path);
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (dialog), "%s.", error->message);
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
      g_error_free (error);
    }

  /* cleanup */
  g_free (command);
}



void
thunar_extension_initialize (GTypeModule *module)
{
  const gchar *mismatch;

  /* verify that the thunarx versions are compatible */
  mismatch = thunarx_check_version (THUNARX_MAJOR_VERSION, THUNARX_MINOR_VERSION, THUNARX_MICRO_VERSION);
  if (G_UNLIKELY (mismatch != NULL))
    {
      g_warning ("Version mismatch: %s", mismatch);
      return;
    }

  g_message ("Initializing OpenTerminalHere extension");

  open_terminal_here_register_type (module);
}



void
thunar_extension_shutdown (void)
{
  g_message ("Shutting down OpenTerminalHere extension");
}



void
thunar_extension_list_types (const GType **types,
                             gint         *n_types)
{
  *types = type_list;
  *n_types = G_N_ELEMENTS (type_list);
}

