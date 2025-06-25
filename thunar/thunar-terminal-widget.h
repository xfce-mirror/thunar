/* thunar-terminal-widget.h

  Copyright (C) 2025

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License as
  published by the Free Software Foundation; either version 2 of the
  License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

  You should have received a copy of the GNU General Public
  License along with this program; if not, see <http://www.gnu.org/licenses/>.

  Author: Bruno Goncalves <biglinux.com.br>
 */

#ifndef __THUNAR_TERMINAL_WIDGET_H__
#define __THUNAR_TERMINAL_WIDGET_H__

#include <gtk/gtk.h>
#include <gio/gio.h>
#include <vte/vte.h>

G_BEGIN_DECLS

typedef enum {
    THUNAR_TERMINAL_SYNC_NONE,
    THUNAR_TERMINAL_SYNC_FM_TO_TERM,
    THUNAR_TERMINAL_SYNC_TERM_TO_FM,
    THUNAR_TERMINAL_SYNC_BOTH
} ThunarTerminalSyncMode;

typedef enum {
    THUNAR_TERMINAL_SSH_AUTOCONNECT_OFF,
    THUNAR_TERMINAL_SSH_AUTOCONNECT_SYNC_BOTH,
    THUNAR_TERMINAL_SSH_AUTOCONNECT_SYNC_FM_TO_TERM,
    THUNAR_TERMINAL_SSH_AUTOCONNECT_SYNC_TERM_TO_FM,
    THUNAR_TERMINAL_SSH_AUTOCONNECT_SYNC_NONE
} ThunarTerminalSshAutoConnectMode;

#define THUNAR_TYPE_TERMINAL_WIDGET (thunar_terminal_widget_get_type())
#define THUNAR_TERMINAL_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_CAST((obj), THUNAR_TYPE_TERMINAL_WIDGET, ThunarTerminalWidget))
#define THUNAR_TERMINAL_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_CAST((klass), THUNAR_TYPE_TERMINAL_WIDGET, ThunarTerminalWidgetClass))
#define THUNAR_IS_TERMINAL_WIDGET(obj) (G_TYPE_CHECK_INSTANCE_TYPE((obj), THUNAR_TYPE_TERMINAL_WIDGET))
#define THUNAR_IS_TERMINAL_WIDGET_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE((klass), THUNAR_TYPE_TERMINAL_WIDGET))
#define THUNAR_TERMINAL_WIDGET_GET_CLASS(obj) (G_TYPE_INSTANCE_GET_CLASS((obj), THUNAR_TYPE_TERMINAL_WIDGET, ThunarTerminalWidgetClass))

typedef struct _ThunarTerminalWidget ThunarTerminalWidget;
typedef struct _ThunarTerminalWidgetClass ThunarTerminalWidgetClass;

struct _ThunarTerminalWidget {
    GtkBox parent_instance;

    GtkWidget *scrolled_window;
    VteTerminal *terminal;
    GtkWidget *ssh_indicator;
    GtkWidget *container_paned;
    GtkWidget *pane;  // placeholder for Thunar pane

    GSimpleActionGroup *action_group;

    gboolean is_visible;
    gboolean maintain_focus;
    gboolean in_toggling;
    gboolean needs_respawn;
    gboolean is_exiting_ssh;
    gboolean ssh_connecting;
    gboolean ignore_next_terminal_cd_signal;
    

    int height;
    guint focus_timeout_id;

    GFile *current_location;

    gchar *color_scheme;

    gboolean in_ssh_mode;
    ThunarTerminalSyncMode ssh_sync_mode;
    ThunarTerminalSyncMode pending_ssh_sync_mode;
    ThunarTerminalSshAutoConnectMode ssh_auto_connect_mode;
    gchar *ssh_hostname;
    gchar *ssh_username;
    gchar *ssh_port;
    gchar *ssh_remote_path;

    ThunarTerminalSyncMode local_sync_mode;
};

struct _ThunarTerminalWidgetClass {
    GtkBoxClass parent_class;
};

GType thunar_terminal_widget_get_type(void);

ThunarTerminalWidget *thunar_terminal_widget_new(void);
ThunarTerminalWidget *thunar_terminal_widget_new_with_location(GFile *location);

void spawn_terminal_in_widget(ThunarTerminalWidget *self);
void thunar_terminal_widget_set_current_location(ThunarTerminalWidget *self, GFile *location);
void thunar_terminal_widget_ensure_terminal_focus(ThunarTerminalWidget *self);

/* This function is now obsolete and should not be used.
 * The new pattern is to create the paned in thunar-window.c and
 * assign it to the terminal via thunar_terminal_widget_set_container_paned().
 */
/* gboolean thunar_terminal_widget_initialize_in_paned(ThunarTerminalWidget *self,
                                                    GtkWidget *unused_view_content,
                                                    GtkWidget *view_overlay); */

void thunar_terminal_widget_toggle_visible(ThunarTerminalWidget *self);
void thunar_terminal_widget_toggle_visible_with_save(ThunarTerminalWidget *self,
                                                      gboolean is_manual_toggle);

gboolean thunar_terminal_widget_get_visible(ThunarTerminalWidget *self);
void thunar_terminal_widget_ensure_state(ThunarTerminalWidget *self);

void thunar_terminal_widget_apply_new_size(ThunarTerminalWidget *self);
int thunar_terminal_widget_get_default_height(void);
void thunar_terminal_widget_save_height(ThunarTerminalWidget *self, int height);

/* NEW PUBLIC FUNCTION DECLARATION */
void thunar_terminal_widget_set_container_paned(ThunarTerminalWidget *self, GtkWidget *paned);

const gchar *thunar_terminal_widget_get_color_scheme(ThunarTerminalWidget *self);
void thunar_terminal_widget_set_color_scheme(ThunarTerminalWidget *self, const gchar *scheme);
void thunar_terminal_widget_apply_color_scheme(ThunarTerminalWidget *self);

G_END_DECLS

#endif /* __THUNAR_TERMINAL_WIDGET_H__ */