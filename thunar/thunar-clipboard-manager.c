/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif

#include <thunar/thunar-application.h>
#include <thunar/thunar-clipboard-manager.h>



enum
{
  PROP_0,
  PROP_CAN_PASTE,
};

enum
{
  CHANGED,
  LAST_SIGNAL,
};

enum
{
  TARGET_GNOME_COPIED_FILES,
  TARGET_UTF8_STRING,
};



static void thunar_clipboard_manager_class_init         (ThunarClipboardManagerClass *klass);
static void thunar_clipboard_manager_finalize           (GObject                     *object);
static void thunar_clipboard_manager_get_property       (GObject                     *object,
                                                         guint                        prop_id,
                                                         GValue                      *value,
                                                         GParamSpec                  *pspec);
static void thunar_clipboard_manager_owner_changed      (GtkClipboard                *clipboard,
                                                         GdkEventOwnerChange         *event,
                                                         ThunarClipboardManager      *manager);
static void thunar_clipboard_manager_contents_received  (GtkClipboard                *clipboard,
                                                         GtkSelectionData            *selection_data,
                                                         gpointer                     user_data);
static void thunar_clipboard_manager_targets_received   (GtkClipboard                *clipboard,
                                                         GtkSelectionData            *selection_data,
                                                         gpointer                     user_data);
static void thunar_clipboard_manager_get_callback       (GtkClipboard                *clipboard,
                                                         GtkSelectionData            *selection_data,
                                                         guint                        info,
                                                         gpointer                     user_data);
static void thunar_clipboard_manager_clear_callback     (GtkClipboard                *clipboard,
                                                         gpointer                     user_data);



struct _ThunarClipboardManagerClass
{
  GObjectClass __parent__;

  void (*changed) (ThunarClipboardManager *manager);
};

struct _ThunarClipboardManager
{
  GObject __parent__;

  GtkClipboard *clipboard;
  gboolean      can_paste;
  GdkAtom       x_special_gnome_copied_files;

  gboolean      uri_copy;
  GList        *uri_list;
};

typedef struct
{
  gboolean copy;
  GList   *uri_list;
} ThunarClipboardInfo;

typedef struct
{
  ThunarClipboardManager *manager;
  ThunarVfsURI           *target_uri;
  GtkWindow              *window;
} ThunarClipboardPasteRequest;



static const GtkTargetEntry clipboard_targets[] =
{
  { "x-special/gnome-copied-files", 0, TARGET_GNOME_COPIED_FILES },
  { "UTF8_STRING", 0, TARGET_UTF8_STRING }
};

static GQuark thunar_clipboard_manager_quark = 0;
static guint  manager_signals[LAST_SIGNAL];



G_DEFINE_TYPE (ThunarClipboardManager, thunar_clipboard_manager, G_TYPE_OBJECT);



static void
thunar_clipboard_manager_class_init (ThunarClipboardManagerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_clipboard_manager_finalize;
  gobject_class->get_property = thunar_clipboard_manager_get_property;

  /**
   * ThunarClipboardManager:can-paste:
   *
   * This property tells whether the current clipboard content of
   * this #ThunarClipboardManager can be pasted into a folder
   * displayed by a #ThunarView.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_CAN_PASTE,
                                   g_param_spec_boolean ("can-paste",
                                                         _("Can paste"),
                                                         _("Whether the clipboard content can be pasted"),
                                                         FALSE,
                                                         EXO_PARAM_READABLE));

  /**
   * ThunarClipboardManager::changed:
   * @manager : a #ThunarClipboardManager.
   *
   * This signal is emitted whenever the contents of the
   * clipboard associated with @manager changes.
   **/
  manager_signals[CHANGED] =
    g_signal_new ("changed",
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_FIRST,
                  G_STRUCT_OFFSET (ThunarClipboardManagerClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



static void
thunar_clipboard_manager_init (ThunarClipboardManager *manager)
{
  manager->can_paste = FALSE;
  manager->x_special_gnome_copied_files = gdk_atom_intern ("x-special/gnome-copied-files", FALSE);
}



static void
thunar_clipboard_manager_finalize (GObject *object)
{
  ThunarClipboardManager *manager = THUNAR_CLIPBOARD_MANAGER (object);

  /* free any pending URI list */
  thunar_vfs_uri_list_free (manager->uri_list);

  /* disconnect from the clipboard */
  g_signal_handlers_disconnect_by_func (G_OBJECT (manager->clipboard), thunar_clipboard_manager_owner_changed, manager);
  g_object_set_qdata (G_OBJECT (manager->clipboard), thunar_clipboard_manager_quark, NULL);
  g_object_unref (G_OBJECT (manager->clipboard));

  G_OBJECT_CLASS (thunar_clipboard_manager_parent_class)->finalize (object);
}



static void
thunar_clipboard_manager_get_property (GObject    *object,
                                       guint       prop_id,
                                       GValue     *value,
                                       GParamSpec *pspec)
{
  ThunarClipboardManager *manager = THUNAR_CLIPBOARD_MANAGER (object);

  switch (prop_id)
    {
    case PROP_CAN_PASTE:
      g_value_set_boolean (value, thunar_clipboard_manager_get_can_paste (manager));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_clipboard_manager_owner_changed (GtkClipboard           *clipboard,
                                        GdkEventOwnerChange    *event,
                                        ThunarClipboardManager *manager)
{
  g_return_if_fail (GTK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (manager));
  g_return_if_fail (manager->clipboard == clipboard);

  /* need to take a reference on the manager, because the clipboards
   * "targets received callback" mechanism is not cancellable.
   */
  g_object_ref (G_OBJECT (manager));

  /* request the list of supported targets from the new owner */
  gtk_clipboard_request_contents (clipboard, gdk_atom_intern ("TARGETS", FALSE),
                                  thunar_clipboard_manager_targets_received, manager);
}



static void
thunar_clipboard_manager_contents_received (GtkClipboard     *clipboard,
                                            GtkSelectionData *selection_data,
                                            gpointer          user_data)
{
  ThunarClipboardPasteRequest *request = user_data;
  ThunarClipboardManager      *manager = THUNAR_CLIPBOARD_MANAGER (request->manager);
  ThunarApplication           *application;
  GtkWidget                   *dialog;
  gboolean                     copy = TRUE;
  GList                       *uri_list = NULL;
  gchar                       *data;

  /* check whether the retrieval worked */
  if (G_LIKELY (selection_data->length > 0))
    {
      /* be sure the selection data is zero-terminated */
      data = (gchar *) selection_data->data;
      data[selection_data->length] = '\0';

      /* check whether to copy or move */
      if (g_ascii_strncasecmp (data, "copy\n", 5) == 0)
        {
          copy = TRUE;
          data += 5;
        }
      else if (g_ascii_strncasecmp (data, "cut\n", 4) == 0)
        {
          copy = FALSE;
          data += 4;
        }

      /* determine the uri list stored with the selection */
      uri_list = thunar_vfs_uri_list_from_string (data, NULL);
    }

  /* perform the action if possible */
  if (G_LIKELY (uri_list != NULL))
    {
      application = thunar_application_get ();
      if (G_LIKELY (copy))
        thunar_application_copy_uris (application, request->window, uri_list, request->target_uri);
      else
        thunar_application_move_uris (application, request->window, uri_list, request->target_uri);
      g_object_unref (G_OBJECT (application));
      thunar_vfs_uri_list_free (uri_list);

      /* clear the clipboard if it contained "cutted data"
       * (gtk_clipboard_clear takes care of not clearing
       * the selection if we don't own it)
       */
      if (G_UNLIKELY (!copy))
        gtk_clipboard_clear (manager->clipboard);

      /* check the contents of the clipboard again
       * if either the Xserver or our GTK+ version
       * doesn't support the XFixes extension.
       */
#if GTK_CHECK_VERSION(2,6,0)
      if (!gdk_display_supports_selection_notification (gtk_clipboard_get_display (manager->clipboard)))
#endif
        {
          thunar_clipboard_manager_owner_changed (manager->clipboard, NULL, manager);
        }
    }
  else
    {
      /* tell the user that we cannot paste */
      dialog = gtk_message_dialog_new (request->window,
                                       GTK_DIALOG_MODAL |
                                       GTK_DIALOG_DESTROY_WITH_PARENT,
                                       GTK_MESSAGE_WARNING,
                                       GTK_BUTTONS_OK,
                                       _("There is nothing on the clipboard to paste"));
      gtk_dialog_run (GTK_DIALOG (dialog));
      gtk_widget_destroy (dialog);
    }

  /* free the request */
  if (G_LIKELY (request->window != NULL))
    g_object_remove_weak_pointer (G_OBJECT (request->window), (gpointer) &request->window);
  g_object_unref (G_OBJECT (request->manager));
  thunar_vfs_uri_unref (request->target_uri);
  g_free (request);
}



static void
thunar_clipboard_manager_targets_received (GtkClipboard     *clipboard,
                                           GtkSelectionData *selection_data,
                                           gpointer          user_data)
{
  ThunarClipboardManager *manager = THUNAR_CLIPBOARD_MANAGER (user_data);
  GdkAtom                *targets;
  gint                    n_targets;
  gint                    n;

  g_return_if_fail (GTK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (manager));
  g_return_if_fail (manager->clipboard == clipboard);

  /* reset the "can-paste" state */
  manager->can_paste = FALSE;

  /* check the list of targets provided by the owner */
  if (gtk_selection_data_get_targets (selection_data, &targets, &n_targets))
    {
      for (n = 0; n < n_targets; ++n)
        if (targets[n] == manager->x_special_gnome_copied_files)
          {
            manager->can_paste = TRUE;
            break;
          }

      g_free (targets);
    }

  /* notify listeners that we have a new clipboard state */
  g_signal_emit (G_OBJECT (manager), manager_signals[CHANGED], 0);
  g_object_notify (G_OBJECT (manager), "can-paste");

  /* drop the reference taken for the callback */
  g_object_unref (G_OBJECT (manager));
}



static void
thunar_clipboard_manager_get_callback (GtkClipboard     *clipboard,
                                       GtkSelectionData *selection_data,
                                       guint             target_info,
                                       gpointer          user_data)
{
  ThunarClipboardManager *manager = THUNAR_CLIPBOARD_MANAGER (user_data);
  gchar                  *string_list;
  gchar                  *data;

  g_return_if_fail (GTK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (manager));
  g_return_if_fail (manager->clipboard == clipboard);

  string_list = thunar_vfs_uri_list_to_string (manager->uri_list);

  switch (target_info)
    {
    case TARGET_GNOME_COPIED_FILES:
      data = g_strconcat (manager->uri_copy ? "copy" : "cut", "\n", string_list, NULL);
      gtk_selection_data_set (selection_data, selection_data->target, 8, (guchar *) data, strlen (data));
      g_free (data);
      break;

    case TARGET_UTF8_STRING:
      gtk_selection_data_set (selection_data, selection_data->target, 8, (guchar *) string_list, strlen (string_list));
      break;

    default:
      g_assert_not_reached ();
    }

  g_free (string_list);
}



static void
thunar_clipboard_manager_clear_callback (GtkClipboard *clipboard,
                                         gpointer      user_data)
{
  ThunarClipboardManager *manager = THUNAR_CLIPBOARD_MANAGER (user_data);

  g_return_if_fail (GTK_IS_CLIPBOARD (clipboard));
  g_return_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (manager));
  g_return_if_fail (manager->clipboard == clipboard);

  if (G_LIKELY (manager->uri_list != NULL))
    {
      thunar_vfs_uri_list_free (manager->uri_list);
      manager->uri_list = NULL;
    }
}



/**
 * thunar_clipboard_manager_get_for_display:
 * @display : a #GdkDisplay.
 *
 * Determines the #ThunarClipboardManager that is used to manage
 * the clipboard on the given @display.
 *
 * The caller is responsible for freeing the returned object
 * using #g_object_unref() when it's no longer needed.
 *
 * Return value: the #ThunarClipboardManager for @display.
 **/
ThunarClipboardManager*
thunar_clipboard_manager_get_for_display (GdkDisplay *display)
{
  ThunarClipboardManager *manager;
  GtkClipboard           *clipboard;

  g_return_val_if_fail (GDK_IS_DISPLAY (display), NULL);

  /* generate the quark on-demand */
  if (G_UNLIKELY (thunar_clipboard_manager_quark == 0))
    thunar_clipboard_manager_quark = g_quark_from_static_string ("thunar-clipboard-manager");

  /* figure out the clipboard for the given display */
  clipboard = gtk_clipboard_get_for_display (display, GDK_SELECTION_CLIPBOARD);

  /* check if a clipboard manager exists */
  manager = g_object_get_qdata (G_OBJECT (clipboard), thunar_clipboard_manager_quark);
  if (G_LIKELY (manager != NULL))
    {
      g_object_ref (G_OBJECT (manager));
      return manager;
    }

  /* allocate a new manager */
  manager = g_object_new (THUNAR_TYPE_CLIPBOARD_MANAGER, NULL);
  manager->clipboard = g_object_ref (G_OBJECT (clipboard));
  g_object_set_qdata (G_OBJECT (clipboard), thunar_clipboard_manager_quark, manager);

  /* listen for "owner-change" on the clipboard */
  g_signal_connect (G_OBJECT (manager->clipboard), "owner-change",
                    G_CALLBACK (thunar_clipboard_manager_owner_changed), manager);

  return manager;
}



/**
 * thunar_clipboard_manager_get_can_paste:
 * @manager : a #ThunarClipboardManager.
 *
 * Tells whether the contents of the clipboard represented
 * by @manager can be pasted into a folder.
 *
 * Return value: %TRUE if the contents of the clipboard
 *               represented by @manager can be pasted
 *               into a folder.
 **/
gboolean
thunar_clipboard_manager_get_can_paste (ThunarClipboardManager *manager)
{
  g_return_val_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (manager), FALSE);
  return manager->can_paste;
}



/**
 * thunar_clipboard_manager_copy_uri_list:
 * @manager  : a #ThunarClipboardManager.
 * @uri_list : a list of #ThunarVfsURIs.
 *
 * Sets the clipboard represented by @manager to
 * contain the @uri_list and marks the files represented
 * by the #ThunarVfsURI<!---->s in @uri_list to be copied
 * when the user pastes from the clipboard.
 **/
void
thunar_clipboard_manager_copy_uri_list (ThunarClipboardManager *manager,
                                        GList                  *uri_list)
{
  g_return_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (manager));
  g_return_if_fail (uri_list != NULL);

  /* free any previously set URI list */
  if (G_UNLIKELY (manager->uri_list != NULL))
    thunar_vfs_uri_list_free (manager->uri_list);

  /* setup the new URI list */
  manager->uri_copy = TRUE;
  manager->uri_list = thunar_vfs_uri_list_copy (uri_list);

  /* acquire the CLIPBOARD ownership */
  gtk_clipboard_set_with_owner (manager->clipboard, clipboard_targets,
                                G_N_ELEMENTS (clipboard_targets),
                                thunar_clipboard_manager_get_callback,
                                thunar_clipboard_manager_clear_callback,
                                G_OBJECT (manager));

  /* Need to fake a "owner-change" event here if the Xserver doesn't support clipboard notification */
#if GTK_CHECK_VERSION(2,6,0)
  if (!gdk_display_supports_selection_notification (gtk_clipboard_get_display (manager->clipboard)))
#endif
    {
      thunar_clipboard_manager_owner_changed (manager->clipboard, NULL, manager);
    }
}



/**
 * thunar_clipboard_manager_cut_uri_list:
 * @manager  : a #ThunarClipboardManager.
 * @uri_list : a list of #ThunarVfsURIs.
 *
 * Sets the clipboard represented by @manager to
 * contain the @uri_list and marks the files represented
 * by the #ThunarVfsURI<!---->s in @uri_list to be moved
 * when the user pastes from the clipboard.
 **/
void
thunar_clipboard_manager_cut_uri_list (ThunarClipboardManager *manager,
                                       GList                  *uri_list)
{
  g_return_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (manager));
  g_return_if_fail (uri_list != NULL);

  /* free any previously set URI list */
  if (G_UNLIKELY (manager->uri_list != NULL))
    thunar_vfs_uri_list_free (manager->uri_list);

  /* setup the new URI list */
  manager->uri_copy = FALSE;
  manager->uri_list = thunar_vfs_uri_list_copy (uri_list);

  /* acquire the CLIPBOARD ownership */
  gtk_clipboard_set_with_owner (manager->clipboard, clipboard_targets,
                                G_N_ELEMENTS (clipboard_targets),
                                thunar_clipboard_manager_get_callback,
                                thunar_clipboard_manager_clear_callback,
                                G_OBJECT (manager));

  /* Need to fake a "owner-change" event here if the Xserver doesn't support clipboard notification */
#if GTK_CHECK_VERSION(2,6,0)
  if (!gdk_display_supports_selection_notification (gtk_clipboard_get_display (manager->clipboard)))
#endif
    {
      thunar_clipboard_manager_owner_changed (manager->clipboard, NULL, manager);
    }
}



/**
 * thunar_clipboard_manager:
 * @manager    : a #ThunarClipboardManager.
 * @target_uri : the #ThunarVfsURI of the folder to which the contents on the clipboard
 *               should be pasted.
 * @window     : a #GtkWindow, on which to perform the paste or %NULL if no window is
 *               known.
 *
 * Pastes the contents from the clipboard associated with @manager to the directory
 * referenced by @target_uri.
 **/
void
thunar_clipboard_manager_paste_uri_list (ThunarClipboardManager *manager,
                                         ThunarVfsURI           *target_uri,
                                         GtkWindow              *window)
{
  ThunarClipboardPasteRequest *request;

  g_return_if_fail (THUNAR_IS_CLIPBOARD_MANAGER (manager));
  g_return_if_fail (window == NULL || GTK_IS_WINDOW (window));

  /* prepare the paste request */
  request = g_new (ThunarClipboardPasteRequest, 1);
  request->manager = g_object_ref (G_OBJECT (manager));
  request->target_uri = thunar_vfs_uri_ref (target_uri);
  request->window = window;

  /* get notified when the window is destroyed prior to
   * completing the clipboard contents retrieval
   */
  g_object_add_weak_pointer (G_OBJECT (request->window), (gpointer) &request->window);

  /* schedule the request */
  gtk_clipboard_request_contents (manager->clipboard, manager->x_special_gnome_copied_files,
                                  thunar_clipboard_manager_contents_received, request);
}

