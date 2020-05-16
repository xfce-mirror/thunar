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

#ifndef __THUNAR_GTK_EXTENSIONS_H__
#define __THUNAR_GTK_EXTENSIONS_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS;


void             thunar_gtk_label_set_a11y_relation           (GtkLabel           *label,
                                                               GtkWidget          *widget);
void             thunar_gtk_menu_run                          (GtkMenu            *menu);

void             thunar_gtk_menu_run_at_event                 (GtkMenu            *menu,
                                                               GdkEvent           *event);
void             thunar_gtk_widget_set_tooltip                (GtkWidget          *widget,
                                                               const gchar        *format,
                                                               ...) G_GNUC_PRINTF (2, 3);
GtkWidget       *thunar_gtk_menu_thunarx_menu_item_new        (GObject            *thunarx_menu_item,
                                                               GtkMenuShell       *menu_to_append_item);

GMountOperation *thunar_gtk_mount_operation_new               (gpointer            parent);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS

static inline int
thunar_gtk_widget_get_allocation_x (GtkWidget *widget)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation (widget, &allocation);

    return allocation.x;
}

static inline int
thunar_gtk_widget_get_allocation_y (GtkWidget *widget)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation (widget, &allocation);

    return allocation.y;
}

static inline int
thunar_gtk_widget_get_allocation_width (GtkWidget *widget)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation (widget, &allocation);

    return allocation.width;
}

static inline int
thunar_gtk_widget_get_allocation_height (GtkWidget *widget)
{
    GtkAllocation allocation;

    gtk_widget_get_allocation (widget, &allocation);

    return allocation.height;
}

static inline int
thunar_gtk_widget_get_requisition_width (GtkWidget *widget)
{
    GtkRequisition requisition;

    gtk_widget_get_requisition (widget, &requisition);

    return requisition.width;
}

static inline int
thunar_gtk_widget_get_requisition_height (GtkWidget *widget)
{
    GtkRequisition requisition;

    gtk_widget_get_requisition (widget, &requisition);

    return requisition.height;
}

G_GNUC_END_IGNORE_DEPRECATIONS

G_END_DECLS;

#endif /* !__THUNAR_GTK_EXTENSIONS_H__ */
