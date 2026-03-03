/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2025 Jeff Guy <jeffguy77@gmail.com>
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

#ifndef __THUNAR_FILTER_BAR_H__
#define __THUNAR_FILTER_BAR_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS;

typedef struct _ThunarFilterBarClass ThunarFilterBarClass;
typedef struct _ThunarFilterBar      ThunarFilterBar;

#define THUNAR_TYPE_FILTER_BAR (thunar_filter_bar_get_type ())
#define THUNAR_FILTER_BAR(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_FILTER_BAR, ThunarFilterBar))
#define THUNAR_FILTER_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_FILTER_BAR, ThunarFilterBarClass))
#define THUNAR_IS_FILTER_BAR(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_FILTER_BAR))
#define THUNAR_IS_FILTER_BAR_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_FILTER_BAR))
#define THUNAR_FILTER_BAR_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_FILTER_BAR, ThunarFilterBarClass))

GType
thunar_filter_bar_get_type (void) G_GNUC_CONST;

GtkWidget *
thunar_filter_bar_new (void);

const gchar *
thunar_filter_bar_get_filter_text (ThunarFilterBar *filter_bar);

void
thunar_filter_bar_set_filter_text (ThunarFilterBar *filter_bar,
                                   const gchar     *text);

void
thunar_filter_bar_focus_entry (ThunarFilterBar *filter_bar);

G_END_DECLS;

#endif /* !__THUNAR_FILTER_BAR_H__ */
