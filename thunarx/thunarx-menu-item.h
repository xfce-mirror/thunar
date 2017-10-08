/*-
 * Copyright (c) 2017 Andre Miranda <andreldm@xfce.org>
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

#ifndef THUNARX_MENU_ITEM_H
#define THUNARX_MENU_ITEM_H

#include <glib-object.h>

#define THUNARX_TYPE_MENU_ITEM            (thunarx_menu_item_get_type())
#define THUNARX_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNARX_TYPE_MENU_ITEM, ThunarxMenuItem))
#define THUNARX_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNARX_TYPE_MENU_ITEM, ThunarxMenuItemClass))
#define THUNARX_IS_MENU_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNARX_TYPE_MENU_ITEM))
#define THUNARX_IS_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), THUNARX_TYPE_MENU_ITEM))
#define THUNARX_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), THUNARX_TYPE_MENU_ITEM, ThunarxMenuItemClass))

typedef struct _ThunarxMenuItem        ThunarxMenuItem;
typedef struct _ThunarxMenuItemDetails ThunarxMenuItemDetails;
typedef struct _ThunarxMenuItemClass   ThunarxMenuItemClass;

struct _ThunarxMenuItem {
	GObject parent;
	ThunarxMenuItemDetails *details;
};

struct _ThunarxMenuItemClass {
	GObjectClass parent;
	void (*activate) (ThunarxMenuItem *item);
};

GType             thunarx_menu_item_get_type      (void);
ThunarxMenuItem  *thunarx_menu_item_new           (const char      *name,
                                                   const char      *label,
                                                   const char      *tip,
                                                   const char      *icon);

void              thunarx_menu_item_activate      (ThunarxMenuItem *item);

#endif
