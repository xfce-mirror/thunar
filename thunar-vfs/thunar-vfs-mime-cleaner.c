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
 *                                                                              *
 * In addition, the defaults.list file will be cleaned and references to dead   *
 * desktop-ids will be removed from the list.                                   *
 ********************************************************************************/

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
#ifdef HAVE_REGEX_H
#include <regex.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
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



static GHashTable*
load_defaults_list (const gchar *directory)
{
  const gchar *type;
  const gchar *ids;
  GHashTable  *defaults_list;
  gchar      **id_list;
  gchar        line[2048];
  gchar       *path;
  gchar       *lp;
  FILE        *fp;
  gint         n;
  gint         m;

  /* allocate a new hash table for the defaults.list */
  defaults_list = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, (GDestroyNotify) g_strfreev);

  /* determine the path to the defaults.list */
  path = g_build_filename (directory, "defaults.list", NULL);

  /* try to open the defaults.list file */
  fp = fopen (path, "r");
  if (G_LIKELY (fp != NULL))
    {
      /* process the file */
      while (fgets (line, sizeof (line), fp) != NULL)
        {
          /* skip everything that doesn't look like a mime type line */
          for (lp = line; g_ascii_isspace (*lp); ++lp) ;
          if (G_UNLIKELY (*lp < 'a' || *lp > 'z'))
            continue;

          /* lookup the '=' */
          for (type = lp++; *lp != '\0' && *lp != '='; ++lp) ;
          if (G_UNLIKELY (*lp != '='))
            continue;
          *lp++ = '\0';

          /* extract the application id list */
          for (ids = lp; g_ascii_isgraph (*lp); ++lp) ;
          if (G_UNLIKELY (ids == lp))
            continue;
          *lp = '\0';

          /* parse the id list */
          id_list = g_strsplit (ids, ";", -1);
          for (m = n = 0; id_list[m] != NULL; ++m)
            {
              if (G_UNLIKELY (id_list[m][0] == '\0'))
                g_free (id_list[m]);
              else
                id_list[n++] = id_list[m];
            }
          id_list[n] = NULL;

          /* verify that the id list is not empty */
          if (G_UNLIKELY (id_list[0] == NULL))
            {
              g_free (id_list);
              continue;
            }

          /* insert the line into the hash table */
          g_hash_table_insert (defaults_list, g_strdup (type), id_list);
        }

      /* close the file */
      fclose (fp);
    }

  /* cleanup */
  g_free (path);

  return defaults_list;
}



static gboolean
check_desktop_id (const gchar *id)
{
  GKeyFile *key_file;
  gboolean  result;
  gchar    *path;

  /* check if we can load a .desktop file for the given desktop-id */
  key_file = g_key_file_new ();
  path = g_build_filename ("applications", id, NULL);
  result = g_key_file_load_from_data_dirs (key_file, path, NULL, G_KEY_FILE_NONE, NULL);
  g_key_file_free (key_file);
  g_free (path);

  return result;
}



typedef struct
{
  GHashTable *replacements;
  FILE       *fp;
} SaveDescriptor;



static void
save_defaults_list_one (const gchar    *type,
                        gchar         **ids,
                        SaveDescriptor *save_descriptor)
{
  const gchar *replacement_id;
  gint         i, j, k;

  /* perform the required transformations on the id list */
  for (i = j = 0; ids[i] != NULL; ++i)
    {
      /* check if we have a replacement for this id */
      replacement_id = g_hash_table_lookup (save_descriptor->replacements, ids[i]);
      if (G_UNLIKELY (replacement_id != NULL))
        {
          g_free (ids[i]);
          ids[i] = g_strdup (replacement_id);
        }

      /* check if this id was already present on the list */
      for (k = 0; k < j; ++k)
        if (strcmp (ids[k], ids[i]) == 0)
          break;

      /* we don't need to keep it, if it was already present */
      if (G_UNLIKELY (k < j))
        {
          g_free (ids[i]);
        }
      else if (!check_desktop_id (ids[i]))
        {
          /* we also don't keep references to dead desktop-ids around */
          g_free (ids[i]);
        }
      else
        {
          /* keep it around */
          ids[j++] = ids[i];
        }
    }

  /* null-terminate the list */
  ids[j] = NULL;

  /* no need to store an empty list */
  if (G_LIKELY (ids[0] != NULL))
    {
      fprintf (save_descriptor->fp, "%s=%s", type, ids[0]);
      for (i = 1; ids[i] != NULL; ++i)
        fprintf (save_descriptor->fp, ";%s", ids[i]);
      fprintf (save_descriptor->fp, "\n");
    }
}



static void
save_defaults_list (const gchar *directory,
                    GHashTable  *defaults_list,
                    GHashTable  *replacements)
{
  SaveDescriptor save_descriptor;
  gchar         *defaults_list_path;
  gchar         *path;
  FILE          *fp;
  gint           fd;

  /* determine the path to the defaults.list */
  defaults_list_path = g_build_filename (directory, "defaults.list", NULL);

  /* write the default applications list to a temporary file */
  path = g_strdup_printf ("%s.XXXXXX", defaults_list_path);
  fd = g_mkstemp (path);
  if (G_LIKELY (fd >= 0))
    {
      /* wrap the descriptor in a file pointer */
      fp = fdopen (fd, "w");

      /* initialize the save descriptor */
      save_descriptor.replacements = replacements;
      save_descriptor.fp = fp;

      /* write the default application list content */
      fprintf (fp, "[Default Applications]\n");
      g_hash_table_foreach (defaults_list, (GHFunc) save_defaults_list_one, &save_descriptor);
      fclose (fp);

      /* try to atomically rename the file */
      if (G_UNLIKELY (g_rename (path, defaults_list_path) < 0))
        {
          /* tell the user that we failed */
          fprintf (stderr, "thunar-vfs-mime-cleaner: Failed to write defaults.list: %s\n", g_strerror (errno));

          /* be sure to remove the temporary file */
          g_unlink (path);
        }
    }

  /* cleanup */
  g_free (defaults_list_path);
  g_free (path);
}



int
main (int argc, char **argv)
{
  const gchar *other_name;
  const gchar *exec;
  const gchar *name;
  const gchar *mt1;
  const gchar *mt2;
  GHashTable  *defaults_list;
  GHashTable  *replacements;
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

  /* load the defaults.list file */
  defaults_list = load_defaults_list (directory);

  /* allocate a hash table used to combine the <app>-usercreated.desktop files */
  usercreated = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

  /* allocate a hash table for the replacements (old.desktop -> new.desktop) */
  replacements = g_hash_table_new_full (g_str_hash, g_str_equal, g_free, g_free);

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
      rc = xfce_rc_simple_open (name, TRUE);
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
          other_name = g_hash_table_lookup (usercreated, exec);
          other_rc = (other_name != NULL) ? xfce_rc_simple_open (other_name, FALSE) : NULL;
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
                  g_free (mt);
                }

              /* remember the replacement for later */
              g_hash_table_insert (replacements, g_strdup (name), g_strdup (other_name));

              /* no need to keep the new file around, now that we merged the stuff */
              if (g_unlink (name) < 0 && errno != ENOENT)
                fprintf (stderr, "thunar-vfs-mime-cleaner: Failed to unlink %s: %s\n", name, g_strerror (errno));

              /* close the other rc file */
              xfce_rc_close (other_rc);
            }
          else
            {
              /* just add this one to the usercreated hash table */
              g_hash_table_insert (usercreated, g_strdup (exec), g_strdup (name));
            }
        }

      /* close the file */
      xfce_rc_close (rc);
    }

  /* write a defaults.list, replacing merged desktop-ids and dropping dead ones */
  save_defaults_list (directory, defaults_list, replacements);

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

  /* cleanup */
  g_hash_table_destroy (defaults_list);
  g_hash_table_destroy (replacements);
  g_hash_table_destroy (usercreated);
  g_free (directory);

  return EXIT_SUCCESS;
}
