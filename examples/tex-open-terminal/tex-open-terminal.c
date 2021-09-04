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

#include <libxfce4util/libxfce4util.h>
#include <libxfce4ui/libxfce4ui.h>
#include <tex-open-terminal/tex-open-terminal.h>
#include <sys/stat.h>



static void   tex_open_terminal_menu_provider_init    (ThunarxMenuProviderIface *iface);
static GList *tex_open_terminal_get_file_menu_items   (ThunarxMenuProvider      *provider,
                                                       GtkWidget                *window,
                                                       GList                    *files);
static GList *tex_open_terminal_get_folder_menu_items (ThunarxMenuProvider      *provider,
                                                       GtkWidget                *window,
                                                       ThunarxFileInfo          *folder);
static void   tex_open_terminal_activated             (ThunarxMenuItem          *item,
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
  iface->get_file_menu_items = tex_open_terminal_get_file_menu_items;
  iface->get_folder_menu_items = tex_open_terminal_get_folder_menu_items;
}



static GList*
tex_open_terminal_get_file_menu_items (ThunarxMenuProvider *provider,
                                       GtkWidget           *window,
                                       GList               *files)
{
  /* check if we have a directory here */
  if (G_LIKELY (files != NULL && files->next == NULL && thunarx_file_info_is_directory (files->data)))
    return tex_open_terminal_get_folder_menu_items (provider, window, files->data);

  return NULL;
}



static GList*
tex_open_terminal_get_folder_menu_items (ThunarxMenuProvider *provider,
                                         GtkWidget           *window,
                                         ThunarxFileInfo     *folder)
{
  ThunarxMenuItem *item = NULL;
  gchar           *scheme;
  gchar           *path;
  gchar           *uri;

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
          item = thunarx_menu_item_new ("TexOpenTerminal::open-terminal-here", "Create shared thumbnail repository", "Create shared thumbnail repositories in this folder and its subfolders", "utilities-terminal");
          g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (tex_open_terminal_activated), window);
          g_object_set_data_full (G_OBJECT (item), "open-terminal-here-path", path, g_free);
        }
    }
  g_free (scheme);

  return (item != NULL) ? g_list_prepend (NULL, item) : NULL;
}



static void
recursive_call (gchar *path)
{
  static int tabs = 0;

  GDir *dir;
  GError *error;
  const gchar *filename;

  dir = g_dir_open(path, 0, &error);
  while ((filename = g_dir_read_name(dir)))
    {
      gchar *new_path = g_build_path ("/", path, filename, NULL);
      GFile *file = g_file_new_for_path (new_path);
      GFileInfo *info = g_file_query_info (file, "*", G_FILE_QUERY_INFO_NONE, NULL, NULL);
      GFileType type = g_file_info_get_file_type (info);

//      for (int i = 0; i < tabs; i++)
//        printf("\t");
//      printf ("%s is a directory -> %d\n", new_path, type);

      if (type == G_FILE_TYPE_DIRECTORY)
        {
          printf("Dir path: %s\n", new_path);
          tabs++;
          if (strcmp (filename, ".sh_thumbnails") != 0)
            recursive_call (new_path);
          tabs--;
        }
      else if (type == G_FILE_TYPE_REGULAR)
        {
          gchar *uri = g_file_get_uri (file);
          gchar *normal_path = xfce_get_local_thumbnail_path (uri, "normal");
          gchar *large_path = xfce_get_local_thumbnail_path (uri, "large");
          gchar *normal_shared_path = xfce_create_shared_thumbnail_path (uri, "normal");
          gchar *large_shared_path = xfce_create_shared_thumbnail_path (uri, "large");


          if (normal_path)
            {
              for (int i = 0; i < tabs; i++)
                printf("\t");
              printf ("Normal thumbnail path: %s\n", normal_path);
              printf ("Shared normal thumbnail path: %s\n", normal_shared_path);
              xfce_mkdirhier (g_path_get_dirname (normal_shared_path), S_IRWXU | S_IRWXG | S_IRWXO, NULL);
              printf ("Success: %d\n", g_file_copy (g_file_new_for_path (normal_path), g_file_new_for_path (normal_shared_path), G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, NULL));
            }
          if (large_path)
            {
              for (int i = 0; i < tabs; i++)
                printf("\t");
              printf ("Large thumbnail path: %s\n", large_path);
              xfce_mkdirhier (g_path_get_dirname (large_shared_path), S_IRWXU | S_IRWXG | S_IRWXO, NULL);
              printf ("Success: %d\n", g_file_copy (g_file_new_for_path (large_path), g_file_new_for_path (large_shared_path), G_FILE_COPY_ALL_METADATA, NULL, NULL, NULL, NULL));
            }
        }
    }
}



static void
tex_open_terminal_activated (ThunarxMenuItem *item,
                             GtkWidget       *window)
{
  gchar *path;

  /* determine the folder path */
  path = g_object_get_data (G_OBJECT (item), "open-terminal-here-path");

  recursive_call (path);
}
