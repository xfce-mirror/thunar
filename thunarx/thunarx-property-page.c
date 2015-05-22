/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#include <glib/gi18n-lib.h>

#include <thunarx/thunarx-private.h>
#include <thunarx/thunarx-property-page.h>



#define THUNARX_PROPERTY_PAGE_GET_PRIVATE(obj) (G_TYPE_INSTANCE_GET_PRIVATE ((obj), THUNARX_TYPE_PROPERTY_PAGE, ThunarxPropertyPagePrivate))



/* Property identifiers */
enum
{
  PROP_0,
  PROP_LABEL,
  PROP_LABEL_WIDGET,
};



static void thunarx_property_page_get_property  (GObject                  *object,
                                                 guint                     prop_id,
                                                 GValue                   *value,
                                                 GParamSpec               *pspec);
static void thunarx_property_page_set_property  (GObject                  *object,
                                                 guint                     prop_id,
                                                 const GValue             *value,
                                                 GParamSpec               *pspec);
static void thunarx_property_page_destroy       (GtkObject                *object);
static void thunarx_property_page_size_request  (GtkWidget                *widget,
                                                 GtkRequisition           *requisition);
static void thunarx_property_page_size_allocate (GtkWidget                *widget,
                                                 GtkAllocation            *allocation);



struct _ThunarxPropertyPagePrivate
{
  GtkWidget *label_widget;
};



G_DEFINE_TYPE (ThunarxPropertyPage, thunarx_property_page, GTK_TYPE_BIN)



static void
thunarx_property_page_class_init (ThunarxPropertyPageClass *klass)
{
  GtkObjectClass *gtkobject_class;
  GtkWidgetClass *gtkwidget_class;
  GObjectClass   *gobject_class;

  /* add our private data to the class type */
  g_type_class_add_private (klass, sizeof (ThunarxPropertyPagePrivate));

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->get_property = thunarx_property_page_get_property;
  gobject_class->set_property = thunarx_property_page_set_property;

  gtkobject_class = GTK_OBJECT_CLASS (klass);
  gtkobject_class->destroy = thunarx_property_page_destroy;

  gtkwidget_class = GTK_WIDGET_CLASS (klass);
  gtkwidget_class->size_request = thunarx_property_page_size_request;
  gtkwidget_class->size_allocate = thunarx_property_page_size_allocate;

  /**
   * ThunarxPropertyPage::label:
   *
   * Text of the page's label.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LABEL,
                                   g_param_spec_string ("label",
                                                        _("Label"),
                                                        _("Text of the page's label"),
                                                        NULL,
                                                        G_PARAM_READWRITE));

  /**
   * ThunarxPropertyPage::label-widget:
   *
   * A widget to display in place of the usual page label.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_LABEL_WIDGET,
                                   g_param_spec_object ("label-widget",
                                                        _("Label widget"),
                                                        _("A widget to display in place of the usual page label"),
                                                        GTK_TYPE_WIDGET,
                                                        G_PARAM_READWRITE));
}



static void
thunarx_property_page_init (ThunarxPropertyPage *property_page)
{
  property_page->priv = THUNARX_PROPERTY_PAGE_GET_PRIVATE (property_page);
}



static void
thunarx_property_page_get_property (GObject    *object,
                                    guint       prop_id,
                                    GValue     *value,
                                    GParamSpec *pspec)
{
  ThunarxPropertyPage *property_page = THUNARX_PROPERTY_PAGE (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      g_value_set_string (value, thunarx_property_page_get_label (property_page));
      break;

    case PROP_LABEL_WIDGET:
      g_value_set_object (value, thunarx_property_page_get_label_widget (property_page));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunarx_property_page_set_property (GObject      *object,
                                    guint         prop_id,
                                    const GValue *value,
                                    GParamSpec   *pspec)
{
  ThunarxPropertyPage *property_page = THUNARX_PROPERTY_PAGE (object);

  switch (prop_id)
    {
    case PROP_LABEL:
      thunarx_property_page_set_label (property_page, g_value_get_string (value));
      break;

    case PROP_LABEL_WIDGET:
      thunarx_property_page_set_label_widget (property_page, g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunarx_property_page_destroy (GtkObject *object)
{
  ThunarxPropertyPage *property_page = THUNARX_PROPERTY_PAGE (object);

  /* destroy the label widget (if any) */
  if (G_LIKELY (property_page->priv->label_widget != NULL))
    {
      gtk_object_destroy (GTK_OBJECT (property_page->priv->label_widget));
      g_object_unref (G_OBJECT (property_page->priv->label_widget));
      property_page->priv->label_widget = NULL;
    }

  (*GTK_OBJECT_CLASS (thunarx_property_page_parent_class)->destroy) (object);
}



static void
thunarx_property_page_size_request (GtkWidget      *widget,
                                    GtkRequisition *requisition)
{
  GtkBin *bin = GTK_BIN (widget);

  if (G_LIKELY (bin->child != NULL && gtk_widget_get_visible (bin->child)))
    {
      gtk_widget_size_request (bin->child, requisition);
    }
  else
    {
      requisition->width = 0;
      requisition->height = 0;
    }

  requisition->width += 2 * (GTK_CONTAINER (bin)->border_width + widget->style->xthickness);
  requisition->height += 2 * (GTK_CONTAINER (bin)->border_width + widget->style->ythickness);
}



static void
thunarx_property_page_size_allocate (GtkWidget     *widget,
                                     GtkAllocation *allocation)
{
  GtkAllocation child_allocation;
  GtkBin       *bin = GTK_BIN (widget);

  /* apply the allocation to the property page */
  widget->allocation = *allocation;

  /* apply the child allocation if we have a child */
  if (G_LIKELY (bin->child != NULL && gtk_widget_get_visible (bin->child)))
    {
      /* calculate the allocation for the child widget */
      child_allocation.x = allocation->x + GTK_CONTAINER (bin)->border_width + widget->style->xthickness;
      child_allocation.y = allocation->y + GTK_CONTAINER (bin)->border_width + widget->style->ythickness;
      child_allocation.width = allocation->width - 2 * (GTK_CONTAINER (bin)->border_width + widget->style->xthickness);
      child_allocation.height = allocation->height - 2 * (GTK_CONTAINER (bin)->border_width + widget->style->ythickness);

      /* apply the child allocation */
      gtk_widget_size_allocate (bin->child, &child_allocation);
    }
}



/**
 * thunarx_property_page_new:
 * @label : the text to use as the label of the page.
 *
 * Allocates a new #ThunarxPropertyPage widget and sets its label to the
 * specified @label. If @label is %NULL, the label is omitted.
 *
 * Return value: the newly allocated #ThunarxPropertyPage
 *               widget.
 **/
GtkWidget*
thunarx_property_page_new (const gchar *label)
{
  return g_object_new (THUNARX_TYPE_PROPERTY_PAGE, "label", label, NULL);
}



/**
 * thunarx_property_page_new_with_label_widget:
 * @label_widget : a #GtkWidget, which should be used as label.
 *
 * Allocates a new #ThunarxPropertyPage widget and sets its label to
 * the specified @label_widget.
 *
 * Return value: the newly allocated #ThunarxPropertyPage widget.
 **/
GtkWidget*
thunarx_property_page_new_with_label_widget (GtkWidget *label_widget)
{
  return g_object_new (THUNARX_TYPE_PROPERTY_PAGE, "label-widget", label_widget, NULL);
}



/**
 * thunarx_property_page_get_label:
 * @property_page : a #ThunarxPropertyPage.
 *
 * If the @property_page's label widget is a #GtkLabel, returns the text
 * in the label widget (the @property_page will have a #GtkLabel for the
 * label widget if a non-%NULL argument was passed to thunarx_property_page_new()).
 *
 * Return value: the text in the label or %NULL if there was no label widget or
 *               the label widget was not a #GtkLabel. The returned string is
 *               owned by the @property_page and must not be modified or freed.
 **/
const gchar*
thunarx_property_page_get_label (ThunarxPropertyPage *property_page)
{
  g_return_val_if_fail (THUNARX_IS_PROPERTY_PAGE (property_page), NULL);

  if (property_page->priv->label_widget != NULL && GTK_IS_LABEL (property_page->priv->label_widget))
    return gtk_label_get_text (GTK_LABEL (property_page->priv->label_widget));
  else
    return NULL;
}



/**
 * thunarx_property_page_set_label:
 * @property_page : a #ThunarxPropertyPage.
 * @label         : the text to use as the label of the page.
 *
 * Sets the text of the label. If @label is %NULL, the current label is
 * removed.
 **/
void
thunarx_property_page_set_label (ThunarxPropertyPage *property_page,
                                 const gchar         *label)
{
  GtkWidget *widget;

  g_return_if_fail (THUNARX_IS_PROPERTY_PAGE (property_page));

  if (label == NULL)
    {
      thunarx_property_page_set_label_widget (property_page, NULL);
    }
  else
    {
      widget = gtk_label_new (label);
      thunarx_property_page_set_label_widget (property_page, widget);
      gtk_widget_show (widget);
    }
}



/**
 * thunarx_property_page_get_label_widget:
 * @property_page : a #ThunarxPropertyPage.
 *
 * Returns the label widget for the @property_page. See
 * thunarx_property_page_set_label_widget().
 *
 * return value: the label widget or %NULL if there is none.
 **/
GtkWidget*
thunarx_property_page_get_label_widget (ThunarxPropertyPage *property_page)
{
  g_return_val_if_fail (THUNARX_IS_PROPERTY_PAGE (property_page), NULL);
  return property_page->priv->label_widget;
}



/**
 * thunarx_property_page_set_label_widget:
 * @property_page : a #ThunarxPropertyPage.
 * @label_widget  : the new label widget.
 *
 * Sets the label widget for the @property_page. This is the widget
 * that will appear in the notebook header for the @property_page.
 **/
void
thunarx_property_page_set_label_widget (ThunarxPropertyPage *property_page,
                                        GtkWidget           *label_widget)
{
  g_return_if_fail (THUNARX_IS_PROPERTY_PAGE (property_page));
  g_return_if_fail (label_widget == NULL || (GTK_IS_WIDGET (label_widget) && label_widget->parent == NULL));

  if (G_UNLIKELY (label_widget == property_page->priv->label_widget))
    return;

  /* disconnect from the previous label widget */
  if (G_LIKELY (property_page->priv->label_widget != NULL))
    g_object_unref (G_OBJECT (property_page->priv->label_widget));

  /* activate the new label widget */
  property_page->priv->label_widget = label_widget;

  /* connect to the new label widget */
  if (G_LIKELY (label_widget != NULL))
    g_object_ref_sink (G_OBJECT (label_widget));

  /* notify listeners */
  g_object_freeze_notify (G_OBJECT (property_page));
  g_object_notify (G_OBJECT (property_page), "label");
  g_object_notify (G_OBJECT (property_page), "label-widget");
  g_object_thaw_notify (G_OBJECT (property_page));
}
