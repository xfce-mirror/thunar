/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#ifndef __THUNAR_METAFILE_H__
#define __THUNAR_METAFILE_H__

#include <gio/gio.h>

G_BEGIN_DECLS;

typedef struct _ThunarMetafileClass ThunarMetafileClass;
typedef struct _ThunarMetafile      ThunarMetafile;

#define THUNAR_TYPE_METAFILE            (thunar_metafile_get_type ())
#define THUNAR_METAFILE(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_METAFILE, ThunarMetafile))
#define THUNAR_METAFILE_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_METAFILE, ThunarMetafileClass))
#define THUNAR_IS_METAFILE(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_METAFILE))
#define THUNAR_IS_METAFILE_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_METAFILE))
#define THUNAR_METAFILE_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_METAFILE, ThunarMetafileClass))

/**
 * ThunarMetafileKey:
 * @THUNAR_METAFILE_KEY_EMBLEMS :
 **/
typedef enum /*< enum >*/
{
  THUNAR_METAFILE_KEY_EMBLEMS,
  THUNAR_METAFILE_N_KEYS,
} ThunarMetafileKey;

GType           thunar_metafile_get_type    (void) G_GNUC_CONST;

ThunarMetafile *thunar_metafile_get_default (void);

const gchar    *thunar_metafile_fetch       (ThunarMetafile   *metafile,
                                             GFile            *file,
                                             ThunarMetafileKey key,
                                             const gchar      *default_value);

void            thunar_metafile_store       (ThunarMetafile   *metafile,
                                             GFile            *file,
                                             ThunarMetafileKey key,
                                             const gchar      *value,
                                             const gchar      *default_value);

G_END_DECLS;

#endif /* !__THUNAR_METAFILE_H__ */
