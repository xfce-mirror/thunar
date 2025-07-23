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
 *
 */

#include "thunar-terminal-widget.h"
#include "thunar-navigator.h"
#include "thunar-preferences.h"
#include "thunar-private.h"

#include <libxfce4ui/libxfce4ui.h>

/* Helper macro to make g_autoptr work with ThunarPreferences */
G_DEFINE_AUTOPTR_CLEANUP_FUNC (ThunarPreferences, g_object_unref)

/* UI constants */
#define MIN_MAIN_VIEW_HEIGHT 200
#define MIN_TERMINAL_HEIGHT 50
#define MIN_FONT_SIZE 6
#define MAX_FONT_SIZE 72

/* GObject properties */
enum
{
  PROP_0,
  PROP_CURRENT_LOCATION,
  PROP_NAVIGATOR_CURRENT_DIRECTORY,
  N_PROPS
};

/* GObject signals */
enum
{
  TOGGLE_VISIBILITY,
  LAST_SIGNAL
};

struct _ThunarTerminalWidgetPrivate
{
  /* Child widgets */
  GtkWidget   *scrolled_window; /* The GtkScrolledWindow that makes the terminal scrollable. */
  VteTerminal *terminal;        /* The core VTE terminal widget. */
  GtkWidget   *ssh_indicator;   /* A label shown at the top to indicate an active SSH session. */

  /* State management */
  ThunarTerminalState state;                          /* The current operational state (local shell or remote SSH session). */
  gboolean            is_visible;                     /* Tracks if the terminal widget is currently shown to the user. */
  gboolean            in_toggling;                    /* A re-entrancy guard for the visibility toggle function to prevent rapid, repeated calls. */
  gboolean            needs_respawn;                  /* Flag indicating if the terminal's child process needs to be respawned (e.g., after being hidden and shown again). */
  gboolean            ignore_next_terminal_cd_signal; /* A flag to prevent feedback loops when the file manager programmatically changes the terminal's directory. */
  guint               focus_timeout_id;               /* The ID for a timeout used to ensure the terminal gets focus after certain operations. */
  GPid                child_pid;                      /* The process ID of the shell or SSH client running in the terminal. -1 if no process is running. */
  GCancellable       *spawn_cancellable;              /* A GCancellable object to allow cancelling an asynchronous terminal spawn operation. */
  GWeakRef            paned_weak_ref;                 /* A weak reference to the parent GtkPaned to avoid circular references and allow size adjustments. */

  /* Preferences */
  gchar                           *color_scheme;          /* The name of the current color scheme (e.g., "dark", "solarized-light"). */
  ThunarTerminalSyncMode           local_sync_mode;       /* The directory synchronization mode for local shell sessions. */
  ThunarTerminalSshAutoConnectMode ssh_auto_connect_mode; /* The auto-connection behavior when navigating to SFTP locations. */

  /* Location and SSH details */
  GFile *current_location; /* The GFile representing the current directory displayed in the file manager view. */

  gchar                 *ssh_hostname;    /* The hostname for the current SSH connection. */
  gchar                 *ssh_username;    /* The username for the current SSH connection. */
  gchar                 *ssh_port;        /* The port for the current SSH connection. */
  gchar                 *ssh_remote_path; /* The remote path to change to after an SSH connection is established. */
  ThunarTerminalSyncMode ssh_sync_mode;   /* The directory synchronization mode for the current SSH session. */

  /* Pending SSH connection details, used when a location is set before the terminal is spawned */
  gchar                 *pending_ssh_hostname;  /* The hostname for a pending SSH connection, to be used after the local shell spawns. */
  gchar                 *pending_ssh_username;  /* The username for a pending SSH connection. */
  gchar                 *pending_ssh_port;      /* The port for a pending SSH connection. */
  ThunarTerminalSyncMode pending_ssh_sync_mode; /* The sync mode for a pending SSH connection. */
};

/* SSH mode
 * GVFS automatically creates an address that allows us to access the server's files connected via SFTP
 * through the terminal,but it behaves like a remote disk. In contrast, an SSH connection actually logs
 * you into the server, enabling direct terminal access to the server. This allows tasks like installing
 * packages or restarting services, which is particularly useful for those working in DevOps, as it enables
 * editing configuration files through a graphical interface while managing processes via the terminal.
 * Keys for g_object_set_data() */
static const gchar *const DATA_KEY_SSH_HOSTNAME = "ttw-ssh-hostname";
static const gchar *const DATA_KEY_SSH_USERNAME = "ttw-ssh-username";
static const gchar *const DATA_KEY_SSH_PORT = "ttw-ssh-port";
static const gchar *const DATA_KEY_SSH_SYNC_MODE = "ttw-ssh-sync-mode";

/* Shell control sequences for preserving user input during programmatic 'cd' */
static const gchar *const SHELL_CTRL_A = "\x01";    /* Move cursor to beginning of line */
static const gchar *const SHELL_CTRL_K = "\x0B";    /* Kill (cut) from cursor to end of line */
static const gchar *const SHELL_CTRL_Y = "\x19";    /* Yank (paste) killed text */
static const gchar *const SHELL_CTRL_E = "\x05";    /* Move cursor to end of line */
static const gchar *const SHELL_DELETE = "\033[3~"; /* Delete character under cursor */

typedef struct
{
  GdkRGBA  foreground;
  GdkRGBA  background;
  GdkRGBA  palette[16];
  gboolean use_system_colors;
} ThunarTerminalColorPalette;

/* Helper macro to reduce verbosity when defining GdkRGBA colors. Alpha is assumed to be 1.0. */
#define RGB(r, g, b) ((GdkRGBA){ .red = (r), .green = (g), .blue = (b), .alpha = 1.0 })

/* Struct to map a color scheme ID to its UI label and its embedded palette data. */
typedef struct
{
  const gchar                     *id;
  const gchar                     *label_pot;
  const ThunarTerminalColorPalette palette;
} MenuSchemeEntry;

/*
 * The .palette array indices correspond to the following colors:
 * [0]  Black               [1]  Red            [2]  Green        [3] Yellow
 * [4]  Blue                [5]  Magenta        [6]  Cyan         [7] White
 * [8]  Bright Black (Gray) [9]  Bright Red     [10] Bright Green [11] Bright Yellow
 * [12] Bright Blue         [13] Bright Magenta [14] Bright Cyan  [15] Bright White
 *
 */
/* clang-format off */
static const MenuSchemeEntry COLOR_SCHEME_ENTRIES[] = {
  { "system", N_("System"), .palette = { .use_system_colors = TRUE } },
  { "dark", N_("Dark"), .palette = {
      .foreground = RGB(0.9, 0.9, 0.9),
      .background = RGB(0.12, 0.12, 0.12),
      .palette = {
        RGB(0.0, 0.0, 0.0), RGB(0.8, 0.0, 0.0), RGB(0.0, 0.8, 0.0), RGB(0.8, 0.8, 0.0),
        RGB(0.0, 0.0, 0.8), RGB(0.8, 0.0, 0.8), RGB(0.0, 0.8, 0.8), RGB(0.8, 0.8, 0.8),
        RGB(0.5, 0.5, 0.5), RGB(1.0, 0.4, 0.4), RGB(0.4, 1.0, 0.4), RGB(1.0, 1.0, 0.4),
        RGB(0.4, 0.4, 1.0), RGB(1.0, 0.4, 1.0), RGB(0.4, 1.0, 1.0), RGB(1.0, 1.0, 1.0)
      }
    }
  },
  { "light", N_("Light"), .palette = {
      .foreground = RGB(0.15, 0.15, 0.15),
      .background = RGB(0.98, 0.98, 0.98),
      .palette = {
        RGB(0.2, 0.2, 0.2), RGB(0.8, 0.2, 0.2), RGB(0.1, 0.6, 0.1), RGB(0.7, 0.6, 0.1),
        RGB(0.2, 0.4, 0.7), RGB(0.6, 0.3, 0.5), RGB(0.3, 0.6, 0.7), RGB(0.7, 0.7, 0.7),
        RGB(0.4, 0.4, 0.4), RGB(0.9, 0.3, 0.3), RGB(0.2, 0.7, 0.2), RGB(0.8, 0.7, 0.2),
        RGB(0.3, 0.5, 0.8), RGB(0.7, 0.4, 0.6), RGB(0.4, 0.7, 0.8), RGB(0.9, 0.9, 0.9)
      }
    }
  },
  { "solarized-dark", N_("Solarized Dark"), .palette = {
      .foreground = RGB(0.8235, 0.8588, 0.8706),
      .background = RGB(0.0000, 0.1686, 0.2118),
      .palette = {
        RGB(0.0275, 0.2118, 0.2588), RGB(0.8627, 0.1961, 0.1843), RGB(0.5216, 0.6000, 0.0000), RGB(0.7098, 0.5412, 0.0000),
        RGB(0.1490, 0.5451, 0.8235), RGB(0.8275, 0.2118, 0.5098), RGB(0.1647, 0.6314, 0.6000), RGB(0.9294, 0.9098, 0.8353),
        RGB(0.0000, 0.1686, 0.2118), RGB(0.8000, 0.2588, 0.2078), RGB(0.3725, 0.4235, 0.4314), RGB(0.4078, 0.4745, 0.4784),
        RGB(0.5137, 0.5804, 0.5843), RGB(0.4235, 0.4431, 0.6118), RGB(0.5804, 0.6078, 0.5373), RGB(0.9922, 0.9647, 0.8902)
      }
    }
  },
  { "solarized-light", N_("Solarized Light"), .palette = {
      .foreground = RGB(0.4000, 0.4784, 0.5098),
      .background = RGB(0.9922, 0.9647, 0.8902),
      .palette = {
        RGB(0.0275, 0.2118, 0.2588), RGB(0.8627, 0.1961, 0.1843), RGB(0.5216, 0.6000, 0.0000), RGB(0.7098, 0.5412, 0.0000),
        RGB(0.1490, 0.5451, 0.8235), RGB(0.8275, 0.2118, 0.5098), RGB(0.1647, 0.6314, 0.6000), RGB(0.9294, 0.9098, 0.8353),
        RGB(0.0000, 0.1686, 0.2118), RGB(0.8000, 0.2588, 0.2078), RGB(0.3725, 0.4235, 0.4314), RGB(0.4078, 0.4745, 0.4784),
        RGB(0.5137, 0.5804, 0.5843), RGB(0.4235, 0.4431, 0.6118), RGB(0.5804, 0.6078, 0.5373), RGB(0.8235, 0.8588, 0.8706)
      }
    }
  },
  { "matrix", N_("Matrix"), .palette = {
      .foreground = RGB(0.1, 0.9, 0.1),
      .background = RGB(0.0, 0.0, 0.0),
      .palette = {
        RGB(0.0, 0.0, 0.0), RGB(0.0, 0.5, 0.0), RGB(0.0, 0.8, 0.0), RGB(0.1, 0.6, 0.0),
        RGB(0.0, 0.4, 0.0), RGB(0.1, 0.5, 0.1), RGB(0.0, 0.7, 0.1), RGB(0.1, 0.9, 0.1),
        RGB(0.0, 0.3, 0.0), RGB(0.0, 0.6, 0.0), RGB(0.0, 1.0, 0.0), RGB(0.2, 0.7, 0.0),
        RGB(0.0, 0.5, 0.0), RGB(0.2, 0.6, 0.2), RGB(0.0, 0.8, 0.2), RGB(0.2, 1.0, 0.2)
      }
    }
  },
  { "one-half-dark", N_("One Half Dark"), .palette = {
      .foreground = RGB(0.870, 0.870, 0.870),
      .background = RGB(0.157, 0.168, 0.184),
      .palette = {
        RGB(0.157, 0.168, 0.184), RGB(0.882, 0.490, 0.470), RGB(0.560, 0.749, 0.450), RGB(0.941, 0.768, 0.470),
        RGB(0.400, 0.627, 0.850), RGB(0.768, 0.470, 0.800), RGB(0.341, 0.709, 0.729), RGB(0.870, 0.870, 0.870),
        RGB(0.400, 0.450, 0.500), RGB(0.882, 0.490, 0.470), RGB(0.560, 0.749, 0.450), RGB(0.941, 0.768, 0.470),
        RGB(0.400, 0.627, 0.850), RGB(0.768, 0.470, 0.800), RGB(0.341, 0.709, 0.729), RGB(0.970, 0.970, 0.970)
      }
    }
  },
  { "one-half-light", N_("One Half Light"), .palette = {
      .foreground = RGB(0.220, 0.240, 0.260),
      .background = RGB(0.980, 0.980, 0.980),
      .palette = {
        RGB(0.220, 0.240, 0.260), RGB(0.858, 0.200, 0.180), RGB(0.310, 0.600, 0.110), RGB(0.850, 0.588, 0.100),
        RGB(0.231, 0.490, 0.749), RGB(0.670, 0.270, 0.729), RGB(0.149, 0.639, 0.678), RGB(0.800, 0.800, 0.800),
        RGB(0.400, 0.400, 0.400), RGB(0.858, 0.200, 0.180), RGB(0.310, 0.600, 0.110), RGB(0.850, 0.588, 0.100),
        RGB(0.231, 0.490, 0.749), RGB(0.670, 0.270, 0.729), RGB(0.149, 0.639, 0.678), RGB(0.080, 0.080, 0.080)
      }
    }
  },
  { "monokai", N_("Monokai"), .palette = {
      .foreground = RGB(0.929, 0.925, 0.910),
      .background = RGB(0, 0, 0),
      .palette = {
        RGB(0.153, 0.157, 0.149), RGB(0.980, 0.149, 0.450), RGB(0.650, 0.890, 0.180), RGB(0.960, 0.780, 0.310),
        RGB(0.208, 0.580, 0.839), RGB(0.670, 0.380, 0.960), RGB(0.400, 0.950, 0.950), RGB(0.929, 0.925, 0.910),
        RGB(0.400, 0.400, 0.400), RGB(0.980, 0.149, 0.450), RGB(0.650, 0.890, 0.180), RGB(0.960, 0.780, 0.310),
        RGB(0.208, 0.580, 0.839), RGB(0.670, 0.380, 0.960), RGB(0.400, 0.950, 0.950), RGB(1.000, 1.000, 1.000)
      }
    }
  },
};
/* clang-format on */

/* Struct to define available font sizes for the menu. */
typedef struct
{
  int size_pts;
} MenuFontSizeEntry;

static const MenuFontSizeEntry FONT_SIZE_ENTRIES[] = {
  { 9 },
  { 10 },
  { 11 },
  { 12 },
  { 13 },
  { 14 },
  { 15 },
  { 16 },
  { 17 },
  { 18 },
  { 20 },
  { 22 },
  { 24 },
  { 28 },
  { 32 },
  { 36 },
  { 40 },
  { 48 }
};

/* Struct to map a sync mode enum to its UI label. */
typedef struct
{
  ThunarTerminalSyncMode mode;
  const gchar           *label_pot;
} MenuSyncModeEntry;

static const MenuSyncModeEntry LOCAL_SYNC_MODE_ENTRIES[] = {
  { THUNAR_TERMINAL_SYNC_BOTH, N_ ("Sync Both Ways") },
  { THUNAR_TERMINAL_SYNC_FM_TO_TERM, N_ ("Sync File Manager → Terminal") },
  { THUNAR_TERMINAL_SYNC_TERM_TO_FM, N_ ("Sync Terminal → File Manager") },
  { THUNAR_TERMINAL_SYNC_NONE, N_ ("No Sync") }
};

/* Struct to map an SSH auto-connect enum to its UI label. */
typedef struct
{
  ThunarTerminalSshAutoConnectMode mode;
  const gchar                     *label_pot;
} MenuSshAutoConnectEntry;

static const MenuSshAutoConnectEntry SFTP_AUTO_CONNECT_ENTRIES[] = {
  { THUNAR_TERMINAL_SSH_AUTOCONNECT_OFF, N_ ("Do not connect automatically") },
  { THUNAR_TERMINAL_SSH_AUTOCONNECT_SYNC_BOTH, N_ ("Automatically connect and sync both ways") },
  { THUNAR_TERMINAL_SSH_AUTOCONNECT_SYNC_FM_TO_TERM, N_ ("Automatically connect and sync: File Manager → Terminal") },
  { THUNAR_TERMINAL_SSH_AUTOCONNECT_SYNC_TERM_TO_FM, N_ ("Automatically connect and sync: Terminal → File Manager") },
  { THUNAR_TERMINAL_SSH_AUTOCONNECT_SYNC_NONE, N_ ("Automatically connect without syncing") }
};

static const MenuSyncModeEntry MANUAL_SSH_SYNC_ENTRIES[] = {
  { THUNAR_TERMINAL_SYNC_BOTH, N_ ("Sync folder both ways") },
  { THUNAR_TERMINAL_SYNC_FM_TO_TERM, N_ ("Sync folder from File Manager → Terminal") },
  { THUNAR_TERMINAL_SYNC_TERM_TO_FM, N_ ("Sync folder from Terminal → File Manager") },
  { THUNAR_TERMINAL_SYNC_NONE, N_ ("No folder sync") }
};

/* Generic data key for attaching a value to a menu widget. */
static const gchar *const DATA_KEY_VALUE = "ttw-value";

static void
on_color_scheme_changed (GtkCheckMenuItem *menuitem, gpointer user_data);
static void
on_font_size_changed (GtkCheckMenuItem *menuitem, gpointer user_data);
static void
on_enum_pref_changed (GtkCheckMenuItem *menuitem, gpointer user_data);
static void
on_ssh_exit_activate (GtkMenuItem *menuitem, gpointer user_data);
static void
on_ssh_connect_activate (GtkMenuItem *menuitem, gpointer user_data);

static GParamSpec *properties[N_PROPS];
static guint       signals[LAST_SIGNAL];

static void
thunar_terminal_widget_navigator_init (ThunarNavigatorIface *iface);

G_DEFINE_TYPE_WITH_CODE (ThunarTerminalWidget, thunar_terminal_widget, GTK_TYPE_BOX,
                         G_ADD_PRIVATE (ThunarTerminalWidget)
                         G_IMPLEMENT_INTERFACE (THUNAR_TYPE_NAVIGATOR, thunar_terminal_widget_navigator_init))

static void
thunar_terminal_widget_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec);
static void
thunar_terminal_widget_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec);
static void
thunar_terminal_widget_finalize (GObject *object);
static void
spawn_terminal_async (ThunarTerminalWidget *self);
static void
spawn_async_callback (VteTerminal *terminal,
                      GPid         pid,
                      GError      *error,
                      gpointer     user_data);
static void
_initiate_ssh_connection (ThunarTerminalWidget  *self,
                          const gchar           *hostname,
                          const gchar           *username,
                          const gchar           *port,
                          ThunarTerminalSyncMode sync_mode);
static gboolean
parse_gvfs_ssh_path (GFile  *location,
                     gchar **hostname,
                     gchar **username,
                     gchar **port);
static void
change_directory_in_terminal (ThunarTerminalWidget *self,
                              GFile                *location);
static void
on_terminal_preference_changed (ThunarPreferences *prefs, GParamSpec *pspec, gpointer user_data);
static void
_clear_ssh_connection_data (ThunarTerminalWidgetPrivate *priv);
static void
_reset_to_local_state (ThunarTerminalWidget *self);
static GtkWidget *
_build_color_scheme_submenu (ThunarTerminalWidget *self);
static GtkWidget *
_build_font_size_submenu (ThunarTerminalWidget *self);
static GtkWidget *
_build_sftp_auto_connect_submenu (ThunarTerminalWidget *self);
static GtkWidget *
_build_local_sync_submenu (ThunarTerminalWidget *self);
static GtkWidget *
_build_manual_ssh_connect_submenu (ThunarTerminalWidget *self,
                                   const gchar          *hostname,
                                   const gchar          *username,
                                   const gchar          *port);
static void
_append_menu_item_with_submenu (GtkMenuShell *menu, const gchar *label, GtkWidget *submenu);
static GtkWidget *
create_terminal_popup_menu (ThunarTerminalWidget *self);
static gboolean
on_terminal_button_release (GtkWidget *widget, GdkEventButton *event, gpointer user_data);


/******************************************************************
 ****************** Public Function Implementations ***************
 ******************************************************************/

ThunarTerminalWidget *
thunar_terminal_widget_new (void)
{
  return thunar_terminal_widget_new_with_location (NULL);
}

ThunarTerminalWidget *
thunar_terminal_widget_new_with_location (GFile *location)
{
  return g_object_new (THUNAR_TYPE_TERMINAL_WIDGET, "current-location", location, NULL);
}

void
thunar_terminal_widget_set_current_location (ThunarTerminalWidget *self,
                                             GFile                *location)
{
  ThunarTerminalWidgetPrivate *priv = thunar_terminal_widget_get_instance_private (self);

  _thunar_return_if_fail (THUNAR_IS_TERMINAL_WIDGET (self));

  /* Do nothing if the location hasn't changed. This handles all NULL cases correctly. */
  if ((priv->current_location == location) || (priv->current_location && location && g_file_equal (priv->current_location, location)))
    {
      return;
    }

  g_set_object (&priv->current_location, location);
  g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CURRENT_LOCATION]);

  /*
   * If the terminal is running a local shell, we first check if the new location
   * is a remote SFTP path. If so, and if auto-connect is enabled, we initiate SSH.
   */
  if (priv->state == THUNAR_TERMINAL_STATE_LOCAL && location)
    {
      g_autofree gchar *hostname = NULL, *username = NULL, *port = NULL;

      /* Check for SFTP auto-connect feature. */
      if (priv->ssh_auto_connect_mode != THUNAR_TERMINAL_SSH_AUTOCONNECT_OFF && parse_gvfs_ssh_path (location, &hostname, &username, &port))
        {
          ThunarTerminalSyncMode sync_mode;

          /* Determine the sync mode for the new SSH session based on the auto-connect preference. */
          switch (priv->ssh_auto_connect_mode)
            {
            case THUNAR_TERMINAL_SSH_AUTOCONNECT_SYNC_BOTH:
              sync_mode = THUNAR_TERMINAL_SYNC_BOTH;
              break;
            case THUNAR_TERMINAL_SSH_AUTOCONNECT_SYNC_FM_TO_TERM:
              sync_mode = THUNAR_TERMINAL_SYNC_FM_TO_TERM;
              break;
            case THUNAR_TERMINAL_SSH_AUTOCONNECT_SYNC_TERM_TO_FM:
              sync_mode = THUNAR_TERMINAL_SYNC_TERM_TO_FM;
              break;
            default:
              sync_mode = THUNAR_TERMINAL_SYNC_NONE;
              break;
            }

          /*
           * RACE CONDITION HANDLING: The local shell might not have spawned yet.
           * If the child process (shell) exists, we can connect immediately.
           */
          if (priv->child_pid != -1)
            {
              _initiate_ssh_connection (self, hostname, username, port, sync_mode);
              return; /* The SSH connection logic takes over from here. */
            }
          else
            {
              /*
               * If there's no shell running, we queue the connection details. The connection
               * will be established right after the local shell has finished spawning.
               */
              priv->pending_ssh_hostname = g_strdup (hostname);
              priv->pending_ssh_username = g_strdup (username);
              priv->pending_ssh_port = g_strdup (port);
              priv->pending_ssh_sync_mode = sync_mode;
            }
        }
    }

  /* If we have reached this point and the new location is NULL, there's nothing left to sync. */
  if (location == NULL)
    {
      return;
    }

  /*
   * If we are already in an SSH session, we only act if the new location is on the same host.
   */
  if (priv->state == THUNAR_TERMINAL_STATE_IN_SSH)
    {
      g_autofree gchar *scheme = g_file_get_uri_scheme (location);
      if (scheme && g_strcmp0 (scheme, "sftp") == 0)
        {
          g_autofree gchar *new_hostname = NULL, *new_username = NULL, *new_port = NULL;
          if (parse_gvfs_ssh_path (location, &new_hostname, &new_username, &new_port))
            {
              /* SAFETY CHECK: Ensure the new location's host matches the current SSH session's host. */
              if (priv->ssh_hostname && new_hostname && g_strcmp0 (priv->ssh_hostname, new_hostname) == 0)
                {
                  /* If hosts match, just change the directory within the existing session. */
                  change_directory_in_terminal (self, location);
                }
            }
        }
    }
  else
    /*
     * If we are in a local shell and the new location is not an SSH auto-connect trigger, simply change the directory.
     */
    {
      change_directory_in_terminal (self, location);
    }
}

static const gchar *
thunar_terminal_widget_get_color_scheme (ThunarTerminalWidget *self)
{
  ThunarTerminalWidgetPrivate *priv = thunar_terminal_widget_get_instance_private (self);
  _thunar_return_val_if_fail (THUNAR_IS_TERMINAL_WIDGET (self), "system");

  if (priv->color_scheme == NULL)
    {
      g_object_get (thunar_preferences_get (), "terminal-color-scheme", &priv->color_scheme, NULL);
    }
  return priv->color_scheme;
}

static void
thunar_terminal_widget_apply_color_scheme (ThunarTerminalWidget *self)
{
  ThunarTerminalWidgetPrivate *priv = thunar_terminal_widget_get_instance_private (self);
  /* The default palette is the first entry in our data-driven array ("system"). */
  const ThunarTerminalColorPalette *palette_to_apply = &COLOR_SCHEME_ENTRIES[0].palette;
  const gchar                      *current_scheme_name;

  _thunar_return_if_fail (THUNAR_IS_TERMINAL_WIDGET (self));
  current_scheme_name = thunar_terminal_widget_get_color_scheme (self);

  /* Find the selected palette by looping through the data structure. */
  for (gsize i = 0; i < G_N_ELEMENTS (COLOR_SCHEME_ENTRIES); ++i)
    {
      if (g_strcmp0 (current_scheme_name, COLOR_SCHEME_ENTRIES[i].id) == 0)
        {
          palette_to_apply = &COLOR_SCHEME_ENTRIES[i].palette;
          break;
        }
    }

  if (palette_to_apply->use_system_colors)
    {
      vte_terminal_set_colors (priv->terminal, NULL, NULL, NULL, 0);
    }
  else
    {
      vte_terminal_set_colors (priv->terminal,
                               &palette_to_apply->foreground,
                               &palette_to_apply->background,
                               palette_to_apply->palette,
                               G_N_ELEMENTS (palette_to_apply->palette));
    }
}

static void
thunar_terminal_widget_set_color_scheme (ThunarTerminalWidget *self,
                                         const gchar          *scheme)
{
  ThunarTerminalWidgetPrivate *priv = thunar_terminal_widget_get_instance_private (self);
  _thunar_return_if_fail (THUNAR_IS_TERMINAL_WIDGET (self));
  _thunar_return_if_fail (scheme != NULL);

  g_free (priv->color_scheme);
  priv->color_scheme = g_strdup (scheme);
  g_object_set (thunar_preferences_get (), "terminal-color-scheme", priv->color_scheme, NULL);
  thunar_terminal_widget_apply_color_scheme (self);
}

void
thunar_terminal_widget_set_container_paned (ThunarTerminalWidget *self,
                                            GtkWidget            *paned)
{
  ThunarTerminalWidgetPrivate *priv = thunar_terminal_widget_get_instance_private (self);
  g_autoptr (GtkWidget) old_paned = NULL;

  _thunar_return_if_fail (THUNAR_IS_TERMINAL_WIDGET (self));

  old_paned = g_weak_ref_get (&priv->paned_weak_ref);

  if (old_paned == paned)
    {
      return;
    }

  g_weak_ref_set (&priv->paned_weak_ref, G_OBJECT (paned));
}

void
thunar_terminal_widget_apply_new_size (ThunarTerminalWidget *self)
{
  ThunarTerminalWidgetPrivate *priv = thunar_terminal_widget_get_instance_private (self);
  g_autoptr (GtkWidget) paned = NULL;

  _thunar_return_if_fail (THUNAR_IS_TERMINAL_WIDGET (self));

  paned = g_weak_ref_get (&priv->paned_weak_ref);

  if (paned && gtk_widget_get_realized (paned))
    {
      const int total_height = gtk_widget_get_allocated_height (paned);
      int       saved_height = 0;
      g_object_get (thunar_preferences_get (), "terminal-height", &saved_height, NULL);
      if (saved_height <= MIN_TERMINAL_HEIGHT)
        {
          saved_height = MIN_TERMINAL_HEIGHT;
        }

      const int max_allowed_height = MAX (total_height - MIN_MAIN_VIEW_HEIGHT, MIN_TERMINAL_HEIGHT);
      const int terminal_height = CLAMP (saved_height, MIN_TERMINAL_HEIGHT, max_allowed_height);
      const int new_pos = total_height - terminal_height;

      gtk_paned_set_position (GTK_PANED (paned), new_pos);
    }
}

static gboolean
focus_once_and_remove (gpointer user_data)
{
  GtkWidget            *widget_to_focus = GTK_WIDGET (user_data);
  ThunarTerminalWidget *self = THUNAR_TERMINAL_WIDGET (gtk_widget_get_ancestor (widget_to_focus, THUNAR_TYPE_TERMINAL_WIDGET));

  if (self)
    {
      ThunarTerminalWidgetPrivate *priv = thunar_terminal_widget_get_instance_private (self);
      if (gtk_widget_get_window (widget_to_focus))
        {
          gtk_widget_grab_focus (widget_to_focus);
          priv->focus_timeout_id = 0;
        }
    }

  return G_SOURCE_REMOVE;
}

void
thunar_terminal_widget_ensure_terminal_focus (ThunarTerminalWidget *self)
{
  ThunarTerminalWidgetPrivate *priv = thunar_terminal_widget_get_instance_private (self);
  _thunar_return_if_fail (THUNAR_IS_TERMINAL_WIDGET (self));
  if (priv->focus_timeout_id > 0)
    g_source_remove (priv->focus_timeout_id);
  priv->focus_timeout_id = g_timeout_add (50, focus_once_and_remove, priv->terminal);
}

void
thunar_terminal_widget_toggle_visible (ThunarTerminalWidget *self)
{
  ThunarTerminalWidgetPrivate *priv = thunar_terminal_widget_get_instance_private (self);
  _thunar_return_if_fail (THUNAR_IS_TERMINAL_WIDGET (self));

  if (priv->in_toggling)
    return;
  priv->in_toggling = TRUE;

  priv->is_visible = !priv->is_visible;

  if (priv->is_visible)
    {
      gtk_widget_show (GTK_WIDGET (self));
      if (priv->needs_respawn)
        spawn_terminal_async (self);

      /* Sync to the current location upon becoming visible */
      if (priv->current_location)
        {
          change_directory_in_terminal (self, priv->current_location);
        }

      thunar_terminal_widget_apply_new_size (self);
      thunar_terminal_widget_ensure_terminal_focus (self);
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (self));
    }

  g_object_set (thunar_preferences_get (), "terminal-visible", priv->is_visible, NULL);
  g_signal_emit (self, signals[TOGGLE_VISIBILITY], 0, priv->is_visible);
  priv->in_toggling = FALSE;
}

void
thunar_terminal_widget_ensure_state (ThunarTerminalWidget *self)
{
  ThunarTerminalWidgetPrivate *priv = thunar_terminal_widget_get_instance_private (self);
  gboolean                     should_be_visible;
  _thunar_return_if_fail (THUNAR_IS_TERMINAL_WIDGET (self));

  g_object_get (thunar_preferences_get (), "terminal-visible", &should_be_visible, NULL);

  if (priv->is_visible != should_be_visible)
    {
      priv->is_visible = should_be_visible;
      g_signal_emit (self, signals[TOGGLE_VISIBILITY], 0, priv->is_visible);
    }

  if (should_be_visible)
    {
      gtk_widget_show (GTK_WIDGET (self));
      if (priv->needs_respawn)
        spawn_terminal_async (self);
    }
  else
    {
      gtk_widget_hide (GTK_WIDGET (self));
    }
}

gboolean
thunar_terminal_widget_get_visible (ThunarTerminalWidget *self)
{
  ThunarTerminalWidgetPrivate *priv = thunar_terminal_widget_get_instance_private (self);
  _thunar_return_val_if_fail (THUNAR_IS_TERMINAL_WIDGET (self), FALSE);
  return priv->is_visible;
}


/******************************************************************
 ****************** GObject Method Implementations ****************
 ******************************************************************/

static void
thunar_terminal_widget_class_init (ThunarTerminalWidgetClass *klass)
{
  GObjectClass   *object_class = G_OBJECT_CLASS (klass);
  GTypeInterface *navigator_iface;

  object_class->set_property = thunar_terminal_widget_set_property;
  object_class->get_property = thunar_terminal_widget_get_property;
  object_class->finalize = thunar_terminal_widget_finalize;

  properties[PROP_CURRENT_LOCATION] =
  g_param_spec_object ("current-location",
                       "Current Location",
                       "The GFile representing the current directory.",
                       G_TYPE_FILE,
                       G_PARAM_READWRITE | G_PARAM_STATIC_STRINGS | G_PARAM_EXPLICIT_NOTIFY);

  navigator_iface = g_type_default_interface_peek (THUNAR_TYPE_NAVIGATOR);
  properties[PROP_NAVIGATOR_CURRENT_DIRECTORY] =
  g_param_spec_override ("current-directory",
                         g_object_interface_find_property (navigator_iface, "current-directory"));

  g_object_class_install_properties (object_class, N_PROPS, properties);

  signals[TOGGLE_VISIBILITY] =
  g_signal_new ("toggle-visibility",
                G_TYPE_FROM_CLASS (klass),
                G_SIGNAL_RUN_LAST,
                0, NULL, NULL,
                g_cclosure_marshal_VOID__BOOLEAN,
                G_TYPE_NONE,
                1,
                G_TYPE_BOOLEAN);
}

static void
on_terminal_child_exited (VteTerminal *terminal, gint status, gpointer user_data);
static int
get_terminal_font_size (void);

#if VTE_CHECK_VERSION(0, 78, 0)
/* Modern VTE uses termprop-changed signal */
static void
on_directory_changed (VteTerminal *terminal, const gchar *prop, gpointer user_data);
#else
/* Legacy VTE uses current-directory-uri-changed signal */
static void
on_legacy_directory_changed (VteTerminal *terminal, gpointer user_data);
#endif


static void
setup_terminal_font (VteTerminal *terminal)
{
  g_autoptr (PangoFontDescription) font_desc = NULL;
  g_autoptr (GSettings) interface_settings = g_settings_new ("org.gnome.desktop.interface");
  g_autofree gchar *font_name = g_settings_get_string (interface_settings, "monospace-font-name");
  int               font_size_pts = get_terminal_font_size ();

  if (font_name && *font_name)
    font_desc = pango_font_description_from_string (font_name);
  else
    font_desc = pango_font_description_new ();

  pango_font_description_set_size (font_desc, font_size_pts * PANGO_SCALE);
  vte_terminal_set_font (terminal, font_desc);
}

static void
thunar_terminal_widget_init (ThunarTerminalWidget *self)
{
  ThunarTerminalWidgetPrivate *priv = thunar_terminal_widget_get_instance_private (self);
  GtkStyleContext             *context = NULL;
  g_autoptr (GtkCssProvider) provider = NULL;
  g_autoptr (ThunarPreferences) prefs = NULL;
  GtkWidget *vbox;

  self->priv = priv;

  /*
   * Initialize state to non-zero default values. All other members
   * (pointers, booleans) are guaranteed to be zero-initialized by GObject.
   */
  priv->state = THUNAR_TERMINAL_STATE_LOCAL;
  priv->needs_respawn = TRUE;
  priv->child_pid = -1;
  priv->spawn_cancellable = g_cancellable_new ();
  g_weak_ref_init (&priv->paned_weak_ref, NULL);

  /* Build UI */
  priv->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
  gtk_widget_set_vexpand (priv->scrolled_window, TRUE);
  gtk_widget_set_hexpand (priv->scrolled_window, TRUE);

  priv->terminal = VTE_TERMINAL (vte_terminal_new ());
  vte_terminal_set_scroll_on_output (priv->terminal, FALSE);
  vte_terminal_set_scroll_on_keystroke (priv->terminal, TRUE);
  vte_terminal_set_scrollback_lines (priv->terminal, 10000);
  gtk_widget_set_can_focus (GTK_WIDGET (priv->terminal), TRUE);

  priv->ssh_indicator = gtk_label_new ("SSH");
  gtk_widget_set_name (priv->ssh_indicator, "ssh-indicator");
  gtk_widget_set_tooltip_text (priv->ssh_indicator, _("The terminal is in an active SSH session."));
  gtk_widget_set_no_show_all (priv->ssh_indicator, TRUE);
  gtk_widget_hide (priv->ssh_indicator);
  gtk_widget_set_vexpand (priv->ssh_indicator, FALSE);
  gtk_widget_set_hexpand (priv->ssh_indicator, TRUE);
  gtk_label_set_xalign (GTK_LABEL (priv->ssh_indicator), 0.5);

  provider = gtk_css_provider_new ();
  /* Use theme colors for better visual integration instead of hardcoded values. */
  gtk_css_provider_load_from_data (provider, "label#ssh-indicator { background-color: @theme_selected_bg_color; color: @theme_selected_fg_color; padding: 2px 5px; margin: 0; font-weight: bold; }", -1, NULL);
  context = gtk_widget_get_style_context (priv->ssh_indicator);
  gtk_style_context_add_provider (context, GTK_STYLE_PROVIDER (provider), GTK_STYLE_PROVIDER_PRIORITY_USER);

  vbox = gtk_box_new (GTK_ORIENTATION_VERTICAL, 0);
  gtk_box_pack_start (GTK_BOX (vbox), priv->ssh_indicator, FALSE, FALSE, 0);
  gtk_container_add (GTK_CONTAINER (priv->scrolled_window), GTK_WIDGET (priv->terminal));
  gtk_box_pack_start (GTK_BOX (vbox), priv->scrolled_window, TRUE, TRUE, 0);
  gtk_box_pack_start (GTK_BOX (self), vbox, TRUE, TRUE, 0);

  /* Load preferences and set up listeners to keep the widget state in sync. */
  prefs = thunar_preferences_get ();
  g_object_get (prefs, "terminal-local-sync-mode", &priv->local_sync_mode, NULL);
  g_object_get (prefs, "terminal-ssh-auto-connect-mode", &priv->ssh_auto_connect_mode, NULL);

  setup_terminal_font (priv->terminal);

  thunar_terminal_widget_get_color_scheme (self);
  thunar_terminal_widget_apply_color_scheme (self);

  /* Connect to preference changes to update the widget's state live. */
  g_signal_connect (prefs, "notify::terminal-local-sync-mode", G_CALLBACK (on_terminal_preference_changed), self);
  g_signal_connect (prefs, "notify::terminal-ssh-auto-connect-mode", G_CALLBACK (on_terminal_preference_changed), self);

  /* Connect signals */
  g_signal_connect (priv->terminal, "child-exited", G_CALLBACK (on_terminal_child_exited), self);
  g_signal_connect (priv->terminal, "button-release-event", G_CALLBACK (on_terminal_button_release), self);

#if VTE_CHECK_VERSION(0, 78, 0)
  g_signal_connect (priv->terminal, "termprop-changed", G_CALLBACK (on_directory_changed), self);
#else
  g_signal_connect (priv->terminal, "current-directory-uri-changed", G_CALLBACK (on_legacy_directory_changed), self);
#endif

  gtk_widget_show_all (GTK_WIDGET (self));
  gtk_widget_hide (GTK_WIDGET (self));
}

static void
thunar_terminal_widget_finalize (GObject *object)
{
  ThunarTerminalWidget        *self = THUNAR_TERMINAL_WIDGET (object);
  ThunarTerminalWidgetPrivate *priv = self->priv;
  g_autoptr (GtkWidget) paned = g_weak_ref_get (&priv->paned_weak_ref);
  g_autoptr (ThunarPreferences) prefs = thunar_preferences_get ();

  /* Disconnect from all signals to prevent use-after-free during destruction. */
  g_signal_handlers_disconnect_by_data (prefs, self);

  if (paned)
    {
      g_signal_handlers_disconnect_by_data (paned, self);
    }
  g_weak_ref_clear (&priv->paned_weak_ref);

  if (priv->focus_timeout_id > 0)
    g_source_remove (priv->focus_timeout_id);

  /* Cancel any pending async operations and free all allocated resources. */
  g_cancellable_cancel (priv->spawn_cancellable);
  g_clear_object (&priv->spawn_cancellable);
  g_clear_object (&priv->current_location);
  g_clear_pointer (&priv->color_scheme, g_free);

  /* Use the helper to clean up all SSH-related pointers. */
  _clear_ssh_connection_data (priv);

  G_OBJECT_CLASS (thunar_terminal_widget_parent_class)->finalize (object);
}

static void
thunar_terminal_widget_set_property (GObject      *object,
                                     guint         prop_id,
                                     const GValue *value,
                                     GParamSpec   *pspec)
{
  ThunarTerminalWidget *self = THUNAR_TERMINAL_WIDGET (object);

  switch (prop_id)
    {
    case PROP_CURRENT_LOCATION:
      thunar_terminal_widget_set_current_location (self, g_value_get_object (value));
      break;
    case PROP_NAVIGATOR_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (object), g_value_get_object (value));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}

static void
thunar_terminal_widget_get_property (GObject    *object,
                                     guint       prop_id,
                                     GValue     *value,
                                     GParamSpec *pspec)
{
  ThunarTerminalWidget        *self = THUNAR_TERMINAL_WIDGET (object);
  ThunarTerminalWidgetPrivate *priv = self->priv;

  switch (prop_id)
    {
    case PROP_CURRENT_LOCATION:
      g_value_set_object (value, priv->current_location);
      break;
    case PROP_NAVIGATOR_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (object)));
      break;
    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}


/******************************************************************
 ******************* Static Function Implementations **************
 ******************************************************************/

static void
spawn_async_callback (VteTerminal *terminal,
                      GPid         pid,
                      GError      *error,
                      gpointer     user_data)
{
  ThunarTerminalWidget        *self = THUNAR_TERMINAL_WIDGET (user_data);
  ThunarTerminalWidgetPrivate *priv;

  if (G_UNLIKELY (gtk_widget_in_destruction (GTK_WIDGET (self))))
    {
      return;
    }
  priv = self->priv;

  if (pid == -1)
    {
      g_warning ("Failed to spawn terminal: %s", error ? error->message : "Unknown error");
      priv->needs_respawn = TRUE;
      priv->child_pid = -1;
    }
  else
    {
      priv->child_pid = pid;
      priv->needs_respawn = FALSE;
      /* If an SSH connection was pending, initiate it now that the shell is ready. */
      if (priv->pending_ssh_hostname)
        {
          _initiate_ssh_connection (self, priv->pending_ssh_hostname, priv->pending_ssh_username, priv->pending_ssh_port, priv->pending_ssh_sync_mode);
          g_clear_pointer (&priv->pending_ssh_hostname, g_free);
          g_clear_pointer (&priv->pending_ssh_username, g_free);
          g_clear_pointer (&priv->pending_ssh_port, g_free);
        }
    }
}

static void
spawn_terminal_async (ThunarTerminalWidget *self)
{
  ThunarTerminalWidgetPrivate *priv = self->priv;
  g_autofree gchar            *working_directory = NULL;
  const gchar                 *shell_executable;
  gchar                      **argv = NULL;
  gchar                      **envp = NULL;

  _thunar_return_if_fail (THUNAR_IS_TERMINAL_WIDGET (self));

  if (priv->child_pid != -1)
    return;

  if (g_cancellable_is_cancelled (priv->spawn_cancellable))
    {
      g_clear_object (&priv->spawn_cancellable);
      priv->spawn_cancellable = g_cancellable_new ();
    }

  shell_executable = g_getenv ("SHELL");
  if (!shell_executable || *shell_executable == '\0')
    shell_executable = "/bin/sh";

  if (priv->pending_ssh_hostname != NULL)
    {
      working_directory = NULL; /* Force start in $HOME for a fast SSH connection */
    }
  else if (priv->current_location && g_file_query_exists (priv->current_location, NULL))
    {
      working_directory = g_file_get_path (priv->current_location);
    }

  if (g_str_has_suffix (shell_executable, "zsh"))
    {
      g_autofree gchar *config_dir = g_build_filename (g_get_user_config_dir (), "Thunar", NULL);
      g_autofree gchar *zshrc_path = g_build_filename (config_dir, ".zshrc", NULL);
      g_autofree gchar *zshrc_content = NULL;
      g_autofree gchar *zdotdir_env = NULL;

      /* Ensure the ~/.config/Thunar directory exists. */
      g_mkdir_with_parents (config_dir, 0700);

      /*
       * Create a .zshrc in Thunar's config dir that defines our hook and
       * then sources the user's real .zshrc. This is persistent and clean.
       * The hook emits an OSC 7 escape sequence to inform VTE of the CWD.
       */
      zshrc_content = g_strdup_printf ("_thunar_vte_update_cwd() { echo -en \"\\033]7;file://$PWD\\007\"; };\n"
                                       "typeset -a precmd_functions;\n"
                                       "precmd_functions+=(_thunar_vte_update_cwd);\n"
                                       "[ -f \"$HOME/.zshrc\" ] && . \"$HOME/.zshrc\";\n");

      g_file_set_contents (zshrc_path, zshrc_content, -1, NULL);

      zdotdir_env = g_strdup_printf ("ZDOTDIR=%s", config_dir);
      gchar *zsh_envp[] = { "TERM=xterm-256color", "COLORTERM=truecolor", zdotdir_env, NULL };
      envp = g_strdupv (zsh_envp);
      argv = g_strsplit (shell_executable, " ", -1);
    }
  else /* Assume bash or other compatible shells */
    {
      argv = g_strsplit (shell_executable, " ", -1);
      /*
       * For bash-compatible shells, inject PROMPT_COMMAND to emit an OSC 7
       * escape sequence before each prompt, informing VTE of the CWD.
       */
      gchar *bash_envp[] = {
        "TERM=xterm-256color",
        "PROMPT_COMMAND=echo -en \"\\033]7;file://$PWD\\007\"",
        "COLORTERM=truecolor",
        NULL
      };
      envp = g_strdupv (bash_envp);
    }

  vte_terminal_spawn_async (priv->terminal,
                            VTE_PTY_DEFAULT,
                            working_directory,
                            argv,
                            envp, /* envv */
                            G_SPAWN_SEARCH_PATH,
                            NULL, /* child_setup */
                            NULL, /* child_setup_data */
                            NULL, /* child_setup_data_destroy */
                            -1,   /* timeout */
                            priv->spawn_cancellable,
                            (VteTerminalSpawnAsyncCallback) spawn_async_callback,
                            self);

  g_strfreev (argv);
  g_strfreev (envp);
}

/* Helper function to clear all data related to an active or pending SSH connection. */
static void
_clear_ssh_connection_data (ThunarTerminalWidgetPrivate *priv)
{
  g_clear_pointer (&priv->ssh_hostname, g_free);
  g_clear_pointer (&priv->ssh_username, g_free);
  g_clear_pointer (&priv->ssh_port, g_free);
  g_clear_pointer (&priv->ssh_remote_path, g_free);
  g_clear_pointer (&priv->pending_ssh_hostname, g_free);
  g_clear_pointer (&priv->pending_ssh_username, g_free);
  g_clear_pointer (&priv->pending_ssh_port, g_free);
}

static void
_reset_to_local_state (ThunarTerminalWidget *self)
{
  ThunarTerminalWidgetPrivate *priv = self->priv;

  /* Clear all SSH-related state information. */
  _clear_ssh_connection_data (priv);
  priv->ssh_sync_mode = THUNAR_TERMINAL_SYNC_NONE;

  /* Revert the state and UI for a living widget. */
  priv->state = THUNAR_TERMINAL_STATE_LOCAL;
  gtk_widget_hide (priv->ssh_indicator);
  priv->needs_respawn = TRUE;
}

/*
 * on_terminal_child_exited:
 * @terminal: The VteTerminal whose child process has exited.
 * @status: The exit status of the child process.
 * @user_data: The #ThunarTerminalWidget instance.
 *
 * This signal handler is the core of the state management. It is triggered
 * whenever the child process of the terminal (either a local shell or an ssh
 * client) terminates.
 */
static void
on_terminal_child_exited (VteTerminal *terminal,
                          gint         status,
                          gpointer     user_data)
{
  ThunarTerminalWidget        *self = THUNAR_TERMINAL_WIDGET (user_data);
  ThunarTerminalWidgetPrivate *priv = self->priv;

  /*
   * Destruction Guard: If the widget is being destroyed (e.g., tab closing),
   * the child process is killed, which triggers this signal. We must not
   * perform any UI operations or try to respawn the terminal in this case.
   * The finalize function will handle all necessary cleanup.
   */
  if (gtk_widget_in_destruction (GTK_WIDGET (self)))
    {
      priv->child_pid = -1;
      return;
    }

  priv->child_pid = -1;

  /* If the terminal was in an SSH session, the end of the ssh process
   * triggers this signal. We reset the state back to local, which flags
   * the terminal for respawning a new local shell below. */
  if (priv->state == THUNAR_TERMINAL_STATE_IN_SSH)
    {
      _reset_to_local_state (self); /* This sets needs_respawn = TRUE */
      if (priv->is_visible)
        {
          spawn_terminal_async (self);
        }
    }
  /*
   * If the local shell exits (e.g., user types 'exit'), we hide the
   * terminal widget and ensure it's marked for a full respawn on next show.
   */
  else if (priv->state == THUNAR_TERMINAL_STATE_LOCAL)
    {
      priv->needs_respawn = TRUE;

      /* If the user saw it exit, hide the pane. */
      if (priv->is_visible)
        {
          thunar_terminal_widget_toggle_visible (self);
        }
    }
}

/*
 * on_terminal_preference_changed:
 * @prefs: The ThunarPreferences object.
 * @pspec: The GParamSpec of the changed property.
 * @user_data: The ThunarTerminalWidget instance.
 *
 * This callback is triggered when a relevant terminal preference is changed
 * globally. It updates the local state of the widget instance to reflect
 * the new preference, ensuring the UI (like the context menu) is always
 * up-to-date.
 */
static void
on_terminal_preference_changed (ThunarPreferences *prefs,
                                GParamSpec        *pspec,
                                gpointer           user_data)
{
  ThunarTerminalWidget        *self = THUNAR_TERMINAL_WIDGET (user_data);
  ThunarTerminalWidgetPrivate *priv = self->priv;
  const gchar                 *name = g_param_spec_get_name (pspec);

  if (g_strcmp0 (name, "terminal-local-sync-mode") == 0)
    {
      g_object_get (prefs, "terminal-local-sync-mode", &priv->local_sync_mode, NULL);
    }
  else if (g_strcmp0 (name, "terminal-ssh-auto-connect-mode") == 0)
    {
      g_object_get (prefs, "terminal-ssh-auto-connect-mode", &priv->ssh_auto_connect_mode, NULL);
    }
}

static void
_sync_terminal_to_fm (ThunarTerminalWidget *self, const gchar *cwd_uri)
{
  ThunarTerminalWidgetPrivate *priv = self->priv;
  g_autoptr (GFile) new_gfile_location = NULL;
  gboolean should_sync_to_fm = FALSE;

  if (!cwd_uri)
    {
      return;
    }

  /* When the FM changes the terminal's directory, we must ignore the signal. */
  if (priv->ignore_next_terminal_cd_signal)
    {
      priv->ignore_next_terminal_cd_signal = FALSE;
      return;
    }

  if (priv->state == THUNAR_TERMINAL_STATE_IN_SSH)
    {
      if (priv->ssh_sync_mode == THUNAR_TERMINAL_SYNC_BOTH || priv->ssh_sync_mode == THUNAR_TERMINAL_SYNC_TERM_TO_FM)
        {
          should_sync_to_fm = TRUE;
          if (g_str_has_prefix (cwd_uri, "file://"))
            {
              g_autofree gchar *local_path = g_filename_from_uri (cwd_uri, NULL, NULL);
              if (local_path && priv->ssh_hostname)
                {
                  g_autoptr (GString) sftp_uri = g_string_new ("sftp://");
                  if (priv->ssh_username && *priv->ssh_username)
                    g_string_append_printf (sftp_uri, "%s@", priv->ssh_username);
                  g_string_append (sftp_uri, priv->ssh_hostname);
                  if (priv->ssh_port && *priv->ssh_port)
                    g_string_append_printf (sftp_uri, ":%s", priv->ssh_port);
                  g_string_append (sftp_uri, local_path);
                  new_gfile_location = g_file_new_for_uri (sftp_uri->str);
                }
            }
        }
    }
  else /* Local state */
    {
      if (priv->local_sync_mode == THUNAR_TERMINAL_SYNC_BOTH || priv->local_sync_mode == THUNAR_TERMINAL_SYNC_TERM_TO_FM)
        {
          should_sync_to_fm = TRUE;
          new_gfile_location = g_file_new_for_uri (cwd_uri);
        }
    }

  if (should_sync_to_fm && new_gfile_location && (priv->current_location == NULL || !g_file_equal (new_gfile_location, priv->current_location)))
    {
      ThunarFile *thunar_file;

      g_set_object (&priv->current_location, new_gfile_location);
      g_object_notify_by_pspec (G_OBJECT (self), properties[PROP_CURRENT_LOCATION]);

      thunar_file = thunar_file_get (new_gfile_location, NULL);
      if (G_LIKELY (thunar_file))
        {
          thunar_navigator_change_directory (THUNAR_NAVIGATOR (self), thunar_file);
          g_object_unref (thunar_file);
        }
    }
}

#if VTE_CHECK_VERSION(0, 78, 0)
static void
on_directory_changed (VteTerminal *terminal,
                      const gchar *prop,
                      gpointer     user_data)
{
  ThunarTerminalWidget *self = THUNAR_TERMINAL_WIDGET (user_data);

  /* We only care about the property for the current directory URI */
  if (g_strcmp0 (prop, VTE_TERMPROP_CURRENT_DIRECTORY_URI) == 0)
    {
      g_autoptr (GUri) cwd_uri = vte_terminal_ref_termprop_uri (terminal, VTE_TERMPROP_CURRENT_DIRECTORY_URI);
      if (cwd_uri)
        {
          g_autofree gchar *cwd_uri_str = g_uri_to_string (cwd_uri);
          _sync_terminal_to_fm (self, cwd_uri_str);
        }
    }
}
#else
static void
on_legacy_directory_changed (VteTerminal *terminal,
                             gpointer     user_data)
{
  ThunarTerminalWidget *self = THUNAR_TERMINAL_WIDGET (user_data);
  const gchar          *cwd_uri_str = vte_terminal_get_current_directory_uri (terminal);
  _sync_terminal_to_fm (self, cwd_uri_str);
}
#endif


static void
feed_cd_command (VteTerminal *terminal, const char *path)
{
  g_return_if_fail (VTE_IS_TERMINAL (terminal));
  g_return_if_fail (path != NULL);

  g_autofree gchar *quoted_path = g_shell_quote (path);
  g_autofree gchar *cd_command_str = g_strdup_printf (" cd %s\r", quoted_path);

  /* This sequence aims to preserve the user's current input line while changing directory. */
  vte_terminal_feed_child (terminal, SHELL_CTRL_A, -1);
  vte_terminal_feed_child (terminal, " ", -1);
  vte_terminal_feed_child (terminal, SHELL_CTRL_A, -1);
  vte_terminal_feed_child (terminal, SHELL_CTRL_K, -1);
  vte_terminal_feed_child (terminal, cd_command_str, -1);
  vte_terminal_feed_child (terminal, SHELL_CTRL_Y, -1);
  vte_terminal_feed_child (terminal, SHELL_CTRL_A, -1);
  vte_terminal_feed_child (terminal, SHELL_DELETE, -1);
  vte_terminal_feed_child (terminal, SHELL_CTRL_E, -1);
}

static void
on_copy_activate (GtkMenuItem *menuitem,
                  gpointer     user_data)
{
  ThunarTerminalWidget        *self = THUNAR_TERMINAL_WIDGET (user_data);
  ThunarTerminalWidgetPrivate *priv = self->priv;

  vte_terminal_copy_clipboard_format (priv->terminal, VTE_FORMAT_TEXT);
}

static void
on_ssh_exit_activate (GtkMenuItem *menuitem,
                      gpointer     user_data)
{
  ThunarTerminalWidget        *self = THUNAR_TERMINAL_WIDGET (user_data);
  ThunarTerminalWidgetPrivate *priv = self->priv;
  if (priv->state == THUNAR_TERMINAL_STATE_IN_SSH)
    {
      /* Just send the exit command. The "child-exited" signal handler
       * will take care of resetting the state and respawning a new shell. */
      vte_terminal_feed_child (priv->terminal, " exit\n", -1);
    }
}

static GtkWidget *
_create_radio_menu_item (GSList     **group,
                         const gchar *label,
                         gboolean     is_active,
                         GCallback    activate_callback,
                         gpointer     user_data,
                         const gchar *data_key,
                         gpointer     data_value)
{
  GtkWidget *item = gtk_radio_menu_item_new_with_label (*group, label);
  if (*group == NULL)
    *group = gtk_radio_menu_item_get_group (GTK_RADIO_MENU_ITEM (item));

  gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (item), is_active);
  g_signal_connect (item, "activate", activate_callback, user_data);
  /* Attach the preference value to the menu item for easy retrieval in the callback. */
  g_object_set_data (G_OBJECT (item), data_key, data_value);

  return item;
}

static GtkWidget *
_build_color_scheme_submenu (ThunarTerminalWidget *self)
{
  GtkWidget   *submenu = gtk_menu_new ();
  GSList      *radio_group = NULL;
  const gchar *current_scheme = thunar_terminal_widget_get_color_scheme (self);

  for (gsize i = 0; i < G_N_ELEMENTS (COLOR_SCHEME_ENTRIES); ++i)
    {
      GtkWidget *item = _create_radio_menu_item (&radio_group,
                                                 _(COLOR_SCHEME_ENTRIES[i].label_pot),
                                                 g_strcmp0 (current_scheme, COLOR_SCHEME_ENTRIES[i].id) == 0,
                                                 G_CALLBACK (on_color_scheme_changed),
                                                 self,
                                                 DATA_KEY_VALUE,
                                                 (gpointer) COLOR_SCHEME_ENTRIES[i].id);
      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
    }
  return submenu;
}

static int
get_terminal_font_size (void)
{
  int saved_size_pts;
  g_object_get (thunar_preferences_get (), "terminal-font-size", &saved_size_pts, NULL);
  /* Clamp the value on retrieval for robustness against manual config edits. */
  return CLAMP (saved_size_pts, MIN_FONT_SIZE, MAX_FONT_SIZE);
}

static GtkWidget *
_build_font_size_submenu (ThunarTerminalWidget *self)
{
  GtkWidget *submenu = gtk_menu_new ();
  GSList    *radio_group = NULL;
  int        current_size_pts = get_terminal_font_size ();

  for (gsize i = 0; i < G_N_ELEMENTS (FONT_SIZE_ENTRIES); ++i)
    {
      g_autofree gchar *label = g_strdup_printf ("%d", FONT_SIZE_ENTRIES[i].size_pts);
      GtkWidget        *item = _create_radio_menu_item (&radio_group,
                                                        label,
                                                        current_size_pts == FONT_SIZE_ENTRIES[i].size_pts,
                                                        G_CALLBACK (on_font_size_changed),
                                                        self,
                                                        DATA_KEY_VALUE,
                                                        GINT_TO_POINTER (FONT_SIZE_ENTRIES[i].size_pts));
      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
    }
  return submenu;
}

static GtkWidget *
_build_local_sync_submenu (ThunarTerminalWidget *self)
{
  ThunarTerminalWidgetPrivate *priv = self->priv;
  GtkWidget                   *submenu = gtk_menu_new ();
  GSList                      *radio_group = NULL;
  for (gsize i = 0; i < G_N_ELEMENTS (LOCAL_SYNC_MODE_ENTRIES); ++i)
    {
      GtkWidget *item = _create_radio_menu_item (&radio_group,
                                                 _(LOCAL_SYNC_MODE_ENTRIES[i].label_pot),
                                                 priv->local_sync_mode == LOCAL_SYNC_MODE_ENTRIES[i].mode,
                                                 G_CALLBACK (on_enum_pref_changed),
                                                 (gpointer) "terminal-local-sync-mode",
                                                 DATA_KEY_VALUE,
                                                 GINT_TO_POINTER (LOCAL_SYNC_MODE_ENTRIES[i].mode));
      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
    }
  return submenu;
}

static GtkWidget *
_build_sftp_auto_connect_submenu (ThunarTerminalWidget *self)
{
  ThunarTerminalWidgetPrivate *priv = self->priv;
  GtkWidget                   *submenu = gtk_menu_new ();
  GSList                      *radio_group = NULL;
  for (gsize i = 0; i < G_N_ELEMENTS (SFTP_AUTO_CONNECT_ENTRIES); ++i)
    {
      GtkWidget *item = _create_radio_menu_item (&radio_group,
                                                 _(SFTP_AUTO_CONNECT_ENTRIES[i].label_pot),
                                                 priv->ssh_auto_connect_mode == SFTP_AUTO_CONNECT_ENTRIES[i].mode,
                                                 G_CALLBACK (on_enum_pref_changed),
                                                 (gpointer) "terminal-ssh-auto-connect-mode",
                                                 DATA_KEY_VALUE,
                                                 GINT_TO_POINTER (SFTP_AUTO_CONNECT_ENTRIES[i].mode));
      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
    }
  return submenu;
}

static GtkWidget *
_build_manual_ssh_connect_submenu (ThunarTerminalWidget *self,
                                   const gchar          *hostname,
                                   const gchar          *username,
                                   const gchar          *port)
{
  GtkWidget *submenu = gtk_menu_new ();
  for (gsize i = 0; i < G_N_ELEMENTS (MANUAL_SSH_SYNC_ENTRIES); ++i)
    {
      GtkWidget *item = gtk_menu_item_new_with_label (_(MANUAL_SSH_SYNC_ENTRIES[i].label_pot));
      g_object_set_data_full (G_OBJECT (item), DATA_KEY_SSH_HOSTNAME, g_strdup (hostname), g_free);
      g_object_set_data_full (G_OBJECT (item), DATA_KEY_SSH_USERNAME, g_strdup (username), g_free);
      g_object_set_data_full (G_OBJECT (item), DATA_KEY_SSH_PORT, g_strdup (port), g_free);
      g_object_set_data (G_OBJECT (item), DATA_KEY_SSH_SYNC_MODE, GINT_TO_POINTER (MANUAL_SSH_SYNC_ENTRIES[i].mode));
      g_signal_connect (item, "activate", G_CALLBACK (on_ssh_connect_activate), self);
      gtk_menu_shell_append (GTK_MENU_SHELL (submenu), item);
    }
  return submenu;
}

static void
_append_menu_item (GtkMenuShell *menu, const gchar *label, GCallback callback, gpointer user_data)
{
  GtkWidget *item = gtk_menu_item_new_with_mnemonic (label);
  g_signal_connect (item, "activate", callback, user_data);
  gtk_menu_shell_append (menu, item);
}

static void
_append_menu_item_with_submenu (GtkMenuShell *menu, const gchar *label, GtkWidget *submenu)
{
  GtkWidget *item = gtk_menu_item_new_with_label (label);
  gtk_menu_item_set_submenu (GTK_MENU_ITEM (item), submenu);
  gtk_menu_shell_append (menu, item);
}

static GtkWidget *
create_terminal_popup_menu (ThunarTerminalWidget *self)
{
  ThunarTerminalWidgetPrivate *priv = self->priv;
  GtkWidget                   *menu = gtk_menu_new ();
  gboolean                     is_sftp_location = FALSE;
  GtkWidget                   *item;

  item = gtk_menu_item_new_with_mnemonic (_("_Copy"));
  g_signal_connect (item, "activate", G_CALLBACK (on_copy_activate), self);
  gtk_widget_set_sensitive (item, vte_terminal_get_has_selection (priv->terminal));
  gtk_menu_shell_append (GTK_MENU_SHELL (menu), item);

  _append_menu_item (GTK_MENU_SHELL (menu), _("_Paste"), G_CALLBACK (vte_terminal_paste_clipboard), priv->terminal);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

  _append_menu_item (GTK_MENU_SHELL (menu), _("Select _All"), G_CALLBACK (vte_terminal_select_all), priv->terminal);

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

  _append_menu_item_with_submenu (GTK_MENU_SHELL (menu), _("Color Scheme"), _build_color_scheme_submenu (self));
  _append_menu_item_with_submenu (GTK_MENU_SHELL (menu), _("Font Size"), _build_font_size_submenu (self));
  _append_menu_item_with_submenu (GTK_MENU_SHELL (menu), _("SSH Auto-Connect"), _build_sftp_auto_connect_submenu (self));

  gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());

  if (priv->current_location)
    {
      g_autofree gchar *scheme = g_file_get_uri_scheme (priv->current_location);
      if (scheme && g_strcmp0 (scheme, "sftp") == 0)
        is_sftp_location = TRUE;
    }

  if (priv->state == THUNAR_TERMINAL_STATE_IN_SSH)
    {
      _append_menu_item (GTK_MENU_SHELL (menu), _("Disconnect from SSH"), G_CALLBACK (on_ssh_exit_activate), self);
    }
  else /* Local state */
    {
      _append_menu_item_with_submenu (GTK_MENU_SHELL (menu), _("Local Folder Sync"), _build_local_sync_submenu (self));

      if (is_sftp_location)
        {
          g_autofree gchar *hostname = NULL, *username = NULL, *port = NULL;
          if (parse_gvfs_ssh_path (priv->current_location, &hostname, &username, &port))
            {
              gtk_menu_shell_append (GTK_MENU_SHELL (menu), gtk_separator_menu_item_new ());
              g_autofree gchar *label = g_strdup_printf (_("SSH Connection to %s"), hostname);
              _append_menu_item_with_submenu (GTK_MENU_SHELL (menu), label, _build_manual_ssh_connect_submenu (self, hostname, username, port));
            }
        }
    }

  gtk_widget_show_all (menu);
  return menu;
}

static gboolean
on_terminal_button_release (GtkWidget      *widget,
                            GdkEventButton *event,
                            gpointer        user_data)
{
  ThunarTerminalWidget *self = THUNAR_TERMINAL_WIDGET (user_data);

  if (event->button == GDK_BUTTON_SECONDARY)
    {
      GtkWidget *menu = create_terminal_popup_menu (self);
      gtk_menu_popup_at_pointer (GTK_MENU (menu), (GdkEvent *) event);
      return TRUE;
    }
  return FALSE;
}

static gchar *
get_remote_path_from_sftp_gfile (GFile *location)
{
  g_autoptr (GUri) uri = NULL;
  g_autofree gchar *uri_str = g_file_get_uri (location);
  if (!uri_str)
    return NULL;

  uri = g_uri_parse (uri_str, G_URI_FLAGS_NONE, NULL);
  if (uri)
    return g_strdup (g_uri_get_path (uri));

  return NULL;
}

static void
change_directory_in_terminal (ThunarTerminalWidget *self,
                              GFile                *location)
{
  ThunarTerminalWidgetPrivate *priv = self->priv;
  g_autofree gchar            *target_path = NULL;
  gboolean                     should_sync = FALSE;

  /* Do not sync directory if the terminal is not visible or has no child process */
  if (!priv->is_visible || priv->child_pid == -1)
    return;

  if (priv->state == THUNAR_TERMINAL_STATE_IN_SSH)
    {
      if (priv->ssh_sync_mode == THUNAR_TERMINAL_SYNC_BOTH || priv->ssh_sync_mode == THUNAR_TERMINAL_SYNC_FM_TO_TERM)
        {
          should_sync = TRUE;
          target_path = get_remote_path_from_sftp_gfile (location);
        }
    }
  else /* Local state */
    {
      if (priv->local_sync_mode == THUNAR_TERMINAL_SYNC_BOTH || priv->local_sync_mode == THUNAR_TERMINAL_SYNC_FM_TO_TERM)
        {
          if (g_file_query_exists (location, NULL))
            {
              target_path = g_file_get_path (location);
              if (target_path != NULL)
                {
                  should_sync = TRUE;
                }
            }
        }
    }

  if (should_sync && target_path)
    {
      g_autofree gchar *term_uri_str = NULL;

/* Conditionally get the current directory using the appropriate VTE function. */
#if VTE_CHECK_VERSION(0, 78, 0)
      g_autoptr (GUri) term_uri = vte_terminal_ref_termprop_uri (priv->terminal, VTE_TERMPROP_CURRENT_DIRECTORY_URI);
      if (term_uri)
        {
          term_uri_str = g_uri_to_string (term_uri);
        }
#else
      const gchar *temp_uri_str = vte_terminal_get_current_directory_uri (priv->terminal);
      if (temp_uri_str)
        {
          term_uri_str = g_strdup (temp_uri_str);
        }
#endif

      g_autoptr (GFile) term_gfile = term_uri_str ? g_file_new_for_uri (term_uri_str) : NULL;
      g_autofree gchar *term_path = term_gfile ? g_file_get_path (term_gfile) : NULL;

      /* Only issue a 'cd' command if the terminal is not already in the target directory. */
      if (term_path == NULL || g_strcmp0 (term_path, target_path) != 0)
        {
          priv->ignore_next_terminal_cd_signal = TRUE;
          feed_cd_command (priv->terminal, target_path);
        }
    }
}

static gboolean
parse_gvfs_ssh_path (GFile  *location,
                     gchar **hostname,
                     gchar **username,
                     gchar **port)
{
  g_autoptr (GUri) uri = NULL;
  g_autofree gchar *uri_str = g_file_get_uri (location);

  *hostname = NULL;
  *username = NULL;
  *port = NULL;

  if (!uri_str)
    return FALSE;

  uri = g_uri_parse (uri_str, G_URI_FLAGS_NONE, NULL);
  if (uri && g_strcmp0 (g_uri_get_scheme (uri), "sftp") == 0)
    {
      *hostname = g_strdup (g_uri_get_host (uri));
      if (g_uri_get_userinfo (uri))
        *username = g_strdup (g_uri_get_userinfo (uri));
      if (g_uri_get_port (uri) > 0)
        *port = g_strdup_printf ("%d", g_uri_get_port (uri));
      return (*hostname != NULL);
    }

  return FALSE;
}

/**
 * _initiate_ssh_connection:
 * @self: The #ThunarTerminalWidget instance.
 * @hostname: The hostname to connect to.
 * @username: The username for the connection (can be %NULL).
 * @port: The port for the connection (can be %NULL).
 * @sync_mode: The synchronization mode for this SSH session.
 *
 * The command executed on the remote host is carefully constructed to first
 * change to the target directory and then start a new interactive shell.
 * It also injects a `PROMPT_COMMAND` to enable directory tracking via OSC 7
 * escape sequences, which is necessary for Terminal -> File Manager sync.
 */
static void
_initiate_ssh_connection (ThunarTerminalWidget  *self,
                          const gchar           *hostname,
                          const gchar           *username,
                          const gchar           *port,
                          ThunarTerminalSyncMode sync_mode)
{
  ThunarTerminalWidgetPrivate *priv = self->priv;
  g_autoptr (GPtrArray) argv_array = g_ptr_array_new_with_free_func (g_free);
  g_autofree gchar *remote_cmd = NULL;
  GString          *remote_cmd_builder;

  /* If there is already a child process (local shell), terminate it. */
  if (priv->child_pid != -1)
    {
      kill (priv->child_pid, SIGTERM);
      priv->child_pid = -1;
    }

  /* Set the widget state to SSH mode and store connection details. */
  priv->state = THUNAR_TERMINAL_STATE_IN_SSH;
  priv->ssh_sync_mode = sync_mode;
  priv->ssh_hostname = g_strdup (hostname);
  priv->ssh_username = g_strdup (username);
  priv->ssh_port = g_strdup (port);

  if (priv->current_location)
    priv->ssh_remote_path = get_remote_path_from_sftp_gfile (priv->current_location);

  /* Build the command to be executed on the remote side. */
  remote_cmd_builder = g_string_new ("");
  if (priv->ssh_remote_path && *priv->ssh_remote_path)
    {
      /* Always quote shell arguments to prevent command injection. */
      g_autofree gchar *quoted_remote_path = g_shell_quote (priv->ssh_remote_path);
      /* Use ';' to ensure the shell starts even if 'cd' fails. */
      g_string_append_printf (remote_cmd_builder, " cd %s; ", quoted_remote_path);
    }
  /* Inject PROMPT_COMMAND to emit OSC 7 escape sequences for directory tracking. */
  if (priv->ssh_sync_mode == THUNAR_TERMINAL_SYNC_BOTH || priv->ssh_sync_mode == THUNAR_TERMINAL_SYNC_TERM_TO_FM)
    {
      g_string_append (remote_cmd_builder, " export PROMPT_COMMAND='echo -en \"\\033]7;file://$PWD\\007\"'; ");
    }
  g_string_append (remote_cmd_builder, "$SHELL -l");
  remote_cmd = g_string_free (remote_cmd_builder, FALSE);

  /* Build the argument vector (argv) for vte_terminal_spawn_async. */
  g_ptr_array_add (argv_array, g_strdup ("ssh"));
  g_ptr_array_add (argv_array, g_strdup ("-t")); /* Force pseudo-terminal allocation. */
  if (port && *port)
    {
      g_ptr_array_add (argv_array, g_strdup ("-p"));
      g_ptr_array_add (argv_array, g_strdup (port));
    }
  if (username && *username)
    g_ptr_array_add (argv_array, g_strdup_printf ("%s@%s", username, hostname));
  else
    g_ptr_array_add (argv_array, g_strdup (hostname));

  g_ptr_array_add (argv_array, g_strdup (remote_cmd));
  g_ptr_array_add (argv_array, NULL); /* Null terminator for argv. */

  gtk_widget_show (priv->ssh_indicator);

  /*
   * Set a flag so that the first 'directory-changed' emission from VTE
   * (which happens automatically upon connection) is ignored, preventing a redundant 'cd'.
   */
  priv->ignore_next_terminal_cd_signal = TRUE;

  vte_terminal_spawn_async (priv->terminal,
                            VTE_PTY_DEFAULT,
                            NULL, /* working_directory (user's home) */
                            (gchar **) argv_array->pdata,
                            NULL, /* envv */
                            G_SPAWN_SEARCH_PATH,
                            NULL, /* child_setup */
                            NULL, /* child_setup_data */
                            NULL, /* child_setup_data_destroy */
                            -1,   /* timeout */
                            priv->spawn_cancellable,
                            (VteTerminalSpawnAsyncCallback) spawn_async_callback,
                            self);
  thunar_terminal_widget_ensure_terminal_focus (self);
}

static void
save_terminal_font_size (int font_size_pts)
{
  g_object_set (thunar_preferences_get (), "terminal-font-size", font_size_pts, NULL);
}

static void
on_color_scheme_changed (GtkCheckMenuItem *menuitem,
                         gpointer          user_data)
{
  ThunarTerminalWidget *self = THUNAR_TERMINAL_WIDGET (user_data);
  if (gtk_check_menu_item_get_active (menuitem))
    {
      const gchar *scheme_name = g_object_get_data (G_OBJECT (menuitem), DATA_KEY_VALUE);
      thunar_terminal_widget_set_color_scheme (self, scheme_name);
    }
}

static void
on_font_size_changed (GtkCheckMenuItem *menuitem,
                      gpointer          user_data)
{
  ThunarTerminalWidget        *self = THUNAR_TERMINAL_WIDGET (user_data);
  ThunarTerminalWidgetPrivate *priv = self->priv;
  if (gtk_check_menu_item_get_active (menuitem))
    {
      int font_size_pts = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem), DATA_KEY_VALUE));
      g_autoptr (PangoFontDescription) font_desc = pango_font_description_copy (vte_terminal_get_font (priv->terminal));
      pango_font_description_set_size (font_desc, font_size_pts * PANGO_SCALE);
      vte_terminal_set_font (priv->terminal, font_desc);
      save_terminal_font_size (font_size_pts);
    }
}

/* Generic callback to handle changes for any enum-based preference from the menu. */
static void
on_enum_pref_changed (GtkCheckMenuItem *menuitem,
                      gpointer          user_data)
{
  const gchar *pref_name = user_data;
  if (gtk_check_menu_item_get_active (menuitem))
    {
      gint new_value = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem), DATA_KEY_VALUE));
      g_object_set (thunar_preferences_get (), pref_name, new_value, NULL);
    }
}

static void
on_ssh_connect_activate (GtkMenuItem *menuitem,
                         gpointer     user_data)
{
  ThunarTerminalWidget  *self = THUNAR_TERMINAL_WIDGET (user_data);
  const gchar           *hostname = g_object_get_data (G_OBJECT (menuitem), DATA_KEY_SSH_HOSTNAME);
  const gchar           *username = g_object_get_data (G_OBJECT (menuitem), DATA_KEY_SSH_USERNAME);
  const gchar           *port = g_object_get_data (G_OBJECT (menuitem), DATA_KEY_SSH_PORT);
  ThunarTerminalSyncMode sync_mode = GPOINTER_TO_INT (g_object_get_data (G_OBJECT (menuitem), DATA_KEY_SSH_SYNC_MODE));

  if (hostname)
    _initiate_ssh_connection (self, hostname, username, port, sync_mode);
}

static ThunarFile *
thunar_terminal_widget_get_current_directory (ThunarNavigator *navigator)
{
  ThunarTerminalWidget        *self = THUNAR_TERMINAL_WIDGET (navigator);
  ThunarTerminalWidgetPrivate *priv = self->priv;
  ThunarFile                  *current_directory = NULL;

  if (priv->current_location)
    current_directory = thunar_file_get (priv->current_location, NULL);

  return current_directory;
}

static void
thunar_terminal_widget_set_current_directory (ThunarNavigator *navigator,
                                              ThunarFile      *current_directory)
{
  ThunarTerminalWidget *self = THUNAR_TERMINAL_WIDGET (navigator);
  GFile                *location = NULL;

  if (current_directory)
    location = thunar_file_get_file (current_directory);

  thunar_terminal_widget_set_current_location (self, location);
}

static void
thunar_terminal_widget_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_terminal_widget_get_current_directory;
  iface->set_current_directory = thunar_terminal_widget_set_current_directory;
}