/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2025 Jeff Guy <jeffguy77@gmail.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, write to the Free
 * Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#include "thunar/thunar-filter-bar.h"
#include "thunar/thunar-private.h"

#include <gdk/gdkkeysyms.h>
#include <libxfce4ui/libxfce4ui.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_FILTER_TEXT,
};

/* Signal identifiers */
enum
{
  CLOSED,
  LAST_SIGNAL,
};



static void
thunar_filter_bar_finalize (GObject *object);
static void
thunar_filter_bar_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec);
static void
thunar_filter_bar_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec);
static void
thunar_filter_bar_entry_changed (GtkEditable     *editable,
                                 ThunarFilterBar *filter_bar);
static gboolean
thunar_filter_bar_entry_key_press (GtkWidget       *widget,
                                   GdkEventKey     *event,
                                   ThunarFilterBar *filter_bar);
static void
thunar_filter_bar_close_clicked (GtkButton       *button,
                                 ThunarFilterBar *filter_bar);



struct _ThunarFilterBarClass
{
  GtkBoxClass __parent__;

  /* signals */
  void (*closed) (ThunarFilterBar *filter_bar);
};

struct _ThunarFilterBar
{
  GtkBox __parent__;

  GtkWidget *entry;        /* text entry for typing filter text */
  GtkWidget *close_button; /* button to dismiss the filter bar */

  gchar *filter_text;      /* current filter string, or NULL if empty */
};



/* table of signal IDs, indexed by the signal enum values */
static guint filter_bar_signals[LAST_SIGNAL];



G_DEFINE_TYPE (ThunarFilterBar, thunar_filter_bar, GTK_TYPE_BOX)



/**
 * thunar_filter_bar_class_init:
 * @klass : a #ThunarFilterBarClass.
 *
 * Registers GObject properties and signals for the filter bar.
 **/
static void
thunar_filter_bar_class_init (ThunarFilterBarClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_filter_bar_finalize;
  gobject_class->get_property = thunar_filter_bar_get_property;
  gobject_class->set_property = thunar_filter_bar_set_property;

  /**
   * ThunarFilterBar:filter-text:
   *
   * The current text in the filter entry, used to filter
   * displayed files by substring match on their display name.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_FILTER_TEXT,
                                   g_param_spec_string ("filter-text",
                                                        "filter-text",
                                                        "filter-text",
                                                        NULL,
                                                        G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  /**
   * ThunarFilterBar::closed:
   * @filter_bar : a #ThunarFilterBar.
   *
   * Emitted when the user closes the filter bar, either by
   * clicking the close button or pressing Escape.
   **/
  filter_bar_signals[CLOSED] =
    g_signal_new ("closed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarFilterBarClass, closed),
                  NULL, NULL,
                  NULL,
                  G_TYPE_NONE, 0);
}



static void
/**
 * thunar_filter_bar_init:
 * @filter_bar : a #ThunarFilterBar.
 *
 * Initializes the filter bar widget, creating the label, text
 * entry, and close button, and connecting their signals.
 **/
thunar_filter_bar_init (ThunarFilterBar *filter_bar)
{
  GtkWidget *label;

  gtk_orientable_set_orientation (GTK_ORIENTABLE (filter_bar), GTK_ORIENTATION_HORIZONTAL);
  gtk_box_set_spacing (GTK_BOX (filter_bar), 6);

  /* small vertical padding to keep the bar compact */
  gtk_widget_set_margin_top (GTK_WIDGET (filter_bar), 2);
  gtk_widget_set_margin_bottom (GTK_WIDGET (filter_bar), 2);
  gtk_widget_set_margin_start (GTK_WIDGET (filter_bar), 6);
  gtk_widget_set_margin_end (GTK_WIDGET (filter_bar), 6);

  /* "Filter:" label */
  label = gtk_label_new_with_mnemonic (_("F_ilter:"));
  gtk_box_pack_start (GTK_BOX (filter_bar), label, FALSE, FALSE, 0);
  gtk_widget_show (label);

  /* text entry */
  filter_bar->entry = gtk_entry_new ();
  gtk_entry_set_placeholder_text (GTK_ENTRY (filter_bar->entry), _("Filter..."));
  gtk_widget_set_hexpand (filter_bar->entry, TRUE);
  gtk_label_set_mnemonic_widget (GTK_LABEL (label), filter_bar->entry);
  gtk_box_pack_start (GTK_BOX (filter_bar), filter_bar->entry, TRUE, TRUE, 0);
  gtk_widget_show (filter_bar->entry);

  /* connect entry signals */
  g_signal_connect (G_OBJECT (filter_bar->entry), "changed",
                    G_CALLBACK (thunar_filter_bar_entry_changed), filter_bar);
  g_signal_connect (G_OBJECT (filter_bar->entry), "key-press-event",
                    G_CALLBACK (thunar_filter_bar_entry_key_press), filter_bar);

  /* close button */
  filter_bar->close_button = gtk_button_new_from_icon_name ("window-close-symbolic", GTK_ICON_SIZE_MENU);
  gtk_button_set_relief (GTK_BUTTON (filter_bar->close_button), GTK_RELIEF_NONE);
  gtk_widget_set_tooltip_text (filter_bar->close_button, _("Close the filter bar"));
  gtk_box_pack_start (GTK_BOX (filter_bar), filter_bar->close_button, FALSE, FALSE, 0);
  gtk_widget_show (filter_bar->close_button);

  g_signal_connect (G_OBJECT (filter_bar->close_button), "clicked",
                    G_CALLBACK (thunar_filter_bar_close_clicked), filter_bar);

  /* start with no active filter */
  filter_bar->filter_text = NULL;
}



/**
 * thunar_filter_bar_finalize:
 * @object : a #ThunarFilterBar.
 *
 * Frees the filter text string and chains up to the parent
 * class finalize handler.
 **/
static void
thunar_filter_bar_finalize (GObject *object)
{
  ThunarFilterBar *filter_bar = THUNAR_FILTER_BAR (object);

  g_free (filter_bar->filter_text);

  (*G_OBJECT_CLASS (thunar_filter_bar_parent_class)->finalize) (object);
}



/**
 * thunar_filter_bar_get_property:
 * @object  : a #ThunarFilterBar.
 * @prop_id : the property identifier.
 * @value   : return location for the property value.
 * @pspec   : the #GParamSpec of the property.
 *
 * GObject get_property implementation for the filter bar.
 **/
static void
thunar_filter_bar_get_property (GObject    *object,
                                guint       prop_id,
                                GValue     *value,
                                GParamSpec *pspec)
{
  ThunarFilterBar *filter_bar = THUNAR_FILTER_BAR (object);

  switch (prop_id)
    {
    case PROP_FILTER_TEXT:
      g_value_set_string (value, filter_bar->filter_text);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/**
 * thunar_filter_bar_set_property:
 * @object  : a #ThunarFilterBar.
 * @prop_id : the property identifier.
 * @value   : the property value to set.
 * @pspec   : the #GParamSpec of the property.
 *
 * GObject set_property implementation for the filter bar.
 **/
static void
thunar_filter_bar_set_property (GObject      *object,
                                guint         prop_id,
                                const GValue *value,
                                GParamSpec   *pspec)
{
  ThunarFilterBar *filter_bar = THUNAR_FILTER_BAR (object);

  switch (prop_id)
    {
    case PROP_FILTER_TEXT:
      thunar_filter_bar_set_filter_text (filter_bar, g_value_get_string (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/**
 * thunar_filter_bar_entry_changed:
 *
 * Called when the user types in the entry. Updates the
 * filter-text property which notifies listeners.
 **/
static void
thunar_filter_bar_entry_changed (GtkEditable     *editable,
                                 ThunarFilterBar *filter_bar)
{
  const gchar *text;

  _thunar_return_if_fail (THUNAR_IS_FILTER_BAR (filter_bar));

  text = gtk_entry_get_text (GTK_ENTRY (filter_bar->entry));

  g_free (filter_bar->filter_text);
  filter_bar->filter_text = (text != NULL && *text != '\0') ? g_strdup (text) : NULL;

  g_object_notify (G_OBJECT (filter_bar), "filter-text");
}



/**
 * thunar_filter_bar_entry_key_press:
 *
 * Handles key press events on the entry. Escape closes the
 * filter bar.
 **/
static gboolean
thunar_filter_bar_entry_key_press (GtkWidget       *widget,
                                   GdkEventKey     *event,
                                   ThunarFilterBar *filter_bar)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILTER_BAR (filter_bar), FALSE);

  if (event->keyval == GDK_KEY_Escape)
    {
      /* two-stage Escape: first press clears text, second press closes the bar */
      if (filter_bar->filter_text != NULL)
        thunar_filter_bar_set_filter_text (filter_bar, NULL);
      else
        g_signal_emit (filter_bar, filter_bar_signals[CLOSED], 0);
      return TRUE;
    }

  return FALSE;
}



/**
 * thunar_filter_bar_close_clicked:
 *
 * Called when the close button is clicked. Emits the "closed" signal.
 **/
static void
thunar_filter_bar_close_clicked (GtkButton       *button,
                                 ThunarFilterBar *filter_bar)
{
  _thunar_return_if_fail (THUNAR_IS_FILTER_BAR (filter_bar));

  g_signal_emit (filter_bar, filter_bar_signals[CLOSED], 0);
}



/**
 * thunar_filter_bar_new:
 *
 * Allocates a new #ThunarFilterBar instance.
 *
 * Return value: the newly allocated #ThunarFilterBar.
 **/
GtkWidget *
thunar_filter_bar_new (void)
{
  return g_object_new (THUNAR_TYPE_FILTER_BAR, NULL);
}



/**
 * thunar_filter_bar_get_filter_text:
 * @filter_bar : a #ThunarFilterBar.
 *
 * Returns the current filter text, or %NULL if empty.
 *
 * Return value: the filter text string owned by @filter_bar, or %NULL.
 **/
const gchar *
thunar_filter_bar_get_filter_text (ThunarFilterBar *filter_bar)
{
  _thunar_return_val_if_fail (THUNAR_IS_FILTER_BAR (filter_bar), NULL);

  return filter_bar->filter_text;
}



/**
 * thunar_filter_bar_set_filter_text:
 * @filter_bar : a #ThunarFilterBar.
 * @text       : the new filter text, or %NULL to clear.
 *
 * Sets the filter entry text. Pass %NULL or an empty string
 * to clear the filter.
 **/
void
thunar_filter_bar_set_filter_text (ThunarFilterBar *filter_bar,
                                   const gchar     *text)
{
  _thunar_return_if_fail (THUNAR_IS_FILTER_BAR (filter_bar));

  /* update internal state */
  g_free (filter_bar->filter_text);
  filter_bar->filter_text = (text != NULL && *text != '\0') ? g_strdup (text) : NULL;

  /* sync the entry widget without re-triggering the changed callback loop:
   * block the signal, set the text, then unblock */
  g_signal_handlers_block_by_func (filter_bar->entry, thunar_filter_bar_entry_changed, filter_bar);
  gtk_entry_set_text (GTK_ENTRY (filter_bar->entry), (text != NULL) ? text : "");
  g_signal_handlers_unblock_by_func (filter_bar->entry, thunar_filter_bar_entry_changed, filter_bar);

  g_object_notify (G_OBJECT (filter_bar), "filter-text");
}



/**
 * thunar_filter_bar_focus_entry:
 * @filter_bar : a #ThunarFilterBar.
 *
 * Grabs keyboard focus to the filter entry widget.
 **/
void
thunar_filter_bar_focus_entry (ThunarFilterBar *filter_bar)
{
  _thunar_return_if_fail (THUNAR_IS_FILTER_BAR (filter_bar));

  gtk_widget_grab_focus (filter_bar->entry);
}
