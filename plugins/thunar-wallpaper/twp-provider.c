/*-
 * Copyright (c) 2008 Stephan Arts <stephan@xfce.org>
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

#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <xfconf/xfconf.h>
#include <thunar-vfs/thunar-vfs.h>
#include <gdk/gdkx.h>
#include <X11/Xlib.h>

#include "twp-provider.h"

#define XFDESKTOP_SELECTION_FMT "XFDESKTOP_SELECTION_%d"

static void   twp_provider_class_init           (TwpProviderClass         *klass);
static void   twp_provider_menu_provider_init   (ThunarxMenuProviderIface *iface);
static void   twp_provider_property_page_provider_init (ThunarxPropertyPageProviderIface *iface);
static void   twp_provider_init                 (TwpProvider              *twp_provider);
static void   twp_provider_finalize             (GObject                  *object);
static GList *twp_provider_get_file_actions     (ThunarxMenuProvider      *menu_provider,
                                                 GtkWidget                *window,
                                                 GList                    *files);
static GList *twp_provider_get_folder_actions   (ThunarxMenuProvider      *menu_provider,
                                                 GtkWidget                *window,
                                                 ThunarxFileInfo          *folder);

static
twp_action_set_wallpaper (GtkAction *action, gpointer user_data);



struct _TwpProviderClass
{
  GObjectClass __parent__;
};

struct _TwpProvider
{
  GObject         __parent__;

  gchar          *child_watch_path;
  gint            child_watch_id;
};



THUNARX_DEFINE_TYPE_WITH_CODE (TwpProvider,
                               twp_provider,
                               G_TYPE_OBJECT,
                               THUNARX_IMPLEMENT_INTERFACE (THUNARX_TYPE_MENU_PROVIDER,
                                                            twp_provider_menu_provider_init)
                               THUNARX_IMPLEMENT_INTERFACE (THUNARX_TYPE_PROPERTY_PAGE_PROVIDER,
                                                            twp_provider_property_page_provider_init));


static void
twp_provider_class_init (TwpProviderClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = twp_provider_finalize;
}



static void
twp_provider_menu_provider_init (ThunarxMenuProviderIface *iface)
{
  iface->get_file_actions = twp_provider_get_file_actions;
}



static void
twp_provider_property_page_provider_init (ThunarxPropertyPageProviderIface *iface)
{
}



static void
twp_provider_init (TwpProvider *twp_provider)
{
    xfconf_init(NULL);
	//twp_gpg_backend_init();
}



static void
twp_provider_finalize (GObject *object)
{
    TwpProvider *twp_provider = TWP_PROVIDER (object);
    GSource     *source;
    xfconf_shutdown();

   (*G_OBJECT_CLASS (twp_provider_parent_class)->finalize) (object);
}

static GList*
twp_provider_get_file_actions (ThunarxMenuProvider *menu_provider,
                               GtkWidget           *window,
                               GList               *files)
{
	ThunarVfsPathScheme scheme;
	ThunarVfsInfo      *info;
	GtkWidget          *action;
	GList              *actions = NULL;
    gint                n_files = 0;
    gchar               selection_name[100];

    GdkScreen *gdk_screen = gdk_screen_get_default();
    gint xscreen = gdk_screen_get_number(gdk_screen);

    g_snprintf(selection_name, 100, XFDESKTOP_SELECTION_FMT, xscreen);

    Window root_window = GDK_ROOT_WINDOW();
    Atom xfce_selection_atom = XInternAtom (gdk_display, selection_name, False);
    if(!(XGetSelectionOwner(GDK_DISPLAY(), xfce_selection_atom)))
    {
        return NULL;
    }

    /* we can only set a single wallpaper */
	if (files->next == NULL)
	{
		/* check if the file is a local file */
		info = thunarx_file_info_get_vfs_info (files->data);
		scheme = thunar_vfs_path_get_scheme (info->path);
		thunar_vfs_info_unref (info);

		/* unable to handle non-local files */
		if (G_UNLIKELY (scheme != THUNAR_VFS_PATH_SCHEME_FILE))
			return NULL;

		if (!thunarx_file_info_is_directory (files->data))
		{
            if (thunarx_file_info_has_mime_type (files->data, "image/jpeg")
              ||thunarx_file_info_has_mime_type (files->data, "image/png")
              ||thunarx_file_info_has_mime_type (files->data, "image/svg+xml")
              ||thunarx_file_info_has_mime_type (files->data, "image/svg+xml-compressed"))
            {
                action = g_object_new (GTK_TYPE_ACTION,
                            "name", "Twp::setwallpaper",
                            "label", _("Set as Wallpaper"),
                            NULL);
                g_signal_connect (action, "activate", G_CALLBACK (twp_action_set_wallpaper), files->data);

                actions = g_list_append (actions, action);
            }
		}
	}

  return actions;
}


static GList*
twp_Provider_get_pages (ThunarxPropertyPageProvider *page_provider, GList *files)
{
  GList *pages = NULL;
  return pages;
}

static
twp_action_set_wallpaper (GtkAction *action, gpointer user_data)
{
    ThunarxFileInfo *file_info = user_data;
    GdkDisplay *display = gdk_display_get_default();
    gint n_screens = gdk_display_get_n_screens (display);
    gint screen_nr = 0;
    gint n_monitors;
    gint monitor_nr = 0;
    GdkScreen *screen;
    gchar *image_path_prop;
    gchar *image_show_prop;
    gchar *image_style_prop;
    gchar *file_uri;
    gchar *file_name;
    gchar *hostname = NULL;

    XfconfChannel *xfdesktop_channel = xfconf_channel_new("xfce4-desktop");

    file_uri = thunarx_file_info_get_uri (file_info);
    file_name = g_filename_from_uri (file_uri, &hostname, NULL);
    if (hostname == NULL)
    {
        if (n_screens > 1)
        {
            screen = gdk_display_get_default_screen (display);
        }
        else
        {
            screen = gdk_display_get_screen (display, 0);
        }

        n_monitors = gdk_screen_get_n_monitors (screen);

        if (n_monitors > 1)
        {

        }
        
        image_path_prop = g_strdup_printf("/backdrop/screen%d/monitor%d/image-path", screen_nr, monitor_nr);
        image_show_prop = g_strdup_printf("/backdrop/screen%d/monitor%d/image-show", screen_nr, monitor_nr);
        image_style_prop = g_strdup_printf("/backdrop/screen%d/monitor%d/image-style", screen_nr, monitor_nr);


        if(xfconf_channel_set_string(xfdesktop_channel, image_path_prop, file_name) == TRUE)
        {
            xfconf_channel_set_bool(xfdesktop_channel, image_show_prop, TRUE);
            xfconf_channel_set_int(xfdesktop_channel, image_style_prop, 4);
        }

        g_free(image_path_prop);
        g_free(image_show_prop);
        g_free(image_style_prop);

    }
    g_free(file_uri);
    g_free(file_name);
    g_object_unref(xfdesktop_channel);

    
}
