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

#define DRIVE_FULLNESS_BAR_HEIGHT 7
#define DRIVE_FULLNESS_BAR_YPAD 4

struct _ThunarShortcutsNameRendererClass
{
  GtkCellRendererTextClass __parent__;
};

struct _ThunarShortcutsNameRenderer
{
  GtkCellRendererText __parent__;

  gboolean disk_space_usage_bar_enabled;
  gint     disk_space_usage_percent;
};

enum
{
  PROP_0,
  PROP_DISK_SPACE_USAGE_BAR_ENABLED,
  PROP_DISK_SPACE_USAGE_PERCENT,
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
                                                         G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

  g_object_class_install_property (object_class, PROP_DISK_SPACE_USAGE_PERCENT,
                                   g_param_spec_int ("disk-space-usage-percent",
                                                     "Disk space usage percentage",
                                                     NULL,
                                                     -1, 100,
                                                     -1,
                                                     G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS));

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

  /* Render name */
  if (thunar_shortcuts_name_renderer_should_show_disk_space_usage_bar (self))
    {
      gtk_cell_renderer_get_aligned_area (GTK_CELL_RENDERER (self), widget, flags, cell_area, &aligned_area);
      gtk_cell_renderer_get_padding (cell, NULL, &ypad);

      extra_height = DRIVE_FULLNESS_BAR_HEIGHT + DRIVE_FULLNESS_BAR_YPAD;
      extra_name_ratio = (gdouble) extra_height / (gdouble) aligned_area.height;
      content_cell_ratio = (gdouble) (aligned_area.height + extra_height) / (gdouble) cell_area->height;
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
  GdkRectangle                 aligned_area;
  GdkRectangle                 bar_area;
  gint                         xpad;
  gint                         progress_width;

  gtk_cell_renderer_get_aligned_area (cell, widget, flags, cell_area, &aligned_area);
  gtk_cell_renderer_get_padding (cell, &xpad, NULL);

  bar_area.x = cell_area->x + xpad;
  bar_area.y = aligned_area.y + aligned_area.height;
  bar_area.width = MIN (175, cell_area->width - MAX (6, xpad * 2));
  bar_area.height = DRIVE_FULLNESS_BAR_HEIGHT;
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
  gtk_render_background (context, cr, bar_area.x, bar_area.y, progress_width, bar_area.height);
  gtk_render_frame (context, cr, bar_area.x, bar_area.y, bar_area.width, bar_area.height);
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

  renderer_class->get_preferred_height_for_width (cell, widget, width, minimum_height, natural_height);

  if (thunar_shortcuts_name_renderer_should_show_disk_space_usage_bar (self))
    {
      *minimum_height += DRIVE_FULLNESS_BAR_HEIGHT + DRIVE_FULLNESS_BAR_YPAD;
      *natural_height += DRIVE_FULLNESS_BAR_HEIGHT + DRIVE_FULLNESS_BAR_YPAD;
    }
}



static gboolean
thunar_shortcuts_name_renderer_should_show_disk_space_usage_bar (const ThunarShortcutsNameRenderer *self)
{
  return self->disk_space_usage_bar_enabled && self->disk_space_usage_percent >= 0;
}
