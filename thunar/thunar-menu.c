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
#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-menu.h>

#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-launcher.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-window.h>



/**
 * SECTION:thunar-menu
 * @Short_description: Wrapper of GtkMenu to simplify the creation commonly used menu-sections in thunar
 * @Title: ThunarMenu
 *
 * #ThunarMenu is a #GtkMenu which provides a unified menu-creation service for different thunar widgets.
 *
 * Based on the passed flags and selected sections, it fills itself with the requested menu-items
 * by creating them with #ThunarLauncher.
 */



/* property identifiers */
enum
{
  PROP_0,
  PROP_MENU_TYPE,
  PROP_LAUNCHER,
  PROP_FORCE_SECTION_OPEN,
  PROP_TAB_SUPPORT_DISABLED,
  PROP_CHANGE_DIRECTORY_SUPPORT_DISABLED,
};

static void thunar_menu_finalize      (GObject                *object);
static void thunar_menu_get_property  (GObject                *object,
                                       guint                   prop_id,
                                       GValue                 *value,
                                       GParamSpec             *pspec);
static void thunar_menu_set_property  (GObject                *object,
                                       guint                   prop_uid,
                                       const GValue           *value,
                                       GParamSpec             *pspec);

struct _ThunarMenuClass
{
  GtkMenuClass __parent__;
};

struct _ThunarMenu
{
  GtkMenu __parent__;
  ThunarLauncher  *launcher;

  /* true, if the 'open' section should be forced */
  gboolean         force_section_open;

  /* true, if 'open as new tab' should not be shown */
  gboolean         tab_support_disabled;

  /* true, if 'open' for folders, which would result in changing the directory, should not be shown */
  gboolean         change_directory_support_disabled;

  /* detailed type of the thunar menu */
  ThunarMenuType   type;
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
                                   PROP_LAUNCHER,
                                   g_param_spec_object ("launcher",
                                                        "launcher",
                                                        "launcher",
                                                        THUNAR_TYPE_LAUNCHER,
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

  g_object_unref (menu->launcher);

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

    case PROP_LAUNCHER:
      menu->launcher = g_value_dup_object (value);
      g_object_ref (G_OBJECT (menu->launcher));
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
thunar_menu_add_sections (ThunarMenu         *menu,
                          ThunarMenuSections  menu_sections)
{
  GtkWidget *window;
  gboolean   item_added;
  gboolean   force = menu->type == THUNAR_MENU_TYPE_WINDOW || menu->type == THUNAR_MENU_TYPE_CONTEXT_TREE_VIEW;

  _thunar_return_val_if_fail (THUNAR_IS_MENU (menu), FALSE);

  if (menu_sections & THUNAR_MENU_SECTION_CREATE_NEW_FILES)
    {
      item_added = FALSE;
      item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_CREATE_FOLDER, force) != NULL);

      /* No document creation for tree-view */
      if (menu->type != THUNAR_MENU_TYPE_CONTEXT_TREE_VIEW)
        item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_CREATE_DOCUMENT, force) != NULL);
      if (item_added)
         xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (menu));
    }

  if (menu_sections & THUNAR_MENU_SECTION_OPEN)
    {
      if (thunar_launcher_append_open_section (menu->launcher, GTK_MENU_SHELL (menu), !menu->tab_support_disabled, !menu->change_directory_support_disabled, menu->force_section_open))
         xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (menu));
    }

  if (menu_sections & THUNAR_MENU_SECTION_SENDTO)
    {
      if (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_SENDTO_MENU, FALSE) != NULL)
         xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (menu));
    }

  item_added = FALSE;
  if (menu_sections & THUNAR_MENU_SECTION_CUT)
    item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_CUT, force) != NULL);
  if (menu_sections & THUNAR_MENU_SECTION_COPY_PASTE)
    {
      item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_COPY, force) != NULL);
      if (menu->type == THUNAR_MENU_TYPE_CONTEXT_LOCATION_BUTTONS)
        item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_PASTE_INTO_FOLDER, force) != NULL);
      else
        item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_PASTE, force) != NULL);
    }
  if (item_added)
     xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (menu));

  if (menu_sections & THUNAR_MENU_SECTION_TRASH_DELETE)
    {
      item_added = FALSE;
      item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_MOVE_TO_TRASH, force) != NULL);
      item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_DELETE, force) != NULL);
      if (item_added)
         xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (menu));
    }
  if (menu_sections & THUNAR_MENU_SECTION_EMPTY_TRASH)
    {
      if (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_EMPTY_TRASH, FALSE) != NULL )
         xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (menu));
    }
  if (menu_sections & THUNAR_MENU_SECTION_RESTORE)
    {
      if (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_RESTORE, FALSE) != NULL)
         xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (menu));
    }

  item_added = FALSE;
  if (menu_sections & THUNAR_MENU_SECTION_DUPLICATE)
    item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_DUPLICATE, force) != NULL);
  if (menu_sections & THUNAR_MENU_SECTION_MAKELINK)
    item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_MAKE_LINK, force) != NULL);
  if (menu_sections & THUNAR_MENU_SECTION_RENAME)
    item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_RENAME, force) != NULL);
  if (item_added)
     xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (menu));

  if (menu_sections & THUNAR_MENU_SECTION_CUSTOM_ACTIONS)
    {
      if (thunar_launcher_append_custom_actions (menu->launcher, GTK_MENU_SHELL (menu)))
         xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (menu));
    }

  if (menu_sections & THUNAR_MENU_SECTION_MOUNTABLE)
    {
      item_added = FALSE;
      item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_MOUNT, FALSE) != NULL);
      item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_UNMOUNT, FALSE) != NULL);
      item_added |= (thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_EJECT, FALSE) != NULL);
      if (item_added)
         xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (menu));
    }

  if (menu_sections & THUNAR_MENU_SECTION_ZOOM)
    {
      window = thunar_launcher_get_widget (menu->launcher);
      if (THUNAR_IS_WINDOW (window))
        {
          thunar_window_append_menu_item (THUNAR_WINDOW (window), GTK_MENU_SHELL (menu), THUNAR_WINDOW_ACTION_ZOOM_IN);
          thunar_window_append_menu_item (THUNAR_WINDOW (window), GTK_MENU_SHELL (menu), THUNAR_WINDOW_ACTION_ZOOM_OUT);
          thunar_window_append_menu_item (THUNAR_WINDOW (window), GTK_MENU_SHELL (menu), THUNAR_WINDOW_ACTION_ZOOM_RESET);
          xfce_gtk_menu_append_seperator (GTK_MENU_SHELL (menu));
        }
    }

  if (menu_sections & THUNAR_MENU_SECTION_PROPERTIES)
      thunar_launcher_append_menu_item (menu->launcher, GTK_MENU_SHELL (menu), THUNAR_LAUNCHER_ACTION_PROPERTIES, FALSE);

  return TRUE;
}



/**
 * thunar_menu_get_launcher:
 * @menu : a #ThunarMenu instance
 *
 * Return value: (transfer none): The launcher of this #ThunarMenu instance
 **/
GtkWidget*
thunar_menu_get_launcher (ThunarMenu *menu)
{
  _thunar_return_val_if_fail (THUNAR_IS_MENU (menu), NULL);
  return GTK_WIDGET (menu->launcher);
}



/**
 * thunar_menu_hide_accel_labels:
 * @menu : a #ThunarMenu instance
 *
 * Will hide the accel_labels of all menu items of this menu
 **/
void
thunar_menu_hide_accel_labels (ThunarMenu *menu)
{
  GList *children, *lp;

  _thunar_return_if_fail (THUNAR_IS_MENU (menu));

  children = gtk_container_get_children (GTK_CONTAINER (menu));
  for (lp = children; lp != NULL; lp = lp->next)
    xfce_gtk_menu_item_set_accel_label (lp->data, NULL);
  g_list_free (children);
}
