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

#include <thunar-vfs/thunar-vfs-mime-application.h>



static void thunar_vfs_mime_application_register_type (GType                         *type);
static void thunar_vfs_mime_application_class_init    (ThunarVfsMimeApplicationClass *klass);
static void thunar_vfs_mime_application_finalize      (ExoObject                     *object);



struct _ThunarVfsMimeApplicationClass
{
  ExoObjectClass __parent__;
};

struct _ThunarVfsMimeApplication
{
  ExoObject __parent__;
  gchar   **argv;
  gchar    *binary_name;
  gchar    *desktop_id;
  gchar    *icon;
  gchar    *name;

  guint     startup_notify : 1;
  guint     terminal : 1;
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
  g_strfreev (application->argv);
  g_free (application->binary_name);
  g_free (application->desktop_id);
  g_free (application->icon);
  g_free (application->name);

  (*EXO_OBJECT_CLASS (thunar_vfs_mime_application_parent_class)->finalize) (object);
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
  name = xfce_rc_read_entry (rc, "GenericName", NULL);
  if (G_UNLIKELY (name == NULL))
    name = xfce_rc_read_entry (rc, "Name", NULL);
  icon = xfce_rc_read_entry (rc, "Icon", NULL);

  /* generate the application object */
  if (G_LIKELY (exec != NULL && name != NULL && g_shell_parse_argv (exec, NULL, &argv, NULL)))
    {
      application = exo_object_new (THUNAR_VFS_TYPE_MIME_APPLICATION);
      application->argv = argv;
      application->binary_name = g_path_get_basename (argv[0]);
      application->desktop_id = g_strdup (desktop_id);
      application->name = g_strdup (name);
      application->icon = g_strdup (icon);

      application->startup_notify = xfce_rc_read_bool_entry (rc, "StartupNotify", FALSE) || xfce_rc_read_bool_entry (rc, "X-KDE-StartupNotify", FALSE);
      application->terminal = xfce_rc_read_bool_entry (rc, "Terminal", FALSE);
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




