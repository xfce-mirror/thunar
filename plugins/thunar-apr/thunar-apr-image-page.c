/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#include <libxfce4util/libxfce4util.h>
#include <thunar-apr/thunar-apr-image-page.h>

#ifdef HAVE_GEXIV2
#include <gexiv2/gexiv2.h>
#endif



static void
thunar_apr_image_page_file_changed (ThunarAprAbstractPage *abstract_page,
                                    ThunarxFileInfo       *file);



#ifdef HAVE_GEXIV2
static const struct
{
  const gchar *name;
  const gchar *tag;
}

TAGS_GEXIV2[] =
{
  { N_ ("Date Taken:"),        "Exif.Image.DateTimeOriginal",  },
  { N_ ("Camera Brand:"),      "Exif.Image.Make",              },
  { N_ ("Camera Model:"),      "Exif.Image.Model",             },
  { N_ ("Exposure Time:"),     "Exif.Photo.ExposureTime",      },
  { N_ ("Exposure Program:"),  "Exif.Photo.ExposureProgram",   },
  { N_ ("Aperture Value:"),    "Exif.Photo.ApertureValue",     },
  { N_ ("Metering Mode:"),     "Exif.Photo.MeteringMode",      },
  { N_ ("Flash Fired:"),       "Exif.Photo.Flash",             },
  { N_ ("Focal Length:"),      "Exif.Photo.FocalLength",       },
  { N_ ("Shutter Speed:"),     "Exif.Image.ShutterSpeedValue", },
  { N_ ("ISO Speed Ratings:"), "Exif.Photo.ISOSpeedRatings",   },
  { N_ ("Software:"),          "Exif.Image.Software",          },
  { N_ ("Description:"),       "Exif.Image.ImageDescription",  },
  { N_ ("Comment:"),           "Exif.Photo.UserComment",       },
};
#endif



struct _ThunarAprImagePageClass
{
  ThunarAprAbstractPageClass __parent__;
};

struct _ThunarAprImagePage
{
  ThunarAprAbstractPage __parent__;
  GtkWidget            *type_label;
  GtkWidget            *dimensions_label;

#ifdef HAVE_GEXIV2
  GtkWidget *gexiv2_labels[G_N_ELEMENTS (TAGS_GEXIV2)];
#endif
};



THUNARX_DEFINE_TYPE (ThunarAprImagePage,
                     thunar_apr_image_page,
                     THUNAR_APR_TYPE_ABSTRACT_PAGE);



static void
thunar_apr_image_page_class_init (ThunarAprImagePageClass *klass)
{
  ThunarAprAbstractPageClass *thunarapr_abstract_page_class;

  thunarapr_abstract_page_class = THUNAR_APR_ABSTRACT_PAGE_CLASS (klass);
  thunarapr_abstract_page_class->file_changed = thunar_apr_image_page_file_changed;
}



static void
thunar_apr_image_page_init (ThunarAprImagePage *image_page)
{
  AtkRelationSet *relations;
  PangoAttribute *attribute;
  PangoAttrList  *attr_list;
  AtkRelation    *relation;
  AtkObject      *object;
  GtkWidget      *label;
  GtkWidget      *grid;
#ifdef HAVE_GEXIV2
  GtkWidget *spacer;
  guint      n;
#endif

  gtk_container_set_border_width (GTK_CONTAINER (image_page), 12);
  thunarx_property_page_set_label (THUNARX_PROPERTY_PAGE (image_page), _("Image"));

  /* allocate shared bold Pango attributes */
  attr_list = pango_attr_list_new ();
  attribute = pango_attr_weight_new (PANGO_WEIGHT_BOLD);
  attribute->start_index = 0;
  attribute->end_index = -1;
  pango_attr_list_insert (attr_list, attribute);

  grid = gtk_grid_new ();
  gtk_grid_set_column_spacing (GTK_GRID (grid), 12);
  gtk_grid_set_row_spacing (GTK_GRID (grid), 6);
  gtk_container_add (GTK_CONTAINER (image_page), grid);
  gtk_widget_show (grid);

  label = gtk_label_new (_("Image Type:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), attr_list);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 0, 1, 1);
  gtk_widget_show (label);

  image_page->type_label = gtk_label_new ("");
  gtk_label_set_selectable (GTK_LABEL (image_page->type_label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (image_page->type_label), 0.0f);
  gtk_label_set_ellipsize (GTK_LABEL (image_page->type_label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_hexpand (image_page->type_label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), image_page->type_label, 1, 0, 1, 1);
  gtk_widget_show (image_page->type_label);

  /* set Atk label relation for the label */
  object = gtk_widget_get_accessible (image_page->type_label);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));
  g_object_unref (relations);

  label = gtk_label_new (_("Image Size:"));
  gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
  gtk_label_set_attributes (GTK_LABEL (label), attr_list);
  gtk_grid_attach (GTK_GRID (grid), label, 0, 1, 1, 1);
  gtk_widget_show (label);

  image_page->dimensions_label = gtk_label_new ("");
  gtk_label_set_selectable (GTK_LABEL (image_page->dimensions_label), TRUE);
  gtk_label_set_xalign (GTK_LABEL (image_page->dimensions_label), 0.0f);
  gtk_label_set_ellipsize (GTK_LABEL (image_page->dimensions_label), PANGO_ELLIPSIZE_END);
  gtk_widget_set_hexpand (image_page->dimensions_label, TRUE);
  gtk_grid_attach (GTK_GRID (grid), image_page->dimensions_label, 1, 1, 1, 1);
  gtk_widget_show (image_page->dimensions_label);

  /* set Atk label relation for the label */
  object = gtk_widget_get_accessible (image_page->dimensions_label);
  relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
  relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
  atk_relation_set_add (relations, relation);
  g_object_unref (G_OBJECT (relation));
  g_object_unref (relations);

#ifdef HAVE_GEXIV2
  /* some spacing between the General info and the gexiv2 info */
  spacer = g_object_new (GTK_TYPE_BOX, "orientation", GTK_ORIENTATION_VERTICAL, "height-request", 6, NULL);
  gtk_grid_attach (GTK_GRID (grid), spacer, 0, 2, 2, 1);
  gtk_widget_show (spacer);

  /* add labels for the gexiv2 info */
  for (n = 0; n < G_N_ELEMENTS (TAGS_GEXIV2); ++n)
    {
      label = gtk_label_new (_(TAGS_GEXIV2[n].name));
      gtk_label_set_xalign (GTK_LABEL (label), 1.0f);
      gtk_label_set_attributes (GTK_LABEL (label), attr_list);
      gtk_grid_attach (GTK_GRID (grid), label, 0, n + 3, 1, 1);
      gtk_widget_show (label);

      image_page->gexiv2_labels[n] = gtk_label_new ("");
      gtk_label_set_selectable (GTK_LABEL (image_page->gexiv2_labels[n]), TRUE);
      gtk_label_set_xalign (GTK_LABEL (image_page->gexiv2_labels[n]), 0.0f);
      gtk_label_set_ellipsize (GTK_LABEL (image_page->gexiv2_labels[n]), PANGO_ELLIPSIZE_END);
      gtk_widget_set_hexpand (image_page->gexiv2_labels[n], TRUE);
      gtk_grid_attach (GTK_GRID (grid), image_page->gexiv2_labels[n], 1, n + 3, 1, 1);
      gtk_widget_show (image_page->gexiv2_labels[n]);

      g_object_bind_property (G_OBJECT (image_page->gexiv2_labels[n]), "visible", G_OBJECT (label), "visible", G_BINDING_SYNC_CREATE);

      /* set Atk label relation for the label */
      object = gtk_widget_get_accessible (image_page->gexiv2_labels[n]);
      relations = atk_object_ref_relation_set (gtk_widget_get_accessible (label));
      relation = atk_relation_new (&object, 1, ATK_RELATION_LABEL_FOR);
      atk_relation_set_add (relations, relation);
      g_object_unref (G_OBJECT (relation));
      g_object_unref (relations);
    }
#endif

  /* release shared bold Pango attributes */
  pango_attr_list_unref (attr_list);
}



static void
thunar_apr_image_page_file_changed (ThunarAprAbstractPage *abstract_page,
                                    ThunarxFileInfo       *file)
{
  ThunarAprImagePage *image_page = THUNAR_APR_IMAGE_PAGE (abstract_page);
  GdkPixbufFormat    *format;
  gchar              *filename;
  gchar              *text;
  gchar              *uri;
  gint                height;
  gint                width;
#ifdef HAVE_GEXIV2
  GExiv2Metadata     *metadata;
  guint               n;
#endif

  /* determine the URI for the file */
  uri = thunarx_file_info_get_uri (file);
  if (G_UNLIKELY (uri == NULL))
    return;

  /* determine the local path of the file */
  filename = g_filename_from_uri (uri, NULL, NULL);
  if (G_LIKELY (filename != NULL))
    {
      /* determine the pixbuf format */
      format = gdk_pixbuf_get_file_info (filename, &width, &height);
      if (G_LIKELY (format != NULL))
        {
          /* update the "Image Type" label */
          gchar *name = gdk_pixbuf_format_get_name (format);
          gchar *desc = gdk_pixbuf_format_get_description (format);
          text = g_strdup_printf ("%s (%s)", name, desc);

          gtk_label_set_text (GTK_LABEL (image_page->type_label), text);
          g_free (name);
          g_free (desc);
          g_free (text);

          /* update the "Image Size" label */
          text = g_strdup_printf (ngettext ("%dx%d pixel", "%dx%d pixels", width + height), width, height);
          gtk_label_set_text (GTK_LABEL (image_page->dimensions_label), text);
          g_free (text);

#ifdef HAVE_GEXIV2
          /* hide all gexiv2 labels (will be shown again if data is available) */
          for (n = 0; n < G_N_ELEMENTS (TAGS_GEXIV2); ++n)
            gtk_widget_hide (image_page->gexiv2_labels[n]);

          metadata = gexiv2_metadata_new ();
          if (G_LIKELY (gexiv2_metadata_open_path (metadata, filename, NULL)))
            {
              for (n = 0; n < G_N_ELEMENTS (TAGS_GEXIV2); ++n)
                {
                  gchar *value = gexiv2_metadata_try_get_tag_interpreted_string (metadata, TAGS_GEXIV2[n].tag, NULL);
                  if (value)
                    {
                      gtk_label_set_text (GTK_LABEL (image_page->gexiv2_labels[n]), value);
                      gtk_widget_show (image_page->gexiv2_labels[n]);
                      g_free (value);
                    }
                }
            }

          g_object_unref (metadata);
#endif
        }
      else
        {
          /* tell the user that we're unable to determine the file info */
          gtk_label_set_text (GTK_LABEL (image_page->type_label), _("Unknown"));
          gtk_label_set_text (GTK_LABEL (image_page->dimensions_label), _("Unknown"));

#ifdef HAVE_GEXIV2
          /* hide all gexiv2 labels */
          for (n = 0; n < G_N_ELEMENTS (TAGS_GEXIV2); ++n)
            gtk_widget_hide (image_page->gexiv2_labels[n]);
#endif
        }
    }

  /* cleanup */
  g_free (filename);
  g_free (uri);
}
