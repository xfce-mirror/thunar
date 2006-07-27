/* $Id$ */
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

#ifndef __THUNAR_TPA_ICON_H__
#define __THUNAR_TPA_ICON_H__

#include <exo/exo.h>

G_BEGIN_DECLS;

typedef struct _ThunarTpaIconClass ThunarTpaIconClass;
typedef struct _ThunarTpaIcon      ThunarTpaIcon;

#define THUNAR_TPA_TYPE_ICON            (thunar_tpa_icon_get_type ())
#define THUNAR_TPA_ICON(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TPA_TYPE_ICON, ThunarTpaIcon))
#define THUNAR_TPA_ICON_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TPA_TYPE_ICON, ThunarTpaIconClass))
#define THUNAR_TPA_IS_ICON(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TPA_TYPE_ICON))
#define THUNAR_TPA_IS_ICON_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TPA_TYPE_ICON))
#define THUNAR_TPA_ICON_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TPA_TYPE_ICON, ThunarTpaIconClass))

GType      thunar_tpa_icon_get_type       (void) G_GNUC_CONST;

GtkWidget *thunar_tpa_icon_new            (void) G_GNUC_MALLOC;

void       thunar_tpa_icon_set_size       (ThunarTpaIcon *icon,
                                           gint           size);

void       thunar_tpa_icon_display_trash  (ThunarTpaIcon *icon);
void       thunar_tpa_icon_empty_trash    (ThunarTpaIcon *icon);
gboolean   thunar_tpa_icon_move_to_trash  (ThunarTpaIcon *icon,
                                           const gchar  **uri_list);
void       thunar_tpa_icon_query_trash    (ThunarTpaIcon *icon);

G_END_DECLS;

#endif /* !__THUNAR_TPA_ICON_H__ */
