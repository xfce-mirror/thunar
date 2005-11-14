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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar-vfs/thunar-vfs-exec.h>
#include <thunar-vfs/thunar-vfs-path.h>
#include <thunar-vfs/thunar-vfs-alias.h>

#ifdef GDK_WINDOWING_X11
#include <X11/Xatom.h>
#include <gdk/gdkx.h>
#endif



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
  const ThunarVfsPath *parent;
  const gchar         *p;
  gboolean             result = FALSE;
  GString             *command_line = g_string_new (NULL);
  GList               *lp;
  gchar                buffer[THUNAR_VFS_PATH_MAXURILEN];
  gchar               *quoted;

  /* prepend terminal command if required */
  if (G_UNLIKELY (terminal))
    {
      g_string_append (command_line, "Terminal ");
      if (G_LIKELY (name != NULL))
        {
          quoted = g_shell_quote (name);
          g_string_append (command_line, "-T ");
          g_string_append (command_line, quoted);
          g_string_append_c (command_line, ' ');
          g_free (quoted);
        }
      g_string_append (command_line, "-x ");
    }

  for (p = exec; *p != '\0'; ++p)
    {
      if (p[0] == '%' && p[1] != '\0')
        {
          switch (*++p)
            {
            case 'f':
              if (G_LIKELY (path_list != NULL))
                {
                  if (thunar_vfs_path_to_string (path_list->data, buffer, sizeof (buffer), error) < 0)
                    goto done;

                  quoted = g_shell_quote (buffer);
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'F':
              for (lp = path_list; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != path_list))
                    g_string_append_c (command_line, ' ');

                  if (thunar_vfs_path_to_string (lp->data, buffer, sizeof (buffer), error) < 0)
                    goto done;

                  quoted = g_shell_quote (buffer);
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'u':
              if (G_LIKELY (path_list != NULL))
                {
                  if (thunar_vfs_path_to_uri (path_list->data, buffer, sizeof (buffer), error) < 0)
                    goto done;

                  quoted = g_shell_quote (buffer);
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'U':
              for (lp = path_list; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != path_list))
                    g_string_append_c (command_line, ' ');

                  if (thunar_vfs_path_to_uri (lp->data, buffer, sizeof (buffer), error) < 0)
                    goto done;

                  quoted = g_shell_quote (buffer);
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'd':
              if (G_LIKELY (path_list != NULL))
                {
                  parent = thunar_vfs_path_get_parent (path_list->data);
                  if (G_LIKELY (parent != NULL))
                    {
                      if (thunar_vfs_path_to_string (parent, buffer, sizeof (buffer), error) < 0)
                        goto done;

                      quoted = g_shell_quote (buffer);
                      g_string_append (command_line, quoted);
                      g_free (quoted);
                    }
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

                      if (thunar_vfs_path_to_string (parent, buffer, sizeof (buffer), error) < 0)
                        goto done;

                      quoted = g_shell_quote (buffer);
                      g_string_append (command_line, quoted);
                      g_free (quoted);
                    }
                }
              break;

            case 'n':
              if (G_LIKELY (path_list != NULL))
                {
                  quoted = g_shell_quote (thunar_vfs_path_get_name (path_list->data));
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'N':
              for (lp = path_list; lp != NULL; lp = lp->next)
                {
                  if (G_LIKELY (lp != path_list))
                    g_string_append_c (command_line, ' ');
                  quoted = g_shell_quote (thunar_vfs_path_get_name (lp->data));
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'i':
              if (G_LIKELY (icon != NULL))
                {
                  quoted = g_shell_quote (icon);
                  g_string_append (command_line, "--icon ");
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'c':
              if (G_LIKELY (name != NULL))
                {
                  quoted = g_shell_quote (name);
                  g_string_append (command_line, quoted);
                  g_free (quoted);
                }
              break;

            case 'k':
              if (G_LIKELY (path != NULL))
                {
                  quoted = g_shell_quote (path);
                  g_string_append (command_line, path);
                  g_free (quoted);
                }
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

static gboolean
tvsn_startup_timeout (gpointer data)
{
  SnLauncherContext *sn_launcher = data;
  GTimeVal           now;
  gdouble            elapsed;
  glong              tv_sec;
  glong              tv_usec;

  GDK_THREADS_ENTER ();

  /* determine the amount of elapsed time */
  g_get_current_time (&now);
  sn_launcher_context_get_last_active_time (sn_launcher, &tv_sec, &tv_usec);
  elapsed = (((gdouble) now.tv_sec - tv_sec) * G_USEC_PER_SEC + (now.tv_usec - tv_usec)) / 1000.0;

  /* check if the timeout was reached */
  if (elapsed >= TVSN_STARTUP_TIMEOUT)
    {
      /* abort the startup notification */
      sn_launcher_context_complete (sn_launcher);
      sn_launcher_context_unref (sn_launcher);
    }

  GDK_THREADS_LEAVE ();

  /* keep the startup timeout if not elapsed */
  return (elapsed < TVSN_STARTUP_TIMEOUT);
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
                          (unsigned char **) &prop_ret) != Success)
    {
      if (XGetWindowProperty (GDK_WINDOW_XDISPLAY (root), GDK_WINDOW_XWINDOW (root),
                              _WIN_WORKSPACE, 0, 32, False, XA_CARDINAL,
                              &type_ret, &format_ret, &nitems_ret, &bytes_after_ret,
                              (unsigned char **) &prop_ret) != Success)
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
                           GError     **error)
{
#ifdef HAVE_LIBSTARTUP_NOTIFICATION
  SnLauncherContext *sn_launcher = NULL;
  extern char      **environ;
  SnDisplay         *sn_display = NULL;
  gint               sn_workspace;
  gint               n, m;
#endif
  gboolean           succeed;
  gchar            **sn_envp = envp;

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
              sn_launcher_context_initiate (sn_launcher, g_get_prgname (), argv[0], CurrentTime);

              /* setup the child environment */
              if (G_LIKELY (envp == NULL))
                envp = (gchar **) environ;
              for (n = 0; envp[n] != NULL; ++n)
                ;
              sn_envp = g_new (gchar *, n + 2);
              for (n = m = 0; envp[n] != NULL; ++n)
                if (strncmp (envp[n], "DESKTOP_STARTUP_ID", 18) != 0)
                  sn_envp[m++] = g_strdup (envp[n]);
              sn_envp[m++] = g_strconcat ("DESKTOP_STARTUP_ID=", sn_launcher_context_get_startup_id (sn_launcher), NULL);
              sn_envp[m] = NULL;
            }
        }
    }
#endif

  /* try to spawn the new process */
  succeed = gdk_spawn_on_screen (screen, working_directory, argv, sn_envp, flags, NULL, NULL, NULL, error);

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
          g_timeout_add (TVSN_STARTUP_TIMEOUT, tvsn_startup_timeout, sn_launcher);
        }
    }

  /* release the sn display */
  if (G_LIKELY (sn_display != NULL))
    sn_display_unref (sn_display);
#endif

  /* release the environment */
  if (G_UNLIKELY (sn_envp != envp))
    g_strfreev (sn_envp);

  return succeed;
}


