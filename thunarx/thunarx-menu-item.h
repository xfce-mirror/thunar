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

#if !defined(THUNARX_INSIDE_THUNARX_H) && !defined(THUNARX_COMPILATION)
#error "Only <thunarx/thunarx.h> can be included directly, this file may disappear or change contents"
#endif

#ifndef __THUNARX_MENU_ITEM_H__
#define __THUNARX_MENU_ITEM_H__

#include <glib-object.h>

G_BEGIN_DECLS;

typedef struct _ThunarxMenuItemPrivate ThunarxMenuItemPrivate;
typedef struct _ThunarxMenuItemClass   ThunarxMenuItemClass;
typedef struct _ThunarxMenuItem        ThunarxMenuItem;

#define THUNARX_TYPE_MENU_ITEM            (thunarx_menu_item_get_type ())
#define THUNARX_MENU_ITEM(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNARX_TYPE_MENU_ITEM, ThunarxMenuItem))
#define THUNARX_MENU_ITEM_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNARX_TYPE_MENU_ITEM, ThunarxMenuItemClass))
#define THUNARX_IS_MENU_ITEM(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNARX_TYPE_MENU_ITEM))
#define THUNARX_IS_MENU_ITEM_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((obj), THUNARX_TYPE_MENU_ITEM))
#define THUNARX_MENU_ITEM_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS((obj), THUNARX_TYPE_MENU_ITEM))

struct _ThunarxMenuItemClass {
  GObjectClass __parent__;
  void (*activate) (ThunarxMenuItem *item);
};

struct _ThunarxMenuItem {
  GObject __parent__;

  /*< private >*/
  ThunarxMenuItemPrivate *priv;
};

GType             thunarx_menu_item_get_type      (void) G_GNUC_CONST;

ThunarxMenuItem  *thunarx_menu_item_new           (const gchar     *name,
                                                   const gchar     *label,
                                                   const gchar     *tip,
                                                   const gchar     *icon) G_GNUC_MALLOC;

void              thunarx_menu_item_activate      (ThunarxMenuItem *item);

gboolean          thunarx_menu_item_get_sensitive (ThunarxMenuItem *item);
void              thunarx_menu_item_set_sensitive (ThunarxMenuItem *item,
                                                   gboolean         sensitive)

G_END_DECLS;

#endif /* !__THUNARX_MENU_ITEM_H__ */
