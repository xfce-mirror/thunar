/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#include <thunar-tpa/thunar-tpa-bindings.h>
#include <thunar-tpa/thunar-tpa-icon.h>

#include <libxfce4panel/xfce-panel-convenience.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FULL,
};

/* Identifiers for DnD target ypes */
enum
{
  TARGET_TEXT_URI_LIST,
};



static void     thunar_tpa_icon_class_init          (ThunarTpaIconClass *klass);
static void     thunar_tpa_icon_init                (ThunarTpaIcon      *icon);
static void     thunar_tpa_icon_finalize            (GObject            *object);
static void     thunar_tpa_icon_get_property        (GObject            *object,
                                                     guint               prop_id,
                                                     GValue             *value,
                                                     GParamSpec         *pspec);
static void     thunar_tpa_icon_error               (ThunarTpaIcon      *icon,
                                                     GError             *error);
static void     thunar_tpa_icon_state               (ThunarTpaIcon      *icon,
                                                     gboolean            full);
static void     thunar_tpa_icon_display_trash_reply (DBusGProxy         *proxy,
                                                     GError             *error,
                                                     gpointer            user_data);
static void     thunar_tpa_icon_empty_trash_reply   (DBusGProxy         *proxy,
                                                     GError             *error,
                                                     gpointer            user_data);
static void     thunar_tpa_icon_move_to_trash_reply (DBusGProxy         *proxy,
                                                     GError             *error,
                                                     gpointer            user_data);
static void     thunar_tpa_icon_query_trash_reply   (DBusGProxy         *proxy,
                                                     gboolean            full,
                                                     GError             *error,
                                                     gpointer            user_data);
static void     thunar_tpa_icon_clicked             (GtkWidget          *button,
                                                     ThunarTpaIcon      *icon);
static void     thunar_tpa_icon_drag_data_received  (GtkWidget          *button,
                                                     GdkDragContext     *context,
                                                     gint                x,
                                                     gint                y,
                                                     GtkSelectionData   *selection_data,
                                                     guint               info,
                                                     guint               time,
                                                     ThunarTpaIcon      *icon);
static gboolean thunar_tpa_icon_enter_notify_event  (GtkWidget          *button,
                                                     GdkEventCrossing   *event,
                                                     ThunarTpaIcon      *icon);
static gboolean thunar_tpa_icon_leave_notify_event  (GtkWidget          *button,
                                                     GdkEventCrossing   *event,
                                                     ThunarTpaIcon      *icon);
static void     thunar_tpa_icon_trash_changed       (DBusGProxy         *proxy,
                                                     gboolean            full,
                                                     ThunarTpaIcon      *icon);



struct _ThunarTpaIconClass
{
  GtkAlignmentClass __parent__;
};

struct _ThunarTpaIcon
{
  GtkAlignment __parent__;

  gboolean full;

  GtkTooltips *tooltips;
  GtkWidget   *button;
  GtkWidget   *image;

  DBusGProxy     *proxy;
  DBusGProxyCall *display_trash_call;
  DBusGProxyCall *empty_trash_call;
  DBusGProxyCall *move_to_trash_call;
  DBusGProxyCall *query_trash_call;
};



/* Target types for dropping to the trash can */
static const GtkTargetEntry drop_targets[] =
{
  { "text/uri-list", 0, TARGET_TEXT_URI_LIST, },
};



G_DEFINE_TYPE (ThunarTpaIcon, thunar_tpa_icon, GTK_TYPE_ALIGNMENT);



static void
thunar_tpa_icon_class_init (ThunarTpaIconClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_tpa_icon_finalize;
  gobject_class->get_property = thunar_tpa_icon_get_property;

  /**
   * ThunarTpaIcon:full:
   *
   * The current state of the trash can.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FULL,
                                   g_param_spec_boolean ("full",
                                                         "full",
                                                         "full",
                                                         FALSE, 
                                                         EXO_PARAM_READABLE));
}



static void
thunar_tpa_icon_init (ThunarTpaIcon *icon)
{
  DBusGConnection *connection;
  GError          *err = NULL;

  gtk_alignment_set (GTK_ALIGNMENT (icon), 0.5f, 0.5f, 1.0f, 1.0f);

  /* allocate the shared tooltips */
  icon->tooltips = gtk_tooltips_new ();
  g_object_ref (G_OBJECT (icon->tooltips));
  gtk_object_sink (GTK_OBJECT (icon->tooltips));

  /* setup the button for the trash icon */
  icon->button = xfce_create_panel_button ();
  GTK_WIDGET_UNSET_FLAGS (icon->button, GTK_CAN_DEFAULT);
  gtk_drag_dest_set (icon->button, GTK_DEST_DEFAULT_ALL, drop_targets, G_N_ELEMENTS (drop_targets), GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (icon->button), "clicked", G_CALLBACK (thunar_tpa_icon_clicked), icon);
  g_signal_connect (G_OBJECT (icon->button), "drag-data-received", G_CALLBACK (thunar_tpa_icon_drag_data_received), icon);
  g_signal_connect (G_OBJECT (icon->button), "enter-notify-event", G_CALLBACK (thunar_tpa_icon_enter_notify_event), icon);
  g_signal_connect (G_OBJECT (icon->button), "leave-notify-event", G_CALLBACK (thunar_tpa_icon_leave_notify_event), icon);
  gtk_container_add (GTK_CONTAINER (icon), icon->button);
  gtk_widget_show (icon->button);

  /* setup the image for the trash icon */
  icon->image = gtk_image_new_from_icon_name ("gnome-fs-trash-empty", GTK_ICON_SIZE_BUTTON);
  gtk_container_add (GTK_CONTAINER (icon->button), icon->image);
  gtk_widget_show (icon->image);

  /* try to connect to the D-BUS session daemon */
  connection = dbus_g_bus_get (DBUS_BUS_SESSION, &err);
  if (G_UNLIKELY (connection == NULL))
    {
      /* we failed to connect, display an error icon/tooltip */
      thunar_tpa_icon_error (icon, err);
      g_error_free (err);
    }
  else
    {
      /* grab a proxy for the /org/xfce/FileManager object on org.xfce.FileManager */
      icon->proxy = dbus_g_proxy_new_for_name (connection, "org.xfce.FileManager", "/org/xfce/FileManager", "org.xfce.Trash");

      /* connect to the "TrashChanged" signal */
      dbus_g_proxy_add_signal (icon->proxy, "TrashChanged", G_TYPE_BOOLEAN, G_TYPE_INVALID);
      dbus_g_proxy_connect_signal (icon->proxy, "TrashChanged", G_CALLBACK (thunar_tpa_icon_trash_changed), icon, NULL);

      /* update the state of the trash icon */
      thunar_tpa_icon_query_trash (icon);
    }
}



static void
thunar_tpa_icon_finalize (GObject *object)
{
  ThunarTpaIcon *icon = THUNAR_TPA_ICON (object);

  /* release the proxy object */
  if (G_LIKELY (icon->proxy != NULL))
    {
      /* cancel any pending calls */
      if (G_UNLIKELY (icon->display_trash_call != NULL))
        dbus_g_proxy_cancel_call (icon->proxy, icon->display_trash_call);
      if (G_UNLIKELY (icon->empty_trash_call != NULL))
        dbus_g_proxy_cancel_call (icon->proxy, icon->empty_trash_call);
      if (G_UNLIKELY (icon->move_to_trash_call != NULL))
        dbus_g_proxy_cancel_call (icon->proxy, icon->move_to_trash_call);
      if (G_UNLIKELY (icon->query_trash_call != NULL))
        dbus_g_proxy_cancel_call (icon->proxy, icon->query_trash_call);

      /* disconnect the signal and release the proxy */
      dbus_g_proxy_disconnect_signal (icon->proxy, "TrashChanged", G_CALLBACK (thunar_tpa_icon_trash_changed), icon);
      g_object_unref (G_OBJECT (icon->proxy));
    }

  /* release the shared tooltips */
  g_object_unref (G_OBJECT (icon->tooltips));

  (*G_OBJECT_CLASS (thunar_tpa_icon_parent_class)->finalize) (object);
}



static void
thunar_tpa_icon_get_property (GObject    *object,
                              guint       prop_id,
                              GValue     *value,
                              GParamSpec *pspec)
{
  ThunarTpaIcon *icon = THUNAR_TPA_ICON (object);

  switch (prop_id)
    {
    case PROP_FULL:
      g_value_set_boolean (value, icon->full);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_tpa_icon_error (ThunarTpaIcon *icon,
                       GError        *error)
{
  gchar *tooltip;

  /* reset to empty first */
  thunar_tpa_icon_state (icon, FALSE);

  /* strip off additional whitespace */
  g_strstrip (error->message);

  /* tell the user that we failed to connect to the trash */
  tooltip = g_strdup_printf ("%s: %s.", _("Failed to connect to the Trash"), error->message);
  gtk_tooltips_set_tip (icon->tooltips, icon->button, tooltip, NULL);
  g_free (tooltip);

  /* setup an error icon */
  gtk_image_set_from_icon_name (GTK_IMAGE (icon->image), "stock_dialog-error", GTK_ICON_SIZE_BUTTON);
}



static void
thunar_tpa_icon_state (ThunarTpaIcon *icon,
                       gboolean       full)
{
  /* tell the user whether the trash is full or empty */
  gtk_tooltips_set_tip (icon->tooltips, icon->button, full ? _("Trash contains files") : _("Trash is empty"), NULL);

  /* setup the appropriate icon */
  gtk_image_set_from_icon_name (GTK_IMAGE (icon->image), full ? "gnome-fs-trash-full" : "gnome-fs-trash-empty", GTK_ICON_SIZE_BUTTON);

  /* apply the new state */
  icon->full = full;
  g_object_notify (G_OBJECT (icon), "full");
}



static void
thunar_tpa_icon_display_trash_reply (DBusGProxy *proxy,
                                     GError     *error,
                                     gpointer    user_data)
{
  ThunarTpaIcon *icon = THUNAR_TPA_ICON (user_data);
  GtkWidget     *message;

  /* reset the call */
  icon->display_trash_call = NULL;

  /* check if we failed */
  if (G_UNLIKELY (error != NULL))
    {
      /* strip off additional whitespace */
      g_strstrip (error->message);

      /* display an error message to the user */
      message = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s.", _("Failed to connect to the Trash"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s.", error->message);
      gtk_window_set_screen (GTK_WINDOW (message), gtk_widget_get_screen (GTK_WIDGET (icon)));
      gtk_dialog_run (GTK_DIALOG (message));
      gtk_widget_destroy (message);
      g_error_free (error);
    }
}



static void
thunar_tpa_icon_empty_trash_reply (DBusGProxy *proxy,
                                   GError     *error,
                                   gpointer    user_data)
{
  ThunarTpaIcon *icon = THUNAR_TPA_ICON (user_data);
  GtkWidget     *message;

  /* reset the call */
  icon->empty_trash_call = NULL;

  /* check if we failed */
  if (G_UNLIKELY (error != NULL))
    {
      /* strip off additional whitespace */
      g_strstrip (error->message);

      /* display an error message to the user */
      message = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s.", _("Failed to connect to the Trash"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s.", error->message);
      gtk_window_set_screen (GTK_WINDOW (message), gtk_widget_get_screen (GTK_WIDGET (icon)));
      gtk_dialog_run (GTK_DIALOG (message));
      gtk_widget_destroy (message);
      g_error_free (error);
    }
  else
    {
      /* query the new state of the trash */
      thunar_tpa_icon_query_trash (icon);
    }
}



static void
thunar_tpa_icon_move_to_trash_reply (DBusGProxy *proxy,
                                     GError     *error,
                                     gpointer    user_data)
{
  ThunarTpaIcon *icon = THUNAR_TPA_ICON (user_data);
  GtkWidget     *message;

  /* reset the call */
  icon->move_to_trash_call = NULL;

  /* check if we failed */
  if (G_UNLIKELY (error != NULL))
    {
      /* strip off additional whitespace */
      g_strstrip (error->message);

      /* display an error message to the user */
      message = gtk_message_dialog_new (NULL, 0, GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, "%s.", _("Failed to connect to the Trash"));
      gtk_message_dialog_format_secondary_text (GTK_MESSAGE_DIALOG (message), "%s.", error->message);
      gtk_window_set_screen (GTK_WINDOW (message), gtk_widget_get_screen (GTK_WIDGET (icon)));
      gtk_dialog_run (GTK_DIALOG (message));
      gtk_widget_destroy (message);
      g_error_free (error);
    }
  else
    {
      /* query the new state of the trash */
      thunar_tpa_icon_query_trash (icon);
    }
}



static void
thunar_tpa_icon_query_trash_reply (DBusGProxy *proxy,
                                   gboolean    full,
                                   GError     *error,
                                   gpointer    user_data)
{
  ThunarTpaIcon *icon = THUNAR_TPA_ICON (user_data);

  /* reset the call */
  icon->query_trash_call = NULL;

  /* check if we failed */
  if (G_UNLIKELY (error != NULL))
    {
      /* setup an error tooltip/icon */
      thunar_tpa_icon_error (icon, error);
      g_error_free (error);
    }
  else
    {
      /* update the tooltip/icon accordingly */
      thunar_tpa_icon_state (icon, full);
    }
}



static void
thunar_tpa_icon_clicked (GtkWidget     *button,
                         ThunarTpaIcon *icon)
{
  g_return_if_fail (THUNAR_TPA_IS_ICON (icon));
  g_return_if_fail (icon->button == button);

  /* display the trash folder */
  thunar_tpa_icon_display_trash (icon);
}



static void
thunar_tpa_icon_drag_data_received (GtkWidget        *button,
                                    GdkDragContext   *context,
                                    gint              x,
                                    gint              y,
                                    GtkSelectionData *selection_data,
                                    guint             info,
                                    guint             time,
                                    ThunarTpaIcon    *icon)
{
  gboolean succeed = FALSE;
  gchar  **uri_list;

  g_return_if_fail (THUNAR_TPA_IS_ICON (icon));
  g_return_if_fail (icon->button == button);

  /* determine the type of drop we received */
  if (G_LIKELY (info == TARGET_TEXT_URI_LIST))
    {
      /* check if the data is valid for text/uri-list */
      if (G_LIKELY (selection_data->length >= 0 && selection_data->format == 8))
        {
          /* parse the URI list according to RFC 2483 */
          uri_list = g_uri_list_extract_uris ((const gchar *) selection_data->data);
          succeed = thunar_tpa_icon_move_to_trash (icon, (const gchar **) uri_list);
          g_strfreev (uri_list);
        }
    }

  /* finish the drag */
  gtk_drag_finish (context, succeed, TRUE, time);
}



static gboolean
thunar_tpa_icon_enter_notify_event (GtkWidget        *button,
                                    GdkEventCrossing *event,
                                    ThunarTpaIcon    *icon)
{
  g_return_val_if_fail (THUNAR_TPA_IS_ICON (icon), FALSE);
  g_return_val_if_fail (icon->button == button, FALSE);

  /* query the new state of the trash */
  thunar_tpa_icon_query_trash (icon);

  return FALSE;
}



static gboolean
thunar_tpa_icon_leave_notify_event (GtkWidget        *button,
                                    GdkEventCrossing *event,
                                    ThunarTpaIcon    *icon)
{
  g_return_val_if_fail (THUNAR_TPA_IS_ICON (icon), FALSE);
  g_return_val_if_fail (icon->button == button, FALSE);

  /* query the new state of the trash */
  thunar_tpa_icon_query_trash (icon);

  return FALSE;
}



static void
thunar_tpa_icon_trash_changed (DBusGProxy    *proxy,
                               gboolean       full,
                               ThunarTpaIcon *icon)
{
  g_return_if_fail (THUNAR_TPA_IS_ICON (icon));
  g_return_if_fail (icon->proxy == proxy);

  /* change the status icon/tooltip appropriately */
  thunar_tpa_icon_state (icon, full);
}



/**
 * thunar_tpa_icon_new:
 *
 * Allocates a new #ThunarTpaIcon instance.
 *
 * Return value: the newly allocated #ThunarTpaIcon.
 **/
GtkWidget*
thunar_tpa_icon_new (void)
{
  return g_object_new (THUNAR_TPA_TYPE_ICON, NULL);
}



/**
 * thunar_tpa_icon_set_size:
 * @icon : a #ThunarTpaIcon.
 * @size : the new width and height for the @icon.
 *
 * Sets the width and height for the @icon to the specified
 * @size.
 **/
void
thunar_tpa_icon_set_size (ThunarTpaIcon *icon,
                          gint           size)
{
  gint focus_line_width;
  gint focus_padding;
  gint pixel_size = size;

  g_return_if_fail (THUNAR_TPA_IS_ICON (icon));
  g_return_if_fail (size > 0);

  /* determine the style properties affecting the button size */
  gtk_widget_style_get (GTK_WIDGET (icon->button),
                        "focus-line-width", &focus_line_width,
                        "focus-padding", &focus_padding,
                        NULL);

  /* determine the pixel size for the image */
  pixel_size -= 2 + 2 * (focus_line_width + focus_padding);
  pixel_size -= GTK_CONTAINER (icon->button)->border_width * 2;
  pixel_size -= MAX (icon->button->style->xthickness, icon->button->style->ythickness) * 2;

  /* setup the pixel size for the image */
  gtk_image_set_pixel_size (GTK_IMAGE (icon->image), pixel_size);
}



/**
 * thunar_tpa_icon_display_trash:
 * @icon : a #ThunarTpaIcon.
 *
 * Displays the trash folder.
 **/
void
thunar_tpa_icon_display_trash (ThunarTpaIcon *icon)
{
  gchar *display_name;

  g_return_if_fail (THUNAR_TPA_IS_ICON (icon));

  /* check if we are connected to the bus */
  if (G_LIKELY (icon->proxy != NULL))
    {
      /* cancel any pending call */
      if (G_UNLIKELY (icon->display_trash_call != NULL))
        dbus_g_proxy_cancel_call (icon->proxy, icon->display_trash_call);

      /* schedule a new call */
      display_name = gdk_screen_make_display_name (gtk_widget_get_screen (GTK_WIDGET (icon)));
      icon->display_trash_call = org_xfce_Trash_display_trash_async (icon->proxy, display_name, thunar_tpa_icon_display_trash_reply, icon);
      g_free (display_name);
    }
}



/**
 * thunar_tpa_icon_empty_trash:
 * @icon : a #ThunarTpaIcon.
 *
 * Empties the trash can.
 **/
void
thunar_tpa_icon_empty_trash (ThunarTpaIcon *icon)
{
  gchar *display_name;

  g_return_if_fail (THUNAR_TPA_IS_ICON (icon));

  /* check if we are connected to the bus */
  if (G_LIKELY (icon->proxy != NULL))
    {
      /* cancel any pending call */
      if (G_UNLIKELY (icon->empty_trash_call != NULL))
        dbus_g_proxy_cancel_call (icon->proxy, icon->empty_trash_call);

      /* schedule a new call */
      display_name = gdk_screen_make_display_name (gtk_widget_get_screen (GTK_WIDGET (icon)));
      icon->empty_trash_call = org_xfce_Trash_empty_trash_async (icon->proxy, display_name, thunar_tpa_icon_empty_trash_reply, icon);
      g_free (display_name);
    }
}



/**
 * thunar_tpa_icon_move_to_trash:
 * @icon     : a #ThunarTpaIcon.
 * @uri_list : the URIs of the files to move to the trash.
 *
 * Tells the trash to move the files in the @uri_list to the
 * trash.
 *
 * Return value: %TRUE if the command was send successfully,
 *               %FALSE otherwise.
 **/
gboolean
thunar_tpa_icon_move_to_trash (ThunarTpaIcon *icon,
                               const gchar  **uri_list)
{
  gchar *display_name;

  g_return_val_if_fail (THUNAR_TPA_IS_ICON (icon), FALSE);
  g_return_val_if_fail (uri_list != NULL, FALSE);

  /* check if we are connected to the bus */
  if (G_UNLIKELY (icon->proxy == NULL))
    return FALSE;

  /* cancel any pending call */
  if (G_UNLIKELY (icon->move_to_trash_call != NULL))
    dbus_g_proxy_cancel_call (icon->proxy, icon->move_to_trash_call);

  /* schedule a new call */
  display_name = gdk_screen_make_display_name (gtk_widget_get_screen (GTK_WIDGET (icon)));
  icon->move_to_trash_call = org_xfce_Trash_move_to_trash_async (icon->proxy, uri_list, display_name, thunar_tpa_icon_move_to_trash_reply, icon);
  g_free (display_name);

  return TRUE;
}



/**
 * thunar_tpa_icon_query_trash:
 * @icon : a #ThunarTpaIcon.
 *
 * Queries the state of the trash can.
 **/
void
thunar_tpa_icon_query_trash (ThunarTpaIcon *icon)
{
  g_return_if_fail (THUNAR_TPA_IS_ICON (icon));

  /* check if we are connected to the bus */
  if (G_LIKELY (icon->proxy != NULL))
    {
      /* cancel any pending call */
      if (G_UNLIKELY (icon->query_trash_call != NULL))
        dbus_g_proxy_cancel_call (icon->proxy, icon->query_trash_call);

      /* schedule a new call */
      icon->query_trash_call = org_xfce_Trash_query_trash_async (icon->proxy, thunar_tpa_icon_query_trash_reply, icon);
    }
}

