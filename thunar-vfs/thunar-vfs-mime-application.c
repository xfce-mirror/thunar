/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar-vfs/thunar-vfs-exec.h>
#include <thunar-vfs/thunar-vfs-mime-action-private.h>
#include <thunar-vfs/thunar-vfs-mime-application.h>
#include <thunar-vfs/thunar-vfs-util.h>
#include <thunar-vfs/thunar-vfs-alias.h>



static void thunar_vfs_mime_application_class_init  (ThunarVfsMimeApplicationClass  *klass);
static void thunar_vfs_mime_application_finalize    (GObject                        *object);



struct _ThunarVfsMimeApplicationClass
{
  ThunarVfsMimeHandlerClass __parent__;
};

struct _ThunarVfsMimeApplication
{
  ThunarVfsMimeHandler __parent__;

  GList  *actions;
  gchar  *desktop_id;
  gchar **mime_types;
};



static GObjectClass *thunar_vfs_mime_application_parent_class;



GType
thunar_vfs_mime_application_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
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

      type = g_type_register_static (THUNAR_VFS_TYPE_MIME_HANDLER, I_("ThunarVfsMimeApplication"), &info, 0);
    }

  return type;
}



static void
thunar_vfs_mime_application_class_init (ThunarVfsMimeApplicationClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_vfs_mime_application_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_mime_application_finalize;
}



static void
thunar_vfs_mime_application_finalize (GObject *object)
{
  ThunarVfsMimeApplication *mime_application = THUNAR_VFS_MIME_APPLICATION (object);

  /* release the mime actions */
  g_list_foreach (mime_application->actions, (GFunc) g_object_unref, NULL);
  g_list_free (mime_application->actions);

  /* release our attributes */
  g_strfreev (mime_application->mime_types);
  g_free (mime_application->desktop_id);

  (*G_OBJECT_CLASS (thunar_vfs_mime_application_parent_class)->finalize) (object);
}



/**
 * thunar_vfs_mime_application_new_from_desktop_id:
 * @desktop_id : the id of an application's .desktop file.
 *
 * Generates a #ThunarVfsMimeApplication instance for the application
 * referenced by @desktop_id. Returns %NULL if @desktop_id is not valid.
 *
 * The caller is responsible to free the returned instance using
 * g_object_unref() when no longer needed.
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
 * g_object_unref() when no longer needed.
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
  ThunarVfsMimeHandlerFlags flags = 0;
  ThunarVfsMimeApplication *application = NULL;
  ThunarVfsMimeAction      *action;
  const gchar              *tryexec;
  const gchar              *exec;
  const gchar              *icon;
  const gchar              *name;
  gboolean                  present;
  XfceRc                   *rc;
  gchar                    *command;
  gchar                   **actions;
  gchar                    *group;
  gchar                   **ms;
  gchar                   **mt;
  guint                     n;

  g_return_val_if_fail (g_path_is_absolute (path), NULL);
  g_return_val_if_fail (desktop_id != NULL && *desktop_id != '\0', NULL);

  /* try to open the .desktop file */
  rc = xfce_rc_simple_open (path, TRUE);
  if (G_UNLIKELY (rc == NULL))
    return NULL;

  /* parse the file */
  xfce_rc_set_group (rc, "Desktop Entry");
  name = xfce_rc_read_entry (rc, "Name", NULL);
  exec = xfce_rc_read_entry_untranslated (rc, "Exec", NULL);
  icon = xfce_rc_read_entry_untranslated (rc, "Icon", NULL);

  /* check if we have a TryExec field */
  tryexec = xfce_rc_read_entry_untranslated (rc, "TryExec", NULL);
  tryexec = (tryexec != NULL) ? tryexec : exec;
  if (G_LIKELY (tryexec != NULL && g_shell_parse_argv (tryexec, NULL, &mt, NULL)))
    {
      /* check if we have an absolute path to an existing file */
      present = g_file_test (mt[0], G_FILE_TEST_EXISTS);

      /* else, we may have a program in $PATH */
      if (G_LIKELY (!present))
        {
          command = g_find_program_in_path (mt[0]);
          present = (command != NULL);
          g_free (command);
        }

      /* cleanup */
      g_strfreev (mt);

      /* if the program is not present, there's no reason to allocate a MimeApplication for it */
      if (G_UNLIKELY (!present))
        {
          xfce_rc_close (rc);
          return NULL;
        }
    }

  /* generate the application object */
  if (G_LIKELY (exec != NULL && name != NULL && g_utf8_validate (name, -1, NULL)))
    {
      /* we assume %f if the application hasn't set anything else,
       * as that's also what KDE and Gnome do in this case.
       */
      if (strstr (exec, "%f") == NULL && strstr (exec, "%F") == NULL && strstr (exec, "%u") == NULL && strstr (exec, "%U") == NULL)
        command = g_strconcat (exec, " %f", NULL);
      else
        command = g_strdup (exec);

      /* determine the flags for the application */
      if (G_UNLIKELY (xfce_rc_read_bool_entry (rc, "Terminal", FALSE)))
        flags |= THUNAR_VFS_MIME_HANDLER_REQUIRES_TERMINAL;
      if (xfce_rc_read_bool_entry (rc, "Hidden", FALSE) || xfce_rc_read_bool_entry (rc, "NoDisplay", FALSE))
        flags |= THUNAR_VFS_MIME_HANDLER_HIDDEN;
      if (xfce_rc_read_bool_entry (rc, "StartupNotify", FALSE) || xfce_rc_read_bool_entry (rc, "X-KDE-StartupNotify", FALSE))
        flags |= THUNAR_VFS_MIME_HANDLER_SUPPORTS_STARTUP_NOTIFY;
      if ((strstr (command, "%F") != NULL) || (strstr (command, "%U") != NULL))
        flags |= THUNAR_VFS_MIME_HANDLER_SUPPORTS_MULTI;

      /* allocate a new application instance */
      application = g_object_new (THUNAR_VFS_TYPE_MIME_APPLICATION, "command", command, "flags", flags, "icon", icon, "name", name, NULL);
      application->desktop_id = g_strdup (desktop_id);

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

      /* determine the list of desktop actions supported by the application */
      actions = xfce_rc_read_list_entry (rc, "Actions", ";");
      if (G_UNLIKELY (actions != NULL))
        {
          /* add ThunarVfsMimeAction's for all specified desktop actions */
          for (n = 0; actions[n] != NULL; ++n)
            {
              /* determine the group name */
              group = g_strconcat ("Desktop Action ", actions[n], NULL);
              if (xfce_rc_has_group (rc, group))
                {
                  /* determine the attributes for the action */
                  xfce_rc_set_group (rc, group);
                  name = xfce_rc_read_entry (rc, "Name", NULL);
                  exec = xfce_rc_read_entry_untranslated (rc, "Exec", NULL);
                  icon = xfce_rc_read_entry_untranslated (rc, "Icon", NULL);

                  /* check if the required attributes were given */
                  if (exec != NULL && name != NULL && g_utf8_validate (name, -1, NULL))
                    {
                      /* check if the actions support multiple files */
                      if ((strstr (exec, "%F") != NULL) || (strstr (exec, "%U") != NULL))
                        flags |= THUNAR_VFS_MIME_HANDLER_SUPPORTS_MULTI;
                      else
                        flags &= ~THUNAR_VFS_MIME_HANDLER_SUPPORTS_MULTI;

                      /* don't trust application maintainers! :-) */
                      flags &= ~THUNAR_VFS_MIME_HANDLER_SUPPORTS_STARTUP_NOTIFY;

                      /* allocate and add the mime action instance */
                      action = _thunar_vfs_mime_action_new (exec, name, (icon != NULL) ? icon : THUNAR_VFS_MIME_HANDLER (application)->icon, flags);
                      application->actions = g_list_append (application->actions, action);
                    }
                }

              /* release the group name */
              g_free (group);
            }

          /* cleanup */
          g_strfreev (actions);
        }

      /* cleanup */
      g_free (command);
    }

  /* close the file */
  xfce_rc_close (rc);

  return application;
}



/**
 * thunar_vfs_mime_application_is_usercreated:
 * @mime_application : a #ThunarVfsMimeApplication.
 *
 * Returns %TRUE if the @mime_application was created by the user
 * using a file manager, i.e. through the "Open With" dialog in
 * Thunar.
 *
 * Return value: %TRUE if @mime_application is usercreated.
 **/
gboolean
thunar_vfs_mime_application_is_usercreated (const ThunarVfsMimeApplication *mime_application)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (mime_application), FALSE);
  return (strstr (mime_application->desktop_id, "-usercreated") != NULL);
}



/**
 * thunar_vfs_mime_application_get_actions:
 * @mime_application : a #ThunarVfsMimeApplication.
 *
 * Returns the list of #ThunarVfsMimeAction<!---->s available
 * for the @mime_application. The #ThunarVfsMimeAction<!---->s
 * are an implementation of the desktop actions mentioned in
 * the desktop entry specification.
 *
 * The caller is responsible to free the returned list using
 * <informalexample><programlisting>
 * g_list_foreach (list, (GFunc) g_object_unref, NULL);
 * g_list_free (list);
 * </programlisting></informalexample>
 * when no longer needed.
 *
 * Return value: the list of #ThunarVfsMimeAction<!---->s
 *               for the @mime_application.
 **/
GList*
thunar_vfs_mime_application_get_actions (ThunarVfsMimeApplication *mime_application)
{
  GList *mime_actions;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (mime_application), NULL);

  /* take a deep copy of the mime actions list */
  mime_actions = g_list_copy (mime_application->actions);
  g_list_foreach (mime_actions, (GFunc) g_object_ref, NULL);

  return mime_actions;
}



/**
 * thunar_vfs_mime_application_get_desktop_id:
 * @mime_application : a #ThunarVfsMimeApplication.
 *
 * Returns the desktop-id of @mime_application.
 *
 * Return value: the desktop-id of @mime_application.
 **/
const gchar*
thunar_vfs_mime_application_get_desktop_id (const ThunarVfsMimeApplication *mime_application)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (mime_application), NULL);
  return mime_application->desktop_id;
}



/**
 * thunar_vfs_mime_application_get_mime_types:
 * @mime_application : a #ThunarVfsMimeApplication.
 *
 * Returns the list of MIME-types supported by @application
 * or %NULL if the @mime_application doesn't support any MIME-types
 * at all.
 *
 * The returned %NULL-terminated string array is owned by
 * @mime_application and must not be free by the caller.
 *
 * Return value: the list of supported MIME-types for @mime_application.
 **/
const gchar* const*
thunar_vfs_mime_application_get_mime_types (const ThunarVfsMimeApplication *mime_application)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (mime_application), NULL);
  return (gconstpointer) mime_application->mime_types;
}



/**
 * thunar_vfs_mime_application_hash:
 * @mime_application : a #ThunarVfsMimeApplication.
 *
 * Converts @mime_application to a hash value. It can be 
 * passed to g_hash_table_new() as the @hash_func parameter,
 * when using #ThunarVfsMimeApplication<!---->s as keys
 * in a #GHashTable.
 *
 * Return value: a hash value corresponding to the key.
 **/
guint
thunar_vfs_mime_application_hash (gconstpointer mime_application)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_APPLICATION (mime_application), 0);
  return g_str_hash (THUNAR_VFS_MIME_APPLICATION (mime_application)->desktop_id);
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
  return exo_str_is_equal (THUNAR_VFS_MIME_APPLICATION (a)->desktop_id,
                           THUNAR_VFS_MIME_APPLICATION (b)->desktop_id);
}



#define __THUNAR_VFS_MIME_APPLICATION_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
