/* $Id$ */
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

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar-vfs/thunar-vfs.h>

#include <thunar/thunar-templates-action.h>



/* Signal identifiers */
enum
{
  CREATE_EMPTY_FILE,
  CREATE_TEMPLATE,
  LAST_SIGNAL,
};



static void       thunar_templates_action_class_init        (ThunarTemplatesActionClass *klass);
static void       thunar_templates_action_init              (ThunarTemplatesAction      *templates_action);
static void       thunar_templates_action_finalize          (GObject                    *object);
static GtkWidget *thunar_templates_action_create_menu_item  (GtkAction                  *action);
static void       thunar_templates_action_build_menu        (ThunarTemplatesAction      *templates_action,
                                                             GtkWidget                  *menu);
static void       thunar_templates_action_menu_mapped       (GtkWidget                  *menu,
                                                             ThunarTemplatesAction      *templates_action);
static void       thunar_templates_action_monitor           (ThunarVfsMonitor           *monitor,
                                                             ThunarVfsMonitorHandle     *handle,
                                                             ThunarVfsMonitorEvent       event,
                                                             ThunarVfsPath              *handle_path,
                                                             ThunarVfsPath              *event_path,
                                                             gpointer                    user_data);



struct _ThunarTemplatesActionClass
{
  GtkActionClass __parent__;

  void (*create_empty_file) (ThunarTemplatesAction *templates_action);
  void (*create_template)   (ThunarTemplatesAction *templates_action,
                             const ThunarVfsInfo   *info);
};

struct _ThunarTemplatesAction
{
  GtkAction __parent__;

  ThunarVfsMonitorHandle *templates_handle;
  ThunarVfsMonitor       *templates_monitor;
  ThunarVfsPath          *templates_path;
};



static GObjectClass *thunar_templates_action_parent_class;
static guint         templates_action_signals[LAST_SIGNAL];



GType
thunar_templates_action_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarTemplatesActionClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_templates_action_class_init,
        NULL,
        NULL,
        sizeof (ThunarTemplatesAction),
        0,
        (GInstanceInitFunc) thunar_templates_action_init,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_ACTION, I_("ThunarTemplatesAction"), &info, 0);
    }

  return type;
}



static void
thunar_templates_action_class_init (ThunarTemplatesActionClass *klass)
{
  GtkActionClass *gtkaction_class;
  GObjectClass   *gobject_class;

  /* determine the parent type class */
  thunar_templates_action_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_templates_action_finalize;

  gtkaction_class = GTK_ACTION_CLASS (klass);
  gtkaction_class->create_menu_item = thunar_templates_action_create_menu_item;

  /**
   * ThunarTemplatesAction::create-empty-file:
   * @templates_action : a #ThunarTemplatesAction.
   *
   * Emitted by @templates_action whenever the user requests to
   * create a new empty file.
   **/
  templates_action_signals[CREATE_EMPTY_FILE] =
    g_signal_new (I_("create-empty-file"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarTemplatesActionClass, create_empty_file),
                  NULL, NULL, g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ThunarTemplatesAction::create-template:
   * @templates_action : a #ThunarTemplatesAction.
   * @info             : the #ThunarVfsInfo of the template file.
   *
   * Emitted by @templates_action whenever the user requests to
   * create a new file based on the template referred to by
   * @info.
   **/
  templates_action_signals[CREATE_TEMPLATE] =
    g_signal_new (I_("create-template"),
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarTemplatesActionClass, create_template),
                  NULL, NULL, g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, THUNAR_VFS_TYPE_INFO);
}



static void
thunar_templates_action_init (ThunarTemplatesAction *templates_action)
{
  ThunarVfsPath *home_path;

  /* determine the path to the ~/Templates directory */
  home_path = thunar_vfs_path_get_for_home ();
  templates_action->templates_path = thunar_vfs_path_relative (home_path, "Templates");
  thunar_vfs_path_unref (home_path);
}



static void
thunar_templates_action_finalize (GObject *object)
{
  ThunarTemplatesAction *templates_action = THUNAR_TEMPLATES_ACTION (object);

  /* unregister from the monitor (if any) */
  if (G_LIKELY (templates_action->templates_monitor != NULL))
    {
      thunar_vfs_monitor_remove (templates_action->templates_monitor, templates_action->templates_handle);
      g_object_unref (G_OBJECT (templates_action->templates_monitor));
    }

  /* release the ~/Templates path */
  thunar_vfs_path_unref (templates_action->templates_path);

  (*G_OBJECT_CLASS (thunar_templates_action_parent_class)->finalize) (object);
}



static GtkWidget*
thunar_templates_action_create_menu_item (GtkAction *action)
{
  GtkWidget *item;
  GtkWidget *menu;

  g_return_val_if_fail (THUNAR_IS_TEMPLATES_ACTION (action), NULL);

  /* let GtkAction allocate the menu item */
  item = (*GTK_ACTION_CLASS (thunar_templates_action_parent_class)->create_menu_item) (action);

  /* associate an empty submenu with the item (will be filled when mapped) */
  menu = gtk_menu_new ();
  g_signal_connect (G_OBJECT (menu), "map", G_CALLBACK (thunar_templates_action_menu_mapped), action);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

  return item;
}



static gint
info_compare (gconstpointer info_a,
              gconstpointer info_b)
{
  gchar *name_a;
  gchar *name_b;
  gint   result;

  name_a = g_utf8_casefold (((ThunarVfsInfo *) info_a)->display_name, -1);
  name_b = g_utf8_casefold (((ThunarVfsInfo *) info_b)->display_name, -1);
  result = g_utf8_collate (name_a, name_b);
  g_free (name_b);
  g_free (name_a);

  return result;
}



static void
item_activated (GtkWidget             *item,
                ThunarTemplatesAction *templates_action)
{
  const ThunarVfsInfo *info;

  g_return_if_fail (THUNAR_IS_TEMPLATES_ACTION (templates_action));
  g_return_if_fail (GTK_IS_WIDGET (item));

  /* check if an info is set for the item (else it's the "Empty File" item) */
  info = g_object_get_data (G_OBJECT (item), I_("thunar-vfs-info"));
  if (G_UNLIKELY (info != NULL))
    g_signal_emit (G_OBJECT (templates_action), templates_action_signals[CREATE_TEMPLATE], 0, info);
  else
    g_signal_emit (G_OBJECT (templates_action), templates_action_signals[CREATE_EMPTY_FILE], 0);
}



static void
thunar_templates_action_build_menu (ThunarTemplatesAction *templates_action,
                                    GtkWidget             *menu)
{
  ThunarVfsInfo *info;
  ThunarVfsPath *path;
  GtkIconTheme  *icon_theme;
  const gchar   *icon_name;
  const gchar   *name;
  GtkWidget     *image;
  GtkWidget     *item;
  gchar         *absolute_path;
  gchar         *label;
  gchar         *dot;
  GList         *info_list = NULL;
  GList         *lp;
  GDir          *dp;

  /* try to open the ~/Templates directory */
  absolute_path = thunar_vfs_path_dup_string (templates_action->templates_path);
  dp = g_dir_open (absolute_path, 0, NULL);
  g_free (absolute_path);

  /* read the directory contents (if opened successfully) */
  if (G_LIKELY (dp != NULL))
    {
      /* process all files within the directory */
      for (;;)
        {
          /* read the name of the next file */
          name = g_dir_read_name (dp);
          if (G_UNLIKELY (name == NULL))
            break;
          else if (name[0] == '.')
            continue;

          /* determine the info for that file */
          path = thunar_vfs_path_relative (templates_action->templates_path, name);
          info = thunar_vfs_info_new_for_path (path, NULL);
          thunar_vfs_path_unref (path);

          /* add the info (if any) to our list */
          if (G_LIKELY (info != NULL))
            info_list = g_list_insert_sorted (info_list, info, info_compare);
        }

      /* close the directory handle */
      g_dir_close (dp);
    }

  /* check if we have any infos */
  if (G_LIKELY (info_list != NULL))
    {
      /* determine the icon theme for the mapped menu */
      icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (menu));

      /* add menu items for all infos */
      for (lp = info_list; lp != NULL; lp = lp->next)
        {
          /* determine the info */
          info = lp->data;

          /* generate a label by stripping off the extension */
          label = g_strdup (info->display_name);
          dot = g_utf8_strrchr (label, -1, '.');
          if (G_LIKELY (dot != NULL))
            *dot = '\0';

          /* allocate a new menu item */
          item = gtk_image_menu_item_new_with_label (label);
          g_object_set_data_full (G_OBJECT (item), I_("thunar-vfs-info"), info, (GDestroyNotify) thunar_vfs_info_unref);
          g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (item_activated), templates_action);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);

          /* lookup the icon for the mime type of that file */
          icon_name = thunar_vfs_mime_info_lookup_icon_name (info->mime_info, icon_theme);

          /* generate an image based on the named icon */
          image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
          gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
          gtk_widget_show (image);

          /* cleanup */
          g_free (label);
        }

      /* release the info list itself */
      g_list_free (info_list);
    }
  else
    {
      /* tell the user that no templates were found */
      item = gtk_menu_item_new_with_label (_("No Templates installed"));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_set_sensitive (item, FALSE);
      gtk_widget_show (item);
    }

  /* append a menu separator */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* add the "Empty File" item */
  item = gtk_image_menu_item_new_with_mnemonic (_("_Empty File"));
  g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (item_activated), templates_action);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* add the icon for the emtpy file item */
  image = gtk_image_new_from_stock (GTK_STOCK_NEW, GTK_ICON_SIZE_MENU);
  gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
  gtk_widget_show (image);
}



static void
thunar_templates_action_menu_mapped (GtkWidget             *menu,
                                     ThunarTemplatesAction *templates_action)
{
  g_return_if_fail (THUNAR_IS_TEMPLATES_ACTION (templates_action));
  g_return_if_fail (GTK_IS_MENU_SHELL (menu));

  /* check if we already added our children to the submenu */
  if (G_LIKELY (GTK_MENU_SHELL (menu)->children != NULL))
    return;

  /* generate the menu */
  thunar_templates_action_build_menu (templates_action, menu);

  /* check if we're already monitoring ~/Templates for changes */
  if (G_UNLIKELY (templates_action->templates_monitor == NULL))
    {
      /* grab a reference on the VFS monitor */
      templates_action->templates_monitor = thunar_vfs_monitor_get_default ();
      templates_action->templates_handle = thunar_vfs_monitor_add_directory (templates_action->templates_monitor,
                                                                             templates_action->templates_path,
                                                                             thunar_templates_action_monitor,
                                                                             templates_action);
    }
}



static void
thunar_templates_action_monitor (ThunarVfsMonitor       *monitor,
                                 ThunarVfsMonitorHandle *handle,
                                 ThunarVfsMonitorEvent   event,
                                 ThunarVfsPath          *handle_path,
                                 ThunarVfsPath          *event_path,
                                 gpointer                user_data)
{
  ThunarTemplatesAction *templates_action = THUNAR_TEMPLATES_ACTION (user_data);
  GtkWidget             *menu;
  GSList                *lp;
  GList                 *children;

  g_return_if_fail (THUNAR_IS_TEMPLATES_ACTION (templates_action));
  g_return_if_fail (templates_action->templates_monitor == monitor);
  g_return_if_fail (templates_action->templates_handle == handle);

  /* clear all menus for all menu proxies (will be rebuild on-demand) */
  for (lp = gtk_action_get_proxies (GTK_ACTION (templates_action)); lp != NULL; lp = lp->next)
    if (G_LIKELY (GTK_IS_MENU_ITEM (lp->data)))
      {
        menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (lp->data));
        if (G_UNLIKELY (menu == NULL))
          continue;

        /* we clear the menu */
        children = gtk_container_get_children (GTK_CONTAINER (menu));
        g_list_foreach (children, (GFunc) gtk_widget_destroy, NULL);
        g_list_free (children);

        /* regenerate it if its currently mapped */
        if (G_UNLIKELY (GTK_WIDGET_MAPPED (menu)))
          thunar_templates_action_build_menu (templates_action, menu);
      }
}



/**
 * thunar_templates_action_new:
 * @name  : the internal name of the action.
 * @label : the label for the action.
 *
 * Allocates a new #ThunarTemplatesAction with the given
 * @name and @label.
 *
 * Return value: the newly allocated #ThunarTemplatesAction.
 **/
GtkAction*
thunar_templates_action_new (const gchar *name,
                             const gchar *label)
{
  g_return_val_if_fail (name != NULL, NULL);
  g_return_val_if_fail (label != NULL, NULL);

  return g_object_new (THUNAR_TYPE_TEMPLATES_ACTION,
                       "hide-if-empty", FALSE,
                       "label", label,
                       "name", name,
                       NULL);
}


