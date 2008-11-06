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

#include <thunar/thunar-private.h>
#include <thunar/thunar-templates-action.h>



/* Signal identifiers */
enum
{
  CREATE_EMPTY_FILE,
  CREATE_TEMPLATE,
  LAST_SIGNAL,
};



static void       thunar_templates_action_class_init        (ThunarTemplatesActionClass *klass);
static GtkWidget *thunar_templates_action_create_menu_item  (GtkAction                  *action);
static void       thunar_templates_action_fill_menu         (ThunarTemplatesAction      *templates_action,
                                                             ThunarVfsPath              *templates_path,
                                                             GtkWidget                  *menu);
static void       thunar_templates_action_menu_shown        (GtkWidget                  *menu,
                                                             ThunarTemplatesAction      *templates_action);



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
        NULL,
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

  /* determine the parent type class */
  thunar_templates_action_parent_class = g_type_class_peek_parent (klass);

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
                  G_TYPE_FROM_CLASS (klass),
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
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarTemplatesActionClass, create_template),
                  NULL, NULL, g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, THUNAR_VFS_TYPE_INFO);
}



static GtkWidget*
thunar_templates_action_create_menu_item (GtkAction *action)
{
  GtkWidget *item;
  GtkWidget *menu;

  _thunar_return_val_if_fail (THUNAR_IS_TEMPLATES_ACTION (action), NULL);

  /* let GtkAction allocate the menu item */
  item = (*GTK_ACTION_CLASS (thunar_templates_action_parent_class)->create_menu_item) (action);

  /* associate an empty submenu with the item (will be filled when shown) */
  menu = gtk_menu_new ();
  g_signal_connect (G_OBJECT (menu), "show", G_CALLBACK (thunar_templates_action_menu_shown), action);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

  return item;
}



static gint
info_compare (gconstpointer a,
              gconstpointer b)
{
  const ThunarVfsInfo *info_a = a;
  const ThunarVfsInfo *info_b = b;
  gchar               *name_a;
  gchar               *name_b;
  gint                 result;

  /* sort folders before files */
  if (info_a->type == THUNAR_VFS_FILE_TYPE_DIRECTORY && info_b->type != THUNAR_VFS_FILE_TYPE_DIRECTORY)
    return -1;
  else if (info_a->type != THUNAR_VFS_FILE_TYPE_DIRECTORY && info_b->type == THUNAR_VFS_FILE_TYPE_DIRECTORY)
    return 1;

  /* compare by name */
  name_a = g_utf8_casefold (info_a->display_name, -1);
  name_b = g_utf8_casefold (info_b->display_name, -1);
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

  _thunar_return_if_fail (THUNAR_IS_TEMPLATES_ACTION (templates_action));
  _thunar_return_if_fail (GTK_IS_WIDGET (item));

  /* check if an info is set for the item (else it's the "Empty File" item) */
  info = g_object_get_data (G_OBJECT (item), I_("thunar-vfs-info"));
  if (G_UNLIKELY (info != NULL))
    g_signal_emit (G_OBJECT (templates_action), templates_action_signals[CREATE_TEMPLATE], 0, info);
  else
    g_signal_emit (G_OBJECT (templates_action), templates_action_signals[CREATE_EMPTY_FILE], 0);
}



static void
thunar_templates_action_fill_menu (ThunarTemplatesAction *templates_action,
                                   ThunarVfsPath         *templates_path,
                                   GtkWidget             *menu)
{
  ThunarVfsInfo *info;
  ThunarVfsPath *path;
  GtkIconTheme  *icon_theme;
  const gchar   *icon_name;
  const gchar   *name;
  GtkWidget     *submenu;
  GtkWidget     *image;
  GtkWidget     *item;
  gchar         *absolute_path;
  gchar         *label;
  gchar         *dot;
  GList         *info_list = NULL;
  GList         *lp;
  GDir          *dp;

  /* try to open the templates (sub)directory */
  absolute_path = thunar_vfs_path_dup_string (templates_path);
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
          path = thunar_vfs_path_relative (templates_path, name);
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
  if (G_UNLIKELY (info_list == NULL))
    return;

  /* determine the icon theme for the menu */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (menu));

  /* add menu items for all infos */
  for (lp = info_list; lp != NULL; lp = lp->next)
    {
      /* determine the info */
      info = lp->data;

      /* check if we have a regular file or a directory here */
      if (G_LIKELY (info->type == THUNAR_VFS_FILE_TYPE_REGULAR))
        {
          /* generate a label by stripping off the extension */
          label = g_strdup (info->display_name);
          dot = g_utf8_strrchr (label, -1, '.');
          if (G_LIKELY (dot != NULL))
            *dot = '\0';

          /* allocate a new menu item */
          item = gtk_image_menu_item_new_with_label (label);
          g_object_set_data_full (G_OBJECT (item), I_("thunar-vfs-info"), thunar_vfs_info_ref (info), (GDestroyNotify) thunar_vfs_info_unref);
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
      else if (info->type == THUNAR_VFS_FILE_TYPE_DIRECTORY)
        {
          /* allocate a new submenu for the directory */
          submenu = gtk_menu_new ();
          exo_gtk_object_ref_sink (GTK_OBJECT (submenu));
          gtk_menu_set_screen (GTK_MENU (submenu), gtk_widget_get_screen (menu));

          /* fill the submenu from the folder contents */
          thunar_templates_action_fill_menu (templates_action, info->path, submenu);

          /* check if any items were added to the submenu */
          if (G_LIKELY (GTK_MENU_SHELL (submenu)->children != NULL))
            {
              /* hook up the submenu */
              item = gtk_image_menu_item_new_with_label (info->display_name);
              gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
              gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
              gtk_widget_show (item);

              /* lookup the icon for the mime type of that file */
              icon_name = thunar_vfs_mime_info_lookup_icon_name (info->mime_info, icon_theme);

              /* generate an image based on the named icon */
              image = gtk_image_new_from_icon_name (icon_name, GTK_ICON_SIZE_MENU);
              gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
              gtk_widget_show (image);
            }

          /* cleanup */
          g_object_unref (G_OBJECT (submenu));
        }
    }

  /* release the info list */
  thunar_vfs_info_list_free (info_list);
}



static void
thunar_templates_action_menu_shown (GtkWidget             *menu,
                                    ThunarTemplatesAction *templates_action)
{
  ThunarVfsPath *templates_path;
  ThunarVfsPath *home_path;
  GtkWidget     *image;
  GtkWidget     *item;
  GList         *children;
  gchar         *templates_dir = NULL;

  _thunar_return_if_fail (THUNAR_IS_TEMPLATES_ACTION (templates_action));
  _thunar_return_if_fail (GTK_IS_MENU_SHELL (menu));

  /* drop all existing children of the menu first */
  children = gtk_container_get_children (GTK_CONTAINER (menu));
  g_list_foreach (children, (GFunc) gtk_widget_destroy, NULL);
  g_list_free (children);

  /* determine the path to the ~/Templates folder */
  home_path = thunar_vfs_path_get_for_home ();

#if GLIB_CHECK_VERSION(2,14,0)
  templates_dir = g_strdup (g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES));
#endif /* GLIB_CHECK_VERSION(2,14,0) */
  if (G_UNLIKELY (templates_dir == NULL))
    {
      templates_dir = g_build_filename (G_DIR_SEPARATOR_S, xfce_get_homedir (),
                                        "Templates", NULL);
    }

  templates_path = thunar_vfs_path_new (templates_dir, NULL);
  if (G_UNLIKELY (templates_path == NULL))
    templates_path = thunar_vfs_path_relative (home_path,"Templates");

  g_free (templates_dir);

  thunar_vfs_path_unref (home_path);

#if GLIB_CHECK_VERSION(2, 14, 0)
  if (!exo_str_is_equal (g_get_user_special_dir (G_USER_DIRECTORY_TEMPLATES), xfce_get_homedir ()))
    {
      /* fill the menu with files/folders from the ~/Templates folder */
      thunar_templates_action_fill_menu (templates_action, templates_path, menu);
    }
#endif

  /* check if any items were added to the menu */
  if (G_UNLIKELY (GTK_MENU_SHELL (menu)->children == NULL))
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

  /* cleanup */
  thunar_vfs_path_unref (templates_path);
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
  _thunar_return_val_if_fail (name != NULL, NULL);
  _thunar_return_val_if_fail (label != NULL, NULL);

  return g_object_new (THUNAR_TYPE_TEMPLATES_ACTION,
                       "hide-if-empty", FALSE,
                       "label", label,
                       "name", name,
                       NULL);
}


