/*
 * @file libsexy/sexy-url-label.c URL Label
 *
 * @Copyright (C) 2007      Benedikt Meurer <benny@xfce.org>
 * @Copyright (C) 2005-2006 Christian Hammond
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA  02111-1307, USA.
 */

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include "sexy-url-label.h"

#define SEXY_URL_LABEL_GET_PRIVATE(obj) \
	(G_TYPE_INSTANCE_GET_PRIVATE((obj), SEXY_TYPE_URL_LABEL, \
								 SexyUrlLabelPrivate))

typedef struct
{
	int start;
	int end;
	const gchar *url;

} SexyUrlLabelLink;

typedef struct
{
	GList *links;
	GList *urls;
	SexyUrlLabelLink *active_link;
	GtkWidget *popup_menu;
	GdkWindow *event_window;

	int layout_x;
	int layout_y;

	size_t wrap_width;

	GString *temp_markup_result;

} SexyUrlLabelPrivate;

/*
 * NOTE: This *MUST* match the LabelWrapWidth struct in gtklabel.c.
 */
typedef struct
{
	gint width;
	PangoFontDescription *font_desc;

} LabelWrapWidth;

enum
{
	URL_ACTIVATED,
	LAST_SIGNAL
};

static void sexy_url_label_finalize(GObject *obj);
static void sexy_url_label_realize(GtkWidget *widget);
static void sexy_url_label_unrealize(GtkWidget *widget);
static void sexy_url_label_map(GtkWidget *widget);
static void sexy_url_label_unmap(GtkWidget *widget);
static void sexy_url_label_size_allocate(GtkWidget *widget,
										 GtkAllocation *allocation);
static gboolean sexy_url_label_motion_notify_event(GtkWidget *widget,
												   GdkEventMotion *event);
static gboolean sexy_url_label_leave_notify_event(GtkWidget *widget,
												  GdkEventCrossing *event);
static gboolean sexy_url_label_button_press_event(GtkWidget *widget,
												  GdkEventButton *event);

static void open_link_activate_cb(GtkMenuItem *menu_item,
								  SexyUrlLabel *url_label);
static void copy_link_activate_cb(GtkMenuItem *menu_item,
								  SexyUrlLabel *url_label);

static void sexy_url_label_clear_links(SexyUrlLabel *url_label);
static void sexy_url_label_clear_urls(SexyUrlLabel *url_label);
static void sexy_url_label_rescan_label(SexyUrlLabel *url_label);

static guint signals[LAST_SIGNAL] = {0};

G_DEFINE_TYPE(SexyUrlLabel, sexy_url_label, GTK_TYPE_LABEL);

static void
sexy_url_label_class_init(SexyUrlLabelClass *klass)
{
	GObjectClass   *object_class = G_OBJECT_CLASS(klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS(klass);

	object_class->finalize = sexy_url_label_finalize;

	widget_class->realize             = sexy_url_label_realize;
	widget_class->unrealize           = sexy_url_label_unrealize;
	widget_class->map                 = sexy_url_label_map;
	widget_class->unmap               = sexy_url_label_unmap;
	widget_class->size_allocate       = sexy_url_label_size_allocate;
	widget_class->motion_notify_event = sexy_url_label_motion_notify_event;
	widget_class->leave_notify_event  = sexy_url_label_leave_notify_event;
	widget_class->button_press_event  = sexy_url_label_button_press_event;

	g_type_class_add_private(klass, sizeof(SexyUrlLabelPrivate));

	/**
	 * SexyUrlLabel::url-activated:
	 * @url_label: The label on which the signal was emitted.
	 * @url: The URL which was activated.
	 *
	 * The ::url-activated signal is emitted when a URL in the label was
	 * clicked.
	 */
	signals[URL_ACTIVATED] =
		g_signal_new("url_activated",
					 G_TYPE_FROM_CLASS(object_class),
					 G_SIGNAL_RUN_LAST | G_SIGNAL_ACTION,
					 G_STRUCT_OFFSET(SexyUrlLabelClass, url_activated),
					 NULL, NULL,
					 g_cclosure_marshal_VOID__STRING,
					 G_TYPE_NONE, 1,
					 G_TYPE_STRING);
}

static void
selectable_changed_cb(SexyUrlLabel *url_label)
{
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

	if (priv->event_window != NULL)
		gdk_window_raise(priv->event_window);
}

static void
sexy_url_label_init(SexyUrlLabel *url_label)
{
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);
	GtkWidget *item;
	GtkWidget *image;

	priv->links        = NULL;
	priv->active_link  = NULL;
	priv->event_window = NULL;

	g_signal_connect(G_OBJECT(url_label), "notify::selectable",
					 G_CALLBACK(selectable_changed_cb), NULL);

	priv->popup_menu = gtk_menu_new();

	/* Open Link */
	item = gtk_image_menu_item_new_with_mnemonic("_Open Link");
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->popup_menu), item);

	g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(open_link_activate_cb), url_label);

	image = gtk_image_new_from_stock(GTK_STOCK_JUMP_TO, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(image);

	/* Copy Link Address */
	item = gtk_image_menu_item_new_with_mnemonic("Copy _Link Address");
	gtk_widget_show(item);
	gtk_menu_shell_append(GTK_MENU_SHELL(priv->popup_menu), item);

	g_signal_connect(G_OBJECT(item), "activate",
					 G_CALLBACK(copy_link_activate_cb), url_label);

	image = gtk_image_new_from_stock(GTK_STOCK_COPY, GTK_ICON_SIZE_MENU);
	gtk_image_menu_item_set_image(GTK_IMAGE_MENU_ITEM(item), image);
	gtk_widget_show(image);
}

static void
sexy_url_label_finalize(GObject *obj)
{
	SexyUrlLabel *url_label = SEXY_URL_LABEL(obj);

	sexy_url_label_clear_links(url_label);
	sexy_url_label_clear_urls(url_label);

	if (G_OBJECT_CLASS(sexy_url_label_parent_class)->finalize != NULL)
		G_OBJECT_CLASS(sexy_url_label_parent_class)->finalize(obj);
}

static gboolean
sexy_url_label_motion_notify_event(GtkWidget *widget, GdkEventMotion *event)
{
	SexyUrlLabel *url_label = (SexyUrlLabel *)widget;
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);
	PangoLayout *layout = gtk_label_get_layout(GTK_LABEL(url_label));
	GdkModifierType state;
	gboolean found = FALSE;
	GList *l;
	int idx, trailing;
	int x, y;
	SexyUrlLabelLink *llink = NULL;

	if (event->is_hint)
		gdk_window_get_pointer(event->window, &x, &y, &state);
	else
	{
		x = event->x;
		y = event->y;
		state = event->state;
	}

	if (pango_layout_xy_to_index(layout,
								 (x - priv->layout_x) * PANGO_SCALE,
								 (y - priv->layout_y) * PANGO_SCALE,
								 &idx, &trailing))
	{
		for (l = priv->links; l != NULL; l = l->next)
		{
			llink = (SexyUrlLabelLink *)l->data;

			if (idx >= llink->start && idx <= llink->end)
			{
				found = TRUE;
				break;
			}
		}
	}

	if (found)
	{
		if (priv->active_link == NULL)
		{
			GdkCursor *cursor;

			cursor = gdk_cursor_new_for_display(
				gtk_widget_get_display(widget), GDK_HAND2);
			gdk_window_set_cursor(priv->event_window, cursor);
			gdk_cursor_unref(cursor);

			priv->active_link = llink;
		}
	}
	else
	{
		if (priv->active_link != NULL)
		{
			if (gtk_label_get_selectable(GTK_LABEL(url_label)))
			{
				GdkCursor *cursor;

				cursor = gdk_cursor_new_for_display(
					gtk_widget_get_display(widget), GDK_XTERM);
				gdk_window_set_cursor(priv->event_window, cursor);
				gdk_cursor_unref(cursor);
			}
			else
				gdk_window_set_cursor(priv->event_window, NULL);

			priv->active_link = NULL;
		}
	}

	/*
	 * Another beautiful libsexy hack. This one prevents the callback
	 * from going "Oh boy, they clicked and dragged! Let's select more of
	 * the text!"
	 */
	if (priv->active_link != NULL)
		event->state = 0;

	GTK_WIDGET_CLASS(sexy_url_label_parent_class)->motion_notify_event(widget, event);

	return FALSE;
}

static gboolean
sexy_url_label_leave_notify_event(GtkWidget *widget, GdkEventCrossing *event)
{
	SexyUrlLabel *url_label = (SexyUrlLabel *)widget;
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

	if (GTK_WIDGET_CLASS(sexy_url_label_parent_class)->leave_notify_event != NULL)
		GTK_WIDGET_CLASS(sexy_url_label_parent_class)->leave_notify_event(widget, event);

	if (event->mode == GDK_CROSSING_NORMAL)
	{
		gdk_window_set_cursor(priv->event_window, NULL);
		priv->active_link = NULL;
	}

	return FALSE;
}

static gboolean
sexy_url_label_button_press_event(GtkWidget *widget, GdkEventButton *event)
{
	SexyUrlLabel *url_label = (SexyUrlLabel *)widget;
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

	if (priv->active_link == NULL)
	{
		return GTK_WIDGET_CLASS(sexy_url_label_parent_class)->button_press_event(widget,
																  event);
	}

	if (event->button == 1)
	{
		g_signal_emit(url_label, signals[URL_ACTIVATED], 0,
					  priv->active_link->url);
	}
	else if (event->button == 3)
	{
		gtk_menu_popup(GTK_MENU(priv->popup_menu), NULL, NULL, NULL, NULL,
					   event->button, event->time);
	}

	return TRUE;
}

static void
update_wrap_width(SexyUrlLabel *url_label, size_t wrap_width)
{
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);
	LabelWrapWidth *wrap_width_data;
	GtkStyle *style;

	if (wrap_width == 0 || !gtk_label_get_line_wrap(GTK_LABEL(url_label)))
		return;

#if 0
	pango_layout_set_width(gtk_label_get_layout(GTK_LABEL(url_label)),
						   wrap_width * PANGO_SCALE);
#endif
	style = GTK_WIDGET(url_label)->style;
	wrap_width_data = g_object_get_data(G_OBJECT(style),
										"gtk-label-wrap-width");

	if (wrap_width_data != NULL &&
		(size_t) wrap_width_data->width != wrap_width * PANGO_SCALE)
	{
		wrap_width_data->width = wrap_width * PANGO_SCALE;
		priv->wrap_width = wrap_width;
		g_object_unref(GTK_LABEL(url_label)->layout);
		GTK_LABEL(url_label)->layout = NULL;
		gtk_label_get_layout(GTK_LABEL(url_label));
		gtk_widget_queue_resize(GTK_WIDGET(url_label));
	}
}

static void
sexy_url_label_realize(GtkWidget *widget)
{
	SexyUrlLabel *url_label = (SexyUrlLabel *)widget;
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);
	GdkWindowAttr attributes;
	gint attributes_mask;

	GTK_WIDGET_CLASS(sexy_url_label_parent_class)->realize(widget);

	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.x = widget->allocation.x;
	attributes.y = widget->allocation.y;
	attributes.width = widget->allocation.width;
	attributes.height = widget->allocation.height;
	attributes.window_type = GDK_WINDOW_CHILD;
	attributes.wclass = GDK_INPUT_ONLY;
	attributes.event_mask = gtk_widget_get_events(widget);
	attributes.event_mask |= (GDK_BUTTON_PRESS_MASK |
							  GDK_BUTTON_RELEASE_MASK |
							  GDK_POINTER_MOTION_MASK |
							  GDK_POINTER_MOTION_HINT_MASK |
							  GDK_LEAVE_NOTIFY_MASK |
							  GDK_LEAVE_NOTIFY_MASK);
	attributes_mask = GDK_WA_X | GDK_WA_Y;

	priv->event_window =
		gdk_window_new(gtk_widget_get_parent_window(widget), &attributes,
					   attributes_mask);
	gdk_window_set_user_data(priv->event_window, widget);
}

static void
sexy_url_label_unrealize(GtkWidget *widget)
{
	SexyUrlLabel *url_label = (SexyUrlLabel *)widget;
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

	if (priv->event_window != NULL)
	{
		gdk_window_set_user_data(priv->event_window, NULL);
		gdk_window_destroy(priv->event_window);
		priv->event_window = NULL;
	}

	GTK_WIDGET_CLASS(sexy_url_label_parent_class)->unrealize(widget);
}

static void
sexy_url_label_map(GtkWidget *widget)
{
	SexyUrlLabel *url_label = (SexyUrlLabel *)widget;
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

	GTK_WIDGET_CLASS(sexy_url_label_parent_class)->map(widget);

	if (priv->event_window != NULL)
		gdk_window_show(priv->event_window);
}

static void
sexy_url_label_unmap(GtkWidget *widget)
{
	SexyUrlLabel *url_label = (SexyUrlLabel *)widget;
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

	if (priv->event_window != NULL)
		gdk_window_hide(priv->event_window);

	GTK_WIDGET_CLASS(sexy_url_label_parent_class)->map(widget);
}

static void
sexy_url_label_size_allocate(GtkWidget *widget, GtkAllocation *allocation)
{
	SexyUrlLabel *url_label = (SexyUrlLabel *)widget;
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

#if 0
	{
		LabelWrapWidth *wrap_width_data;
		GtkStyle *style;
		style = GTK_WIDGET(url_label)->style;
		wrap_width_data = g_object_get_data(G_OBJECT(style),
											"gtk-label-wrap-width");
		if (wrap_width_data != NULL)
			printf("wrap width = %d\n", wrap_width_data->width / PANGO_SCALE);
	}
#endif
	update_wrap_width(url_label, allocation->width);
	GTK_WIDGET_CLASS(sexy_url_label_parent_class)->size_allocate(widget, allocation);
	pango_layout_set_width(gtk_label_get_layout(GTK_LABEL(url_label)),
						   allocation->width * PANGO_SCALE);

	if (GTK_WIDGET_REALIZED(widget))
	{
		gdk_window_move_resize(priv->event_window,
							   allocation->x, allocation->y,
							   allocation->width, allocation->height);
	}

	sexy_url_label_rescan_label(url_label);
}

static void
open_link_activate_cb(GtkMenuItem *menu_item, SexyUrlLabel *url_label)
{
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

	if (priv->active_link == NULL)
		return;

	g_signal_emit(url_label, signals[URL_ACTIVATED], 0, priv->active_link->url);
}

static void
copy_link_activate_cb(GtkMenuItem *menu_item, SexyUrlLabel *url_label)
{
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);
	GtkClipboard *clipboard;

	if (priv->active_link == NULL)
		return;

	clipboard = gtk_widget_get_clipboard(GTK_WIDGET(url_label),
										 GDK_SELECTION_PRIMARY);

	gtk_clipboard_set_text(clipboard, priv->active_link->url,
						   strlen(priv->active_link->url));
}

/**
 * sexy_url_label_new
 *
 * Creates a new SexyUrlLabel widget.
 *
 * Returns: a new #SexyUrlLabel.
 */
GtkWidget *
sexy_url_label_new(void)
{
	return g_object_new(SEXY_TYPE_URL_LABEL, NULL);
}

static void
sexy_url_label_clear_links(SexyUrlLabel *url_label)
{
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

	if (priv->links == NULL)
		return;

	g_list_foreach(priv->links, (GFunc)g_free, NULL);
	g_list_free(priv->links);
	priv->links = NULL;
}

static void
sexy_url_label_clear_urls(SexyUrlLabel *url_label)
{
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

	if (priv->urls == NULL)
		return;

	g_list_foreach(priv->urls, (GFunc)g_free, NULL);
	g_list_free(priv->urls);
	priv->urls = NULL;
}

static void
sexy_url_label_rescan_label(SexyUrlLabel *url_label)
{
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);
	PangoLayout *layout = gtk_label_get_layout(GTK_LABEL(url_label));
	PangoAttrList *list = pango_layout_get_attributes(layout);
	PangoAttrIterator *iter;
	GList *url_list;

	sexy_url_label_clear_links(url_label);

	if (list == NULL)
		return;

	iter = pango_attr_list_get_iterator(list);

	gtk_label_get_layout_offsets(GTK_LABEL(url_label),
								 &priv->layout_x, &priv->layout_y);

	priv->layout_x -= GTK_WIDGET(url_label)->allocation.x;
	priv->layout_y -= GTK_WIDGET(url_label)->allocation.y;

	url_list = priv->urls;

	do
	{
		PangoAttribute *underline;
		PangoAttribute *color;

		underline = pango_attr_iterator_get(iter, PANGO_ATTR_UNDERLINE);
		color     = pango_attr_iterator_get(iter, PANGO_ATTR_FOREGROUND);

		if (underline != NULL && color != NULL)
		{
			gint start, end;
			PangoRectangle start_pos;
			PangoRectangle end_pos;
			SexyUrlLabelLink *llink;

			pango_attr_iterator_range(iter, &start, &end);
			pango_layout_index_to_pos(layout, start, &start_pos);
			pango_layout_index_to_pos(layout, end,   &end_pos);

			llink = g_new0(SexyUrlLabelLink, 1);
			llink->start = start;
			llink->end   = end;
			llink->url   = (const gchar *)url_list->data;
			priv->links = g_list_append(priv->links, llink);

			url_list = url_list->next;
		}

	} while (pango_attr_iterator_next(iter));

	pango_attr_iterator_destroy (iter);
}

static void
start_element_handler(GMarkupParseContext *context,
					  const gchar *element_name,
					  const gchar **attribute_names,
					  const gchar **attribute_values,
					  gpointer user_data,
					  GError **error)
{
	SexyUrlLabel *url_label   = SEXY_URL_LABEL(user_data);
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

	if (!strcmp(element_name, "a"))
	{
		const gchar *url = NULL;
		int line_number;
		int char_number;
		int i;

		g_markup_parse_context_get_position(context, &line_number,
											&char_number);

		for (i = 0; attribute_names[i] != NULL; i++)
		{
			const gchar *attr = attribute_names[i];

			if (!strcmp(attr, "href"))
			{
				if (url != NULL)
				{
					g_set_error(error, G_MARKUP_ERROR,
								G_MARKUP_ERROR_INVALID_CONTENT,
								"Attribute '%s' occurs twice on <a> tag "
								"on line %d char %d, may only occur once",
								attribute_names[i], line_number, char_number);
					return;
				}

				url = attribute_values[i];
			}
			else
			{
				g_set_error(error, G_MARKUP_ERROR,
							G_MARKUP_ERROR_UNKNOWN_ATTRIBUTE,
							"Attribute '%s' is not allowed on the <a> tag "
							"on line %d char %d",
							attribute_names[i], line_number, char_number);
				return;
			}
		}

		if (url == NULL)
		{
			g_set_error(error, G_MARKUP_ERROR,
						G_MARKUP_ERROR_INVALID_CONTENT,
						"Attribute 'href' was missing on the <a> tag "
						"on line %d char %d",
						line_number, char_number);
			return;
		}

		g_string_append(priv->temp_markup_result,
						"<span color=\"blue\" underline=\"single\">");

		priv->urls = g_list_append(priv->urls, g_strdup(url));
	}
	else
	{
		int i;

		g_string_append_printf(priv->temp_markup_result,
							   "<%s", element_name);

		for (i = 0; attribute_names[i] != NULL; i++)
		{
			const gchar *attr  = attribute_names[i];
			const gchar *value = attribute_values[i];

			g_string_append_printf(priv->temp_markup_result,
								   " %s=\"%s\"",
								   attr, value);
		}

		g_string_append_c(priv->temp_markup_result, '>');
	}
}

static void
end_element_handler(GMarkupParseContext *context,
					const gchar *element_name,
					gpointer user_data,
					GError **error)
{
	SexyUrlLabel *url_label   = SEXY_URL_LABEL(user_data);
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

	if (!strcmp(element_name, "a"))
	{
		g_string_append(priv->temp_markup_result, "</span>");
	}
	else
	{
		g_string_append_printf(priv->temp_markup_result,
							   "</%s>", element_name);
	}
}

static void
text_handler(GMarkupParseContext *context,
			 const gchar *text,
			 gsize text_len,
			 gpointer user_data,
			 GError **error)
{
	SexyUrlLabel *url_label   = SEXY_URL_LABEL(user_data);
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

	gchar *newtext = g_markup_escape_text (text, text_len);
	g_string_append_len(priv->temp_markup_result, newtext, strlen (newtext));
	g_free (newtext);
}

static const GMarkupParser markup_parser =
{
	start_element_handler,
	end_element_handler,
	text_handler,
	NULL,
	NULL
};

static gboolean
xml_isspace(char c)
{
	return (c == ' ' || c == '\t' || c == '\n' || c == '\r');
}

static gboolean
parse_custom_markup(SexyUrlLabel *url_label, const gchar *markup,
					gchar **ret_markup)
{
	GMarkupParseContext *context = NULL;
	SexyUrlLabelPrivate *priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);
	GError *error = NULL;
	const gchar *p, *end;
	gboolean needs_root = TRUE;
	gsize length;

	g_return_val_if_fail(markup     != NULL, FALSE);
	g_return_val_if_fail(ret_markup != NULL, FALSE);

	priv->temp_markup_result = g_string_new(NULL);

	length = strlen(markup);
	p = markup;
	end = markup + length;

	while (p != end && xml_isspace(*p))
		p++;

	if (end - p >= 8 && strncmp(p, "<markup>", 8) == 0)
		needs_root = FALSE;

	context = g_markup_parse_context_new(&markup_parser, 0, url_label, NULL);

	if (needs_root)
	{
		if (!g_markup_parse_context_parse(context, "<markup>", -1, &error))
			goto failed;
	}

	if (!g_markup_parse_context_parse(context, markup, strlen(markup), &error))
		goto failed;

	if (needs_root)
	{
		if (!g_markup_parse_context_parse(context, "</markup>", -1, &error))
			goto failed;
	}

	if (!g_markup_parse_context_end_parse(context, &error))
		goto failed;

	if (error != NULL)
		g_error_free(error);

	g_markup_parse_context_free(context);

	*ret_markup = g_string_free(priv->temp_markup_result, FALSE);
	priv->temp_markup_result = NULL;

	return TRUE;

failed:
	fprintf(stderr, "Unable to parse markup: %s\n", error->message);
	g_error_free(error);

	g_string_free(priv->temp_markup_result, TRUE);
	priv->temp_markup_result = NULL;

	g_markup_parse_context_free(context);
	return FALSE;
}

/**
 * sexy_url_label_set_markup
 * @url_label: A #SexyUrlLabel.
 * @markup: a markup string (see <link linkend="PangoMarkupFormat">Pango markup format</link>)
 *
 * Parses @markup which is marked up with the <link
 * linkend="PangoMarkupFormat">Pango text markup language</link> as well as
 * HTML-style hyperlinks, setting the label's text and attribute list based
 * on the parse results.  If the @markup is external data, you may need to
 * escape it with g_markup_escape_text() or g_markup_printf_escaped()
 */
void
sexy_url_label_set_markup(SexyUrlLabel *url_label, const gchar *markup)
{
	SexyUrlLabelPrivate *priv;
	gchar *new_markup;

	g_return_if_fail(SEXY_IS_URL_LABEL(url_label));

	priv = SEXY_URL_LABEL_GET_PRIVATE(url_label);

	sexy_url_label_clear_links(url_label);
	sexy_url_label_clear_urls(url_label);

	if (markup == NULL || *markup == '\0')
	{
		gtk_label_set_markup(GTK_LABEL(url_label), "");
		return;
	}

	if (parse_custom_markup(url_label, markup, &new_markup))
	{
		gtk_label_set_markup(GTK_LABEL(url_label), new_markup);
		g_free(new_markup);
	}
	else
	{
		gtk_label_set_markup(GTK_LABEL(url_label), "");
	}

	sexy_url_label_rescan_label(url_label);

	update_wrap_width(url_label, priv->wrap_width);
}
