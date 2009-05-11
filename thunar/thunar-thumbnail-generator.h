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

#ifndef __THUNAR_THUMBNAIL_GENERATOR_H__
#define __THUNAR_THUMBNAIL_GENERATOR_H__

#include <thunar-vfs/thunar-vfs.h>

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarThumbnailGeneratorClass ThunarThumbnailGeneratorClass;
typedef struct _ThunarThumbnailGenerator      ThunarThumbnailGenerator;

#define THUNAR_TYPE_THUMBNAIL_GENERATOR             (thunar_thumbnail_generator_get_type ())
#define THUNAR_THUMBNAIL_GENERATOR(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_THUMBNAIL_GENERATOR, ThunarThumbnailGenerator))
#define THUNAR_THUMBNAIL_GENERATOR_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_THUMBNAIL_GENERATOR, ThunarThumbnailGeneratorClass))
#define THUNAR_IS_THUMBNAIL_GENERATOR(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_THUMBNAIL_GENERATOR))
#define THUNAR_IS_THUMBNAIL_GENERATOR_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_THUMBNAIL_GENERATOR))
#define THUNAR_THUMBNAIL_GENERATOR_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_THUMBNAIL_GENERATOR, ThunarThumbnailGeneratorClass))

GType                     thunar_thumbnail_generator_get_type (void) G_GNUC_CONST;

ThunarThumbnailGenerator *thunar_thumbnail_generator_new      (ThunarVfsThumbFactory    *factory) G_GNUC_MALLOC;

void                      thunar_thumbnail_generator_enqueue  (ThunarThumbnailGenerator *generator,
                                                               ThunarFile               *file);

G_END_DECLS;

#endif /* !__THUNAR_THUMBNAIL_GENERATOR_H__ */
