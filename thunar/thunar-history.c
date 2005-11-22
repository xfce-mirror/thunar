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

#include <thunar/thunar-history.h>
#include <thunar/thunar-navigator.h>



/* Property identifiers */
enum
{
  PROP_0,
  PROP_ACTION_GROUP,
  PROP_CURRENT_DIRECTORY,
};



static void         thunar_history_class_init             (ThunarHistoryClass   *klass);
static void         thunar_history_navigator_init         (ThunarNavigatorIface *iface);
static void         thunar_history_init                   (ThunarHistory        *history);
static void         thunar_history_dispose                (GObject              *object);
static void         thunar_history_finalize               (GObject              *object);
static void         thunar_history_get_property           (GObject              *object,
                                                           guint                 prop_id,
                                                           GValue               *value,
                                                           GParamSpec           *pspec);
static void         thunar_history_set_property           (GObject              *object,
                                                           guint                 prop_id,
                                                           const GValue         *value,
                                                           GParamSpec           *pspec);
static ThunarFile  *thunar_history_get_current_directory  (ThunarNavigator      *navigator);
static void         thunar_history_set_current_directory  (ThunarNavigator      *navigator,
                                                           ThunarFile           *current_directory);
static void         thunar_history_action_back            (GtkAction            *action,
                                                           ThunarHistory        *history);
static void         thunar_history_action_forward         (GtkAction            *action,
                                                           ThunarHistory        *history);



struct _ThunarHistoryClass
{
  GObjectClass __parent__;
};

struct _ThunarHistory
{
  GObject __parent__;

  ThunarFile     *current_directory;

  GtkActionGroup *action_group;

  GtkAction      *action_back;
  GtkAction      *action_forward;

  GList          *back_list;
  GList          *forward_list;
};



static GObjectClass *thunar_history_parent_class;



GType
thunar_history_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarHistoryClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_history_class_init,
        NULL,
        NULL,
        sizeof (ThunarHistory),
        0,
        (GInstanceInitFunc) thunar_history_init,
        NULL,
      };

      static const GInterfaceInfo navigator_info =
      {
        (GInterfaceInitFunc) thunar_history_navigator_init,
        NULL,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, I_("ThunarHistory"), &info, 0);
      g_type_add_interface_static (type, THUNAR_TYPE_NAVIGATOR, &navigator_info);
    }

  return type;
}



static void
thunar_history_class_init (ThunarHistoryClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_history_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->dispose = thunar_history_dispose;
  gobject_class->finalize = thunar_history_finalize;
  gobject_class->get_property = thunar_history_get_property;
  gobject_class->set_property = thunar_history_set_property;

  /**
   * ThunarHistory::action-group:
   *
   * The #GtkActionGroup to which the #ThunarHistory<!---->s
   * actions "back" and "forward" should be connected.
   **/
  g_object_class_install_property (gobject_class,
                                   PROP_ACTION_GROUP,
                                   g_param_spec_object ("action-group",
                                                        _("Action group"),
                                                        _("Action group"),
                                                        GTK_TYPE_ACTION_GROUP,
                                                        EXO_PARAM_READWRITE));

  /**
   * ThunarHistory::current-directory:
   *
   * Inherited from #ThunarNavigator.
   **/
  g_object_class_override_property (gobject_class,
                                    PROP_CURRENT_DIRECTORY,
                                    "current-directory");
}



static void
thunar_history_navigator_init (ThunarNavigatorIface *iface)
{
  iface->get_current_directory = thunar_history_get_current_directory;
  iface->set_current_directory = thunar_history_set_current_directory;
}



static void
thunar_history_init (ThunarHistory *history)
{
  /* create the "back" action */
  history->action_back = gtk_action_new ("back", _("Back"), _("Go to the previous visited folder"), GTK_STOCK_GO_BACK);
  g_signal_connect (G_OBJECT (history->action_back), "activate", G_CALLBACK (thunar_history_action_back), history);
  gtk_action_set_sensitive (history->action_back, FALSE);

  /* create the "forward" action */
  history->action_forward = gtk_action_new ("forward", _("Forward"), _("Go to the next visited folder"), GTK_STOCK_GO_FORWARD);
  g_signal_connect (G_OBJECT (history->action_forward), "activate", G_CALLBACK (thunar_history_action_forward), history);
  gtk_action_set_sensitive (history->action_forward, FALSE);
}



static void
thunar_history_dispose (GObject *object)
{
  ThunarHistory *history = THUNAR_HISTORY (object);

  /* disconnect from the current directory */
  thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (history), NULL);

  /* disconnect from the action group */
  thunar_history_set_action_group (history, NULL);

  (*G_OBJECT_CLASS (thunar_history_parent_class)->dispose) (object);
}



static void
thunar_history_finalize (GObject *object)
{
  ThunarHistory *history = THUNAR_HISTORY (object);

  /* disconnect from the "forward" action */
  g_signal_handlers_disconnect_by_func (G_OBJECT (history->action_forward), thunar_history_action_forward, history);
  g_object_unref (G_OBJECT (history->action_forward));

  /* disconnect from the "back" action */
  g_signal_handlers_disconnect_by_func (G_OBJECT (history->action_back), thunar_history_action_back, history);
  g_object_unref (G_OBJECT (history->action_back));

  /* release the "forward" and "back" lists */
  thunar_file_list_free (history->forward_list);
  thunar_file_list_free (history->back_list);

  (*G_OBJECT_CLASS (thunar_history_parent_class)->finalize) (object);
}



static void
thunar_history_get_property (GObject    *object,
                             guint       prop_id,
                             GValue     *value,
                             GParamSpec *pspec)
{
  ThunarHistory *history = THUNAR_HISTORY (object);

  switch (prop_id)
    {
    case PROP_ACTION_GROUP:
      g_value_set_object (value, thunar_history_get_action_group (history));
      break;

    case PROP_CURRENT_DIRECTORY:
      g_value_set_object (value, thunar_navigator_get_current_directory (THUNAR_NAVIGATOR (history)));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static void
thunar_history_set_property (GObject      *object,
                             guint         prop_id,
                             const GValue *value,
                             GParamSpec   *pspec)
{
  ThunarHistory *history = THUNAR_HISTORY (object);

  switch (prop_id)
    {
    case PROP_ACTION_GROUP:
      thunar_history_set_action_group (history, g_value_get_object (value));
      break;

    case PROP_CURRENT_DIRECTORY:
      thunar_navigator_set_current_directory (THUNAR_NAVIGATOR (history), g_value_get_object (value));
      break;

    default:
      G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
      break;
    }
}



static ThunarFile*
thunar_history_get_current_directory (ThunarNavigator *navigator)
{
  ThunarHistory *history = THUNAR_HISTORY (navigator);
  return history->current_directory;
}



static void
thunar_history_set_current_directory (ThunarNavigator *navigator,
                                      ThunarFile      *current_directory)
{
  ThunarHistory *history = THUNAR_HISTORY (navigator);

  /* verify that we don't already use that directory */
  if (G_UNLIKELY (current_directory == history->current_directory))
    return;

  /* we try to be smart and check if the new current directory
   * is the first element on either "back" or "forward" and if
   * so, perform the appropriate operation.
   */
  if (history->back_list != NULL && history->back_list->data == current_directory)
    {
      thunar_history_action_back (history->action_back, history);
    }
  else if (history->forward_list != NULL && history->forward_list->data == current_directory)
    {
      thunar_history_action_forward (history->action_forward, history);
    }
  else
    {
      /* clear the "forward" list */
      gtk_action_set_sensitive (history->action_forward, FALSE);
      thunar_file_list_free (history->forward_list);
      history->forward_list = NULL;

      /* prepend the previous current directory to the "back" list */
      if (G_LIKELY (history->current_directory != NULL))
        {
          history->back_list = g_list_prepend (history->back_list, history->current_directory);
          gtk_action_set_sensitive (history->action_back, TRUE);
        }

      /* activate the new current directory */
      history->current_directory = current_directory;

      /* connect to the new current directory */
      if (G_LIKELY (current_directory != NULL))
        g_object_ref (G_OBJECT (current_directory));
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (history), "current-directory");
}



static void
thunar_history_action_back (GtkAction     *action,
                            ThunarHistory *history)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_HISTORY (history));

  /* make sure the "back" list isn't empty */
  if (G_UNLIKELY (history->back_list == NULL))
    return;

  /* prepend the previous current directory to the "forward" list */
  if (G_LIKELY (history->current_directory != NULL))
    {
      history->forward_list = g_list_prepend (history->forward_list, history->current_directory);
      gtk_action_set_sensitive (history->action_forward, TRUE);
    }

  /* remove the first directory from the "back" list
   * and make it the current directory.
   */
  history->current_directory = history->back_list->data;
  history->back_list = g_list_delete_link (history->back_list, history->back_list);
  gtk_action_set_sensitive (history->action_back, (history->back_list != NULL));

  /* tell the other modules to change the current directory */
  thunar_navigator_change_directory (THUNAR_NAVIGATOR (history), history->current_directory);
}



static void
thunar_history_action_forward (GtkAction     *action,
                               ThunarHistory *history)
{
  g_return_if_fail (GTK_IS_ACTION (action));
  g_return_if_fail (THUNAR_IS_HISTORY (history));

  /* make sure the "forward" list isn't empty */
  if (G_UNLIKELY (history->forward_list == NULL))
    return;

  /* prepend the previous current directory to the "back" list */
  if (G_LIKELY (history->current_directory != NULL))
    {
      history->back_list = g_list_prepend (history->back_list, history->current_directory);
      gtk_action_set_sensitive (history->action_back, TRUE);
    }

  /* remove the first directory from the "forward" list
   * and make it the current directory.
   */
  history->current_directory = history->forward_list->data;
  history->forward_list = g_list_delete_link (history->forward_list, history->forward_list);
  gtk_action_set_sensitive (history->action_forward, (history->forward_list != NULL));

  /* tell the other modules to change the current directory */
  thunar_navigator_change_directory (THUNAR_NAVIGATOR (history), history->current_directory);
}



/**
 * thunar_history_new:
 *
 * Allocates a new #ThunarHistory object.
 *
 * Return value: the newly allocated #ThunarHistory object.
 **/
ThunarHistory*
thunar_history_new (void)
{
  return g_object_new (THUNAR_TYPE_HISTORY, NULL);
}



/**
 * thunar_history_get_action_group:
 * @history : a #ThunarHistory.
 *
 * Returns the #GtkActionGroup to which @history
 * is currently attached, or %NULL if @history is
 * not attached to any #GtkActionGroup right now.
 *
 * Return value: the #GtkActionGroup to which
 *               @history is currently attached.
 **/
GtkActionGroup*
thunar_history_get_action_group (const ThunarHistory *history)
{
  g_return_val_if_fail (THUNAR_IS_HISTORY (history), NULL);
  return history->action_group;
}



/**
 * thunar_history_set_action_group:
 * @history      : a #ThunarHistory.
 * @action_group : a #GtkActionGroup or %NULL.
 *
 * Attaches @history to the specified @action_group,
 * and thereby registers the actions "back" and
 * "forward" provided by @history on the given
 * @action_group.
 **/
void
thunar_history_set_action_group (ThunarHistory  *history,
                                 GtkActionGroup *action_group)
{
  g_return_if_fail (THUNAR_IS_HISTORY (history));
  g_return_if_fail (action_group == NULL || GTK_IS_ACTION_GROUP (action_group));

  /* verify that we don't already use that action group */
  if (G_UNLIKELY (history->action_group == action_group))
    return;

  /* disconnect from the previous action group */
  if (G_UNLIKELY (history->action_group != NULL))
    {
      gtk_action_group_remove_action (history->action_group, history->action_back);
      gtk_action_group_remove_action (history->action_group, history->action_forward);
      g_object_unref (G_OBJECT (history->action_group));
    }

  /* activate the new action group */
  history->action_group = action_group;

  /* connect to the new action group */
  if (G_LIKELY (action_group != NULL))
    {
      g_object_ref (G_OBJECT (action_group));
      gtk_action_group_add_action_with_accel (action_group, history->action_back, "<alt>Left");
      gtk_action_group_add_action_with_accel (action_group, history->action_forward, "<alt>Right");
    }

  /* notify listeners */
  g_object_notify (G_OBJECT (history), "action-group");
}

