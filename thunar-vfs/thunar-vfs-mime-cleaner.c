/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, write to the Free Software Foundation, Inc., 59 Temple
 * Place, Suite 330, Boston, MA  02111-1307  USA
 */

/********************************************************************************
 * WHAT IS THIS?                                                                *
 *                                                                              *
 * thunar-vfs-mime-cleaner is a small utility program, used to cleanup the      *
 * $XDG_DATA_HOME/applications/ folder, in which both Thunar and Nautilus       *
 * create <app>-usercreated.desktop files when the user enters a custom         *
 * command to open a file. These .desktop files aren't removed by the package   *
 * manager when the associated applications are removed, so we do this manu-    *
 * ally using this simple tool.                                                 *
 ********************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <glib/gstdio.h>
#include <libxfce4util/libxfce4util.h>



static gboolean
check_exec (const gchar *exec)
{
  gboolean result = TRUE;
  gchar   *command;
  gchar  **argv;

  /* exec = NULL is invalid */
  if (G_UNLIKELY (exec == NULL))
    return FALSE;

  /* try to parse the "Exec" line */
  if (g_shell_parse_argv (exec, NULL, &argv, NULL))
    {
      /* check if argv[0] is an absolute path to an existing file */
      result = (g_path_is_absolute (argv[0]) && g_file_test (argv[0], G_FILE_TEST_EXISTS));

      /* otherwise, argv[0] must be a program in $PATH */
      if (G_LIKELY (!result))
        {
          /* search in $PATH for argv[0] */
          command = g_find_program_in_path (argv[0]);
          result = (command != NULL);
          g_free (command);
        }

      /* release the argv */
      g_strfreev (argv);
    }

  return result;
}



int
main (int argc, char **argv)
{
  const gchar *exec;
  const gchar *name;
  const gchar *mt1;
  const gchar *mt2;
  GHashTable  *usercreated;
  regex_t      regex;
  XfceRc      *other_rc;
  XfceRc      *rc;
  GError      *error = NULL;
  gchar       *directory;
  gchar       *command;
  gchar       *mt;
  GList       *obsolete = NULL;
  GList       *lp;
  GDir        *dp;

  /* determine the $XDG_DATA_HOME/applications directory */
  directory = xfce_resource_save_location (XFCE_RESOURCE_DATA, "applications/", TRUE);
  if (G_UNLIKELY (directory == NULL))
    {
      fprintf (stderr, "thunar-vfs-mime-cleaner: Failed to determine the user's applications directory.\n");
      return EXIT_FAILURE;
    }

  /* verify that we can enter that directory */
  if (G_UNLIKELY (chdir (directory) < 0))
    {
      fprintf (stderr, "thunar-vfs-mime-cleaner: Failed to enter directory %s: %s\n", directory, g_strerror (errno));
      return EXIT_FAILURE;
    }

  /* compile the regular expression to match usercreated .desktop files */
  if (regcomp (&regex, "^.*-usercreated(-[0-9]+)?\\.desktop", REG_EXTENDED) < 0)
    {
      fprintf (stderr, "thunar-vfs-mime-cleaner: Failed to compile regular expression: %s\n", g_strerror (errno));
      return EXIT_FAILURE;
    }

  /* try to open the new current directory */
  dp = g_dir_open (".", 0, &error);
  if (G_UNLIKELY (dp == NULL))
    {
      fprintf (stderr, "thunar-vfs-mime-cleaner: Failed to open directory %s: %s\n", directory, error->message);
      g_error_free (error);
      return EXIT_FAILURE;
    }

  /* allocate a hash table used to combine the <app>-usercreated.desktop files */
  usercreated = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) xfce_rc_close);

  /* process the directory contents, collecting obsolete files */
  for (;;)
    {
      /* determine the next file name */
      name = g_dir_read_name (dp);
      if (G_UNLIKELY (name == NULL))
        break;

      /* check if the file name matches */
      if (regexec (&regex, name, 0, NULL, 0) != 0)
        continue;

      /* try to open that file */
      rc = xfce_rc_simple_open (name, FALSE);
      if (G_UNLIKELY (rc == NULL))
        continue;

      /* we care only for the [Desktop Entry] group */
      xfce_rc_set_group (rc, "Desktop Entry");

      /* determine the "Exec" value */
      exec = xfce_rc_read_entry_untranslated (rc, "Exec", NULL);
      if (G_LIKELY (!check_exec (exec)))
        {
          /* this one is obsolete */
          obsolete = g_list_append (obsolete, g_strdup (name));
        }
      else if (G_LIKELY (exec != NULL))
        {
          /* check if already present in the usercreated hash table */
          other_rc = g_hash_table_lookup (usercreated, exec);
          if (G_UNLIKELY (other_rc != NULL))
            {
              /* two files with the same "Exec" line, we can combine them by merging the MimeType */
              mt1 = xfce_rc_read_entry_untranslated (other_rc, "MimeType", NULL);
              mt2 = xfce_rc_read_entry_untranslated (rc, "MimeType", NULL);

              /* combine the MimeTypes to a single list */
              if (G_LIKELY (mt1 != NULL && g_str_has_suffix (mt1, ";")))
                mt = g_strconcat (mt1, mt2, NULL);
              else if (G_LIKELY (mt1 != NULL))
                mt = g_strconcat (mt1, ";", mt2, NULL);
              else
                mt = g_strdup (mt2);

              /* store the new entry (if any) to the other file */
              if (G_LIKELY (mt != NULL))
                {
                  xfce_rc_write_entry (other_rc, "MimeType", mt);
                  xfce_rc_flush (other_rc);
                  g_free (mt);
                }

              /* no need to keep the new file around, now that we merged the stuff */
              if (g_unlink (name) < 0 && errno != ENOENT)
                fprintf (stderr, "thunar-vfs-mime-cleaner: Failed to unlink %s: %s\n", name, g_strerror (errno));
            }
          else
            {
              /* just add this one to the usercreated hash table */
              g_hash_table_insert (usercreated, g_strdup (exec), rc);
              continue;
            }
        }

      /* close the file */
      xfce_rc_close (rc);
    }

  /* release the regular expression */
  regfree (&regex);

  /* close the directory handle */
  g_dir_close (dp);

  /* check if we have any obsolete files */
  if (G_UNLIKELY (obsolete != NULL))
    {
      /* remove all obsolete files from the directory */
      for (lp = obsolete; lp != NULL; lp = lp->next)
        {
          /* try to remove the file */
          if (G_UNLIKELY (g_unlink (lp->data) < 0 && errno != ENOENT))
            {
              fprintf (stderr, "thunar-vfs-mime-cleaner: Failed to unlink %s: %s\n", (gchar *) lp->data, g_strerror (errno));
              return EXIT_FAILURE;
            }
          g_free (lp->data);
        }
      g_list_free (obsolete);

      /* run update-desktop-database on the applications folder */
      command = g_strdup_printf ("update-desktop-database %s", directory);
      if (!g_spawn_command_line_sync (command, NULL, NULL, NULL, &error))
        {
          fprintf (stderr, "thunar-vfs-mime-cleaner: Failed to execute %s: %s\n", command, error->message);
          g_error_free (error);
          return EXIT_FAILURE;
        }
      g_free (command);
    }

  /* release the directory */
  g_free (directory);

  return EXIT_SUCCESS;
}
