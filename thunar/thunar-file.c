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

#include "fallback-icon.h"



enum
{
  PROP_0,
  PROP_DISPLAY_NAME,
  PROP_MIME_INFO,
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



static GObjectClass *parent_class;
static guint         file_signals[LAST_SIGNAL];



G_DEFINE_TYPE (ThunarFile, thunar_file, GTK_TYPE_OBJECT);



static void
thunar_file_class_init (ThunarFileClass *klass)
{
  GObjectClass *gobject_class;

  parent_class = g_type_class_peek_parent (klass);

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
   * ThunarFile:path:
   *
   * The absolute path to this file.
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
                  g_cclosure_marshal_VOID__POINTER,
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

  /* free the mime info */
  if (G_LIKELY (file->mime_info != NULL))
    g_object_unref (G_OBJECT (file->mime_info));

  /* reset the vfs info (freeing the specific data) */
  //thunar_vfs_info_drop_from_cache (&file->info);
  thunar_vfs_info_reset (&file->info);

  g_free (file->display_name);

  G_OBJECT_CLASS (parent_class)->finalize (object);
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
      g_value_set_string (value, thunar_file_get_display_name (file));
      break;

    case PROP_MIME_INFO:
      g_value_set_object (value, thunar_file_get_mime_info (file));
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

  /* watch this file for changes */
  //thunar_vfs_info_add_to_cache (&file->info);

  return file;
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
  GtkIconTheme *icon_theme;
  GtkIconInfo  *icon_info;
  ExoMimeInfo  *mime_info;
  GdkPixbuf    *icon = NULL;
  GdkPixbuf    *tmp;

  g_return_val_if_fail (THUNAR_IS_FILE (file), NULL);

  icon_theme = gtk_icon_theme_get_default ();

  mime_info = thunar_file_get_mime_info (file);
  if (G_LIKELY (mime_info != NULL))
    {
      icon_info = exo_mime_info_lookup_icon (mime_info, icon_theme, size, 0);
      if (G_LIKELY (icon_info != NULL))
        {
          icon = gtk_icon_info_load_icon (icon_info, NULL);
          gtk_icon_info_free (icon_info);
        }
    }

  if (G_UNLIKELY (icon == NULL))
    {
      tmp = gdk_pixbuf_new_from_inline (sizeof (fallback_icon),
                                        fallback_icon, FALSE, NULL);
      icon = exo_gdk_pixbuf_scale_ratio (tmp, size);
      g_object_unref (G_OBJECT (tmp));
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



