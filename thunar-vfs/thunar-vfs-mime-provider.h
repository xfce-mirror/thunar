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

#ifndef __THUNAR_VFS_MIME_PROVIDER_H__
#define __THUNAR_VFS_MIME_PROVIDER_H__

#include <exo/exo.h>

G_BEGIN_DECLS;

typedef struct _ThunarVfsMimeProviderClass ThunarVfsMimeProviderClass;
typedef struct _ThunarVfsMimeProvider      ThunarVfsMimeProvider;

#define THUNAR_VFS_TYPE_MIME_PROVIDER             (thunar_vfs_mime_provider_get_type ())
#define THUNAR_VFS_MIME_PROVIDER(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_MIME_PROVIDER, ThunarVfsMimeProvider))
#define THUNAR_VFS_MIME_PROVIDER_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_MIME_PROVIDER, ThunarVfsMimeProviderClass))
#define THUNAR_VFS_IS_MIME_PROVIDER(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_MIME_PROVIDER))
#define THUNAR_VFS_IS_MIME_PROVIDER_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_MIME_PROVIDER))
#define THUNAR_VFS_MIME_PROVIDER_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_MIME_PROVIDER, ThunarVfsMimeProviderClass))

struct _ThunarVfsMimeProviderClass
{
  GObjectClass __parent__;

  const gchar *(*lookup_data)             (ThunarVfsMimeProvider *provider,
                                           gconstpointer          data,
                                           gsize                  length,
                                           gint                  *priority);

  const gchar *(*lookup_literal)          (ThunarVfsMimeProvider *provider,
                                           const gchar           *filename);
  const gchar *(*lookup_suffix)           (ThunarVfsMimeProvider *provider,
                                           const gchar           *suffix,
                                           gboolean               ignore_case);
  const gchar *(*lookup_glob)             (ThunarVfsMimeProvider *provider,
                                           const gchar           *filename);

  const gchar *(*lookup_alias)            (ThunarVfsMimeProvider *provider,
                                           const gchar           *alias);

  guint        (*lookup_parents)          (ThunarVfsMimeProvider *provider,
                                           const gchar           *mime_type,
                                           gchar                **parents,
                                           guint                  max_parents);

  GList       *(*get_stop_characters)     (ThunarVfsMimeProvider *provider);
  gsize        (*get_max_buffer_extents)  (ThunarVfsMimeProvider *provider);
};

struct _ThunarVfsMimeProvider
{
  GObject __parent__;
};

GType thunar_vfs_mime_provider_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;

/**
 * thunar_vfs_mime_provider_lookup_data:
 * @provider : a #ThunarVfsMimeProvider.
 * @data     : pointer to the data.
 * @length   : length of @data in bytes.
 * @priority : return location for the priority or %NULL.
 *
 * The location pointed to by @priority (if not %NULL) will only
 * be set to a meaningfull value if this method returns a
 * non-%NULL value.
 *
 * Return value: a pointer to the MIME type or %NULL.
 **/
static inline const gchar*
thunar_vfs_mime_provider_lookup_data (ThunarVfsMimeProvider *provider,
                                      gconstpointer          data,
                                      gsize                  length,
                                      gint                  *priority)
{
  return (*THUNAR_VFS_MIME_PROVIDER_GET_CLASS (provider)->lookup_data) (provider, data, length, priority);
}

/**
 * thunar_vfs_mime_provider_lookup_literal:
 * @provider : a #ThunarVfsMimeProvider.
 * @filename : a filename.
 *
 * Return value: a pointer to the MIME type or %NULL.
 **/
static inline const gchar*
thunar_vfs_mime_provider_lookup_literal (ThunarVfsMimeProvider *provider,
                                         const gchar           *filename)
{
  return (*THUNAR_VFS_MIME_PROVIDER_GET_CLASS (provider)->lookup_literal) (provider, filename);
}

/**
 * thunar_vfs_mime_provider_lookup_suffix:
 * @provider    : a #ThunarVfsMimeProvider.
 * @suffix      : a filename suffix.
 * @ignore_case : %TRUE to perform case-insensitive comparison.
 *
 * Return value: a pointer to the MIME type or %NULL.
 **/
static inline const gchar*
thunar_vfs_mime_provider_lookup_suffix (ThunarVfsMimeProvider *provider,
                                        const gchar           *suffix,
                                        gboolean               ignore_case)
{
  return (*THUNAR_VFS_MIME_PROVIDER_GET_CLASS (provider)->lookup_suffix) (provider, suffix, ignore_case);
}

/**
 * thunar_vfs_mime_provider_lookup_glob:
 * @provider : a #ThunarVfsMimeProvider.
 * @filename : a filename.
 *
 * Return value: a pointer to the MIME type or %NULL.
 **/
static inline const gchar*
thunar_vfs_mime_provider_lookup_glob (ThunarVfsMimeProvider *provider,
                                      const gchar           *filename)
{
  return (*THUNAR_VFS_MIME_PROVIDER_GET_CLASS (provider)->lookup_glob) (provider, filename);
}

/**
 * thunar_vfs_mime_provider_lookup_alias:
 * @provider : a #ThunarVfsMimeProvider.
 * @alias    : a valid mime type.
 *
 * Unaliases the @alias using the alias table found in @provider.
 *
 * Return value: the unaliased mime type or %NULL if @alias
 *               is not a valid mime type alias.
 */
static inline const gchar*
thunar_vfs_mime_provider_lookup_alias (ThunarVfsMimeProvider *provider,
                                       const gchar           *alias)
{
  return (*THUNAR_VFS_MIME_PROVIDER_GET_CLASS (provider)->lookup_alias) (provider, alias);
}

/**
 * thunar_vfs_mime_provider_lookup_parents:
 * @provider    : a #ThunarVfsMimeProvider.
 * @mime_type   : a valid mime type.
 * @parents     : buffer to store the parents to.
 * @max_parents : the maximum number of parents that should be stored to @parents.
 *
 * Looks up up to @max_parents parent types of @mime_type in @provider and
 * stores them to @parents. Returns the list of mime parents.
 *
 * Return value: the list of mime parents.
 **/
static inline guint
thunar_vfs_mime_provider_lookup_parents (ThunarVfsMimeProvider *provider,
                                         const gchar           *mime_type,
                                         gchar                **parents,
                                         guint                  max_parents)
{
  return (*THUNAR_VFS_MIME_PROVIDER_GET_CLASS (provider)->lookup_parents) (provider, mime_type, parents, max_parents);
}

/**
 * thunar_vfs_mime_provider_get_stop_characters:
 * @provider : a #ThunarVfsMimeProvider.
 *
 * Returns the list of stop characters for all suffix entries in @provider as
 * a #GList of #gunichar<!---->s. The caller is responsible to free the list
 * using g_list_free().
 *
 * Return value: the list of stop characters for the suffix entries in @provider.
 **/
static inline GList*
thunar_vfs_mime_provider_get_stop_characters (ThunarVfsMimeProvider *provider)
{
  return (*THUNAR_VFS_MIME_PROVIDER_GET_CLASS (provider)->get_stop_characters) (provider);
}

/**
 * thunar_vfs_mime_provider_get_max_buffer_extents:
 * @provider : a #ThunarVfsMimeProvider.
 *
 * Returns the max buffer extents required for a data lookup in @provider.
 *
 * Return value: the max buffer extents for @provider.
 **/
static inline gsize
thunar_vfs_mime_provider_get_max_buffer_extents (ThunarVfsMimeProvider *provider)
{
  return (*THUNAR_VFS_MIME_PROVIDER_GET_CLASS (provider)->get_max_buffer_extents) (provider);
}

G_END_DECLS;

#endif /* !__THUNAR_VFS_MIME_PROVIDER_H__ */
