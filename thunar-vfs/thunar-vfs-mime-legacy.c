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
 *
 * Based on code initially written by Matthias Clasen <mclasen@redhat.com>
 * for the xdgmime library.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar-vfs/thunar-vfs-mime-legacy.h>



static void         thunar_vfs_mime_legacy_class_init             (ThunarVfsMimeLegacyClass *klass);
static void         thunar_vfs_mime_legacy_finalize               (ExoObject                *object);
static const gchar *thunar_vfs_mime_legacy_lookup_data            (ThunarVfsMimeProvider    *provider,
                                                                   gconstpointer             data,
                                                                   gsize                     length,
                                                                   gint                     *priority);
static const gchar *thunar_vfs_mime_legacy_lookup_literal         (ThunarVfsMimeProvider    *provider,
                                                                   const gchar              *filename);
static const gchar *thunar_vfs_mime_legacy_lookup_suffix          (ThunarVfsMimeProvider    *provider,
                                                                   const gchar              *suffix,
                                                                   gboolean                  ignore_case);
static const gchar *thunar_vfs_mime_legacy_lookup_glob            (ThunarVfsMimeProvider    *provider,
                                                                   const gchar              *filename);
static GList       *thunar_vfs_mime_legacy_get_stop_characters    (ThunarVfsMimeProvider    *provider);
static gsize        thunar_vfs_mime_legacy_get_max_buffer_extents (ThunarVfsMimeProvider    *provider);



struct _ThunarVfsMimeLegacyClass
{
  ThunarVfsMimeProviderClass __parent__;
};

struct _ThunarVfsMimeLegacy
{
  ThunarVfsMimeProvider __parent__;
};



static ExoObjectClass *thunar_vfs_mime_legacy_parent_class;



GType
thunar_vfs_mime_legacy_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsMimeLegacyClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_mime_legacy_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsMimeLegacy),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_MIME_PROVIDER,
                                     "ThunarVfsMimeLegacy", &info, 0);
    }

  return type;
}



static void
thunar_vfs_mime_legacy_class_init (ThunarVfsMimeLegacyClass *klass)
{
  ThunarVfsMimeProviderClass *thunarvfs_mime_provider_class;
  ExoObjectClass             *exoobject_class;

  thunar_vfs_mime_legacy_parent_class = g_type_class_peek_parent (klass);

  exoobject_class = EXO_OBJECT_CLASS (klass);
  exoobject_class->finalize = thunar_vfs_mime_legacy_finalize;

  thunarvfs_mime_provider_class = THUNAR_VFS_MIME_PROVIDER_CLASS (klass);
  thunarvfs_mime_provider_class->lookup_data = thunar_vfs_mime_legacy_lookup_data;
  thunarvfs_mime_provider_class->lookup_literal = thunar_vfs_mime_legacy_lookup_literal;
  thunarvfs_mime_provider_class->lookup_suffix = thunar_vfs_mime_legacy_lookup_suffix;
  thunarvfs_mime_provider_class->lookup_glob = thunar_vfs_mime_legacy_lookup_glob;
  thunarvfs_mime_provider_class->get_stop_characters = thunar_vfs_mime_legacy_get_stop_characters;
  thunarvfs_mime_provider_class->get_max_buffer_extents = thunar_vfs_mime_legacy_get_max_buffer_extents;
}



static void
thunar_vfs_mime_legacy_finalize (ExoObject *object)
{
  (*EXO_OBJECT_CLASS (thunar_vfs_mime_legacy_parent_class)->finalize) (object);
}



static const gchar*
thunar_vfs_mime_legacy_lookup_data (ThunarVfsMimeProvider *provider,
                                    gconstpointer          data,
                                    gsize                  length,
                                    gint                  *priority)
{
  return NULL;
}



static const gchar*
thunar_vfs_mime_legacy_lookup_literal (ThunarVfsMimeProvider *provider,
                                       const gchar           *filename)
{
  return NULL;
}



static const gchar*
thunar_vfs_mime_legacy_lookup_suffix (ThunarVfsMimeProvider *provider,
                                      const gchar           *suffix,
                                      gboolean               ignore_case)
{
  return NULL;
}



static const gchar*
thunar_vfs_mime_legacy_lookup_glob (ThunarVfsMimeProvider *provider,
                                    const gchar           *filename)
{
  return NULL;
}



static GList*
thunar_vfs_mime_legacy_get_stop_characters (ThunarVfsMimeProvider *provider)
{
  return NULL;
}



static gsize
thunar_vfs_mime_legacy_get_max_buffer_extents (ThunarVfsMimeProvider *provider)
{
  return 0;
}



/**
 * thunar_vfs_mime_legacy_new:
 * @directory : an XDG mime base directory.
 *
 * Allocates a new #ThunarVfsMimeLegacy for @directory and
 * returns the instance on success, or %NULL on error.
 *
 * The caller is responsible to free the returned instance
 * using #exo_object_unref().
 *
 * Return value: the newly allocated #ThunarVfsMimeLegacy
 *               instance or %NULL on error.
 **/
ThunarVfsMimeProvider*
thunar_vfs_mime_legacy_new (const gchar *directory)
{
  return NULL;
}



