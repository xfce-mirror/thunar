/* $Id$ */
/*-
 * Copyright (c) 2005 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif

#ifdef HAVE_GRP_H
#include <grp.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#ifdef HAVE_PWD_H
#include <pwd.h>
#endif
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-user.h>
#include <thunar-vfs/thunar-vfs-alias.h>



/* the interval in which the user/group cache is flushed (in ms) */
#define THUNAR_VFS_USER_MANAGER_FLUSH_INTERVAL (10 * 60 * 1000)



GType
thunar_vfs_group_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsGroupClass),
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        sizeof (ThunarVfsGroup),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, "ThunarVfsGroup",
                                     &info, G_TYPE_FLAG_ABSTRACT);
    }

  return type;
}



/**
 * thunar_vfs_group_get_id:
 * @group : a #ThunarVfsGroup.
 *
 * Returns the unique id of the given @group.
 *
 * Return value: the unique id of @group.
 **/
ThunarVfsGroupId
thunar_vfs_group_get_id (ThunarVfsGroup *group)
{
  g_return_val_if_fail (THUNAR_VFS_IS_GROUP (group), 0);
  return THUNAR_VFS_GROUP_GET_CLASS (group)->get_id (group);
}



/**
 * thunar_vfs_group_get_name:
 * @group : a #ThunarVfsGroup.
 *
 * Returns the name of the @group. If the system is
 * unable to determine the name of @group, it'll
 * return the group id as string.
 *
 * Return value: the name of @group.
 **/
const gchar*
thunar_vfs_group_get_name (ThunarVfsGroup *group)
{
  g_return_val_if_fail (THUNAR_VFS_IS_GROUP (group), NULL);
  return THUNAR_VFS_GROUP_GET_CLASS (group)->get_name (group);
}




GType
thunar_vfs_user_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsUserClass),
        NULL,
        NULL,
        NULL,
        NULL,
        NULL,
        sizeof (ThunarVfsUser),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (G_TYPE_OBJECT, "ThunarVfsUser",
                                     &info, G_TYPE_FLAG_ABSTRACT);
    }

  return type;
}



/**
 * thunar_vfs_user_get_groups:
 * @user : a #ThunarVfsUser.
 *
 * Returns all #ThunarVfsGroup<!---->s that @user
 * belongs to. The returned list and the #ThunarVfsGroup<!---->s
 * contained within the list are owned by @user and must not be
 * freed or altered by the caller.
 *
 * Note that if @user has a primary group, this group will
 * also be contained in the returned list.
 *
 * Return value: the groups that @user belongs to.
 **/
GList*
thunar_vfs_user_get_groups (ThunarVfsUser *user)
{
  g_return_val_if_fail (THUNAR_VFS_IS_USER (user), NULL);
  return THUNAR_VFS_USER_GET_CLASS (user)->get_groups (user);
}



/**
 * thunar_vfs_user_get_primary_group:
 * @user : a #ThunarVfsUser.
 *
 * Returns the primary group of @user or %NULL if @user
 * has no primary group.
 *
 * No reference is taken for the caller, so you must
 * not call #g_object_unref() on the returned object.
 *
 * Return value: the primary group of @user or %NULL.
 **/
ThunarVfsGroup*
thunar_vfs_user_get_primary_group (ThunarVfsUser *user)
{
  g_return_val_if_fail (THUNAR_VFS_IS_USER (user), NULL);
  return THUNAR_VFS_USER_GET_CLASS (user)->get_primary_group (user);
}



/**
 * thunar_vfs_user_get_id:
 * @user : a #ThunarVfsUser.
 *
 * Returns the unique id of @user.
 *
 * Return value: the unique id of @user.
 **/
ThunarVfsUserId
thunar_vfs_user_get_id (ThunarVfsUser *user)
{
  g_return_val_if_fail (THUNAR_VFS_IS_USER (user), 0);
  return THUNAR_VFS_USER_GET_CLASS (user)->get_id (user);
}



/**
 * thunar_vfs_user_get_name:
 * @user : a #ThunarVfsUser.
 *
 * Returns the name of @user. If the system is
 * unable to determine the account name of @user,
 * it'll return the user id as string.
 *
 * Return value: the name of @user.
 **/
const gchar*
thunar_vfs_user_get_name (ThunarVfsUser *user)
{
  g_return_val_if_fail (THUNAR_VFS_IS_USER (user), 0);
  return THUNAR_VFS_USER_GET_CLASS (user)->get_name (user);
}



/**
 * thunar_vfs_user_get_real_name:
 * @user : a #ThunarVfsUser.
 *
 * Returns the real name of @user or %NULL if the
 * real name for @user is not known to the underlying
 * system.
 *
 * Return value: the real name for @user or %NULL.
 **/
const gchar*
thunar_vfs_user_get_real_name (ThunarVfsUser *user)
{
  g_return_val_if_fail (THUNAR_VFS_IS_USER (user), 0);
  return THUNAR_VFS_USER_GET_CLASS (user)->get_real_name (user);
}



/**
 * thunar_vfs_user_is_me:
 * @user : a #ThunarVfsUser.
 *
 * Checks whether the owner of the current process is
 * described by @user.
 *
 * Return value: %TRUE if @user is the owner of the current
 *               process, else %FALSE.
 **/
gboolean
thunar_vfs_user_is_me (ThunarVfsUser *user)
{
  g_return_val_if_fail (THUNAR_VFS_IS_USER (user), FALSE);
  return THUNAR_VFS_USER_GET_CLASS (user)->is_me (user);
}




typedef struct _ThunarVfsLocalGroupClass ThunarVfsLocalGroupClass;
typedef struct _ThunarVfsLocalGroup      ThunarVfsLocalGroup;



#define THUNAR_VFS_TYPE_LOCAL_GROUP             (thunar_vfs_local_group_get_type ())
#define THUNAR_VFS_LOCAL_GROUP(obj)             (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_LOCAL_GROUP, ThunarVfsLocalGroup))
#define THUNAR_VFS_LOCAL_GROUP_CLASS(klass)     (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_LOCAL_GROUP, ThunarVfsLocalGroupClass))
#define THUNAR_VFS_IS_LOCAL_GROUP(obj)          (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_LOCAL_GROUP))
#define THUNAR_VFS_IS_LOCAL_GROUP_CLASS(klass)  (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_LOCAL_GROUP))
#define THUNAR_VFS_LOCAL_GROUP_GET_CLASS(obj)   (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_LOCAL_GROUP, ThunarVfsLocalGroupClass))



static GType            thunar_vfs_local_group_get_type   (void) G_GNUC_CONST;
static void             thunar_vfs_local_group_class_init (ThunarVfsLocalGroupClass *klass);
static void             thunar_vfs_local_group_finalize   (GObject                  *object);
static ThunarVfsGroupId thunar_vfs_local_group_get_id     (ThunarVfsGroup           *group);
static const gchar     *thunar_vfs_local_group_get_name   (ThunarVfsGroup           *group);
static ThunarVfsGroup  *thunar_vfs_local_group_new        (ThunarVfsGroupId          id);



struct _ThunarVfsLocalGroupClass
{
  ThunarVfsGroupClass __parent__;
};

struct _ThunarVfsLocalGroup
{
  ThunarVfsGroup __parent__;

  ThunarVfsGroupId id;
  gchar           *name;
};



static GObjectClass *thunar_vfs_local_group_parent_class;



static GType
thunar_vfs_local_group_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsLocalGroupClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_local_group_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsLocalGroup),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_GROUP,
                                     "ThunarVfsLocalGroup",
                                     &info, 0);
    }

  return type;
}



static void
thunar_vfs_local_group_class_init (ThunarVfsLocalGroupClass *klass)
{
  ThunarVfsGroupClass *thunarvfs_group_class;
  GObjectClass        *gobject_class;

  thunar_vfs_local_group_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_local_group_finalize;

  thunarvfs_group_class = THUNAR_VFS_GROUP_CLASS (klass);
  thunarvfs_group_class->get_id = thunar_vfs_local_group_get_id;
  thunarvfs_group_class->get_name = thunar_vfs_local_group_get_name;
}



static void
thunar_vfs_local_group_finalize (GObject *object)
{
  ThunarVfsLocalGroup *local_group = THUNAR_VFS_LOCAL_GROUP (object);

  /* free the group name */
  g_free (local_group->name);

  G_OBJECT_CLASS (thunar_vfs_local_group_parent_class)->finalize (object);
}



static ThunarVfsGroupId
thunar_vfs_local_group_get_id (ThunarVfsGroup *group)
{
  return THUNAR_VFS_LOCAL_GROUP (group)->id;
}



static const gchar*
thunar_vfs_local_group_get_name (ThunarVfsGroup *group)
{
  ThunarVfsLocalGroup *local_group = THUNAR_VFS_LOCAL_GROUP (group);
  struct group        *grp;

  /* determine the name on-demand */
  if (G_UNLIKELY (local_group->name == NULL))
    {
      grp = getgrgid (local_group->id);
      if (G_LIKELY (grp != NULL))
        local_group->name = g_strdup (grp->gr_name);
      else
        local_group->name = g_strdup_printf ("%u", (guint) local_group->id);
    }

  return local_group->name;
}



static ThunarVfsGroup*
thunar_vfs_local_group_new (ThunarVfsGroupId id)
{
  ThunarVfsLocalGroup *local_group;

  local_group = g_object_new (THUNAR_VFS_TYPE_LOCAL_GROUP, NULL);
  local_group->id = id;

  return THUNAR_VFS_GROUP (local_group);
}




typedef struct _ThunarVfsLocalUserClass ThunarVfsLocalUserClass;
typedef struct _ThunarVfsLocalUser      ThunarVfsLocalUser;



#define THUNAR_VFS_TYPE_LOCAL_USER            (thunar_vfs_local_user_get_type ())
#define THUNAR_VFS_LOCAL_USER(obj)            (G_TYPE_CHECK_INSTANCE_CAST ((obj), THUNAR_VFS_TYPE_LOCAL_USER, ThunarVfsLocalUser))
#define THUNAR_VFS_LOCAL_USER_CLASS(klass)    (G_TYPE_CHECK_CLASS_CAST ((klass), THUNAR_VFS_TYPE_LOCAL_USER, ThunarVfsLocalUserClass))
#define THUNAR_VFS_IS_LOCAL_USER(obj)         (G_TYPE_CHECK_INSTANCE_TYPE ((obj), THUNAR_VFS_TYPE_LOCAL_USER))
#define THUNAR_VFS_IS_LOCAL_USER_CLASS(klass) (G_TYPE_CHECK_CLASS_TYPE ((klass), THUNAR_VFS_TYPE_LOCAL_USER))
#define THUNAR_VFS_LOCAL_USER_GET_CLASS(obj)  (G_TYPE_INSTANCE_GET_CLASS ((obj), THUNAR_VFS_TYPE_LOCAL_USER, ThunarVfsLocalUserClass))



static GType           thunar_vfs_local_user_get_type          (void) G_GNUC_CONST;
static void            thunar_vfs_local_user_class_init        (ThunarVfsLocalUserClass *klass);
static void            thunar_vfs_local_user_finalize          (GObject                 *object);
static GList          *thunar_vfs_local_user_get_groups        (ThunarVfsUser           *user);
static ThunarVfsGroup *thunar_vfs_local_user_get_primary_group (ThunarVfsUser           *user);
static ThunarVfsUserId thunar_vfs_local_user_get_id            (ThunarVfsUser           *user);
static const gchar    *thunar_vfs_local_user_get_name          (ThunarVfsUser           *user);
static const gchar    *thunar_vfs_local_user_get_real_name     (ThunarVfsUser           *user);
static gboolean        thunar_vfs_local_user_is_me             (ThunarVfsUser           *user);
static void            thunar_vfs_local_user_load              (ThunarVfsLocalUser      *local_user);
static ThunarVfsUser  *thunar_vfs_local_user_new               (ThunarVfsUserId          id);



struct _ThunarVfsLocalUserClass
{
  ThunarVfsUserClass __parent__;

  ThunarVfsUserId effective_uid;
};

struct _ThunarVfsLocalUser
{
  ThunarVfsUser __parent__;

  GList          *groups;
  ThunarVfsGroup *primary_group;
  ThunarVfsUserId id;
  gchar          *name;
  gchar          *real_name;
};



static GObjectClass *thunar_vfs_local_user_parent_class;



static GType
thunar_vfs_local_user_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsLocalUserClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_local_user_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsLocalUser),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_USER,
                                     "ThunarVfsLocalUser",
                                     &info, 0);
    }

  return type;
}



static void
thunar_vfs_local_user_class_init (ThunarVfsLocalUserClass *klass)
{
  ThunarVfsUserClass *thunarvfs_user_class;
  GObjectClass       *gobject_class;

  thunar_vfs_local_user_parent_class = g_type_class_peek_parent (klass);

  /* determine the current process' effective user id, we do
   * this only once to avoid the syscall overhead on every
   * is_me() invokation.
   */
  klass->effective_uid = geteuid ();

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_local_user_finalize;

  thunarvfs_user_class = THUNAR_VFS_USER_CLASS (klass);
  thunarvfs_user_class->get_groups = thunar_vfs_local_user_get_groups;
  thunarvfs_user_class->get_primary_group = thunar_vfs_local_user_get_primary_group;
  thunarvfs_user_class->get_id = thunar_vfs_local_user_get_id;
  thunarvfs_user_class->get_name = thunar_vfs_local_user_get_name;
  thunarvfs_user_class->get_real_name = thunar_vfs_local_user_get_real_name;
  thunarvfs_user_class->is_me = thunar_vfs_local_user_is_me;
}



static void
thunar_vfs_local_user_finalize (GObject *object)
{
  ThunarVfsLocalUser *local_user = THUNAR_VFS_LOCAL_USER (object);
  GList              *lp;

  /* unref the associated groups */
  for (lp = local_user->groups; lp != NULL; lp = lp->next)
    g_object_unref (G_OBJECT (lp->data));
  g_list_free (local_user->groups);

  /* drop the ref on the primary group */
  if (G_LIKELY (local_user->primary_group != NULL))
    g_object_unref (G_OBJECT (local_user->primary_group));

  /* free the names */
  g_free (local_user->real_name);
  g_free (local_user->name);

  G_OBJECT_CLASS (thunar_vfs_local_user_parent_class)->finalize (object);
}



static GList*
thunar_vfs_local_user_get_groups (ThunarVfsUser *user)
{
  ThunarVfsUserManager *manager;
  ThunarVfsLocalUser   *local_user = THUNAR_VFS_LOCAL_USER (user);
  ThunarVfsGroup       *primary_group;
  ThunarVfsGroup       *group;
  gid_t                 gidset[NGROUPS_MAX];
  gint                  gidsetlen;
  gint                  n;

  /* load the groups on-demand */
  if (G_UNLIKELY (local_user->groups == NULL))
    {
      primary_group = thunar_vfs_local_user_get_primary_group (user);

      /* we can only determine the groups list for the current
       * process owner in a portable fashion, and in fact, we
       * only need the list for the current user.
       */
      if (thunar_vfs_local_user_is_me (user))
        {
          manager = thunar_vfs_user_manager_get_default ();

          /* add all supplementary groups */
          gidsetlen = getgroups (G_N_ELEMENTS (gidset), gidset);
          for (n = 0; n < gidsetlen; ++n)
            if (primary_group == NULL || thunar_vfs_group_get_id (primary_group) != gidset[n])
              {
                group = thunar_vfs_user_manager_get_group_by_id (manager, gidset[n]);
                if (G_LIKELY (group != NULL))
                  local_user->groups = g_list_append (local_user->groups, group);
              }

          g_object_unref (G_OBJECT (manager));
        }

      /* prepend the primary group (if any) */
      if (G_LIKELY (primary_group != NULL))
        {
          local_user->groups = g_list_prepend (local_user->groups, primary_group);
          g_object_ref (G_OBJECT (primary_group));
        }
    }

  return local_user->groups;
}



static ThunarVfsGroup*
thunar_vfs_local_user_get_primary_group (ThunarVfsUser *user)
{
  ThunarVfsLocalUser *local_user = THUNAR_VFS_LOCAL_USER (user);

  if (G_UNLIKELY (local_user->name == NULL))
    thunar_vfs_local_user_load (local_user);

  return local_user->primary_group;
}



static ThunarVfsUserId
thunar_vfs_local_user_get_id (ThunarVfsUser *user)
{
  return THUNAR_VFS_LOCAL_USER (user)->id;
}



static const gchar*
thunar_vfs_local_user_get_name (ThunarVfsUser *user)
{
  ThunarVfsLocalUser *local_user = THUNAR_VFS_LOCAL_USER (user);

  if (G_UNLIKELY (local_user->name == NULL))
    thunar_vfs_local_user_load (local_user);

  return local_user->name;
}



static const gchar*
thunar_vfs_local_user_get_real_name (ThunarVfsUser *user)
{
  ThunarVfsLocalUser *local_user = THUNAR_VFS_LOCAL_USER (user);

  if (G_UNLIKELY (local_user->name == NULL))
    thunar_vfs_local_user_load (local_user);

  return local_user->real_name;
}



static gboolean
thunar_vfs_local_user_is_me (ThunarVfsUser *user)
{
  return (THUNAR_VFS_LOCAL_USER (user)->id == THUNAR_VFS_LOCAL_USER_GET_CLASS (user)->effective_uid);
}



static void
thunar_vfs_local_user_load (ThunarVfsLocalUser *local_user)
{
  ThunarVfsUserManager *manager;
  struct passwd        *pw;
  const gchar          *s;

  g_return_if_fail (local_user->name == NULL);

  pw = getpwuid (local_user->id);
  if (G_LIKELY (pw != NULL))
    {
      manager = thunar_vfs_user_manager_get_default ();

      /* query name and primary group */
      local_user->name = g_strdup (pw->pw_name);
      local_user->primary_group = thunar_vfs_user_manager_get_group_by_id (manager, pw->pw_gid);

      /* try to figure out the real name */
      s = strchr (pw->pw_gecos, ',');
      if (s != NULL)
        local_user->real_name = g_strndup (pw->pw_gecos, s - pw->pw_gecos);
      else if (pw->pw_gecos[0] != '\0')
        local_user->real_name = g_strdup (pw->pw_gecos);

      g_object_unref (G_OBJECT (manager));
    }
  else
    {
      local_user->name = g_strdup_printf ("%u", (guint) local_user->id);
    }
}



static ThunarVfsUser*
thunar_vfs_local_user_new (ThunarVfsUserId id)
{
  ThunarVfsLocalUser *local_user;

  local_user = g_object_new (THUNAR_VFS_TYPE_LOCAL_USER, NULL);
  local_user->id = id;

  return THUNAR_VFS_USER (local_user);
}




static void     thunar_vfs_user_manager_class_init          (ThunarVfsUserManagerClass *klass);
static void     thunar_vfs_user_manager_init                (ThunarVfsUserManager      *manager);
static void     thunar_vfs_user_manager_finalize            (GObject                   *object);
static gboolean thunar_vfs_user_manager_flush_timer         (gpointer                   user_data);
static void     thunar_vfs_user_manager_flush_timer_destroy (gpointer                   user_data);



struct _ThunarVfsUserManagerClass
{
  GObjectClass __parent__;
};

struct _ThunarVfsUserManager
{
  GObject __parent__;

  GHashTable *groups;
  GHashTable *users;

  gint        flush_timer_id;
};



G_DEFINE_TYPE (ThunarVfsUserManager, thunar_vfs_user_manager, G_TYPE_OBJECT);



static void
thunar_vfs_user_manager_class_init (ThunarVfsUserManagerClass *klass)
{
  GObjectClass *gobject_class;

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_user_manager_finalize;
}



static void
thunar_vfs_user_manager_init (ThunarVfsUserManager *manager)
{
  manager->groups = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);
  manager->users = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL, g_object_unref);

  /* keep the groups file in memory if possible */
#ifdef HAVE_SETGROUPENT
  setgroupent (TRUE);
#endif

  /* keep the passwd file in memory if possible */
#ifdef HAVE_SETPASSENT
  setpassent (TRUE);
#endif

  /* start the flush timer */
  manager->flush_timer_id = g_timeout_add_full (G_PRIORITY_LOW, THUNAR_VFS_USER_MANAGER_FLUSH_INTERVAL,
                                                thunar_vfs_user_manager_flush_timer, manager,
                                                thunar_vfs_user_manager_flush_timer_destroy);
}



static void
thunar_vfs_user_manager_finalize (GObject *object)
{
  ThunarVfsUserManager *manager = THUNAR_VFS_USER_MANAGER (object);

  /* stop the flush timer */
  if (G_LIKELY (manager->flush_timer_id >= 0))
    g_source_remove (manager->flush_timer_id);

  /* destroy the hash tables */
  g_hash_table_destroy (manager->groups);
  g_hash_table_destroy (manager->users);

  /* unload the groups file */
  endgrent ();

  /* unload the passwd file */
  endpwent ();

  G_OBJECT_CLASS (thunar_vfs_user_manager_parent_class)->finalize (object);
}



static gboolean
thunar_vfs_user_manager_flush_timer (gpointer user_data)
{
  ThunarVfsUserManager *manager = THUNAR_VFS_USER_MANAGER (user_data);
  guint                 size = 0;

  GDK_THREADS_ENTER ();

  /* drop all cached groups */
  size += g_hash_table_foreach_remove (manager->groups, (GHRFunc) gtk_true, NULL);

  /* drop all cached users */
  size += g_hash_table_foreach_remove (manager->users, (GHRFunc) gtk_true, NULL);

  /* reload groups and passwd files if we had cached entities */
  if (G_LIKELY (size > 0))
    {
      endgrent ();
      endpwent ();

#ifdef HAVE_SETGROUPENT
      setgroupent (TRUE);
#endif

#ifdef HAVE_SETPASSENT
      setpassent (TRUE);
#endif
    }

  GDK_THREADS_LEAVE ();

  return TRUE;
}



static void
thunar_vfs_user_manager_flush_timer_destroy (gpointer user_data)
{
  GDK_THREADS_ENTER ();
  THUNAR_VFS_USER_MANAGER (user_data)->flush_timer_id = -1;
  GDK_THREADS_LEAVE ();
}



/**
 * thunar_vfs_user_manager_get_default:
 *
 * Returns the default #ThunarVfsUserManager instance, which is shared
 * by all modules using the user module. Call #g_object_unref() on the
 * returned object when you are done with it.
 *
 * Return value: the default #ThunarVfsUserManager instance.
 **/
ThunarVfsUserManager*
thunar_vfs_user_manager_get_default (void)
{
  static ThunarVfsUserManager *manager = NULL;

  if (G_UNLIKELY (manager == NULL))
    {
      manager = g_object_new (THUNAR_VFS_TYPE_USER_MANAGER, NULL);
      g_object_add_weak_pointer (G_OBJECT (manager), (gpointer) &manager);
    }
  else
    {
      g_object_ref (G_OBJECT (manager));
    }

  return manager;
}



/**
 * thunar_vfs_user_manager_get_group_by_id:
 * @manager : a #ThunarVfsUserManager.
 * @id      : the group id.
 *
 * Looks up the #ThunarVfsGroup corresponding to @id in @manager. Returns
 * %NULL if @manager is unable to determine the #ThunarVfsGroup for @id,
 * else a pointer to the corresponding #ThunarVfsGroup. The caller is
 * responsible for freeing the returned object using #g_object_unref().
 *
 * Return value: the #ThunarVfsGroup corresponding to @id or %NULL.
 **/
ThunarVfsGroup*
thunar_vfs_user_manager_get_group_by_id (ThunarVfsUserManager *manager,
                                         ThunarVfsGroupId      id)
{
  ThunarVfsGroup *group;

  g_return_val_if_fail (THUNAR_VFS_IS_USER_MANAGER (manager), NULL);

  /* lookup/load the group corresponding to id */
  group = g_hash_table_lookup (manager->groups, GINT_TO_POINTER (id));
  if (group == NULL)
    {
      group = thunar_vfs_local_group_new (id);
      g_hash_table_insert (manager->groups, GINT_TO_POINTER (id), group);
    }

  /* take a reference for the caller */
  g_object_ref (G_OBJECT (group));

  return group;
}



/**
 * thunar_vfs_user_manager_get_user_by_id:
 * @manager : a #ThunarVfsUserManager.
 * @id      : the user id.
 *
 * Looks up the #ThunarVfsUser corresponding to @id in @manager. Returns
 * %NULL if @manager is unable to determine the #ThunarVfsUser for @id,
 * else a pointer to the corresponding #ThunarVfsUser. The caller is
 * responsible for freeing the returned object using #g_object_unref().
 *
 * Return value: the #ThunarVfsUser corresponding to @id or %NULL.
 **/
ThunarVfsUser*
thunar_vfs_user_manager_get_user_by_id (ThunarVfsUserManager *manager,
                                        ThunarVfsUserId       id)
{
  ThunarVfsUser *user;

  g_return_val_if_fail (THUNAR_VFS_IS_USER_MANAGER (manager), NULL);

  /* lookup/load the user corresponding to id */
  user = g_hash_table_lookup (manager->users, GINT_TO_POINTER (id));
  if (user == NULL)
    {
      user = thunar_vfs_local_user_new (id);
      g_hash_table_insert (manager->users, GINT_TO_POINTER (id), user);
    }

  /* take a reference for the caller */
  g_object_ref (G_OBJECT (user));

  return user;
}



#define __THUNAR_VFS_USER_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
