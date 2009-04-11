/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
 * Copyright (c) 2009 Jannis Pohlmann <jannis@xfce.org>
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

#include <thunar/thunar-file.h>
#include <thunar/thunar-stock.h>
#include <thunar/thunar-private.h>
#include <thunar/thunar-trash-action.h>



static void thunar_trash_action_class_init (ThunarTrashActionClass *klass);
static void thunar_trash_action_init       (ThunarTrashAction      *trash_action);
static void thunar_trash_action_finalize   (GObject                *object);
static void thunar_trash_action_changed    (ThunarTrashAction      *trash_action,
                                            ThunarFile             *trash_bin);


struct _ThunarTrashActionClass
{
  GtkActionClass __parent__;
};

struct _ThunarTrashAction
{
  GtkAction   __parent__;
  ThunarFile *trash_bin;
};



static GObjectClass *thunar_trash_action_parent_class;



GType
thunar_trash_action_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarTrashActionClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_trash_action_class_init,
        NULL,
        NULL,
        sizeof (ThunarTrashAction),
        0,
        (GInstanceInitFunc) thunar_trash_action_init,
        NULL,
      };

      type = g_type_register_static (GTK_TYPE_ACTION, I_("ThunarTrashAction"), &info, 0);
    }

  return type;
}



static void
thunar_trash_action_class_init (ThunarTrashActionClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_trash_action_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_trash_action_finalize;
}



static void
thunar_trash_action_init (ThunarTrashAction *trash_action)
{
  GFile *trash_bin;

  /* try to connect to the trash bin */
  trash_bin = g_file_new_for_trash ();
  trash_action->trash_bin = thunar_file_get (trash_bin, NULL);
  g_object_unref (trash_bin);

  /* safety check for trash bin... */
  if (G_LIKELY (trash_action->trash_bin != NULL))
    {
      /* watch the trash bin for changes */
      thunar_file_watch (trash_action->trash_bin);

      /* stay informed about changes to the trash bin */
      g_signal_connect_swapped (G_OBJECT (trash_action->trash_bin), "changed",
                                G_CALLBACK (thunar_trash_action_changed),
                                trash_action);

      /* initially update the stock icon */
      thunar_trash_action_changed (trash_action, trash_action->trash_bin);
    }
}



static void
thunar_trash_action_finalize (GObject *object)
{
  ThunarTrashAction *trash_action = THUNAR_TRASH_ACTION (object);

  /* check if we are connected to the trash bin */
  if (G_LIKELY (trash_action->trash_bin != NULL))
    {
      /* unwatch the trash bin */
      thunar_file_unwatch (trash_action->trash_bin);

      /* release the trash bin */
      g_signal_handlers_disconnect_by_func (G_OBJECT (trash_action->trash_bin), thunar_trash_action_changed, trash_action);
      g_object_unref (G_OBJECT (trash_action->trash_bin));
    }

  (*G_OBJECT_CLASS (thunar_trash_action_parent_class)->finalize) (object);
}



static void
thunar_trash_action_changed (ThunarTrashAction *trash_action,
                             ThunarFile        *trash_bin)
{
  _thunar_return_if_fail (THUNAR_IS_TRASH_ACTION (trash_action));
  _thunar_return_if_fail (trash_action->trash_bin == trash_bin);
  _thunar_return_if_fail (THUNAR_IS_FILE (trash_bin));

  /* adjust the stock icon appropriately */
  g_object_set (G_OBJECT (trash_action), "stock-id", (thunar_file_get_size (trash_bin) > 0) ? THUNAR_STOCK_TRASH_FULL : THUNAR_STOCK_TRASH_EMPTY, NULL);
}



/**
 * thunar_trash_action_new:
 *
 * Allocates a new #ThunarTrashAction, whose associated widgets update their icons according to the
 * current trash state.
 *
 * Return value: the newly allocated #ThunarTrashAction.
 **/
GtkAction*
thunar_trash_action_new (void)
{
  return g_object_new (THUNAR_TYPE_TRASH_ACTION,
                       "name", "open-trash",
                       "label", _("T_rash"),
                       "tooltip", _("Display the contents of the trash can"),
                       "stock-id", THUNAR_STOCK_TRASH_FULL,
                       NULL);
}

