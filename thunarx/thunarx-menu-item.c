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

#include <config.h>
#include <glib/gi18n-lib.h>

#include "thunarx-menu-item.h"

enum
{
    ACTIVATE,
    LAST_SIGNAL
};

enum
{
    PROP_0,
    PROP_NAME,
    PROP_LABEL,
    PROP_TOOLTIP,
    PROP_ICON,
    PROP_SENSITIVE,
    PROP_PRIORITY,
    PROP_MENU,
    LAST_PROP
};


static void        thunarx_menu_item_finalize         (GObject    *object);
static void        thunarx_menu_item_get_property     (GObject    *object,
                                                       guint       param_id,
                                                       GValue     *value,
                                                       GParamSpec *pspec);
static void        thunarx_menu_item_set_property     (GObject      *object,
                                                       guint         param_id,
                                                       const GValue *value,
                                                       GParamSpec   *pspec);

struct _ThunarxMenuItemDetails
{
    char *name;
    char *label;
    char *tooltip;
    char *icon;
    gboolean sensitive;
    gboolean priority;
};

static guint signals[LAST_SIGNAL];

static GObjectClass *parent_class = NULL;


G_DEFINE_TYPE (ThunarxMenuItem, thunarx_menu_item, G_TYPE_OBJECT)



static void
thunarx_menu_item_class_init (ThunarxMenuItemClass *class)
{
    parent_class = g_type_class_peek_parent (class);

    G_OBJECT_CLASS (class)->finalize = thunarx_menu_item_finalize;
    G_OBJECT_CLASS (class)->get_property = thunarx_menu_item_get_property;
    G_OBJECT_CLASS (class)->set_property = thunarx_menu_item_set_property;

    signals[ACTIVATE] =
        g_signal_new ("activate",
                      G_TYPE_FROM_CLASS (class),
                      G_SIGNAL_RUN_LAST,
                      G_STRUCT_OFFSET (ThunarxMenuItemClass,
                                       activate),
                      NULL, NULL,
                      g_cclosure_marshal_VOID__VOID,
                      G_TYPE_NONE, 0);

    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_NAME,
                                     g_param_spec_string ("name",
                                                          "Name",
                                                          "Name of the item",
                                                          NULL,
                                                          G_PARAM_CONSTRUCT_ONLY | G_PARAM_WRITABLE | G_PARAM_READABLE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_LABEL,
                                     g_param_spec_string ("label",
                                                          "Label",
                                                          "Label to display to the user",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_TOOLTIP,
                                     g_param_spec_string ("tooltip",
                                                          "Tooltip",
                                                          "Tooltip for the menu item",
                                                          NULL,
                                                          G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_ICON,
                                     g_param_spec_string ("icon",
                                                          "Icon",
                                                          "Name of the icon to display in the menu item",
                                                          NULL,
                                                          G_PARAM_READWRITE));

    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_SENSITIVE,
                                     g_param_spec_boolean ("sensitive",
                                                           "Sensitive",
                                                           "Whether the menu item is sensitive",
                                                           TRUE,
                                                           G_PARAM_READWRITE));
    g_object_class_install_property (G_OBJECT_CLASS (class),
                                     PROP_PRIORITY,
                                     g_param_spec_boolean ("priority",
                                                           "Priority",
                                                           "Show priority text in toolbars",
                                                           TRUE,
                                                           G_PARAM_READWRITE));
}



static void
thunarx_menu_item_init (ThunarxMenuItem *item)
{
    item->details = g_new0 (ThunarxMenuItemDetails, 1);
    item->details->sensitive = TRUE;
}



/**
 * thunarx_menu_item_new:
 * @name: identifier for the menu item
 * @label: user-visible label of the menu item
 * @tooltip: tooltip of the menu item
 * @icon: path or name of the icon to display in the menu item
 *
 * Creates a new menu item that can be added to the toolbar or to a contextual menu.
 *
 * Returns: a newly created #ThunarxMenuItem
 */
ThunarxMenuItem *
thunarx_menu_item_new (const char *name,
                       const char *label,
                       const char *tooltip,
                       const char *icon)
{
    ThunarxMenuItem *item;

    g_return_val_if_fail (name != NULL, NULL);
    g_return_val_if_fail (label != NULL, NULL);

    item = g_object_new (THUNARX_TYPE_MENU_ITEM,
                         "name", name,
                         "label", label,
                         "tooltip", tooltip,
                         "icon", icon,
                         NULL);

    return item;
}



/**
 * thunarx_menu_item_activate:
 * @item: pointer to a #ThunarxMenuItem instance
 *
 * emits the activate signal.
 */
void
thunarx_menu_item_activate (ThunarxMenuItem *item)
{
    g_signal_emit (item, signals[ACTIVATE], 0);
}



static void
thunarx_menu_item_get_property (GObject    *object,
                                 guint       param_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
    ThunarxMenuItem *item;

    item = THUNARX_MENU_ITEM (object);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_value_set_string (value, item->details->name);
        }
        break;

        case PROP_LABEL:
        {
            g_value_set_string (value, item->details->label);
        }
        break;

        case PROP_TOOLTIP:
        {
            g_value_set_string (value, item->details->tooltip);
        }
        break;

        case PROP_ICON:
        {
            g_value_set_string (value, item->details->icon);
        }
        break;

        case PROP_SENSITIVE:
        {
            g_value_set_boolean (value, item->details->sensitive);
        }
        break;

        case PROP_PRIORITY:
        {
            g_value_set_boolean (value, item->details->priority);
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        }
        break;
    }
}



static void
thunarx_menu_item_set_property (GObject      *object,
                                guint         param_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
    ThunarxMenuItem *item;

    item = THUNARX_MENU_ITEM (object);

    switch (param_id)
    {
        case PROP_NAME:
        {
            g_free (item->details->name);
            item->details->name = g_strdup (g_value_get_string (value));
            g_object_notify (object, "name");
        }
        break;

        case PROP_LABEL:
        {
            g_free (item->details->label);
            item->details->label = g_strdup (g_value_get_string (value));
            g_object_notify (object, "label");
        }
        break;

        case PROP_TOOLTIP:
        {
            g_free (item->details->tooltip);
            item->details->tooltip = g_strdup (g_value_get_string (value));
            g_object_notify (object, "tooltip");
        }
        break;

        case PROP_ICON:
        {
            g_free (item->details->icon);
            item->details->icon = g_strdup (g_value_get_string (value));
            g_object_notify (object, "icon");
        }
        break;

        case PROP_SENSITIVE:
        {
            item->details->sensitive = g_value_get_boolean (value);
            g_object_notify (object, "sensitive");
        }
        break;

        case PROP_PRIORITY:
        {
            item->details->priority = g_value_get_boolean (value);
            g_object_notify (object, "priority");
        }
        break;

        default:
        {
            G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
        }
        break;
    }
}



static void
thunarx_menu_item_finalize (GObject *object)
{
    ThunarxMenuItem *item;

    item = THUNARX_MENU_ITEM (object);

    g_free (item->details->name);
    g_free (item->details->label);
    g_free (item->details->tooltip);
    g_free (item->details->icon);

    g_free (item->details);

    G_OBJECT_CLASS (parent_class)->finalize (object);
}
