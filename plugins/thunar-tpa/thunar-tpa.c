/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2010 Nick Schermer <nick@xfce.org>
 * Copyright (c) 2010 Jannis Pohlmann <jannis@xfce.org>
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

#include <glib.h>

#include <gtk/gtk.h>

#include <libxfce4util/libxfce4util.h>

#include <libxfce4ui/libxfce4ui.h>

#include <libxfce4panel/libxfce4panel.h>
#include <libxfce4panel/xfce-panel-macros.h>

#include <thunar-tpa/thunar-tpa-bindings.h>

#ifdef LIBXFCE4PANEL_CHECK_VERSION
#if LIBXFCE4PANEL_CHECK_VERSION (4,9,0)
#define HAS_PANEL_49
#endif
#endif

typedef struct _ThunarTpaClass ThunarTpaClass;
typedef struct _ThunarTpa      ThunarTpa;



#define THUNAR_TYPE_TPA            (thunar_tpa_get_type ())
#define THUNAR_TPA(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_TPA, ThunarTpa))
#define THUNAR_TPA_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_TPA, ThunarTpaClass))
#define THUNAR_IS_TPA(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_TPA))
#define THUNAR_IS_TPA_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_TPA))
#define THUNAR_TPA_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_TPA, ThunarTpaClass))



GType           thunar_tpa_get_type            (void);
void            thunar_tpa_register_type       (XfcePanelTypeModule *type_module);
static void     thunar_tpa_finalize            (GObject             *object);
static void     thunar_tpa_construct           (XfcePanelPlugin     *panel_plugin);

#ifdef HAS_PANEL_49
static gboolean thunar_tpa_size_changed        (XfcePanelPlugin     *panel_plugin,
                                                gint                 size);
#endif

static void     thunar_tpa_error               (ThunarTpa           *plugin,
                                                GError              *error);
static void     thunar_tpa_state               (ThunarTpa           *plugin,
                                                gboolean             full);
static void     thunar_tpa_display_trash_reply (DBusGProxy          *proxy,
                                                GError              *error,
                                                gpointer             user_data);
static void     thunar_tpa_empty_trash_reply   (DBusGProxy          *proxy,
                                                GError              *error,
                                                gpointer             user_data);
static void     thunar_tpa_move_to_trash_reply (DBusGProxy          *proxy,
                                                GError              *error,
                                                gpointer             user_data);
static void     thunar_tpa_query_trash_reply   (DBusGProxy          *proxy,
                                                gboolean             full,
                                                GError              *error,
                                                gpointer             user_data);
static void     thunar_tpa_drag_data_received  (GtkWidget           *button,
                                                GdkDragContext      *context,
                                                gint                 x,
                                                gint                 y,
                                                GtkSelectionData    *selection_data,
                                                guint                info,
                                                guint                time,
                                                ThunarTpa           *plugin);
static gboolean thunar_tpa_enter_notify_event  (GtkWidget           *button,
                                                GdkEventCrossing    *event,
                                                ThunarTpa           *plugin);
static gboolean thunar_tpa_leave_notify_event  (GtkWidget           *button,
                                                GdkEventCrossing    *event,
                                                ThunarTpa           *plugin);
static void     thunar_tpa_trash_changed       (DBusGProxy          *proxy,
                                                gboolean             full,
                                                ThunarTpa           *plugin);
static void     thunar_tpa_display_trash       (ThunarTpa           *plugin);
static void     thunar_tpa_empty_trash         (ThunarTpa           *plugin);
static gboolean thunar_tpa_move_to_trash       (ThunarTpa           *plugin,
                                                const gchar        **uri_list);
static void     thunar_tpa_query_trash         (ThunarTpa           *plugin);



struct _ThunarTpaClass
{
  XfcePanelPluginClass __parent__;
};

struct _ThunarTpa
{
  XfcePanelPlugin __parent__;

  /* widgets */
  GtkWidget      *button;
  GtkWidget      *image;
  GtkWidget      *mi;

  DBusGProxy     *proxy;
  DBusGProxyCall *display_trash_call;
  DBusGProxyCall *empty_trash_call;
  DBusGProxyCall *move_to_trash_call;
  DBusGProxyCall *query_trash_call;
};

/* Target types for dropping to the trash can */
enum
{
  TARGET_TEXT_URI_LIST,
};

static const GtkTargetEntry drop_targets[] =
{
  { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
};



/* define the plugin */
XFCE_PANEL_DEFINE_PLUGIN (ThunarTpa, thunar_tpa)



static void
thunar_tpa_class_init (ThunarTpaClass *klass)
{
  XfcePanelPluginClass *plugin_class;
  GObjectClass         *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_tpa_finalize;

  plugin_class = XFCE_PANEL_PLUGIN_CLASS (klass);
  plugin_class->construct = thunar_tpa_construct;

#ifdef HAS_PANEL_49
  plugin_class->size_changed = thunar_tpa_size_changed;
#endif
}



static void
thunar_tpa_init (ThunarTpa *plugin)
{
  DBusGConnection *connection;
  GError          *err = NULL;

  /* setup the button for the trash plugin */
  plugin->button = xfce_create_panel_button ();
  xfce_panel_plugin_add_action_widget (XFCE_PANEL_PLUGIN (plugin), plugin->button);
  gtk_drag_dest_set (plugin->button, GTK_DEST_DEFAULT_ALL, drop_targets, G_N_ELEMENTS (drop_targets), GDK_ACTION_MOVE);
  g_signal_connect_swapped (G_OBJECT (plugin->button), "clicked", G_CALLBACK (thunar_tpa_display_trash), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "drag-data-received", G_CALLBACK (thunar_tpa_drag_data_received), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "enter-notify-event", G_CALLBACK (thunar_tpa_enter_notify_event), plugin);
  g_signal_connect (G_OBJECT (plugin->button), "leave-notify-event", G_CALLBACK (thunar_tpa_leave_notify_event), plugin);
  gtk_container_add (GTK_CONTAINER (plugin), plugin->button);
  gtk_widget_show (plugin->button);

  /* setup the image for the trash plugin */
  plugin->image = xfce_panel_image_new_from_source ("user-trash");
  gtk_container_add (GTK_CONTAINER (plugin->button), plugin->image);
  gtk_widget_show (plugin->image);

  /* prepare the menu item */
  plugin->mi = gtk_menu_item_new_with_mnemonic (_("_Empty Trash"));
  g_signal_connect_swapped (G_OBJECT (plugin->mi), "activate", G_CALLBACK (thunar_tpa_empty_trash), plugin);
  gtk_widget_show (plugin->mi);

  /* try to connect to the D-BUS session daemon */
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &err);
  if (G_UNLIKELY (connection == NULL))
    {
      /* we failed to connect, display an error plugin/tooltip */
      thunar_tpa_error (plugin, err);
      g_error_free (err);
    }
  else
    {
      /* grab a proxy for the /org/xfce/FileManager object on org.xfce.FileManager */
      plugin->proxy = dbus_g_proxy_new_for_name (connection, "org.xfce.FileManager", "/org/xfce/FileManager", "org.xfce.Trash");

      /* connect to the "TrashChanged" signal */
      dbus_g_proxy_add_signal (plugin->proxy, "TrashChanged", G_TYPE_BOOLEAN, G_TYPE_INVALID);
      dbus_g_proxy_connect_signal (plugin->proxy, "TrashChanged", G_CALLBACK (thunar_tpa_trash_changed), plugin, NULL);
    }
}



static void
thunar_tpa_finalize (GObject *object)
{
  ThunarTpa *plugin = THUNAR_TPA (object);

  /* release the proxy object */
  if (G_LIKELY (plugin->proxy != NULL))
    {
      /* cancel any pending calls */
      if (G_UNLIKELY (plugin->display_trash_call != NULL))
        dbus_g_proxy_cancel_call (plugin->proxy, plugin->display_trash_call);
      if (G_UNLIKELY (plugin->empty_trash_call != NULL))
        dbus_g_proxy_cancel_call (plugin->proxy, plugin->empty_trash_call);
      if (G_UNLIKELY (plugin->move_to_trash_call != NULL))
        dbus_g_proxy_cancel_call (plugin->proxy, plugin->move_to_trash_call);
      if (G_UNLIKELY (plugin->query_trash_call != NULL))
        dbus_g_proxy_cancel_call (plugin->proxy, plugin->query_trash_call);

      /* disconnect the signal and release the proxy */
      dbus_g_proxy_disconnect_signal (plugin->proxy, "TrashChanged", G_CALLBACK (thunar_tpa_trash_changed), plugin);
      g_object_unref (G_OBJECT (plugin->proxy));
    }

  (*G_OBJECT_CLASS (thunar_tpa_parent_class)->finalize) (object);
}



static void
thunar_tpa_construct (XfcePanelPlugin *panel_plugin)
{
  ThunarTpa *plugin = THUNAR_TPA (panel_plugin);

#ifdef HAS_PANEL_49
  /* make the plugin fit a single row */
  xfce_panel_plugin_set_small (panel_plugin, TRUE);
#endif

  /* add the "Empty Trash" menu item */
  xfce_panel_plugin_menu_insert_item (panel_plugin, GTK_MENU_ITEM (plugin->mi));

  /* update the state of the trash plugin */
  thunar_tpa_query_trash (plugin);
}



#ifdef HAS_PANEL_49
static gboolean
thunar_tpa_size_changed (XfcePanelPlugin *panel_plugin,
                         gint             size)
{
  g_return_val_if_fail (panel_plugin != NULL, FALSE);

  /* make the plugin fit a single row */
  size /= xfce_panel_plugin_get_nrows (panel_plugin);
  gtk_widget_set_size_request (GTK_WIDGET (panel_plugin), size, size);

  return TRUE;
}
#endif



static void
thunar_tpa_error (ThunarTpa *plugin,
                  GError    *error)
{
  gchar *tooltip;

  /* reset to empty first */
  thunar_tpa_state (plugin, FALSE);

  /* strip off additional whitespace */
  g_strstrip (error->message);

  /* tell the user that we failed to connect to the trash */
  tooltip = g_strdup_printf ("%s: %s.", _("Failed to connect to the Trash"), error->message);
  gtk_widget_set_tooltip_text (plugin->button, tooltip);
  g_free (tooltip);

  /* setup an error plugin */
  xfce_panel_image_set_from_source (XFCE_PANEL_IMAGE (plugin->image), "stock_dialog-error");
}



static void
thunar_tpa_state (ThunarTpa *plugin,
                  gboolean   full)
{
  /* tell the user whether the trash is full or empty */
  gtk_widget_set_tooltip_text (plugin->button, full ? _("Trash contains files") : _("Trash is empty"));

  /* setup the appropriate plugin */
  xfce_panel_image_set_from_source (XFCE_PANEL_IMAGE (plugin->image), full ? "user-trash-full" : "user-trash");

  /* sensitivity of the menu item */
  gtk_widget_set_sensitive (plugin->mi, full);
}



static void
thunar_tpa_display_trash_reply (DBusGProxy *proxy,
                                GError     *error,
                                gpointer    user_data)
{
  ThunarTpa *plugin = THUNAR_TPA (user_data);

  /* reset the call */
  plugin->display_trash_call = NULL;

  /* check if we failed */
  if (G_UNLIKELY (error != NULL))
    {
      /* display an error message to the user */
      g_strstrip (error->message);
      xfce_dialog_show_error (NULL, error, "%s.", _("Failed to connect to the Trash"));
      g_error_free (error);
    }
}



static void
thunar_tpa_empty_trash_reply (DBusGProxy *proxy,
                              GError     *error,
                              gpointer    user_data)
{
  ThunarTpa *plugin = THUNAR_TPA (user_data);

  /* reset the call */
  plugin->empty_trash_call = NULL;

  /* check if we failed */
  if (G_UNLIKELY (error != NULL))
    {
      /* display an error message to the user */
      g_strstrip (error->message);
      xfce_dialog_show_error (NULL, error, "%s.", _("Failed to connect to the Trash"));
      g_error_free (error);
    }
  else
    {
      /* query the new state of the trash */
      thunar_tpa_query_trash (plugin);
    }
}



static void
thunar_tpa_move_to_trash_reply (DBusGProxy *proxy,
                                GError     *error,
                                gpointer    user_data)
{
  ThunarTpa *plugin = THUNAR_TPA (user_data);

  /* reset the call */
  plugin->move_to_trash_call = NULL;

  /* check if we failed */
  if (G_UNLIKELY (error != NULL))
    {
      /* display an error message to the user */
      g_strstrip (error->message);
      xfce_dialog_show_error (NULL, error, "%s.", _("Failed to connect to the Trash"));
      g_error_free (error);
    }
  else
    {
      /* query the new state of the trash */
      thunar_tpa_query_trash (plugin);
    }
}



static void
thunar_tpa_query_trash_reply (DBusGProxy *proxy,
                              gboolean    full,
                              GError     *error,
                              gpointer    user_data)
{
  ThunarTpa *plugin = THUNAR_TPA (user_data);

  /* reset the call */
  plugin->query_trash_call = NULL;

  /* check if we failed */
  if (G_UNLIKELY (error != NULL))
    {
      /* setup an error tooltip/plugin */
      thunar_tpa_error (plugin, error);
      g_error_free (error);
    }
  else
    {
      /* update the tooltip/plugin accordingly */
      thunar_tpa_state (plugin, full);
    }
}



static void
thunar_tpa_drag_data_received (GtkWidget        *button,
                               GdkDragContext   *context,
                               gint              x,
                               gint              y,
                               GtkSelectionData *selection_data,
                               guint             info,
                               guint             timestamp,
                               ThunarTpa        *plugin)
{
  gboolean succeed = FALSE;
  gchar  **uri_list;

  g_return_if_fail (THUNAR_IS_TPA (plugin));
  g_return_if_fail (plugin->button == button);

  /* determine the type of drop we received */
  if (G_LIKELY (info == TARGET_TEXT_URI_LIST))
    {
      /* check if the data is valid for text/uri-list */
      uri_list = gtk_selection_data_get_uris (selection_data);
      if (G_LIKELY (uri_list != NULL))
        {
          succeed = thunar_tpa_move_to_trash (plugin, (const gchar **) uri_list);
          g_strfreev (uri_list);
        }
    }

  /* finish the drag */
  gtk_drag_finish (context, succeed, TRUE, timestamp);
}



static gboolean
thunar_tpa_enter_notify_event (GtkWidget        *button,
                               GdkEventCrossing *event,
                               ThunarTpa        *plugin)
{
  g_return_val_if_fail (THUNAR_IS_TPA (plugin), FALSE);
  g_return_val_if_fail (plugin->button == button, FALSE);

  /* query the new state of the trash */
  thunar_tpa_query_trash (plugin);

  return FALSE;
}



static gboolean
thunar_tpa_leave_notify_event (GtkWidget        *button,
                               GdkEventCrossing *event,
                               ThunarTpa        *plugin)
{
  g_return_val_if_fail (THUNAR_IS_TPA (plugin), FALSE);
  g_return_val_if_fail (plugin->button == button, FALSE);

  /* query the new state of the trash */
  thunar_tpa_query_trash (plugin);

  return FALSE;
}



static void
thunar_tpa_trash_changed (DBusGProxy *proxy,
                          gboolean    full,
                          ThunarTpa  *plugin)
{
  g_return_if_fail (THUNAR_IS_TPA (plugin));
  g_return_if_fail (plugin->proxy == proxy);

  /* change the status plugin/tooltip appropriately */
  thunar_tpa_state (plugin, full);
}



static void
thunar_tpa_display_trash (ThunarTpa *plugin)
{
  gchar *display_name;
  gchar *startup_id;

  g_return_if_fail (THUNAR_IS_TPA (plugin));

  /* check if we are connected to the bus */
  if (G_LIKELY (plugin->proxy != NULL))
    {
      /* cancel any pending call */
      if (G_UNLIKELY (plugin->display_trash_call != NULL))
        dbus_g_proxy_cancel_call (plugin->proxy, plugin->display_trash_call);

      /* schedule a new call */
      display_name = gdk_screen_make_display_name (gtk_widget_get_screen (GTK_WIDGET (plugin)));
      startup_id = g_strdup_printf ("_TIME%d", gtk_get_current_event_time ());
      plugin->display_trash_call = org_xfce_Trash_display_trash_async (plugin->proxy, display_name, startup_id, thunar_tpa_display_trash_reply, plugin);
      g_free (startup_id);
      g_free (display_name);
    }
}



static void
thunar_tpa_empty_trash (ThunarTpa *plugin)
{
  gchar *display_name;
  gchar *startup_id;

  g_return_if_fail (THUNAR_IS_TPA (plugin));

  /* check if we are connected to the bus */
  if (G_LIKELY (plugin->proxy != NULL))
    {
      /* cancel any pending call */
      if (G_UNLIKELY (plugin->empty_trash_call != NULL))
        dbus_g_proxy_cancel_call (plugin->proxy, plugin->empty_trash_call);

      /* schedule a new call */
      display_name = gdk_screen_make_display_name (gtk_widget_get_screen (GTK_WIDGET (plugin)));
      startup_id = g_strdup_printf ("_TIME%d", gtk_get_current_event_time ());
      plugin->empty_trash_call = org_xfce_Trash_empty_trash_async (plugin->proxy, display_name, startup_id, thunar_tpa_empty_trash_reply, plugin);
      g_free (startup_id);
      g_free (display_name);
    }
}



static gboolean
thunar_tpa_move_to_trash (ThunarTpa    *plugin,
                          const gchar **uri_list)
{
  gchar *display_name;
  gchar *startup_id;

  g_return_val_if_fail (THUNAR_IS_TPA (plugin), FALSE);
  g_return_val_if_fail (uri_list != NULL, FALSE);

  /* check if we are connected to the bus */
  if (G_UNLIKELY (plugin->proxy == NULL))
    return FALSE;

  /* cancel any pending call */
  if (G_UNLIKELY (plugin->move_to_trash_call != NULL))
    dbus_g_proxy_cancel_call (plugin->proxy, plugin->move_to_trash_call);

  /* schedule a new call */
  display_name = gdk_screen_make_display_name (gtk_widget_get_screen (GTK_WIDGET (plugin)));
  startup_id = g_strdup_printf ("_TIME%d", gtk_get_current_event_time ());
  plugin->move_to_trash_call = org_xfce_Trash_move_to_trash_async (plugin->proxy, uri_list, display_name, startup_id, thunar_tpa_move_to_trash_reply, plugin);
  g_free (startup_id);
  g_free (display_name);

  return TRUE;
}



static void
thunar_tpa_query_trash (ThunarTpa *plugin)
{
  g_return_if_fail (THUNAR_IS_TPA (plugin));

  /* check if we are connected to the bus */
  if (G_LIKELY (plugin->proxy != NULL))
    {
      /* cancel any pending call */
      if (G_UNLIKELY (plugin->query_trash_call != NULL))
        dbus_g_proxy_cancel_call (plugin->proxy, plugin->query_trash_call);

      /* schedule a new call */
      plugin->query_trash_call = org_xfce_Trash_query_trash_async (plugin->proxy, thunar_tpa_query_trash_reply, plugin);
    }
}
