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

#ifndef __THUNAR_VIEW_H__
#define __THUNAR_VIEW_H__

#include <thunar/thunar-list-model.h>

G_BEGIN_DECLS;

typedef struct _ThunarViewIface ThunarViewIface;
typedef struct _ThunarView      ThunarView;

#define THUNAR_TYPE_VIEW            (thunar_view_get_type ())
#define THUNAR_VIEW(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_VIEW, ThunarView))
#define THUNAR_IS_VIEW(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_VIEW))
#define THUNAR_VIEW_GET_IFACE(obj)  (G_TYPE_INSTANCE_GET_INTERFACE ((obj), THUNAR_TYPE_VIEW, ThunarViewIface))

struct _ThunarViewIface
{
  GTypeInterface __parent__;

  /* methods */
  ThunarListModel*  (*get_model)              (ThunarView      *view);
  void              (*set_model)              (ThunarView      *view,
                                               ThunarListModel *model);
  GList            *(*get_selected_items)     (ThunarView      *view);

  /* signals */
  void              (*file_activated)         (ThunarView      *view,
                                               ThunarFile      *file);
  void              (*file_selection_changed) (ThunarView      *view);
};

GType            thunar_view_get_type               (void) G_GNUC_CONST;

ThunarListModel *thunar_view_get_model              (ThunarView      *view);
void             thunar_view_set_model              (ThunarView      *view,
                                                     ThunarListModel *model);
GList           *thunar_view_get_selected_files     (ThunarView      *view);
GList           *thunar_view_get_selected_items     (ThunarView      *view);

void             thunar_view_file_activated         (ThunarView      *view,
                                                     ThunarFile      *file);
void             thunar_view_file_selection_changed (ThunarView      *view);

G_END_DECLS;

#endif /* !__THUNAR_VIEW_H__ */
