/*
 * @file libsexy/sexy-url-label.h URL Label
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
#ifndef _SEXY_URL_LABEL_H_
#define _SEXY_URL_LABEL_H_

typedef struct _SexyUrlLabel      SexyUrlLabel;
typedef struct _SexyUrlLabelClass SexyUrlLabelClass;

#include <gtk/gtk.h>

#define SEXY_TYPE_URL_LABEL (sexy_url_label_get_type())
#define SEXY_URL_LABEL(obj) \
		(G_TYPE_CHECK_INSTANCE_CAST((obj), SEXY_TYPE_URL_LABEL, SexyUrlLabel))
#define SEXY_URL_LABEL_CLASS(klass) \
		(G_TYPE_CHECK_CLASS_CAST((klass), SEXY_TYPE_URL_LABEL, SexyUrlLabelClass))
#define SEXY_IS_URL_LABEL(obj) \
		(G_TYPE_CHECK_INSTANCE_TYPE((obj), SEXY_TYPE_URL_LABEL))
#define SEXY_IS_URL_LABEL_CLASS(klass) \
		(G_TYPE_CHECK_CLASS_TYPE((klass), SEXY_TYPE_URL_LABEL))
#define SEXY_URL_LABEL_GET_CLASS(obj) \
		(G_TYPE_INSTANCE_GET_CLASS ((obj), SEXY_TYPE_URL_LABEL, SexyUrlLabelClass))

struct _SexyUrlLabel
{
	GtkLabel parent_object;

	void (*gtk_reserved1)(void);
	void (*gtk_reserved2)(void);
	void (*gtk_reserved3)(void);
	void (*gtk_reserved4)(void);
};

struct _SexyUrlLabelClass
{
	GtkLabelClass parent_class;

	/* Signals */
	void (*url_activated)(SexyUrlLabel *url_label, const gchar *url);

	void (*gtk_reserved1)(void);
	void (*gtk_reserved2)(void);
	void (*gtk_reserved3)(void);
	void (*gtk_reserved4)(void);
};

G_BEGIN_DECLS

GType sexy_url_label_get_type(void);

GtkWidget *sexy_url_label_new(void);
void sexy_url_label_set_markup(SexyUrlLabel *url_label, const gchar *markup);

G_END_DECLS

#endif /* _SEXY_URL_LABEL_H_ */
