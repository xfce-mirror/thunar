/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2022 Pratyaksh Gautam <pratyakshgautam11@gmail.com>
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

#ifndef __THUNAR_ABSTRACT_GALLERY_VIEW_H__
#define __THUNAR_ABSTRACT_GALLERY_VIEW_H__

#include <thunar/thunar-standard-view.h>

G_BEGIN_DECLS;

typedef struct _ThunarAbstractGalleryViewPrivate ThunarAbstractGalleryViewPrivate;
typedef struct _ThunarAbstractGalleryViewClass   ThunarAbstractGalleryViewClass;
typedef struct _ThunarAbstractGalleryView        ThunarAbstractGalleryView;

#define THUNAR_TYPE_ABSTRACT_GALLERY_VIEW             (thunar_abstract_gallery_view_get_type ())
#define THUNAR_ABSTRACT_GALLERY_VIEW(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_ABSTRACT_GALLERY_VIEW, ThunarAbstractGalleryView))
#define THUNAR_ABSTRACT_GALLERY_VIEW_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_ABSTRACT_GALLERY_VIEW, ThunarAbstractGalleryViewClass))
#define THUNAR_IS_ABSTRACT_GALLERY_VIEW(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_ABSTRACT_GALLERY_VIEW))
#define THUNAR_IS_ABSTRACT_GALLERY_VIEW_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((obj), THUNAR_TYPE_ABSTRACT_GALLERY_VIEW))
#define THUNAR_ABSTRACT_GALLERY_VIEW_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_ABSTRACT_GALLERY_VIEW, ThunarAbstractGalleryViewClass))

struct _ThunarAbstractGalleryViewClass
{
  ThunarStandardViewClass __parent__;
};

struct _ThunarAbstractGalleryView
{
  ThunarStandardView             __parent__;
  ThunarAbstractGalleryViewPrivate *priv;
};

GType thunar_abstract_gallery_view_get_type (void) G_GNUC_CONST;

G_END_DECLS;

#endif /* !__THUNAR_ABSTRACT_GALLERY_VIEW_H__ */
