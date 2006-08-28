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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-application.h>
#include <thunar/thunar-dnd.h>
#include <thunar/thunar-icon-factory.h>
#include <thunar/thunar-path-button.h>
#include <thunar/thunar-pango-extensions.h>
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
  LAST_SIGNAL,
};



static void           thunar_path_button_class_init             (ThunarPathButtonClass  *klass);
static void           thunar_path_button_init                   (ThunarPathButton       *path_button);
static void           thunar_path_button_finalize               (GObject                *object);
static void           thunar_path_button_get_property           (GObject                *object,
                                                                 guint                   prop_id,
                                                                 GValue                 *value,
                                                                 GParamSpec             *pspec);
static void           thunar_path_button_set_property           (GObject                *object,
                                                                 guint                   prop_id,
                                                                 const GValue           *value,
                                                                 GParamSpec             *pspec);
static void           thunar_path_button_style_set              (GtkWidget              *widget,
                                                                 GtkStyle               *previous_style);
static GdkDragAction  thunar_path_button_get_dest_actions       (ThunarPathButton       *path_button,
                                                                 GdkDragContext         *context,
                                                                 GtkWidget              *button,
                                                                 guint                   time);
static void           thunar_path_button_file_changed           (ThunarPathButton       *path_button,
                                                                 ThunarFile             *file);
static void           thunar_path_button_file_destroy           (ThunarPathButton       *path_button,
                                                                 ThunarFile             *file);
static void           thunar_path_button_align_size_request     (GtkWidget              *align,
                                                                 GtkRequisition         *requisition,
                                                                 ThunarPathButton       *path_button);
static gboolean       thunar_path_button_button_press_event     (GtkWidget              *button,
                                                                 GdkEventButton         *event,
                                                                 ThunarPathButton       *path_button);
static gboolean       thunar_path_button_button_release_event   (GtkWidget              *button,
                                                                 GdkEventButton         *event,
                                                                 ThunarPathButton       *path_button);
static gboolean       thunar_path_button_drag_drop              (GtkWidget              *button,
                                                                 GdkDragContext         *context,
                                                                 gint                    x,
                                                                 gint                    y,
                                                                 guint                   time,
                                                                 ThunarPathButton       *path_button);
static void           thunar_path_button_drag_data_get          (GtkWidget              *button,
                                                                 GdkDragContext         *context,
                                                                 GtkSelectionData       *selection_data,
                                                                 guint                   info,
                                                                 guint                   time,
                                                                 ThunarPathButton       *path_button);
static void           thunar_path_button_drag_data_received     (GtkWidget              *button,
                                                                 GdkDragContext         *context,
                                                                 gint                    x,
                                                                 gint                    y,
                                                                 GtkSelectionData       *selection_data,
                                                                 guint                   info,
                                                                 guint                   time,
                                                                 ThunarPathButton       *path_button);
static void           thunar_path_button_drag_leave             (GtkWidget              *button,
                                                                 GdkDragContext         *context,
                                                                 guint                   time,
                                                                 ThunarPathButton       *path_button);
static gboolean       thunar_path_button_drag_motion            (GtkWidget              *button,
                                                                 GdkDragContext         *context,
                                                                 gint                    x,
                                                                 gint                    y,
                                                                 guint                   time,
                                                                 ThunarPathButton       *path_button);
static gboolean       thunar_path_button_enter_timeout          (gpointer                user_data);
static void           thunar_path_button_enter_timeout_destroy  (gpointer                user_data);



struct _ThunarPathButtonClass
{
  GtkAlignmentClass __parent__;
};

struct _ThunarPathButton
{
  GtkAlignment __parent__;

  GtkWidget          *image;
  GtkWidget          *label;

  /* the current icon state (i.e. accepting drops) */
  ThunarFileIconState file_icon_state;
  
  /* enter folders using DnD */
  gint                enter_timeout_id;

  /* drop support for the button */
  GList              *drop_path_list;
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

static GObjectClass *thunar_path_button_parent_class;
static guint         path_button_signals[LAST_SIGNAL];



GType
thunar_path_button_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarPathButtonClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_path_button_class_init,
        NULL,
        NULL,
        sizeof (ThunarPathButton),
        0,
        (GInstanceInitFunc) thunar_path_button_init,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_ALIGNMENT, I_("ThunarPathButton"), &info, 0);
    }

  return type;
}



static void
thunar_path_button_class_init (ThunarPathButtonClass *klass)
{
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  /* determine the parent type class */
  thunar_path_button_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_path_button_finalize;
  gobject_class->get_property = thunar_path_button_get_property;
  gobject_class->set_property = thunar_path_button_set_property;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->style_set = thunar_path_button_style_set;

  /**
   * ThunarPathButton:active:
   *
   * Whether the path button is currently active.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ACTIVE,
                                   g_param_spec_boolean ("active",
                                                         "active",
                                                         "active",
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  /**
   * ThunarPathButton:file:
   *
   * The #ThunarFile represented by this path button.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILE,
                                   g_param_spec_object ("file",
                                                        "file",
                                                        "file",
                                                        THUNAR_TYPE_FILE,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarPathButton::clicked:
   * @path_button : a #ThunarPathButton.
   *
   * Emitted by @path_button when the user clicks on the
   * @path_button or thunar_path_button_clicked() is
   * called.
   **/
  path_button_signals[CLICKED] =
    g_signal_new (I_("clicked"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ThunarPathButton::context-menu:
   * @path_button : a #ThunarPathButton.
   * @event           : a #GdkEventButton.
   *
   * Emitted by @path_button when the user requests to open
   * the context menu for @path_button.
   **/
  path_button_signals[CONTEXT_MENU] =
    g_signal_new (I_("context-menu"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  0, NULL, NULL,
                  g_cclosure_marshal_VOID__BOXED,
                  G_TYPE_NONE, 1, GDK_TYPE_EVENT);
}



static void
thunar_path_button_init (ThunarPathButton *path_button)
{
  GtkWidget *button;
  GtkWidget *align;
  GtkWidget *hbox;

  path_button->enter_timeout_id = -1;

  /* create the toggle button */
  button = gtk_toggle_button_new ();
  GTK_WIDGET_UNSET_FLAGS (button, GTK_CAN_FOCUS);
  g_signal_connect_swapped (G_OBJECT (button), "clicked", G_CALLBACK (thunar_path_button_clicked), path_button);
  exo_mutual_binding_new (G_OBJECT (path_button), "active", G_OBJECT (button), "active");
  gtk_container_add (GTK_CONTAINER (path_button), button);
  gtk_widget_show (button);

  /* setup drag support for the button */
  gtk_drag_source_set (GTK_WIDGET (button), GDK_BUTTON1_MASK, drag_targets, G_N_ELEMENTS (drag_targets), GDK_ACTION_LINK);
  gtk_drag_dest_set (GTK_WIDGET (button), GTK_DEST_DEFAULT_HIGHLIGHT | GTK_DEST_DEFAULT_MOTION, drag_targets,
                     G_N_ELEMENTS (drag_targets), GDK_ACTION_ASK | GDK_ACTION_COPY | GDK_ACTION_LINK | GDK_ACTION_MOVE);
  g_signal_connect (G_OBJECT (button), "button-press-event", G_CALLBACK (thunar_path_button_button_press_event), path_button);
  g_signal_connect (G_OBJECT (button), "button-release-event", G_CALLBACK (thunar_path_button_button_release_event), path_button);
  g_signal_connect (G_OBJECT (button), "drag-drop", G_CALLBACK (thunar_path_button_drag_drop), path_button);
  g_signal_connect (G_OBJECT (button), "drag-data-get", G_CALLBACK (thunar_path_button_drag_data_get), path_button);
  g_signal_connect (G_OBJECT (button), "drag-data-received", G_CALLBACK (thunar_path_button_drag_data_received), path_button);
  g_signal_connect (G_OBJECT (button), "drag-leave", G_CALLBACK (thunar_path_button_drag_leave), path_button);
  g_signal_connect (G_OBJECT (button), "drag-motion", G_CALLBACK (thunar_path_button_drag_motion), path_button);

  /* create the horizontal box */
  hbox = gtk_hbox_new (FALSE, 2);
  gtk_container_add (GTK_CONTAINER (button), hbox);
  gtk_widget_show (hbox);

  /* create the button image */
  path_button->image = gtk_image_new ();
  gtk_box_pack_start (GTK_BOX (hbox), path_button->image, TRUE, FALSE, 0);
  gtk_widget_show (path_button->image);

  /* create the button label alignment */
  align = gtk_alignment_new (0.5f, 0.5f, 1.0f, 1.0f);
  g_signal_connect (G_OBJECT (align), "size-request", G_CALLBACK (thunar_path_button_align_size_request), path_button);
  gtk_box_pack_start (GTK_BOX (hbox), align, TRUE, TRUE, 0);
  gtk_widget_show (align);

  /* create the button label */
  path_button->label = g_object_new (GTK_TYPE_LABEL, NULL);
  exo_binding_new (G_OBJECT (path_button->label), "visible", G_OBJECT (align), "visible");
  gtk_container_add (GTK_CONTAINER (align), path_button->label);
  gtk_widget_show (path_button->label);
}



static void
thunar_path_button_finalize (GObject *object)
{
  ThunarPathButton *path_button = THUNAR_PATH_BUTTON (object);

  /* release the drop path list (just in case the drag-leave wasn't fired before) */
  thunar_vfs_path_list_free (path_button->drop_path_list);

  /* be sure to cancel any pending enter timeout */
  if (G_UNLIKELY (path_button->enter_timeout_id >= 0))
    g_source_remove (path_button->enter_timeout_id);

  /* disconnect from the file */
  thunar_path_button_set_file (path_button, NULL);

  (*G_OBJECT_CLASS (thunar_path_button_parent_class)->finalize) (object);
}



static void
thunar_path_button_get_property (GObject    *object,
                                 guint       prop_id,
                                 GValue     *value,
                                 GParamSpec *pspec)
{
  ThunarPathButton *path_button = THUNAR_PATH_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      g_value_set_boolean (value, thunar_path_button_get_active (path_button));
      break;

    case PROP_FILE:
      g_value_set_object (value, thunar_path_button_get_file (path_button));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_path_button_set_property (GObject      *object,
                                 guint         prop_id,
                                 const GValue *value,
                                 GParamSpec   *pspec)
{
  ThunarPathButton *path_button = THUNAR_PATH_BUTTON (object);

  switch (prop_id)
    {
    case PROP_ACTIVE:
      thunar_path_button_set_active (path_button, g_value_get_boolean (value));
      break;

    case PROP_FILE:
      thunar_path_button_set_file (path_button, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_path_button_style_set (GtkWidget *widget,
                              GtkStyle  *previous_style)
{
  ThunarPathButton *path_button = THUNAR_PATH_BUTTON (widget);

  /* update the user interface if we have a file */
  if (G_LIKELY (path_button->file != NULL))
    thunar_path_button_file_changed (path_button, path_button->file);

  (*GTK_WIDGET_CLASS (thunar_path_button_parent_class)->style_set) (widget, previous_style);
}



static GdkDragAction
thunar_path_button_get_dest_actions (ThunarPathButton *path_button,
                                     GdkDragContext   *context,
                                     GtkWidget        *button,
                                     guint             time)
{
  GdkDragAction actions = 0;
  GdkDragAction action = 0;

  /* check if we can drop here */
  if (G_LIKELY (path_button->file != NULL))
    {
      /* determine the possible drop actions for the file (and the suggested action if any) */
      actions = thunar_file_accepts_drop (path_button->file, path_button->drop_path_list, context, &action);
    }

  /* tell Gdk whether we can drop here */
  gdk_drag_status (context, action, time);

  return actions;
}



static void
thunar_path_button_file_changed (ThunarPathButton *path_button,
                                 ThunarFile       *file)
{
  ThunarIconFactory *icon_factory;
  GtkIconTheme      *icon_theme;
  GtkSettings       *settings;
#if GTK_CHECK_VERSION(2,8,0)
  const gchar       *icon_name;
#endif
  GdkPixbuf         *icon;
  gint               height;
  gint               width;
  gint               size;

  _thunar_return_if_fail (THUNAR_IS_PATH_BUTTON (path_button));
  _thunar_return_if_fail (path_button->file == file);
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* determine the icon theme for the widget */
  icon_theme = gtk_icon_theme_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (path_button)));

  /* the label is hidden for the file system root */
  if (thunar_file_is_local (file) && thunar_file_is_root (file))
    {
      /* hide the label widget */
      gtk_widget_hide (path_button->label);
    }
  else
    {
      /* update and show the label widget */
      gtk_label_set_text (GTK_LABEL (path_button->label), thunar_file_get_display_name (file));
      gtk_widget_show (path_button->label);
    }

  /* the image is only visible for certain special paths */
  if (thunar_file_is_home (file) || thunar_file_is_root (file))
    {
      /* determine the icon size for menus (to be compatible with GtkPathBar) */
      settings = gtk_settings_get_for_screen (gtk_widget_get_screen (GTK_WIDGET (path_button)));
      if (gtk_icon_size_lookup_for_settings (settings, GTK_ICON_SIZE_MENU, &width, &height))
        size = MAX (width, height);
      else
        size = 16;

      /* update the icon for the image */
      icon_factory = thunar_icon_factory_get_for_icon_theme (icon_theme);
      icon = thunar_icon_factory_load_file_icon (icon_factory, file, path_button->file_icon_state, size);
      gtk_image_set_from_pixbuf (GTK_IMAGE (path_button->image), icon);
      g_object_unref (G_OBJECT (icon_factory));
      g_object_unref (G_OBJECT (icon));

      /* show the image widget */
      gtk_widget_show (path_button->image);
    }
  else
    {
      /* hide the image widget */
      gtk_widget_hide (path_button->image);
    }

#if GTK_CHECK_VERSION(2,8,0)
  /* setup the DnD icon for the button */
  icon_name = thunar_file_get_custom_icon (file);
  if (G_LIKELY (icon_name == NULL))
    icon_name = thunar_file_get_icon_name (file, path_button->file_icon_state, icon_theme);
  gtk_drag_source_set_icon_name (GTK_BIN (path_button)->child, icon_name);
#endif
}



static void
thunar_path_button_file_destroy (ThunarPathButton *path_button,
                                 ThunarFile       *file)
{
  _thunar_return_if_fail (THUNAR_IS_PATH_BUTTON (path_button));
  _thunar_return_if_fail (path_button->file == file);
  _thunar_return_if_fail (THUNAR_IS_FILE (file));

  /* the file is gone, no need to keep the button around anymore */
  gtk_widget_destroy (GTK_WIDGET (path_button));
}



static void
thunar_path_button_align_size_request (GtkWidget        *align,
                                       GtkRequisition   *requisition,
                                       ThunarPathButton *path_button)
{
  PangoLayout *layout;
  gint         normal_height;
  gint         normal_width;
  gint         bold_height;
  gint         bold_width;

  /* allocate a pango layout for the label text */
  layout = gtk_widget_create_pango_layout (path_button->label, gtk_label_get_text (GTK_LABEL (path_button->label)));

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
thunar_path_button_button_press_event (GtkWidget        *button,
                                       GdkEventButton   *event,
                                       ThunarPathButton *path_button)
{
  _thunar_return_val_if_fail (THUNAR_IS_PATH_BUTTON (path_button), FALSE);

  /* check if we can handle the button event */
  if (G_UNLIKELY (event->button == 2))
    {
      /* set button state to inconsistent while holding down the middle-mouse-button */
      gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (button), TRUE);
    }
  else if (event->button == 3)
    {
      /* emit the "context-menu" signal and let the surrounding ThunarPathButtons popup a menu */
      g_signal_emit (G_OBJECT (path_button), path_button_signals[CONTEXT_MENU], 0, event);
      return TRUE;
    }

  return FALSE;
}



static gboolean
thunar_path_button_button_release_event (GtkWidget        *button,
                                         GdkEventButton   *event,
                                         ThunarPathButton *path_button)
{
  ThunarApplication *application;

  _thunar_return_val_if_fail (THUNAR_IS_PATH_BUTTON (path_button), FALSE);

  /* reset inconsistent button state after releasing the middle-mouse-button */
  if (G_UNLIKELY (event->button == 2))
    {
      /* reset button state */
      gtk_toggle_button_set_inconsistent (GTK_TOGGLE_BUTTON (button), FALSE);

      /* verify that we still have a valid file */
      if (G_LIKELY (path_button->file != NULL))
        {
          /* open a new window for the folder */
          application = thunar_application_get ();
          thunar_application_open_window (application, path_button->file, gtk_widget_get_screen (GTK_WIDGET (path_button)));
          g_object_unref (G_OBJECT (application));
        }
    }

  return FALSE;
}



static gboolean
thunar_path_button_drag_drop (GtkWidget        *button,
                              GdkDragContext   *context,
                              gint              x,
                              gint              y,
                              guint             time,
                              ThunarPathButton *path_button)
{
  GdkAtom target;

  _thunar_return_val_if_fail (GTK_IS_WIDGET (button), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_PATH_BUTTON (path_button), FALSE);

  /* determine the DnD target and see if we can handle it */
  target = gtk_drag_dest_find_target (button, context, NULL);
  if (G_UNLIKELY (target != gdk_atom_intern ("text/uri-list", FALSE)))
    return FALSE;

  /* set state so drag-data-received knows that
   * this is really a drop this time.
   */
  path_button->drop_occurred = TRUE;

  /* request the drag data from the source */
  gtk_drag_get_data (button, context, target, time);

  /* we'll call gtk_drag_finish() later */
  return TRUE;
}



static void
thunar_path_button_drag_data_get (GtkWidget        *button,
                                  GdkDragContext   *context,
                                  GtkSelectionData *selection_data,
                                  guint             info,
                                  guint             time,
                                  ThunarPathButton *path_button)
{
  gchar *uri_string;
  GList  path_list;

  _thunar_return_if_fail (GTK_IS_WIDGET (button));
  _thunar_return_if_fail (THUNAR_IS_PATH_BUTTON (path_button));

  /* verify that we have a valid file */
  if (G_LIKELY (path_button->file != NULL))
    {
      /* transform the path into an uri list string */
      path_list.data = thunar_file_get_path (path_button->file); path_list.next = path_list.prev = NULL;
      uri_string = thunar_vfs_path_list_to_string (&path_list);

      /* set the uri list for the drag selection */
      gtk_selection_data_set (selection_data, selection_data->target, 8, (guchar *) uri_string, strlen (uri_string));

      /* cleanup */
      g_free (uri_string);
    }
}



static void
thunar_path_button_drag_data_received (GtkWidget        *button,
                                       GdkDragContext   *context,
                                       gint              x,
                                       gint              y,
                                       GtkSelectionData *selection_data,
                                       guint             info,
                                       guint             time,
                                       ThunarPathButton *path_button)
{
  GdkDragAction actions;
  GdkDragAction action;
  gboolean      succeed = FALSE;

  /* check if we don't already know the drop data */
  if (G_LIKELY (!path_button->drop_data_ready))
    {
      /* extract the URI list from the selection data (if valid) */
      if (selection_data->format == 8 && selection_data->length > 0)
        path_button->drop_path_list = thunar_vfs_path_list_from_string ((const gchar *) selection_data->data, NULL);

      /* reset the state */
      path_button->drop_data_ready = TRUE;
    }

  /* check if the data was dropped */
  if (G_UNLIKELY (path_button->drop_occurred))
    {
      /* reset the state */
      path_button->drop_occurred = FALSE;

      /* determine the drop dest actions */
      actions = thunar_path_button_get_dest_actions (path_button, context, button, time);
      if (G_LIKELY ((actions & (GDK_ACTION_COPY | GDK_ACTION_MOVE | GDK_ACTION_LINK)) != 0))
        {
          /* as the user what to do with the drop data */
          action = (context->action == GDK_ACTION_ASK) ? thunar_dnd_ask (button, time, actions) : context->action;

          /* perform the requested action */
          if (G_LIKELY (action != 0))
            succeed = thunar_dnd_perform (button, path_button->file, path_button->drop_path_list, action, NULL);
        }

      /* tell the peer that we handled the drop */
      gtk_drag_finish (context, succeed, FALSE, time);

      /* disable the highlighting and release the drag data */
      thunar_path_button_drag_leave (button, context, time, path_button);
    }
}



static void
thunar_path_button_drag_leave (GtkWidget        *button,
                               GdkDragContext   *context,
                               guint             time,
                               ThunarPathButton *path_button)
{
  _thunar_return_if_fail (GTK_IS_BUTTON (button));
  _thunar_return_if_fail (THUNAR_IS_PATH_BUTTON (path_button));

  /* reset the file icon state to default appearance */
  if (G_LIKELY (path_button->file_icon_state != THUNAR_FILE_ICON_STATE_DEFAULT))
    {
      /* we may switch to "drop icon state" during drag motion */
      path_button->file_icon_state = THUNAR_FILE_ICON_STATE_DEFAULT;

      /* update the user interface if we still have a file */
      if (G_LIKELY (path_button->file != NULL))
        thunar_path_button_file_changed (path_button, path_button->file);
    }

  /* reset the "drop data ready" status and free the path list */
  if (G_LIKELY (path_button->drop_data_ready))
    {
      thunar_vfs_path_list_free (path_button->drop_path_list);
      path_button->drop_data_ready = FALSE;
      path_button->drop_path_list = NULL;
    }

  /* be sure to cancel any running enter timeout */
  if (G_LIKELY (path_button->enter_timeout_id >= 0))
    g_source_remove (path_button->enter_timeout_id);
}



static gboolean
thunar_path_button_drag_motion (GtkWidget        *button,
                                GdkDragContext   *context,
                                gint              x,
                                gint              y,
                                guint             time,
                                ThunarPathButton *path_button)
{
  ThunarFileIconState file_icon_state;
  GdkDragAction       actions;
  GtkSettings        *settings;
  GdkAtom             target;
  gint                delay;

  _thunar_return_val_if_fail (GTK_IS_BUTTON (button), FALSE);
  _thunar_return_val_if_fail (THUNAR_IS_PATH_BUTTON (path_button), FALSE);

  /* schedule the enter timeout if not already done */
  if (G_UNLIKELY (path_button->enter_timeout_id < 0))
    {
      /* we use the gtk-menu-popdown-delay here, which seems to be sane for our purpose */
      settings = gtk_settings_get_for_screen (gtk_widget_get_screen (button));
      g_object_get (G_OBJECT (settings), "gtk-menu-popdown-delay", &delay, NULL);

      /* schedule the timeout */
      path_button->enter_timeout_id = g_timeout_add_full (G_PRIORITY_DEFAULT, delay, thunar_path_button_enter_timeout,
                                                              path_button, thunar_path_button_enter_timeout_destroy);
    }

  /* request the drop on-demand (if we don't have it already) */
  if (G_UNLIKELY (!path_button->drop_data_ready))
    {
      /* check if we can handle that drag data (can only drop text/uri-list) */
      target = gtk_drag_dest_find_target (button, context, NULL);
      if (G_LIKELY (target == gdk_atom_intern ("text/uri-list", FALSE)))
        {
          /* request the drop data from the source */
          gtk_drag_get_data (button, context, target, time);
        }

      /* tell GDK that it cannot drop here (yet!) */
      gdk_drag_status (context, 0, time);
    }
  else
    {
      /* check whether we can drop into the buttons' folder */
      actions = thunar_path_button_get_dest_actions (path_button, context, button, time);

      /* determine the new file icon state */
      file_icon_state = (actions == 0) ? THUNAR_FILE_ICON_STATE_DEFAULT : THUNAR_FILE_ICON_STATE_DROP;

      /* check if we need to update the user interface */
      if (G_UNLIKELY (path_button->file_icon_state != file_icon_state))
        {
          /* apply the new icon state */
          path_button->file_icon_state = file_icon_state;

          /* update the user interface if we have a file */
          if (G_LIKELY (path_button->file != NULL))
            thunar_path_button_file_changed (path_button, path_button->file);
        }
    }

  return FALSE;
}



static gboolean
thunar_path_button_enter_timeout (gpointer user_data)
{
  ThunarPathButton *path_button = THUNAR_PATH_BUTTON (user_data);

  GDK_THREADS_ENTER ();

  /* We emulate a "clicked" event here, because else the buttons
   * would be destroyed and replaced by new buttons, which causes
   * the Gtk DND code to dump core once the mouse leaves the area
   * of the new button.
   *
   * Besides that, handling this as "clicked" event allows the user
   * to go back to the initial directory.
   */
  thunar_path_button_clicked (path_button);

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_path_button_enter_timeout_destroy (gpointer user_data)
{
  THUNAR_PATH_BUTTON (user_data)->enter_timeout_id = -1;
}



/**
 * thunar_path_button_new:
 *
 * Allocates a new #ThunarPathButton instance.
 *
 * Return value: the newly allocated #ThunarPathButton.
 **/
GtkWidget*
thunar_path_button_new (void)
{
  return g_object_new (THUNAR_TYPE_PATH_BUTTON, NULL);
}



/**
 * thunar_path_button_get_active:
 * @path_button : a #ThunarPathButton.
 *
 * Returns %TRUE if @path_button is currently active.
 *
 * Return value: %TRUE if @path_button is currently active.
 **/
gboolean
thunar_path_button_get_active (ThunarPathButton *path_button)
{
  _thunar_return_val_if_fail (THUNAR_IS_PATH_BUTTON (path_button), FALSE);
  return path_button->active;
}



/**
 * thunar_path_button_set_active:
 * @path_button : a #ThunarPathButton.
 * @active          : %TRUE if @path_button should be active.
 *
 * Sets the active state of @path_button to @active.
 **/
void
thunar_path_button_set_active (ThunarPathButton *path_button,
                               gboolean          active)
{
  _thunar_return_if_fail (THUNAR_IS_PATH_BUTTON (path_button));

  /* apply the new state */
  path_button->active = active;

  /* use a bold label for active path buttons */
  gtk_label_set_attributes (GTK_LABEL (path_button->label), active ? thunar_pango_attr_list_bold () : NULL);

  /* notify listeners */
  g_object_notify (G_OBJECT (path_button), "active");
}



/**
 * thunar_path_button_get_file:
 * @path_button : a #ThunarPathButton.
 *
 * Returns the #ThunarFile for @path_button.
 *
 * Return value: the #ThunarFile for @path_button.
 **/
ThunarFile*
thunar_path_button_get_file (ThunarPathButton *path_button)
{
  _thunar_return_val_if_fail (THUNAR_IS_PATH_BUTTON (path_button), NULL);
  return path_button->file;
}



/**
 * thunar_path_button_set_file:
 * @path_button : a #ThunarPathButton.
 * @file            : a #ThunarFile or %NULL.
 *
 * Sets the file for @path_button to @file.
 **/
void
thunar_path_button_set_file (ThunarPathButton *path_button,
                             ThunarFile       *file)
{
  _thunar_return_if_fail (THUNAR_IS_PATH_BUTTON (path_button));
  _thunar_return_if_fail (file == NULL || THUNAR_IS_FILE (file));

  /* check if we already use that file */
  if (G_UNLIKELY (path_button->file == file))
    return;

  /* disconnect from the previous file */
  if (path_button->file != NULL)
    {
      /* stop watching the file */
      thunar_file_unwatch (path_button->file);

      /* disconnect signals and release reference */
      g_signal_handlers_disconnect_by_func (G_OBJECT (path_button->file), thunar_path_button_file_destroy, path_button);
      g_signal_handlers_disconnect_by_func (G_OBJECT (path_button->file), thunar_path_button_file_changed, path_button);
      g_object_unref (G_OBJECT (path_button->file));
    }

  /* activate the new file */
  path_button->file = file;

  /* connect to the new file */
  if (G_LIKELY (file != NULL))
    {
      /* take a reference on the new file */
      g_object_ref (G_OBJECT (file));

      /* watch the file for changes */
      thunar_file_watch (file);

      /* stay informed about changes to the file */
      g_signal_connect_swapped (G_OBJECT (file), "changed", G_CALLBACK (thunar_path_button_file_changed), path_button);
      g_signal_connect_swapped (G_OBJECT (file), "destroy", G_CALLBACK (thunar_path_button_file_destroy), path_button);

      /* update our internal state for the new file (if realized) */
      if (GTK_WIDGET_REALIZED (path_button))
        thunar_path_button_file_changed (path_button, file);
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (path_button), "file");
}



/**
 * thunar_path_button_clicked:
 * @path_button : a #ThunarPathButton.
 *
 * Emits the ::clicked signal on @path_button.
 **/
void
thunar_path_button_clicked (ThunarPathButton *path_button)
{
  _thunar_return_if_fail (THUNAR_IS_PATH_BUTTON (path_button));
  g_signal_emit (G_OBJECT (path_button), path_button_signals[CLICKED], 0);
}


