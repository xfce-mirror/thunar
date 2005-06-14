/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-file.h>
#include <thunar/thunar-icon-factory.h>



enum
{
  PROP_0,
  PROP_DISPLAY_NAME,
  PROP_MIME_INFO,
  PROP_SPECIAL_NAME,
  PROP_URI,
};

enum
{
  CHANGED,
  LAST_SIGNAL,
};



static void thunar_file_class_init    (ThunarFileClass  *klass);
static void thunar_file_init          (ThunarFile       *file);
static void thunar_file_finalize      (GObject          *object);
static void thunar_file_get_property  (GObject          *object,
                                       guint             prop_id,
                                       GValue           *value,
                                       GParamSpec       *pspec);



static GHashTable *file_cache;
static guint       file_signals[LAST_SIGNAL];



G_DEFINE_TYPE (ThunarFile, thunar_file, GTK_TYPE_OBJECT);



static void
thunar_file_class_init (ThunarFileClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_file_finalize;
  gobject_class->get_property = thunar_file_get_property;

  /**
   * ThunarFile:display-name:
   *
   * The displayable name of the this file in the UTF-8 encoding.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_DISPLAY_NAME,
                                   g_param_spec_string ("display-name",
                                                        _("Displayable file name"),
                                                        _("The displayable file name in UTF-8 encoding"),
                                                        NULL,
                                                        G_PARAM_READABLE));

  /**
   * ThunarFile:mime-info:
   *
   * The MIME type information for this file.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_MIME_INFO,
                                   g_param_spec_object ("mime-info",
                                                        _("MIME info"),
                                                        _("The MIME info for this file"),
                                                        EXO_TYPE_MIME_INFO,
                                                        G_PARAM_READABLE));

  /**
   * ThunarFile:special-name:
   *
   * A special for this file. This includes the special name for a file, if there's
   * any. E.g. the special name for "/" is "Filesystem", and so on. If there's no
   * special name for a given #ThunarFile, this property will include the same
   * value as the "display-name" property, so it's safe to query this property.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_SPECIAL_NAME,
                                   g_param_spec_string ("special-name",
                                                        _("Special file name"),
                                                        _("The special name of this file"),
                                                        NULL,
                                                        G_PARAM_READABLE));

  /**
   * ThunarFile:uri:
   *
   * The absolute uri to this file.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_URI,
                                   g_param_spec_object ("uri",
                                                        _("File URI"),
                                                        _("The URI describing the location of the file"),
                                                        THUNAR_VFS_TYPE_URI,
                                                        G_PARAM_READABLE));
  
  /**
   * ThunarFile:changed:
   * @file : the #ThunarFile instance.
   *
   * Emitted whenever the system notices a change on this @file.
   **/
  file_signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarFileClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
thunar_file_init (ThunarFile *file)
{
  g_return_if_fail (THUNAR_IS_FILE (file));

  thunar_vfs_info_init (&file->info);
}



static void
thunar_file_finalize (GObject *object)
{
  ThunarFile *file = THUNAR_FILE (object);

  g_return_if_fail (THUNAR_IS_FILE (file));

  /* drop this ThunarFile from the cache */
  if (G_LIKELY (file->info.uri != NULL))
    g_hash_table_remove (file_cache, file->info.uri);

  /* free the mime info */
  if (G_LIKELY (file->mime_info != NULL))
    g_object_unref (G_OBJECT (file->mime_info));

  /* reset the vfs info (freeing the specific data) */
  thunar_vfs_info_reset (&file->info);

  g_free (file->display_name);

  G_OBJECT_CLASS (thunar_file_parent_class)->finalize (object);
}



static void
thunar_file_get_property  (GObject    *object,
                           guint       prop_id,
                           GValue     *value,
                           GParamSpec *pspec)
{
  ThunarFile *file = THUNAR_FILE (object);

  switch (prop_id)
    {
    case PROP_DISPLAY_NAME:
      g_value_set_static_string (value, thunar_file_get_display_name (file));
      break;

    case PROP_MIME_INFO:
      g_value_set_object (value, thunar_file_get_mime_info (file));
      break;

    case PROP_SPECIAL_NAME:
      g_value_set_static_string (value, thunar_file_get_special_name (file));
      break;

    case PROP_URI:
      g_value_set_object (value, thunar_file_get_uri (file));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/**
 * thunar_file_get_for_uri:
 * @uri   : an #ThunarVfsURI instance.
 * @error : error return location.
 *
 * Tries to query the file referred to by @uri. Returns %NULL
 * if the file could not be queried and @error will point
 * to an error describing the problem, else the #ThunarFile
 * instance will be returned, which needs to freed using
 * #g_object_unref() when no longer needed.
 *
 * Note that this function is not thread-safe and may only
 * be called from the main thread.
 *
 * Return value: the #ThunarFile instance or %NULL on error.
 **/
ThunarFile*
thunar_file_get_for_uri (ThunarVfsURI *uri,
                         GError      **error)
{
  ThunarFile *file;

  g_return_val_if_fail (THUNAR_VFS_IS_URI (uri), NULL);

  /* allocate the ThunarFile cache on-demand */
  if (G_UNLIKELY (file_cache == NULL))
    file_cache = g_hash_table_new (thunar_vfs_uri_hash, thunar_vfs_uri_equal);

  /* check if we have the corresponding file cached already */
  file = g_hash_table_lookup (file_cache, uri);
  if (file == NULL)
    {
      /* allocate the new file object */
      file = g_object_new (THUNAR_TYPE_FILE, NULL);
      file->display_name = thunar_vfs_uri_get_display_name (uri);

      /* drop the floating reference */
      g_object_ref (G_OBJECT (file));
      gtk_object_sink (GTK_OBJECT (file));

      /* query the file info */
      if (!thunar_vfs_info_query (&file->info, uri, error))
        {
          g_object_unref (G_OBJECT (file));
          return NULL;
        }

      /* insert the file into the cache */
      g_hash_table_insert (file_cache, uri, file);
    }
  else
    {
      /* take another reference on the cached file */
      g_object_ref (G_OBJECT (file));
    }

  return file;
}



/**
 * thunar_file_get_parent:
 * @file  : a #ThunarFile instance.
 * @error : return location for errors.
 *
 * Queries the parent #ThunarFile (the directory, which includes @file)
 * for @file. If an error happens, %NULL will be returned and @error
 * will be set to point to a #GError. Else the #ThunarFile will
 * be returned. The method will automatically take a reference
 * on the returned file for the caller, so you'll have to call
 * #g_object_unref() when you're done with the returned file object.
 *
 * There's one special case to take care of: If @file refers to the
 * root directory ("/"), %NULL will be returned, but @error won't
 * be set. You'll have to handle this case gracefully.
 *
 * Return value: the parent #ThunarFile or %NULL.
 **/
ThunarFile*
thunar_file_get_parent (ThunarFile *file,
                        GError    **error)
{
  ThunarVfsURI *parent_uri;
  ThunarFile   *parent_file;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  /* lookup the parent's URI */
  parent_uri = thunar_vfs_uri_parent (thunar_file_get_uri (file));
  if (G_UNLIKELY (parent_uri == NULL))
    {
      /* file has no parent (e.g. "/") */
      return NULL;
    }

  /* lookup the file object for the parent_uri */
  parent_file = thunar_file_get_for_uri (parent_uri, error);

  /* release the reference on the parent_uri */
  g_object_unref (G_OBJECT (parent_uri));

  return parent_file;
}



/**
 * thunar_file_get_display_name:
 * @file  : a #ThunarFile instance.
 *
 * Returns the @file name in the UTF-8 encoding, which is
 * suitable for displaying the file name in the GUI.
 *
 * Return value: the @file name suitable for display.
 **/
const gchar*
thunar_file_get_display_name (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return file->display_name;
}



/**
 * thunar_file_get_mime_info:
 * @file  : a #ThunarFile instance.
 *
 * Returns the MIME type information for the given @file object. This
 * function may return %NULL if it is unable to determine a MIME type.
 *
 * Note, that there's no reference taken for the caller on the
 * returned #ExoMimeInfo, so if you need the object for a longer
 * period, you'll need to take a reference yourself using the
 * #g_object_ref() function.
 *
 * Return value: the MIME type or %NULL.
 **/
ExoMimeInfo*
thunar_file_get_mime_info (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  /* determine the MIME type on-demand */
  if (G_UNLIKELY (file->mime_info == NULL))
    file->mime_info = thunar_vfs_info_get_mime_info (&file->info);

  return file->mime_info;
}



/**
 * thunar_file_get_special_name:
 * @file : a #ThunarFile instance.
 *
 * Returns the special name for the @file, e.g. "Filesystem" for the #ThunarFile
 * referring to "/" and so on. If there's no special name for this @file, the
 * value of the "display-name" property will be returned instead.
 *
 * Return value: the special name of @file.
 **/
const gchar*
thunar_file_get_special_name (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  if (thunar_vfs_uri_get_scheme (file->info.uri) == THUNAR_VFS_URI_SCHEME_FILE)
    {
      if (thunar_vfs_uri_is_root (file->info.uri))
        return _("Filesystem");
      else if (thunar_vfs_uri_is_home (file->info.uri))
        return _("Home");
    }

  return thunar_file_get_display_name (file);
}



/**
 * thunar_file_get_uri:
 * @file  : a #ThunarFile instance.
 *
 * Returns the #ThunarVfsURI, that refers to the location of the @file.
 *
 * This method cannot return %NULL, unless there's a bug in the
 * application. So you should never check the returned value for
 * a possible %NULL pointer, except in the context of a #g_return_if_fail()
 * or #g_assert() macro, which will not be compiled into the final
 * binary.
 *
 * Note, that there's no reference taken for the caller on the
 * returned #ThunarVfsURI, so if you need the object for a longer
 * period, you'll need to take a reference yourself using the
 * #g_object_ref() function.
 *
 * Return value: the URI to the @file.
 **/
ThunarVfsURI*
thunar_file_get_uri (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return file->info.uri;
}



/**
 * thunar_file_get_mode_string:
 * @file : a #ThunarFile instance.
 *
 * Returns the mode of @file as text. You'll need to free
 * the result using #g_free() when you're done with it.
 *
 * Return value: the mode of @file as string.
 **/
gchar*
thunar_file_get_mode_string (ThunarFile *file)
{
  ThunarVfsFileMode mode;
  ThunarVfsFileType type;
  gchar            *text;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  mode = file->info.mode;
  type = file->info.type;
  text = g_new (gchar, 11);

  /* file type */
  switch (type)
    {
    case THUNAR_VFS_FILE_TYPE_SOCKET:     text[0] = 's'; break;
    case THUNAR_VFS_FILE_TYPE_SYMLINK:    text[0] = 'l'; break;
    case THUNAR_VFS_FILE_TYPE_REGULAR:    text[0] = '-'; break;
    case THUNAR_VFS_FILE_TYPE_BLOCKDEV:   text[0] = 'b'; break;
    case THUNAR_VFS_FILE_TYPE_DIRECTORY:  text[0] = 'd'; break;
    case THUNAR_VFS_FILE_TYPE_CHARDEV:    text[0] = 'c'; break;
    case THUNAR_VFS_FILE_TYPE_FIFO:       text[0] = 'f'; break;
    default:                              g_assert_not_reached ();
    }

  /* permission flags */
  text[1] = (mode & THUNAR_VFS_FILE_MODE_USR_READ)  ? 'r' : '-';
  text[2] = (mode & THUNAR_VFS_FILE_MODE_USR_WRITE) ? 'w' : '-';
  text[3] = (mode & THUNAR_VFS_FILE_MODE_USR_EXEC)  ? 'x' : '-';
  text[4] = (mode & THUNAR_VFS_FILE_MODE_GRP_READ)  ? 'r' : '-';
  text[5] = (mode & THUNAR_VFS_FILE_MODE_GRP_WRITE) ? 'w' : '-';
  text[6] = (mode & THUNAR_VFS_FILE_MODE_GRP_EXEC)  ? 'x' : '-';
  text[7] = (mode & THUNAR_VFS_FILE_MODE_OTH_READ)  ? 'r' : '-';
  text[8] = (mode & THUNAR_VFS_FILE_MODE_OTH_WRITE) ? 'w' : '-';
  text[9] = (mode & THUNAR_VFS_FILE_MODE_OTH_EXEC)  ? 'x' : '-';

  /* special flags */
  if (G_UNLIKELY (mode & THUNAR_VFS_FILE_MODE_SUID))
    text[3] = 's';
  if (G_UNLIKELY (mode & THUNAR_VFS_FILE_MODE_SGID))
    text[6] = 's';
  if (G_UNLIKELY (mode & THUNAR_VFS_FILE_MODE_STICKY))
    text[9] = 't';

  text[10] = '\0';

  return text;
}



/**
 * thunar_file_get_size_string:
 * @file : a #ThunarFile instance.
 *
 * Returns the size of the file as text in a human readable
 * format. You'll need to free the result using #g_free()
 * if you're done with it.
 *
 * Return value: the size of @file in a human readable
 *               format.
 **/
gchar*
thunar_file_get_size_string (ThunarFile *file)
{
  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);
  return thunar_vfs_humanize_size (file->info.size, NULL, 0);
}



/**
 * thunar_file_load_icon:
 * @file  : a #ThunarFile instance.
 * @size  : the icon size in pixels.
 *
 * TODO: We need to perform some kind of caching, esp. on
 * common MIME icons.
 *
 * You need to call #g_object_unref() on the returned icon
 * when you don't need it any longer.
 *
 * Return value: the icon for @file at @size.
 **/
GdkPixbuf*
thunar_file_load_icon (ThunarFile *file,
                       gint        size)
{
  ThunarIconFactory *icon_factory;
  GtkIconTheme      *icon_theme;
  ExoMimeInfo       *mime_info;
  const gchar       *icon_name;
  GdkPixbuf         *icon = NULL;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  icon_factory = thunar_icon_factory_get_default ();

  /* special icon for the root node */
  if (G_UNLIKELY (thunar_vfs_uri_is_root (file->info.uri)))
    icon = thunar_icon_factory_load_icon (icon_factory, "gnome-dev-harddisk", size, NULL, FALSE);

  /* special icon for the home node */
  if (G_UNLIKELY (icon == NULL && thunar_vfs_uri_is_home (file->info.uri)))
    icon = thunar_icon_factory_load_icon (icon_factory, "gnome-fs-home", size, NULL, FALSE);

  /* use the icon for the given MIME type */
  if (icon == NULL)
    {
      mime_info = thunar_file_get_mime_info (file);
      icon_theme = thunar_icon_factory_get_icon_theme (icon_factory);
      icon_name = exo_mime_info_lookup_icon_name (mime_info, icon_theme);
      icon = thunar_icon_factory_load_icon (icon_factory, icon_name, size, NULL, TRUE);
    }

  return icon;
}


  
/**
 * thunar_file_is_hidden:
 * @file : a #ThunarFile instance.
 *
 * Checks whether @file can be considered a hidden file.
 *
 * Return value: %TRUE if @file is a hidden file, else %FALSE.
 **/
gboolean
thunar_file_is_hidden (ThunarFile *file)
{
  const gchar *p;

  g_return_val_if_fail (THUNAR_IS_FILE (file), FALSE);
  g_return_val_if_fail (file->display_name != NULL, FALSE);

  p = file->display_name;
  if (*p != '.' && *p != '\0')
    {
      for (; p[1] != '\0'; ++p)
        ;
      return (*p == '~');
    }

  return TRUE;
}



