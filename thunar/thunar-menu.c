/*-
 * Copyright (c) 2020 Alexander Schwinn <alexxcons@xfce.org>
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
#include "thunar/thunar-menu.h"

#include "thunar/thunar-action-manager.h"
#include "thunar/thunar-context-menu-order-model.h"
#include "thunar/thunar-enum-types.h"
#include "thunar/thunar-gtk-extensions.h"
#include "thunar/thunar-private.h"
#include "thunar/thunar-util.h"
#include "thunar/thunar-window.h"



/**
 * SECTION:thunar-menu
 * @Short_description: Wrapper of GtkMenu to simplify the creation commonly used menu-sections in thunar
 * @Title: ThunarMenu
 *
 * #ThunarMenu is a #GtkMenu which provides a unified menu-creation service for different thunar widgets.
 *
 * Based on the passed flags and selected sections, it fills itself with the requested menu-items
 * by creating them with #ThunarActionManager.
 */



/* property identifiers */
enum
{
  PROP_0,
  PROP_MENU_TYPE,
  PROP_ACTION_MANAGER,
  PROP_FORCE_SECTION_OPEN,
  PROP_TAB_SUPPORT_DISABLED,
  PROP_CHANGE_DIRECTORY_SUPPORT_DISABLED,
};

static void
thunar_menu_finalize (GObject *object);
static void
thunar_menu_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec);
static void
thunar_menu_set_property (GObject      *object,
                          guint         prop_uid,
                          const GValue *value,
                          GParamSpec   *pspec);

struct _ThunarMenuClass
{
  GtkMenuClass __parent__;
};

struct _ThunarMenu
{
  GtkMenu              __parent__;
  ThunarActionManager *action_mgr;

  /* true, if the 'open' section should be forced */
  gboolean force_section_open;

  /* true, if 'open as new tab' should not be shown */
  gboolean tab_support_disabled;

  /* true, if 'open' for folders, which would result in changing the directory, should not be shown */
  gboolean change_directory_support_disabled;

  /* detailed type of the thunar menu */
  ThunarMenuType type;
};



static GQuark thunar_menu_handler_quark;

G_DEFINE_TYPE (ThunarMenu, thunar_menu, GTK_TYPE_MENU)



static void
thunar_menu_class_init (ThunarMenuClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the "thunar-menu-handler" quark */
  thunar_menu_handler_quark = g_quark_from_static_string ("thunar-menu-handler");

  gobject_class = G_OBJECT_CLASS (klass);

  gobject_class->finalize = thunar_menu_finalize;
  gobject_class->get_property = thunar_menu_get_property;
  gobject_class->set_property = thunar_menu_set_property;

  g_object_class_install_property (gobject_class,
                                   PROP_MENU_TYPE,
                                   g_param_spec_int ("menu-type",
                                                     "menu-type",
                                                     "menu-type",
                                                     0, N_THUNAR_MENU_TYPE - 1, 0, // min, max, default
                                                     G_PARAM_WRITABLE
                                                     | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                   PROP_ACTION_MANAGER,
                                   g_param_spec_object ("action_mgr",
                                                        "action_mgr",
                                                        "action_mgr",
                                                        THUNAR_TYPE_ACTION_MANAGER,
                                                        G_PARAM_WRITABLE
                                                        | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                   PROP_FORCE_SECTION_OPEN,
                                   g_param_spec_boolean ("force-section-open",
                                                         "force-section-open",
                                                         "force-section-open",
                                                         FALSE,
                                                         G_PARAM_WRITABLE
                                                         | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                   PROP_TAB_SUPPORT_DISABLED,
                                   g_param_spec_boolean ("tab-support-disabled",
                                                         "tab-support-disabled",
                                                         "tab-support-disabled",
                                                         FALSE,
                                                         G_PARAM_WRITABLE
                                                         | G_PARAM_CONSTRUCT_ONLY));

  g_object_class_install_property (gobject_class,
                                   PROP_CHANGE_DIRECTORY_SUPPORT_DISABLED,
                                   g_param_spec_boolean ("change_directory-support-disabled",
                                                         "change_directory-support-disabled",
                                                         "change_directory-support-disabled",
                                                         FALSE,
                                                         G_PARAM_WRITABLE
                                                         | G_PARAM_CONSTRUCT_ONLY));
}



static void
thunar_menu_init (ThunarMenu *menu)
{
  menu->force_section_open = FALSE;
  menu->type = FALSE;
  menu->tab_support_disabled = FALSE;
  menu->change_directory_support_disabled = FALSE;
}



static void
thunar_menu_finalize (GObject *object)
{
  ThunarMenu *menu = THUNAR_MENU (object);

  g_object_unref (menu->action_mgr);

  (*G_OBJECT_CLASS (thunar_menu_parent_class)->finalize) (object);
}



static void
thunar_menu_get_property (GObject    *object,
                          guint       prop_id,
                          GValue     *value,
                          GParamSpec *pspec)
{
  switch (prop_id)
    {
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_menu_set_property (GObject      *object,
                          guint         prop_id,
                          const GValue *value,
                          GParamSpec   *pspec)
{
  ThunarMenu *menu = THUNAR_MENU (object);

  switch (prop_id)
    {
    case PROP_MENU_TYPE:
      menu->type = g_value_get_int (value);
      break;

    case PROP_ACTION_MANAGER:
      menu->action_mgr = g_value_dup_object (value);
      break;

    case PROP_FORCE_SECTION_OPEN:
      menu->force_section_open = g_value_get_boolean (value);
      break;

    case PROP_TAB_SUPPORT_DISABLED:
      menu->tab_support_disabled = g_value_get_boolean (value);
      break;

    case PROP_CHANGE_DIRECTORY_SUPPORT_DISABLED:
      menu->change_directory_support_disabled = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_menu_remove_all_separators (ThunarMenu *menu)
{
  GList *children = gtk_container_get_children (GTK_CONTAINER (menu));

  for (GList *l = children; l != NULL; l = l->next)
    {
      GtkWidget *item = GTK_WIDGET (l->data);

      if (GTK_IS_SEPARATOR_MENU_ITEM (item))
        gtk_container_remove (GTK_CONTAINER (menu), item);
    }

  g_list_free (children);
}



static void
thunar_menu_insert_separators (ThunarMenu *menu)
{
  ThunarContextMenuOrderModel *order_model = thunar_context_menu_order_model_get_default ();
  GList                       *children = gtk_container_get_children (GTK_CONTAINER (menu));
  GList                       *items = thunar_context_menu_order_model_get_items (order_model);
  gboolean                     allow_separator = FALSE;
  gint                         index = 0;

  /* Inserts separators as in ThunarContextMenuOrderModel. If several consecutive separators
   * are encountered, only one is inserted. If the elements above and below the separator do
   * not exist in this menu, the separator will still be inserted. */
  for (GList *li = children, *lj = items; li != NULL && lj != NULL; lj = lj->next)
    {
      GtkWidget                       *child = GTK_WIDGET (li->data);
      ThunarContextMenuItem            child_id;
      const gchar                     *child_secondary_id;
      ThunarContextMenuOrderModelItem *item = lj->data;

      if (item->id == THUNAR_CONTEXT_MENU_ITEM_SEPARATOR && item->visibility)
        {
          if (allow_separator)
            {
              GtkWidget *separator = gtk_separator_menu_item_new ();

              gtk_menu_shell_insert (GTK_MENU_SHELL (menu), separator, index++);
              allow_separator = FALSE;
            }
        }
      else
        {
          if (g_object_get_data (G_OBJECT (child), "id") == NULL)
            continue;

          child_id = thunar_context_menu_item_get_id (child);
          child_secondary_id = g_object_get_data (G_OBJECT (child), "secondary-id");

          if (item->id == child_id && g_strcmp0 (item->secondary_id, child_secondary_id) == 0)
            {
              allow_separator = TRUE;
              li = li->next;
              ++index;
            }
        }
    }

  g_list_free (children);
  g_list_free (items);
  g_object_unref (order_model);
}



static void
thunar_menu_reorder (ThunarMenu *menu)
{
  ThunarContextMenuOrderModel *order_model = thunar_context_menu_order_model_get_default ();
  GList                       *new_order = thunar_context_menu_order_model_get_items (order_model);
  GList                       *children = gtk_container_get_children (GTK_CONTAINER (menu));
  gint                         index = 0;

  /* Changes the order of elements to the one specified in ThunarContextMenuOrderModel. It also removes
   * elements that should be invisible. */
  for (GList *l = new_order; l != NULL; l = l->next, ++index)
    {
      const ThunarContextMenuOrderModelItem *item = l->data;
      gboolean                               any_removed = FALSE;

      /* reverse order, so that inserting several elements with the same id does not invert their order */
      for (GList *child = g_list_last (children); child != NULL; child = child->prev)
        {
          GtkWidget            *menu_item = GTK_WIDGET (child->data);
          ThunarContextMenuItem menu_item_id;
          const gchar          *menu_item_secondary_id;

          if (g_object_get_data (G_OBJECT (menu_item), "id") == NULL)
            continue;

          menu_item_id = thunar_context_menu_item_get_id (menu_item);
          menu_item_secondary_id = g_object_get_data (G_OBJECT (menu_item), "secondary-id");

          if (item->id == menu_item_id && g_strcmp0 (item->secondary_id, menu_item_secondary_id) == 0)
            {
              if (item->visibility)
                {
                  gtk_menu_reorder_child (GTK_MENU (menu), menu_item, index);
                }
              else
                {
                  gtk_container_remove (GTK_CONTAINER (menu), menu_item);
                  any_removed = TRUE;
                }
            }
        }

      if (any_removed)
        {
          g_list_free (children);
          children = gtk_container_get_children (GTK_CONTAINER (menu));
        }
    }

  g_list_free (new_order);
  g_list_free (children);
  g_object_unref (order_model);
}



/**
 * thunar_menu_add_sections:
 * @menu : a #ThunarMenu instance
 * @menu_sections : bit enumeration of #ThunarMenuSections which should be added to the #ThunarMenu
 *
 * Method to add different sections of #GtkMenuItems to the #ThunarMenu,
 * according to the selected #ThunarMenuSections
 *
 * Return value: TRUE if any #GtkMenuItem was added
 **/
gboolean
thunar_menu_add_sections (ThunarMenu        *menu,
                          ThunarMenuSections menu_sections)
{
  GtkWidget *window;
  gboolean   item_added;
  gboolean   force = menu->type == THUNAR_MENU_TYPE_WINDOW || menu->type == THUNAR_MENU_TYPE_CONTEXT_TREE_VIEW || menu->type == THUNAR_MENU_TYPE_CONTEXT_SHORTCUTS_VIEW;

  _thunar_return_val_if_fail (THUNAR_IS_MENU (menu), FALSE);

  if (menu_sections & THUNAR_MENU_SECTION_CREATE_NEW_FILES)
    {
      item_added = FALSE;
      item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_CREATE_FOLDER, force) != NULL);

      /* No document creation for side pane views */
      if (menu->type != THUNAR_MENU_TYPE_CONTEXT_TREE_VIEW && menu->type != THUNAR_MENU_TYPE_CONTEXT_SHORTCUTS_VIEW)
        item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_CREATE_DOCUMENT, force) != NULL);
      if (item_added)
        xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
    }

  if (menu_sections & THUNAR_MENU_SECTION_OPEN)
    {
      if (thunar_action_manager_append_open_section (menu->action_mgr, GTK_MENU_SHELL (menu), !menu->tab_support_disabled, !menu->change_directory_support_disabled, menu->force_section_open))
        xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
    }

  if (menu_sections & THUNAR_MENU_SECTION_SENDTO)
    {
      if (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_SENDTO_MENU, FALSE) != NULL)
        xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
    }

  item_added = FALSE;
  if (menu_sections & THUNAR_MENU_SECTION_CUT)
    item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_CUT, force) != NULL);
  if (menu_sections & THUNAR_MENU_SECTION_COPY_PASTE)
    {
      item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_COPY, force) != NULL);
      if (!(item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_PASTE_INTO_FOLDER, force) != NULL)))
        {
          item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_PASTE, force) != NULL);
          item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_PASTE_LINK, force) != NULL);
        }
      if (menu->type == THUNAR_MENU_TYPE_WINDOW)
        {
          item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_PASTE, force) != NULL);
          item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_PASTE_LINK, force) != NULL);
        }
    }
  if (item_added)
    xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));

  if (menu_sections & THUNAR_MENU_SECTION_TRASH_DELETE)
    {
      item_added = FALSE;
      item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_MOVE_TO_TRASH, force) != NULL);
      item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_DELETE, force) != NULL);
      if (item_added)
        xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
    }
  if (menu_sections & THUNAR_MENU_SECTION_EMPTY_TRASH)
    {
      if (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_EMPTY_TRASH, FALSE) != NULL)
        xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
    }

  item_added = FALSE;
  if (menu_sections & THUNAR_MENU_SECTION_DUPLICATE)
    item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_DUPLICATE, force) != NULL);
  if (menu_sections & THUNAR_MENU_SECTION_MAKELINK)
    item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_MAKE_LINK, force) != NULL);
  if (menu_sections & THUNAR_MENU_SECTION_RENAME)
    item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_RENAME, force) != NULL);
  if (item_added)
    xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));

  if (menu_sections & THUNAR_MENU_SECTION_RESTORE)
    {
      item_added = FALSE;
      item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_RESTORE, FALSE) != NULL);
      item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_RESTORE_SHOW, FALSE) != NULL);
      if (item_added)
        xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
    }
  if (menu_sections & THUNAR_MENU_SECTION_REMOVE_FROM_RECENT)
    {
      if (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_REMOVE_FROM_RECENT, FALSE) != NULL)
        xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
    }

  if (menu_sections & THUNAR_MENU_SECTION_CUSTOM_ACTIONS)
    {
      if (thunar_action_manager_append_custom_actions (menu->action_mgr, GTK_MENU_SHELL (menu)))
        xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
    }

  if (menu_sections & THUNAR_MENU_SECTION_MOUNTABLE)
    {
      item_added = FALSE;
      item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_MOUNT, FALSE) != NULL);
      item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_UNMOUNT, FALSE) != NULL);
      item_added |= (thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_EJECT, FALSE) != NULL);
      if (item_added)
        xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
    }

  if (menu_sections & THUNAR_MENU_SECTION_ZOOM)
    {
      window = thunar_action_manager_get_widget (menu->action_mgr);
      if (THUNAR_IS_WINDOW (window))
        {
          thunar_window_append_menu_item (THUNAR_WINDOW (window), GTK_MENU_SHELL (menu), THUNAR_WINDOW_ACTION_ZOOM_IN);
          thunar_window_append_menu_item (THUNAR_WINDOW (window), GTK_MENU_SHELL (menu), THUNAR_WINDOW_ACTION_ZOOM_OUT);
          thunar_window_append_menu_item (THUNAR_WINDOW (window), GTK_MENU_SHELL (menu), THUNAR_WINDOW_ACTION_ZOOM_RESET);
          xfce_gtk_menu_append_separator (GTK_MENU_SHELL (menu));
        }
    }

  if (menu_sections & THUNAR_MENU_SECTION_PROPERTIES)
    thunar_action_manager_append_menu_item (menu->action_mgr, GTK_MENU_SHELL (menu), THUNAR_ACTION_MANAGER_ACTION_PROPERTIES, FALSE);

  /* if this is a right-click context menu, then change the order and visibility of the elements to custom ones */
  if (menu->type == THUNAR_MENU_TYPE_CONTEXT_STANDARD_VIEW || menu->type == THUNAR_MENU_TYPE_CONTEXT_TREE_VIEW || menu->type == THUNAR_MENU_TYPE_CONTEXT_SHORTCUTS_VIEW)
    {
      thunar_menu_remove_all_separators (menu);
      thunar_menu_reorder (menu);
      thunar_menu_insert_separators (menu);
    }

  return TRUE;
}



/**
 * thunar_menu_get_action_manager:
 * @menu : a #ThunarMenu instance
 *
 * Return value: (transfer none): The action manager of this #ThunarMenu instance
 **/
GtkWidget *
thunar_menu_get_action_manager (ThunarMenu *menu)
{
  _thunar_return_val_if_fail (THUNAR_IS_MENU (menu), NULL);
  return GTK_WIDGET (menu->action_mgr);
}
