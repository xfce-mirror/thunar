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

#ifndef __THUNAR_DESKTOP_VIEW_H__
#define __THUNAR_DESKTOP_VIEW_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _ThunarDesktopViewClass ThunarDesktopViewClass;
typedef struct _ThunarDesktopView      ThunarDesktopView;

#define THUNAR_TYPE_DESKTOP_VIEW            (thunar_desktop_view_get_type ())
#define THUNAR_DESKTOP_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_DESKTOP_VIEW, ThunarDesktopView))
#define THUNAR_DESKTOP_VIEW_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_DESKTOP_VIEW, ThunarDesktopViewClass))
#define THUNAR_IS_DESKTOP_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_DESKTOP_VIEW))
#define THUNAR_IS_DESKTOP_VIEW_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_DESKTOP_VIEW))
#define THUNAR_DESKTOP_VIEW_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_DESKTOP_VIEW, ThunarDesktopView))

GType         thunar_desktop_view_get_type             (void) G_GNUC_CONST;

GtkWidget    *thunar_desktop_view_new                  (void);

gint          thunar_desktop_view_get_icon_size        (ThunarDesktopView *view);
void          thunar_desktop_view_set_icon_size        (ThunarDesktopView *view,
                                                        gint               icon_size);

GtkTreeModel *thunar_desktop_view_get_model            (ThunarDesktopView *view);
void          thunar_desktop_view_set_model            (ThunarDesktopView *view,
                                                        GtkTreeModel      *model);

gint          thunar_desktop_view_get_file_column      (ThunarDesktopView *view);
void          thunar_desktop_view_set_file_column      (ThunarDesktopView *view,
                                                        gint               file_column);

gint          thunar_desktop_view_get_position_column  (ThunarDesktopView *view);
void          thunar_desktop_view_set_position_column  (ThunarDesktopView *view,
                                                        gint               position_column);

void          thunar_desktop_view_unselect_all         (ThunarDesktopView *view);

G_END_DECLS;

#endif /* !__THUNAR_DESKTOP_VIEW_H__ */
