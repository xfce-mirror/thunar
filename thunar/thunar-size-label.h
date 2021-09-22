/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_SIZE_LABEL_H__
#define __THUNAR_SIZE_LABEL_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarSizeLabelClass ThunarSizeLabelClass;
typedef struct _ThunarSizeLabel      ThunarSizeLabel;

#define THUNAR_TYPE_SIZE_LABEL            (thunar_size_label_get_type ())
#define THUNAR_SIZE_LABEL(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_SIZE_LABEL, ThunarSizeLabel))
#define THUNAR_SIZE_LABEL_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_SIZE_LABEL, ThunarSizeLabelClass))
#define THUNAR_IS_SIZE_LABEL(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_SIZE_LABEL))
#define THUNAR_IS_SIZE_LABEL_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_SIZE_LABEL))
#define THUNAR_SIZE_LABEL_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_SIZE_LABEL, ThunarSizeLabelClass))

typedef enum
{
    THUNAR_SIZE_LABEL_SIZE,
    THUNAR_SIZE_LABEL_CONTENT,
    N_THUNAR_SIZE_LABEL
} ThunarSizeLabelType;


GType       thunar_size_label_get_type  (void) G_GNUC_CONST;

GtkWidget  *thunar_size_label_new       (void) G_GNUC_MALLOC;
GtkWidget  *thunar_content_label_new    (void) G_GNUC_MALLOC;

G_END_DECLS;

#endif /* !__THUNAR_SIZE_LABEL_H__ */
