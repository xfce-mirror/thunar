/* $Id$ */
/*-
 * Copyright (c) 2004-2006 Benedikt Meurer <benny@xfce.org>
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
#ifdef HAVE_SYS_WAIT_H
#include <sys/wait.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
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

#include <png.h>

#include <thunar-vfs/thunar-vfs-enum-types.h>
#include <thunar-vfs/thunar-vfs-mime-database.h>
#include <thunar-vfs/thunar-vfs-thumb-jpeg.h>
#include <thunar-vfs/thunar-vfs-thumb-pixbuf.h>
#include <thunar-vfs/thunar-vfs-thumb.h>
#include <thunar-vfs/thunar-vfs-alias.h>

#ifdef HAVE_GCONF
#include <gconf/gconf-client.h>
#endif

/* use g_rename() and g_unlink() on win32 */
#if GLIB_CHECK_VERSION(2,6,0) && defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_rename(oldfilename, newfilename) (rename ((oldfilename), (newfilename)))
#define g_unlink(filename) (unlink ((filename)))
#endif



/* Property identifiers */
enum
{
  PROP_0,
  PROP_SIZE,
};



static void thunar_vfs_thumb_factory_class_init   (ThunarVfsThumbFactoryClass *klass);
static void thunar_vfs_thumb_factory_init         (ThunarVfsThumbFactory      *factory);
static void thunar_vfs_thumb_factory_finalize     (GObject                    *object);
static void thunar_vfs_thumb_factory_get_property (GObject                    *object,
                                                   guint                       prop_id,
                                                   GValue                     *value,
                                                   GParamSpec                 *pspec);
static void thunar_vfs_thumb_factory_set_property (GObject                    *object,
                                                   guint                       prop_id,
                                                   const GValue               *value,
                                                   GParamSpec                 *pspec);



struct _ThunarVfsThumbFactoryClass
{
  GObjectClass __parent__;
};

struct _ThunarVfsThumbFactory
{
  GObject __parent__;

  ThunarVfsMimeDatabase *mime_database;
  ThunarVfsMimeInfo     *mime_image_jpeg;

  GHashTable            *pixbuf_mime_infos;

  gchar                 *base_path;
  gchar                 *fail_path;
  ThunarVfsThumbSize     size;

#ifdef HAVE_GCONF
  GConfClient           *client;
  GHashTable            *scripts;
#endif
};



static GObjectClass *thunar_vfs_thumb_factory_parent_class;



GType
thunar_vfs_thumb_factory_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsThumbFactoryClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_thumb_factory_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsThumbFactory),
        0,
        (GInstanceInitFunc) thunar_vfs_thumb_factory_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarVfsThumbFactory"), &info, 0);
    }

  return type;
}



static void
thunar_vfs_thumb_factory_class_init (ThunarVfsThumbFactoryClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_vfs_thumb_factory_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_thumb_factory_finalize;
  gobject_class->get_property = thunar_vfs_thumb_factory_get_property;
  gobject_class->set_property = thunar_vfs_thumb_factory_set_property;

  /**
   * ThunarVfsThumbFactory::size:
   *
   * The #ThunarVfsThumbSize at which thumbnails should be
   * looked up and saved for this #ThunarVfsThumbFactory.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SIZE,
                                   g_param_spec_enum ("size",
                                                      _("Size"),
                                                      _("The desired thumbnail size"),
                                                      THUNAR_VFS_TYPE_VFS_THUMB_SIZE,
                                                      THUNAR_VFS_THUMB_SIZE_NORMAL,
                                                      G_PARAM_CONSTRUCT_ONLY | EXO_PARAM_READWRITE));
}



static void
thunar_vfs_thumb_factory_init (ThunarVfsThumbFactory *factory)
{
  ThunarVfsMimeInfo *mime_info;
  GSList            *formats;
  GSList            *lp;
  gchar            **mime_types;
  guint              n;

  /* take a reference on the mime database */
  factory->mime_database = thunar_vfs_mime_database_get_default ();

  /* take a reference on the image/jpeg mime info */
  factory->mime_image_jpeg = thunar_vfs_mime_database_get_info (factory->mime_database, "image/jpeg");

  /* pre-allocate the failed path, so we don't need to do that on every method call */
  factory->fail_path = g_strconcat (xfce_get_homedir (),
                                    G_DIR_SEPARATOR_S ".thumbnails"
                                    G_DIR_SEPARATOR_S "fail"
                                    G_DIR_SEPARATOR_S "thunar-vfs"
                                    G_DIR_SEPARATOR_S, NULL);

  /* we default to normal size here, so we don't need to override the constructor */
  factory->base_path = g_strconcat (xfce_get_homedir (), G_DIR_SEPARATOR_S ".thumbnails" G_DIR_SEPARATOR_S "normal" G_DIR_SEPARATOR_S, NULL);
  factory->size = THUNAR_VFS_THUMB_SIZE_NORMAL;

  /* determine the MIME types supported by GdkPixbuf */
  factory->pixbuf_mime_infos = g_hash_table_new_full (g_direct_hash, g_direct_equal, (GDestroyNotify) thunar_vfs_mime_info_unref, NULL);
  formats = gdk_pixbuf_get_formats ();
  for (lp = formats; lp != NULL; lp = lp->next)
    if (!gdk_pixbuf_format_is_disabled (lp->data))
      {
        mime_types = gdk_pixbuf_format_get_mime_types (lp->data);
        for (n = 0; mime_types[n] != NULL; ++n)
          {
            mime_info = thunar_vfs_mime_database_get_info (factory->mime_database, mime_types[n]);
            if (G_LIKELY (mime_info != NULL))
              g_hash_table_replace (factory->pixbuf_mime_infos, mime_info, GUINT_TO_POINTER (1));
          }
        g_strfreev (mime_types);
      }
  g_slist_free (formats);

#ifdef HAVE_GCONF
  /* grab a reference on the default GConf client */
  factory->client = gconf_client_get_default ();

  /* allocate the hash table for the GNOME thumbnailer scripts */
  factory->scripts = g_hash_table_new_full (g_direct_hash, g_direct_equal, (GDestroyNotify) thunar_vfs_mime_info_unref, g_free);

  /* check if the GNOME thumbnailers are enabled */
  if (!gconf_client_get_bool (factory->client, "/desktop/gnome/thumbnailers/disable_all", NULL))
    {
      /* determine the MIME types supported by the GNOME thumbnailers */
      formats = gconf_client_all_dirs (factory->client, "/desktop/gnome/thumbnailers", NULL);
      for (lp = formats; lp != NULL; lp = lp->next)
        {
          gchar *mime_type;
          gchar *script;
          gchar *format = lp->data;
          gchar  key[1024];

          /* check if the given thumbnailer is enabled */
          g_snprintf (key, sizeof (key), "%s/enable", format);
          if (gconf_client_get_bool (factory->client, key, NULL))
            {
              /* determine the command */
              g_snprintf (key, sizeof (key), "%s/command", format);
              script = gconf_client_get_string (factory->client, key, NULL);
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

                      /* determine the mime info for the given mime_type */
                      mime_info = thunar_vfs_mime_database_get_info (factory->mime_database, mime_type);
                      if (G_LIKELY (mime_info != NULL))
                        {
                          g_hash_table_replace (factory->scripts, mime_info, script);
                          script = NULL;
                        }
                    }
                }

              g_free (script);
            }

          g_free (format);
        }

      g_slist_free (formats);
    }
#endif
}



static void
thunar_vfs_thumb_factory_finalize (GObject *object)
{
  ThunarVfsThumbFactory *factory = THUNAR_VFS_THUMB_FACTORY (object);

#ifdef HAVE_GCONF
  /* disconnect from the GConf client */
  g_object_unref (G_OBJECT (factory->client));

  /* release the scripts hash table */
  g_hash_table_destroy (factory->scripts);
#endif

  /* release the image/jpeg mime info */
  thunar_vfs_mime_info_unref (factory->mime_image_jpeg);

  /* release the reference on the mime database */
  g_object_unref (G_OBJECT (factory->mime_database));

  /* destroy the hash table with mime infos supported by GdkPixbuf */
  g_hash_table_destroy (factory->pixbuf_mime_infos);

  /* free the thumbnail paths */
  g_free (factory->base_path);
  g_free (factory->fail_path);

  (*G_OBJECT_CLASS (thunar_vfs_thumb_factory_parent_class)->finalize) (object);
}



static void
thunar_vfs_thumb_factory_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ThunarVfsThumbFactory *factory = THUNAR_VFS_THUMB_FACTORY (object);

  switch (prop_id)
    {
    case PROP_SIZE:
      g_value_set_enum (value, factory->size);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_vfs_thumb_factory_set_property (GObject      *object,
                                       guint         prop_id,
                                       const GValue *value,
                                       GParamSpec   *pspec)
{
  ThunarVfsThumbFactory *factory = THUNAR_VFS_THUMB_FACTORY (object);

  switch (prop_id)
    {
    case PROP_SIZE:
      if (G_UNLIKELY (g_value_get_enum (value) == THUNAR_VFS_THUMB_SIZE_LARGE))
        {
          /* free the previous base path */
          g_free (factory->base_path);

          /* setup the new base path */
          factory->base_path = g_strconcat (xfce_get_homedir (), G_DIR_SEPARATOR_S ".thumbnails" G_DIR_SEPARATOR_S "large" G_DIR_SEPARATOR_S, NULL);
          factory->size = THUNAR_VFS_THUMB_SIZE_LARGE;
        }
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/**
 * thunar_vfs_thumb_factory_new:
 * @size : the desired #ThunarVfsThumbSize.
 *
 * Allocates a new #ThunarVfsThumbFactory, that is able to
 * load and store thumbnails in the given @size.
 *
 * The caller is responsible to free the returned object
 * using g_object_unref() when no longer needed.
 *
 * Return value: the newly allocated #ThunarVfsThumbFactory.
 **/
ThunarVfsThumbFactory*
thunar_vfs_thumb_factory_new (ThunarVfsThumbSize size)
{
  return g_object_new (THUNAR_VFS_TYPE_THUMB_FACTORY, "size", size, NULL);
}



/**
 * thunar_vfs_thumb_factory_lookup:
 * @factory : a #ThunarVfsThumbFactory.
 * @info    : the #ThunarVfsInfo of the original file.
 *
 * Looks up the path to a thumbnail for @info in @factory and returns
 * the absolute path to the thumbnail file if a valid thumbnail is
 * found. Else %NULL is returned.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: the path to a valid thumbnail for @info in @factory
 *               or %NULL if no valid thumbnail was found.
 **/
gchar*
thunar_vfs_thumb_factory_lookup_thumbnail (ThunarVfsThumbFactory *factory,
                                           const ThunarVfsInfo   *info)
{
  gchar  uri[THUNAR_VFS_PATH_MAXURILEN];
  gchar *path;
  gchar *md5;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), NULL);
  g_return_val_if_fail (info != NULL, NULL);

  /* determine the URI for the path */
  if (thunar_vfs_path_to_uri (info->path, uri, sizeof (uri), NULL) < 0)
    return FALSE;

  /* determine the path to the thumbnail for the factory */
  md5 = exo_str_get_md5_str (uri);
  path = g_strconcat (factory->base_path, md5, ".png", NULL);
  g_free (md5);

  /* check if the path contains a valid thumbnail */
  if (thunar_vfs_thumbnail_is_valid (path, uri, info->mtime))
    return path;

  /* no valid thumbnail in the global repository */
  g_free (path);

  return NULL;
}



/**
 * thunar_vfs_thumb_factory_can_thumbnail:
 * @factory : a #ThunarVfsThumbFactory.
 * @info    : the #ThunarVfsInfo of a file.
 *
 * Checks if @factory can generate a thumbnail for
 * the file specified by @info.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: %TRUE if @factory can generate a thumbnail for @info.
 **/
gboolean
thunar_vfs_thumb_factory_can_thumbnail (ThunarVfsThumbFactory *factory,
                                        const ThunarVfsInfo   *info)
{
  ThunarVfsPath *path;
  const gchar   *name;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

  /* we can only handle thumbnails for regular files */
  if (G_UNLIKELY (info->type != THUNAR_VFS_FILE_TYPE_REGULAR))
    return FALSE;

  /* we really don't want to generate a thumbnail for a thumbnail */
  for (path = info->path; path != NULL; path = thunar_vfs_path_get_parent (path))
    {
      name = thunar_vfs_path_get_name (path);
      if (strcmp (name, ".thumbnails") == 0 || strcmp (name, ".thumblocal") == 0)
        return FALSE;
    }

  /* check GdkPixbuf supports the given mime info */
  if (g_hash_table_lookup (factory->pixbuf_mime_infos, info->mime_info))
    return !thunar_vfs_thumb_factory_has_failed_thumbnail (factory, info);

#ifdef HAVE_GCONF
  /* check if we have a GNOME thumbnailer for the given mime info */
  if (g_hash_table_lookup (factory->scripts, info->mime_info) != NULL)
    return !thunar_vfs_thumb_factory_has_failed_thumbnail (factory, info);
#endif

  return FALSE;
}



/**
 * thunar_vfs_thumb_factory_has_failed_thumbnail:
 * @factory : a #ThunarVfsThumbFactory.
 * @info    : the #ThunarVfsInfo of a file.
 *
 * Checks whether we know that @factory won't be able to generate
 * a thumbnail for the file referred to by @info.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: %TRUE if @factory knows that it cannot generate
 *               a thumbnail for @info, else %TRUE.
 **/
gboolean
thunar_vfs_thumb_factory_has_failed_thumbnail (ThunarVfsThumbFactory *factory,
                                               const ThunarVfsInfo   *info)
{
  gchar  uri[THUNAR_VFS_PATH_MAXURILEN];
  gchar  path[4096];
  gchar *md5;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), FALSE);
  g_return_val_if_fail (info != NULL, FALSE);

  /* determine the URI of the info */
  if (thunar_vfs_path_to_uri (info->path, uri, sizeof (uri), NULL) < 0)
    return FALSE;

  /* determine the path to the thumbnail */
  md5 = exo_str_get_md5_str (uri);
  g_snprintf (path, sizeof (path), "%s%s.png", factory->fail_path, md5);
  g_free (md5);

  return thunar_vfs_thumbnail_is_valid (path, uri, info->mtime);
}



#ifdef HAVE_GCONF
static gchar*
gnome_thumbnailer_script_expand (const gchar *script,
                                 const gchar *ipath,
                                 const gchar *opath,
                                 gint         size)
{
  const gchar *p;
  GString     *s;
  gchar       *quoted;
  gchar       *uri;

  s = g_string_new (NULL);
  for (p = script; *p != '\0'; ++p)
    {
      if (G_LIKELY (*p != '%'))
        {
          g_string_append_c (s, *p);
          continue;
        }

      switch (*++p)
        {
        case 'u':
          uri = g_filename_to_uri (ipath, NULL, NULL);
          if (G_LIKELY (uri != NULL))
            {
              quoted = g_shell_quote (uri);
              g_string_append (s, quoted);
              g_free (quoted);
              g_free (uri);
            }
          break;

        case 'i':
          quoted = g_shell_quote (ipath);
          g_string_append (s, quoted);
          g_free (quoted);
          break;

        case 'o':
          quoted = g_shell_quote (opath);
          g_string_append (s, quoted);
          g_free (quoted);
          break;

        case 's':
          g_string_append_printf (s, "%d", size);
          break;

        case '%':
          g_string_append_c (s, '%');
          break;

        case '\0':
          --p;
          break;

        default:
          break;
        }
    }
  return g_string_free (s, FALSE);
}

static GdkPixbuf*
gnome_thumbnailer_script_run (const gchar *script,
                              const gchar *path,
                              gint         size)
{
  GdkPixbuf *pixbuf = NULL;
  gchar     *tmp_path;
  gchar     *command;
  gint       status;
  gint       fd;

  /* create a temporary file for the thumbnailer */
  fd = g_file_open_tmp (".thunar-vfs-thumbnail.XXXXXX", &tmp_path, NULL);
  if (G_UNLIKELY (fd < 0))
    return NULL;

  /* determine the command for the script */
  command = gnome_thumbnailer_script_expand (script, path, tmp_path, size);
  if (G_LIKELY (command != NULL))
    {
      /* run the thumbnailer script and load the generated file */
      if (g_spawn_command_line_sync (command, NULL, NULL, &status, NULL) && WIFEXITED (status) && WEXITSTATUS (status) == 0)
        pixbuf = gdk_pixbuf_new_from_file (tmp_path, NULL);
    }

  /* unlink the temporary file */
  g_unlink (tmp_path);

  /* close the temporary file */
  close (fd);

  /* cleanup */
  g_free (tmp_path);
  g_free (command);

  return pixbuf;
}
#endif



/**
 * thunar_vfs_thumb_factory_generate_thumbnail:
 * @factory : a #ThunarVfsThumbFactory.
 * @info    : the #ThunarVfsInfo of the file for which a thumbnail
 *            should be generated.
 *
 * Tries to generate a thumbnail for the file referred to by @info in
 * @factory.
 *
 * The caller is responsible to free the returned #GdkPixbuf using
 * g_object_unref() when no longer needed.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: the thumbnail for the @uri or %NULL.
 **/
GdkPixbuf*
thunar_vfs_thumb_factory_generate_thumbnail (ThunarVfsThumbFactory *factory,
                                             const ThunarVfsInfo   *info)
{
  GdkPixbuf *pixbuf = NULL;
  GdkPixbuf *scaled;
#ifdef HAVE_GCONF
  gchar     *script;
#endif
  gchar     *path;
  gint       size;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), NULL);
  g_return_val_if_fail (info != NULL, NULL);

  /* determine the pixel size of the thumbnail */
  size = G_LIKELY (factory->size == THUNAR_VFS_THUMB_SIZE_NORMAL) ? 128 : 256;

  /* determine the absolute path to the file */
  path = thunar_vfs_path_dup_string (info->path);

#ifdef HAVE_GCONF
  /* check if we have a GNOME thumbnailer for the given mime info */
  script = g_hash_table_lookup (factory->scripts, info->mime_info);
  if (G_UNLIKELY (script != NULL))
    pixbuf = gnome_thumbnailer_script_run (script, path, size);
#endif

  /* try the fast JPEG thumbnailer */
  if (G_LIKELY (pixbuf == NULL && info->mime_info == factory->mime_image_jpeg))
    pixbuf = thunar_vfs_thumb_jpeg_load (path, size);

  /* fallback to GdkPixbuf based loading */
  if (G_LIKELY (pixbuf == NULL))
    pixbuf = thunar_vfs_thumb_pixbuf_load (path, thunar_vfs_mime_info_get_name (info->mime_info), size);

  /* check if we need to scale the image */
  if (pixbuf != NULL && (gdk_pixbuf_get_width (pixbuf) > size || gdk_pixbuf_get_height (pixbuf) > size))
    {
      scaled = exo_gdk_pixbuf_scale_ratio (pixbuf, size);
      g_object_unref (G_OBJECT (pixbuf));
      pixbuf = scaled;
    }

  /* cleanup */
  g_free (path);

  return pixbuf;
}



/**
 * thunar_vfs_thumb_factory_store_thumbnail:
 * @factory : a #ThunarVfsThumbFactory.
 * @pixbuf  : the thumbnail #GdkPixbuf to store or %NULL
 *            to remember the thumbnail for @uri as failed.
 * @info    : the #ThunarVfsInfo of the original file.
 * @error   : return location for errors or %NULL.
 *
 * Stores @pixbuf as thumbnail for @info in the right place, according
 * to the size set for @factory.
 *
 * If you specify %NULL for @pixbuf, the @factory will remember that
 * the thumbnail generation for @info failed.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: %TRUE if the thumbnail was stored successfully,
 *               else %FALSE.
 **/
gboolean
thunar_vfs_thumb_factory_store_thumbnail (ThunarVfsThumbFactory *factory,
                                          const GdkPixbuf       *pixbuf,
                                          const ThunarVfsInfo   *info,
                                          GError               **error)
{
  const gchar *base_path;
  GdkPixbuf   *thumbnail;
  gboolean     succeed;
  gchar       *dst_path;
  gchar       *tmp_path;
  gchar       *mtime;
  gchar       *size;
  gchar       *md5;
  gchar       *uri;
  gint         tmp_fd;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), FALSE);
  g_return_val_if_fail (pixbuf == NULL || GDK_IS_PIXBUF (pixbuf), FALSE);
  g_return_val_if_fail (info != NULL, FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* check whether we should save a thumbnail or remember failed generation */
  base_path = G_LIKELY (pixbuf != NULL) ? factory->base_path : factory->fail_path;

  /* verify that the target directory exists */
  if (!xfce_mkdirhier (base_path, 0700, error))
    return FALSE;

  /* determine the URI of the file */
  uri = thunar_vfs_path_dup_uri (info->path);

  /* determine the MD5 sum for the URI */
  md5 = exo_str_get_md5_str (uri);

  /* try to open a temporary file to write the thumbnail to */
  tmp_path = g_strconcat (base_path, md5, ".png.XXXXXX", NULL);
  tmp_fd = g_mkstemp (tmp_path);
  if (G_UNLIKELY (tmp_fd < 0))
    {
      g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
      g_free (md5);
      g_free (uri);
      return FALSE;
    }

  /* close the temporary file as it exists now, and hence
   * we successfully avoided a race condition there.
   */
  close (tmp_fd);

  /* generate a 1x1 image if we're storing a failure */
  thumbnail = (pixbuf != NULL) ? GDK_PIXBUF (pixbuf) : gdk_pixbuf_new (GDK_COLORSPACE_RGB, TRUE, 8, 1, 1);

  /* determine string representations for the modification time and size */
  mtime = g_strdup_printf ("%lu", (gulong) info->mtime);
  size = g_strdup_printf ("%" G_GUINT64_FORMAT, (guint64) info->size);

  /* write the thumbnail to the temporary location */
  succeed = gdk_pixbuf_save (thumbnail, tmp_path, "png", error,
                             "tEXt::Thumb::URI", uri,
                             "tEXt::Thumb::Size", size,
                             "tEXt::Thumb::MTime", mtime,
                             "tEXt::Thumb::Mimetype", thunar_vfs_mime_info_get_name (info->mime_info),
                             "tEXt::Software", "Thunar-VFS Thumbnail Factory",
                             NULL);

  /* drop the failed thumbnail pixbuf (if any) */
  if (G_UNLIKELY (pixbuf == NULL))
    g_object_unref (G_OBJECT (thumbnail));

  /* rename the file to the final location */
  if (G_LIKELY (succeed))
    {
      dst_path = g_strconcat (base_path, md5, ".png", NULL);
      if (G_UNLIKELY (g_rename (tmp_path, dst_path) < 0))
        {
          g_set_error (error, G_FILE_ERROR, g_file_error_from_errno (errno), g_strerror (errno));
          succeed = FALSE;
        }
      g_free (dst_path);
    }

  /* cleanup */
  g_free (tmp_path);
  g_free (mtime);
  g_free (size);
  g_free (md5);
  g_free (uri);

  return succeed;
}



/**
 * thunar_vfs_thumbnail_for_path:
 * @path : the #ThunarVfsPath to the original file.
 * @size : the desired #ThunarVfsThumbSize.
 *
 * Returns the absolute path to the thumbnail location
 * for the file described by @path, at the given @size.
 *
 * The caller is responsible to free the returned
 * string using g_free() when no longer needed.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: the absolute path to the thumbnail
 *               location for @path at @size.
 **/
gchar*
thunar_vfs_thumbnail_for_path (const ThunarVfsPath *path,
                               ThunarVfsThumbSize   size)
{
  gchar *thumbnail;
  gchar *md5;
  gchar *uri;

  uri = thunar_vfs_path_dup_uri (path);
  md5 = exo_str_get_md5_str (uri);
  thumbnail = g_strconcat (xfce_get_homedir (),
                           G_DIR_SEPARATOR_S ".thumbnails" G_DIR_SEPARATOR_S,
                           (size == THUNAR_VFS_THUMB_SIZE_NORMAL) ? "normal" : "large",
                           G_DIR_SEPARATOR_S, md5, ".png", NULL);
  g_free (md5);
  g_free (uri);

  return thumbnail;
}



/**
 * thunar_vfs_thumbnail_is_valid:
 * @thumbnail : the absolute path to a thumbnail file.
 * @uri       : the URI of the original file.
 * @mtime     : the modification time of the original file.
 *
 * Checks whether the file located at @thumbnail contains a
 * valid thumbnail for the file described by @uri and @mtime.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: %TRUE if @thumbnail is a valid thumbnail for
 *               the file referred to by @uri, else %FALSE.
 **/
gboolean
thunar_vfs_thumbnail_is_valid (const gchar      *thumbnail,
                               const gchar      *uri,
                               ThunarVfsFileTime mtime)
{
  png_structp  png_ptr;
  png_infop    info_ptr;
  png_textp    text_ptr;
  gboolean     is_valid = FALSE;
  gchar        signature[4];
  FILE        *fp;
  gint         n_checked;
  gint         n_text;
  gint         n;

  g_return_val_if_fail (g_path_is_absolute (thumbnail), FALSE);
  g_return_val_if_fail (uri != NULL && *uri != '\0', FALSE);

  /* try to open the thumbnail file */
  fp = fopen (thumbnail, "r");
  if (G_UNLIKELY (fp == NULL))
    return FALSE;

  /* read the png signature */
  if (G_UNLIKELY (fread (signature, 1, sizeof (signature), fp) != sizeof (signature)))
    goto done0;

  /* verify the png signature */
  if (G_LIKELY (png_check_sig ((png_bytep) signature, sizeof (signature))))
    rewind (fp);
  else
    goto done0;

  /* allocate the png read structure */
  png_ptr = png_create_read_struct (PNG_LIBPNG_VER_STRING, NULL, NULL, NULL);
  if (G_UNLIKELY (png_ptr == NULL))
    goto done0;

  /* allocate the png info structure */
  info_ptr = png_create_info_struct (png_ptr);
  if (G_UNLIKELY (info_ptr == NULL))
    {
      png_destroy_read_struct (&png_ptr, NULL, NULL);
      goto done0;
    }

  /* read the png info from the file */
  png_init_io (png_ptr, fp);
  png_read_info (png_ptr, info_ptr);

  /* verify the tEXt attributes defined by the thumbnail spec */
  n_text = png_get_text (png_ptr, info_ptr, &text_ptr, &n_text);
  for (n = 0, n_checked = 0; n_checked < 2 && n < n_text; ++n)
    {
      if (strcmp (text_ptr[n].key, "Thumb::MTime") == 0)
        {
          /* verify the modification time */
          if (G_UNLIKELY (atol (text_ptr[n].text) != mtime))
            goto done1;
          ++n_checked;
        }
      else if (strcmp (text_ptr[n].key, "Thumb::URI") == 0)
        {
          /* check if the URIs are equal */
          if (strcmp (text_ptr[n].text, uri) != 0)
            goto done1;
          ++n_checked;
        }
    }

  /* check if all required attributes were checked successfully */
  is_valid = (n_checked == 2);

done1:
  png_destroy_read_struct (&png_ptr, &info_ptr, NULL);
done0:
  fclose (fp);
  return is_valid;
}



#define __THUNAR_VFS_THUMB_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
