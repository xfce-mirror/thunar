/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009-2011 Jannis Pohlmann <jannis@xfce.org>
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

#ifdef HAVE_CONFIG_H
#include <config.h>
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
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <gio/gio.h>

#include <exo/exo.h>

#include <tdb/tdb.h>

#include <thunar/thunar-metafile.h>
#include <thunar/thunar-private.h>



static void     thunar_metafile_class_init  (ThunarMetafileClass  *klass);
static void     thunar_metafile_init        (ThunarMetafile       *metafile);
static void     thunar_metafile_finalize    (GObject              *object);
static TDB_DATA thunar_metafile_read        (ThunarMetafile       *metafile,
                                             TDB_DATA              data);



struct _ThunarMetafileClass
{
  GObjectClass __parent__;
};

struct _ThunarMetafile
{
  GObject __parent__;

  TDB_CONTEXT *context;
  TDB_DATA     data;
};



static GObjectClass *thunar_metafile_parent_class;



GType
thunar_metafile_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarMetafileClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_metafile_class_init,
        NULL,
        NULL,
        sizeof (ThunarMetafile),
        0,
        (GInstanceInitFunc) thunar_metafile_init,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarMetafile"), &info, 0);
    }

  return type;
}



static void
thunar_metafile_class_init (ThunarMetafileClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_metafile_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_metafile_finalize;
}



static void
thunar_metafile_init (ThunarMetafile *metafile)
{
  gchar *path;

  /* determine the path to the metafile database (create directories as required) */
  path = xfce_resource_save_location (XFCE_RESOURCE_CACHE, "Thunar/metafile.tdb", TRUE);
  if (G_UNLIKELY (path == NULL))
    {
      path = xfce_resource_save_location (XFCE_RESOURCE_CACHE, "Thunar/", FALSE);
      g_warning ("Failed to create the Thunar cache directory in %s", path);
      g_free (path);
      return;
    }

  /* try to open the metafile database file */
  metafile->context = tdb_open (path, 0, TDB_DEFAULT, O_CREAT | O_RDWR, 0600);
  if (G_UNLIKELY (metafile->context == NULL))
    g_warning ("Failed to open metafile database in %s: %s.", path, g_strerror (errno));

  /* release the path */
  g_free (path);
}



static void
thunar_metafile_finalize (GObject *object)
{
  ThunarMetafile *metafile = THUNAR_METAFILE (object);

  /* close the database (if open) */
  if (G_LIKELY (metafile->context != NULL))
    tdb_close (metafile->context);

  /* release any pending data */
  if (G_LIKELY (metafile->data.dptr != NULL))
    free (metafile->data.dptr);

  (*G_OBJECT_CLASS (thunar_metafile_parent_class)->finalize) (object);
}



static TDB_DATA
thunar_metafile_read (ThunarMetafile *metafile,
                      TDB_DATA        data)
{
  /* perform the fetch operation on the database */
  data = tdb_fetch (metafile->context, data);
  if (G_UNLIKELY (data.dptr != NULL))
    {
      /* validate the result */
      if (data.dsize < sizeof (guint32)
          || (data.dsize % sizeof (guint32)) != 0
          || data.dptr[data.dsize - 1] != '\0')
        {
          free (data.dptr);
          data.dptr = NULL;
          data.dsize = 0;
        }
    }

  return data;
}



/**
 * thunar_metafile_get_default:
 *
 * Returns a reference to the default #ThunarMetafile
 * instance. There can be only one #ThunarMetafile
 * instance at any time.
 *
 * The caller is responsible to free the returned
 * object using g_object_unref() when no longer
 * needed.
 *
 * Return value: a reference to the default #ThunarMetafile
 *               instance.
 **/
ThunarMetafile*
thunar_metafile_get_default (void)
{
  static ThunarMetafile *metafile = NULL;

  if (G_UNLIKELY (metafile == NULL))
    {
      /* allocate a new metafile instance. */
      metafile = g_object_new (THUNAR_TYPE_METAFILE, NULL);
      g_object_add_weak_pointer (G_OBJECT (metafile), (gpointer) &metafile);
    }
  else
    {
      /* take a reference for the caller */
      g_object_ref (G_OBJECT (metafile));
    }

  return metafile;
}



/**
 * thunar_metafile_fetch:
 * @metafile      : a #ThunarMetafile.
 * @file          : a #Gfile.
 * @key           : a #ThunarMetafileKey.
 * @default_value : the default value for @key,
 *                  which may be %NULL.
 *
 * Fetches the value for @key on @path in
 * @metafile. Returns a pointer to the
 * value if found, or the default value
 * if the @key is explicitly not set for
 * @path in @metafile, as specified in
 * @default_value.
 *
 * The returned string is owned by @metafile
 * and is only valid until the next call to
 * thunar_metafile_fetch(), so you might need
 * to take a copy of the value if you need to
 * keep for a longer period.
 *
 * Return value: the value for @key on @path
 *               in @metafile or the default
 *               value for @key, as specified
 *               by @default_value.
 **/
const gchar*
thunar_metafile_fetch (ThunarMetafile   *metafile,
                       GFile            *file,
                       ThunarMetafileKey key,
                       const gchar      *default_value)
{
  const guchar *dend;
  const guchar *dp;
  TDB_DATA      key_data;
  gssize        key_size;
  gchar        *key_path = NULL;

  _thunar_return_val_if_fail (THUNAR_IS_METAFILE (metafile), NULL);
  _thunar_return_val_if_fail (G_IS_FILE (file), NULL);
  _thunar_return_val_if_fail (key < THUNAR_METAFILE_N_KEYS, NULL);

  /* check if the database handle is available */
  if (G_UNLIKELY (metafile->context == NULL))
    goto use_default_value;

  /* determine the string representation of the path (using the URI for non-local paths) */
  key_path = g_file_get_uri (file);
  key_size = strlen (key_path);

  if (G_UNLIKELY (key_size <= 0))
    goto use_default_value;

  /* generate the key data */
  key_data.dptr = key_path;
  key_data.dsize = key_size;

  /* release any earlier result data */
  if (G_LIKELY (metafile->data.dptr != NULL))
    free (metafile->data.dptr);

  /* perform the fetch operation on the database */
  metafile->data = thunar_metafile_read (metafile, key_data);
  if (G_LIKELY (metafile->data.dptr == NULL))
    goto use_default_value;

  /* lookup the value for the given key */
  dp = (const guchar *) metafile->data.dptr;
  dend = dp + metafile->data.dsize;
  for (;;)
    {
      /* check if we have a match */
      if (*dp == (guint) key)
        {
          g_free (key_path);
          return (const gchar *) (dp + 1);
        }

      /* lookup the next entry */
      do
        {
          /* skip another 4 bytes */
          dp += sizeof (guint32);
          if (G_UNLIKELY (dp == dend))
            goto use_default_value;
        }
      while (*(dp - 1) != '\0');
    }

  /* use the default value */
use_default_value:
  g_free (key_path);
  return default_value;
}



/**
 * thunar_metafile_store:
 * @metafile      : a #ThunarMetafile.
 * @file          : a #GFile.
 * @key           : a #ThunarMetafileKey.
 * @value         : the new value for @key on @path.
 * @default_value : the default value for @key on @path.
 *
 * Stores the given @value for @key on @path in
 * @metafile.
 *
 * No error is returned from this method, but
 * the store operation may nevertheless fail,
 * so don't depend on the success of the operation.
 *
 * Note that if @value equals the @default_value
 * for @key, it isn't stored in the @metafile to
 * save memory.
 **/
void
thunar_metafile_store (ThunarMetafile   *metafile,
                       GFile            *file,
                       ThunarMetafileKey key,
                       const gchar      *value,
                       const gchar      *default_value)
{
  TDB_DATA value_data;
  TDB_DATA key_data;
  gssize   value_size;
  gssize   key_size;
  gchar   *buffer;
  gchar   *bp;
  gchar   *key_path;

  _thunar_return_if_fail (THUNAR_IS_METAFILE (metafile));
  _thunar_return_if_fail (G_IS_FILE (file));
  _thunar_return_if_fail (key < THUNAR_METAFILE_N_KEYS);
  _thunar_return_if_fail (value != NULL);
  _thunar_return_if_fail (default_value != NULL);

  /* check if the database handle is available */
  if (G_UNLIKELY (metafile->context == NULL))
    return;

  /* determine the string representation of the file */
  key_path = g_file_get_uri (file);
  key_size = strlen (key_path);

  if (G_UNLIKELY (key_size <= 0))
    return;

  /* generate the key data */
  key_data.dptr = key_path;
  key_data.dsize = key_size;

  /* fetch the current value for the key */
  value_data = thunar_metafile_read (metafile, key_data);

  /* determine the size required for the new value */
  value_size = strlen (value) + 2;
  value_size = ((value_size + sizeof (guint32) - 1) / sizeof (guint32)) * sizeof (guint32);

  /* allocate a buffer to merge the existing value set with the new value */
  buffer = g_new0 (gchar, value_data.dsize + value_size);

  /* copy the new value to the buffer if it's not equal to the default value */
  if (G_LIKELY (strcmp (value, default_value) != 0))
    {
      buffer[0] = key;
      strcpy (buffer + 1, value);
      bp = buffer + value_size;
    }
  else
    {
      bp = buffer;
    }

  /* copy the existing entries (if any) */
  if (G_LIKELY (value_data.dptr != NULL))
    {
      const guchar *vp = (const guchar *) value_data.dptr;
      const guchar *vend = vp + value_data.dsize;
      const guchar *vx;

      for (; vp < vend; vp = vx)
        {
          /* grab a pointer to the next entry (thereby calc
           * the length of this entry).
           */
          for (vx = vp + sizeof (guint32); *(vx - 1) != '\0'; vx += sizeof (guint32))
            ;

          /* verify the vx pointer */
          _thunar_assert (vx <= vend);
          _thunar_assert (vx > vp);

          /* check if we should copy the entry */
          if (*vp != key)
            {
              memcpy (bp, vp, vx - vp);
              bp += (vx - vp);
            }
        }

      /* verify the buffer space */
      _thunar_assert (bp <= buffer + value_data.dsize + value_size);
      _thunar_assert ((bp - buffer) % sizeof (guint32) == 0);

      /* release the previous value set */
      free (value_data.dptr);
    }

  /* delete the key from the database if the new
   * value set is the same as the default value set.
   */
  if (G_UNLIKELY (bp == buffer))
    {
      tdb_delete (metafile->context, key_data);
    }
  else
    {
      /* setup the new value set */
      value_data.dptr = buffer;
      value_data.dsize = bp - buffer;

      /* execute the store operation */
      tdb_store (metafile->context, key_data, value_data, TDB_REPLACE);
    }

  /* free the file URI */
  g_free (key_path);

  /* free the buffer space */
  g_free (buffer);
}

