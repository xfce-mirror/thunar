/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2015 Jonas Kümmerlin <rgcjonas@gmail.com>
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

#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-shortcuts-pane.h>
#include <thunar/thunar-shortcuts-pane-ui.h>
#include <thunar/thunar-side-pane.h>
#include <thunar/thunar-stock.h>
#include <thunar/thunar-browser.h>
#include <thunar/thunar-dialogs.h>
#include <thunar/thunar-application.h>
#include <thunar/thunar-dnd.h>
#include <thunar/thunar-gtk-extensions.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-util.h>



enum
{
  PROP_0,
  PROP_CURRENT_DIRECTORY,
  PROP_SELECTED_FILES,
  PROP_SHOW_HIDDEN,
  PROP_UI_MANAGER,
};



static void          thunar_shortcuts_pane_component_init        (ThunarComponentIface     *iface);
static void          thunar_shortcuts_pane_navigator_init        (ThunarNavigatorIface     *iface);
static void          thunar_shortcuts_pane_side_pane_init        (ThunarSidePaneIface      *iface);
static void          thunar_shortcuts_pane_dispose               (GObject                  *object);
static void          thunar_shortcuts_pane_finalize              (GObject                  *object);
static void          thunar_shortcuts_pane_get_property          (GObject                  *object,
                                                                  guint                     prop_id,
                                                                  GValue                   *value,
                                                                  GParamSpec               *pspec);
static void          thunar_shortcuts_pane_set_property          (GObject                  *object,
                                                                  guint                     prop_id,
                                                                  const GValue             *value,
                                                                  GParamSpec               *pspec);
static ThunarFile   *thunar_shortcuts_pane_get_current_directory (ThunarNavigator          *navigator);
static void          thunar_shortcuts_pane_set_current_directory (ThunarNavigator          *navigator,
                                                                  ThunarFile               *current_directory);
static GList        *thunar_shortcuts_pane_get_selected_files    (ThunarComponent          *component);
static void          thunar_shortcuts_pane_set_selected_files    (ThunarComponent          *component,
                                                                  GList                    *selected_files);
static GtkUIManager *thunar_shortcuts_pane_get_ui_manager        (ThunarComponent          *component);
static void          thunar_shortcuts_pane_set_ui_manager        (ThunarComponent          *component,
                                                                  GtkUIManager             *ui_manager);
static void          thunar_shortcuts_pane_action_shortcuts_add  (GtkAction                *action,
                                                                  ThunarShortcutsPane      *shortcuts_pane);
static void          thunar_shortcuts_pane_open_location         (GtkPlacesSidebar         *sidebar,
                                                                  GFile                    *location,
                                                                  GtkPlacesOpenFlags        flags,
                                                                  ThunarShortcutsPane      *shortcuts_pane);
static gint          thunar_shortcuts_pane_drag_action_requested (GtkPlacesSidebar         *sidebar,
                                                                  GdkDragContext           *context,
                                                                  GFile                    *dest_file,
                                                                  GList                    *source_file_list,
                                                                  ThunarShortcutsPane      *shortcuts_pane);
static void          thunar_shortcuts_pane_drag_perform_drop     (GtkPlacesSidebar         *sidebar,
                                                                  GFile                    *dest_file,
                                                                  GList                    *source_file_list,
                                                                  GdkDragAction             action,
                                                                  ThunarShortcutsPane      *shortcuts_pane);
static gint          thunar_shortcuts_pane_drag_action_ask       (GtkPlacesSidebar         *sidebar,
                                                                  GdkDragAction             actions,
                                                                  ThunarShortcutsPane      *shortcuts_pane);




struct _ThunarShortcutsPaneClass
{
  GtkBinClass __parent__;
};

struct _ThunarShortcutsPane
{
  GtkBin __parent__;

  ThunarFile       *current_directory;
  GList            *selected_files;

  GtkActionGroup   *action_group;
  GtkUIManager     *ui_manager;
  guint             ui_merge_id;

  GtkWidget        *places;

  guint             idle_select_directory;
};



static const GtkActionEntry action_entries[] =
{
  { "sendto-shortcuts", THUNAR_STOCK_SHORTCUTS, "", NULL, NULL, G_CALLBACK (thunar_shortcuts_pane_action_shortcuts_add), },
};



G_DEFINE_TYPE_WITH_CODE (ThunarShortcutsPane, thunar_shortcuts_pane, GTK_TYPE_BIN,
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_shortcuts_pane_navigator_init)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_COMPONENT, thunar_shortcuts_pane_component_init)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_SIDE_PANE, thunar_shortcuts_pane_side_pane_init)
    G_IMPLEMENT_INTERFACE (THUNAR_TYPE_BROWSER, NULL))



static void
thunar_shortcuts_pane_class_init (ThunarShortcutsPaneClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_shortcuts_pane_dispose;
  gobject_class->finalize = thunar_shortcuts_pane_finalize;
  gobject_class->get_property = thunar_shortcuts_pane_get_property;
  gobject_class->set_property = thunar_shortcuts_pane_set_property;

  /* override ThunarNavigator's properties */
  g_object_class_override_property (gobject_class, PROP_CURRENT_DIRECTORY, "current-directory");

  /* override ThunarComponent's properties */
  g_object_class_override_property (gobject_class, PROP_SELECTED_FILES, "selected-files");
  g_object_class_override_property (gobject_class, PROP_UI_MANAGER, "ui-manager");

  /* override ThunarSidePane's properties */
  g_object_class_override_property (gobject_class, PROP_SHOW_HIDDEN, "show-hidden");
}



static void
thunar_shortcuts_pane_component_init (ThunarComponentIface *iface)
{
  iface->get_selected_files = thunar_shortcuts_pane_get_selected_files;
  iface->set_selected_files = thunar_shortcuts_pane_set_selected_files;
  iface->get_ui_manager = thunar_shortcuts_pane_get_ui_manager;
  iface->set_ui_manager = thunar_shortcuts_pane_set_ui_manager;
}



static void
thunar_shortcuts_pane_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_shortcuts_pane_get_current_directory;
  iface->set_current_directory = thunar_shortcuts_pane_set_current_directory;
}



static void
thunar_shortcuts_pane_side_pane_init (ThunarSidePaneIface *iface)
{
  iface->get_show_hidden = (gpointer) exo_noop_false;
  iface->set_show_hidden = (gpointer) exo_noop;
}



static void
thunar_shortcuts_pane_init (ThunarShortcutsPane *shortcuts_pane)
{
  /* setup the action group for the shortcuts actions */
  shortcuts_pane->action_group = gtk_action_group_new ("ThunarShortcutsPane");
  gtk_action_group_set_translation_domain (shortcuts_pane->action_group, GETTEXT_PACKAGE);
  gtk_action_group_add_actions (shortcuts_pane->action_group, action_entries, G_N_ELEMENTS (action_entries), shortcuts_pane);

  /* configure the places sidebar */
  shortcuts_pane->places = gtk_places_sidebar_new ();
  gtk_places_sidebar_set_open_flags (GTK_PLACES_SIDEBAR (shortcuts_pane->places), GTK_PLACES_OPEN_NORMAL|GTK_PLACES_OPEN_NEW_TAB|GTK_PLACES_OPEN_NEW_WINDOW);

  g_signal_connect (shortcuts_pane->places, "open-location", G_CALLBACK (thunar_shortcuts_pane_open_location), shortcuts_pane);
  g_signal_connect (shortcuts_pane->places, "drag-action-requested", G_CALLBACK (thunar_shortcuts_pane_drag_action_requested), shortcuts_pane);
  g_signal_connect (shortcuts_pane->places, "drag-perform-drop", G_CALLBACK (thunar_shortcuts_pane_drag_perform_drop), shortcuts_pane);
  g_signal_connect (shortcuts_pane->places, "drag-action-ask", G_CALLBACK (thunar_shortcuts_pane_drag_action_ask), shortcuts_pane);

  gtk_container_add (GTK_CONTAINER (shortcuts_pane), shortcuts_pane->places);
  gtk_widget_show (shortcuts_pane->places);
}



static void
thunar_shortcuts_pane_dispose (GObject *object)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (object);

  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (shortcuts_pane), NULL);
  thunar_component_set_selected_files (THUNAR_COMPONENT (shortcuts_pane), NULL);
  thunar_component_set_ui_manager (THUNAR_COMPONENT (shortcuts_pane), NULL);

  (*G_OBJECT_CLASS (thunar_shortcuts_pane_parent_class)->dispose) (object);
}



static void
thunar_shortcuts_pane_finalize (GObject *object)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (object);

  /* release our action group */
  g_object_unref (G_OBJECT (shortcuts_pane->action_group));

  (*G_OBJECT_CLASS (thunar_shortcuts_pane_parent_class)->finalize) (object);
}



static void
thunar_shortcuts_pane_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;

    case PROP_SELECTED_FILES:
      g_value_set_boxed (value, thunar_component_get_selected_files (THUNAR_COMPONENT (object)));
      break;

    case PROP_SHOW_HIDDEN:
      g_value_set_boolean (value, thunar_side_pane_get_show_hidden (THUNAR_SIDE_PANE (object)));
      break;

    case PROP_UI_MANAGER:
      g_value_set_object (value, thunar_component_get_ui_manager (THUNAR_COMPONENT (object)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_shortcuts_pane_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  switch (prop_id)
    {
    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;

    case PROP_SELECTED_FILES:
      thunar_component_set_selected_files (THUNAR_COMPONENT (object), g_value_get_boxed (value));
      break;

    case PROP_SHOW_HIDDEN:
      thunar_side_pane_set_show_hidden (THUNAR_SIDE_PANE (object), g_value_get_boolean (value));
      break;

    case PROP_UI_MANAGER:
      thunar_component_set_ui_manager (THUNAR_COMPONENT (object), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_shortcuts_pane_get_current_directory (ThunarNavigator *navigator)
{
  return THUNAR_SHORTCUTS_PANE (navigator)->current_directory;
}



static gboolean
thunar_shortcuts_pane_set_current_directory_idle (gpointer data)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (data);

  if (shortcuts_pane->current_directory != NULL)
    {
      gtk_places_sidebar_set_location (GTK_PLACES_SIDEBAR (shortcuts_pane->places),
                                       thunar_file_get_file (shortcuts_pane->current_directory));
    }

  /* unset id */
  shortcuts_pane->idle_select_directory = 0;

  return FALSE;
}



static void
thunar_shortcuts_pane_set_current_directory (ThunarNavigator *navigator,
                                             ThunarFile      *current_directory)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (navigator);

  /* disconnect from the previously set current directory */
  if (G_LIKELY (shortcuts_pane->current_directory != NULL))
    g_object_unref (G_OBJECT (shortcuts_pane->current_directory));

  /* remove pending timeout */
  if (shortcuts_pane->idle_select_directory != 0)
    {
      g_source_remove (shortcuts_pane->idle_select_directory);
      shortcuts_pane->idle_select_directory = 0;
    }

  /* activate the new directory */
  shortcuts_pane->current_directory = current_directory;

  /* connect to the new directory */
  if (G_LIKELY (current_directory != NULL))
    {
      /* take a reference on the new directory */
      g_object_ref (G_OBJECT (current_directory));

      /* start idle to select item in sidepane (this to also make
       * the selection work when the bookmarks are loaded idle) */
      shortcuts_pane->idle_select_directory =
        g_idle_add_full (G_PRIORITY_LOW, thunar_shortcuts_pane_set_current_directory_idle,
                         shortcuts_pane, NULL);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (shortcuts_pane), "current-directory");
}



static GList*
thunar_shortcuts_pane_get_selected_files (ThunarComponent *component)
{
  return THUNAR_SHORTCUTS_PANE (component)->selected_files;
}



static void
thunar_shortcuts_pane_set_selected_files (ThunarComponent *component,
                                          GList           *selected_files)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (component);
  GtkTreeModel        *model;
  GtkTreeModel        *child_model;
  GtkAction           *action;
  GList               *lp;
  gint                 n;

  /* disconnect from the previously selected files... */
  thunar_g_file_list_free (shortcuts_pane->selected_files);

  /* ...and take a copy of the newly selected files */
  shortcuts_pane->selected_files = thunar_g_file_list_copy (selected_files);

  /* check if the selection contains only folders */
  for (lp = selected_files, n = 0; lp != NULL; lp = lp->next, ++n)
    if (!thunar_file_is_directory (lp->data))
      break;

  /* change the visibility of the "shortcuts-add" action appropriately */
  action = gtk_action_group_get_action (shortcuts_pane->action_group, "sendto-shortcuts");
  if (lp == NULL && selected_files != NULL)
    {
      /* check if atleast one of the selected folders is not already present in the model */
      /* TODO */

      /* display the action and change the label appropriately */
      g_object_set (G_OBJECT (action),
                    "label", ngettext ("Side Pane (Create Shortcut)", "Side Pane (Create Shortcuts)", n),
                    "sensitive", /*(lp != NULL)*/ TRUE,
                    "tooltip", ngettext ("Add the selected folder to the shortcuts side pane",
                                         "Add the selected folders to the shortcuts side pane", n),
                    "visible", TRUE,
                    NULL);
    }
  else
    {
      /* hide the action */
      gtk_action_set_visible (action, FALSE);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (shortcuts_pane), "selected-files");
}



static GtkUIManager*
thunar_shortcuts_pane_get_ui_manager (ThunarComponent *component)
{
  return THUNAR_SHORTCUTS_PANE (component)->ui_manager;
}



static void
thunar_shortcuts_pane_set_ui_manager (ThunarComponent *component,
                                      GtkUIManager    *ui_manager)
{
  ThunarShortcutsPane *shortcuts_pane = THUNAR_SHORTCUTS_PANE (component);
  GError              *error = NULL;

  /* disconnect from the previous UI manager */
  if (G_UNLIKELY (shortcuts_pane->ui_manager != NULL))
    {
      /* drop our action group from the previous UI manager */
      gtk_ui_manager_remove_action_group (shortcuts_pane->ui_manager, shortcuts_pane->action_group);

      /* unmerge our ui controls from the previous UI manager */
      gtk_ui_manager_remove_ui (shortcuts_pane->ui_manager, shortcuts_pane->ui_merge_id);

      /* drop our reference on the previous UI manager */
      g_object_unref (G_OBJECT (shortcuts_pane->ui_manager));
    }

  /* activate the new UI manager */
  shortcuts_pane->ui_manager = ui_manager;

  /* connect to the new UI manager */
  if (G_LIKELY (ui_manager != NULL))
    {
      /* we keep a reference on the new manager */
      g_object_ref (G_OBJECT (ui_manager));

      /* add our action group to the new manager */
      gtk_ui_manager_insert_action_group (ui_manager, shortcuts_pane->action_group, -1);

      /* merge our UI control items with the new manager */
      shortcuts_pane->ui_merge_id = gtk_ui_manager_add_ui_from_string (ui_manager, thunar_shortcuts_pane_ui, thunar_shortcuts_pane_ui_length, &error);
      if (G_UNLIKELY (shortcuts_pane->ui_merge_id == 0))
        {
          g_error ("Failed to merge ThunarShortcutsPane menus: %s", error->message);
          g_error_free (error);
        }
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (shortcuts_pane), "ui-manager");
}

struct bookmark
{
  struct bookmark *next;
  GFile           *file;
  gchar           *name;
};

static void store_bookmark (GFile       *file,
                            const gchar *name,
                            gint         row_num,
                            gpointer     user_data)
{
  struct bookmark **plist = user_data;
  struct bookmark *bm   = g_slice_new0 (struct bookmark);

  _thunar_return_if_fail (plist != NULL);

  bm->next = *plist;
  bm->file = g_object_ref (file);
  bm->name = g_strdup (name);

  *plist = bm;
}



static void
thunar_shortcuts_pane_action_shortcuts_add (GtkAction           *action,
                                            ThunarShortcutsPane *shortcuts_pane)
{
  GFile           *bookmark_file = NULL, *parent = NULL;
  gchar           *uri = NULL;
  GString         *contents = NULL;
  struct bookmark *bm_list = NULL, *bm_i = NULL;
  GList           *lp = NULL;
  GError          *error = NULL;

  _thunar_return_if_fail (GTK_IS_ACTION (action));
  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_PANE (shortcuts_pane));

  bookmark_file = thunar_g_file_new_for_bookmarks ();

  thunar_util_load_bookmarks (bookmark_file, store_bookmark, &bm_list);

  /* add all selected folders to the list - unless they are already contained in there */
  /* FIXME: complexity */
  for (lp = shortcuts_pane->selected_files; lp != NULL; lp = lp->next)
    if (G_LIKELY (thunar_file_is_directory (lp->data)))
      {
        uri = thunar_file_dup_uri (lp->data);

        for (bm_i = bm_list; bm_i != NULL; bm_i = bm_i->next)
          {
            if (g_file_equal(thunar_file_get_file (lp->data), bm_i->file))
              break;
          }

        if (bm_i == NULL)
          {
            bm_i = g_slice_new0 (struct bookmark);
            bm_i->next = bm_list;
            bm_i->file = g_object_ref (thunar_file_get_file (lp->data));
            bm_i->name = NULL;

            bm_list = bm_i;
          }
      }

  /* create the new contents of the file */
  contents = g_string_new ("");

  for (bm_i = bm_list; bm_i != NULL; bm_i = bm_i->next)
    {
      uri = g_file_get_uri (bm_i->file);
      if (!uri)
        continue;

      /* Since we built the list in reverse, we will prepend instead of append
       * to the string. That way, it evens out. */
      g_string_prepend_c (contents, '\n');

      if (bm_i->name)
        {
          g_string_prepend (contents, bm_i->name);
          g_string_prepend_c (contents, ' ');
        }

      g_string_prepend (contents, uri);

      g_free (uri);
    }

  /* save the contents back into the file */
  parent = g_file_get_parent (bookmark_file);
  if (!g_file_make_directory_with_parents (parent, NULL, &error))
    {
      if (g_error_matches (error, G_IO_ERROR, G_IO_ERROR_EXISTS))
        g_clear_error (&error);
      else
        goto out;
    }

  if (!g_file_replace_contents (bookmark_file,
                                contents->str,
                                contents->len,
                                NULL, FALSE, 0, NULL,
                                NULL, &error))

  /* update the user interface to reflect the new action state */
  lp = thunar_g_file_list_copy (shortcuts_pane->selected_files);
  thunar_component_set_selected_files (THUNAR_COMPONENT (shortcuts_pane), lp);
  thunar_g_file_list_free (lp);

out:
  if (error)
    {
      g_critical ("%s", error->message);
      g_error_free (error);
    }
  g_string_free (contents, TRUE);
  g_object_unref (parent);
  g_object_unref (bookmark_file);

  for (bm_i = bm_list; bm_i != NULL; bm_i = bm_list)
    {
      g_object_unref (bm_i->file);
      g_free (bm_i->name);

      bm_list = bm_i->next;
      g_slice_free (struct bookmark, bm_i);
    }
}



static void
thunar_shortcuts_pane_poke_location_finish (ThunarBrowser *browser,
                                            GFile         *location,
                                            ThunarFile    *file,
                                            ThunarFile    *target_file,
                                            GError        *error,
                                            gpointer       user_data)
{
  gchar *name;
  GtkPlacesOpenFlags open_in;
  ThunarApplication *application;

  _thunar_return_if_fail (THUNAR_IS_SHORTCUTS_PANE (browser));
  _thunar_return_if_fail (G_IS_FILE (location));

  open_in = (GtkPlacesOpenFlags)GPOINTER_TO_INT (user_data);

  /* sotre the new file in the shortcuts model */
  if (error == NULL)
    {
      if (open_in == GTK_PLACES_OPEN_NEW_WINDOW)
        {
          /* open a new window for the target folder */
          application = thunar_application_get ();
          thunar_application_open_window (application, target_file,
                                          gtk_widget_get_screen (GTK_WIDGET (browser)), NULL);
          g_object_unref (application);
        }
      else if (open_in == GTK_PLACES_OPEN_NEW_TAB)
        {
          thunar_navigator_open_new_tab (THUNAR_NAVIGATOR (browser), target_file);
        }
      else if (thunar_file_check_loaded (target_file))
        {
          thunar_navigator_change_directory (THUNAR_NAVIGATOR (browser), target_file);
        }
      else
        {
          g_warning ("WTF: File to be activated but not loaded!");
        }
    }
  else
    {
      name = thunar_g_file_get_display_name_remote (location);
      thunar_dialogs_show_error (GTK_WIDGET (browser), error, _("Failed to open \"%s\""), name);
      g_free (name);
    }
}



static void
thunar_shortcuts_pane_open_location (GtkPlacesSidebar    *sidebar,
                                     GFile               *location,
                                     GtkPlacesOpenFlags   flags,
                                     ThunarShortcutsPane *shortcuts_pane)
{
  thunar_browser_poke_location (THUNAR_BROWSER (shortcuts_pane), location,
                                GTK_WIDGET (shortcuts_pane),
                                thunar_shortcuts_pane_poke_location_finish,
                                GINT_TO_POINTER (flags));
}



static gint thunar_shortcuts_pane_drag_action_requested (GtkPlacesSidebar    *sidebar,
                                                         GdkDragContext      *context,
                                                         GFile               *dest_file,
                                                         GList               *source_file_list,
                                                         ThunarShortcutsPane *shortcuts_pane)
{
  ThunarFile *th_dest_file;
  GdkDragAction suggested_action;

  /* create ThunarFile* instances for the given file */
  th_dest_file = thunar_file_get (dest_file, NULL);

  _thunar_return_val_if_fail (th_dest_file != NULL, 0);

  /* check for allowed drop actions */
  thunar_file_accepts_drop (th_dest_file, source_file_list, context, &suggested_action);

  /* cleanup */
  g_object_unref (th_dest_file);

  return suggested_action;
}



void thunar_shortcuts_pane_drag_perform_drop (GtkPlacesSidebar    *sidebar,
                                              GFile               *dest_file,
                                              GList               *source_file_list,
                                              GdkDragAction        action,
                                              ThunarShortcutsPane *shortcuts_pane)
{
  ThunarFile *th_dest_file;

  if (G_UNLIKELY ((action & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)) == 0))
    return;

  /* get the thunar file */
  th_dest_file = thunar_file_get (dest_file, NULL);

  _thunar_return_if_fail (th_dest_file != NULL);

  /* perform the drop */
  thunar_dnd_perform (GTK_WIDGET (shortcuts_pane), th_dest_file, source_file_list, action, NULL);

  /* cleanup */
  g_object_unref (th_dest_file);
}

static void drag_ask_menu_copy_activated (GtkMenuItem   *menuitem,
                                          GdkDragAction *out_action)
{
  *out_action = GDK_ACTION_COPY;
}
static void drag_ask_menu_link_activated (GtkMenuItem   *menuitem,
                                          GdkDragAction *out_action)
{
  *out_action = GDK_ACTION_LINK;
}
static void drag_ask_menu_move_activated (GtkMenuItem   *menuitem,
                                          GdkDragAction *out_action)
{
  *out_action = GDK_ACTION_MOVE;
}

static gint thunar_shortcuts_pane_drag_action_ask (GtkPlacesSidebar    *sidebar,
                                                   GdkDragAction        actions,
                                                   ThunarShortcutsPane *shortcuts_pane)
{
  GtkWidget *menu, *item;
  GdkDragAction chosen_action = 0;

  /* prepare the popup menu */
  menu = gtk_menu_new ();

  /* append copy */
  if (actions & GDK_ACTION_COPY)
    {
      item = gtk_menu_item_new_with_mnemonic (_ ("_Copy here"));
      g_signal_connect (item, "activate", G_CALLBACK (drag_ask_menu_copy_activated), &chosen_action);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }
  if (actions & GDK_ACTION_MOVE)
    {
      item = gtk_menu_item_new_with_mnemonic (_ ("_Move here"));
      g_signal_connect (item, "activate", G_CALLBACK (drag_ask_menu_move_activated), &chosen_action);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }
  if (actions & GDK_ACTION_LINK)
    {
      item = gtk_menu_item_new_with_mnemonic (_ ("_Link here"));
      g_signal_connect (item, "activate", G_CALLBACK (drag_ask_menu_link_activated), &chosen_action);
      gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
      gtk_widget_show (item);
    }

  /* append the separator */
  item = gtk_separator_menu_item_new ();
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);


  /* append the cancel item */
  item = gtk_menu_item_new_with_mnemonic (_ ("_Cancel"));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);
  gtk_widget_show (item);

  /* run the menu on the widget's screen (takes over the floating reference of menu) */
  thunar_gtk_menu_run (GTK_MENU (menu), sidebar, NULL, NULL, 3, 0);

  return chosen_action;
}
