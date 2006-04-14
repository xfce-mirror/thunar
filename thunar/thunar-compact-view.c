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

#include <thunar/thunar-compact-view.h>



static void         thunar_compact_view_class_init     (ThunarCompactViewClass  *klass);
static void         thunar_compact_view_init           (ThunarCompactView       *compact_view);
static AtkObject   *thunar_compact_view_get_accessible (GtkWidget               *widget);



struct _ThunarCompactViewClass
{
  ThunarAbstractIconViewClass __parent__;
};

struct _ThunarCompactView
{
  ThunarAbstractIconView __parent__;
};



static GObjectClass *thunar_compact_view_parent_class;



GType
thunar_compact_view_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarCompactViewClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_compact_view_class_init,
        NULL,
        NULL,
        sizeof (ThunarCompactView),
        0,
        (GInstanceInitFunc) thunar_compact_view_init,
        NULL,
      };

      type = g_type_register_static (THUNAR_TYPE_ABSTRACT_ICON_VIEW, I_("ThunarCompactView"), &info, 0);
    }

  return type;
}



static void
thunar_compact_view_class_init (ThunarCompactViewClass *klass)
{
  ThunarStandardViewClass *thunarstandard_view_class;
  GtkWidgetClass          *gtkwidget_class;

  /* determine the parent type class */
  thunar_compact_view_parent_class = g_type_class_peek_parent (klass);

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->get_accessible = thunar_compact_view_get_accessible;

  thunarstandard_view_class = THUNAR_STANDARD_VIEW_CLASS (klass);
  thunarstandard_view_class->zoom_level_property_name = "last-compact-view-zoom-level";
}



static void
thunar_compact_view_init (ThunarCompactView *compact_view)
{
 /* initialize the icon view properties */
  exo_icon_view_set_margin (EXO_ICON_VIEW (GTK_BIN (compact_view)->child), 3);
  exo_icon_view_set_row_spacing (EXO_ICON_VIEW (GTK_BIN (compact_view)->child), 0);
  exo_icon_view_set_column_spacing (EXO_ICON_VIEW (GTK_BIN (compact_view)->child), 6);
  exo_icon_view_set_layout_mode (EXO_ICON_VIEW (GTK_BIN (compact_view)->child), EXO_ICON_VIEW_LAYOUT_COLS);
  exo_icon_view_set_orientation (EXO_ICON_VIEW (GTK_BIN (compact_view)->child), GTK_ORIENTATION_HORIZONTAL);

  /* setup the icon renderer */
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (compact_view)->icon_renderer),
                "ypad", 2u,
                NULL);

  /* setup the name renderer (wrap only very long names) */
  g_object_set (G_OBJECT (THUNAR_STANDARD_VIEW (compact_view)->name_renderer),
                "wrap-mode", PANGO_WRAP_WORD_CHAR,
                "wrap-width", 1280,
                "xalign", 0.0f,
                "yalign", 0.5f,
                NULL);
}



static AtkObject*
thunar_compact_view_get_accessible (GtkWidget *widget)
{
  AtkObject *object;

  /* query the atk object for the icon view class */
  object = (*GTK_WIDGET_CLASS (thunar_compact_view_parent_class)->get_accessible) (widget);

  /* set custom Atk properties for the icon view */
  if (G_LIKELY (object != NULL))
    {
      atk_object_set_description (object, _("Compact directory listing"));
      atk_object_set_name (object, _("Compact view"));
      atk_object_set_role (object, ATK_ROLE_DIRECTORY_PANE);
    }

  return object;
}



