/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

/********************************************************************************
 * WHAT IS THIS?                                                                *
 *                                                                              *
 * thunar-vfs-update-thumbnailers-cache-1 is a small utility that collects the  *
 * available thumbnailers on the system and generates a thumbnailers.cache file *
 * in the users home directory (in $XDG_CACHE_HOME/Thunar/), which provides a   *
 * mapping between mime types and the thumbnailers that can handle these mime   *
 * types.                                                                       *
 *                                                                              *
 * See the file docs/ThumbnailersCacheFormat.txt for a description of the file  *
 * format and thumbnailing in thunar-vfs.                                       *
 ********************************************************************************/

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_MMAN_H
#include <sys/mman.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <gdk-pixbuf/gdk-pixbuf.h>

#include <libxfce4util/libxfce4util.h>

#ifdef HAVE_GCONF
#include <gconf/gconf-client.h>
#endif

/* use g_access(), g_open(), g_rename() and g_unlink() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_access(filename, mode) (access ((filename), (mode)))
#define g_open(filename, flags, mode) (open ((filename), (flags), (mode)))
#define g_rename(oldfilename, newfilename) (rename ((oldfilename), (newfilename)))
#define g_unlink(filename) (unlink ((filename)))
#endif



#define CACHE_VERSION_MAJOR (1)
#define CACHE_VERSION_MINOR (0)



static gint
mime_type_compare (gconstpointer a,
                   gconstpointer b)
{
  gint a_len;
  gint b_len;

  a_len = strlen (a);
  b_len = strlen (b);

  if (a_len == b_len)
    return strcmp (a, b);
  else
    return a_len - b_len;
}



static void
thumbnailers_serialize_foreach (gpointer key,
                                gpointer value,
                                gpointer user_data)
{
  /* insert the key (-> mime type) into the return list */
  *((GSList **) user_data) = g_slist_insert_sorted (*((GSList **) user_data), key, mime_type_compare);
}



static guchar*
thumbnailers_serialize (GHashTable *thumbnailers,
                        guint      *size_return)
{
  GHashTable *thumbnailer_scripts_offsets;
  guint       thumbnailer_scripts_size;
  GSList     *mime_types = NULL;
  guint       mime_type_strings_size;
  GSList     *lp;
  guchar     *serialized;
  gchar      *script;
  guint       offset_thumbnailers_strings;
  guint       offset_thumbnailers_table;
  guint       offset_mime_type_strings;
  guint       offset_mime_type_table;
  guint       offset_strings;
  guint       offset_table;
  guint       offset;
  guint       length;
  guint       n_mime_types;
  guint       size;

  /* collect all keys (-> mime types) from the hash table */
  g_hash_table_foreach (thumbnailers, thumbnailers_serialize_foreach, &mime_types);

  /* the mime type strings */
  mime_type_strings_size = 0;
  for (lp = mime_types, n_mime_types = 0; lp != NULL; lp = lp->next, ++n_mime_types)
    {
      /* mime type strings are aligned at 4 byte boundaries */
      mime_type_strings_size += ((strlen (lp->data) + 1 + 3) / 4) * 4;
    }

  /* the thumbnailer scripts */
  thumbnailer_scripts_size = 0;
  thumbnailer_scripts_offsets = g_hash_table_new (g_str_hash, g_str_equal);
  for (lp = mime_types; lp != NULL; lp = lp->next)
    {
      /* do not add thumbnailer scripts twice */
      script = g_hash_table_lookup (thumbnailers, lp->data);
      if (!g_hash_table_lookup_extended (thumbnailer_scripts_offsets, script, NULL, NULL))
        {
          /* thumbnailer strings are aligned at 4 byte boundaries */
          offset = thumbnailer_scripts_size;
          thumbnailer_scripts_size += ((strlen (script) + 1 + 3) / 4) * 4;
          g_hash_table_insert (thumbnailer_scripts_offsets, script, GUINT_TO_POINTER (offset));
        }
    }

  /* overall size: header + mime type table + mime type strings + thumbnailer table + thumbnailer strings */
  size = (4 + 4 + 4 + 4) + (n_mime_types * 8) + mime_type_strings_size + (n_mime_types * 4) + thumbnailer_scripts_size;

  /* determine the various offsets */
  offset_mime_type_table = 4 + 4 + 4 + 4;
  offset_mime_type_strings = offset_mime_type_table + (n_mime_types * 8);
  offset_thumbnailers_table = offset_mime_type_strings + mime_type_strings_size;
  offset_thumbnailers_strings = offset_thumbnailers_table + (n_mime_types * 4);

  /* allocate memory and setup the header */
  serialized = g_malloc0 (size);
  *((guint32 *) (serialized +  0)) = GUINT32_TO_BE ((guint32) CACHE_VERSION_MAJOR);
  *((guint32 *) (serialized +  4)) = GUINT32_TO_BE ((guint32) CACHE_VERSION_MINOR);
  *((guint32 *) (serialized +  8)) = GUINT32_TO_BE ((guint32) n_mime_types);
  *((guint32 *) (serialized + 12)) = GUINT32_TO_BE ((guint32) offset_thumbnailers_table);

  /* write the mime type table */
  for (lp = mime_types, offset_strings = offset_mime_type_strings, offset_table = offset_mime_type_table; lp != NULL; lp = lp->next)
    {
      /* write the mime type table entry */
      length = strlen (lp->data);
      *((guint32 *) (serialized + offset_table + 0)) = GUINT32_TO_BE ((guint32) length);
      *((guint32 *) (serialized + offset_table + 4)) = GUINT32_TO_BE ((guint32) offset_strings);
      offset_table += 8;

      /* write the mime type string */
      memcpy (serialized + offset_strings, lp->data, length + 1);
      offset_strings += ((length + 1 + 3) / 4) * 4;
    }

  /* write the thumbnailer table */
  for (lp = mime_types, offset_table = offset_thumbnailers_table; lp != NULL; lp = lp->next)
    {
      /* determine the script for the mime type */
      script = g_hash_table_lookup (thumbnailers, lp->data);

      /* lookup the offset for the thumbnailer string */
      offset_strings = offset_thumbnailers_strings + GPOINTER_TO_UINT (g_hash_table_lookup (thumbnailer_scripts_offsets, script));

      /* write the thumbnailer table entry */
      *((guint32 *) (serialized + offset_table)) = GUINT32_TO_BE ((guint32) offset_strings);
      offset_table += 4;

      /* write the thumbnailer scripts string */
      memcpy (serialized + offset_strings, script, strlen (script));
    }

  *size_return = size;
  return serialized;
}



static void
thumbnailers_load_gnome (GHashTable *thumbnailers)
{
#ifdef HAVE_GCONF
  /* load the available thumbnailers from GConf */
  GConfClient *client;
  GSList      *formats;
  GSList      *lp;
  gchar       *mime_type;
  gchar       *script;
  gchar       *format;
  gchar        key[1024];
  guint        n;

  /* grab a reference on the default GConf client */
  client = gconf_client_get_default ();

  /* determine the MIME types supported by the GNOME thumbnailers */
  formats = gconf_client_all_dirs (client, "/desktop/gnome/thumbnailers", NULL);
  for (lp = formats; lp != NULL; lp = lp->next)
    {
      /* check if the given thumbnailer is enabled */
      format = (gchar *) lp->data;
      g_snprintf (key, sizeof (key), "%s/enable", format);
      if (gconf_client_get_bool (client, key, NULL))
        {
          /* determine the command */
          g_snprintf (key, sizeof (key), "%s/command", format);
          script = gconf_client_get_string (client, key, NULL);
          if (G_LIKELY (script != NULL))
            {
              mime_type = strrchr (format, '/');
              if (G_LIKELY (mime_type != NULL))
                {
                  /* skip past slash */
                  ++mime_type;

                  /* convert '@' to slash in the mime_type */
                  for (n = 0; mime_type[n] != '\0'; ++n)
                    if (G_UNLIKELY (mime_type[n] == '@'))
                      mime_type[n] = '/';

                  /* add the script to the thumbnailers list */
                  g_hash_table_insert (thumbnailers, mime_type, script);
                }
            }
        }
    }
#else
  /* load some well known GNOME thumbnailers */
  static const struct
  {
    const gchar *binary;
    const gchar *parameters;
    const gchar *mime_types;
  } WELL_KNOWN_THUMBNAILERS[] =
  {
    { /* Evince */
      "evince-thumbnailer", "-s %s %u %o",
      "application/pdf;"
      "application/x-dvi",
    },
    { /* Totem */
      "totem-video-thumbnailer", "-s %s %u %o",
      "application/ogg;"
      "application/vnd.rn-realmedia;"
      "application/x-extension-m4a;"
      "application/x-extension-mp4;"
      "application/x-matroska;"
      "application/x-ogg;"
      "application/x-shockwave-flash;"
      "application/x-shorten;"
      "audio/x-pn-realaudio;"
      "image/vnd.rn-realpix;"
      "misc/ultravox;"
      "video/3gpp;"
      "video/dv;"
      "video/mp4;"
      "video/mpeg;"
      "video/msvideo;"
      "video/quicktime;"
      "video/vnd.rn-realvideo;"
      "video/x-anim;"
      "video/x-avi;"
      "video/x-flc;"
      "video/x-fli;"
      "video/x-mpeg;"
      "video/x-ms-asf;"
      "video/x-ms-wmv;"
      "video/x-msvideo;"
      "video/x-nsv",
    },
    { /* Gsf Thumbnailer */
      "gsf-office-thumbnailer", "-i %i -o %o -s %s",
      "application/vnd.ms-excel;"
      "application/vnd.ms-powerpoint;"
      "application/vnd.oasis.opendocument.presentation;"
      "application/vnd.oasis.opendocument.presentation-template;"
      "application/vnd.oasis.opendocument.spreadsheet;"
      "application/vnd.oasis.opendocument.spreadsheet-template;"
      "application/vnd.oasis.opendocument.text;"
      "application/vnd.oasis.opendocument.text-template",
    },
    { /* Font Thumbnailer */
      "gnome-thumbnail-font", "%u %o",
      "application/x-font-otf;"
      "application/x-font-pcf;"
      "application/x-font-ttf;"
      "application/x-font-type1",
    },
    { /* Theme Thumbnailer */
      "gnome-theme-thumbnailer", "%u %o",
      "application/x-gnome-theme;"
      "application/x-gnome-theme-installed",
    },
  };

  gchar **mime_types;
  gchar  *script;
  gchar  *path;
  guint   m, n;

  /* test all of the well-known thumbnailers */
  for (n = 0; n < G_N_ELEMENTS (WELL_KNOWN_THUMBNAILERS); ++n)
    {
      /* check if the thumbnailer is installed */
      path = g_find_program_in_path (WELL_KNOWN_THUMBNAILERS[n].binary);
      if (G_UNLIKELY (path == NULL))
        continue;

      /* generate the script command */
      script = g_strconcat (WELL_KNOWN_THUMBNAILERS[n].binary, " ", WELL_KNOWN_THUMBNAILERS[n].parameters, NULL);

      /* add the script for all specified mime types */
      mime_types = g_strsplit (WELL_KNOWN_THUMBNAILERS[n].mime_types, ";", -1);
      for (m = 0; mime_types[m] != NULL; ++m)
        g_hash_table_insert (thumbnailers, mime_types[m], script);
    }
#endif
}



static void
thumbnailers_load_pixbuf (GHashTable *thumbnailers)
{
  GSList *formats;
  GSList *lp;
  gchar  **mime_types;
  guint    n;

  /* determine the formats supported by gdk-pixbuf */
  formats = gdk_pixbuf_get_formats ();
  for (lp = formats; lp != NULL; lp = lp->next)
    {
      /* determine the mime types for the format */
      mime_types = gdk_pixbuf_format_get_mime_types (lp->data);
      for (n = 0; mime_types[n] != NULL; ++n)
        {
          /* set our thumbnailer for all those mime types */
          g_hash_table_insert (thumbnailers, mime_types[n], LIBEXECDIR G_DIR_SEPARATOR_S "thunar-vfs-pixbuf-thumbnailer-1 %s %i %o");
        }
    }
}



static void
thumbnailers_load_custom (GHashTable *thumbnailers)
{
  const gchar *exec;
  const gchar *type;
  XfceRc      *rc;
  gchar      **mime_types;
  gchar      **specs;
  gchar       *path;
  guint        n, m;

  /* load available custom thumbnailers from $XDG_DATA_DIRS/thumbnailers/ */
  specs = xfce_resource_match (XFCE_RESOURCE_DATA, "thumbnailers/*.desktop", TRUE);
  for (n = 0; specs[n] != NULL; ++n)
    {
      /* try to load the .desktop file */
      rc = xfce_rc_config_open (XFCE_RESOURCE_DATA, specs[n], TRUE);
      if (G_UNLIKELY (rc == NULL))
        continue;

      /* we only care for the [Desktop Entry] group */
      xfce_rc_set_group (rc, "Desktop Entry");

      /* verify that we have an X-Thumbnailer here */
      type = xfce_rc_read_entry_untranslated (rc, "Type", NULL);
      if (G_UNLIKELY (type == NULL || strcmp (type, "X-Thumbnailer") != 0))
        continue;

      /* check if the thumbnailer specifies a TryExec field */
      exec = xfce_rc_read_entry_untranslated (rc, "TryExec", NULL);
      if (G_UNLIKELY (exec != NULL))
        {
          /* check if the binary exists and is executable */
          path = g_path_is_absolute (exec) ? (gchar *) exec : g_find_program_in_path (exec);
          if (G_UNLIKELY (path == NULL || g_access (path, X_OK) < 0))
            continue;
        }

      /* verify that the thumbnailer specifies an X-Thumbnailer-Exec field */
      exec = xfce_rc_read_entry_untranslated (rc, "X-Thumbnailer-Exec", NULL);
      if (G_UNLIKELY (exec == NULL))
        continue;

      /* determine the mime types for this thumbnailer */
      mime_types = xfce_rc_read_list_entry (rc, "MimeType", ";");
      if (G_UNLIKELY (mime_types == NULL))
        continue;
      
      /* process all specified mime types */
      for (m = 0; mime_types[m] != NULL; ++m)
        {
          /* check if we have a valid mime type here */
          if (strlen (mime_types[m]) > 0 && strstr (mime_types[m], "/") != NULL)
            {
              /* set our thumbnailer for all those mime types (don't need to 
               * duplicate the exec string, because we leave the RC file open).
               */
              g_hash_table_insert (thumbnailers, mime_types[m], (gchar *) exec);
            }
        }
    }
}



static gboolean
thumbnailers_needs_update (const gchar  *filename,
                           gconstpointer serialized,
                           guint         serialized_size)
{
  struct stat statb;
  gboolean    needs_update;
  gssize      m;
  gchar      *contents;
  gsize       n;
  gint        fd;

  /* try to open the file */
  fd = g_open (filename, O_RDONLY, 0000);
  if (G_UNLIKELY (fd < 0))
    return TRUE;

  /* try to stat the file */
  if (fstat (fd, &statb) < 0 || statb.st_size != serialized_size)
    {
      close (fd);
      return TRUE;
    }

  /* try to mmap the file */
#ifdef HAVE_MMAP
  contents = mmap (NULL, statb.st_size, PROT_READ, MAP_SHARED, fd, 0);
  if (G_LIKELY (contents != MAP_FAILED))
    {
      /* check if the contents is the same */
      needs_update = (memcmp (contents, serialized, statb.st_size) != 0);

      /* unmap the file */
      munmap (contents, statb.st_size);
    }
  else
#endif
    {
      /* read the file */
      contents = g_malloc (statb.st_size);
      for (n = 0; n < statb.st_size; n += m)
        {
          /* read the next chunk */
          m = read (fd, contents + n, statb.st_size - n);
          if (G_UNLIKELY (m <= 0))
            {
              close (fd);
              return TRUE;
            }
        }

      /* compare the contents */
      needs_update = (memcmp (contents, serialized, statb.st_size) == 0);
    }

  /* close the file */
  close (fd);

  return needs_update;
}



int
main (int argc, char **argv)
{
  GHashTable *thumbnailers;
  gchar      *filename;
  gchar      *tmp_name;
  guchar     *serialized;
  gssize      m;
  guint       serialized_size;
  gsize       n;
  gint        fd;

  /* be sure to initialize the GType system */
  g_type_init ();

  /* allocate the hash table for the thumbnailers */
  thumbnailers = g_hash_table_new (g_str_hash, g_str_equal);

  /* load the available GNOME thumbnailers */
  thumbnailers_load_gnome (thumbnailers);

  /* load the available gdk-pixbuf thumbnailers */
  thumbnailers_load_pixbuf (thumbnailers);

  /* load the available custom thumbnailers */
  thumbnailers_load_custom (thumbnailers);

  /* serialize the loaded thumbnailers */
  serialized = thumbnailers_serialize (thumbnailers, &serialized_size);

  /* determine the cache file */
  filename = xfce_resource_save_location (XFCE_RESOURCE_CACHE, "Thunar/thumbnailers.cache", TRUE);
  if (G_UNLIKELY (filename == NULL))
    return EXIT_FAILURE;

  /* check if we need to update the thumbnailers.cache */
  if (thumbnailers_needs_update (filename, serialized, serialized_size))
    {
      tmp_name = g_strconcat (filename, ".XXXXXX", NULL);
      fd = g_mkstemp (tmp_name);
      if (G_UNLIKELY (fd < 0))
        return EXIT_FAILURE;

      /* write the content */
      for (n = 0; n < serialized_size; n += m)
        {
          /* write the next chunk */
          m = write (fd, serialized + n, serialized_size - n);
          if (G_UNLIKELY (m < 0))
            {
err0:         g_unlink (tmp_name);
              return EXIT_FAILURE;
            }
        }

      /* make sure the file isn't writable */
      fchmod (fd, S_IRUSR);

      /* close the temp file */
      close (fd);

      /* try to rename to the cache file */
      if (g_rename (tmp_name, filename) < 0)
        goto err0;

      /* we did it, new cache contents -> 33 */
      return 33;
    }

  /* not updated -> 0 */
  return EXIT_SUCCESS;
}



