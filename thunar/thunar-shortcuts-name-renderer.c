/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2025 The Xfce Development Team
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

#include "thunar/thunar-shortcuts-name-renderer.h"

#define SPACE_USAGE_BAR_YPAD 4

#define SPACE_USAGE_BAR_HEIGHT 4

/*
 * In addition to the standard capabilities of GtkCellRendererText, this renderer
 * can display a progress bar beneath the text, which is used to indicate disk usage.
 * Despite the existence of GtkCellRendererProgress, it cannot be used as a replacement
 * for this renderer, as GtkCellRendererProgress always displays the progress bar at the
 * full height of the cell and offers no way to modify or override this behavior.
 *
 * In addition to the standard properties inherited from GtkCellRendererText,
 * this renderer has the following additional properties:
 *
 *   disk-space-usage-bar-enabled - boolean, controls whether the progress bar is displayed
 *
 *   disk-space-usage-percent - integer, accepts values (-1, 0-100); when set to -1, the progress bar is not displayed
 *
 *   disk-space-warning-percent - integer, accept values (0-100); color is not displayed if value is 0
 *
 *   disk-space-error-percent - integer, accepts value (0-100); color is not displayed if value is 0
 *
 * CAUTION: This class uses the yalign property for positioning the text, so you cannot
 * set this property yourself.
 */
struct _ThunarShortcutsNameRendererClass
{
  GtkCellRendererTextClass __parent__;
};

struct _ThunarShortcutsNameRenderer
{
  GtkCellRendererText __parent__;

  gboolean disk_space_usage_bar_enabled;
  gint     disk_space_usage_percent;
  gint     disk_space_usage_warning_percent;
  gint     disk_space_usage_error_percent;
};

enum
{
  PROP_0,
  PROP_DISK_SPACE_USAGE_BAR_ENABLED,
  PROP_DISK_SPACE_USAGE_PERCENT,
  PROP_DISK_SPACE_USAGE_WARNING_PERCENT,
  PROP_DISK_SPACE_USAGE_ERROR_PERCENT
};



static void
thunar_shortcuts_name_renderer_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec);

static void
thunar_shortcuts_name_renderer_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec);

static void
thunar_shortcuts_name_renderer_render (GtkCellRenderer     *cell,
                                       cairo_t             *cr,
                                       GtkWidget           *widget,
                                       const GdkRectangle  *background_area,
                                       const GdkRectangle  *cell_area,
                                       GtkCellRendererState flags);

static void
thunar_shortcuts_name_renderer_render_disk_space_usage_bar (GtkCellRenderer     *cell,
                                                            cairo_t             *cr,
                                                            GtkWidget           *widget,
                                                            const GdkRectangle  *background_area,
                                                            const GdkRectangle  *cell_area,
                                                            GtkCellRendererState flags);

static void
thunar_shortcuts_name_renderer_get_preferred_height_for_width (GtkCellRenderer *cell,
                                                               GtkWidget       *widget,
                                                               gint             width,
                                                               gint            *minimum_height,
                                                               gint            *natural_height);

static gboolean
thunar_shortcuts_name_renderer_should_show_disk_space_usage_bar (const ThunarShortcutsNameRenderer *self);



static gint
thunar_shortcuts_name_renderer_scale_for_dpi (GtkWidget *widget,
                                              gint       value);



static const gchar *
thunar_shortcuts_name_renderer_get_disk_space_usage_bar_additional_class (const ThunarShortcutsNameRenderer *self);


static void
thunar_shortcuts_name_renderer_install_disk_space_usage_bar_style (GtkWidget *widget);


static gboolean
thunar_shortcuts_name_renderer_disk_space_usage_bar_styled (GtkWidget *widget);


static gint
thunar_shortcuts_name_renderer_get_disk_space_usage_bar_height (GtkWidget *widget);



G_DEFINE_TYPE (ThunarShortcutsNameRenderer, thunar_shortcuts_name_renderer, GTK_TYPE_CELL_RENDERER_TEXT)



static void
thunar_shortcuts_name_renderer_class_init (ThunarShortcutsNameRendererClass *klass)
{
  GObjectClass         *object_class = G_OBJECT_CLASS (klass);
  GtkCellRendererClass *renderer_class = GTK_CELL_RENDERER_CLASS (klass);

  object_class->get_property = thunar_shortcuts_name_renderer_get_property;
  object_class->set_property = thunar_shortcuts_name_renderer_set_property;

  g_object_class_install_property (object_class, PROP_DISK_SPACE_USAGE_BAR_ENABLED,
                                   g_param_spec_boolean ("disk-space-usage-bar-enabled",
                                                         "Show disk space usage bar",
                                                         NULL,
                                                         TRUE,
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_DISK_SPACE_USAGE_PERCENT,
                                   g_param_spec_int ("disk-space-usage-percent",
                                                     "Disk space usage percentage",
                                                     NULL,
                                                     -1, 100,
                                                     -1,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_DISK_SPACE_USAGE_WARNING_PERCENT,
                                   g_param_spec_int ("disk-space-usage-warning-percent",
                                                     NULL,
                                                     NULL,
                                                     0, 100,
                                                     0,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  g_object_class_install_property (object_class, PROP_DISK_SPACE_USAGE_ERROR_PERCENT,
                                   g_param_spec_int ("disk-space-usage-error-percent",
                                                     NULL,
                                                     NULL,
                                                     0, 100,
                                                     0,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_CONSTRUCT));

  renderer_class->render = thunar_shortcuts_name_renderer_render;
  renderer_class->get_preferred_height_for_width = thunar_shortcuts_name_renderer_get_preferred_height_for_width;
}



static void
thunar_shortcuts_name_renderer_init (ThunarShortcutsNameRenderer *self)
{
}



static void
thunar_shortcuts_name_renderer_get_property (GObject    *object,
                                             guint       prop_id,
                                             GValue     *value,
                                             GParamSpec *pspec)
{
  const ThunarShortcutsNameRenderer *self = THUNAR_SHORTCUTS_NAME_RENDERER (object);

  switch (prop_id)
    {
    case PROP_DISK_SPACE_USAGE_BAR_ENABLED:
      g_value_set_boolean (value, self->disk_space_usage_bar_enabled);
      break;

    case PROP_DISK_SPACE_USAGE_PERCENT:
      g_value_set_int (value, self->disk_space_usage_percent);
      break;

    case PROP_DISK_SPACE_USAGE_WARNING_PERCENT:
      g_value_set_int (value, self->disk_space_usage_warning_percent);
      break;

    case PROP_DISK_SPACE_USAGE_ERROR_PERCENT:
      g_value_set_int (value, self->disk_space_usage_error_percent);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}



static void
thunar_shortcuts_name_renderer_set_property (GObject      *object,
                                             guint         prop_id,
                                             const GValue *value,
                                             GParamSpec   *pspec)
{
  ThunarShortcutsNameRenderer *self = THUNAR_SHORTCUTS_NAME_RENDERER (object);

  switch (prop_id)
    {
    case PROP_DISK_SPACE_USAGE_BAR_ENABLED:
      self->disk_space_usage_bar_enabled = g_value_get_boolean (value);
      break;

    case PROP_DISK_SPACE_USAGE_PERCENT:
      self->disk_space_usage_percent = g_value_get_int (value);
      break;

    case PROP_DISK_SPACE_USAGE_WARNING_PERCENT:
      self->disk_space_usage_warning_percent = g_value_get_int (value);
      break;

    case PROP_DISK_SPACE_USAGE_ERROR_PERCENT:
      self->disk_space_usage_error_percent = g_value_get_int (value);
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
    }
}



static void
thunar_shortcuts_name_renderer_render (GtkCellRenderer     *cell,
                                       cairo_t             *cr,
                                       GtkWidget           *widget,
                                       const GdkRectangle  *background_area,
                                       const GdkRectangle  *cell_area,
                                       GtkCellRendererState flags)
{
  ThunarShortcutsNameRenderer *self = THUNAR_SHORTCUTS_NAME_RENDERER (cell);
  const GtkCellRendererClass  *renderer_class = GTK_CELL_RENDERER_CLASS (thunar_shortcuts_name_renderer_parent_class);
  GdkRectangle                 aligned_area;
  gdouble                      extra_height, extra_name_ratio, content_cell_ratio, yalign;
  gint                         ypad;
  gint                         text_height;
  gint                         cell_height;
  gint                         bar_height = thunar_shortcuts_name_renderer_get_disk_space_usage_bar_height (widget);


  /* Render name */
  if (thunar_shortcuts_name_renderer_should_show_disk_space_usage_bar (self))
    {
      gtk_cell_renderer_get_aligned_area (GTK_CELL_RENDERER (self), widget, flags, cell_area, &aligned_area);
      gtk_cell_renderer_get_padding (cell, NULL, &ypad);

      /* Preventing division by zero */
      text_height = MAX (1, aligned_area.height);
      cell_height = MAX (1, cell_area->height);

      /* By default, yalign is 0.5, which means center. Set yalign so that the center
       * corresponds not to the text, but to the text plus the bar. */
      extra_height = bar_height + SPACE_USAGE_BAR_YPAD;
      extra_name_ratio = (gdouble) extra_height / (gdouble) text_height;
      content_cell_ratio = (gdouble) (aligned_area.height + extra_height) / (gdouble) cell_height;
      yalign = MAX (0.0, 0.5 - (extra_name_ratio * content_cell_ratio));
      g_object_set (cell,
                    "yalign", yalign,
                    NULL);
    }
  else
    {
      g_object_set (cell,
                    "yalign", 0.5,
                    NULL);
    }
  renderer_class->render (cell, cr, widget, background_area, cell_area, flags);

  /* Render bar */
  if (thunar_shortcuts_name_renderer_should_show_disk_space_usage_bar (self))
    thunar_shortcuts_name_renderer_render_disk_space_usage_bar (cell, cr, widget, background_area, cell_area, flags);
}



static void
thunar_shortcuts_name_renderer_render_disk_space_usage_bar (GtkCellRenderer     *cell,
                                                            cairo_t             *cr,
                                                            GtkWidget           *widget,
                                                            const GdkRectangle  *background_area,
                                                            const GdkRectangle  *cell_area,
                                                            GtkCellRendererState flags)
{
  ThunarShortcutsNameRenderer *self = THUNAR_SHORTCUTS_NAME_RENDERER (cell);
  GtkStyleContext             *context = gtk_widget_get_style_context (widget);
  gboolean                     is_rtl = gtk_widget_get_direction (widget) == GTK_TEXT_DIR_RTL;
  gint                         bar_width_max = thunar_shortcuts_name_renderer_scale_for_dpi (widget, 125);
  const gchar                 *bar_additonal_class = thunar_shortcuts_name_renderer_get_disk_space_usage_bar_additional_class (self);
  gint                         bar_height = thunar_shortcuts_name_renderer_get_disk_space_usage_bar_height (widget);
  GdkRectangle                 aligned_area;
  GdkRectangle                 bar_area;
  gint                         xpad;
  gint                         progress_width;

  thunar_shortcuts_name_renderer_install_disk_space_usage_bar_style (widget);

  /*
   * This code is based on gtk_cell_renderer_progress_render:
   * https://gitlab.gnome.org/GNOME/gtk/-/blob/503e8f40b9559ce921e9330b71ec6f2bb99c8923/gtk/gtkcellrendererprogress.c#L557
   */
  gtk_cell_renderer_get_aligned_area (cell, widget, flags, cell_area, &aligned_area);
  gtk_cell_renderer_get_padding (cell, &xpad, NULL);
  bar_area.width = MIN (bar_width_max, cell_area->width - MAX (6, xpad * 2));
  bar_area.height = bar_height;

  if (is_rtl)
    bar_area.x = cell_area->x + cell_area->width - xpad - bar_area.width;
  else
    bar_area.x = cell_area->x + xpad;

  bar_area.y = aligned_area.y + aligned_area.height;

  progress_width = (bar_area.width / 100.0) * self->disk_space_usage_percent;

  cairo_save (cr);

  /* Background */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_TROUGH);
  gtk_render_background (context, cr, bar_area.x, bar_area.y, bar_area.width, bar_area.height);
  gtk_render_frame (context, cr, bar_area.x, bar_area.y, bar_area.width, bar_area.height);
  gtk_style_context_restore (context);

  /* Foreground */
  gtk_style_context_save (context);
  gtk_style_context_add_class (context, GTK_STYLE_CLASS_PROGRESSBAR);
  gtk_style_context_add_class (context, "disk-space-usage-bar");
  gtk_style_context_add_class (context, bar_additonal_class);
  gtk_render_background (context, cr, bar_area.x, bar_area.y, progress_width, bar_area.height);
  gtk_render_frame (context, cr, bar_area.x, bar_area.y, progress_width, bar_area.height);
  gtk_style_context_restore (context);

  cairo_restore (cr);
}



static void
thunar_shortcuts_name_renderer_get_preferred_height_for_width (GtkCellRenderer *cell,
                                                               GtkWidget       *widget,
                                                               gint             width,
                                                               gint            *minimum_height,
                                                               gint            *natural_height)
{
  ThunarShortcutsNameRenderer *self = THUNAR_SHORTCUTS_NAME_RENDERER (cell);
  const GtkCellRendererClass  *renderer_class = GTK_CELL_RENDERER_CLASS (thunar_shortcuts_name_renderer_parent_class);
  gint                         bar_height = thunar_shortcuts_name_renderer_get_disk_space_usage_bar_height (widget);

  renderer_class->get_preferred_height_for_width (cell, widget, width, minimum_height, natural_height);

  if (thunar_shortcuts_name_renderer_should_show_disk_space_usage_bar (self))
    {
      *minimum_height += bar_height + SPACE_USAGE_BAR_YPAD;
      *natural_height += bar_height + SPACE_USAGE_BAR_YPAD;
    }
}



static gboolean
thunar_shortcuts_name_renderer_should_show_disk_space_usage_bar (const ThunarShortcutsNameRenderer *self)
{
  return self->disk_space_usage_bar_enabled && self->disk_space_usage_percent >= 0;
}



static gint
thunar_shortcuts_name_renderer_scale_for_dpi (GtkWidget *widget,
                                              gint       value)
{
  /* 96.0 is considered the base value, as specified in the description of gdk_screen_set_resolution */
  gdouble dpi_scale = gdk_screen_get_resolution (gtk_widget_get_screen (widget)) / 96.0;
  return (value * dpi_scale) + 0.5;
}



static const gchar *
thunar_shortcuts_name_renderer_get_disk_space_usage_bar_additional_class (const ThunarShortcutsNameRenderer *self)
{
  if (self->disk_space_usage_error_percent > 0 && self->disk_space_usage_percent >= self->disk_space_usage_error_percent)
    return "disk-space-usage-bar--error";

  if (self->disk_space_usage_warning_percent > 0 && self->disk_space_usage_percent >= self->disk_space_usage_warning_percent)
    return "disk-space-usage-bar--warning";

  return "disk-space-usage-bar--normal";
}



static void
thunar_shortcuts_name_renderer_install_disk_space_usage_bar_style (GtkWidget *widget)
{
  GtkCssProvider *provider;

  /* Gtk has non-standard precedence and overriding rules for CSS, to ensure correct display with different themes,
   * this function checks for the presence of a style for the progressbar, and if it is not there, adds its own. */
  if (!thunar_shortcuts_name_renderer_disk_space_usage_bar_styled (widget))
    {
      provider = gtk_css_provider_new ();
      gtk_css_provider_load_from_data (provider,
                                       ".disk-space-usage-bar--warning { background: none; background-color: @warning_color; }"
                                       ".disk-space-usage-bar--error { background: none; background-color: @error_color; }",
                                       -1, NULL);

      gtk_style_context_add_provider_for_screen (gdk_screen_get_default (),
                                                 GTK_STYLE_PROVIDER (provider),
                                                 GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);
      g_object_unref (provider);
    }
}



static gboolean
thunar_shortcuts_name_renderer_disk_space_usage_bar_styled (GtkWidget *widget)
{
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  GdkRGBA         *background_color = NULL;
  GList           *classes;
  GList           *l;
  gboolean         status;

  gtk_style_context_save (context);

  classes = gtk_style_context_list_classes (context);
  for (l = classes; l != NULL; l = l->next)
    {
      gtk_style_context_remove_class (context, l->data);
    }
  g_list_free (classes);

  gtk_style_context_add_class (context, "disk-space-usage-bar--normal");
  gtk_style_context_add_class (context, "disk-space-usage-bar--warning");
  gtk_style_context_add_class (context, "disk-space-usage-bar--error");
  gtk_style_context_get (context, GTK_STATE_FLAG_NORMAL,
                         GTK_STYLE_PROPERTY_BACKGROUND_COLOR, &background_color,
                         NULL);

  gtk_style_context_restore (context);

  status = background_color != NULL && background_color->alpha > 0;

  g_free (background_color);

  return status;
}



static gint
thunar_shortcuts_name_renderer_get_disk_space_usage_bar_height (GtkWidget *widget)
{
  GtkStyleContext *context = gtk_widget_get_style_context (widget);
  gint             min_height;

  gtk_style_context_save (context);
  gtk_style_context_add_class (context, "disk-space-usage-bar");
  gtk_style_context_get (context, GTK_STATE_FLAG_NORMAL,
                         "min-height", &min_height,
                         NULL);
  gtk_style_context_restore (context);

  if (min_height <= 0)
    min_height = SPACE_USAGE_BAR_HEIGHT;

  return thunar_shortcuts_name_renderer_scale_for_dpi (widget, min_height);
}
