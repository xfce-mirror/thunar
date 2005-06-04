/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2005 Jeff Franks <jcfranks@xfce.org>
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

#include <thunar/thunar-application.h>
#include <thunar/thunar-desktop-view.h>
#include <thunar/thunar-preferences.h>



static void thunar_application_class_init       (ThunarApplicationClass *klass);
static void thunar_application_init             (ThunarApplication      *application);
static void thunar_application_finalize         (GObject                *object);
static void thunar_application_window_destroyed (GtkWidget         *window,
                                                 ThunarApplication *application);



struct _ThunarApplicationClass
{
  GObjectClass __parent__;
};

struct _ThunarApplication
{
  GObject __parent__;

  ThunarDesktopView *desktop_view;
  ThunarPreferences *preferences;
  GList             *windows;
};



static GObjectClass *parent_class;



G_DEFINE_TYPE (ThunarApplication, thunar_application, G_TYPE_OBJECT);



static void
thunar_application_class_init (ThunarApplicationClass *klass)
{
  GObjectClass *gobject_class;

  parent_class = g_type_class_peek_parent (klass);
  
  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_application_finalize;
}



static void
thunar_application_init (ThunarApplication *application)
{
  application->preferences = thunar_preferences_get ();
  application->desktop_view = NULL;
}



static void
thunar_application_finalize (GObject *object)
{
  ThunarApplication *application = THUNAR_APPLICATION (object);
  GList             *lp;

  for (lp = application->windows; lp != NULL; lp = lp->next)
    {
      g_signal_handlers_disconnect_by_func (G_OBJECT (lp->data), G_CALLBACK (thunar_application_window_destroyed), application);
      gtk_widget_destroy (GTK_WIDGET (lp->data));
    }
  g_list_free (application->windows);

  g_object_unref (G_OBJECT (application->preferences));
  
  if (G_LIKELY (application->desktop_view != NULL))
    g_object_unref (G_OBJECT (application->desktop_view));

  G_OBJECT_CLASS (parent_class)->finalize (object);
}



static void
thunar_application_window_destroyed (GtkWidget         *window,
                                     ThunarApplication *application)
{
  g_return_if_fail (GTK_IS_WINDOW (window));
  g_return_if_fail (THUNAR_IS_APPLICATION (application));
  g_return_if_fail (g_list_find (application->windows, window) != NULL);

  application->windows = g_list_remove (application->windows, window);

  /* terminate the application if we don't have any more
   * windows and we don't manage the desktop.
   */
  if (G_UNLIKELY (application->windows == NULL
        && application->desktop_view == NULL))
    {
      gtk_main_quit ();
    }
}

