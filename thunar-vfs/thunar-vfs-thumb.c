/* $Id$ */
/*-
 * Copyright (c) 2004-2005 Benedikt Meurer <benny@xfce.org>
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <png.h>

#include <thunar-vfs/thunar-vfs-enum-types.h>
#include <thunar-vfs/thunar-vfs-mime-database.h>
#include <thunar-vfs/thunar-vfs-thumb.h>
#include <thunar-vfs/thunar-vfs-alias.h>



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

  GHashTable            *pixbuf_mime_infos;

  gchar                 *base_path;
  gchar                 *fail_path;
  ThunarVfsThumbSize     size;
};



G_DEFINE_TYPE (ThunarVfsThumbFactory, thunar_vfs_thumb_factory, G_TYPE_OBJECT);



static void
thunar_vfs_thumb_factory_class_init (ThunarVfsThumbFactoryClass *klass)
{
  GObjectClass *gobject_class;

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
  factory->pixbuf_mime_infos = g_hash_table_new_full (g_direct_hash, g_direct_equal, exo_object_unref, NULL);
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
}



static void
thunar_vfs_thumb_factory_finalize (GObject *object)
{
  ThunarVfsThumbFactory *factory = THUNAR_VFS_THUMB_FACTORY (object);

  /* release the reference on the mime database */
  exo_object_unref (EXO_OBJECT (factory->mime_database));

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
 * @uri     : the #ThunarVfsURI to the original file.
 * @mtime   : the last known modification time of the original file.
 *
 * Looks up the path to a thumbnail for @uri in @factory and returns
 * the absolute path to the thumbnail file if a valid thumbnail is
 * found. Else %NULL is returned.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: the path to a valid thumbnail for @uri in @factory
 *               or %NULL if no valid thumbnail was found.
 **/
gchar*
thunar_vfs_thumb_factory_lookup_thumbnail (ThunarVfsThumbFactory *factory,
                                           const ThunarVfsURI    *uri,
                                           ThunarVfsFileTime      mtime)
{
  gchar *path;
  gchar *md5;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), NULL);
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);

  /* determine the path to the thumbnail for the factory */
  md5 = thunar_vfs_uri_get_md5sum (uri);
  path = g_strconcat (factory->base_path, md5, ".png", NULL);
  g_free (md5);

  /* check if the path contains a valid thumbnail */
  if (thunar_vfs_thumb_path_is_valid (path, uri, mtime))
    return path;

  /* no valid thumbnail */
  g_free (path);

  return NULL;
}



/**
 * thunar_vfs_thumb_factory_can_thumbnail:
 * @factory   : a #ThunarVfsThumbFactory.
 * @uri       : the #ThunarVfsURI to a file.
 * @mime_info : the #ThunarVfsMimeInfo of the file referred to by @uri.
 * @mtime     : the modification time of the file referred to by @uri.
 *
 * Checks if @factory can generate a thumbnail for
 * the file specified by @uri, which is of type
 * @mime_info.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: %TRUE if @factory can generate a thumbnail for @uri.
 **/
gboolean
thunar_vfs_thumb_factory_can_thumbnail (ThunarVfsThumbFactory   *factory,
                                        const ThunarVfsURI      *uri,
                                        const ThunarVfsMimeInfo *mime_info,
                                        ThunarVfsFileTime        mtime)
{
  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), FALSE);
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), FALSE);
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_INFO (mime_info), FALSE);

  /* we support only local files for thumbnail generation */
  if (G_UNLIKELY (thunar_vfs_uri_get_scheme (uri) != THUNAR_VFS_URI_SCHEME_FILE))
    return FALSE;

  /* we really don't want to generate a thumbnail for a thumbnail */
  if (G_UNLIKELY (strstr (thunar_vfs_uri_get_path (uri), G_DIR_SEPARATOR_S ".thumbnails" G_DIR_SEPARATOR_S) != NULL))
    return FALSE;

  /* check GdkPixbuf supports the given mime info */
  if (g_hash_table_lookup (factory->pixbuf_mime_infos, mime_info))
    {
      return !thunar_vfs_thumb_factory_has_failed_thumbnail (factory, uri, mtime);
    }

  return FALSE;
}



/**
 * thunar_vfs_thumb_factory_has_failed_thumbnail:
 * @factory : a #ThunarVfsThumbFactory.
 * @uri     : the #ThunarVfsURI to a file.
 * @mtime   : the modification time of the file referred to by @uri.
 *
 * Checks whether we know that @factory won't be able to generate
 * a thumbnail for the file referred to by @uri, whose modification
 * time is @mtime.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: %TRUE if @factory knows that it cannot generate
 *               a thumbnail for @uri, else %TRUE.
 **/
gboolean
thunar_vfs_thumb_factory_has_failed_thumbnail (ThunarVfsThumbFactory *factory,
                                               const ThunarVfsURI    *uri,
                                               ThunarVfsFileTime      mtime)
{
  gchar  path[4096];
  gchar *md5;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), FALSE);
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), FALSE);

  md5 = thunar_vfs_uri_get_md5sum (uri);
  g_snprintf (path, sizeof (path), "%s%s.png", factory->fail_path, md5);
  g_free (md5);

  return thunar_vfs_thumb_path_is_valid (path, uri, mtime);
}



/**
 * thunar_vfs_thumb_factory_generate_thumbnail:
 * @factory   : a #ThunarVfsThumbFactory.
 * @uri       : the #ThunarVfsURI to the file for which a thumbnail
 *              should be generated.
 * @mime_info : the #ThunarVfsMimeInfo for the file referred to by @uri.
 *
 * Tries to generate a thumbnail for the file referred to by @uri in
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
thunar_vfs_thumb_factory_generate_thumbnail (ThunarVfsThumbFactory   *factory,
                                             const ThunarVfsURI      *uri,
                                             const ThunarVfsMimeInfo *mime_info)
{
  GdkPixbuf *pixbuf;
  GdkPixbuf *scaled;
  gint       size;

  g_return_val_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory), NULL);
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);
  g_return_val_if_fail (THUNAR_VFS_IS_MIME_INFO (mime_info), NULL);

  /* we can only generate thumbnails for local files */
  if (G_UNLIKELY (thunar_vfs_uri_get_scheme (uri) != THUNAR_VFS_URI_SCHEME_FILE))
    return NULL;

  /* determine the pixel size of the thumbnail */
  size = G_LIKELY (factory->size == THUNAR_VFS_THUMB_SIZE_NORMAL) ? 128 : 256;

  /* try to load the original image using GdkPixbuf */
  pixbuf = gdk_pixbuf_new_from_file (thunar_vfs_uri_get_path (uri), NULL);
  if (G_UNLIKELY (pixbuf == NULL))
    return NULL;

  /* check if we need to scale the image */
  if (gdk_pixbuf_get_width (pixbuf) > size || gdk_pixbuf_get_height (pixbuf) > size)
    {
      scaled = exo_gdk_pixbuf_scale_ratio (pixbuf, size);
      g_object_unref (G_OBJECT (pixbuf));
      pixbuf = scaled;
    }

  return pixbuf;
}



/**
 * thunar_vfs_thumb_factory_store_thumbnail:
 * @factory : a #ThunarVfsThumbFactory.
 * @pixbuf  :
 * @uri     :
 * @mtime   :
 *
 * FIXME
 *
 * The usage of this method is thread-safe.
 *
 * Return value:
 **/
void
thunar_vfs_thumb_factory_store_thumbnail (ThunarVfsThumbFactory *factory,
                                          const GdkPixbuf       *pixbuf,
                                          const ThunarVfsURI    *uri,
                                          ThunarVfsFileTime      mtime)
{
  g_return_if_fail (THUNAR_VFS_IS_THUMB_FACTORY (factory));
  g_return_if_fail (GDK_IS_PIXBUF (pixbuf));
  g_return_if_fail (THUNAR_VFS_IS_URI (uri));
  
  // FIXME
}



/**
 * thunar_vfs_thumb_path_for_uri:
 * @uri  : the #ThunarVfsURI to the original file.
 * @size : the desired #ThunarVfsThumbSize.
 *
 * Returns the absolute path to the thumbnail location
 * for the file described by @uri, at the given @size.
 *
 * The caller is responsible to free the returned
 * string using g_free() when no longer needed.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: the absolute path to the thumbnail
 *               location for @uri at @size.
 **/
gchar*
thunar_vfs_thumb_path_for_uri (const ThunarVfsURI *uri,
                               ThunarVfsThumbSize  size)
{
  gchar *path;
  gchar *md5;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);

  md5 = thunar_vfs_uri_get_md5sum (uri);
  path = g_strconcat (xfce_get_homedir (),
                      G_DIR_SEPARATOR_S ".thumbnails" G_DIR_SEPARATOR_S,
                      (size == THUNAR_VFS_THUMB_SIZE_NORMAL) ? "normal" : "large",
                      G_DIR_SEPARATOR_S, md5, ".png", NULL);
  g_free (md5);

  return path;
}



/**
 * thunar_vfs_thumb_path_is_valid:
 * @thumb_path : the absolute path to a thumbnail file.
 * @uri        : the #ThunarVfsURI to the original file.
 * @mtime      : the modification time of the original file.
 *
 * Checks whether the file located at @thumb_path contains a
 * valid thumbnail for the file described by @uri.
 *
 * The usage of this method is thread-safe.
 *
 * Return value: %TRUE if @thumb_path is a valid thumbnail for
 *               the file referred to by @uri, else %FALSE.
 **/
gboolean
thunar_vfs_thumb_path_is_valid (const gchar        *thumb_path,
                                const ThunarVfsURI *uri,
                                ThunarVfsFileTime   mtime)
{
  ThunarVfsURI *thumb_uri;
  png_structp   png_ptr;
  png_infop     info_ptr;
  png_textp     text_ptr;
  gboolean      is_valid = FALSE;
  gchar         signature[4];
  FILE         *fp;
  gint          n_checked;
  gint          n_text;
  gint          n;

  g_return_val_if_fail (g_path_is_absolute (thumb_path), FALSE);
  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), FALSE);

  /* try to open the thumbnail file */
  fp = fopen (thumb_path, "r");
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
          /* transform the stored URI */
          thumb_uri = thunar_vfs_uri_new (text_ptr[n].text, NULL);
          if (G_UNLIKELY (thumb_uri == NULL))
            goto done1;

          /* check if the URIs are equal */
          if (!thunar_vfs_uri_equal (thumb_uri, uri))
            {
              thunar_vfs_uri_unref (thumb_uri);
              goto done1;
            }

          /* the URIs are equal */
          thunar_vfs_uri_unref (thumb_uri);
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
