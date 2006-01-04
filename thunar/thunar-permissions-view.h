/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_PERMISSIONS_VIEW_H__
#define __THUNAR_PERMISSIONS_VIEW_H__

#include <thunar/thunar-file.h>

G_BEGIN_DECLS;

typedef struct _ThunarPermissionsViewClass ThunarPermissionsViewClass;
typedef struct _ThunarPermissionsView      ThunarPermissionsView;

#define THUNAR_TYPE_PERMISSIONS_VIEW            (thunar_permissions_view_get_type ())
#define THUNAR_PERMISSIONS_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_PERMISSIONS_VIEW, ThunarPermissionsView))
#define THUNAR_PERMISSIONS_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_PERMISSIONS_VIEW, ThunarPermissionsViewClass))
#define THUNAR_IS_PERMISSIONS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_PERMISSIONS_VIEW))
#define THUNAR_IS_PERMISSIONS_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_PERMISSIONS_VIEW))
#define THUNAR_PERMISSIONS_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_PERMISSIONS_VIEW, ThunarPermissionsViewClass))

GType       thunar_permissions_view_get_type (void) G_GNUC_CONST;

GtkWidget  *thunar_permissions_view_new      (void) G_GNUC_MALLOC;

ThunarFile *thunar_permissions_view_get_file (ThunarPermissionsView *view);
void        thunar_permissions_view_set_file (ThunarPermissionsView *view,
                                              ThunarFile            *file);

G_END_DECLS;

#endif /* !__THUNAR_PERMISSIONS_VIEW_H__ */
