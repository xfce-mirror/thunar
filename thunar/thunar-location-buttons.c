/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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
 *
 * Based on code written by Jonathan Blandford <jrb@gnome.org> for
 * Red Hat, Inc.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <thunar/thunar-application.h>
#include <thunar/thunar-clipboard-manager.h>
#include <thunar/thunar-create-dialog.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-location-button.h>
#include <thunar/thunar-location-buttons.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-properties-dialog.h>



#define THUNAR_LOCATION_BUTTONS_SCROLL_TIMEOUT  (200)



/* Property identifiers */
enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
};



static void           thunar_location_buttons_navigator_init              (ThunarNavigatorIface       *iface);
static void           thunar_location_buttons_finalize                    (GObject                    *object);
static void           thunar_location_buttons_get_property                (GObject                    *object,
                                                                           guint                       prop_id,
                                                                           GValue                     *value,
                                                                           GParamSpec                 *pspec);
static void           thunar_location_buttons_set_property                (GObject                    *object,
                                                                           guint                       prop_id,
                                                                           const GValue               *value,
                                                                           GParamSpec                 *pspec);
static ThunarFile    *thunar_location_buttons_get_current_directory       (ThunarNavigator            *navigator);
static void           thunar_location_buttons_set_current_directory       (ThunarNavigator            *navigator,
                                                                           ThunarFile                 *current_directory);
static void           thunar_location_buttons_unmap                       (GtkWidget                  *widget);
static void           thunar_location_buttons_on_filler_clicked           (ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_get_preferred_width         (GtkWidget                  *widget,
                                                                           gint                       *minimum,
                                                                           gint                       *natural);
static void           thunar_location_buttons_get_preferred_height        (GtkWidget                  *widget,
                                                                           gint                       *minimum,
                                                                           gint                       *natural);
static GtkWidgetPath *thunar_location_buttons_get_path_for_child          (GtkContainer               *container,
                                                                           GtkWidget                  *child);
static void           thunar_location_buttons_child_ordering_changed      (ThunarLocationButtons      *buttons);
static GList         *thunar_location_buttons_detect_first_visible_button (ThunarLocationButtons      *buttons,
                                                                           gint                        available_width,
                                                                           gint                       *resulting_occupied_width);
static GList         *thunar_location_buttons_detect_last_visible_button  (ThunarLocationButtons      *buttons,
                                                                           gint                        available_width,
                                                                           gint                       *resulting_occupied_width);
static void           thunar_location_buttons_size_allocate               (GtkWidget                  *widget,
                                                                           GtkAllocation              *allocation);
static void           thunar_location_buttons_state_changed               (GtkWidget                  *widget,
                                                                           GtkStateType                previous_state);
static void           thunar_location_buttons_grab_notify                 (GtkWidget                  *widget,
                                                                           gboolean                    was_grabbed);
static void           thunar_location_buttons_add                         (GtkContainer               *container,
                                                                           GtkWidget                  *widget);
static void           thunar_location_buttons_remove                      (GtkContainer               *container,
                                                                           GtkWidget                  *widget);
static void           thunar_location_buttons_forall                      (GtkContainer               *container,
                                                                           gboolean                    include_internals,
                                                                           GtkCallback                 callback,
                                                                           gpointer                    callback_data);
static GtkWidget     *thunar_location_buttons_make_button                 (ThunarLocationButtons      *buttons,
                                                                           ThunarFile                 *file);
static void           thunar_location_buttons_remove_1                    (GtkContainer               *container,
                                                                           GtkWidget                  *widget);
static gboolean       thunar_location_buttons_draw                        (GtkWidget                  *buttons,
                                                                           cairo_t                    *cr);
static gboolean       thunar_location_buttons_scroll_timeout              (gpointer                    user_data);
static void           thunar_location_buttons_scroll_timeout_destroy      (gpointer                    user_data);
static void           thunar_location_buttons_stop_scrolling              (ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_update_sliders              (ThunarLocationButtons      *buttons);
static gboolean       thunar_location_buttons_slider_button_press         (GtkWidget                  *button,
                                                                           GdkEventButton             *event,
                                                                           ThunarLocationButtons      *buttons);
static gboolean       thunar_location_buttons_slider_button_release       (GtkWidget                  *button,
                                                                           GdkEventButton             *event,
                                                                           ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_scroll_left                 (GtkWidget                  *button,
                                                                           ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_scroll_right                (GtkWidget                  *button,
                                                                           ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_clicked                     (ThunarLocationButton       *button,
                                                                           gboolean                    open_in_tab,
                                                                           ThunarLocationButtons      *buttons);
static gboolean       thunar_location_buttons_context_menu                (ThunarLocationButton       *button,
                                                                           ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_gone                        (ThunarLocationButton       *button,
                                                                           ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_create_folder        (GtkAction                  *action,
                                                                           ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_down_folder          (GtkAction                  *action,
                                                                           ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_empty_trash          (GtkAction                  *action,
                                                                           ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_open                 (GtkAction                  *action,
                                                                           ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_open_in_new_tab      (GtkAction                  *action,
                                                                           ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_open_in_new_window   (GtkAction                  *action,
                                                                           ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_paste_into_folder    (GtkAction                  *action,
                                                                           ThunarLocationButtons      *buttons);
static void           thunar_location_buttons_action_properties           (GtkAction                  *action,
                                                                           ThunarLocationButtons      *buttons);



struct _ThunarLocationButtonsClass
{
  GtkContainerClass __parent__;

  void (*entry_requested)(const gchar *initial_text);
};

struct _ThunarLocationButtons
{
  GtkContainer __parent__;

  GtkActionGroup    *action_group;

  GtkWidget         *left_slider;
  GtkWidget         *right_slider;
  GtkWidget         *filler_widget;

  ThunarFile        *current_directory;

  gint               slider_width;
  gboolean           ignore_click : 1;

  GList             *list;
  GList             *fake_root_button;
  GList             *first_visible_button;
  GList             *last_visible_button;

  guint              scroll_timeout_id;
};



static GQuark thunar_file_quark = 0;



static const GtkActionEntry action_entries[] =
{
  { "location-buttons-down-folder", NULL, "Down Folder", "<alt>Down", NULL, G_CALLBACK (thunar_location_buttons_action_down_folder), },
  { "location-buttons-context-menu", NULL, "Context Menu", NULL, "", NULL, },
  { "location-buttons-open", "document-open", N_("_Open"), "", NULL, G_CALLBACK (thunar_location_buttons_action_open), },
  { "location-buttons-open-in-new-tab", NULL, N_("Open in New Tab"), "", NULL, G_CALLBACK (thunar_location_buttons_action_open_in_new_tab), },
  { "location-buttons-open-in-new-window", NULL, N_("Open in New Window"), "", NULL, G_CALLBACK (thunar_location_buttons_action_open_in_new_window), },
  { "location-buttons-create-folder", "folder-new", N_("Create _Folder..."), "", NULL, G_CALLBACK (thunar_location_buttons_action_create_folder), },
  { "location-buttons-empty-trash", NULL, N_("_Empty Trash"), "", N_("Delete all files and folders in the Trash"), G_CALLBACK (thunar_location_buttons_action_empty_trash), },
  { "location-buttons-paste-into-folder", "edit-paste", N_("Paste Into Folder"), "", NULL, G_CALLBACK (thunar_location_buttons_action_paste_into_folder), },
  { "location-buttons-properties", "document-properties", N_("_Properties..."), "", NULL, G_CALLBACK (thunar_location_buttons_action_properties), },
};



G_DEFINE_TYPE_WITH_CODE (ThunarLocationButtons, thunar_location_buttons, GTK_TYPE_CONTAINER,
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_location_buttons_navigator_init))



static void
thunar_location_buttons_class_init (ThunarLocationButtonsClass *klass)
{
  GtkContainerClass *gtkcontainer_class;
  GtkWidgetClass    *gtkwidget_class;
  GObjectClass      *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_location_buttons_finalize;
  gobject_class->get_property = thunar_location_buttons_get_property;
  gobject_class->set_property = thunar_location_buttons_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->draw = thunar_location_buttons_draw;
  gtkwidget_class->unmap = thunar_location_buttons_unmap;
  gtkwidget_class->get_preferred_width = thunar_location_buttons_get_preferred_width;
  gtkwidget_class->get_preferred_height = thunar_location_buttons_get_preferred_height;
  gtkwidget_class->size_allocate = thunar_location_buttons_size_allocate;
  gtkwidget_class->state_changed = thunar_location_buttons_state_changed;
  gtkwidget_class->grab_notify = thunar_location_buttons_grab_notify;

  gtkcontainer_class = GTK_CONTAINER_CLASS (klass);
  gtkcontainer_class->add = thunar_location_buttons_add;
  gtkcontainer_class->remove = thunar_location_buttons_remove;
  gtkcontainer_class->forall = thunar_location_buttons_forall;
  gtkcontainer_class->get_path_for_child = thunar_location_buttons_get_path_for_child;

  /* Override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

  /* signals */
  g_signal_new ("entry-requested",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                G_STRUCT_OFFSET (ThunarLocationButtonsClass, entry_requested),
                NULL, NULL,
                NULL,
                G_TYPE_NONE,
                1,
                G_TYPE_STRING);


  thunar_file_quark = g_quark_from_static_string ("button-thunar-file");
}


static void
thunar_location_buttons_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_location_buttons_get_current_directory;
  iface->set_current_directory = thunar_location_buttons_set_current_directory;
}



static void
thunar_location_buttons_on_filler_clicked (ThunarLocationButtons *buttons)
{
  g_signal_emit_by_name (buttons, "entry-requested", NULL);
}



static void
thunar_location_buttons_init (ThunarLocationButtons *buttons)
{
  GtkWidget       *icon;

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  /* setup the action group for the location buttons */
  buttons->action_group = gtk_action_group_new ("ThunarLocationButtons");
  gtk_action_group_set_accel_group (buttons->action_group, gtk_accel_group_new ());
  gtk_action_group_set_translation_domain (buttons->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (buttons->action_group, action_entries, G_N_ELEMENTS (action_entries), buttons);
G_GNUC_END_IGNORE_DEPRECATIONS

  gtk_widget_set_has_window (GTK_WIDGET (buttons), FALSE);
  gtk_widget_set_redraw_on_allocate (GTK_WIDGET (buttons), FALSE);

  buttons->left_slider = gtk_button_new ();
  g_signal_connect (G_OBJECT (buttons->left_slider), "button-press-event", G_CALLBACK (thunar_location_buttons_slider_button_press), buttons);
  g_signal_connect (G_OBJECT (buttons->left_slider), "button-release-event", G_CALLBACK (thunar_location_buttons_slider_button_release), buttons);
  g_signal_connect (G_OBJECT (buttons->left_slider), "clicked", G_CALLBACK (thunar_location_buttons_scroll_left), buttons);
  gtk_container_add (GTK_CONTAINER (buttons), buttons->left_slider);
  gtk_widget_show (buttons->left_slider);

  icon = gtk_image_new_from_icon_name ("pan-start-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (buttons->left_slider), icon);
  gtk_widget_show (icon);

  buttons->right_slider = gtk_button_new ();
  g_signal_connect (G_OBJECT (buttons->right_slider), "button-press-event", G_CALLBACK (thunar_location_buttons_slider_button_press), buttons);
  g_signal_connect (G_OBJECT (buttons->right_slider), "button-release-event", G_CALLBACK (thunar_location_buttons_slider_button_release), buttons);
  g_signal_connect (G_OBJECT (buttons->right_slider), "clicked", G_CALLBACK (thunar_location_buttons_scroll_right), buttons);
  gtk_container_add (GTK_CONTAINER (buttons), buttons->right_slider);
  gtk_widget_show (buttons->right_slider);

  icon = gtk_image_new_from_icon_name ("pan-end-symbolic", GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (buttons->right_slider), icon);
  gtk_widget_show (icon);

  buttons->filler_widget = gtk_button_new ();
  g_signal_connect_swapped (buttons->filler_widget, "clicked", G_CALLBACK (thunar_location_buttons_on_filler_clicked), buttons);

  icon = gtk_image_new_from_icon_name ("gtk-edit", GTK_ICON_SIZE_BUTTON);
  gtk_widget_set_halign (icon, GTK_ALIGN_END);
  gtk_container_add (GTK_CONTAINER (buttons->filler_widget), icon);
  gtk_widget_show (icon);

  gtk_container_add (GTK_CONTAINER (buttons), buttons->filler_widget);
  gtk_widget_show (buttons->filler_widget);

  g_object_set (buttons, "valign", GTK_ALIGN_CENTER, NULL);

  /* setup style classes */
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (buttons)),
                               GTK_STYLE_CLASS_LINKED);
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (buttons)),
                               "path-bar");

  /* add sub-buttons to css class which matches all buttons in the path-bar */
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (buttons->left_slider)),
                               "path-bar-button");
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (buttons->right_slider)),
                               "path-bar-button");
  gtk_style_context_add_class (gtk_widget_get_style_context (GTK_WIDGET (buttons->filler_widget)),
                               "path-bar-button");
}



static void
thunar_location_buttons_finalize (GObject *object)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (object);

  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* be sure to cancel the scrolling */
  thunar_location_buttons_stop_scrolling (buttons);

  /* release from the current_directory */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (buttons), NULL);

  /* release our action group */
  g_object_unref (G_OBJECT (buttons->action_group));

  (*G_OBJECT_CLASS (thunar_location_buttons_parent_class)->finalize) (object);
}



static void
thunar_location_buttons_get_property (GObject    *object,
                                      guint       prop_id,
                                      GValue     *value,
                                      GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_location_buttons_set_property (GObject      *object,
                                      guint         prop_id,
                                      const GValue *value,
                                      GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_location_buttons_get_current_directory (ThunarNavigator *navigator)
{
  return THUNAR_LOCATION_BUTTONS (navigator)->current_directory;
}



static inline gboolean
eglible_for_fake_root (ThunarFile *file)
{
  /* use 'Home' as fake root button */
  return thunar_file_is_home (file);

  /* TODO: mounted devices */
}



static void
thunar_location_buttons_set_current_directory (ThunarNavigator *navigator,
                                               ThunarFile      *current_directory)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (navigator);
  ThunarFile            *file_parent;
  ThunarFile            *file;
  GtkWidget             *button;
  GList                 *lp;

  _thunar_return_if_fail (current_directory == NULL || THUNAR_IS_FILE (current_directory));

  /* verify that we're not already using the new directory */
  if (G_UNLIKELY (buttons->current_directory == current_directory))
    return;

  /* check if we already have a button for that directory */
  for (lp = buttons->list; lp != NULL; lp = lp->next)
    {
      if (thunar_location_button_get_file (lp->data) == current_directory)
        {
          /* fake a "clicked" event for that button */
          gtk_button_clicked (GTK_BUTTON (lp->data));
          return;
        }
    }

  if (G_LIKELY (buttons->current_directory != NULL))
    {
      /* remove all buttons */
      g_object_unref (G_OBJECT (buttons->current_directory));

      while (buttons->list != NULL)
        gtk_container_remove (GTK_CONTAINER (buttons), buttons->list->data);

      /* clear scroll positions and fake root button */
      buttons->first_visible_button = NULL;
      buttons->last_visible_button = NULL;
      buttons->fake_root_button = NULL;
    }

  buttons->current_directory = current_directory;

  /* regenerate the button list */
  if (G_LIKELY (current_directory != NULL))
    {
      g_object_ref (G_OBJECT (current_directory));

      /* add the new buttons */
      for (file = current_directory; file != NULL; file = file_parent)
        {
          button = thunar_location_buttons_make_button (buttons, file);
          buttons->list = g_list_append (buttons->list, button);
          gtk_container_add (GTK_CONTAINER (buttons), button);
          gtk_widget_show (button);

          /* use 'Home' as fake root button */
          if (!buttons->fake_root_button && eglible_for_fake_root (file))
            buttons->fake_root_button = g_list_last (buttons->list);

          /* continue with the parent (if any) */
          file_parent = thunar_file_get_parent (file, NULL);

          if (G_LIKELY (file != current_directory))
            g_object_unref (G_OBJECT (file));
        }
    }

  g_object_notify (G_OBJECT (buttons), "current-directory");
}



static void
thunar_location_buttons_unmap (GtkWidget *widget)
{
  /* be sure to stop the scroll timer before the buttons are unmapped */
  thunar_location_buttons_stop_scrolling (THUNAR_LOCATION_BUTTONS (widget));

  /* do the real unmap */
  (*GTK_WIDGET_CLASS (thunar_location_buttons_parent_class)->unmap) (widget);
}



static void
thunar_location_buttons_get_preferred_width (GtkWidget  *widget,
                                             gint       *minimum,
                                             gint       *natural)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (widget);
  gint                   width = 0, height = 0, child_width = 0, child_height = 0, filler_width = 0;
  GList                 *lp;

  /* calculate the size of the biggest button */
  for (lp = buttons->list; lp != NULL; lp = lp->next)
    {
      gtk_widget_get_preferred_width (GTK_WIDGET (lp->data), &child_width, NULL);
      gtk_widget_get_preferred_height (GTK_WIDGET (lp->data), &child_height, NULL);
      width = MAX (width, child_width);
      height = MAX (height, child_height);
    }

  /* add space for the sliders if we have more than one path */
  buttons->slider_width = MIN (height * 2 / 3 + 5, height);
  if (buttons->list != NULL && buttons->list->next != NULL)
    width += (buttons->slider_width) * 2;

  gtk_widget_get_preferred_width (buttons->filler_widget, &filler_width, NULL);
  width += filler_width; // default is the minimum

  *minimum = *natural = width;
}



static void
thunar_location_buttons_get_preferred_height (GtkWidget *widget,
                                              gint      *minimum,
                                              gint      *natural)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (widget);
  gint                   height = 0, child_height = 0;
  GList                 *lp;

  /* calculate the size of the biggest button */
  for (lp = buttons->list; lp != NULL; lp = lp->next)
    {
      gtk_widget_get_preferred_height (GTK_WIDGET (lp->data), &child_height, NULL);
      height = MAX (height, child_height);
    }

  *minimum = *natural = height;
}



static GtkWidgetPath *
thunar_location_buttons_get_path_for_child (GtkContainer *container,
                                            GtkWidget    *child)
{
  ThunarLocationButtons *path_bar = THUNAR_LOCATION_BUTTONS (container);
  GtkWidgetPath *path;

  path = gtk_widget_path_copy (gtk_widget_get_path (GTK_WIDGET (path_bar)));

  if (gtk_widget_get_visible (child) &&
      gtk_widget_get_child_visible (child))
    {
      GtkWidgetPath *sibling_path;
      GList *visible_children;
      GList *l;
      int pos;

      /* 1. Build the list of visible children, in visually left-to-right order
        * (i.e. independently of the widget's direction).  Note that our
        * list is stored in innermost-to-outermost path order!
        */
      visible_children = NULL;
      visible_children = g_list_prepend (visible_children, path_bar->right_slider);
      visible_children = g_list_prepend (visible_children, path_bar->filler_widget);

      for (l = path_bar->list; l; l = l->next)
        {
          if (gtk_widget_get_visible (GTK_WIDGET (l->data)) &&
              gtk_widget_get_child_visible (GTK_WIDGET (l->data)))
            visible_children = g_list_prepend (visible_children, l->data);
        }

      visible_children = g_list_prepend (visible_children, path_bar->left_slider);

      if (gtk_widget_get_direction (GTK_WIDGET (path_bar)) == GTK_TEXT_DIR_RTL)
        {
          visible_children = g_list_reverse (visible_children);
        }

      /* 2. Find the index of the child within that list */
      pos = 0;

      for (l = visible_children; l; l = l->next)
        {
          GtkWidget *button = l->data;

          if (button == child)
            break;

          pos++;
        }

      /* 3. Build the path */
      sibling_path = gtk_widget_path_new ();

      for (l = visible_children; l; l = l->next)
        {
          gtk_widget_path_append_for_widget (sibling_path, l->data);
        }

      gtk_widget_path_append_with_siblings (path, sibling_path, pos);

      g_list_free (visible_children);
      gtk_widget_path_unref (sibling_path);
    }
  else
    {
      gtk_widget_path_append_for_widget (path, child);
    }

  return path;
}



static void
thunar_location_buttons_child_ordering_changed (ThunarLocationButtons *location_buttons)
{
  gtk_widget_reset_style (GTK_WIDGET (location_buttons));
}



static GList*
thunar_location_buttons_detect_first_visible_button (ThunarLocationButtons *buttons,
                                                     gint                   available_width,
                                                     gint                  *resulting_occupied_width)
{
  GList *lp;
  gint   button_width    = 0;
  gint   resulting_width = 0;

  /* walk all buttons starting by last, until no space is left */
  for (lp = buttons->last_visible_button; lp != NULL; lp = lp->next)
    {
      gtk_widget_get_preferred_width (GTK_WIDGET (lp->data), &button_width, NULL);
      if (resulting_width + button_width > available_width)
        {
          return lp->prev;
        }
      resulting_width += button_width;
      if(resulting_occupied_width != NULL)
        *resulting_occupied_width = resulting_width;
    }
  return g_list_last (buttons->list);
}



static GList*
thunar_location_buttons_detect_last_visible_button (ThunarLocationButtons *buttons,
                                                    gint                   available_width,
                                                    gint                  *resulting_occupied_width)
{
  GList *lp;
  gint   button_width    = 0;
  gint   resulting_width = 0;

  /* walk all buttons starting by first, until no space is left */
  for (lp = buttons->first_visible_button; lp != NULL; lp = lp->prev)
    {
      gtk_widget_get_preferred_width (GTK_WIDGET (lp->data), &button_width, NULL);
      if (resulting_width + button_width > available_width)
        {
          return lp->next;
        }
      resulting_width += button_width;
      if(resulting_occupied_width != NULL)
        *resulting_occupied_width = resulting_width;
    }
  return buttons->list;
}



static void
thunar_location_buttons_size_allocate (GtkWidget     *widget,
                                       GtkAllocation *allocation)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (widget);
  GtkTextDirection       direction;
  GtkAllocation          available_space;
  GtkAllocation          folder_button_allocation;
  GtkAllocation          first_slider_allocation;  /* left slider if direction is LTR */
  GtkAllocation          second_slider_allocation; /* right slider if direction is LTR */
  GtkAllocation          filler_allocation;
  GList                 *lp;
  gint                   required_width_total = 0;
  gint                   border_width;
  gint                   temp_width;
  gboolean               need_reorder = FALSE;

  gtk_widget_set_allocation (widget, allocation);
  /* if no path is set, we don't have to allocate anything */
  if (G_UNLIKELY (buttons->list == NULL))
    return;

  direction = gtk_widget_get_direction (widget);
  border_width = gtk_container_get_border_width (GTK_CONTAINER (buttons));
  available_space.x = allocation->x + border_width;
  available_space.y = allocation->y + border_width;
  available_space.width = allocation->width - 2 * border_width;
  available_space.height = MAX (1, allocation->height - 2 * border_width );

  first_slider_allocation.width = buttons->slider_width;
  first_slider_allocation.height = available_space.height;
  first_slider_allocation.y = available_space.y;

  second_slider_allocation.width = buttons->slider_width;
  second_slider_allocation.height = available_space.height;
  second_slider_allocation.y = available_space.y;

  if (G_LIKELY (direction == GTK_TEXT_DIR_LTR))
    {
      first_slider_allocation.x = available_space.x;
      second_slider_allocation.x = available_space.x + available_space.width - buttons->slider_width;
    }
  else
    {
      first_slider_allocation.x = available_space.x + available_space.width - buttons->slider_width;
      second_slider_allocation.x = available_space.x;
    }

  gtk_widget_get_preferred_width (buttons->filler_widget, &filler_allocation.width, NULL);
  required_width_total += buttons->slider_width * 2 + filler_allocation.width;
  filler_allocation.height = available_space.height;
  filler_allocation.y = available_space.y;

  for (lp = buttons->list; lp != NULL; lp = lp->next)
    {
      gtk_widget_get_preferred_width (GTK_WIDGET (lp->data), &temp_width, NULL);
      required_width_total += temp_width;
      if (lp == buttons->fake_root_button)
        break;
    }

  if ( required_width_total <= available_space.width  )
    {
      /* Show full list of buttons, starting by fake root, if available */
      if (buttons->fake_root_button == NULL)
        buttons->first_visible_button = g_list_last (buttons->list);
      else
        buttons->first_visible_button = buttons->fake_root_button;
      buttons->last_visible_button = buttons->list;
    }
  else
    {
      /* reset calculation for required_width_total .. we need to cut off some folders */
      required_width_total = first_slider_allocation.width + second_slider_allocation.width + filler_allocation.width;

      /* No scrolling defined ? Use last button as fixed start! */
      if(buttons->last_visible_button == NULL && buttons->first_visible_button == NULL)
        buttons->last_visible_button = buttons->list;

      if(buttons->first_visible_button == NULL) /* last button is fixed defined */
        buttons->first_visible_button = thunar_location_buttons_detect_first_visible_button(buttons, available_space.width - required_width_total, &temp_width);
      else                                      /* first button is fixed defined */
        buttons->last_visible_button = thunar_location_buttons_detect_last_visible_button(buttons, available_space.width - required_width_total, &temp_width);

      required_width_total += temp_width;
    }

  /* hide buttons before first_visible_button */
  for (lp = buttons->first_visible_button->next; lp != NULL; lp = lp->next)
    {
      need_reorder |= gtk_widget_get_child_visible (GTK_WIDGET (lp->data)) == TRUE;
      gtk_widget_set_child_visible (GTK_WIDGET (lp->data), FALSE);
    }
  /* hide buttons after last visible_button */
  for (lp = buttons->last_visible_button->prev; lp != NULL; lp = lp->prev)
    {
      need_reorder |= gtk_widget_get_child_visible (GTK_WIDGET (lp->data)) == TRUE;
      gtk_widget_set_child_visible (GTK_WIDGET (lp->data), FALSE);
    }

  /* allocate space for the buttons */
  folder_button_allocation.y = available_space.y;
  folder_button_allocation.height = available_space.height;
  if (G_LIKELY (direction == GTK_TEXT_DIR_LTR))
    folder_button_allocation.x = first_slider_allocation.x + first_slider_allocation.width;
  else
    {
      if (first_slider_allocation.width == 0)
        folder_button_allocation.x = available_space.x + available_space.width;
      else
        folder_button_allocation.x = first_slider_allocation.x;
    }

  for (lp = buttons->first_visible_button; lp != NULL; lp = lp->prev)
    {
      gtk_widget_get_preferred_width (GTK_WIDGET (lp->data), &folder_button_allocation.width, NULL);

      if (G_UNLIKELY (direction == GTK_TEXT_DIR_RTL))
        folder_button_allocation.x -= folder_button_allocation.width;

      need_reorder |= gtk_widget_get_child_visible (GTK_WIDGET (lp->data)) == FALSE;
      gtk_widget_set_child_visible (GTK_WIDGET (lp->data), TRUE);
      gtk_widget_size_allocate (lp->data, &folder_button_allocation);

      if (G_LIKELY (direction == GTK_TEXT_DIR_LTR))
        folder_button_allocation.x += folder_button_allocation.width;

      if( lp == buttons->last_visible_button )
        break;
    }

  /* allocate the filler */
  filler_allocation.width += available_space.width - required_width_total;
  filler_allocation.x = folder_button_allocation.x;
  if (G_UNLIKELY (direction == GTK_TEXT_DIR_RTL))
    filler_allocation.x -= filler_allocation.width;
  gtk_widget_size_allocate (GTK_WIDGET (buttons->filler_widget), &filler_allocation);

  /* allocate the sliders */
  gtk_widget_get_preferred_width (GTK_WIDGET (buttons->left_slider), &temp_width, NULL); //to dont get gtk warnings
  gtk_widget_get_preferred_width (GTK_WIDGET (buttons->right_slider), &temp_width, NULL);
  gtk_widget_size_allocate (buttons->left_slider, &first_slider_allocation);
  gtk_widget_size_allocate (buttons->right_slider, &second_slider_allocation);
  thunar_location_buttons_update_sliders (buttons);

  if (need_reorder)
    thunar_location_buttons_child_ordering_changed (buttons);

  gtk_widget_queue_draw (GTK_WIDGET (buttons));
}



static gboolean
thunar_location_buttons_draw (GtkWidget *widget,
                              cairo_t   *cr)
{
  GtkStyleContext *context;
  GtkAllocation alloc;

  context = gtk_widget_get_style_context (widget);
  gtk_widget_get_allocation (widget, &alloc);

  gtk_render_background (context, cr, 0, 0, alloc.width, alloc.height);
  gtk_render_frame (context, cr, 0, 0, alloc.width, alloc.height);

  return GTK_WIDGET_CLASS (thunar_location_buttons_parent_class)->draw (widget, cr);
}



static void
thunar_location_buttons_state_changed (GtkWidget   *widget,
                                       GtkStateType previous_state)
{
  if (!gtk_widget_is_sensitive (widget))
    thunar_location_buttons_stop_scrolling (THUNAR_LOCATION_BUTTONS (widget));
}



static void
thunar_location_buttons_grab_notify (GtkWidget *widget,
                                     gboolean   was_grabbed)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (widget);

  if (!was_grabbed)
    thunar_location_buttons_stop_scrolling (buttons);
}



static void
thunar_location_buttons_add (GtkContainer *container,
                             GtkWidget    *widget)
{
  gtk_widget_set_parent (widget, GTK_WIDGET (container));
}



static void
thunar_location_buttons_remove (GtkContainer *container,
                                GtkWidget    *widget)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (container);
  GList                 *lp;

  if (widget == buttons->left_slider)
    {
      thunar_location_buttons_remove_1 (container, widget);
      buttons->left_slider = NULL;
      return;
    }
  else if (widget == buttons->right_slider)
    {
      thunar_location_buttons_remove_1 (container, widget);
      buttons->right_slider = NULL;
      return;
    }

  for (lp = buttons->list; lp != NULL; lp = lp->next)
    if (widget == GTK_WIDGET (lp->data))
      {
        thunar_location_buttons_remove_1 (container, widget);
        buttons->list = g_list_remove_link (buttons->list, lp);
        g_list_free (lp);
        return;
      }

  /* fallback case */
  thunar_location_buttons_remove_1 (container, widget);
}



static void
thunar_location_buttons_forall (GtkContainer *container,
                                gboolean      include_internals,
                                GtkCallback   callback,
                                gpointer      callback_data)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (container);
  GtkWidget             *child;
  GList                 *lp;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));
  _thunar_return_if_fail (callback != NULL);

  for (lp = buttons->list; lp != NULL; )
    {
      child = GTK_WIDGET (lp->data);
      lp = lp->next;

      (*callback) (child, callback_data);
    }

  if (buttons->left_slider != NULL && include_internals)
    (*callback) (buttons->left_slider, callback_data);

  if (buttons->right_slider != NULL && include_internals)
    (*callback) (buttons->right_slider, callback_data);

  if (buttons->filler_widget != NULL && include_internals)
    (*callback) (buttons->filler_widget, callback_data);
}



static GtkWidget*
thunar_location_buttons_make_button (ThunarLocationButtons *buttons,
                                     ThunarFile            *file)
{
  GtkWidget *button;

  /* allocate the button */
  button = thunar_location_button_new ();

  /* connect to the file */
  thunar_location_button_set_file (THUNAR_LOCATION_BUTTON (button), file);

  /* the current directory is active */
  gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), (file == buttons->current_directory));

  /* connect signal handlers */
  g_signal_connect (G_OBJECT (button), "location-button-clicked", G_CALLBACK (thunar_location_buttons_clicked), buttons);
  g_signal_connect (G_OBJECT (button), "gone", G_CALLBACK (thunar_location_buttons_gone), buttons);
  g_signal_connect (G_OBJECT (button), "popup-menu", G_CALLBACK (thunar_location_buttons_context_menu), buttons);

  return button;
}



static void
thunar_location_buttons_remove_1 (GtkContainer *container,
                                  GtkWidget    *widget)
{
  gboolean need_resize = gtk_widget_get_visible (widget);
  gtk_widget_unparent (widget);
  if (G_LIKELY (need_resize))
    gtk_widget_queue_resize (GTK_WIDGET (container));
}



static gboolean
thunar_location_buttons_scroll_timeout (gpointer user_data)
{
  ThunarLocationButtons *buttons = THUNAR_LOCATION_BUTTONS (user_data);

  if (gtk_widget_has_focus (buttons->left_slider))
    thunar_location_buttons_scroll_left (buttons->left_slider, buttons);
  else if (gtk_widget_has_focus (buttons->right_slider))
    thunar_location_buttons_scroll_right (buttons->right_slider, buttons);

  return TRUE;
}



static void
thunar_location_buttons_scroll_timeout_destroy (gpointer user_data)
{
  THUNAR_LOCATION_BUTTONS (user_data)->scroll_timeout_id = 0;
}



static void
thunar_location_buttons_stop_scrolling (ThunarLocationButtons *buttons)
{
  if (buttons->scroll_timeout_id != 0)
    g_source_remove (buttons->scroll_timeout_id);
}



static void
thunar_location_buttons_update_sliders (ThunarLocationButtons *buttons)
{
  GtkWidget *button;

  if (G_LIKELY (buttons->list != NULL))
    {
      button = GTK_WIDGET (buttons->list->data);
      if (gtk_widget_get_child_visible (button))
        {
          gtk_widget_set_sensitive (buttons->right_slider, FALSE);
          thunar_location_buttons_stop_scrolling (buttons);
        }
      else
        {
          gtk_widget_set_sensitive (buttons->right_slider, TRUE);
        }

      button = GTK_WIDGET (g_list_last (buttons->list)->data);
      if (gtk_widget_get_child_visible (button) && buttons->fake_root_button == NULL)
        {
          gtk_widget_set_sensitive (buttons->left_slider, FALSE);
          thunar_location_buttons_stop_scrolling (buttons);
        }
      else
        {
          gtk_widget_set_sensitive (buttons->left_slider, TRUE);
        }
    }
}



static gboolean
thunar_location_buttons_slider_button_press (GtkWidget             *button,
                                             GdkEventButton        *event,
                                             ThunarLocationButtons *buttons)
{
  if (!gtk_widget_has_focus (button))
    gtk_widget_grab_focus (button);

  if (event->type != GDK_BUTTON_PRESS || event->button != 1)
    return FALSE;

  buttons->ignore_click = FALSE;

  if (button == buttons->left_slider)
    thunar_location_buttons_scroll_left (button, buttons);
  else if (button == buttons->right_slider)
    thunar_location_buttons_scroll_right (button, buttons);

  if (G_LIKELY (buttons->scroll_timeout_id == 0))
    {
      buttons->scroll_timeout_id = gdk_threads_add_timeout_full (G_PRIORITY_LOW,
                                                                 THUNAR_LOCATION_BUTTONS_SCROLL_TIMEOUT,
                                                                 thunar_location_buttons_scroll_timeout, buttons,
                                                                 thunar_location_buttons_scroll_timeout_destroy);
    }

  return FALSE;
}



static gboolean
thunar_location_buttons_slider_button_release (GtkWidget             *button,
                                               GdkEventButton        *event,
                                               ThunarLocationButtons *buttons)
{
  if (event->type == GDK_BUTTON_RELEASE)
    {
      thunar_location_buttons_stop_scrolling (buttons);
      buttons->ignore_click = TRUE;
    }

  return FALSE;
}



static void
thunar_location_buttons_scroll_left (GtkWidget             *button,
                                     ThunarLocationButtons *buttons)
{
  if (G_UNLIKELY (buttons->ignore_click))
    {
      buttons->ignore_click = FALSE;
      return;
    }

  gtk_widget_queue_resize (GTK_WIDGET (buttons));
  if (buttons->first_visible_button->next != NULL)
    {
      if (buttons->first_visible_button == buttons->fake_root_button)
        buttons->fake_root_button = NULL;
      buttons->first_visible_button = buttons->first_visible_button->next;
      buttons->last_visible_button = NULL;
    }
}



static void
thunar_location_buttons_scroll_right (GtkWidget             *button,
                                      ThunarLocationButtons *buttons)
{
  if (G_UNLIKELY (buttons->ignore_click))
    {
      buttons->ignore_click = FALSE;
      return;
    }

  gtk_widget_queue_resize (GTK_WIDGET (buttons));
  if (buttons->last_visible_button->prev != NULL)
    {
      buttons->last_visible_button = buttons->last_visible_button->prev;
      buttons->first_visible_button = NULL;
    }
}



static void
thunar_location_buttons_clicked (ThunarLocationButton  *button,
                                 gboolean               open_in_tab,
                                 ThunarLocationButtons *buttons)
{
  ThunarFile *directory;
  GList      *lp;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTON (button));
  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));

  /* determine the directory associated with the clicked button */
  directory = thunar_location_button_get_file (button);

  /* open in tab */
  if (open_in_tab)
    {
      thunar_navigator_open_new_tab (THUNAR_NAVIGATOR (buttons), directory);
      return;
    }

  /* disconnect from previous current directory (if any) */
  if (G_LIKELY (buttons->current_directory != NULL))
    g_object_unref (G_OBJECT (buttons->current_directory));

  /* setup the new current directory */
  buttons->current_directory = directory;

  /* take a reference on the new directory (if any) */
  if (G_LIKELY (directory != NULL))
    g_object_ref (G_OBJECT (directory));

  /* check if the button is visible on the button bar */
  if (!gtk_widget_get_child_visible (GTK_WIDGET (button)))
    {
      for (lp = buttons->list; lp != NULL; lp = lp->next)
        {
          if( lp->data == button )
            buttons->first_visible_button = lp;
        }
      buttons->last_visible_button = NULL;
    }

  /* update all buttons */
  for (lp = buttons->list; lp != NULL; lp = lp->next)
    {
      /* determine the directory for this button */
      button = THUNAR_LOCATION_BUTTON (lp->data);
      directory = thunar_location_button_get_file (button);

      /* update the location button state (making sure to not recurse with the "clicked" handler) */
      g_signal_handlers_block_by_func (G_OBJECT (button), thunar_location_buttons_clicked, buttons);
      gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (button), directory == buttons->current_directory);
      g_signal_handlers_unblock_by_func (G_OBJECT (button), thunar_location_buttons_clicked, buttons);
    }

  /* notify the surrounding module that we want to change to a different directory.  */
  thunar_navigator_change_directory (THUNAR_NAVIGATOR (buttons), buttons->current_directory);
}



static void
thunar_location_buttons_gone (ThunarLocationButton  *button,
                              ThunarLocationButtons *buttons)
{
  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTON (button));
  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));
  _thunar_return_if_fail (g_list_find (buttons->list, button) != NULL);

  /* drop all buttons up to the button that emitted the "gone" signal */
  while (buttons->list->data != button)
    gtk_widget_destroy (buttons->list->data);

  /* drop the button itself */
  gtk_widget_destroy (GTK_WIDGET (button));
}



static gboolean
thunar_location_buttons_context_menu (ThunarLocationButton  *button,
                                      ThunarLocationButtons *buttons)
{
  ThunarClipboardManager *clipboard;
  const gchar            *display_name;
  ThunarFile             *file;
  GtkAction              *action;
  GtkWidget              *menu, *item;

  _thunar_return_val_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_LOCATION_BUTTON (button), FALSE);

  /* determine the file for the button */
  file = thunar_location_button_get_file (button);
  if (G_LIKELY (file != NULL))
    {
      menu = gtk_menu_new ();

      /* determine the display name of the file */
      display_name = thunar_file_get_display_name (file);

      /* be sure to keep a reference on the navigation bar */
      g_object_ref (G_OBJECT (buttons));

      /* grab a reference on the clipboard manager for this display */
      clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (GTK_WIDGET (buttons)));

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      /* setup the "Open" action */
      action = gtk_action_group_get_action (buttons->action_group, "location-buttons-open");
      thunar_gtk_action_set_tooltip (action, _("Open \"%s\" in this window"), display_name);
      g_object_set_qdata_full (G_OBJECT (action), thunar_file_quark, g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);
      gtk_action_set_sensitive (action, (file != buttons->current_directory));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_action_create_menu_item (action));

      /* setup the "Open in New Tab" action */
      action = gtk_action_group_get_action (buttons->action_group, "location-buttons-open-in-new-tab");
      thunar_gtk_action_set_tooltip (action, _("Open \"%s\" in a new tab"), display_name);
      g_object_set_qdata_full (G_OBJECT (action), thunar_file_quark, g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_action_create_menu_item (action));

      /* setup the "Open in New Window" action */
      action = gtk_action_group_get_action (buttons->action_group, "location-buttons-open-in-new-window");
      thunar_gtk_action_set_tooltip (action, _("Open \"%s\" in a new window"), display_name);
      g_object_set_qdata_full (G_OBJECT (action), thunar_file_quark, g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_action_create_menu_item (action));

      /* setup the "Create Folder..." action */
      action = gtk_action_group_get_action (buttons->action_group, "location-buttons-create-folder");
      thunar_gtk_action_set_tooltip (action, _("Create a new folder in \"%s\""), display_name);
      g_object_set_qdata_full (G_OBJECT (action), thunar_file_quark, g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);
      gtk_action_set_sensitive (action, thunar_file_is_writable (file));
      gtk_action_set_visible (action, !thunar_file_is_trashed (file));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_action_create_menu_item (action));

      /* setup the "Empty Trash" action */
      action = gtk_action_group_get_action (buttons->action_group, "location-buttons-empty-trash");
      gtk_action_set_visible (action, (thunar_file_is_root (file) && thunar_file_is_trashed (file)));
      gtk_action_set_sensitive (action, (thunar_file_get_size (file) > 0));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_action_create_menu_item (action));
G_GNUC_END_IGNORE_DEPRECATIONS

      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

G_GNUC_BEGIN_IGNORE_DEPRECATIONS
      /* setup the "Paste Into Folder" action */
      action = gtk_action_group_get_action (buttons->action_group, "location-buttons-paste-into-folder");
      thunar_gtk_action_set_tooltip (action, _("Move or copy files previously selected by a Cut or Copy command into \"%s\""), display_name);
      g_object_set_qdata_full (G_OBJECT (action), thunar_file_quark, g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);
      gtk_action_set_sensitive (action, thunar_clipboard_manager_get_can_paste (clipboard));
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_action_create_menu_item (action));

      item = gtk_separator_menu_item_new ();
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);

      /* setup the "Properties..." action */
      action = gtk_action_group_get_action (buttons->action_group, "location-buttons-properties");
      thunar_gtk_action_set_tooltip (action, _("View the properties of the folder \"%s\""), display_name);
      g_object_set_qdata_full (G_OBJECT (action), thunar_file_quark, g_object_ref (G_OBJECT (file)), (GDestroyNotify) g_object_unref);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_action_create_menu_item (action));
G_GNUC_END_IGNORE_DEPRECATIONS

      /* run the menu (takes over the floating of menu) */
      thunar_gtk_menu_run (GTK_MENU (menu));

      /* cleanup */
      g_object_unref (buttons);
      g_object_unref (clipboard);

      return TRUE;
    }

  return FALSE;
}



static void
thunar_location_buttons_action_create_folder (GtkAction             *action,
                                              ThunarLocationButtons *buttons)
{
  ThunarApplication *application;
  ThunarFile        *directory;
  GList              path_list;
  gchar             *name;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _thunar_return_if_fail (GTK_IS_ACTION (action));
G_GNUC_END_IGNORE_DEPRECATIONS

  /* determine the directory for the action */
  directory = g_object_get_qdata (G_OBJECT (action), thunar_file_quark);
  if (G_UNLIKELY (directory == NULL))
    return;

  /* ask the user to enter a name for the new folder */
  name = thunar_show_create_dialog (GTK_WIDGET (buttons),
                                    "inode/directory",
                                    _("New Folder"),
                                    _("Create New Folder"));
  if (G_LIKELY (name != NULL))
    {
      /* fake the path list */
      path_list.data = g_file_resolve_relative_path (thunar_file_get_file (directory), name);
      path_list.next = path_list.prev = NULL;

      /* launch the operation */
      application = thunar_application_get ();
      thunar_application_mkdir (application, GTK_WIDGET (buttons), &path_list, NULL);
      g_object_unref (G_OBJECT (application));

      /* release the path */
      g_object_unref (path_list.data);

      /* release the file name */
      g_free (name);
    }
}



static void
thunar_location_buttons_action_down_folder (GtkAction             *action,
                                            ThunarLocationButtons *buttons)
{
  GList *lp;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _thunar_return_if_fail (GTK_IS_ACTION (action));
G_GNUC_END_IGNORE_DEPRECATIONS

  /* lookup the active button */
  for (lp = buttons->list; lp != NULL; lp = lp->next)
    if (gtk_toggle_button_get_active (GTK_TOGGLE_BUTTON (lp->data)))
      {
        /* check if we have a folder below that one */
        if (G_LIKELY (lp->prev != NULL))
          {
            /* enter that folder */
            gtk_button_clicked (GTK_BUTTON (lp->prev->data));
          }

        break;
      }
}



static void
thunar_location_buttons_action_empty_trash (GtkAction             *action,
                                            ThunarLocationButtons *buttons)
{
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _thunar_return_if_fail (GTK_IS_ACTION (action));
G_GNUC_END_IGNORE_DEPRECATIONS

  /* launch the operation */
  application = thunar_application_get ();
  thunar_application_empty_trash (application, GTK_WIDGET (buttons), NULL);
  g_object_unref (G_OBJECT (application));
}



static void
thunar_location_buttons_action_open (GtkAction             *action,
                                     ThunarLocationButtons *buttons)
{
  ThunarFile *directory;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _thunar_return_if_fail (GTK_IS_ACTION (action));
G_GNUC_END_IGNORE_DEPRECATIONS

  /* determine the directory for the action */
  directory = g_object_get_qdata (G_OBJECT (action), thunar_file_quark);
  if (G_LIKELY (directory != NULL && thunar_file_is_directory (directory)))
    {
      /* open the folder in this window */
      thunar_navigator_change_directory (THUNAR_NAVIGATOR (buttons), directory);
    }
}



static void
thunar_location_buttons_action_open_in_new_tab (GtkAction             *action,
                                                ThunarLocationButtons *buttons)
{
  ThunarFile *directory;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _thunar_return_if_fail (GTK_IS_ACTION (action));
G_GNUC_END_IGNORE_DEPRECATIONS

  /* determine the directory for the action */
  directory = g_object_get_qdata (G_OBJECT (action), thunar_file_quark);
  if (G_LIKELY (directory != NULL))
    {
      /* open tab in thsi window */
      thunar_navigator_open_new_tab (THUNAR_NAVIGATOR (buttons), directory);
    }
}



static void
thunar_location_buttons_action_open_in_new_window (GtkAction             *action,
                                                   ThunarLocationButtons *buttons)
{
  ThunarApplication *application;
  ThunarFile        *directory;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _thunar_return_if_fail (GTK_IS_ACTION (action));
G_GNUC_END_IGNORE_DEPRECATIONS

  /* determine the directory for the action */
  directory = g_object_get_qdata (G_OBJECT (action), thunar_file_quark);
  if (G_LIKELY (directory != NULL))
    {
      /* open a new window for the directory */
      application = thunar_application_get ();
      thunar_application_open_window (application, directory, gtk_widget_get_screen (GTK_WIDGET (buttons)), NULL);
      g_object_unref (G_OBJECT (application));
    }
}



static void
thunar_location_buttons_action_paste_into_folder (GtkAction             *action,
                                                  ThunarLocationButtons *buttons)
{
  ThunarClipboardManager *clipboard;
  ThunarFile             *directory;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _thunar_return_if_fail (GTK_IS_ACTION (action));
G_GNUC_END_IGNORE_DEPRECATIONS

  /* determine the directory for the action */
  directory = g_object_get_qdata (G_OBJECT (action), thunar_file_quark);
  if (G_LIKELY (directory != NULL))
    {
      /* paste files from the clipboard to the folder represented by this button */
      clipboard = thunar_clipboard_manager_get_for_display (gtk_widget_get_display (GTK_WIDGET (buttons)));
      thunar_clipboard_manager_paste_files (clipboard, thunar_file_get_file (directory), GTK_WIDGET (buttons), NULL);
      g_object_unref (G_OBJECT (clipboard));
    }
}



static void
thunar_location_buttons_action_properties (GtkAction             *action,
                                           ThunarLocationButtons *buttons)
{
  ThunarFile *directory;
  GtkWidget  *toplevel;
  GtkWidget  *dialog;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTONS (buttons));
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  _thunar_return_if_fail (GTK_IS_ACTION (action));
G_GNUC_END_IGNORE_DEPRECATIONS

  /* determine the directory for the action */
  directory = g_object_get_qdata (G_OBJECT (action), thunar_file_quark);
  if (G_LIKELY (directory != NULL))
    {
      /* determine the toplevel window */
      toplevel = gtk_widget_get_toplevel (GTK_WIDGET (buttons));
      if (G_LIKELY (toplevel != NULL && gtk_widget_get_toplevel (toplevel)))
        {
          /* popup the properties dialog */
          dialog = thunar_properties_dialog_new (GTK_WINDOW (toplevel));
          thunar_properties_dialog_set_file (THUNAR_PROPERTIES_DIALOG (dialog), directory);
          gtk_widget_show (dialog);
        }
    }
}

