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
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-application.h>
#include <thunar/thunar-dnd.h>
#include <thunar/thunar-gio-extensions.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-location-button.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-preferences.h>
#include <thunar/thunar-private.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_ACTIVE,
  PROP_FILE,
};

/* Signal identifiers */
enum
{
  CLICKED,
  CONTEXT_MENU,
  GONE,
  LAST_SIGNAL,
};



static void           thunar_location_button_finalize               (GObject                    *object);
static void           thunar_location_button_get_property           (GObject                    *object,
                                                                     guint                       prop_id,
                                                                     GValue                     *value,
                                                                     GParamSpec                 *pspec);
static void           thunar_location_button_set_property           (GObject                    *object,
                                                                     guint                       prop_id,
                                                                     const GValue               *value,
                                                                     GParamSpec                 *pspec);
static void           thunar_location_button_style_set              (GtkWidget                  *widget,
                                                                     GtkStyle                   *previous_style);
static GdkDragAction  thunar_location_button_get_dest_actions       (ThunarLocationButton       *location_button,
                                                                     GdkDragContext             *context,
                                                                     GtkWidget                  *button,
                                                                     guint                       timestamp);
static void           thunar_location_button_file_changed           (ThunarLocationButton       *location_button,
                                                                     ThunarFile                 *file);
static void           thunar_location_button_file_destroy           (ThunarLocationButton       *location_button,
                                                                     ThunarFile                 *file);
static void           thunar_location_button_align_size_request     (GtkWidget                  *align,
                                                                     GtkRequisition             *requisition,
                                                                     ThunarLocationButton       *location_button);
static gboolean       thunar_location_button_button_press_event     (GtkWidget                  *button,
                                                                     GdkEventButton             *event,
                                                                     ThunarLocationButton       *location_button);
static gboolean       thunar_location_button_button_release_event   (GtkWidget                  *button,
                                                                     GdkEventButton             *event,
                                                                     ThunarLocationButton       *location_button);
static gboolean       thunar_location_button_drag_drop              (GtkWidget                  *button,
                                                                     GdkDragContext             *context,
                                                                     gint                        x,
                                                                     gint                        y,
                                                                     guint                       timestamp,
                                                                     ThunarLocationButton       *location_button);
static void           thunar_location_button_drag_data_get          (GtkWidget                  *button,
                                                                     GdkDragContext             *context,
                                                                     GtkSelectionData           *selection_data,
                                                                     guint                       info,
                                                                     guint                       timestamp,
                                                                     ThunarLocationButton       *location_button);
static void           thunar_location_button_drag_data_received     (GtkWidget                  *button,
                                                                     GdkDragContext             *context,
                                                                     gint                        x,
                                                                     gint                        y,
                                                                     GtkSelectionData           *selection_data,
                                                                     guint                       info,
                                                                     guint                       timestamp,
                                                                     ThunarLocationButton       *location_button);
static void           thunar_location_button_drag_leave             (GtkWidget                  *button,
                                                                     GdkDragContext             *context,
                                                                     guint                       timestamp,
                                                                     ThunarLocationButton       *location_button);
static gboolean       thunar_location_button_drag_motion            (GtkWidget                  *button,
                                                                     GdkDragContext             *context,
                                                                     gint                        x,
                                                                     gint                        y,
                                                                     guint                       timestamp,
                                                                     ThunarLocationButton       *location_button);
static gboolean       thunar_location_button_enter_timeout          (gpointer                    user_data);
static void           thunar_location_button_enter_timeout_destroy  (gpointer                    user_data);



struct _ThunarLocationButtonClass
{
  GtkAlignmentClass __parent__;
};

struct _ThunarLocationButton
{
  GtkAlignment __parent__;

  GtkWidget          *image;
  GtkWidget          *label;

  /* the current icon state (i.e. accepting drops) */
  ThunarFileIconState file_icon_state;
  
  /* enter folders using DnD */
  guint               enter_timeout_id;

  /* drop support for the button */
  GList              *drop_file_list;
  guint               drop_data_ready : 1;
  guint               drop_occurred : 1;

  /* public properties */
  guint               active : 1;
  ThunarFile         *file;
};



static const GtkTargetEntry drag_targets[] =
{
  { "text/uri-list", 0, 0 },
};

static guint location_button_signals[LAST_SIGNAL];



G_DEFINE_TYPE (ThunarLocationButton, thunar_location_button, GTK_TYPE_ALIGNMENT)



static void
thunar_location_button_class_init (ThunarLocationButtonClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_location_button_finalize;
  gobject_class->get_property = thunar_location_button_get_property;
  gobject_class->set_property = thunar_location_button_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->style_set = thunar_location_button_style_set;

  /**
   * ThunarLocationButton:active:
   *
   * Whether the location button is currently active.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         "active",
                                                         "active",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarLocationButton:file:
   *
   * The #ThunarFile represented by this location button.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "file",
                                                        "file",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarLocationButton::clicked:
   * @location_button : a #ThunarLocationButton.
   *
   * Emitted by @location_button when the user clicks on the
   * @location_button or thunar_location_button_clicked() is
   * called.
   **/
  location_button_signals[CLICKED] =
    g_signal_new (I_("clicked"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOOLEAN,
                  G_TYPE_NONE, 1,
                  G_TYPE_BOOLEAN);

  /**
   * ThunarLocationButton::context-menu:
   * @location_button : a #ThunarLocationButton.
   * @event           : a #GdkEventButton.
   *
   * Emitted by @location_button when the user requests to open
   * the context menu for @location_button.
   **/
  location_button_signals[CONTEXT_MENU] =
    g_signal_new (I_("context-menu"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, GDK_TYPE_EVENT);

  /**
   * ThunarLocationButton::gone:
   * @location_button : a #ThunarLocationButton.
   *
   * Emitted by @location_button when the file associated with
   * the button is deleted.
   **/
  location_button_signals[GONE] =
    g_signal_new (I_("gone"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
thunar_location_button_init (ThunarLocationButton *location_button)
{
  GtkWidget *align;
  GtkWidget *button;
  GtkWidget *hbox;

  /* create the toggle button */
  button = gtk_toggle_button_new ();
  gtk_widget_set_can_focus (button, FALSE);
  g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (thunar_location_button_clicked), location_button);
  exo_mutual_binding_new (G_OBJECT (location_button), "active", G_OBJECT (button), "active");
  gtk_container_add (GTK_CONTAINER (location_button), button);
  gtk_widget_show (button);

  /* setup drag support for the button */
  gtk_drag_source_set (GTK_WIDGET (button), GDK_BUTTON1_MASK, drag_targets, G_N_ELEMENTS (drag_targets), GDK_ACTION_LINK);
  gtk_drag_dest_set (GTK_WIDGET (button), GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_MOTION, drag_targets,
                     G_N_ELEMENTS (drag_targets), GDK_ACTION_ASK | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (button), "button-press-event", G_CALLBACK (thunar_location_button_button_press_event), location_button);
  g_signal_connect (G_OBJECT (button), "button-release-event", G_CALLBACK (thunar_location_button_button_release_event), location_button);
  g_signal_connect (G_OBJECT (button), "drag-drop", G_CALLBACK (thunar_location_button_drag_drop), location_button);
  g_signal_connect (G_OBJECT (button), "drag-data-get", G_CALLBACK (thunar_location_button_drag_data_get), location_button);
  g_signal_connect (G_OBJECT (button), "drag-data-received", G_CALLBACK (thunar_location_button_drag_data_received), location_button);
  g_signal_connect (G_OBJECT (button), "drag-leave", G_CALLBACK (thunar_location_button_drag_leave), location_button);
  g_signal_connect (G_OBJECT (button), "drag-motion", G_CALLBACK (thunar_location_button_drag_motion), location_button);

  /* create the horizontal box */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (button), hbox);
  gtk_widget_show (hbox);

  /* create the button image */
  location_button->image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (hbox), location_button->image, TRUE, FALSE, 0);
  gtk_widget_show (location_button->image);

  /* create the button label alignment */
  align = gtk_alignment_new (0.5f, 0.5f, 1.0f, 1.0f);
  g_signal_connect (G_OBJECT (align), "size-request", G_CALLBACK (thunar_location_button_align_size_request), location_button);
  gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);
  gtk_widget_show (align);

  /* create the button label */
  location_button->label = g_object_new (GTK_TYPE_LABEL, NULL);
  gtk_container_add (GTK_CONTAINER (align), location_button->label);
  gtk_widget_show (location_button->label);
}



static void
thunar_location_button_finalize (GObject *object)
{
  ThunarLocationButton *location_button = THUNAR_LOCATION_BUTTON (object);

  /* release the drop path list (just in case the drag-leave wasn't fired before) */
  thunar_g_file_list_free (location_button->drop_file_list);

  /* be sure to cancel any pending enter timeout */
  if (G_UNLIKELY (location_button->enter_timeout_id != 0))
    g_source_remove (location_button->enter_timeout_id);

  /* disconnect from the file */
  thunar_location_button_set_file (location_button, NULL);

  (*G_OBJECT_CLASS (thunar_location_button_parent_class)->finalize) (object);
}



static void
thunar_location_button_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ThunarLocationButton *location_button = THUNAR_LOCATION_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, thunar_location_button_get_active (location_button));
      break;

    case PROP_FILE:
      g_value_set_object (value, thunar_location_button_get_file (location_button));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_location_button_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ThunarLocationButton *location_button = THUNAR_LOCATION_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      thunar_location_button_set_active (location_button, g_value_get_boolean (value));
      break;

    case PROP_FILE:
      thunar_location_button_set_file (location_button, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_location_button_style_set (GtkWidget *widget,
                                  GtkStyle  *previous_style)
{
  ThunarLocationButton *location_button = THUNAR_LOCATION_BUTTON (widget);

  /* update the user interface if we have a file */
  if (G_LIKELY (location_button->file != NULL))
    thunar_location_button_file_changed (location_button, location_button->file);

  (*GTK_WIDGET_CLASS (thunar_location_button_parent_class)->style_set) (widget, previous_style);
}



static GdkDragAction
thunar_location_button_get_dest_actions (ThunarLocationButton *location_button,
                                         GdkDragContext       *context,
                                         GtkWidget            *button,
                                         guint                 timestamp)
{
  GdkDragAction actions = 0;
  GdkDragAction action = 0;

  /* check if we can drop here */
  if (G_LIKELY (location_button->file != NULL))
    {
      /* determine the possible drop actions for the file (and the suggested action if any) */
      actions = thunar_file_accepts_drop (location_button->file, location_button->drop_file_list, context, &action);
    }

  /* tell Gdk whether we can drop here */
  gdk_drag_status (context, action, timestamp);

  return actions;
}



static void
thunar_location_button_file_changed (ThunarLocationButton *location_button,
                                     ThunarFile           *file)
{
  ThunarIconFactory *icon_factory;
  GtkIconTheme      *icon_theme;
  GtkSettings       *settings;
  GdkPixbuf         *icon;
  const gchar       *icon_name;
  gint               height;
  gint               width;
  gint               size;
  const gchar       *custom_icon;

  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTON (location_button));
  _thunar_return_if_fail (location_button->file == file);
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* determine the icon theme for the widget */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (location_button)));

  /* update and show the label widget (hide for the local root folder) */
  if (thunar_file_is_local (file) && thunar_file_is_root (file)) 
    {
      /* hide the alignment in which the label would otherwise show up */
      gtk_widget_hide (gtk_widget_get_parent (location_button->label));
    }
  else
    {
      /* set label to the file's display name and show the alignment (and thereby the label) */
      gtk_label_set_text (GTK_LABEL (location_button->label), thunar_file_get_display_name (file));
      gtk_widget_show (gtk_widget_get_parent (location_button->label));
    }

  /* the image is only visible for certain special paths */
  if (thunar_file_is_home (file) || thunar_file_is_desktop (file) || thunar_file_is_root (file))
    {
      /* determine the icon size for menus (to be compatible with GtkPathBar) */
      settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (location_button)));
      if (gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU, &width, &height))
        size = MAX (width, height);
      else
        size = 16;

      /* update the icon for the image */
      icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);
      icon = thunar_icon_factory_load_file_icon (icon_factory, file, location_button->file_icon_state, size);
      if (G_LIKELY (icon != NULL))
        {
          gtk_image_set_from_pixbuf (GTK_IMAGE (location_button->image), icon);
          g_object_unref (G_OBJECT (icon));
        }
      g_object_unref (G_OBJECT (icon_factory));

      /* show the image widget */
      gtk_widget_show (location_button->image);
    }
  else
    {
      /* hide the image widget */
      gtk_widget_hide (location_button->image);
    }

  /* setup the DnD icon for the button */
  custom_icon = thunar_file_get_custom_icon (file);
  if (custom_icon != NULL)
    {
      gtk_drag_source_set_icon_name (GTK_BIN (location_button)->child, custom_icon);
    }
  else
    {
      icon_name = thunar_file_get_icon_name (file, location_button->file_icon_state, icon_theme);
      gtk_drag_source_set_icon_name (GTK_BIN (location_button)->child, icon_name);
    }
}



static void
thunar_location_button_file_destroy (ThunarLocationButton *location_button,
                                     ThunarFile           *file)
{
  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTON (location_button));
  _thunar_return_if_fail (location_button->file == file);
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* the file is gone, emit the "gone" signal */
  g_signal_emit (G_OBJECT (location_button), location_button_signals[GONE], 0);
}



static void
thunar_location_button_align_size_request (GtkWidget            *align,
                                           GtkRequisition       *requisition,
                                           ThunarLocationButton *location_button)
{
  PangoLayout *layout;
  gint         normal_height;
  gint         normal_width;
  gint         bold_height;
  gint         bold_width;

  /* allocate a pango layout for the label text */
  layout = gtk_widget_create_pango_layout (location_button->label, gtk_label_get_text (GTK_LABEL (location_button->label)));

  /* determine the normal pixel size */
  pango_layout_get_pixel_size (layout, &normal_width, &normal_height);

  /* determine the bold pixel size */
  pango_layout_set_attributes (layout, thunar_pango_attr_list_bold ());
  pango_layout_get_pixel_size (layout, &bold_width, &bold_height);

  /* request the maximum of the pixel sizes, to make sure
   * the button always requests the same size, no matter if
   * the label is bold or not.
   */
  requisition->width = MAX (bold_width, normal_width);
  requisition->height = MAX (bold_height, normal_height);

  /* cleanup */
  g_object_unref (G_OBJECT (layout));
}



static gboolean
thunar_location_button_button_press_event (GtkWidget            *button,
                                           GdkEventButton       *event,
                                           ThunarLocationButton *location_button)
{
  _thunar_return_val_if_fail (THUNAR_IS_LOCATION_BUTTON (location_button), FALSE);

  /* check if we can handle the button event */
  if (G_UNLIKELY (event->button == 2))
    {
      /* set button state to inconsistent while holding down the middle-mouse-button */
      gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (button), TRUE);
    }
  else if (event->button == 3)
    {
      /* emit the "context-menu" signal and let the surrounding ThunarLocationButtons popup a menu */
      g_signal_emit (G_OBJECT (location_button), location_button_signals[CONTEXT_MENU], 0, event);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_location_button_button_release_event (GtkWidget            *button,
                                             GdkEventButton       *event,
                                             ThunarLocationButton *location_button)
{
  ThunarApplication *application;
  ThunarPreferences *preferences;
  gboolean           open_in_tab;

  _thunar_return_val_if_fail (THUNAR_IS_LOCATION_BUTTON (location_button), FALSE);

  /* reset inconsistent button state after releasing the middle-mouse-button */
  if (G_UNLIKELY (event->button == 2))
    {
      /* reset button state */
      gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (button), FALSE);

      /* verify that we still have a valid file */
      if (G_LIKELY (location_button->file != NULL))
        {
          preferences = thunar_preferences_get ();
          g_object_get (preferences, "misc-middle-click-in-tab", &open_in_tab, NULL);
          g_object_unref (preferences);

          if (open_in_tab)
            {
              /* open in tab */
              g_signal_emit (G_OBJECT (location_button), location_button_signals[CLICKED], 0, TRUE);
            }
          else
            {
              /* open a new window for the folder */
              application = thunar_application_get ();
              thunar_application_open_window (application, location_button->file, gtk_widget_get_screen (GTK_WIDGET (location_button)), NULL);
              g_object_unref (G_OBJECT (application));
            }
        }
    }

  return FALSE;
}



static gboolean
thunar_location_button_drag_drop (GtkWidget            *button,
                                  GdkDragContext       *context,
                                  gint                  x,
                                  gint                  y,
                                  guint                 timestamp,
                                  ThunarLocationButton *location_button)
{
  GdkAtom target;

  _thunar_return_val_if_fail (GTK_IS_WIDGET (button), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_LOCATION_BUTTON (location_button), FALSE);

  /* determine the DnD target and see if we can handle it */
  target = gtk_drag_dest_find_target (button, context, NULL);
  if (G_UNLIKELY (target != gdk_atom_intern_static_string ("text/uri-list")))
    return FALSE;

  /* set state so drag-data-received knows that
   * this is really a drop this time.
   */
  location_button->drop_occurred = TRUE;

  /* request the drag data from the source */
  gtk_drag_get_data (button, context, target, timestamp);

  /* we'll call gtk_drag_finish() later */
  return TRUE;
}



static void
thunar_location_button_drag_data_get (GtkWidget            *button,
                                      GdkDragContext       *context,
                                      GtkSelectionData     *selection_data,
                                      guint                 info,
                                      guint                 timestamp,
                                      ThunarLocationButton *location_button)
{
  gchar **uris;
  GList   path_list;

  _thunar_return_if_fail (GTK_IS_WIDGET (button));
  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTON (location_button));

  /* verify that we have a valid file */
  if (G_LIKELY (location_button->file != NULL))
    {
      /* transform the path into an uri list string */
      path_list.next = path_list.prev = NULL;
      path_list.data = thunar_file_get_file (location_button->file);

      /* set the uri list for the drag selection */
      uris = thunar_g_file_list_to_stringv (&path_list);
      gtk_selection_data_set_uris (selection_data, uris);
      g_strfreev (uris);
    }
}



static void
thunar_location_button_drag_data_received (GtkWidget            *button,
                                           GdkDragContext       *context,
                                           gint                  x,
                                           gint                  y,
                                           GtkSelectionData     *selection_data,
                                           guint                 info,
                                           guint                 timestamp,
                                           ThunarLocationButton *location_button)
{
  GdkDragAction actions;
  GdkDragAction action;
  gboolean      succeed = FALSE;

  /* check if we don't already know the drop data */
  if (G_LIKELY (!location_button->drop_data_ready))
    {
      /* extract the URI list from the selection data (if valid) */
      if (selection_data->format == 8 && selection_data->length > 0)
        location_button->drop_file_list = thunar_g_file_list_new_from_string ((const gchar *) selection_data->data);

      /* reset the state */
      location_button->drop_data_ready = TRUE;
    }

  /* check if the data was dropped */
  if (G_UNLIKELY (location_button->drop_occurred))
    {
      /* reset the state */
      location_button->drop_occurred = FALSE;

      /* determine the drop dest actions */
      actions = thunar_location_button_get_dest_actions (location_button, context, button, timestamp);
      if (G_LIKELY ((actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)) != 0))
        {
          /* as the user what to do with the drop data */
          action = (context->action == GDK_ACTION_ASK)
                 ? thunar_dnd_ask (button, location_button->file, location_button->drop_file_list, timestamp, actions)
                 : context->action;

          /* perform the requested action */
          if (G_LIKELY (action != 0))
            succeed = thunar_dnd_perform (button, location_button->file, location_button->drop_file_list, action, NULL);
        }

      /* tell the peer that we handled the drop */
      gtk_drag_finish (context, succeed, FALSE, timestamp);

      /* disable the highlighting and release the drag data */
      thunar_location_button_drag_leave (button, context, timestamp, location_button);
    }
}



static void
thunar_location_button_drag_leave (GtkWidget            *button,
                                   GdkDragContext       *context,
                                   guint                 timestamp,
                                   ThunarLocationButton *location_button)
{
  _thunar_return_if_fail (GTK_IS_BUTTON (button));
  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTON (location_button));

  /* reset the file icon state to default appearance */
  if (G_LIKELY (location_button->file_icon_state != THUNAR_FILE_ICON_STATE_DEFAULT))
    {
      /* we may switch to "drop icon state" during drag motion */
      location_button->file_icon_state = THUNAR_FILE_ICON_STATE_DEFAULT;

      /* update the user interface if we still have a file */
      if (G_LIKELY (location_button->file != NULL))
        thunar_location_button_file_changed (location_button, location_button->file);
    }

  /* reset the "drop data ready" status and free the path list */
  if (G_LIKELY (location_button->drop_data_ready))
    {
      thunar_g_file_list_free (location_button->drop_file_list);
      location_button->drop_data_ready = FALSE;
      location_button->drop_file_list = NULL;
    }

  /* be sure to cancel any running enter timeout */
  if (G_LIKELY (location_button->enter_timeout_id != 0))
    g_source_remove (location_button->enter_timeout_id);
}



static gboolean
thunar_location_button_drag_motion (GtkWidget            *button,
                                    GdkDragContext       *context,
                                    gint                  x,
                                    gint                  y,
                                    guint                 timestamp,
                                    ThunarLocationButton *location_button)
{
  ThunarFileIconState file_icon_state;
  GdkDragAction       actions;
  GtkSettings        *settings;
  GdkAtom             target;
  gint                delay;

  _thunar_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_LOCATION_BUTTON (location_button), FALSE);

  /* schedule the enter timeout if not already done */
  if (G_UNLIKELY (location_button->enter_timeout_id == 0))
    {
      /* we use the gtk-menu-popdown-delay here, which seems to be sane for our purpose */
      settings = gtk_settings_get_for_screen (gtk_widget_get_screen (button));
      g_object_get (G_OBJECT (settings), "gtk-menu-popdown-delay", &delay, NULL);

      /* schedule the timeout */
      location_button->enter_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT, delay, thunar_location_button_enter_timeout,
                                                              location_button, thunar_location_button_enter_timeout_destroy);
    }

  /* request the drop on-demand (if we don't have it already) */
  if (G_UNLIKELY (!location_button->drop_data_ready))
    {
      /* check if we can handle that drag data (can only drop text/uri-list) */
      target = gtk_drag_dest_find_target (button, context, NULL);
      if (G_LIKELY (target == gdk_atom_intern_static_string ("text/uri-list")))
        {
          /* request the drop data from the source */
          gtk_drag_get_data (button, context, target, timestamp);
        }

      /* tell GDK that it cannot drop here (yet!) */
      gdk_drag_status (context, 0, timestamp);
    }
  else
    {
      /* check whether we can drop into the buttons' folder */
      actions = thunar_location_button_get_dest_actions (location_button, context, button, timestamp);

      /* determine the new file icon state */
      file_icon_state = (actions == 0) ? THUNAR_FILE_ICON_STATE_DEFAULT : THUNAR_FILE_ICON_STATE_DROP;

      /* check if we need to update the user interface */
      if (G_UNLIKELY (location_button->file_icon_state != file_icon_state))
        {
          /* apply the new icon state */
          location_button->file_icon_state = file_icon_state;

          /* update the user interface if we have a file */
          if (G_LIKELY (location_button->file != NULL))
            thunar_location_button_file_changed (location_button, location_button->file);
        }
    }

  return FALSE;
}



static gboolean
thunar_location_button_enter_timeout (gpointer user_data)
{
  ThunarLocationButton *location_button = THUNAR_LOCATION_BUTTON (user_data);

  GDK_THREADS_ENTER ();

  /* We emulate a "clicked" event here, because else the buttons
   * would be destroyed and replaced by new buttons, which causes
   * the Gtk DND code to dump core once the mouse leaves the area
   * of the new button.
   *
   * Besides that, handling this as "clicked" event allows the user
   * to go back to the initial directory.
   */
  thunar_location_button_clicked (location_button);

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_location_button_enter_timeout_destroy (gpointer user_data)
{
  THUNAR_LOCATION_BUTTON (user_data)->enter_timeout_id = 0;
}



/**
 * thunar_location_button_new:
 *
 * Allocates a new #ThunarLocationButton instance.
 *
 * Return value: the newly allocated #ThunarLocationButton.
 **/
GtkWidget*
thunar_location_button_new (void)
{
  return g_object_new (THUNAR_TYPE_LOCATION_BUTTON, NULL);
}



/**
 * thunar_location_button_get_active:
 * @location_button : a #ThunarLocationButton.
 *
 * Returns %TRUE if @location_button is currently active.
 *
 * Return value: %TRUE if @location_button is currently active.
 **/
gboolean
thunar_location_button_get_active (ThunarLocationButton *location_button)
{
  _thunar_return_val_if_fail (THUNAR_IS_LOCATION_BUTTON (location_button), FALSE);
  return location_button->active;
}



/**
 * thunar_location_button_set_active:
 * @location_button : a #ThunarLocationButton.
 * @active          : %TRUE if @location_button should be active.
 *
 * Sets the active state of @location_button to @active.
 **/
void
thunar_location_button_set_active (ThunarLocationButton *location_button,
                                   gboolean              active)
{
  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTON (location_button));

  /* apply the new state */
  location_button->active = active;

  /* use a bold label for active location buttons */
  gtk_label_set_attributes (GTK_LABEL (location_button->label), active ? thunar_pango_attr_list_bold () : NULL);

  /* notify listeners */
  g_object_notify (G_OBJECT (location_button), "active");
}



/**
 * thunar_location_button_get_file:
 * @location_button : a #ThunarLocationButton.
 *
 * Returns the #ThunarFile for @location_button.
 *
 * Return value: the #ThunarFile for @location_button.
 **/
ThunarFile*
thunar_location_button_get_file (ThunarLocationButton *location_button)
{
  _thunar_return_val_if_fail (THUNAR_IS_LOCATION_BUTTON (location_button), NULL);
  return location_button->file;
}



/**
 * thunar_location_button_set_file:
 * @location_button : a #ThunarLocationButton.
 * @file            : a #ThunarFile or %NULL.
 *
 * Sets the file for @location_button to @file.
 **/
void
thunar_location_button_set_file (ThunarLocationButton *location_button,
                                 ThunarFile           *file)
{
  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTON (location_button));
  _thunar_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  /* check if we already use that file */
  if (G_UNLIKELY (location_button->file == file))
    return;

  /* disconnect from the previous file */
  if (location_button->file != NULL)
    {
      /* stop watching the file */
      thunar_file_unwatch (location_button->file);

      /* disconnect signals and release reference */
      g_signal_handlers_disconnect_by_func (G_OBJECT (location_button->file), thunar_location_button_file_destroy, location_button);
      g_signal_handlers_disconnect_by_func (G_OBJECT (location_button->file), thunar_location_button_file_changed, location_button);
      g_object_unref (G_OBJECT (location_button->file));
    }

  /* activate the new file */
  location_button->file = file;

  /* connect to the new file */
  if (G_LIKELY (file != NULL))
    {
      /* take a reference on the new file */
      g_object_ref (G_OBJECT (file));

      /* watch the file for changes */
      thunar_file_watch (file);

      /* stay informed about changes to the file */
      g_signal_connect_swapped (G_OBJECT (file), "changed", G_CALLBACK (thunar_location_button_file_changed), location_button);
      g_signal_connect_swapped (G_OBJECT (file), "destroy", G_CALLBACK (thunar_location_button_file_destroy), location_button);

      /* update our internal state for the new file (if realized) */
      if (gtk_widget_get_realized (GTK_WIDGET (location_button)))
        thunar_location_button_file_changed (location_button, file);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (location_button), "file");
}



/**
 * thunar_location_button_clicked:
 * @location_button : a #ThunarLocationButton.
 *
 * Emits the ::clicked signal on @location_button.
 **/
void
thunar_location_button_clicked (ThunarLocationButton *location_button)
{
  _thunar_return_if_fail (THUNAR_IS_LOCATION_BUTTON (location_button));
  g_signal_emit (G_OBJECT (location_button), location_button_signals[CLICKED], 0, FALSE);
}


