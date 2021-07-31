/* vi:set et ai sw=2 sts=2 ts=2: */
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

#ifndef __THUNAR_LOCATION_BAR_H__
#define __THUNAR_LOCATION_BAR_H__

#include <thunar/thunar-component.h>

G_BEGIN_DECLS;

typedef struct _ThunarLocationBarClass ThunarLocationBarClass;
typedef struct _ThunarLocationBar      ThunarLocationBar;

#define THUNAR_TYPE_LOCATION_BAR            (thunar_location_bar_get_type ())
#define THUNAR_LOCATION_BAR(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_LOCATION_BAR, ThunarLocationBar))
#define THUNAR_IS_LOCATION_BAR(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_LOCATION_BAR))
#define THUNAR_LOCATION_BAR_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_LOCATION_BAR, ThunarLocationBarClass))


GType      thunar_location_bar_get_type      (void) G_GNUC_CONST;

GtkWidget *thunar_location_bar_new           (void);

void       thunar_location_bar_request_entry (ThunarLocationBar *bar, const gchar *initial_text);

void       thunar_location_bar_cancel_search (ThunarLocationBar *bar);

gchar*     thunar_location_bar_get_search_query (ThunarLocationBar *entry);

G_END_DECLS;

#endif /* !__THUNAR_LOCATION_BAR_H__ */
