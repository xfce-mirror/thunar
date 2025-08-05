/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2025 Bruno Gonçalves <biglinux@biglinux.com.br>
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

#ifndef __THUNAR_TERMINAL_WIDGET_H__
#define __THUNAR_TERMINAL_WIDGET_H__

#include <gio/gio.h>
#include <gtk/gtk.h>

#ifdef HAVE_VTE
#include <vte/vte.h>
#endif

G_BEGIN_DECLS

#define THUNAR_TYPE_TERMINAL_WIDGET (thunar_terminal_widget_get_type ())
#define THUNAR_TERMINAL_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_TYPE_TERMINAL_WIDGET, ThunarTerminalWidget))
#define THUNAR_TERMINAL_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_TYPE_TERMINAL_WIDGET, ThunarTerminalWidgetClass))
#define THUNAR_IS_TERMINAL_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_TYPE_TERMINAL_WIDGET))
#define THUNAR_IS_TERMINAL_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_TYPE_TERMINAL_WIDGET))
#define THUNAR_TERMINAL_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_TYPE_TERMINAL_WIDGET, ThunarTerminalWidgetClass))

typedef struct _ThunarTerminalWidget        ThunarTerminalWidget;
typedef struct _ThunarTerminalWidgetClass   ThunarTerminalWidgetClass;
typedef struct _ThunarTerminalWidgetPrivate ThunarTerminalWidgetPrivate;

/*
 * ThunarTerminalState:
 * @THUNAR_TERMINAL_STATE_LOCAL:  The terminal is running a local shell.
 * @THUNAR_TERMINAL_STATE_IN_SSH: The terminal is in an active SSH session.
 *
 * Represents the operational state of the terminal widget.
 */
typedef enum
{
  THUNAR_TERMINAL_STATE_LOCAL,
  THUNAR_TERMINAL_STATE_IN_SSH,
} ThunarTerminalState;

struct _ThunarTerminalWidget
{
  GtkBox                       parent_instance;
  ThunarTerminalWidgetPrivate *priv;
};

struct _ThunarTerminalWidgetClass
{
  GtkBoxClass parent_class;
};

GType
thunar_terminal_widget_get_type (void) G_GNUC_CONST;

ThunarTerminalWidget *
thunar_terminal_widget_new (void);

void
thunar_terminal_widget_check_spawn_needed (ThunarTerminalWidget *self);

G_END_DECLS

#endif /* !__THUNAR_TERMINAL_WIDGET_H__ */
