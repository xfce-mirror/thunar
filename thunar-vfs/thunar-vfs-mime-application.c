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

#include <thunar-vfs/thunar-vfs-mime-application.h>
#include <thunar-vfs/thunar-vfs-alias.h>



GQuark
thunar_vfs_mime_application_error_quark (void)
{
  static GQuark quark = 0;
  if (G_UNLIKELY (quark == 0))
    quark = g_quark_from_static_string ("thunar-vfs-mime-application-error-quark");
  return quark;
}




static void     thunar_vfs_mime_application_register_type (GType                          *type);
static void     thunar_vfs_mime_application_class_init    (ThunarVfsMimeApplicationClass  *klass);
static void     thunar_vfs_mime_application_finalize      (ExoObject                      *object);
static gboolean thunar_vfs_mime_application_get_argv      (const ThunarVfsMimeApplication *application,
                                                           GList                          *uris,
                                                           gint                           *argc,
                                                           gchar                        ***argv,
                                                           GError                        **error);



struct _ThunarVfsMimeApplicationClass
{
  ExoObjectClass __parent__;
};

struct _ThunarVfsMimeApplication
{
  ExoObject __parent__;

  gchar    *binary_name;
  gchar    *desktop_id;
  gchar    *exec;
  gchar    *icon;
  gchar    *name;

  guint     requires_terminal : 1;
  guint     supports_startup_notify : 1;
  guint     supports_multi : 1; /* %F or %U */
  guint     supports_uris : 1;  /* %u or %U */
};



static ExoObjectClass *thunar_vfs_mime_application_parent_class;



GType
thunar_vfs_mime_application_get_type (void)
{
  static GType type = G_TYPE_INVALID;
  static GOnce once = G_ONCE_INIT;

  /* thread-safe type registration */
  g_once (&once, (GThreadFunc) thunar_vfs_mime_application_register_type, &type);

  return type;
}



static void
thunar_vfs_mime_application_register_type (GType *type)
{
  static const GTypeInfo info =
  {
    sizeof (ThunarVfsMimeApplicationClass),
    NULL,
    NULL,
    (GClassInitFunc) thunar_vfs_mime_application_class_init,
    NULL,
    NULL,
    sizeof (ThunarVfsMimeApplication),
    0,
    NULL,
    NULL,
  };

  *type = g_type_register_static (EXO_TYPE_OBJECT, "ThunarVfsMimeApplication", &info, 0);
}



static void
thunar_vfs_mime_application_class_init (ThunarVfsMimeApplicationClass *klass)
{
  ExoObjectClass *exoobject_class;

  thunar_vfs_mime_application_parent_class = g_type_class_peek_parent (klass);

  exoobject_class = EXO_OBJECT_CLASS (klass);
  exoobject_class->finalize = thunar_vfs_mime_application_finalize;
}



static void
thunar_vfs_mime_application_finalize (ExoObject *object)
{
  ThunarVfsMimeApplication *application = THUNAR_VFS_MIME_APPLICATION (object);

  /* free resources */
  g_free (application->binary_name);
  g_free (application->desktop_id);
  g_free (application->exec);
  g_free (application->icon);
  g_free (application->name);

  (*EXO_OBJECT_CLASS (thunar_vfs_mime_application_parent_class)->finalize) (object);
}



static gboolean
thunar_vfs_mime_application_get_argv (const ThunarVfsMimeApplication *application,
                                      GList                          *uris,
                                      gint                           *argc,
                                      gchar                        ***argv,
                                      GError                        **error)
{
  const gchar *p;
  gboolean     result;
  GString     *command_line = g_string_new (NULL);
  GList       *lp;
  gchar       *uri_string;
  gchar       *quoted;

  /* prepend terminal command if required */
  if (G_UNLIKELY (application->requires_terminal))
    {
      quoted = g_shell_quote (application->name);
      g_string_append (command_line, "Terminal -T ");
      g_string_append (command_line, quoted);
      g_string_append (command_line, " -x ");
      g_free (quoted);
    }

  for (p = application->exec; *p != '\0'; ++p)
    {
      if (p[0] == '%' && p[1] != '\0')
        {
          switch (*++p)
            {
            case 'f':
              quoted = g_shell_quote (thunar_vfs_uri_get_path (uris->data));
              g_string_append (command_line, quoted);
              g_free (quoted);
              break;

            case 'F':
              for (lp = uris; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != uris))
                    g_string_append_c (command_line, ' ');
                  quoted = g_shell_quote (thunar_vfs_uri_get_path (lp->data));
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'u':
              /* we need to hide the host parameter here, because there are quite a few
               * applications out there (namely GNOME applications), which cannot handle
               * 'file:'-URIs with host names.
               */
              uri_string = thunar_vfs_uri_to_string (uris->data, THUNAR_VFS_URI_HIDE_HOST);
              quoted = g_shell_quote (uri_string);
              g_string_append (command_line, quoted);
              g_free (uri_string);
              g_free (quoted);
              break;

            case 'U':
              for (lp = uris; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != uris))
                    g_string_append_c (command_line, ' ');
                  uri_string = thunar_vfs_uri_to_string (lp->data, THUNAR_VFS_URI_HIDE_HOST);
                  quoted = g_shell_quote (uri_string);
                  g_string_append (command_line, quoted);
                  g_free (uri_string);
                  g_free (quoted);
                }
              break;

            case 'd':
              uri_string = g_path_get_dirname (thunar_vfs_uri_get_path (uris->data));
              quoted = g_shell_quote (uri_string);
              g_string_append (command_line, quoted);
              g_free (uri_string);
              g_free (quoted);
              break;

            case 'D':
              for (lp = uris; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != uris))
                    g_string_append_c (command_line, ' ');
                  uri_string = g_path_get_dirname (thunar_vfs_uri_get_path (lp->data));
                  quoted = g_shell_quote (uri_string);
                  g_string_append (command_line, quoted);
                  g_free (uri_string);
                  g_free (quoted);
                }
              break;

            case 'n':
              quoted = g_shell_quote (thunar_vfs_uri_get_name (uris->data));
              g_string_append (command_line, quoted);
              g_free (quoted);
              break;

            case 'N':
              for (lp = uris; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != uris))
                    g_string_append_c (command_line, ' ');
                  quoted = g_shell_quote (thunar_vfs_uri_get_name (lp->data));
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'i':
              if (G_LIKELY (application->icon != NULL))
                {
                  quoted = g_shell_quote (application->icon);
                  g_string_append (command_line, "--icon ");
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'c':
              quoted = g_shell_quote (application->name);
              g_string_append (command_line, quoted);
              g_free (quoted);
              break;

            case '%':
              g_string_append_c (command_line, '%');
              break;
            }
        }
      else
        {
          g_string_append_c (command_line, *p);
        }
    }

  result = g_shell_parse_argv (command_line->str, argc, argv, NULL);

  g_string_free (command_line, TRUE);

  return result;
}



/**
 * thunar_vfs_mime_application_new_from_desktop_id:
 * @desktop_id : the id of an application's .desktop file.
 *
 * Generates a #ThunarVfsMimeApplication instance for the application
 * referenced by @desktop_id. Returns %NULL if @desktop_id is not valid.
 *
 * The caller is responsible to free the returned instance using
 * #exo_object_unref().
 *
 * Return value: the #ThunarVfsMimeApplication for @desktop_id or %NULL.
 **/
ThunarVfsMimeApplication*
thunar_vfs_mime_application_new_from_desktop_id (const gchar *desktop_id)
{
  ThunarVfsMimeApplication *application = NULL;
  const gchar              *exec;
  const gchar              *icon;
  const gchar              *name;
  XfceRc                   *rc;
  gchar                   **argv;
  gchar                    *spec;
  gchar                    *path = NULL;
  gchar                    *s;
  gint                      argc;

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

  /* try to open the .desktop file */
  rc = xfce_rc_simple_open (path, TRUE);
  g_free (path);
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
      application = exo_object_new (THUNAR_VFS_TYPE_MIME_APPLICATION);

      application->binary_name = g_path_get_basename (argv[0]);
      application->desktop_id = g_strdup (desktop_id);
      application->name = g_strdup (name);
      application->icon = g_strdup (icon);

      /* we assume %f if the application hasn't set anything else,
       * as that's also what KDE and Gnome do in this case.
       */
      if (strstr (exec, "%f") == NULL && strstr (exec, "%F") == NULL && strstr (exec, "%u") == NULL && strstr (exec, "%U") == NULL)
        application->exec = g_strconcat (exec, " %f", NULL);
      else
        application->exec = g_strdup (exec);

      application->requires_terminal = xfce_rc_read_bool_entry (rc, "Terminal", FALSE);
      application->supports_startup_notify = xfce_rc_read_bool_entry (rc, "StartupNotify", FALSE)
                                          || xfce_rc_read_bool_entry (rc, "X-KDE-StartupNotify", FALSE);
      application->supports_multi = (strstr (application->exec, "%F") != NULL) || (strstr (application->exec, "%U") != NULL);
      application->supports_uris = (strstr (application->exec, "%u") != NULL) || (strstr (application->exec, "%U") != NULL);

      g_strfreev (argv);
    }

  /* close the file */
  xfce_rc_close (rc);

  return application;
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
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (application), NULL);
  return application->desktop_id;
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
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (application), NULL);
  return application->name;
}



/**
 * thunar_vfs_mime_application_exec:
 * @application : a #ThunarVfsMimeApplication.
 * @screen      : a #GdkScreen or %NULL to use the default screen.
 * @uris        : a list of #ThunarVfsURI<!---->s to open.
 * @error       : return location for errors or %NULL.
 *
 * Wrapper to #thunar_vfs_mime_application_exec_with_env(), which
 * simply passes a %NULL pointer for the environment variables.
 *
 * Return value: %TRUE if the execution succeed, else %FALSE.
 **/
gboolean
thunar_vfs_mime_application_exec (const ThunarVfsMimeApplication *application,
                                  GdkScreen                      *screen,
                                  GList                          *uris,
                                  GError                        **error)
{
  return thunar_vfs_mime_application_exec_with_env (application, screen, uris, NULL, error);
}



/**
 * thunar_vfs_mime_application_exec_with_env:
 * @application : a #ThunarVfsMimeApplication.
 * @screen      : a #GdkScreen or %NULL to use the default screen.
 * @uris        : a list of #ThunarVfsURI<!---->s to open.
 * @envp        : child's environment or %NULL to inherit parent's.
 * @error       : return location for errors or %NULL.
 *
 * Executes @application on @screen using the given @uris. If
 * @uris contains more than one #ThunarVfsURI and @application
 * doesn't support opening multiple documents at once, one
 * instance of @application will be spawned for every #ThunarVfsURI
 * given in @uris.
 *
 * Return value: %TRUE if the execution succeed, else %FALSE.
 **/
gboolean
thunar_vfs_mime_application_exec_with_env (const ThunarVfsMimeApplication *application,
                                           GdkScreen                      *screen,
                                           GList                          *uris,
                                           gchar                         **envp,
                                           GError                        **error)
{
  gboolean result = TRUE;
  GList    list;
  GList   *lp;
  gchar   *working_directory;
  gchar  **argv;
  gint     argc;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (application), FALSE);
  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (uris != NULL && THUNAR_VFS_IS_URI (uris->data), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* fallback to the default screen if no screen is given */
  if (G_UNLIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* verify that either the application supports URIs or only local files are given */
  if (G_LIKELY (!application->supports_uris))
    for (lp = uris; lp != NULL; lp = lp->next)
      if (thunar_vfs_uri_get_scheme (lp->data) != THUNAR_VFS_URI_SCHEME_FILE)
        {
          g_set_error (error, THUNAR_VFS_MIME_APPLICATION_ERROR, THUNAR_VFS_MIME_APPLICATION_ERROR_LOCAL_FILES_ONLY,
                       _("The application \"%s\" supports only local documents."), application->name);
          return FALSE;
        }

  /* check whether the application can open multiple documents at once */
  if (G_LIKELY (!application->supports_multi))
    {
      for (lp = uris; lp != NULL; lp = lp->next)
        {
          /* use a short list with only one entry */
          list.data = lp->data;
          list.next = NULL;
          list.prev = NULL;

          /* figure out the argument vector to run the application */
          if (!thunar_vfs_mime_application_get_argv (application, &list, &argc, &argv, error))
            return FALSE;

          /* use the first URI's base directory as working directory for the application */
          working_directory = g_path_get_dirname (thunar_vfs_uri_get_path (list.data));

          /* try to spawn the application */
          result = gdk_spawn_on_screen (screen, working_directory, argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, error);

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
      if (!thunar_vfs_mime_application_get_argv (application, uris, &argc, &argv, error))
        return FALSE;

      /* use the first URI's base directory as working directory for the application */
      working_directory = g_path_get_dirname (thunar_vfs_uri_get_path (uris->data));

      /* try to spawn the application */
      result = gdk_spawn_on_screen (screen, working_directory, argv, envp, G_SPAWN_SEARCH_PATH, NULL, NULL, NULL, error);

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
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (application), NULL);

  if (application->icon != NULL && gtk_icon_theme_has_icon (icon_theme, application->icon))
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
 * to #g_hash_table_new() as the @hash_func parameter,
 * when using #ThunarVfsMimeApplication<!---->s as keys
 * in a #GHashTable.
 *
 * Return value: a hash value corresponding to the key.
 **/
guint
thunar_vfs_mime_application_hash (gconstpointer application)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (application), 0);
  return g_str_hash (THUNAR_VFS_MIME_APPLICATION (application)->desktop_id);
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
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (a), FALSE);
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (b), FALSE);
  return (strcmp (THUNAR_VFS_MIME_APPLICATION (a)->desktop_id, THUNAR_VFS_MIME_APPLICATION (b)->desktop_id) == 0);
}



#define __THUNAR_VFS_MIME_APPLICATION_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
