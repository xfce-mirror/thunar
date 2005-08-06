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

#include <thunar-vfs/thunar-vfs-mime.h>
#include <thunar-vfs/thunar-vfs-mime-database.h>



static ThunarVfsMimeDatabase *mime_database = NULL;



/**
 * _thunar_vfs_mime_init:
 *
 * Initializes the MIME component of the ThunarVFS
 * library.
 **/
void
_thunar_vfs_mime_init (void)
{
  g_return_if_fail (mime_database == NULL);

  mime_database = thunar_vfs_mime_database_get ();
}



/**
 * _thunar_vfs_mime_shutdown:
 *
 * Shuts down the MIME component of the ThunarVFS
 * library.
 **/
void
_thunar_vfs_mime_shutdown (void)
{
  g_return_if_fail (mime_database != NULL);

  exo_object_unref (EXO_OBJECT (mime_database));
  mime_database = NULL;
}



/**
 * thunar_vfs_mime_info_get:
 * @mime_type : the string representation of a mime type.
 *
 * Returns the #ThunarVfsMimeInfo corresponding to 
 * @mime_type.
 *
 * The caller is responsible to free the returned object
 * using #thunar_vfs_mime_info_unref().
 *
 * Return value: the #ThunarVfsMimeInfo for @mime_type.
 **/
ThunarVfsMimeInfo*
thunar_vfs_mime_info_get (const gchar *mime_type)
{
  return thunar_vfs_mime_database_get_info (mime_database, mime_type);
}


 
/**
 * thunar_vfs_mime_info_get_for_file:
 * @path : the path to a local file.
 *
 * Determines the #ThunarVfsMimeInfo of the file referred to
 * by @path.
 *
 * The caller is responsible to free the returned object
 * using #thunar_vfs_mime_info_unref().
 *
 * Return value: the #ThunarVfsMimeInfo for @path.
 **/
ThunarVfsMimeInfo*
thunar_vfs_mime_info_get_for_file (const gchar *path)
{
  return thunar_vfs_mime_database_get_info_for_file (mime_database, path, NULL);
}



