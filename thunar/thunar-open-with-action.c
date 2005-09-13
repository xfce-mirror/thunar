/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#include <thunar/thunar-open-with-action.h>



enum
{
  PROP_0,
  PROP_FILE,
};

enum
{
  OPEN_APPLICATION,
  LAST_SIGNAL,
};



static void       thunar_open_with_action_class_init        (ThunarOpenWithActionClass *klass);
static void       thunar_open_with_action_init              (ThunarOpenWithAction      *open_with_action);
static void       thunar_open_with_action_dispose           (GObject                   *object);
static void       thunar_open_with_action_finalize          (GObject                   *object);
static void       thunar_open_with_action_get_property      (GObject                   *object,
                                                             guint                      prop_id,
                                                             GValue                    *value,
                                                             GParamSpec                *pspec);
static void       thunar_open_with_action_set_property      (GObject                   *object,
                                                             guint                      prop_id,
                                                             const GValue              *value,
                                                             GParamSpec                *pspec);
static GtkWidget *thunar_open_with_action_create_menu_item  (GtkAction                 *action);
static void       thunar_open_with_action_activated         (GtkWidget                 *item,
                                                             ThunarOpenWithAction      *open_with_action);
static void       thunar_open_with_action_menu_mapped       (GtkWidget                 *menu,
                                                             ThunarOpenWithAction      *open_with_action);
static void       thunar_open_with_action_file_destroy      (ThunarFile                *file,
                                                             ThunarOpenWithAction      *open_with_action);



struct _ThunarOpenWithActionClass
{
  GtkActionClass __parent__;

  void (*open_application) (ThunarOpenWithAction     *action,
                            ThunarVfsMimeApplication *application,
                            GList                    *uri_list);
};

struct _ThunarOpenWithAction
{
  GtkAction __parent__;

  ThunarVfsMimeDatabase *mime_database;
  ThunarFile            *file;
};



static guint open_with_action_signals[LAST_SIGNAL];

G_DEFINE_TYPE (ThunarOpenWithAction, thunar_open_with_action, GTK_TYPE_ACTION);



static void
marshal_VOID__EOBJECT_POINTER (GClosure     *closure,
                               GValue       *return_value,
                               guint         n_param_values,
                               const GValue *param_values,
                               gpointer      invocation_hint,
                               gpointer      marshal_data)
{
  typedef void (*MarshalFunc_VOID__EOBJECT_POINTER) (gpointer data1, gpointer arg_1, gpointer arg_2, gpointer data2);
  register MarshalFunc_VOID__EOBJECT_POINTER callback;
  register gpointer data1, data2;

  g_return_if_fail (n_param_values == 3);

  if (G_CCLOSURE_SWAP_DATA (closure))
    {
      data1 = closure->data;
      data2 = g_value_peek_pointer (param_values + 0);
    }
  else
    {
      data1 = g_value_peek_pointer (param_values + 0);
      data2 = closure->data;
    }

  callback = (gpointer) ((marshal_data != NULL) ? marshal_data : ((GCClosure *) closure)->callback);
  callback (data1, exo_value_get_object (param_values + 1), g_value_get_pointer (param_values + 2), data2);
}



static void
thunar_open_with_action_class_init (ThunarOpenWithActionClass *klass)
{
  GtkActionClass *gtkaction_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_open_with_action_dispose;
  gobject_class->finalize = thunar_open_with_action_finalize;
  gobject_class->get_property = thunar_open_with_action_get_property;
  gobject_class->set_property = thunar_open_with_action_set_property;

  gtkaction_class = GTK_ACTION_CLASS (klass);
  gtkaction_class->create_menu_item = thunar_open_with_action_create_menu_item;

  /**
   * ThunarOpenWithAction:file:
   *
   * The #ThunarFile for which to display the MIME application
   * selector menu.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        _("File"),
                                                        _("File"),
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarOpenWithAction::open-application:
   * @open_with_action : a #ThunarOpenWithAction.
   * @application      : a #ThunarVfsMimeApplication.
   * @uri_list         : a list of #ThunarVfsURI<!---->s.
   *
   * Emitted by @open_with_action whenever the user requests to
   * open @uri_list with @application.
   **/
  open_with_action_signals[OPEN_APPLICATION] =
    g_signal_new ("open-application",
                  G_TYPE_FROM_CLASS (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarOpenWithActionClass, open_application),
                  NULL, NULL, marshal_VOID__EOBJECT_POINTER,
                  G_TYPE_NONE, 2, THUNAR_VFS_TYPE_MIME_APPLICATION, G_TYPE_POINTER);
}



static void
thunar_open_with_action_init (ThunarOpenWithAction *open_with_action)
{
  open_with_action->mime_database = thunar_vfs_mime_database_get_default ();
}



static void
thunar_open_with_action_dispose (GObject *object)
{
  ThunarOpenWithAction *open_with_action = THUNAR_OPEN_WITH_ACTION (object);

  thunar_open_with_action_set_file (open_with_action, NULL);

  (*G_OBJECT_CLASS (thunar_open_with_action_parent_class)->dispose) (object);
}



static void
thunar_open_with_action_finalize (GObject *object)
{
  ThunarOpenWithAction *open_with_action = THUNAR_OPEN_WITH_ACTION (object);

  g_object_unref (G_OBJECT (open_with_action->mime_database));

  (*G_OBJECT_CLASS (thunar_open_with_action_parent_class)->finalize) (object);
}



static void
thunar_open_with_action_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  ThunarOpenWithAction *open_with_action = THUNAR_OPEN_WITH_ACTION (object);

  switch (prop_id)
    {
    case PROP_FILE:
      g_value_set_object (value, thunar_open_with_action_get_file (open_with_action));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_open_with_action_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  ThunarOpenWithAction *open_with_action = THUNAR_OPEN_WITH_ACTION (object);

  switch (prop_id)
    {
    case PROP_FILE:
      thunar_open_with_action_set_file (open_with_action, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static GtkWidget*
thunar_open_with_action_create_menu_item (GtkAction *action)
{
  GtkWidget *item;
  GtkWidget *menu;

  g_return_val_if_fail (THUNAR_IS_OPEN_WITH_ACTION (action), NULL);

  /* let GtkAction allocate the menu item */
  item = (*GTK_ACTION_CLASS (thunar_open_with_action_parent_class)->create_menu_item) (action);

  /* associate an empty submenu with the item (will be filled when mapped) */
  menu = gtk_menu_new ();
  g_signal_connect (G_OBJECT (menu), "map", G_CALLBACK (thunar_open_with_action_menu_mapped), action);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), menu);

  return item;
}



static void
thunar_open_with_action_activated (GtkWidget            *item,
                                   ThunarOpenWithAction *open_with_action)
{
  ThunarVfsMimeApplication *application;
  ThunarFile               *file;
  GList                     uri_list;

  g_return_if_fail (THUNAR_IS_OPEN_WITH_ACTION (open_with_action));
  g_return_if_fail (GTK_IS_MENU_ITEM (item));

  /* verify that the file is still alive */
  file = open_with_action->file;
  if (G_UNLIKELY (file == NULL))
    return;

  /* query the launch parameters for this application item */
  application = g_object_get_data (G_OBJECT (item), "thunar-vfs-mime-application");

  /* generate a single item list */
  uri_list.data = thunar_file_get_uri (file);
  uri_list.next = NULL;
  uri_list.prev = NULL;

  /* emit the "open-application" signal */
  g_signal_emit (G_OBJECT (open_with_action), open_with_action_signals[OPEN_APPLICATION], 0, application, &uri_list);
}



static void
thunar_open_with_action_menu_mapped (GtkWidget            *menu,
                                     ThunarOpenWithAction *open_with_action)
{
  ThunarVfsMimeApplication *default_application;
  ThunarVfsMimeInfo        *info;
  ThunarIconFactory        *icon_factory;
  GtkIconTheme             *icon_theme;
  const gchar              *icon_name;
  GdkPixbuf                *icon;
  GtkWidget                *image;
  GtkWidget                *item;
  GList                    *applications;
  GList                    *lp;
  gchar                    *text;
  gint                      icon_size;

  g_return_if_fail (THUNAR_IS_OPEN_WITH_ACTION (open_with_action));
  g_return_if_fail (GTK_IS_MENU_SHELL (menu));

  /* check if we already added our children to the submenu (or we don't have a file) */
  if (G_LIKELY (GTK_MENU_SHELL (menu)->children != NULL || open_with_action->file == NULL))
    return;

  /* determine the mime info for the file (may fail) */
  info = thunar_file_get_mime_info (open_with_action->file);

  /* determine the icon size for menus */
  gtk_icon_size_lookup (GTK_ICON_SIZE_MENU, &icon_size, &icon_size);

  /* determine the icon factory */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (menu));
  icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);

  /* determine all applications that are claim to be able to handle the file */
  applications = thunar_vfs_mime_database_get_applications (open_with_action->mime_database, info);

  /* determine the default application (fallback to the first available application) */
  default_application = thunar_vfs_mime_database_get_default_application (open_with_action->mime_database, info);
  if (G_UNLIKELY (default_application == NULL && applications != NULL))
    default_application = exo_object_ref (applications->data);

  /* add a menu entry for the default application */
  if (G_LIKELY (default_application != NULL))
    {
      /* drop the default application from the application list */
      lp = g_list_find (applications, default_application);
      if (G_LIKELY (lp != NULL))
        {
          applications = g_list_delete_link (applications, lp);
          exo_object_unref (EXO_OBJECT (default_application));
        }

      text = g_strdup_printf (_("%s (default)"), thunar_vfs_mime_application_get_name (default_application));
      item = gtk_image_menu_item_new_with_label (text);
      g_object_set_data_full (G_OBJECT (item), "thunar-vfs-mime-application", default_application, exo_object_unref);
      g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (thunar_open_with_action_activated), open_with_action);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
      g_free (text);

      /* setup the icon for the application */
      icon_name = thunar_vfs_mime_application_lookup_icon_name (default_application, icon_theme);
      icon = thunar_icon_factory_load_icon (icon_factory, icon_name, icon_size, NULL, FALSE);
      image = gtk_image_new_from_pixbuf (icon);
      gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
      gtk_widget_show (image);
      if (G_LIKELY (icon != NULL))
        g_object_unref (icon);

      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  /* add the other possible applications */
  if (G_LIKELY (applications != NULL))
    {
      for (lp = applications; lp != NULL; lp = lp->next)
        {
          item = gtk_image_menu_item_new_with_label (thunar_vfs_mime_application_get_name (lp->data));
          g_object_set_data_full (G_OBJECT (item), "thunar-vfs-mime-application", lp->data, exo_object_unref);
          g_signal_connect (G_OBJECT (item), "activate", G_CALLBACK (thunar_open_with_action_activated), open_with_action);
          gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
          gtk_widget_show (item);

          /* setup the icon for the application */
          icon_name = thunar_vfs_mime_application_lookup_icon_name (lp->data, icon_theme);
          icon = thunar_icon_factory_load_icon (icon_factory, icon_name, icon_size, NULL, FALSE);
          image = gtk_image_new_from_pixbuf (icon);
          gtk_image_menu_item_set_image (GTK_IMAGE_MENU_ITEM (item), image);
          gtk_widget_show (image);
          if (G_LIKELY (icon != NULL))
            g_object_unref (icon);
        }

      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      g_list_free (applications);
    }

  /* add our custom children */
  item = gtk_image_menu_item_new_with_label (_("Other..."));
  g_signal_connect_swapped (G_OBJECT (item), "activate", G_CALLBACK (gtk_action_activate), open_with_action);
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* clean up */
  g_object_unref (G_OBJECT (icon_factory));
  thunar_vfs_mime_info_unref (info);
}



static void
thunar_open_with_action_file_destroy (ThunarFile           *file,
                                      ThunarOpenWithAction *open_with_action)
{
  g_return_if_fail (THUNAR_IS_OPEN_WITH_ACTION (open_with_action));
  g_return_if_fail (open_with_action->file == file);
  g_return_if_fail (THUNAR_IS_FILE (file));

  /* just unset the file here (will automatically hide the action) */
  thunar_open_with_action_set_file (open_with_action, NULL);
}



/**
 * thunar_open_with_action_new:
 * @name  : the name of the #ThunarOpenWithAction.
 * @label : the label of the #ThunarOpenWithAction.
 *
 * Allocates a new #ThunarOpenWithAction instance
 * and returns it.
 *
 * Return value: the newly allocated #ThunarOpenWithAction
 *               instance.
 **/
GtkAction*
thunar_open_with_action_new (const gchar *name,
                             const gchar *label)
{
  g_return_val_if_fail (name != NULL, NULL);

  return g_object_new (THUNAR_TYPE_OPEN_WITH_ACTION,
                       "hide-if-empty", FALSE,
                       "label", label,
                       "name", name,
                       "visible", FALSE,
                       NULL);
}



/**
 * thunar_open_with_action_get_file:
 * @open_with_action : a #ThunarOpenWithAction.
 *
 * Returns the #ThunarFile set for the @open_with_action,
 * which may be %NULL.
 *
 * Return value: the #ThunarFile for @open_with_action.
 **/
ThunarFile*
thunar_open_with_action_get_file (ThunarOpenWithAction *open_with_action)
{
  g_return_val_if_fail (THUNAR_IS_OPEN_WITH_ACTION (open_with_action), NULL);
  return open_with_action->file;
}



/**
 * thunar_open_with_action_set_file:
 * @open_with_action : a #ThunarOpenWithAction.
 * @file             : a #ThunarFile or %NULL.
 *
 * Tells @open_with_action to display a selector menu
 * for @file.
 *
 * The @open_with_action will be visible if @file is
 * not %NULL, else it's invisible.
 **/
void
thunar_open_with_action_set_file (ThunarOpenWithAction *open_with_action,
                                  ThunarFile           *file)
{
  GtkWidget *menu;
  GSList    *lp;
  GList     *children;

  g_return_if_fail (THUNAR_IS_OPEN_WITH_ACTION (open_with_action));
  g_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  /* check that we have different files */
  if (G_UNLIKELY (open_with_action->file == file))
    return;

  /* disconnect from the previous file */
  if (G_LIKELY (open_with_action->file != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (open_with_action->file), thunar_open_with_action_file_destroy, open_with_action);
      g_object_unref (G_OBJECT (open_with_action->file));
    }

  /* activate the new file */
  open_with_action->file = file;

  /* connect to the new file */
  if (G_LIKELY (file != NULL))
    {
      g_object_ref (G_OBJECT (file));
      g_signal_connect (G_OBJECT (file), "destroy", G_CALLBACK (thunar_open_with_action_file_destroy), open_with_action);
    }

  /* clear menus for all menu proxies (will be rebuild on-demand) */
  for (lp = gtk_action_get_proxies (GTK_ACTION (open_with_action)); lp != NULL; lp = lp->next)
    if (G_LIKELY (GTK_IS_MENU_ITEM (lp->data)))
      {
        menu = gtk_menu_item_get_submenu (GTK_MENU_ITEM (lp->data));
        if (G_LIKELY (menu != NULL))
          {
            children = gtk_container_get_children (GTK_CONTAINER (menu));
            g_list_foreach (children, (GFunc) gtk_widget_destroy, NULL);
            g_list_free (children);
          }
      }
        

  /* toggle the visibility of the action */
#if GTK_CHECK_VERSION(2,6,0)
  gtk_action_set_visible (GTK_ACTION (open_with_action), (file != NULL && !thunar_file_is_directory (file)));
#else
  g_object_set (G_OBJECT (open_with_action), "visible", (file != NULL && !thunar_file_is_directory (file)), NULL);
#endif

  /* notify listeners */
  g_object_notify (G_OBJECT (open_with_action), "file");
}




