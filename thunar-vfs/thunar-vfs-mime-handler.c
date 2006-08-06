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

#include <thunar-vfs/thunar-vfs-enum-types.h>
#include <thunar-vfs/thunar-vfs-exec.h>
#include <thunar-vfs/thunar-vfs-mime-handler-private.h>
#include <thunar-vfs/thunar-vfs-mime-handler.h>
#include <thunar-vfs/thunar-vfs-path-private.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_COMMAND,
  PROP_FLAGS,
  PROP_ICON,
  PROP_NAME,
};



static void         thunar_vfs_mime_handler_class_init    (ThunarVfsMimeHandlerClass  *klass);
static void         thunar_vfs_mime_handler_finalize      (GObject                    *object);
static void         thunar_vfs_mime_handler_get_property  (GObject                    *object,
                                                           guint                       prop_id,
                                                           GValue                     *value,
                                                           GParamSpec                 *pspec);
static void         thunar_vfs_mime_handler_set_property  (GObject                    *object,
                                                           guint                       prop_id,
                                                           const GValue               *value,
                                                           GParamSpec                 *pspec);
static gboolean     thunar_vfs_mime_handler_execute       (const ThunarVfsMimeHandler *mime_handler,
                                                           GdkScreen                  *screen,
                                                           GList                      *path_list,
                                                           gchar                     **envp,
                                                           GError                    **error);
static gboolean     thunar_vfs_mime_handler_get_argv      (const ThunarVfsMimeHandler *mime_handler,
                                                           GList                      *path_list,
                                                           gint                       *argc,
                                                           gchar                    ***argv,
                                                           GError                    **error);
static void         thunar_vfs_mime_handler_set_command   (ThunarVfsMimeHandler       *mime_handler,
                                                           const gchar                *command);
static void         thunar_vfs_mime_handler_set_flags     (ThunarVfsMimeHandler       *mime_handler,
                                                           ThunarVfsMimeHandlerFlags   flags);
static const gchar *thunar_vfs_mime_handler_get_icon      (const ThunarVfsMimeHandler *mime_handler);
static void         thunar_vfs_mime_handler_set_icon      (ThunarVfsMimeHandler       *mime_handler,
                                                           const gchar                *icon);
static void         thunar_vfs_mime_handler_set_name      (ThunarVfsMimeHandler       *mime_handler,
                                                           const gchar                *name);



static GObjectClass *thunar_vfs_mime_handler_parent_class;



GType
thunar_vfs_mime_handler_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = _thunar_vfs_g_type_register_simple (G_TYPE_OBJECT,
                                                 "ThunarVfsMimeHandler",
                                                 sizeof (ThunarVfsMimeHandlerClass),
                                                 thunar_vfs_mime_handler_class_init,
                                                 sizeof (ThunarVfsMimeHandler),
                                                 NULL,
                                                 0);
    }

  return type;
}



static void
thunar_vfs_mime_handler_class_init (ThunarVfsMimeHandlerClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_vfs_mime_handler_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_mime_handler_finalize;
  gobject_class->get_property = thunar_vfs_mime_handler_get_property;
  gobject_class->set_property = thunar_vfs_mime_handler_set_property;

  /**
   * ThunarVfsMimeHandler:command:
   *
   * The command line for this #ThunarVfsMimeHandler.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_COMMAND,
                                   g_param_spec_string ("command",
                                                        _("Command"),
                                                        _("The command to run the mime handler"),
                                                        NULL,
                                                        EXO_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * ThunarVfsMimeHandler:flags:
   *
   * The #ThunarVfsMimeHandlerFlags for this #ThunarVfsMimeHandler.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FLAGS,
                                   g_param_spec_flags ("flags",
                                                       _("Flags"),
                                                       _("The flags for the mime handler"),
                                                       THUNAR_VFS_TYPE_VFS_MIME_HANDLER_FLAGS,
                                                       0,
                                                       EXO_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * ThunarVfsMimeHandler:icon:
   *
   * The icon of this #ThunarVfsMimeHandler, which can be either
   * %NULL in which case no icon is known, an absolute path to
   * an icon file, or a named icon.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ICON,
                                   g_param_spec_string ("icon",
                                                        _("Icon"),
                                                        _("The icon of the mime handler"),
                                                        NULL,
                                                        EXO_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));

  /**
   * ThunarVfsMimeHandler:name:
   *
   * The name of this #ThunarVfsMimeHandler.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_NAME,
                                   g_param_spec_string ("name",
                                                        _("Name"),
                                                        _("The name of the mime handler"),
                                                        NULL,
                                                        EXO_PARAM_READWRITE | G_PARAM_CONSTRUCT_ONLY));
}



static void
thunar_vfs_mime_handler_finalize (GObject *object)
{
  ThunarVfsMimeHandler *mime_handler = THUNAR_VFS_MIME_HANDLER (object);

  /* release the attributes */
  g_free (mime_handler->binary_name);
  g_free (mime_handler->command);
  g_free (mime_handler->name);
  g_free (mime_handler->icon);

  (*G_OBJECT_CLASS (thunar_vfs_mime_handler_parent_class)->finalize) (object);
}



static void
thunar_vfs_mime_handler_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ThunarVfsMimeHandler *mime_handler = THUNAR_VFS_MIME_HANDLER (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      g_value_set_string (value, thunar_vfs_mime_handler_get_command (mime_handler));
      break;

    case PROP_FLAGS:
      g_value_set_flags (value, thunar_vfs_mime_handler_get_flags (mime_handler));
      break;

    case PROP_ICON:
      g_value_set_string (value, thunar_vfs_mime_handler_get_icon (mime_handler));
      break;

    case PROP_NAME:
      g_value_set_string (value, thunar_vfs_mime_handler_get_name (mime_handler));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_vfs_mime_handler_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ThunarVfsMimeHandler *mime_handler = THUNAR_VFS_MIME_HANDLER (object);

  switch (prop_id)
    {
    case PROP_COMMAND:
      thunar_vfs_mime_handler_set_command (mime_handler, g_value_get_string (value));
      break;

    case PROP_FLAGS:
      thunar_vfs_mime_handler_set_flags (mime_handler, g_value_get_flags (value));
      break;

    case PROP_ICON:
      thunar_vfs_mime_handler_set_icon (mime_handler, g_value_get_string (value));
      break;

    case PROP_NAME:
      thunar_vfs_mime_handler_set_name (mime_handler, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static gboolean
thunar_vfs_mime_handler_execute (const ThunarVfsMimeHandler *mime_handler,
                                 GdkScreen                  *screen,
                                 GList                      *path_list,
                                 gchar                     **envp,
                                 GError                    **error)
{
  ThunarVfsPath *parent;
  gboolean       result;
  gchar         *working_directory;
  gchar        **argv;
  gint           argc;

  /* parse the command line */
  if (!thunar_vfs_mime_handler_get_argv (mime_handler, path_list, &argc, &argv, error))
    return FALSE;

  /* use the first paths base directory as working directory for the application */
  parent = (path_list != NULL) ? thunar_vfs_path_get_parent (path_list->data) : NULL;
  working_directory = (parent != NULL) ? _thunar_vfs_path_translate_dup_string (parent, THUNAR_VFS_PATH_SCHEME_FILE, NULL) : NULL;

  /* try to spawn the application */
  result = thunar_vfs_exec_on_screen (screen, working_directory, argv, envp, G_SPAWN_SEARCH_PATH,
                                      mime_handler->flags & THUNAR_VFS_MIME_HANDLER_SUPPORTS_STARTUP_NOTIFY, error);

  /* cleanup */
  g_free (working_directory);
  g_strfreev (argv);

  return result;
}



static gboolean
thunar_vfs_mime_handler_get_argv (const ThunarVfsMimeHandler *mime_handler,
                                  GList                      *path_list,
                                  gint                       *argc,
                                  gchar                    ***argv,
                                  GError                    **error)
{
  return thunar_vfs_exec_parse (mime_handler->command, path_list, mime_handler->icon, mime_handler->name, NULL,
                                (mime_handler->flags & THUNAR_VFS_MIME_HANDLER_REQUIRES_TERMINAL) != 0,
                                argc, argv, error);
}



static void
thunar_vfs_mime_handler_set_command (ThunarVfsMimeHandler *mime_handler,
                                     const gchar          *command)
{
  gchar **argv;
  gint    argc;

  /* release the previous command and binary name */
  g_free (mime_handler->binary_name);
  g_free (mime_handler->command);

  /* determine the new binary name */
  if (command != NULL && g_shell_parse_argv (command, &argc, &argv, NULL))
    {
      /* yep, we have a new binary name */
      mime_handler->binary_name = g_path_get_basename (argv[0]);
      g_strfreev (argv);
    }
  else
    {
      /* no binary name */
      mime_handler->binary_name = NULL;
    }

  /* determine the new command */
  mime_handler->command = g_strdup (command);
}



static void
thunar_vfs_mime_handler_set_flags (ThunarVfsMimeHandler     *mime_handler,
                                   ThunarVfsMimeHandlerFlags flags)
{
  mime_handler->flags = flags;
}



static const gchar*
thunar_vfs_mime_handler_get_icon (const ThunarVfsMimeHandler *mime_handler)
{
  return mime_handler->icon;
}



static void
thunar_vfs_mime_handler_set_icon (ThunarVfsMimeHandler *mime_handler,
                                  const gchar          *icon)
{
  gint icon_len;

  /* release the previous icon */
  g_free (mime_handler->icon);

  /* setup the new icon */
  mime_handler->icon = g_strdup (icon);

  /* strip off known suffixes for image files if a themed icon is specified */
  if (mime_handler->icon != NULL && !g_path_is_absolute (mime_handler->icon))
    {
      /* check if the icon name ends in .png */
      icon_len = strlen (mime_handler->icon);
      if (G_LIKELY (icon_len > 4) && strcmp (mime_handler->icon + (icon_len - 4), ".png") == 0)
        mime_handler->icon[icon_len - 4] = '\0';
    }
}



static void
thunar_vfs_mime_handler_set_name (ThunarVfsMimeHandler *mime_handler,
                                  const gchar          *name)
{
  g_free (mime_handler->name);
  mime_handler->name = g_strdup (name);
}



/**
 * thunar_vfs_mime_handler_get_command:
 * @mime_handler : a #ThunarVfsMimeHandler.
 *
 * Returns the command associated with @mime_handler.
 *
 * Return value: the command associated with @mime_handler.
 **/
const gchar*
thunar_vfs_mime_handler_get_command (const ThunarVfsMimeHandler *mime_handler)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_HANDLER (mime_handler), NULL);
  return mime_handler->command;
}



/**
 * thunar_vfs_mime_handler_get_flags:
 * @mime_handler : a #ThunarVfsMimeHandler.
 *
 * Returns the #ThunarVfsMimeHandlerFlags for @mime_handler.
 *
 * Return value: the #ThunarVfsMimeHandlerFlags for @mime_handler.
 **/
ThunarVfsMimeHandlerFlags
thunar_vfs_mime_handler_get_flags (const ThunarVfsMimeHandler *mime_handler)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_HANDLER (mime_handler), 0);
  return mime_handler->flags;
}



/**
 * thunar_vfs_mime_handler_get_name:
 * @mime_handler : a #ThunarVfsMimeHandler.
 *
 * Returns the name of @mime_handler.
 *
 * Return value: the name of @mime_handler.
 **/
const gchar*
thunar_vfs_mime_handler_get_name (const ThunarVfsMimeHandler *mime_handler)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_HANDLER (mime_handler), NULL);
  return mime_handler->name;
}



/**
 * thunar_vfs_mime_handler_exec:
 * @mime_handler : a #ThunarVfsMimeHandler.
 * @screen       : a #GdkScreen or %NULL to use the default screen.
 * @path_list    : a list of #ThunarVfsPath<!---->s to open.
 * @error        : return location for errors or %NULL.
 *
 * Wrapper to thunar_vfs_mime_handler_exec_with_env(), which
 * simply passes a %NULL pointer for the environment variables.
 *
 * Return value: %TRUE if the execution succeed, else %FALSE.
 **/
gboolean
thunar_vfs_mime_handler_exec (const ThunarVfsMimeHandler *mime_handler,
                              GdkScreen                  *screen,
                              GList                      *path_list,
                              GError                    **error)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_HANDLER (mime_handler), FALSE);
  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  return thunar_vfs_mime_handler_exec_with_env (mime_handler, screen, path_list, NULL, error);
}



/**
 * thunar_vfs_mime_handler_exec_with_env:
 * @mime_handler : a #ThunarVfsMimeHandler.
 * @screen       : a #GdkScreen or %NULL to use the default screen.
 * @path_list    : a list of #ThunarVfsPath<!---->s to open.
 * @envp         : child's environment or %NULL to inherit parent's.
 * @error        : return location for errors or %NULL.
 *
 * Executes @mime_handler on @screen using the given @path_list. If
 * @path_list contains more than one #ThunarVfsPath and @mime_handler
 * doesn't support opening multiple documents at once, one
 * instance of @mime_handler will be spawned for every #ThunarVfsPath
 * given in @path_list.
 *
 * Return value: %TRUE if the execution succeed, else %FALSE.
 **/
gboolean
thunar_vfs_mime_handler_exec_with_env (const ThunarVfsMimeHandler *mime_handler,
                                       GdkScreen                  *screen,
                                       GList                      *path_list,
                                       gchar                     **envp,
                                       GError                    **error)
{
  gboolean succeed = TRUE;
  GList    list;
  GList   *lp;

  g_return_val_if_fail (THUNAR_VFS_IS_MIME_HANDLER (mime_handler), FALSE);
  g_return_val_if_fail (screen == NULL || GDK_IS_SCREEN (screen), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* fallback to the default screen if no screen is given */
  if (G_UNLIKELY (screen == NULL))
    screen = gdk_screen_get_default ();

  /* check whether the application can open multiple documents at once */
  if (G_LIKELY ((mime_handler->flags & THUNAR_VFS_MIME_HANDLER_SUPPORTS_MULTI) == 0 && path_list != NULL))
    {
      /* fake an empty list */
      list.next = NULL;
      list.prev = NULL;

      /* spawn a new instance for each path item */
      for (lp = path_list; lp != NULL && succeed; lp = lp->next)
        {
          /* "insert" into faked list */
          list.data = lp->data;

          /* spawn the instance */
          succeed = thunar_vfs_mime_handler_execute (mime_handler, screen, &list, envp, error);
        }
    }
  else
    {
      /* spawn a single instance for all path items */
      succeed = thunar_vfs_mime_handler_execute (mime_handler, screen, path_list, envp, error);
    }

  return succeed;
}



/**
 * thunar_vfs_mime_handler_lookup_icon_name:
 * @mime_handler : a #ThunarVfsMimeHandler.
 * @icon_theme   : a #GtkIconTheme.
 *
 * Looks up the icon name for @mime_handler in
 * @icon_theme. Returns %NULL if no suitable
 * icon is present in @icon_theme.
 *
 * The returned icon can be either a named icon in
 * @icon_theme or an absolute path to an icon file,
 * or %NULL.
 *
 * Return value: the icon name for @mime_handler or
 *               %NULL.
 **/
const gchar*
thunar_vfs_mime_handler_lookup_icon_name (const ThunarVfsMimeHandler *mime_handler,
                                          GtkIconTheme               *icon_theme)
{
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_HANDLER (mime_handler), NULL);
  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);

  if (mime_handler->icon != NULL && (g_path_is_absolute (mime_handler->icon) || gtk_icon_theme_has_icon (icon_theme, mime_handler->icon)))
    return mime_handler->icon;
  else if (mime_handler->binary_name != NULL && gtk_icon_theme_has_icon (icon_theme, mime_handler->binary_name))
    return mime_handler->binary_name;
  else
    return NULL;
}



#define __THUNAR_VFS_MIME_HANDLER_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
