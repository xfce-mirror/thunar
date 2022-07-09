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

#include <thunar/thunar-gobject-extensions.h>
#include <thunar/thunar-gdk-extensions.h>
#include <thunar/thunar-text-renderer.h>



enum
{
  PROP_0,

  PROP_TEXT,
  PROP_MARKUP,
  PROP_ATTRIBUTES,
  PROP_SINGLE_PARAGRAPH_MODE,
  PROP_WIDTH_CHARS,
  PROP_MAX_WIDTH_CHARS,
  PROP_WRAP_WIDTH,
  PROP_ALIGN,
  PROP_PLACEHOLDER_TEXT,

  /* Style args */
  PROP_BACKGROUND,
  PROP_FOREGROUND,
  PROP_BACKGROUND_GDK,
  PROP_FOREGROUND_GDK,
  PROP_BACKGROUND_RGBA,
  PROP_FOREGROUND_RGBA,
  PROP_FONT,
  PROP_FONT_DESC,
  PROP_FAMILY,
  PROP_STYLE,
  PROP_VARIANT,
  PROP_WEIGHT,
  PROP_STRETCH,
  PROP_SIZE,
  PROP_SIZE_POINTS,
  PROP_SCALE,
  PROP_EDITABLE,
  PROP_STRIKETHROUGH,
  PROP_UNDERLINE,
  PROP_RISE,
  PROP_LANGUAGE,
  PROP_ELLIPSIZE,
  PROP_WRAP_MODE,
  PROP_HIGHLIGHT,
  PROP_BORDER_RADIUS,
  PROP_BORDER_RADIUS_SET,
  
  /* Whether-a-style-arg-is-set args */
  PROP_BACKGROUND_SET,
  PROP_FOREGROUND_SET,
  PROP_FAMILY_SET,
  PROP_STYLE_SET,
  PROP_VARIANT_SET,
  PROP_WEIGHT_SET,
  PROP_STRETCH_SET,
  PROP_SIZE_SET,
  PROP_SCALE_SET,
  PROP_EDITABLE_SET,
  PROP_STRIKETHROUGH_SET,
  PROP_UNDERLINE_SET,
  PROP_RISE_SET,
  PROP_LANGUAGE_SET,
  PROP_ELLIPSIZE_SET,
  PROP_ALIGN_SET,
  PROP_HIGHLIGHT_SET,
};



static void thunar_text_renderer_finalize                        (GObject               *object);
static void thunar_text_renderer_get_property                    (GObject               *object,
                                                                  guint                  prop_id,
                                                                  GValue                *value,
                                                                  GParamSpec            *pspec);
static void thunar_text_renderer_set_property                    (GObject               *object,
                                                                  guint                  prop_id,
                                                                  const GValue          *value,
                                                                  GParamSpec            *pspec);
static void thunar_text_renderer_get_preferred_width             (GtkCellRenderer       *renderer,
                                                                  GtkWidget             *widget,
                                                                  gint                  *minimum,
                                                                  gint                  *natural);
static void thunar_text_renderer_get_preferred_height            (GtkCellRenderer       *renderer,
                                                                  GtkWidget             *widget,
                                                                  gint                  *minimum,
                                                                  gint                  *natural);
static void thunar_text_renderer_get_preferred_height_for_width  (GtkCellRenderer       *cell,
                                                                  GtkWidget             *widget,
                                                                  gint                   width,
                                                                  gint                  *minimum_height,
                                                                  gint                  *natural_height);
static void thunar_text_renderer_render                          (GtkCellRenderer       *renderer,
                                                                  cairo_t               *cr,
                                                                  GtkWidget             *widget,
                                                                  const GdkRectangle    *background_area,
                                                                  const GdkRectangle    *cell_area,
                                                                  GtkCellRendererState   flags);



struct _ThunarTextRendererClass
{
  GtkCellRendererClass __parent__;
};

struct _ThunarTextRenderer
{
  GtkCellRenderer __parent__;
  GtkWidget *entry;

  PangoAttrList        *extra_attrs;
  GdkRGBA               foreground;
  GdkRGBA               background;
  gchar                *highlight;
  gchar                *border_radius;
  PangoAlignment        align;
  PangoEllipsizeMode    ellipsize;
  PangoFontDescription *font;
  PangoLanguage        *language;
  PangoUnderline        underline_style;
  PangoWrapMode         wrap_mode;

  gchar *text;
  gchar *placeholder_text;

  gdouble font_scale;

  gint rise;
  gint fixed_height_rows;
  gint width_chars;
  gint max_width_chars;
  gint wrap_width;

  guint in_entry_menu     : 1;
  guint strikethrough     : 1;
  guint editable          : 1;
  guint scale_set         : 1;
  guint foreground_set    : 1;
  guint background_set    : 1;
  guint highlight_set     : 1;
  guint underline_set     : 1;
  guint rise_set          : 1;
  guint strikethrough_set : 1;
  guint editable_set      : 1;
  guint calc_fixed_height : 1;
  guint single_paragraph  : 1;
  guint language_set      : 1;
  guint markup_set        : 1;
  guint ellipsize_set     : 1;
  guint align_set         : 1;
  guint border_radius_set : 1;
};



G_DEFINE_TYPE (ThunarTextRenderer, thunar_text_renderer, GTK_TYPE_CELL_RENDERER);



static void
thunar_text_renderer_class_init (ThunarTextRendererClass *klass)
{
  GObjectClass *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *cell_class = GTK_CELL_RENDERER_CLASS (klass);

  object_class->finalize = thunar_text_renderer_finalize;
  
  object_class->get_property = thunar_text_renderer_get_property;
  object_class->set_property = thunar_text_renderer_set_property;

  cell_class->render = thunar_text_renderer_render;
  cell_class->get_preferred_width = thunar_text_renderer_get_preferred_width;
  cell_class->get_preferred_height = thunar_text_renderer_get_preferred_height;
  cell_class->get_preferred_height_for_width = thunar_text_renderer_get_preferred_height_for_width;

  g_object_class_install_property (object_class,
                                   PROP_TEXT,
                                   g_param_spec_string ("text", "Text", "Text",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));
  
  g_object_class_install_property (object_class,
                                   PROP_MARKUP,
                                   g_param_spec_string ("markup", "Markup", "Mark",
                                                        NULL,
                                                        EXO_PARAM_WRITABLE));

  g_object_class_install_property (object_class,
				   PROP_ATTRIBUTES,
				   g_param_spec_boxed ("attributes",
						       ("Attributes"),
						       ("A list of style attributes to apply to the text of the renderer"),
						       PANGO_TYPE_ATTR_LIST,
						       EXO_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SINGLE_PARAGRAPH_MODE,
                                   g_param_spec_boolean ("single-paragraph-mode",
                                                         ("Single Paragraph Mode"),
                                                         ("Whether to keep all text in a single paragraph"),
                                                         FALSE,
                                                         EXO_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  
  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND,
                                   g_param_spec_string ("background",
                                                        ("Background color name"),
                                                        ("Background color as a string"),
                                                        NULL,
                                                        EXO_PARAM_WRITABLE));

  /**
   * GtkCellRendererText:background-gdk:
   *
   * Background color as a #GdkColor
   *
   * Deprecated: 3.4: Use #GtkCellRendererText:background-rgba instead.
   */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND_GDK,
                                   g_param_spec_boxed ("background-gdk",
                                                       ("Background color"),
                                                       ("Background color as a GdkColor"),
                                                       GDK_TYPE_COLOR,
                                                       EXO_PARAM_READWRITE | G_PARAM_DEPRECATED));
G_GNUC_END_IGNORE_DEPRECATIONS

  /**
   * GtkCellRendererText:background-rgba:
   *
   * Background color as a #GdkRGBA
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_BACKGROUND_RGBA,
                                   g_param_spec_boxed ("background-rgba",
                                                       ("Background color as RGBA"),
                                                       ("Background color as a GdkRGBA"),
                                                       GDK_TYPE_RGBA,
                                                       EXO_PARAM_READWRITE));
  g_object_class_install_property (object_class,
                                   PROP_FOREGROUND,
                                   g_param_spec_string ("foreground",
                                                        ("Foreground color name"),
                                                        ("Foreground color as a string"),
                                                        NULL,
                                                        EXO_PARAM_WRITABLE));

  /**
   * GtkCellRendererText:foreground-gdk:
   *
   * Foreground color as a #GdkColor
   *
   * Deprecated: 3.4: Use #GtkCellRendererText:foreground-rgba instead.
   */
G_GNUC_BEGIN_IGNORE_DEPRECATIONS
  g_object_class_install_property (object_class,
                                   PROP_FOREGROUND_GDK,
                                   g_param_spec_boxed ("foreground-gdk",
                                                       ("Foreground color"),
                                                       ("Foreground color as a GdkColor"),
                                                       GDK_TYPE_COLOR,
                                                       EXO_PARAM_READWRITE | G_PARAM_DEPRECATED));
G_GNUC_END_IGNORE_DEPRECATIONS

  /**
   * GtkCellRendererText:foreground-rgba:
   *
   * Foreground color as a #GdkRGBA
   *
   * Since: 3.0
   */
  g_object_class_install_property (object_class,
                                   PROP_FOREGROUND_RGBA,
                                   g_param_spec_boxed ("foreground-rgba",
                                                       ("Foreground color as RGBA"),
                                                       ("Foreground color as a GdkRGBA"),
                                                       GDK_TYPE_RGBA,
                                                       EXO_PARAM_READWRITE));


  g_object_class_install_property (object_class,
                                   PROP_EDITABLE,
                                   g_param_spec_boolean ("editable",
                                                         ("Editable"),
                                                         ("Whether the text can be modified by the user"),
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_FONT,
                                   g_param_spec_string ("font",
                                                        ("Font"),
                                                        ("Font description as a string, e.g. \"Sans Italic 12\""),
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_FONT_DESC,
                                   g_param_spec_boxed ("font-desc",
                                                       ("Font"),
                                                       ("Font description as a PangoFontDescription struct"),
                                                       PANGO_TYPE_FONT_DESCRIPTION,
                                                       EXO_PARAM_READWRITE));

  
  g_object_class_install_property (object_class,
                                   PROP_FAMILY,
                                   g_param_spec_string ("family",
                                                        ("Font family"),
                                                        ("Name of the font family, e.g. Sans, Helvetica, Times, Monospace"),
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_STYLE,
                                   g_param_spec_enum ("style",
                                                      ("Font style"),
                                                      ("Font style"),
                                                      PANGO_TYPE_STYLE,
                                                      PANGO_STYLE_NORMAL,
                                                      EXO_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_VARIANT,
                                   g_param_spec_enum ("variant",
                                                     ("Font variant"),
                                                     ("Font variant"),
                                                      PANGO_TYPE_VARIANT,
                                                      PANGO_VARIANT_NORMAL,
                                                      EXO_PARAM_READWRITE));
  
  g_object_class_install_property (object_class,
                                   PROP_WEIGHT,
                                   g_param_spec_int ("weight",
                                                     ("Font weight"),
                                                     ("Font weight"),
                                                     0,
                                                     G_MAXINT,
                                                     PANGO_WEIGHT_NORMAL,
                                                     EXO_PARAM_READWRITE));

   g_object_class_install_property (object_class,
                                   PROP_STRETCH,
                                   g_param_spec_enum ("stretch",
                                                      ("Font stretch"),
                                                      ("Font stretch"),
                                                      PANGO_TYPE_STRETCH,
                                                      PANGO_STRETCH_NORMAL,
                                                      EXO_PARAM_READWRITE));
  
  g_object_class_install_property (object_class,
                                   PROP_SIZE,
                                   g_param_spec_int ("size",
                                                     ("Font size"),
                                                     ("Font size"),
                                                     0,
                                                     G_MAXINT,
                                                     0,
                                                     EXO_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_SIZE_POINTS,
                                   g_param_spec_double ("size-points",
                                                        ("Font points"),
                                                        ("Font size in points"),
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        0.0,
                                                        EXO_PARAM_READWRITE));  

  g_object_class_install_property (object_class,
                                   PROP_SCALE,
                                   g_param_spec_double ("scale",
                                                        ("Font scale"),
                                                        ("Font scaling factor"),
                                                        0.0,
                                                        G_MAXDOUBLE,
                                                        1.0,
                                                        EXO_PARAM_READWRITE));
  
  g_object_class_install_property (object_class,
                                   PROP_RISE,
                                   g_param_spec_int ("rise",
                                                     ("Rise"),
                                                     ("Offset of text above the baseline "
							"(below the baseline if rise is negative)"),
                                                     -G_MAXINT,
                                                     G_MAXINT,
                                                     0,
                                                     EXO_PARAM_READWRITE));


  g_object_class_install_property (object_class,
                                   PROP_STRIKETHROUGH,
                                   g_param_spec_boolean ("strikethrough",
                                                         ("Strikethrough"),
                                                         ("Whether to strike through the text"),
                                                         FALSE,
                                                         EXO_PARAM_READWRITE));
  
  g_object_class_install_property (object_class,
                                   PROP_UNDERLINE,
                                   g_param_spec_enum ("underline",
                                                      ("Underline"),
                                                      ("Style of underline for this text"),
                                                      PANGO_TYPE_UNDERLINE,
                                                      PANGO_UNDERLINE_NONE,
                                                      EXO_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_LANGUAGE,
                                   g_param_spec_string ("language",
                                                        ("Language"),
                                                        ("The language this text is in, as an ISO code. "
							   "Pango can use this as a hint when rendering the text. "
							   "If you don't understand this parameter, you probably don't need it"),
                                                        NULL,
                                                        EXO_PARAM_READWRITE));


  /**
   * GtkCellRendererText:ellipsize:
   *
   * Specifies the preferred place to ellipsize the string, if the cell renderer 
   * does not have enough room to display the entire string. Setting it to 
   * %PANGO_ELLIPSIZE_NONE turns off ellipsizing. See the wrap-width property
   * for another way of making the text fit into a given width.
   *
   * Since: 2.6
   */
  g_object_class_install_property (object_class,
                                   PROP_ELLIPSIZE,
                                   g_param_spec_enum ("ellipsize",
						      ("Ellipsize"),
						      ("The preferred place to ellipsize the string, "
							 "if the cell renderer does not have enough room "
							 "to display the entire string"),
						      PANGO_TYPE_ELLIPSIZE_MODE,
						      PANGO_ELLIPSIZE_NONE,
						      EXO_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellRendererText:width-chars:
   * 
   * The desired width of the cell, in characters. If this property is set to
   * -1, the width will be calculated automatically, otherwise the cell will
   * request either 3 characters or the property value, whichever is greater.
   * 
   * Since: 2.6
   **/
  g_object_class_install_property (object_class,
                                   PROP_WIDTH_CHARS,
                                   g_param_spec_int ("width-chars",
                                                     ("Width In Characters"),
                                                     ("The desired width of the label, in characters"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     EXO_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  

  /**
   * GtkCellRendererText:max-width-chars:
   * 
   * The desired maximum width of the cell, in characters. If this property 
   * is set to -1, the width will be calculated automatically.
   *
   * For cell renderers that ellipsize or wrap text; this property
   * controls the maximum reported width of the cell. The
   * cell should not receive any greater allocation unless it is
   * set to expand in its #GtkCellLayout and all of the cell's siblings
   * have received their natural width.
   *
   * Since: 3.0
   **/
  g_object_class_install_property (object_class,
                                   PROP_MAX_WIDTH_CHARS,
                                   g_param_spec_int ("max-width-chars",
                                                     ("Maximum Width In Characters"),
                                                     ("The maximum width of the cell, in characters"),
                                                     -1,
                                                     G_MAXINT,
                                                     -1,
                                                     EXO_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));
  
  /**
   * GtkCellRendererText:wrap-mode:
   *
   * Specifies how to break the string into multiple lines, if the cell 
   * renderer does not have enough room to display the entire string. 
   * This property has no effect unless the wrap-width property is set.
   *
   * Since: 2.8
   */
  g_object_class_install_property (object_class,
                                   PROP_WRAP_MODE,
                                   g_param_spec_enum ("wrap-mode",
						      ("Wrap mode"),
						      ("How to break the string into multiple lines, "
							 "if the cell renderer does not have enough room "
							 "to display the entire string"),
						      PANGO_TYPE_WRAP_MODE,
						      PANGO_WRAP_CHAR,
						      EXO_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellRendererText:wrap-width:
   *
   * Specifies the minimum width at which the text is wrapped. The wrap-mode property can 
   * be used to influence at what character positions the line breaks can be placed.
   * Setting wrap-width to -1 turns wrapping off.
   *
   * Since: 2.8
   */
  g_object_class_install_property (object_class,
				   PROP_WRAP_WIDTH,
				   g_param_spec_int ("wrap-width",
						     ("Wrap width"),
						     ("The width at which the text is wrapped"),
						     -1,
						     G_MAXINT,
						     -1,
						     EXO_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellRendererText:alignment:
   *
   * Specifies how to align the lines of text with respect to each other. 
   *
   * Note that this property describes how to align the lines of text in 
   * case there are several of them. The "xalign" property of #GtkCellRenderer, 
   * on the other hand, sets the horizontal alignment of the whole text.
   *
   * Since: 2.10
   */
  g_object_class_install_property (object_class,
                                   PROP_ALIGN,
                                   g_param_spec_enum ("alignment",
						      ("Alignment"),
						      ("How to align the lines"),
						      PANGO_TYPE_ALIGNMENT,
						      PANGO_ALIGN_LEFT,
						      EXO_PARAM_READWRITE|G_PARAM_EXPLICIT_NOTIFY));

  /**
   * GtkCellRendererText:placeholder-text:
   *
   * The text that will be displayed in the #GtkCellRenderer if
   * #GtkCellRendererText:editable is %TRUE and the cell is empty.
   *
   * Since 3.6
   */
  g_object_class_install_property (object_class,
                                   PROP_PLACEHOLDER_TEXT,
                                   g_param_spec_string ("placeholder-text",
                                                        ("Placeholder text"),
                                                        ("Text rendered when an editable cell is empty"),
                                                        NULL,
                                                        EXO_PARAM_READWRITE));
  /**
   * ThunarIconRenderer:highlight:
   *
   * The color with which the file should be highlighted
   * #ThunarIconRenderer instance.
   **/
  g_object_class_install_property (object_class,
                                   PROP_HIGHLIGHT,
                                   g_param_spec_string ("highlight", "highlight", "highlight",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));

  g_object_class_install_property (object_class,
                                   PROP_BORDER_RADIUS,
                                   g_param_spec_string ("border-radius", "border-radius", "border-radius",
                                                        NULL,
                                                        EXO_PARAM_READWRITE));


  /* Style props are set or not */

#define ADD_SET_PROP(propname, propval, nick, blurb) g_object_class_install_property (object_class, propval, g_param_spec_boolean (propname, nick, blurb, FALSE, EXO_PARAM_READWRITE))

  ADD_SET_PROP ("background-set", PROP_BACKGROUND_SET,
                ("Background set"),
                ("Whether this tag affects the background color"));

  ADD_SET_PROP ("foreground-set", PROP_FOREGROUND_SET,
                ("Foreground set"),
                ("Whether this tag affects the foreground color"));
  
  ADD_SET_PROP ("editable-set", PROP_EDITABLE_SET,
                ("Editability set"),
                ("Whether this tag affects text editability"));

  ADD_SET_PROP ("family-set", PROP_FAMILY_SET,
                ("Font family set"),
                ("Whether this tag affects the font family"));  

  ADD_SET_PROP ("style-set", PROP_STYLE_SET,
                ("Font style set"),
                ("Whether this tag affects the font style"));

  ADD_SET_PROP ("variant-set", PROP_VARIANT_SET,
                ("Font variant set"),
                ("Whether this tag affects the font variant"));

  ADD_SET_PROP ("weight-set", PROP_WEIGHT_SET,
                ("Font weight set"),
                ("Whether this tag affects the font weight"));

  ADD_SET_PROP ("stretch-set", PROP_STRETCH_SET,
                ("Font stretch set"),
                ("Whether this tag affects the font stretch"));

  ADD_SET_PROP ("size-set", PROP_SIZE_SET,
                ("Font size set"),
                ("Whether this tag affects the font size"));

  ADD_SET_PROP ("scale-set", PROP_SCALE_SET,
                ("Font scale set"),
                ("Whether this tag scales the font size by a factor"));
  
  ADD_SET_PROP ("rise-set", PROP_RISE_SET,
                ("Rise set"),
                ("Whether this tag affects the rise"));

  ADD_SET_PROP ("strikethrough-set", PROP_STRIKETHROUGH_SET,
                ("Strikethrough set"),
                ("Whether this tag affects strikethrough"));

  ADD_SET_PROP ("underline-set", PROP_UNDERLINE_SET,
                ("Underline set"),
                ("Whether this tag affects underlining"));

  ADD_SET_PROP ("language-set", PROP_LANGUAGE_SET,
                ("Language set"),
                ("Whether this tag affects the language the text is rendered as"));

  ADD_SET_PROP ("ellipsize-set", PROP_ELLIPSIZE_SET,
                ("Ellipsize set"),
                ("Whether this tag affects the ellipsize mode"));

  ADD_SET_PROP ("align-set", PROP_ALIGN_SET,
                ("Align set"),
                ("Whether this tag affects the alignment mode"));

  ADD_SET_PROP ("highlight-set", PROP_HIGHLIGHT_SET,
                ("Highlight set"),
                ("Whether this tag affects the highlight mode"));

  ADD_SET_PROP ("border-radius-set", PROP_BORDER_RADIUS_SET,
                ("Border-radius set"),
                ("Whether this tag affects the border-radius mode"));
}



static void
thunar_text_renderer_init (ThunarTextRenderer *text_renderer)
{
  GtkCellRenderer *cell = GTK_CELL_RENDERER (text_renderer);

  gtk_cell_renderer_set_alignment (cell, 0.0, 0.5);
  gtk_cell_renderer_set_padding (cell, 2, 2);
  text_renderer->font_scale = 1.0;
  text_renderer->fixed_height_rows = -1;
  text_renderer->font = pango_font_description_new ();

  text_renderer->width_chars = -1;
  text_renderer->max_width_chars = -1;
  text_renderer->wrap_width = -1;
  text_renderer->wrap_mode = PANGO_WRAP_CHAR;
  text_renderer->align = PANGO_ALIGN_LEFT;
  text_renderer->align_set = FALSE;
}



static void
thunar_text_renderer_finalize (GObject *object)
{
  ThunarTextRenderer *celltext = THUNAR_TEXT_RENDERER (object);

  pango_font_description_free (celltext->font);

  g_free (celltext->text);
  g_free (celltext->placeholder_text);

  if (celltext->extra_attrs)
    pango_attr_list_unref (celltext->extra_attrs);

  if (celltext->language)
    g_object_unref (celltext->language);

  g_free (celltext->highlight);

  G_OBJECT_CLASS (thunar_text_renderer_parent_class)->finalize (object);
}



static PangoFontMask
get_property_font_set_mask (guint prop_id)
{
  switch (prop_id)
    {
    case PROP_FAMILY_SET:
      return PANGO_FONT_MASK_FAMILY;
    case PROP_STYLE_SET:
      return PANGO_FONT_MASK_STYLE;
    case PROP_VARIANT_SET:
      return PANGO_FONT_MASK_VARIANT;
    case PROP_WEIGHT_SET:
      return PANGO_FONT_MASK_WEIGHT;
    case PROP_STRETCH_SET:
      return PANGO_FONT_MASK_STRETCH;
    case PROP_SIZE_SET:
      return PANGO_FONT_MASK_SIZE;
    }

  return 0;
}



static void
thunar_text_renderer_get_property (GObject    *object,
                                   guint       prop_id,
                                   GValue     *value,
                                   GParamSpec *pspec)
{
  ThunarTextRenderer *celltext = THUNAR_TEXT_RENDERER (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_value_set_string (value, celltext->text);
      break;

    case PROP_ATTRIBUTES:
      g_value_set_boxed (value, celltext->extra_attrs);
      break;

    case PROP_SINGLE_PARAGRAPH_MODE:
      g_value_set_boolean (value, celltext->single_paragraph);
      break;

    case PROP_BACKGROUND_GDK:
      {
        GdkColor color;

        color.red = (guint16) (celltext->background.red * 65535);
        color.green = (guint16) (celltext->background.green * 65535);
        color.blue = (guint16) (celltext->background.blue * 65535);

        g_value_set_boxed (value, &color);
      }
      break;

    case PROP_FOREGROUND_GDK:
      {
        GdkColor color;

        color.red = (guint16) (celltext->foreground.red * 65535);
        color.green = (guint16) (celltext->foreground.green * 65535);
        color.blue = (guint16) (celltext->foreground.blue * 65535);

        g_value_set_boxed (value, &color);
      }
      break;

    case PROP_BACKGROUND_RGBA:
      g_value_set_boxed (value, &celltext->background);
      break;

    case PROP_FOREGROUND_RGBA:
      g_value_set_boxed (value, &celltext->foreground);
      break;

    case PROP_FONT:
      g_value_take_string (value, pango_font_description_to_string (celltext->font));
      break;
      
    case PROP_FONT_DESC:
      g_value_set_boxed (value, celltext->font);
      break;

    case PROP_FAMILY:
      g_value_set_string (value, pango_font_description_get_family (celltext->font));
      break;

    case PROP_STYLE:
      g_value_set_enum (value, pango_font_description_get_style (celltext->font));
      break;

    case PROP_VARIANT:
      g_value_set_enum (value, pango_font_description_get_variant (celltext->font));
      break;

    case PROP_WEIGHT:
      g_value_set_int (value, pango_font_description_get_weight (celltext->font));
      break;

    case PROP_STRETCH:
      g_value_set_enum (value, pango_font_description_get_stretch (celltext->font));
      break;

    case PROP_SIZE:
      g_value_set_int (value, pango_font_description_get_size (celltext->font));
      break;

    case PROP_SIZE_POINTS:
      g_value_set_double (value, ((double)pango_font_description_get_size (celltext->font)) / (double)PANGO_SCALE);
      break;

    case PROP_SCALE:
      g_value_set_double (value, celltext->font_scale);
      break;
      
    case PROP_EDITABLE:
      g_value_set_boolean (value, celltext->editable);
      break;

    case PROP_STRIKETHROUGH:
      g_value_set_boolean (value, celltext->strikethrough);
      break;

    case PROP_UNDERLINE:
      g_value_set_enum (value, celltext->underline_style);
      break;

    case PROP_RISE:
      g_value_set_int (value, celltext->rise);
      break;  

    case PROP_LANGUAGE:
      g_value_set_static_string (value, pango_language_to_string (celltext->language));
      break;

    case PROP_ELLIPSIZE:
      g_value_set_enum (value, celltext->ellipsize);
      break;
      
    case PROP_WRAP_MODE:
      g_value_set_enum (value, celltext->wrap_mode);
      break;

    case PROP_WRAP_WIDTH:
      g_value_set_int (value, celltext->wrap_width);
      break;
      
    case PROP_ALIGN:
      g_value_set_enum (value, celltext->align);
      break;

    case PROP_BACKGROUND_SET:
      g_value_set_boolean (value, celltext->background_set);
      break;

    case PROP_FOREGROUND_SET:
      g_value_set_boolean (value, celltext->foreground_set);
      break;

    case PROP_FAMILY_SET:
    case PROP_STYLE_SET:
    case PROP_VARIANT_SET:
    case PROP_WEIGHT_SET:
    case PROP_STRETCH_SET:
    case PROP_SIZE_SET:
      {
	PangoFontMask mask = get_property_font_set_mask (prop_id);
	g_value_set_boolean (value, (pango_font_description_get_set_fields (celltext->font) & mask) != 0);
	
	break;
      }

    case PROP_SCALE_SET:
      g_value_set_boolean (value, celltext->scale_set);
      break;
      
    case PROP_EDITABLE_SET:
      g_value_set_boolean (value, celltext->editable_set);
      break;

    case PROP_STRIKETHROUGH_SET:
      g_value_set_boolean (value, celltext->strikethrough_set);
      break;

    case PROP_UNDERLINE_SET:
      g_value_set_boolean (value, celltext->underline_set);
      break;

    case  PROP_RISE_SET:
      g_value_set_boolean (value, celltext->rise_set);
      break;

    case PROP_LANGUAGE_SET:
      g_value_set_boolean (value, celltext->language_set);
      break;

    case PROP_ELLIPSIZE_SET:
      g_value_set_boolean (value, celltext->ellipsize_set);
      break;

    case PROP_ALIGN_SET:
      g_value_set_boolean (value, celltext->align_set);
      break;
      
    case PROP_WIDTH_CHARS:
      g_value_set_int (value, celltext->width_chars);
      break;  

    case PROP_MAX_WIDTH_CHARS:
      g_value_set_int (value, celltext->max_width_chars);
      break;  

    case PROP_PLACEHOLDER_TEXT:
      g_value_set_string (value, celltext->placeholder_text);
      break;

    case PROP_HIGHLIGHT:
      g_value_set_string (value, celltext->highlight);
      break;

    case PROP_HIGHLIGHT_SET:
      g_value_set_boolean (value, celltext->highlight_set);
      break;

    case PROP_BORDER_RADIUS:
      g_value_set_string (value, celltext->border_radius);
      break;

    case PROP_BORDER_RADIUS_SET:
      g_value_set_boolean (value, celltext->border_radius_set);
      break;

    case PROP_BACKGROUND:
    case PROP_FOREGROUND:
    case PROP_MARKUP:
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, param_id, pspec);
      break;
    }
}



static void
set_bg_color (ThunarTextRenderer  *celltext,
              GdkRGBA             *rgba)
{
  if (rgba)
    {
      if (!celltext->background_set)
        {
          celltext->background_set = TRUE;
          g_object_notify (G_OBJECT (celltext), "background-set");
        }

      celltext->background = *rgba;
    }
  else
    {
      if (celltext->background_set)
        {
          celltext->background_set = FALSE;
          g_object_notify (G_OBJECT (celltext), "background-set");
        }
    }
}



static void
set_fg_color (ThunarTextRenderer  *celltext,
              GdkRGBA             *rgba)
{
  if (rgba)
    {
      if (!celltext->foreground_set)
        {
          celltext->foreground_set = TRUE;
          g_object_notify (G_OBJECT (celltext), "foreground-set");
        }

      celltext->foreground = *rgba;
    }
  else
    {
      if (celltext->foreground_set)
        {
          celltext->foreground_set = FALSE;
          g_object_notify (G_OBJECT (celltext), "foreground-set");
        }
    }
}

static PangoFontMask
set_font_desc_fields (PangoFontDescription *desc,
		      PangoFontMask         to_set)
{
  PangoFontMask changed_mask = 0;
  
  if (to_set & PANGO_FONT_MASK_FAMILY)
    {
      const char *family = pango_font_description_get_family (desc);
      if (!family)
	{
	  family = "sans";
	  changed_mask |= PANGO_FONT_MASK_FAMILY;
	}

      pango_font_description_set_family (desc, family);
    }
  if (to_set & PANGO_FONT_MASK_STYLE)
    pango_font_description_set_style (desc, pango_font_description_get_style (desc));
  if (to_set & PANGO_FONT_MASK_VARIANT)
    pango_font_description_set_variant (desc, pango_font_description_get_variant (desc));
  if (to_set & PANGO_FONT_MASK_WEIGHT)
    pango_font_description_set_weight (desc, pango_font_description_get_weight (desc));
  if (to_set & PANGO_FONT_MASK_STRETCH)
    pango_font_description_set_stretch (desc, pango_font_description_get_stretch (desc));
  if (to_set & PANGO_FONT_MASK_SIZE)
    {
      gint size = pango_font_description_get_size (desc);
      if (size <= 0)
	{
	  size = 10 * PANGO_SCALE;
	  changed_mask |= PANGO_FONT_MASK_SIZE;
	}
      
      pango_font_description_set_size (desc, size);
    }

  return changed_mask;
}

static void
notify_set_changed (GObject       *object,
		    PangoFontMask  changed_mask)
{
  if (changed_mask & PANGO_FONT_MASK_FAMILY)
    g_object_notify (object, "family-set");
  if (changed_mask & PANGO_FONT_MASK_STYLE)
    g_object_notify (object, "style-set");
  if (changed_mask & PANGO_FONT_MASK_VARIANT)
    g_object_notify (object, "variant-set");
  if (changed_mask & PANGO_FONT_MASK_WEIGHT)
    g_object_notify (object, "weight-set");
  if (changed_mask & PANGO_FONT_MASK_STRETCH)
    g_object_notify (object, "stretch-set");
  if (changed_mask & PANGO_FONT_MASK_SIZE)
    g_object_notify (object, "size-set");
}

static void
notify_fields_changed (GObject       *object,
		       PangoFontMask  changed_mask)
{
  if (changed_mask & PANGO_FONT_MASK_FAMILY)
    g_object_notify (object, "family");
  if (changed_mask & PANGO_FONT_MASK_STYLE)
    g_object_notify (object, "style");
  if (changed_mask & PANGO_FONT_MASK_VARIANT)
    g_object_notify (object, "variant");
  if (changed_mask & PANGO_FONT_MASK_WEIGHT)
    g_object_notify (object, "weight");
  if (changed_mask & PANGO_FONT_MASK_STRETCH)
    g_object_notify (object, "stretch");
  if (changed_mask & PANGO_FONT_MASK_SIZE)
    g_object_notify (object, "size");
}

static void
set_font_description (ThunarTextRenderer   *celltext,
                      PangoFontDescription *font_desc)
{
  GObject *object = G_OBJECT (celltext);
  PangoFontDescription *new_font_desc;
  PangoFontMask old_mask, new_mask, changed_mask, set_changed_mask;
  
  if (font_desc)
    new_font_desc = pango_font_description_copy (font_desc);
  else
    new_font_desc = pango_font_description_new ();

  old_mask = pango_font_description_get_set_fields (celltext->font);
  new_mask = pango_font_description_get_set_fields (new_font_desc);

  changed_mask = old_mask | new_mask;
  set_changed_mask = old_mask ^ new_mask;

  pango_font_description_free (celltext->font);
  celltext->font = new_font_desc;
  
  g_object_freeze_notify (object);

  g_object_notify (object, "font-desc");
  g_object_notify (object, "font");
  
  if (changed_mask & PANGO_FONT_MASK_FAMILY)
    g_object_notify (object, "family");
  if (changed_mask & PANGO_FONT_MASK_STYLE)
    g_object_notify (object, "style");
  if (changed_mask & PANGO_FONT_MASK_VARIANT)
    g_object_notify (object, "variant");
  if (changed_mask & PANGO_FONT_MASK_WEIGHT)
    g_object_notify (object, "weight");
  if (changed_mask & PANGO_FONT_MASK_STRETCH)
    g_object_notify (object, "stretch");
  if (changed_mask & PANGO_FONT_MASK_SIZE)
    {
      g_object_notify (object, "size");
      g_object_notify (object, "size-points");
    }

  notify_set_changed (object, set_changed_mask);
  
  g_object_thaw_notify (object);
}




static void
thunar_text_renderer_set_property (GObject      *object,
                                   guint         prop_id,
                                   const GValue *value,
                                   GParamSpec   *pspec)
{
  ThunarTextRenderer *celltext = THUNAR_TEXT_RENDERER (object);

  switch (prop_id)
    {
    case PROP_TEXT:
      g_free (celltext->text);

      if (celltext->markup_set)
        {
          if (celltext->extra_attrs)
            pango_attr_list_unref (celltext->extra_attrs);
          celltext->extra_attrs = NULL;
          celltext->markup_set = FALSE;
        }

      celltext->text = g_value_dup_string (value);
      g_object_notify (object, "text");
      break;

    case PROP_ATTRIBUTES:
      if (celltext->extra_attrs)
	pango_attr_list_unref (celltext->extra_attrs);

      celltext->extra_attrs = g_value_get_boxed (value);
      if (celltext->extra_attrs)
        pango_attr_list_ref (celltext->extra_attrs);
      break;
    case PROP_MARKUP:
      {
	const gchar *str;
	gchar *text = NULL;
	GError *error = NULL;
	PangoAttrList *attrs = NULL;

	str = g_value_get_string (value);
	if (str && !pango_parse_markup (str,
					-1, 0,
					&attrs, &text,
					NULL, &error))
	  {
	    g_warning ("Failed to set text from markup due to error parsing markup: %s",
		       error->message);
	    g_error_free (error);
	    return;
	  }

	g_free (celltext->text);

	if (celltext->extra_attrs)
	  pango_attr_list_unref (celltext->extra_attrs);

	celltext->text = text;
	celltext->extra_attrs = attrs;
        celltext->markup_set = TRUE;
      }
      break;

    case PROP_SINGLE_PARAGRAPH_MODE:
      if (celltext->single_paragraph != g_value_get_boolean (value))
        {
          celltext->single_paragraph = g_value_get_boolean (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
      
    case PROP_BACKGROUND:
      {
        GdkRGBA rgba;

        if (!g_value_get_string (value))
          set_bg_color (celltext, NULL);       /* reset to background_set to FALSE */
        else if (gdk_rgba_parse (&rgba, g_value_get_string (value)))
          set_bg_color (celltext, &rgba);
        else
          g_warning ("Don't know color `%s'", g_value_get_string (value));

        g_object_notify (object, "background-gdk");
      }
      break;
      
    case PROP_FOREGROUND:
      {
        GdkRGBA rgba;

        if (!g_value_get_string (value))
          set_fg_color (celltext, NULL);       /* reset to foreground_set to FALSE */
        else if (gdk_rgba_parse (&rgba, g_value_get_string (value)))
          set_fg_color (celltext, &rgba);
        else
          g_warning ("Don't know color `%s'", g_value_get_string (value));

        g_object_notify (object, "foreground-gdk");
      }
      break;

    case PROP_BACKGROUND_GDK:
      {
        GdkColor *color;

        color = g_value_get_boxed (value);
        if (color)
          {
            GdkRGBA rgba;

            rgba.red = color->red / 65535.;
            rgba.green = color->green / 65535.;
            rgba.blue = color->blue / 65535.;
            rgba.alpha = 1;

            set_bg_color (celltext, &rgba);
          }
        else
          {
            set_bg_color (celltext, NULL);
          }
      }
      break;

    case PROP_FOREGROUND_GDK:
      {
        GdkColor *color;

        color = g_value_get_boxed (value);
        if (color)
          {
            GdkRGBA rgba;

            rgba.red = color->red / 65535.;
            rgba.green = color->green / 65535.;
            rgba.blue = color->blue / 65535.;
            rgba.alpha = 1;

            set_fg_color (celltext, &rgba);
          }
        else
          {
            set_fg_color (celltext, NULL);
          }
      }
      break;

    case PROP_BACKGROUND_RGBA:
      /* This notifies the GObject itself. */
      set_bg_color (celltext, g_value_get_boxed (value));
      break;

    case PROP_FOREGROUND_RGBA:
      /* This notifies the GObject itself. */
      set_fg_color (celltext, g_value_get_boxed (value));
      break;

    case PROP_FONT:
      {
        PangoFontDescription *font_desc = NULL;
        const gchar *name;

        name = g_value_get_string (value);

        if (name)
          font_desc = pango_font_description_from_string (name);

        set_font_description (celltext, font_desc);

	pango_font_description_free (font_desc);
        
	if (celltext->fixed_height_rows != -1)
	  celltext->calc_fixed_height = TRUE;
      }
      break;

    case PROP_FONT_DESC:
      set_font_description (celltext, g_value_get_boxed (value));
      
      if (celltext->fixed_height_rows != -1)
	celltext->calc_fixed_height = TRUE;
      break;

    case PROP_FAMILY:
    case PROP_STYLE:
    case PROP_VARIANT:
    case PROP_WEIGHT:
    case PROP_STRETCH:
    case PROP_SIZE:
    case PROP_SIZE_POINTS:
      {
	PangoFontMask old_set_mask = pango_font_description_get_set_fields (celltext->font);
	
	switch (prop_id)
	  {
	  case PROP_FAMILY:
	    pango_font_description_set_family (celltext->font,
					       g_value_get_string (value));
	    break;
	  case PROP_STYLE:
	    pango_font_description_set_style (celltext->font,
					      g_value_get_enum (value));
	    break;
	  case PROP_VARIANT:
	    pango_font_description_set_variant (celltext->font,
						g_value_get_enum (value));
	    break;
	  case PROP_WEIGHT:
	    pango_font_description_set_weight (celltext->font,
					       g_value_get_int (value));
	    break;
	  case PROP_STRETCH:
	    pango_font_description_set_stretch (celltext->font,
						g_value_get_enum (value));
	    break;
	  case PROP_SIZE:
	    pango_font_description_set_size (celltext->font,
					     g_value_get_int (value));
	    g_object_notify (object, "size-points");
	    break;
	  case PROP_SIZE_POINTS:
	    pango_font_description_set_size (celltext->font,
					     g_value_get_double (value) * PANGO_SCALE);
	    g_object_notify (object, "size");
	    break;
	  }
	
	if (celltext->fixed_height_rows != -1)
	  celltext->calc_fixed_height = TRUE;
	
	notify_set_changed (object, old_set_mask & pango_font_description_get_set_fields (celltext->font));
	g_object_notify (object, "font-desc");
	g_object_notify (object, "font");

	break;
      }
      
    case PROP_SCALE:
      celltext->font_scale = g_value_get_double (value);
      celltext->scale_set = TRUE;
      if (celltext->fixed_height_rows != -1)
	celltext->calc_fixed_height = TRUE;
      g_object_notify (object, "scale-set");
      break;
      
    case PROP_EDITABLE:
      celltext->editable = g_value_get_boolean (value);
      celltext->editable_set = TRUE;
      if (celltext->editable)
        g_object_set (celltext, "mode", GTK_CELL_RENDERER_MODE_EDITABLE, NULL);
      else
        g_object_set (celltext, "mode", GTK_CELL_RENDERER_MODE_INERT, NULL);
      g_object_notify (object, "editable-set");
      break;

    case PROP_STRIKETHROUGH:
      celltext->strikethrough = g_value_get_boolean (value);
      celltext->strikethrough_set = TRUE;
      g_object_notify (object, "strikethrough-set");
      break;

    case PROP_UNDERLINE:
      celltext->underline_style = g_value_get_enum (value);
      celltext->underline_set = TRUE;
      g_object_notify (object, "underline-set");
            
      break;

    case PROP_RISE:
      celltext->rise = g_value_get_int (value);
      celltext->rise_set = TRUE;
      g_object_notify (object, "rise-set");
      if (celltext->fixed_height_rows != -1)
	celltext->calc_fixed_height = TRUE;
      break;  

    case PROP_LANGUAGE:
      celltext->language_set = TRUE;
      if (celltext->language)
        g_object_unref (celltext->language);
      celltext->language = pango_language_from_string (g_value_get_string (value));
      g_object_notify (object, "language-set");
      break;

    case PROP_ELLIPSIZE:
      celltext->ellipsize = g_value_get_enum (value);
      celltext->ellipsize_set = TRUE;
      g_object_notify (object, "ellipsize-set");
      break;
      
    case PROP_WRAP_MODE:
      if (celltext->wrap_mode != (PangoWrapMode) g_value_get_enum (value))
        {
          celltext->wrap_mode = g_value_get_enum (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
      
    case PROP_WRAP_WIDTH:
      if (celltext->wrap_width != g_value_get_int (value))
        {
          celltext->wrap_width = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;
            
    case PROP_WIDTH_CHARS:
      if (celltext->width_chars != g_value_get_int (value))
        {
          celltext->width_chars  = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;  

    case PROP_MAX_WIDTH_CHARS:
      if (celltext->max_width_chars != g_value_get_int (value))
        {
          celltext->max_width_chars  = g_value_get_int (value);
          g_object_notify_by_pspec (object, pspec);
        }
      break;  

    case PROP_ALIGN:
      if (celltext->align != (PangoAlignment) g_value_get_enum (value))
        {
          celltext->align = g_value_get_enum (value);
          g_object_notify (object, "alignment");
        }
      celltext->align_set = TRUE;
      g_object_notify (object, "align-set");
      break;

    case PROP_BACKGROUND_SET:
      celltext->background_set = g_value_get_boolean (value);
      break;

    case PROP_FOREGROUND_SET:
      celltext->foreground_set = g_value_get_boolean (value);
      break;

    case PROP_FAMILY_SET:
    case PROP_STYLE_SET:
    case PROP_VARIANT_SET:
    case PROP_WEIGHT_SET:
    case PROP_STRETCH_SET:
    case PROP_SIZE_SET:
      if (!g_value_get_boolean (value))
	{
	  pango_font_description_unset_fields (celltext->font,
					       get_property_font_set_mask (prop_id));
	}
      else
	{
	  PangoFontMask changed_mask;
	  
	  changed_mask = set_font_desc_fields (celltext->font,
					       get_property_font_set_mask (prop_id));
	  notify_fields_changed (G_OBJECT (celltext), changed_mask);
	}
      break;

    case PROP_SCALE_SET:
      celltext->scale_set = g_value_get_boolean (value);
      break;
      
    case PROP_EDITABLE_SET:
      celltext->editable_set = g_value_get_boolean (value);
      break;

    case PROP_STRIKETHROUGH_SET:
      celltext->strikethrough_set = g_value_get_boolean (value);
      break;

    case PROP_UNDERLINE_SET:
      celltext->underline_set = g_value_get_boolean (value);
      break;

    case PROP_RISE_SET:
      celltext->rise_set = g_value_get_boolean (value);
      break;

    case PROP_LANGUAGE_SET:
      celltext->language_set = g_value_get_boolean (value);
      break;

    case PROP_ELLIPSIZE_SET:
      celltext->ellipsize_set = g_value_get_boolean (value);
      break;

    case PROP_ALIGN_SET:
      celltext->align_set = g_value_get_boolean (value);
      break;

    case PROP_PLACEHOLDER_TEXT:
      g_free (celltext->placeholder_text);
      celltext->placeholder_text = g_value_dup_string (value);
      break;

    case PROP_HIGHLIGHT:
      if (G_UNLIKELY (celltext->highlight != NULL))
        g_free (celltext->highlight);
      celltext->highlight = g_value_dup_string (value);
      break;

    case PROP_HIGHLIGHT_SET:
      celltext->highlight_set = g_value_get_boolean (value);
      break;

    case PROP_BORDER_RADIUS:
      if (G_UNLIKELY (celltext->border_radius != NULL))
        g_free (celltext->border_radius);
      celltext->border_radius = g_value_dup_string (value);
      break;

    case PROP_BORDER_RADIUS_SET:
      celltext->border_radius_set = g_value_get_boolean (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



/**
 * thunar_text_renderer_new:
 *
 * Creates a new #ThunarTextRenderer. Adjust rendering
 * parameters using object properties. Object properties can be
 * set globally with #g_object_set. Also, with #GtkTreeViewColumn,
 * you can bind a property to a value in a #GtkTreeModel.
 *
 * Return value: the newly allocated #ThunarTextRenderer.
 **/
GtkCellRenderer*
thunar_text_renderer_new (void)
{
  return g_object_new (THUNAR_TYPE_TEXT_RENDERER, NULL);
}

static inline gboolean
show_placeholder_text (ThunarTextRenderer *celltext)
{
  return celltext->editable && celltext->placeholder_text &&
    (!celltext->text || !celltext->text[0]);
}

static void
add_attr (PangoAttrList  *attr_list,
          PangoAttribute *attr)
{
  attr->start_index = 0;
  attr->end_index = G_MAXINT;
  
  pango_attr_list_insert (attr_list, attr);
}

static PangoLayout*
get_layout (ThunarTextRenderer  *celltext,
            GtkWidget           *widget,
            const GdkRectangle  *cell_area,
            GtkCellRendererState flags)
{
  PangoAttrList *attr_list;
  PangoLayout *layout;
  PangoUnderline uline;
  gint xpad;
  gboolean placeholder_layout = show_placeholder_text (celltext);

  layout = gtk_widget_create_pango_layout (widget, placeholder_layout ?
                                           celltext->placeholder_text : celltext->text);

  gtk_cell_renderer_get_padding (GTK_CELL_RENDERER (celltext), &xpad, NULL);

  if (celltext->extra_attrs)
    attr_list = pango_attr_list_copy (celltext->extra_attrs);
  else
    attr_list = pango_attr_list_new ();

  pango_layout_set_single_paragraph_mode (layout, celltext->single_paragraph);

  if (!placeholder_layout && cell_area)
    {
      /* Add options that affect appearance but not size */
      
      /* note that background doesn't go here, since it affects
       * background_area not the PangoLayout area
       */
      
      if (celltext->foreground_set
	  && (flags & GTK_CELL_RENDERER_SELECTED) == 0)
        {
          PangoColor color;

          color.red = (guint16) (celltext->foreground.red * 65535);
          color.green = (guint16) (celltext->foreground.green * 65535);
          color.blue = (guint16) (celltext->foreground.blue * 65535);

          add_attr (attr_list,
                    pango_attr_foreground_new (color.red, color.green, color.blue));
        }

      if (celltext->strikethrough_set)
        add_attr (attr_list,
                  pango_attr_strikethrough_new (celltext->strikethrough));
    }
  else if (placeholder_layout)
    {
      PangoColor color;
      GtkStyleContext *context;
      GdkRGBA fg = { 0.5, 0.5, 0.5 };

      context = gtk_widget_get_style_context (widget);
      gtk_style_context_lookup_color (context, "placeholder_text_color", &fg);

      color.red = CLAMP (fg.red * 65535. + 0.5, 0, 65535);
      color.green = CLAMP (fg.green * 65535. + 0.5, 0, 65535);
      color.blue = CLAMP (fg.blue * 65535. + 0.5, 0, 65535);

      add_attr (attr_list,
                pango_attr_foreground_new (color.red, color.green, color.blue));
    }

  add_attr (attr_list, pango_attr_font_desc_new (celltext->font));

  if (celltext->scale_set &&
      celltext->font_scale != 1.0)
    add_attr (attr_list, pango_attr_scale_new (celltext->font_scale));
  
  if (celltext->underline_set)
    uline = celltext->underline_style;
  else
    uline = PANGO_UNDERLINE_NONE;

  if (celltext->language_set)
    add_attr (attr_list, pango_attr_language_new (celltext->language));
  
  if ((flags & GTK_CELL_RENDERER_PRELIT) == GTK_CELL_RENDERER_PRELIT)
    {
      switch (uline)
        {
        case PANGO_UNDERLINE_NONE:
          uline = PANGO_UNDERLINE_SINGLE;
          break;

        case PANGO_UNDERLINE_SINGLE:
          uline = PANGO_UNDERLINE_DOUBLE;
          break;

        default:
          break;
        }
    }

  if (uline != PANGO_UNDERLINE_NONE)
    add_attr (attr_list, pango_attr_underline_new (celltext->underline_style));

  if (celltext->rise_set)
    add_attr (attr_list, pango_attr_rise_new (celltext->rise));

  /* Now apply the attributes as they will effect the outcome
   * of pango_layout_get_extents() */
  pango_layout_set_attributes (layout, attr_list);
  pango_attr_list_unref (attr_list);

  if (celltext->ellipsize_set)
    pango_layout_set_ellipsize (layout, celltext->ellipsize);
  else
    pango_layout_set_ellipsize (layout, PANGO_ELLIPSIZE_NONE);

  if (celltext->wrap_width != -1)
    {
      PangoRectangle rect;
      gint           width, text_width;

      pango_layout_get_extents (layout, NULL, &rect);
      text_width = rect.width;

      if (cell_area)
	width = (cell_area->width - xpad * 2) * PANGO_SCALE;
      else
	width = celltext->wrap_width * PANGO_SCALE;

      width = MIN (width, text_width);

      pango_layout_set_width (layout, width);
      pango_layout_set_wrap (layout, celltext->wrap_mode);
    }
  else
    {
      pango_layout_set_width (layout, -1);
      pango_layout_set_wrap (layout, PANGO_WRAP_CHAR);
    }

  if (celltext->align_set)
    pango_layout_set_alignment (layout, celltext->align);
  else
    {
      PangoAlignment align;

      if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	align = PANGO_ALIGN_RIGHT;
      else
	align = PANGO_ALIGN_LEFT;

      pango_layout_set_alignment (layout, align);
    }
  
  return layout;
}


static void
get_size (ThunarTextRenderer *cell,
	        GtkWidget          *widget,
	        const GdkRectangle *cell_area,
	        PangoLayout        *layout,
	        gint               *x_offset,
	        gint               *y_offset,
	        gint               *width,
	        gint               *height)
{
  PangoRectangle rect;
  gint xpad, ypad;
  gint cell_width, cell_height;

  gtk_cell_renderer_get_padding (GTK_CELL_RENDERER (cell), &xpad, &ypad);

  if (cell->calc_fixed_height)
    {
      GtkStyleContext *style_context;
      GtkStateFlags state;
      PangoContext *context;
      PangoFontMetrics *metrics;
      PangoFontDescription *font_desc;
      gint row_height;

      style_context = gtk_widget_get_style_context (widget);
      state = gtk_widget_get_state_flags (widget);

      gtk_style_context_get (style_context, state, "font", &font_desc, NULL);
      pango_font_description_merge_static (font_desc, cell->font, TRUE);

      if (cell->scale_set)
	pango_font_description_set_size (font_desc,
					 cell->font_scale * pango_font_description_get_size (font_desc));

      context = gtk_widget_get_pango_context (widget);

      metrics = pango_context_get_metrics (context,
					   font_desc,
					   pango_context_get_language (context));
      row_height = (pango_font_metrics_get_ascent (metrics) +
		    pango_font_metrics_get_descent (metrics));
      pango_font_metrics_unref (metrics);

      pango_font_description_free (font_desc);

      gtk_cell_renderer_get_fixed_size (GTK_CELL_RENDERER (cell), &cell_width, &cell_height);

      gtk_cell_renderer_set_fixed_size (GTK_CELL_RENDERER (cell),
					cell_width, 2 * ypad +
					cell->fixed_height_rows * PANGO_PIXELS (row_height));
      
      if (height)
	{
	  *height = cell_height;
	  height = NULL;
	}
      cell->calc_fixed_height = FALSE;
      if (width == NULL)
	return;
    }
  
  if (layout)
    g_object_ref (layout);
  else
    layout = get_layout (cell, widget, NULL, 0);

  pango_layout_get_pixel_extents (layout, NULL, &rect);

  if (cell_area)
    {
      gfloat xalign, yalign;

      gtk_cell_renderer_get_alignment (GTK_CELL_RENDERER (cell), &xalign, &yalign);

      rect.height = MIN (rect.height, cell_area->height - 2 * ypad);
      rect.width  = MIN (rect.width, cell_area->width - 2 * xpad);

      if (x_offset)
	{
	  if (gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL)
	    *x_offset = (1.0 - xalign) * (cell_area->width - (rect.width + (2 * xpad)));
	  else 
	    *x_offset = xalign * (cell_area->width - (rect.width + (2 * xpad)));

	  if ((cell->ellipsize_set && cell->ellipsize != PANGO_ELLIPSIZE_NONE) || cell->wrap_width != -1)
	    *x_offset = MAX(*x_offset, 0);
	}
      if (y_offset)
	{
	  *y_offset = yalign * (cell_area->height - (rect.height + (2 * ypad)));
	  *y_offset = MAX (*y_offset, 0);
	}
    }
  else
    {
      if (x_offset) *x_offset = 0;
      if (y_offset) *y_offset = 0;
    }

  if (height)
    *height = ypad * 2 + rect.height;

  if (width)
    *width = xpad * 2 + rect.width;

  g_object_unref (layout);
}

static void
thunar_text_renderer_render (GtkCellRenderer      *cell,
			                       cairo_t              *cr,
			                       GtkWidget            *widget,
			                       const GdkRectangle   *background_area,
			                       const GdkRectangle   *cell_area,
			                       GtkCellRendererState  flags)
{
  ThunarTextRenderer *celltext = THUNAR_TEXT_RENDERER (cell);
  GtkStyleContext    *context;
  GtkStateFlags       state;
  PangoLayout        *layout;
  PangoRectangle      rect;
  gint                x_offset = 0;
  gint                y_offset = 0;
  gint                xpad, ypad;
  gdouble             corner_radius = cell_area->width / 10.0;
  gdouble             degrees = G_PI / 180.0;
  GdkRGBA            *color = NULL;
  GdkRGBA             highlight_color;

  layout = get_layout (celltext, widget, cell_area, flags);
  get_size (celltext, widget, cell_area, layout, &x_offset, &y_offset, NULL, NULL);
  context = gtk_widget_get_style_context (widget);

  /* clip the background_area to rounded corners */
  cairo_new_sub_path (cr);
  cairo_arc (cr, background_area->x - 2.0 + background_area->width - 0, background_area->y - 2.0 + 0, 0, -90 * degrees, 0 * degrees);
  cairo_arc (cr, background_area->x - 2.0 + background_area->width - corner_radius, background_area->y - 2.0 + background_area->height - corner_radius, corner_radius, 0 * degrees, 90 * degrees);
  cairo_arc (cr, background_area->x + 2.0 + corner_radius, background_area->y - 2.0 + background_area->height - corner_radius, corner_radius, 90 * degrees, 180 * degrees);
  cairo_arc (cr, background_area->x + 2.0 + 0, background_area->y - 2.0 + 0, 0, 180 * degrees, 270 * degrees);
  cairo_close_path (cr);
  // cairo_clip (cr);

  if (celltext->highlight_set)
    {
      gdk_rgba_parse (&highlight_color, celltext->highlight);
      color = gdk_rgba_copy (&highlight_color);
    }
  if ((flags & GTK_CELL_RENDERER_SELECTED) != 0)
    {
      state = gtk_widget_has_focus (widget) ? GTK_STATE_FLAG_SELECTED : GTK_STATE_FLAG_ACTIVE;
      gtk_style_context_get (context, state, GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &color, NULL);
    }

  if (color != NULL)
    {
      gdk_cairo_set_source_rgba (cr, color);
      gdk_rgba_free (color);
      cairo_fill_preserve (cr);
      gdk_cairo_set_source_rgba (cr, &highlight_color);
      cairo_set_line_width (cr, 2.0);
      cairo_stroke (cr);
    }

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

  if (celltext->ellipsize_set && celltext->ellipsize != PANGO_ELLIPSIZE_NONE)
    pango_layout_set_width (layout, (cell_area->width - x_offset - 2 * xpad) * PANGO_SCALE);
  else if (celltext->wrap_width == -1)
    pango_layout_set_width (layout, -1);

  pango_layout_get_pixel_extents (layout, NULL, &rect);
  x_offset = x_offset - rect.x;

  cairo_save (cr);

  gdk_cairo_rectangle (cr, cell_area);
  cairo_clip (cr);

  gtk_render_layout (context, cr,
                     cell_area->x + x_offset + xpad,
                     cell_area->y + y_offset + ypad,
                     layout);

  cairo_restore (cr);

  g_object_unref (layout);
}



/**
 * thunar_text_renderer_set_fixed_height_from_font:
 * @renderer: A #GtkCellRendererText
 * @number_of_rows: Number of rows of text each cell renderer is allocated, or -1
 * 
 * Sets the height of a renderer to explicitly be determined by the “font” and
 * “y_pad” property set on it.  Further changes in these properties do not
 * affect the height, so they must be accompanied by a subsequent call to this
 * function.  Using this function is unflexible, and should really only be used
 * if calculating the size of a cell is too slow (ie, a massive number of cells
 * displayed).  If @number_of_rows is -1, then the fixed height is unset, and
 * the height is determined by the properties again.
 **/
void
thunar_text_renderer_set_fixed_height_from_font (ThunarTextRenderer  *renderer,
						                                       gint                 number_of_rows)
{
  GtkCellRenderer *cell;

  g_return_if_fail (THUNAR_IS_TEXT_RENDERER (renderer));
  g_return_if_fail (number_of_rows == -1 || number_of_rows > 0);

  cell = GTK_CELL_RENDERER (renderer);

  if (number_of_rows == -1)
    {
      gint width, height;

      gtk_cell_renderer_get_fixed_size (cell, &width, &height);
      gtk_cell_renderer_set_fixed_size (cell, width, -1);
    }
  else
    {
      renderer->fixed_height_rows = number_of_rows;
      renderer->calc_fixed_height = TRUE;
    }
}

static void
thunar_text_renderer_get_preferred_width (GtkCellRenderer *cell,
                                            GtkWidget       *widget,
                                            gint            *minimum_size,
                                            gint            *natural_size)
{
  ThunarTextRenderer         *celltext;
  PangoLayout                *layout;
  PangoContext               *context;
  PangoFontMetrics           *metrics;
  PangoRectangle              rect;
  gint char_width, text_width, ellipsize_chars, xpad;
  gint min_width, nat_width;

  /* "width-chars" Hard-coded minimum width:
   *    - minimum size should be MAX (width-chars, strlen ("..."));
   *    - natural size should be MAX (width-chars, strlen (label->text));
   *
   * "wrap-width" User specified natural wrap width
   *    - minimum size should be MAX (width-chars, 0)
   *    - natural size should be MIN (wrap-width, strlen (label->text))
   */

  celltext = THUNAR_TEXT_RENDERER (cell);

  gtk_cell_renderer_get_padding (cell, &xpad, NULL);

  layout = get_layout (celltext, widget, NULL, 0);

  /* Fetch the length of the complete unwrapped text */
  pango_layout_set_width (layout, -1);
  pango_layout_get_extents (layout, NULL, &rect);
  text_width = rect.width;

  /* Fetch the average size of a charachter */
  context = pango_layout_get_context (layout);
  metrics = pango_context_get_metrics (context,
                                       pango_context_get_font_description (context),
                                       pango_context_get_language (context));

  char_width = pango_font_metrics_get_approximate_char_width (metrics);

  pango_font_metrics_unref (metrics);
  g_object_unref (layout);

  /* enforce minimum width for ellipsized labels at ~3 chars */
  if (celltext->ellipsize_set && celltext->ellipsize != PANGO_ELLIPSIZE_NONE)
    ellipsize_chars = 3;
  else
    ellipsize_chars = 0;

  if ((celltext->ellipsize_set && celltext->ellipsize != PANGO_ELLIPSIZE_NONE) || celltext->width_chars > 0)
    min_width = xpad * 2 +
      MIN (PANGO_PIXELS_CEIL (text_width),
           (PANGO_PIXELS (char_width) * MAX (celltext->width_chars, ellipsize_chars)));
  /* If no width-chars set, minimum for wrapping text will be the wrap-width */
  else if (celltext->wrap_width > -1)
    min_width = xpad * 2 + rect.x + MIN (PANGO_PIXELS_CEIL (text_width), celltext->wrap_width);
  else
    min_width = xpad * 2 + rect.x + PANGO_PIXELS_CEIL (text_width);

  if (celltext->width_chars > 0)
    nat_width = xpad * 2 +
      MAX ((PANGO_PIXELS (char_width) * celltext->width_chars), PANGO_PIXELS_CEIL (text_width));
  else
    nat_width = xpad * 2 + PANGO_PIXELS_CEIL (text_width);

  nat_width = MAX (nat_width, min_width);

  if (celltext->max_width_chars > 0)
    {
      gint max_width = xpad * 2 + PANGO_PIXELS (char_width) * celltext->max_width_chars;
      
      min_width = MIN (min_width, max_width);
      nat_width = MIN (nat_width, max_width);
    }

  if (minimum_size)
    *minimum_size = min_width;

  if (natural_size)
    *natural_size = nat_width;
}

static void
thunar_text_renderer_get_preferred_height_for_width (GtkCellRenderer *cell,
                                                       GtkWidget       *widget,
                                                       gint             width,
                                                       gint            *minimum_height,
                                                       gint            *natural_height)
{
  ThunarTextRenderer  *celltext;
  PangoLayout         *layout;
  gint                 text_height, xpad, ypad;


  celltext = THUNAR_TEXT_RENDERER (cell);

  gtk_cell_renderer_get_padding (cell, &xpad, &ypad);

  layout = get_layout (celltext, widget, NULL, 0);

  pango_layout_set_width (layout, (width - xpad * 2) * PANGO_SCALE);
  pango_layout_get_pixel_size (layout, NULL, &text_height);

  if (minimum_height)
    *minimum_height = text_height + ypad * 2;

  if (natural_height)
    *natural_height = text_height + ypad * 2;

  g_object_unref (layout);
}

static void
thunar_text_renderer_get_preferred_height (GtkCellRenderer *cell,
                                             GtkWidget       *widget,
                                             gint            *minimum_size,
                                             gint            *natural_size)
{
  gint min_width;

  /* Thankfully cell renderers dont rotate, so they only have to do
   * height-for-width and not the opposite. Here we have only to return
   * the height for the base minimum width of the renderer.
   *
   * Note this code path wont be followed by GtkTreeView which is
   * height-for-width specifically.
   */
  gtk_cell_renderer_get_preferred_width (cell, widget, &min_width, NULL);
  thunar_text_renderer_get_preferred_height_for_width (cell, widget, min_width,
                                                         minimum_size, natural_size);
}
