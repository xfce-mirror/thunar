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

#include <exo/exo.h>

#include <thunar-vfs/thunar-vfs-exec.h>
#include <thunar-vfs/thunar-vfs-mime-application.h>
#include <thunar-vfs/thunar-vfs-util.h>
#include <thunar-vfs/thunar-vfs-alias.h>



GQuark
thunar_vfs_mime_application_error_quark (void)
{
  static GQuark quark = 0;
  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("thunar-vfs-mime-application-error-quark");
  return quark;
}




static gboolean thunar_vfs_mime_application_get_argv (const ThunarVfsMimeApplication *application,
                                                      GList                          *path_list,
                                                      gint                           *argc,
                                                      gchar                        ***argv,
                                                      GError                        **error);



struct _ThunarVfsMimeApplication
{
  gint                          ref_count;
  gchar                        *binary_name;
  gchar                        *desktop_id;
  gchar                        *exec;
  gchar                        *icon;
  gchar                        *name;
  gchar                       **mime_types;
  ThunarVfsMimeApplicationFlags flags;
};



GType
thunar_vfs_mime_application_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = g_boxed_type_register_static ("ThunarVfsMimeApplication",
                                           (GBoxedCopyFunc) thunar_vfs_mime_application_ref,
                                           (GBoxedFreeFunc) thunar_vfs_mime_application_unref);
    }

  return type;
}



static gboolean
thunar_vfs_mime_application_get_argv (const ThunarVfsMimeApplication *application,
                                      GList                          *path_list,
                                      gint                           *argc,
                                      gchar                        ***argv,
                                      GError                        **error)
{
  return thunar_vfs_exec_parse (application->exec, path_list, application->icon, application->name, NULL,
                                (application->flags & THUNAR_VFS_MIME_APPLICATION_REQUIRES_TERMINAL) != 0,
                                argc, argv, error);
}



/**
 * thunar_vfs_mime_application_new_from_desktop_id:
 * @desktop_id : the id of an application's .desktop file.
 *
 * Generates a #ThunarVfsMimeApplication instance for the application
 * referenced by @desktop_id. Returns %NULL if @desktop_id is not valid.
 *
 * The caller is responsible to free the returned instance using
 * thunar_vfs_mime_application_unref().
 *
 * Return value: the #ThunarVfsMimeApplication for @desktop_id or %NULL.
 **/
ThunarVfsMimeApplication*
thunar_vfs_mime_application_new_from_desktop_id (const gchar *desktop_id)
{
  ThunarVfsMimeApplication *application = NULL;
  gchar                    *spec;
  gchar                    *path = NULL;
  gchar                    *s;

  g_return_val_if_fail (desktop_id != NULL, NULL);

  /* lookup the .desktop file by the given desktop-id */
  s = spec = g_build_filename ("applications", desktop_id, NULL);
  do
    {
      path = xfce_resource_lookup (XFCE_RESOURCE_DATA, spec);
      if (G_LIKELY (path != NULL))
        break;

      for (; *s != '\0' && *s != '-'; ++s) ;
      if (G_LIKELY (*s == '-'))
        *s++ = G_DIR_SEPARATOR;
    }
  while (*s != '\0');
  g_free (spec);

  /* check if we found a file */
  if (G_UNLIKELY (path == NULL))
    return NULL;

  /* try to load the application from the path */
  application = thunar_vfs_mime_application_new_from_file (path, desktop_id);

  /* cleanup */
  g_free (path);

  return application;
}



/**
 * thunar_vfs_mime_application_new_from_file:
 * @path       : the absolute path to the desktop file.
 * @desktop_id : the desktop-id of the file.
 *
 * Generates a new #ThunarVfsMimeApplication for the application
 * described by @path and @desktop_id.
 *
 * The caller is responsible to free the returned instance using
 * thunar_vfs_mime_application_unref().
 *
 * You should really seldomly use this function and always
 * prefer thunar_vfs_mime_application_new_from_desktop_id().
 *
 * Return value: the #ThunarVfsMimeApplication for @desktop_id
 *               or %NULL.
 **/
ThunarVfsMimeApplication*
thunar_vfs_mime_application_new_from_file (const gchar *path,
                                           const gchar *desktop_id)
{
  ThunarVfsMimeApplication *application = NULL;
  const gchar              *exec;
  const gchar              *icon;
  const gchar              *name;
  XfceRc                   *rc;
  gchar                   **argv;
  gchar                   **ms;
  gchar                   **mt;
  gint                      argc;

  g_return_val_if_fail (g_path_is_absolute (path), NULL);
  g_return_val_if_fail (desktop_id != NULL && *desktop_id != '\0', NULL);

  /* try to open the .desktop file */
  rc = xfce_rc_simple_open (path, TRUE);
  if (G_UNLIKELY (rc == NULL))
    return NULL;

  /* parse the file */
  xfce_rc_set_group (rc, "Desktop Entry");
  exec = xfce_rc_read_entry (rc, "Exec", NULL);
  name = xfce_rc_read_entry (rc, "Name", NULL);
  icon = xfce_rc_read_entry (rc, "Icon", NULL);

  /* generate the application object */
  if (G_LIKELY (exec != NULL && name != NULL && g_shell_parse_argv (exec, &argc, &argv, NULL)))
    {
      application = g_new0 (ThunarVfsMimeApplication, 1);
      application->ref_count = 1;

      application->binary_name = g_path_get_basename (argv[0]);
      application->desktop_id = g_strdup (desktop_id);
      application->name = g_strdup (name);
      application->icon = g_strdup (icon);

      /* strip off known suffixes for image files if a themed icon is specified */
      if (application->icon != NULL && !g_path_is_absolute (application->icon) && g_str_has_suffix (application->icon, ".png"))
        application->icon[strlen (application->icon) - 4] = '\0';

      /* determine the list of mime types supported by the application */
      application->mime_types = xfce_rc_read_list_entry (rc, "MimeType", ";");
      if (G_LIKELY (application->mime_types != NULL))
        {
          /* strip off the useless mime types */
          for (ms = mt = application->mime_types; *ms != NULL; ++ms)
            {
              /* ignore empty entries, GNOME pseudo mime types and KDE junk */
              if (**ms == '\0' || g_str_equal (*ms, "x-directory/gnome-default-handler") || g_str_has_prefix (*ms, "print/"))
                g_free (*ms);
              else
                *mt++ = *ms;
            }

          /* verify that we have atleast one mime type left */
          if (G_UNLIKELY (mt == application->mime_types))
            {
              g_free (application->mime_types);
              application->mime_types = NULL;
            }
          else
            {
              /* be sure to zero-terminate the new list */
              *mt = NULL;
            }
        }

      /* we assume %f if the application hasn't set anything else,
       * as that's also what KDE and Gnome do in this case.
       */
      if (strstr (exec, "%f") == NULL && strstr (exec, "%F") == NULL && strstr (exec, "%u") == NULL && strstr (exec, "%U") == NULL)
        application->exec = g_strconcat (exec, " %f", NULL);
      else
        application->exec = g_strdup (exec);

      if (G_UNLIKELY (xfce_rc_read_bool_entry (rc, "Terminal", FALSE)))
        application->flags |= THUNAR_VFS_MIME_APPLICATION_REQUIRES_TERMINAL;

      if (xfce_rc_read_bool_entry (rc, "Hidden", FALSE) || xfce_rc_read_bool_entry (rc, "NoDisplay", FALSE))
        application->flags |= THUNAR_VFS_MIME_APPLICATION_HIDDEN;

      if (xfce_rc_read_bool_entry (rc, "StartupNotify", FALSE) || xfce_rc_read_bool_entry (rc, "X-KDE-StartupNotify", FALSE))
        application->flags |= THUNAR_VFS_MIME_APPLICATION_SUPPORTS_STARTUP_NOTIFY;

      if ((strstr (application->exec, "%F") != NULL) || (strstr (application->exec, "%U") != NULL))
        application->flags |= THUNAR_VFS_MIME_APPLICATION_SUPPORTS_MULTI;

      g_strfreev (argv);
    }

  /* close the file */
  xfce_rc_close (rc);

  return application;
}



/**
 * thunar_vfs_mime_application_ref:
 * @application : a #ThunarVfsMimeApplication.
 *
 * Increases the reference count on @application by one
 * and returns the reference to @application.
 *
 * Return value: a reference to @application.
 **/
ThunarVfsMimeApplication*
thunar_vfs_mime_application_ref (ThunarVfsMimeApplication *application)
{
  exo_atomic_inc (&application->ref_count);
  return application;
}



/**
 * thunar_vfs_mime_application_unref:
 * @application : a #ThunarVfsMimeApplication.
 *
 * Decreases the reference count on @application and frees
 * the @application object once the reference count drops
 * to zero.
 **/
void
thunar_vfs_mime_application_unref (ThunarVfsMimeApplication *application)
{
  if (exo_atomic_dec (&application->ref_count))
    {
      /* free resources */
      g_strfreev (application->mime_types);
      g_free (application->binary_name);
      g_free (application->desktop_id);
      g_free (application->exec);
      g_free (application->icon);
      g_free (application->name);
      g_free (application);
    }
}



/**
 * thunar_vfs_mime_application_get_command:
 * @application : a #ThunarVfsMimeApplication.
 *
 * Returns the command line to run @application.
 *
 * Return value: the command to run @application.
 **/
const gchar*
thunar_vfs_mime_application_get_command (const ThunarVfsMimeApplication *application)
{
  return application->exec;
}



/**
 * thunar_vfs_mime_application_get_desktop_id:
 * @application : a #ThunarVfsMimeApplication.
 *
 * Returns the desktop-id of @application.
 *
 * Return value: the desktop-id of @application.
 **/
const gchar*
thunar_vfs_mime_application_get_desktop_id (const ThunarVfsMimeApplication *application)
{
  return application->desktop_id;
}



/**
 * thunar_vfs_mime_application_get_flags:
 * @application : a #ThunarVfsMimeApplication.
 *
 * Returns the flags for @application.
 *
 * Return value: the flags for @application.
 **/
ThunarVfsMimeApplicationFlags
thunar_vfs_mime_application_get_flags (const ThunarVfsMimeApplication *application)
{
  return application->flags;
}



/**
 * thunar_vfs_mime_application_get_name:
 * @application : a #ThunarVfsMimeApplication.
 *
 * Returns the generic name of @application.
 *
 * Return value: the generic name of @application.
 **/
const gchar*
thunar_vfs_mime_application_get_name (const ThunarVfsMimeApplication *application)
{
  return application->name;
}



/**
 * thunar_vfs_mime_application_get_mime_types:
 * @application : a #ThunarVfsMimeApplication.
 *
 * Returns the list of MIME-types supported by @application
 * or %NULL if the @application doesn't support any MIME-types
 * at all.
 *
 * The returned %NULL-terminated string array is owned by
 * @application and must not be free by the caller.
 *
 * Return value: the list of supported MIME-types for @application.
 **/
const gchar* const*
thunar_vfs_mime_application_get_mime_types (const ThunarVfsMimeApplication *application)
{
  return (gconstpointer) application->mime_types;
}



/**
 * thunar_vfs_mime_application_exec:
 * @application : a #ThunarVfsMimeApplication.
 * @screen      : a #GdkScreen or %NULL to use the default screen.
 * @path_list   : a list of #ThunarVfsPath<!---->s to open.
 * @error       : return location for errors or %NULL.
 *
 * Wrapper to thunar_vfs_mime_application_exec_with_env(), which
 * simply passes a %NULL pointer for the environment variables.
 *
 * Return value: %TRUE if the execution succeed, else %FALSE.
 **/
gboolean
thunar_vfs_mime_application_exec (const ThunarVfsMimeApplication *application,
                                  GdkScreen                      *screen,
                                  GList                          *path_list,
                                  GError                        **error)
{
  return thunar_vfs_mime_application_exec_with_env (application, screen, path_list, NULL, error);
}



/**
 * thunar_vfs_mime_application_exec_with_env:
 * @application : a #ThunarVfsMimeApplication.
 * @screen      : a #GdkScreen or %NULL to use the default screen.
 * @path_list   : a list of #ThunarVfsPath<!---->s to open.
 * @envp        : child's environment or %NULL to inherit parent's.
 * @error       : return location for errors or %NULL.
 *
 * Executes @application on @screen using the given @path_list. If
 * @path_list contains more than one #ThunarVfsPath and @application
 * doesn't support opening multiple documents at once, one
 * instance of @application will be spawned for every #ThunarVfsPath
 * given in @path_list.
 *
 * Return value: %TRUE if the execution succeed, else %FALSE.
 **/
gboolean
thunar_vfs_mime_application_exec_with_env (const ThunarVfsMimeApplication *application,
                                           GdkScreen                      *screen,
                                           GList                          *path_list,
                                           gchar                         **envp,
                                           GError                        **error)
{
  ThunarVfsPath *parent;
  gboolean       result = TRUE;
  GList          list;
  GList         *lp;
  gchar         *working_directory;
  gchar        **argv;
  gint           argc;

  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* fallback to the default screen if no screen is given */
  if (G_UNLIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* check whether the application can open multiple documents at once */
  if (G_LIKELY ((application->flags & THUNAR_VFS_MIME_APPLICATION_SUPPORTS_MULTI) == 0))
    {
      for (lp = path_list; lp != NULL; lp = lp->next)
        {
          /* use a short list with only one entry */
          list.data = lp->data;
          list.next = NULL;
          list.prev = NULL;

          /* figure out the argument vector to run the application */
          if (!thunar_vfs_mime_application_get_argv (application, &list, &argc, &argv, error))
            return FALSE;

          /* use the paths base directory as working directory for the application */
          parent = thunar_vfs_path_get_parent (list.data);
          working_directory = (parent != NULL) ? thunar_vfs_path_dup_string (parent) : NULL;

          /* try to spawn the application */
          result = thunar_vfs_exec_on_screen (screen, working_directory, argv, envp, G_SPAWN_SEARCH_PATH,
                                              application->flags & THUNAR_VFS_MIME_APPLICATION_SUPPORTS_STARTUP_NOTIFY, error);

          /* cleanup */
          g_free (working_directory);
          g_strfreev (argv);

          /* check if we succeed */
          if (G_UNLIKELY (!result))
            break;
        }
    }
  else
    {
      /* we can open all documents at once */
      if (!thunar_vfs_mime_application_get_argv (application, path_list, &argc, &argv, error))
        return FALSE;

      /* use the first paths base directory as working directory for the application */
      parent = (path_list != NULL) ? thunar_vfs_path_get_parent (path_list->data) : NULL;
      working_directory = (parent != NULL) ? thunar_vfs_path_dup_string (parent) : NULL;

      /* try to spawn the application */
      result = thunar_vfs_exec_on_screen (screen, working_directory, argv, envp, G_SPAWN_SEARCH_PATH,
                                          application->flags & THUNAR_VFS_MIME_APPLICATION_SUPPORTS_STARTUP_NOTIFY, error);

      /* cleanup */
      g_free (working_directory);
      g_strfreev (argv);
    }

  return result;
}



/**
 * thunar_vfs_mime_application_lookup_icon_name:
 * @application : a #ThunarVfsMimeApplication.
 * @icon_theme  : a #GtkIconTheme.
 *
 * Looks up the icon name for @application in
 * @icon_theme. Returns %NULL if no suitable
 * icon is present in @icon_theme.
 *
 * Return value: the icon name for @application or
 *               %NULL.
 **/
const gchar*
thunar_vfs_mime_application_lookup_icon_name (const ThunarVfsMimeApplication *application,
                                              GtkIconTheme                   *icon_theme)
{
  if (application->icon != NULL && (g_path_is_absolute (application->icon) || gtk_icon_theme_has_icon (icon_theme, application->icon)))
    return application->icon;
  else if (application->binary_name != NULL && gtk_icon_theme_has_icon (icon_theme, application->binary_name))
    return application->binary_name;
  else
    return NULL;
}



/**
 * thunar_vfs_mime_application_hash:
 * @application : a #ThunarVfsMimeApplication.
 *
 * Converts @application to a hash value. It can be passed
 * to g_hash_table_new() as the @hash_func parameter,
 * when using #ThunarVfsMimeApplication<!---->s as keys
 * in a #GHashTable.
 *
 * Return value: a hash value corresponding to the key.
 **/
guint
thunar_vfs_mime_application_hash (gconstpointer application)
{
  return g_str_hash (((const ThunarVfsMimeApplication *) application)->desktop_id);
}



/**
 * thunar_vfs_mime_application_equal:
 * @a : a #ThunarVfsMimeApplication.
 * @b : a #ThunarVfsMimeApplication.
 *
 * Checks whether @a and @b refer to the same application.
 *
 * Return value: %TRUE if @a and @b are equal.
 **/
gboolean
thunar_vfs_mime_application_equal (gconstpointer a,
                                   gconstpointer b)
{
  const ThunarVfsMimeApplication *a_application = a;
  const ThunarVfsMimeApplication *b_application = b;
  return (strcmp (a_application->desktop_id, b_application->desktop_id) == 0);
}



#define __THUNAR_VFS_MIME_APPLICATION_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
