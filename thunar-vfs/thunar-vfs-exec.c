/* $Id$ */
/*-
 * Copyright (c) 2005-2007 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDARG_H
#include <stdarg.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar-vfs/thunar-vfs-exec.h>
#include <thunar-vfs/thunar-vfs-path-private.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>

#ifdef GDK_WINDOWING_X11
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#endif



static void     tve_string_append_quoted      (GString             *string,
                                               const gchar         *unquoted);
static gboolean tve_string_append_quoted_path (GString             *string,
                                               ThunarVfsPath       *path,
                                               GError             **error);
static void     tve_string_append_quoted_uri  (GString             *string,
                                               const ThunarVfsPath *path);



static void
tve_string_append_quoted (GString     *string,
                          const gchar *unquoted)
{
  gchar *quoted;

  quoted = g_shell_quote (unquoted);
  g_string_append (string, quoted);
  g_free (quoted);
}



static gboolean
tve_string_append_quoted_path (GString       *string,
                               ThunarVfsPath *path,
                               GError       **error)
{
  gchar *absolute_path;

  /* append the absolute, local, quoted path to the string */
  absolute_path = _thunar_vfs_path_translate_dup_string (path, THUNAR_VFS_PATH_SCHEME_FILE, error);
  if (G_LIKELY (absolute_path != NULL))
    {
      tve_string_append_quoted (string, absolute_path);
      g_free (absolute_path);
    }

  return (absolute_path != NULL);
}



static void
tve_string_append_quoted_uri (GString             *string,
                              const ThunarVfsPath *path)
{
  gchar *uri;

  /* append the quoted URI for the path */
  uri = thunar_vfs_path_dup_uri (path);
  tve_string_append_quoted (string, uri);
  g_free (uri);
}



/**
 * thunar_vfs_exec_parse:
 * @exec      : the value of the <literal>Exec</literal> field.
 * @path_list : the list of #ThunarVfsPath<!---->s.
 * @icon      : value of the <literal>Icon</literal> field or %NULL.
 * @name      : translated value for the <literal>Name</literal> field or %NULL.
 * @path      : full path to the desktop file or %NULL.
 * @terminal  : whether to execute the command in a terminal.
 * @argc      : return location for the number of items placed into @argv.
 * @argv      : return location for the argument vector.
 * @error     : return location for errors or %NULL.
 *
 * Substitutes <literal>Exec</literal> parameter variables according
 * to the <ulink href="http://freedesktop.org/wiki/Standards_2fdesktop_2dentry_2dspec"
 * type="http">Desktop Entry Specification</ulink> and returns the
 * parsed argument vector (in @argv) and the number of items placed
 * into @argv (in @argc).
 *
 * The @icon, @name and @path fields are optional and may be %NULL
 * if you don't know their values. The @icon parameter should be
 * the value of the <literal>Icon</literal> field from the desktop
 * file, the @name parameter should be the translated <literal>Name</literal>
 * value, while the @path parameter should refer to the full path
 * to the desktop file, whose <literal>Exec</literal> field is
 * being parsed here.
 *
 * Return value: %TRUE on success, else %FALSE.
 **/
gboolean
thunar_vfs_exec_parse (const gchar *exec,
                       GList       *path_list,
                       const gchar *icon,
                       const gchar *name,
                       const gchar *path,
                       gboolean     terminal,
                       gint        *argc,
                       gchar     ***argv,
                       GError     **error)
{
  ThunarVfsPath *parent;
  const gchar   *p;
  gboolean       result = FALSE;
  GString       *command_line = g_string_new (NULL);
  GList         *lp;

  /* prepend terminal command if required */
  if (G_UNLIKELY (terminal))
    g_string_append (command_line, "exo-open --launch TerminalEmulator ");

  for (p = exec; *p != '\0'; ++p)
    {
      if (p[0] == '%' && p[1] != '\0')
        {
          switch (*++p)
            {
            case 'f':
              /* append the absolute local path of the first path object */
              if (path_list != NULL && !tve_string_append_quoted_path (command_line, path_list->data, error))
                goto done;
              break;

            case 'F':
              for (lp = path_list; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != path_list))
                    g_string_append_c (command_line, ' ');
                  if (!tve_string_append_quoted_path (command_line, lp->data, error))
                    goto done;
                }
              break;

            case 'u':
              if (G_LIKELY (path_list != NULL))
                tve_string_append_quoted_uri (command_line, path_list->data);
              break;

            case 'U':
              for (lp = path_list; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != path_list))
                    g_string_append_c (command_line, ' ');
                  tve_string_append_quoted_uri (command_line, lp->data);
                }
              break;

            case 'd':
              if (G_LIKELY (path_list != NULL))
                {
                  parent = thunar_vfs_path_get_parent (path_list->data);
                  if (parent != NULL && !tve_string_append_quoted_path (command_line, parent, error))
                    goto done;
                }
              break;

            case 'D':
              for (lp = path_list; lp != NULL; lp = lp->next)
                {
                  parent = thunar_vfs_path_get_parent (lp->data);
                  if (G_LIKELY (parent != NULL))
                    {
                      if (G_LIKELY (lp != path_list))
                        g_string_append_c (command_line, ' ');
                      if (!tve_string_append_quoted_path (command_line, parent, error))
                        goto done;
                    }
                }
              break;

            case 'n':
              if (G_LIKELY (path_list != NULL))
                tve_string_append_quoted (command_line, thunar_vfs_path_get_name (path_list->data));
              break;

            case 'N':
              for (lp = path_list; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != path_list))
                    g_string_append_c (command_line, ' ');
                  tve_string_append_quoted (command_line, thunar_vfs_path_get_name (lp->data));
                }
              break;

            case 'i':
              if (G_LIKELY (icon != NULL))
                {
                  g_string_append (command_line, "--icon ");
                  tve_string_append_quoted (command_line, icon);
                }
              break;

            case 'c':
              if (G_LIKELY (name != NULL))
                tve_string_append_quoted (command_line, name);
              break;

            case 'k':
              if (G_LIKELY (path != NULL))
                tve_string_append_quoted (command_line, path);
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

  result = g_shell_parse_argv (command_line->str, argc, argv, error);

done:
  g_string_free (command_line, TRUE);
  return result;
}



#ifdef HAVE_LIBSTARTUP_NOTIFICATION
#include <libsn/sn.h>

/* the max. timeout for an application to startup */
#define TVSN_STARTUP_TIMEOUT (30 * 1000)

typedef struct
{
  SnLauncherContext *sn_launcher;
  guint              timeout_id;
  guint              watch_id;
  GPid               pid;
} TvsnStartupData;

static gboolean
tvsn_startup_timeout (gpointer data)
{
  TvsnStartupData *startup_data = data;
  GTimeVal         now;
  gdouble          elapsed;
  glong            tv_sec;
  glong            tv_usec;

  GDK_THREADS_ENTER ();

  /* determine the amount of elapsed time */
  g_get_current_time (&now);
  sn_launcher_context_get_last_active_time (startup_data->sn_launcher, &tv_sec, &tv_usec);
  elapsed = (((gdouble) now.tv_sec - tv_sec) * G_USEC_PER_SEC + (now.tv_usec - tv_usec)) / 1000.0;

  /* check if the timeout was reached */
  if (elapsed >= TVSN_STARTUP_TIMEOUT)
    {
      /* abort the startup notification */
      sn_launcher_context_complete (startup_data->sn_launcher);
      sn_launcher_context_unref (startup_data->sn_launcher);
      startup_data->sn_launcher = NULL;
    }

  GDK_THREADS_LEAVE ();

  /* keep the startup timeout if not elapsed */
  return (elapsed < TVSN_STARTUP_TIMEOUT);
}

static void
tvsn_startup_timeout_destroy (gpointer data)
{
  TvsnStartupData *startup_data = data;

  _thunar_vfs_return_if_fail (startup_data->sn_launcher == NULL);

  /* cancel the watch (if any) */
  if (startup_data->watch_id != 0)
    g_source_remove (startup_data->watch_id);

  /* make sure we don't leave zombies (see bug #2983 for details) */
  g_child_watch_add_full (G_PRIORITY_LOW, startup_data->pid,
                          (GChildWatchFunc) g_spawn_close_pid,
                          NULL, NULL);

  /* release the startup data */
  _thunar_vfs_slice_free (TvsnStartupData, startup_data);
}

static void
tvsn_startup_watch (GPid     pid,
                    gint     status,
                    gpointer data)
{
  TvsnStartupData *startup_data = data;

  _thunar_vfs_return_if_fail (startup_data->sn_launcher != NULL);
  _thunar_vfs_return_if_fail (startup_data->watch_id != 0);
  _thunar_vfs_return_if_fail (startup_data->pid == pid);

  /* abort the startup notification (application exited) */
  sn_launcher_context_complete (startup_data->sn_launcher);
  sn_launcher_context_unref (startup_data->sn_launcher);
  startup_data->sn_launcher = NULL;

  /* cancel the startup notification timeout */
  g_source_remove (startup_data->timeout_id);
}

static gint
tvsn_get_active_workspace_number (GdkScreen *screen)
{
  GdkWindow *root;
  gulong     bytes_after_ret = 0;
  gulong     nitems_ret = 0;
  guint     *prop_ret = NULL;
  Atom       _NET_CURRENT_DESKTOP;
  Atom       _WIN_WORKSPACE;
  Atom       type_ret = None;
  gint       format_ret;
  gint       ws_num = 0;

  gdk_error_trap_push ();

  root = gdk_screen_get_root_window (screen);

  /* determine the X atom values */
  _NET_CURRENT_DESKTOP = XInternAtom (GDK_WINDOW_XDISPLAY (root), "_NET_CURRENT_DESKTOP", False);
  _WIN_WORKSPACE = XInternAtom (GDK_WINDOW_XDISPLAY (root), "_WIN_WORKSPACE", False);

  if (XGetWindowProperty (GDK_WINDOW_XDISPLAY (root), GDK_WINDOW_XWINDOW (root),
                          _NET_CURRENT_DESKTOP, 0, 32, False, XA_CARDINAL,
                          &type_ret, &format_ret, &nitems_ret, &bytes_after_ret,
                          (gpointer) &prop_ret) != Success)
    {
      if (XGetWindowProperty (GDK_WINDOW_XDISPLAY (root), GDK_WINDOW_XWINDOW (root),
                              _WIN_WORKSPACE, 0, 32, False, XA_CARDINAL,
                              &type_ret, &format_ret, &nitems_ret, &bytes_after_ret,
                              (gpointer) &prop_ret) != Success)
        {
          if (G_UNLIKELY (prop_ret != NULL))
            {
              XFree (prop_ret);
              prop_ret = NULL;
            }
        }
    }

  if (G_LIKELY (prop_ret != NULL))
    {
      if (G_LIKELY (type_ret != None && format_ret != 0))
        ws_num = *prop_ret;
      XFree (prop_ret);
    }

  gdk_error_trap_pop ();

  return ws_num;
}
#endif



/**
 * thunar_vfs_exec_on_screen:
 * @screen            : a #GdkScreen.
 * @working_directory : child's current working directory or %NULL to inherit parent's.
 * @argv              : child's argument vector.
 * @envp              : child's environment vector or %NULL to inherit parent's.
 * @flags             : flags from #GSpawnFlags.
 * @startup_notify    : whether to use startup notification.
 * @icon_name         : application icon or %NULL.
 * @error             : return location for errors or %NULL.
 *
 * Like gdk_spawn_on_screen(), but also supports startup notification
 * (if Thunar-VFS was built with startup notification support).
 *
 * Return value: %TRUE on success, %FALSE if @error is set.
 **/
gboolean
thunar_vfs_exec_on_screen (GdkScreen   *screen,
                           const gchar *working_directory,
                           gchar      **argv,
                           gchar      **envp,
                           GSpawnFlags  flags,
                           gboolean     startup_notify,
                           const gchar *icon_name,
                           GError     **error)
{
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
  SnLauncherContext *sn_launcher = NULL;
  TvsnStartupData   *startup_data;
  SnDisplay         *sn_display = NULL;
  gint               sn_workspace;
#endif
  extern gchar     **environ;
  gboolean           succeed;
  gchar             *display_name;
  gchar            **cenvp = envp;
  gint               n_cenvp, n;
  GPid               pid;

  /* setup the child environment (stripping $DESKTOP_STARTUP_ID and $DISPLAY) */
  if (G_LIKELY (envp == NULL))
    envp = (gchar **) environ;
  for (n = 0; envp[n] != NULL; ++n) ;
  cenvp = g_new0 (gchar *, n + 3);
  for (n_cenvp = n = 0; envp[n] != NULL; ++n)
    if (strncmp (envp[n], "DESKTOP_STARTUP_ID", 18) != 0 && strncmp (envp[n], "DISPLAY", 7) != 0)
      cenvp[n_cenvp++] = g_strdup (envp[n]);

  /* add the real display name for the screen */
  display_name = gdk_screen_make_display_name (screen);
  cenvp[n_cenvp++] = g_strconcat ("DISPLAY=", display_name, NULL);
  g_free (display_name);

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
  /* initialize the sn launcher context */
  if (G_LIKELY (startup_notify))
    {
      sn_display = sn_display_new (GDK_SCREEN_XDISPLAY (screen),
                                   (SnDisplayErrorTrapPush) gdk_error_trap_push,
                                   (SnDisplayErrorTrapPop) gdk_error_trap_pop);

      if (G_LIKELY (sn_display != NULL))
        {
          sn_launcher = sn_launcher_context_new (sn_display, GDK_SCREEN_XNUMBER (screen));

          if (G_LIKELY (sn_launcher != NULL && !sn_launcher_context_get_initiated (sn_launcher)))
            {
              /* initiate the sn launcher context */
              sn_workspace = tvsn_get_active_workspace_number (screen);
              sn_launcher_context_set_binary_name (sn_launcher, argv[0]);
              sn_launcher_context_set_workspace (sn_launcher, sn_workspace);
              sn_launcher_context_set_icon_name (sn_launcher, (icon_name != NULL) ? icon_name : "applications-other");
              sn_launcher_context_initiate (sn_launcher, g_get_prgname (), argv[0], gtk_get_current_event_time ());

              /* add the real startup id to the child environment */
              cenvp[n_cenvp++] = g_strconcat ("DESKTOP_STARTUP_ID=", sn_launcher_context_get_startup_id (sn_launcher), NULL);

              /* we want to watch the child process */
              flags |= G_SPAWN_DO_NOT_REAP_CHILD;
            }
        }
    }
#endif

  /* try to spawn the new process */
  succeed = g_spawn_async (working_directory, argv, cenvp, flags, NULL, NULL, &pid, error);

#ifdef HAVE_LIBSTARTUP_NOTIFICATION
  /* handle the sn launcher context */
  if (G_LIKELY (sn_launcher != NULL))
    {
      if (G_UNLIKELY (!succeed))
        {
          /* abort the sn sequence */
          sn_launcher_context_complete (sn_launcher);
          sn_launcher_context_unref (sn_launcher);
        }
      else
        {
          /* schedule a startup notification timeout */
          startup_data = _thunar_vfs_slice_new (TvsnStartupData);
          startup_data->sn_launcher = sn_launcher;
          startup_data->timeout_id = g_timeout_add_full (G_PRIORITY_LOW, TVSN_STARTUP_TIMEOUT, tvsn_startup_timeout,
                                                         startup_data, tvsn_startup_timeout_destroy);
          startup_data->watch_id = g_child_watch_add_full (G_PRIORITY_LOW, pid, tvsn_startup_watch, startup_data, NULL);
          startup_data->pid = pid;
        }
    }
  else if (G_LIKELY (succeed))
    {
      /* make sure we don't leave zombies (see bug #2983 for details) */
      g_child_watch_add_full (G_PRIORITY_LOW, pid, (GChildWatchFunc) g_spawn_close_pid, NULL, NULL);

    }

  /* release the sn display */
  if (G_LIKELY (sn_display != NULL))
    sn_display_unref (sn_display);
#endif

  /* release the child environment */
  g_strfreev (cenvp);

  return succeed;
}



/**
 * thunar_vfs_exec_sync:
 * @command_fmt : the command to execute (can be a printf
 *                format string).
 * @error       : return location for errors or %NULL.
 * @...         : additional parameters to fill into 
 *                @command_fmt.
 *
 * Executes the given @command_fmt and returns %TRUE if the
 * command terminated successfully. Else, the @error is set
 * to the standard error output.
 *
 * Return value: %TRUE if the @command_line was executed
 *               successfully, %FALSE if @error is set.
 **/
gboolean
thunar_vfs_exec_sync (const gchar *command_fmt,
                      GError     **error,
                      ...)
{
  gboolean result;
  va_list  args;
  gchar   *standard_error;
  gchar   *command_line;
  gint     exit_status;

  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (command_fmt != NULL, FALSE);

  /* determine the command line */
  va_start (args, error);
  command_line = g_strdup_vprintf (command_fmt, args);
  va_end (args);

  /* try to execute the command line */
  result = g_spawn_command_line_sync (command_line, NULL, &standard_error, &exit_status, error);
  if (G_UNLIKELY (result))
    {
      /* check if the command failed */
      if (G_UNLIKELY (exit_status != 0))
        {
          /* drop additional whitespace from the stderr output */
          g_strstrip (standard_error);

          /* strip all trailing dots from the stderr output */
          while (*standard_error != '\0' && standard_error[strlen (standard_error) - 1] == '.')
            standard_error[strlen (standard_error) - 1] = '\0';

          /* generate an error from the stderr output */
          if (G_LIKELY (*standard_error != '\0'))
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, "%s", standard_error);
          else
            g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("Unknown error"));

          /* and yes, we failed */
          result = FALSE;
        }

      /* release the stderr output */
      g_free (standard_error);
    }

  /* cleanup */
  g_free (command_line);

  return result;
}



#define __THUNAR_VFS_EXEC_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
