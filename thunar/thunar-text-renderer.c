/* vi:set et ai sw=2 sts=2 ts=2: */
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

#include <exo/exo.h>

#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-marshal.h>
#include <thunar/thunar-pango-extensions.h>
#include <thunar/thunar-text-renderer.h>
#include <thunar/thunar-util.h>



enum
{
  PROP_0,
  PROP_ALIGNMENT,
  PROP_FOLLOW_PRELIT,
  PROP_FOLLOW_STATE,
  PROP_TEXT,
  PROP_WRAP_MODE,
  PROP_WRAP_WIDTH,
  N_PROPERTIES
};

enum
{
  EDITED,
  LAST_SIGNAL,
};



static void             thunar_text_renderer_finalize                         (GObject                 *object);
static void             thunar_text_renderer_get_property                     (GObject                 *object,
                                                                               guint                    prop_id,
                                                                               GValue                  *value,
                                                                               GParamSpec              *pspec);
static void             thunar_text_renderer_set_property                     (GObject                 *object,
                                                                               guint                    prop_id,
                                                                               const GValue            *value,
                                                                               GParamSpec              *pspec);
static void             thunar_text_renderer_get_size                         (GtkCellRenderer         *renderer,
                                                                               GtkWidget               *widget,
                                                                               GdkRectangle            *cell_area,
                                                                               gint                    *x_offset,
                                                                               gint                    *y_offset,
                                                                               gint                    *width,
                                                                               gint                    *height);
static void             thunar_text_renderer_render                           (GtkCellRenderer         *renderer,
                                                                               GdkWindow               *window,
                                                                               GtkWidget               *widget,
                                                                               GdkRectangle            *background_area,
                                                                               GdkRectangle            *cell_area,
                                                                               GdkRectangle            *expose_area,
                                                                               GtkCellRendererState     flags);
static GtkCellEditable *thunar_text_renderer_start_editing                    (GtkCellRenderer         *renderer,
                                                                               GdkEvent                *event,
                                                                               GtkWidget               *widget,
                                                                               const gchar             *path,
                                                                               GdkRectangle            *background_area,
                                                                               GdkRectangle            *cell_area,
                                                                               GtkCellRendererState     flags);
static void             thunar_text_renderer_invalidate                       (ThunarTextRenderer      *text_renderer);
static void             thunar_text_renderer_set_widget                       (ThunarTextRenderer      *text_renderer,
                                                                               GtkWidget               *widget);
static void             thunar_text_renderer_editing_done                     (GtkCellEditable         *editable,
                                                                               ThunarTextRenderer      *text_renderer);
static void             thunar_text_renderer_grab_focus                       (GtkWidget               *entry,
                                                                               ThunarTextRenderer      *text_renderer);
static gboolean         thunar_text_renderer_focus_out_event                  (GtkWidget               *entry,
                                                                               GdkEventFocus           *event,
                                                                               ThunarTextRenderer      *text_renderer);
static void             thunar_text_renderer_populate_popup                   (GtkEntry                *entry,
                                                                               GtkMenu                 *menu,
                                                                               ThunarTextRenderer      *text_renderer);
static void             thunar_text_renderer_popup_unmap                      (GtkMenu                 *menu,
                                                                               ThunarTextRenderer      *text_renderer);
static gboolean         thunar_text_renderer_entry_menu_popdown_timer         (gpointer                 user_data);
static void             thunar_text_renderer_entry_menu_popdown_timer_destroy (gpointer                 user_data);



struct _ThunarTextRendererClass
{
  GtkCellRendererClass __parent__;

  void (*edited) (ThunarTextRenderer *text_renderer,
                  const gchar        *path,
                  const gchar        *text);
};

struct _ThunarTextRenderer
{
  GtkCellRenderer __parent__;

  /* pango renderer properties */
  PangoLayout    *layout;
  GtkWidget      *widget;
  guint           text_static : 1;
  gchar          *text;
  gint            char_width;
  gint            char_height;
  PangoWrapMode   wrap_mode;
  gint            wrap_width;
  guint           follow_state : 1;
  gint            focus_width;;
  PangoAlignment  alignment;

  /* underline prelited rows */
  guint           follow_prelit : 1;

  /* cell editing support */
  GtkWidget      *entry;
  guint           entry_menu_active : 1;
  guint           entry_menu_popdown_timer_id;
};



static guint       text_renderer_signals[LAST_SIGNAL];
static GParamSpec *text_renderer_props[N_PROPERTIES] = { NULL, };



G_DEFINE_TYPE (ThunarTextRenderer, thunar_text_renderer, GTK_TYPE_CELL_RENDERER)



static void
thunar_text_renderer_class_init (ThunarTextRendererClass *klass)
{
  GtkCellRendererClass *gtkcell_renderer_class;
  GObjectClass         *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_text_renderer_finalize;
  gobject_class->get_property = thunar_text_renderer_get_property;
  gobject_class->set_property = thunar_text_renderer_set_property;

  gtkcell_renderer_class = GTK_CELL_RENDERER_CLASS (klass);
  gtkcell_renderer_class->get_size = thunar_text_renderer_get_size;
  gtkcell_renderer_class->render = thunar_text_renderer_render;
  gtkcell_renderer_class->start_editing = thunar_text_renderer_start_editing;

  /**
   * ThunarTextRenderer:alignment:
   *
   * Specifies how to align the lines of text with respect to each other.
   **/
  text_renderer_props[PROP_ALIGNMENT] =
      g_param_spec_enum ("alignment",
                         "alignment",
                         "alignment",
                         PANGO_TYPE_ALIGNMENT,
                         PANGO_ALIGN_LEFT,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarTextRenderer:follow-prelit:
   *
   * Whether to underline prelited cells. This is used for the single
   * click support in the detailed list view.
   **/
  text_renderer_props[PROP_FOLLOW_PRELIT] =
      g_param_spec_boolean ("follow-prelit",
                            "follow-prelit",
                            "follow-prelit",
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarTextRenderer:follow-state:
   *
   * Specifies whether the text renderer should render text
   * based on the selection state of the items. This is necessary
   * for #ExoIconView, which doesn't draw any item state indicators
   * itself.
   **/
  text_renderer_props[PROP_FOLLOW_STATE] =
      g_param_spec_boolean ("follow-state",
                            "follow-state",
                            "follow-state",
                            FALSE,
                            EXO_PARAM_READWRITE);

  /**
   * ThunarTextRenderer:text:
   *
   * The text to render.
   **/
  text_renderer_props[PROP_TEXT] =
      g_param_spec_string ("text",
                           "text",
                           "text",
                           NULL,
                           EXO_PARAM_READWRITE);

  /**
   * ThunarTextRenderer:wrap-mode:
   *
   * Specifies how to break the string into multiple lines, if the cell renderer
   * does not have enough room to display the entire string. This property has
   * no effect unless the wrap-width property is set.
   **/
  text_renderer_props[PROP_WRAP_MODE] =
      g_param_spec_enum ("wrap-mode",
                         "wrap-mode",
                         "wrap-mode",
                         PANGO_TYPE_WRAP_MODE,
                         PANGO_WRAP_CHAR,
                         EXO_PARAM_READWRITE);

  /**
   * ThunarTextRenderer:wrap-width:
   *
   * Specifies the width at which the text is wrapped. The wrap-mode property can
   * be used to influence at what character positions the line breaks can be placed.
   * Setting wrap-width to -1 turns wrapping off.
   **/
  text_renderer_props[PROP_WRAP_WIDTH] =
      g_param_spec_int ("wrap-width",
                        "wrap-width",
                        "wrap-width",
                        -1, G_MAXINT, -1,
                        EXO_PARAM_READWRITE);

  /* install properties */
  g_object_class_install_properties (gobject_class, N_PROPERTIES, text_renderer_props);

  /**
   * ThunarTextRenderer::edited:
   * @text_renderer : a #ThunarTextRenderer.
   * @path          : the string representation of the tree path, which was edited.
   * @text          : the new text for the cell.
   * @user_data     : user data set when the signal handler was connected.
   *
   * Emitted whenever the user successfully edits a cell.
   **/
  text_renderer_signals[EDITED] =
    g_signal_new (I_("edited"),
                  G_OBJECT_CLASS_TYPE (gobject_class),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarTextRendererClass, edited),
                  NULL, NULL,
                  _thunar_marshal_VOID__STRING_STRING,
                  G_TYPE_NONE, 2, G_TYPE_STRING, G_TYPE_STRING);
}



static void
thunar_text_renderer_init (ThunarTextRenderer *text_renderer)
{
  text_renderer->wrap_width = -1;
  text_renderer->alignment = PANGO_ALIGN_LEFT;
}



static void
thunar_text_renderer_finalize (GObject *object)
{
  ThunarTextRenderer *text_renderer = THUNAR_TEXT_RENDERER (object);

  /* release text (if not static) */
  if (!text_renderer->text_static)
    g_free (text_renderer->text);

  /* drop the cached widget */
  thunar_text_renderer_set_widget (text_renderer, NULL);

  (*G_OBJECT_CLASS (thunar_text_renderer_parent_class)->finalize) (object);
}



static void
thunar_text_renderer_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarTextRenderer *text_renderer = THUNAR_TEXT_RENDERER (object);

  switch (prop_id)
    {
    case PROP_ALIGNMENT:
      g_value_set_enum (value, text_renderer->alignment);
      break;

    case PROP_FOLLOW_PRELIT:
      g_value_set_boolean (value, text_renderer->follow_prelit);
      break;

    case PROP_FOLLOW_STATE:
      g_value_set_boolean (value, text_renderer->follow_state);
      break;

    case PROP_TEXT:
      g_value_set_string (value, text_renderer->text);
      break;

    case PROP_WRAP_MODE:
      g_value_set_enum (value, text_renderer->wrap_mode);
      break;

    case PROP_WRAP_WIDTH:
      g_value_set_int (value, text_renderer->wrap_width);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_text_renderer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ThunarTextRenderer *text_renderer = THUNAR_TEXT_RENDERER (object);
  const gchar        *sval;

  switch (prop_id)
    {
    case PROP_ALIGNMENT:
      text_renderer->alignment = g_value_get_enum (value);
      break;

    case PROP_FOLLOW_PRELIT:
      text_renderer->follow_prelit = g_value_get_boolean (value);
      break;

    case PROP_FOLLOW_STATE:
      text_renderer->follow_state = g_value_get_boolean (value);
      break;

    case PROP_TEXT:
      /* release the previous text (if not static) */
      if (!text_renderer->text_static)
        g_free (text_renderer->text);
      sval = g_value_get_string (value);
      text_renderer->text_static = (value->data[1].v_uint & G_VALUE_NOCOPY_CONTENTS);
      text_renderer->text = (sval == NULL) ? "" : (gchar *)sval;
      if (!text_renderer->text_static)
        text_renderer->text = g_strdup (text_renderer->text);
      break;

    case PROP_WRAP_MODE:
      text_renderer->wrap_mode = g_value_get_enum (value);
      break;

    case PROP_WRAP_WIDTH:
      /* be sure to reset fixed height if wrapping is requested */
      text_renderer->wrap_width = g_value_get_int (value);
      if (G_LIKELY (text_renderer->wrap_width >= 0))
        gtk_cell_renderer_set_fixed_size (GTK_CELL_RENDERER (text_renderer), -1, -1);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_text_renderer_get_size (GtkCellRenderer *renderer,
                               GtkWidget       *widget,
                               GdkRectangle    *cell_area,
                               gint            *x_offset,
                               gint            *y_offset,
                               gint            *width,
                               gint            *height)
{
  ThunarTextRenderer *text_renderer = THUNAR_TEXT_RENDERER (renderer);
  gint                text_length;
  gint                text_width;
  gint                text_height;

  /* setup the new widget */
  thunar_text_renderer_set_widget (text_renderer, widget);

  /* we can guess the dimensions if we don't wrap */
  if (text_renderer->wrap_width < 0)
    {
      /* determine the text_length in characters */
      text_length = g_utf8_strlen (text_renderer->text, -1);

      /* the approximation is usually 1-2 chars wrong, so wth */
      text_length += 2;

      /* calculate the appromixate text width/height */
      text_width = text_renderer->char_width * text_length;
      text_height = text_renderer->char_height;
    }
  else
    {
      /* calculate the real text dimension */
      pango_layout_set_width (text_renderer->layout, text_renderer->wrap_width * PANGO_SCALE);
      pango_layout_set_wrap (text_renderer->layout, text_renderer->wrap_mode);
      pango_layout_set_text (text_renderer->layout, text_renderer->text, -1);
      pango_layout_get_pixel_size (text_renderer->layout, &text_width, &text_height);
    }

  /* if we have to follow the state manually, we'll need
   * to reserve some space to render the indicator to.
   */
  if (text_renderer->follow_state)
    {
      text_width += 2 * text_renderer->focus_width;
      text_height += 2 * text_renderer->focus_width;
    }

  /* update width/height */
  if (G_LIKELY (width != NULL))
    *width = text_width + 2 * renderer->xpad;
  if (G_LIKELY (height != NULL))
    *height = text_height + 2 * renderer->ypad;

  /* update the x/y offsets */
  if (G_LIKELY (cell_area != NULL))
    {
      if (G_LIKELY (x_offset != NULL))
        {
          *x_offset = ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ? (1.0 - renderer->xalign) : renderer->xalign)
                    * (cell_area->width - text_width - (2 * renderer->xpad));
          *x_offset = MAX (*x_offset, 0);
        }

      if (G_LIKELY (y_offset != NULL))
        {
          *y_offset = renderer->yalign * (cell_area->height - text_height - (2 * renderer->ypad));
          *y_offset = MAX (*y_offset, 0);
        }
    }
}



static void
thunar_text_renderer_render (GtkCellRenderer     *renderer,
                             GdkWindow           *window,
                             GtkWidget           *widget,
                             GdkRectangle        *background_area,
                             GdkRectangle        *cell_area,
                             GdkRectangle        *expose_area,
                             GtkCellRendererState flags)
{
  ThunarTextRenderer *text_renderer = THUNAR_TEXT_RENDERER (renderer);
  GtkStateType        state;
  cairo_t            *cr;
  gint                x0, x1, y0, y1;
  gint                x_offset;
  gint                y_offset;
  PangoRectangle      rect;

  /* setup the new widget */
  thunar_text_renderer_set_widget (text_renderer, widget);

  if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED)
    {
      if (gtk_widget_has_focus (widget))
        state = GTK_STATE_SELECTED;
      else
        state = GTK_STATE_ACTIVE;
    }
  else if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT
        && gtk_widget_get_state (widget) == GTK_STATE_PRELIGHT)
    {
      state = GTK_STATE_PRELIGHT;
    }
  else
    {
      if (gtk_widget_get_state (widget) == GTK_STATE_INSENSITIVE)
        state = GTK_STATE_INSENSITIVE;
      else
        state = GTK_STATE_NORMAL;
    }

  /* check if we should follow the prelit state (used for single click support) */
  if (text_renderer->follow_prelit && (flags & GTK_CELL_RENDERER_PRELIT) != 0)
    pango_layout_set_attributes (text_renderer->layout, thunar_pango_attr_list_underline_single ());
  else
    pango_layout_set_attributes (text_renderer->layout, NULL);

  /* setup the wrapping */
  if (text_renderer->wrap_width < 0)
    {
      pango_layout_set_width (text_renderer->layout, -1);
      pango_layout_set_wrap (text_renderer->layout, PANGO_WRAP_CHAR);
    }
  else
    {
      pango_layout_set_width (text_renderer->layout, text_renderer->wrap_width * PANGO_SCALE);
      pango_layout_set_wrap (text_renderer->layout, text_renderer->wrap_mode);
    }

  pango_layout_set_text (text_renderer->layout, text_renderer->text, -1);
  pango_layout_set_alignment (text_renderer->layout, text_renderer->alignment);

  /* calculate the real text dimension */
  pango_layout_get_pixel_extents (text_renderer->layout, NULL, &rect);

  /* take into account the state indicator (required for calculation) */
  if (text_renderer->follow_state)
    {
      rect.width += 2 * text_renderer->focus_width;
      rect.height += 2 * text_renderer->focus_width;
    }

  /* calculate the real x-offset */
  x_offset = ((gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL) ? (1.0 - renderer->xalign) : renderer->xalign)
           * (cell_area->width - rect.width - (2 * renderer->xpad));
  x_offset = MAX (x_offset, 0);

  /* calculate the real y-offset */
  y_offset = renderer->yalign * (cell_area->height - rect.height - (2 * renderer->ypad));
  y_offset = MAX (y_offset, 0);

  /* render the state indicator */
  if ((flags & GTK_CELL_RENDERER_SELECTED) == GTK_CELL_RENDERER_SELECTED && text_renderer->follow_state)
    {
      /* calculate the text bounding box (including the focus padding/width) */
      x0 = cell_area->x + x_offset;
      y0 = cell_area->y + y_offset;
      x1 = x0 + rect.width;
      y1 = y0 + rect.height;

      /* Cairo produces nicer results than using a polygon
       * and so we use it directly if possible.
       */
      cr = gdk_cairo_create (window);
      cairo_move_to (cr, x0 + 5, y0);
      cairo_line_to (cr, x1 - 5, y0);
      cairo_curve_to (cr, x1 - 5, y0, x1, y0, x1, y0 + 5);
      cairo_line_to (cr, x1, y1 - 5);
      cairo_curve_to (cr, x1, y1 - 5, x1, y1, x1 - 5, y1);
      cairo_line_to (cr, x0 + 5, y1);
      cairo_curve_to (cr, x0 + 5, y1, x0, y1, x0, y1 - 5);
      cairo_line_to (cr, x0, y0 + 5);
      cairo_curve_to (cr, x0, y0 + 5, x0, y0, x0 + 5, y0);
      gdk_cairo_set_source_color (cr, &widget->style->base[state]);
      cairo_fill (cr);
      cairo_destroy (cr);
    }

  /* draw the focus indicator */
  if (text_renderer->follow_state && (flags & GTK_CELL_RENDERER_FOCUSED) != 0)
    {
      gtk_paint_focus (widget->style, window, gtk_widget_get_state (widget), NULL, widget, "icon_view",
                       cell_area->x + x_offset, cell_area->y + y_offset, rect.width, rect.height);
    }

  /* get proper sizing for the layout drawing */
  if (text_renderer->follow_state)
    {
      rect.width -= 2 * text_renderer->focus_width;
      rect.height -= 2 * text_renderer->focus_width;
      x_offset += text_renderer->focus_width;
      y_offset += text_renderer->focus_width;
    }

  /* draw the text */
  gtk_paint_layout (widget->style, window, state, TRUE,
                    expose_area, widget, "cellrenderertext",
                    cell_area->x + x_offset + renderer->xpad - rect.x,
                    cell_area->y + y_offset + renderer->ypad - rect.y,
                    text_renderer->layout);
}



static GtkCellEditable*
thunar_text_renderer_start_editing (GtkCellRenderer     *renderer,
                                    GdkEvent            *event,
                                    GtkWidget           *widget,
                                    const gchar         *path,
                                    GdkRectangle        *background_area,
                                    GdkRectangle        *cell_area,
                                    GtkCellRendererState flags)
{
  ThunarTextRenderer *text_renderer = THUNAR_TEXT_RENDERER (renderer);

  /* verify that we are editable */
  if (renderer->mode != GTK_CELL_RENDERER_MODE_EDITABLE)
    return NULL;

  /* allocate a new text entry widget to be used for editing */
  text_renderer->entry = g_object_new (GTK_TYPE_ENTRY,
                                       "has-frame", FALSE,
                                       "text", text_renderer->text,
                                       "visible", TRUE,
                                       "xalign", renderer->xalign,
                                       NULL);

  /* select the whole text */
  gtk_editable_select_region (GTK_EDITABLE (text_renderer->entry), 0, -1);

  /* remember the tree path that we're editing */
  g_object_set_data_full (G_OBJECT (text_renderer->entry), I_("thunar-text-renderer-path"), g_strdup (path), g_free);

  /* connect required signals */
  g_signal_connect (G_OBJECT (text_renderer->entry), "editing-done", G_CALLBACK (thunar_text_renderer_editing_done), text_renderer);
  g_signal_connect_after (G_OBJECT (text_renderer->entry), "grab-focus", G_CALLBACK (thunar_text_renderer_grab_focus), text_renderer);
  g_signal_connect (G_OBJECT (text_renderer->entry), "focus-out-event", G_CALLBACK (thunar_text_renderer_focus_out_event), text_renderer);
  g_signal_connect (G_OBJECT (text_renderer->entry), "populate-popup", G_CALLBACK (thunar_text_renderer_populate_popup), text_renderer);

  return GTK_CELL_EDITABLE (text_renderer->entry);
}



static void
thunar_text_renderer_invalidate (ThunarTextRenderer *text_renderer)
{
  thunar_text_renderer_set_widget (text_renderer, NULL);
}



static void
thunar_text_renderer_set_widget (ThunarTextRenderer *text_renderer,
                                 GtkWidget          *widget)
{
  PangoFontMetrics *metrics;
  PangoContext     *context;
  gint              focus_padding;
  gint              focus_line_width;

  if (G_LIKELY (widget == text_renderer->widget))
    return;

  /* disconnect from the previously set widget */
  if (G_UNLIKELY (text_renderer->widget != NULL))
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (text_renderer->widget), thunar_text_renderer_invalidate, text_renderer);
      g_object_unref (G_OBJECT (text_renderer->layout));
      g_object_unref (G_OBJECT (text_renderer->widget));
    }

  /* activate the new widget */
  text_renderer->widget = widget;

  /* connect to the new widget */
  if (G_LIKELY (widget != NULL))
    {
      /* take a reference on the widget */
      g_object_ref (G_OBJECT (widget));

      /* we need to recalculate the metrics when a new style (and thereby a new font) is set */
      g_signal_connect_swapped (G_OBJECT (text_renderer->widget), "destroy", G_CALLBACK (thunar_text_renderer_invalidate), text_renderer);
      g_signal_connect_swapped (G_OBJECT (text_renderer->widget), "style-set", G_CALLBACK (thunar_text_renderer_invalidate), text_renderer);

      /* allocate a new pango layout for this widget */
      context = gtk_widget_get_pango_context (widget);
      text_renderer->layout = pango_layout_new (context);

      /* disable automatic text direction, but use the direction specified by Gtk+ */
      pango_layout_set_auto_dir (text_renderer->layout, FALSE);

      /* we don't want to interpret line separators in file names */
      pango_layout_set_single_paragraph_mode (text_renderer->layout, TRUE);

      /* calculate the average character dimensions */
      metrics = pango_context_get_metrics (context, widget->style->font_desc, pango_context_get_language (context));
      text_renderer->char_width = PANGO_PIXELS (pango_font_metrics_get_approximate_char_width (metrics));
      text_renderer->char_height = PANGO_PIXELS (pango_font_metrics_get_ascent (metrics) + pango_font_metrics_get_descent (metrics));
      pango_font_metrics_unref (metrics);

      /* tell the cell renderer about the fixed height if we're not wrapping text */
      if (G_LIKELY (text_renderer->wrap_width < 0))
        gtk_cell_renderer_set_fixed_size (GTK_CELL_RENDERER (text_renderer), -1, text_renderer->char_height);

      /* determine the focus-padding and focus-line-width style properties from the widget */
      gtk_widget_style_get (widget, "focus-padding", &focus_padding, "focus-line-width", &focus_line_width, NULL);
      text_renderer->focus_width = focus_padding + focus_line_width;
    }
  else
    {
      text_renderer->layout = NULL;
      text_renderer->char_width = 0;
      text_renderer->char_height = 0;
    }
}



static void
thunar_text_renderer_editing_done (GtkCellEditable    *editable,
                                   ThunarTextRenderer *text_renderer)
{
  const gchar *path;
  const gchar *text;

  /* disconnect our signals from the cell editable */
  g_signal_handlers_disconnect_by_func (G_OBJECT (editable), thunar_text_renderer_editing_done, text_renderer);
  g_signal_handlers_disconnect_by_func (G_OBJECT (editable), thunar_text_renderer_focus_out_event, text_renderer);
  g_signal_handlers_disconnect_by_func (G_OBJECT (editable), thunar_text_renderer_populate_popup, text_renderer);

  /* let the GtkCellRenderer class do it's part of the job */
  gtk_cell_renderer_stop_editing (GTK_CELL_RENDERER (text_renderer), GTK_ENTRY (editable)->editing_canceled);

  /* inform whoever is interested that we have new text (if not cancelled) */
  if (G_LIKELY (!GTK_ENTRY (editable)->editing_canceled))
    {
      text = gtk_entry_get_text (GTK_ENTRY (editable));
      path = g_object_get_data (G_OBJECT (editable), "thunar-text-renderer-path");
      g_signal_emit (G_OBJECT (text_renderer), text_renderer_signals[EDITED], 0, path, text);
    }
}



static void
thunar_text_renderer_grab_focus (GtkWidget          *entry,
                                 ThunarTextRenderer *text_renderer)
{
  const gchar *text;
  const gchar *dot;
  glong        offset;

  /* determine the text from the entry widget */
  text = gtk_entry_get_text (GTK_ENTRY (entry));

  /* lookup the last dot in the text */
  dot = thunar_util_str_get_extension (text);
  if (G_LIKELY (dot != NULL))
    {
      /* determine the UTF-8 char offset */
      offset = g_utf8_pointer_to_offset (text, dot);

      /* select the text prior to the dot */
      if (G_LIKELY (offset > 0))
        gtk_editable_select_region (GTK_EDITABLE (entry), 0, offset);
    }

  /* disconnect the grab-focus handler, so we change the selection only once */
  g_signal_handlers_disconnect_by_func (G_OBJECT (entry), thunar_text_renderer_grab_focus, text_renderer);
}



static gboolean
thunar_text_renderer_focus_out_event (GtkWidget          *entry,
                                      GdkEventFocus      *event,
                                      ThunarTextRenderer *text_renderer)
{
  /* cancel editing if we haven't popped up the menu */
  if (G_LIKELY (!text_renderer->entry_menu_active))
    thunar_text_renderer_editing_done (GTK_CELL_EDITABLE (entry), text_renderer);

  /* we need to pass the event to the entry */
  return FALSE;
}



static void
thunar_text_renderer_populate_popup (GtkEntry           *entry,
                                     GtkMenu            *menu,
                                     ThunarTextRenderer *text_renderer)
{
  if (G_UNLIKELY (text_renderer->entry_menu_popdown_timer_id != 0))
    g_source_remove (text_renderer->entry_menu_popdown_timer_id);

  text_renderer->entry_menu_active = TRUE;

  g_signal_connect (G_OBJECT (menu), "unmap", G_CALLBACK (thunar_text_renderer_popup_unmap), text_renderer);
}



static void
thunar_text_renderer_popup_unmap (GtkMenu            *menu,
                                  ThunarTextRenderer *text_renderer)
{
  text_renderer->entry_menu_active = FALSE;

  if (G_LIKELY (text_renderer->entry_menu_popdown_timer_id == 0))
    {
      text_renderer->entry_menu_popdown_timer_id = g_timeout_add_full (G_PRIORITY_LOW, 500u, thunar_text_renderer_entry_menu_popdown_timer,
                                                                       text_renderer, thunar_text_renderer_entry_menu_popdown_timer_destroy);
    }
}



static gboolean
thunar_text_renderer_entry_menu_popdown_timer (gpointer user_data)
{
  ThunarTextRenderer *text_renderer = THUNAR_TEXT_RENDERER (user_data);

  GDK_THREADS_ENTER ();

  /* check if we still have the keyboard focus */
  if (G_UNLIKELY (!gtk_widget_has_focus (text_renderer->entry)))
    thunar_text_renderer_editing_done (GTK_CELL_EDITABLE (text_renderer->entry), text_renderer);

  GDK_THREADS_LEAVE ();

  return FALSE;
}



static void
thunar_text_renderer_entry_menu_popdown_timer_destroy (gpointer user_data)
{
  THUNAR_TEXT_RENDERER (user_data)->entry_menu_popdown_timer_id = 0;
}



/**
 * thunar_text_renderer_new:
 **/
GtkCellRenderer*
thunar_text_renderer_new (void)
{
  return g_object_new (THUNAR_TYPE_TEXT_RENDERER, NULL);
}

