/* $Id$ */
/*-
 * Copyright (c) 2006-2007 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_SYS_PARAM_H
#include <sys/param.h>
#endif
#ifdef HAVE_SYS_MOUNT_H
#include <sys/mount.h>
#endif
#ifdef HAVE_SYS_TYPES_H
#include <sys/types.h>
#endif
#ifdef HAVE_SYS_STAT_H
#include <sys/stat.h>
#endif

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_FCNTL_H
#include <fcntl.h>
#endif
#ifdef HAVE_MEMORY_H
#include <memory.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif
#ifdef HAVE_STRING_H
#include <string.h>
#endif
#ifdef HAVE_SYSLOG_H
#include <syslog.h>
#endif
#ifdef HAVE_TIME_H
#include <time.h>
#endif
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <thunar-vfs/thunar-vfs-io-local.h>
#include <thunar-vfs/thunar-vfs-io-trash.h>
#include <thunar-vfs/thunar-vfs-mime-database-private.h>
#include <thunar-vfs/thunar-vfs-monitor-private.h>
#include <thunar-vfs/thunar-vfs-os.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>

/* Use g_mkdir(), g_open(), g_remove(), g_lstat(), and g_unlink() on win32 */
#if defined(G_OS_WIN32)
#include <glib/gstdio.h>
#else
#define g_mkdir(path, mode) (mkdir ((path), (mode)))
#define g_open(path, flags, mode) (open ((path), (flags), (mode)))
#define g_remove(path) (remove ((path)))
#define g_lstat(path, statb) (lstat ((path), (statb)))
#define g_unlink(path) (unlink ((path)))
#endif



static inline gboolean tvit_initialize_trash_dir      (const gchar *trash_dir) G_GNUC_WARN_UNUSED_RESULT;
static void            tvit_rescan_mount_points       (void);
static inline gchar   *tvit_resolve_original_path     (guint        trash_id,
                                                       const gchar *original_path_escaped,
                                                       const gchar *relative_path,
                                                       GError     **error) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;
static guint           tvit_resolve_trash_dir_to_id   (const gchar *trash_dir) G_GNUC_WARN_UNUSED_RESULT;
static gchar          *tvit_trash_dir_for_mount_point (const gchar *top_dir,
                                                       gboolean     create) G_GNUC_MALLOC G_GNUC_WARN_UNUSED_RESULT;



/* representation of a trash directory */
typedef struct
{
  gchar *top_dir;   /* toplevel directory for the trash (mount point or home dir) */
  gchar *trash_dir; /* absolute path to the trash directory with "files" and "info" */
  time_t mtime;     /* the last modification time of the "files" folder */
  guint  empty : 1; /* %TRUE if the trash is empty */
} ThunarVfsIOTrash;

/* list of all known ThunarVfsIOTrashes */
static ThunarVfsIOTrash *_thunar_vfs_io_trashes;
static guint             _thunar_vfs_io_n_trashes;

/* the trash rescan timer id */
static guint            _thunar_vfs_io_trash_timer_id;

/* the device of the home folder */
static dev_t            _thunar_vfs_io_trash_homedev;

/* the trash subsystem lock */
G_LOCK_DEFINE_STATIC (_thunar_vfs_io_trash);



static inline gboolean
tvit_initialize_trash_dir (const gchar *trash_dir)
{
  const gchar *name;
  struct stat  statb;
  gboolean     result = FALSE;
  gchar       *files_dir;
  gchar       *info_dir;
  gchar       *basename;
  gchar       *dirname;
  GDir        *dp;

  /* try to create the directory with the appropriate permissions */
  if (G_LIKELY (g_mkdir (trash_dir, 0700) == 0))
    {
      /* check if the trash dir is usable */
      if (G_LIKELY (g_lstat (trash_dir, &statb) == 0))
        {
          /* the trash_dir must be owned by user now, with rwx for user only */
          if (G_LIKELY (statb.st_uid == getuid () && (statb.st_mode & 0777) == 0700))
            {
              /* we want to be smart, and we need to be smarter
               * than file systems like msdosfs, which translate
               * for example ".Trash-200" to "trash-20", which
               * we don't want to support, so be sure the
               * directory now contains an entry with our name.
               */
              dirname = g_path_get_dirname (trash_dir);
              dp = g_dir_open (dirname, 0, NULL);
              if (G_LIKELY (dp != NULL))
                {
                  /* determine the basename to look for */
                  basename = g_path_get_basename (trash_dir);
                  do
                    {
                      /* compare the next entry */
                      name = g_dir_read_name (dp);
                      if (G_LIKELY (name != NULL))
                        result = (strcmp (name, basename) == 0);
                    }
                  while (name != NULL && !result);
                  g_free (basename);
                }
              g_free (dirname);

              /* check if the entry was present */
              if (G_LIKELY (result))
                {
                  /* create the infos and files directories */
                  info_dir = g_build_filename (trash_dir, "info", NULL);
                  files_dir = g_build_filename (trash_dir, "files", NULL);
                  result = (g_mkdir (info_dir, 0700) == 0 && g_mkdir (files_dir, 0700) == 0);
                  g_free (files_dir);
                  g_free (info_dir);
                }
            }

          if (G_UNLIKELY (!result))
            {
              /* Not good, e.g. USB key. Delete the trash directory again.
               * I'm paranoid, it would be better to find a solution that allows
               * to trash directly onto the USB key, but I don't see how that would
               * pass the security checks. It would also make the USB key appears as
               * empty when it's in fact full...
               *
               * Thanks to the KDE guys for this tip!
               */
              rmdir (trash_dir);
            }
        }
    }

  return result;
}



static void
tvit_rescan_mount_points (void)
{
  ExoMountPoint *mount_point;
  GSList        *mount_points;
  GSList        *lp;
  gchar         *trash_dir;
  guint          trash_id;

  /* determine the currently active mount points */
  mount_points = exo_mount_point_list_active (NULL);
  for (lp = mount_points; lp != NULL; lp = lp->next)
    {
      /* skip pseudo file systems as we won't find trash directories
       * there, and of course skip read-only file systems, as we
       * cannot trash to them anyway...
       */
      mount_point = (ExoMountPoint *) lp->data;
      if (strncmp (mount_point->device, "/dev/", 5) == 0
          && (mount_point->flags & EXO_MOUNT_POINT_READ_ONLY) == 0)
        {
          /* lookup the trash directory for this mount point */
          trash_dir = tvit_trash_dir_for_mount_point (mount_point->folder, FALSE);
          if (G_UNLIKELY (trash_dir != NULL))
            {
              /* check if the trash dir was already registered */
              trash_id = tvit_resolve_trash_dir_to_id (trash_dir);
              if (G_UNLIKELY (trash_id == 0))
                {
                  /* allocate space for the new trash directory */
                  trash_id = _thunar_vfs_io_n_trashes++;
                  _thunar_vfs_io_trashes = g_renew (ThunarVfsIOTrash,
                                                    _thunar_vfs_io_trashes,
                                                    _thunar_vfs_io_n_trashes);

                  /* register the new trash directory */
                  _thunar_vfs_io_trashes[trash_id].top_dir = g_strdup (mount_point->folder);
                  _thunar_vfs_io_trashes[trash_id].trash_dir = trash_dir;
                  _thunar_vfs_io_trashes[trash_id].mtime = (time_t) -1;
                }
              else
                {
                  /* cleanup */
                  g_free (trash_dir);
                }
            }
        }
      exo_mount_point_free (mount_point);
    }
  g_slist_free (mount_points);
}



static inline gchar*
tvit_resolve_original_path (guint        trash_id,
                            const gchar *original_path_escaped,
                            const gchar *relative_path,
                            GError     **error)
{
  gchar *original_path_unescaped;
  gchar *absolute_path = NULL;
  gchar *top_dir;

  /* unescape the path according to RFC 2396 */
  original_path_unescaped = _thunar_vfs_unescape_rfc2396_string (original_path_escaped, -1, "/", FALSE, error);
  if (G_UNLIKELY (original_path_unescaped == NULL))
    return NULL;

  /* check if we have a relative path here */
  if (!g_path_is_absolute (original_path_unescaped))
    {
      /* determine the toplevel directory for the trash-id */
      top_dir = _thunar_vfs_io_trash_get_top_dir (trash_id, error);
      if (G_LIKELY (top_dir != NULL))
        absolute_path = g_build_filename (top_dir, original_path_unescaped, relative_path, NULL);
      g_free (original_path_unescaped);
      g_free (top_dir);
    }
  else
    {
      /* generate the absolute with the relative part */
      absolute_path = g_build_filename (original_path_unescaped, relative_path, NULL);
      g_free (original_path_unescaped);
    }

  return absolute_path;
}



static guint
tvit_resolve_trash_dir_to_id (const gchar *trash_dir)
{
  guint id;

  for (id = 1; id < _thunar_vfs_io_n_trashes; ++id)
    if (strcmp (_thunar_vfs_io_trashes[id].trash_dir, trash_dir) == 0)
      return id;

  /* fallback to home trash! */
  return 0;
}



static gchar*
tvit_trash_dir_for_mount_point (const gchar *top_dir,
                                gboolean     create)
{
  struct stat statb;
  gchar      *trash_dir;
  gchar      *dir;
  uid_t       uid;

  /* determine our UID */
  uid = getuid ();

  /* try with $top_dir/.Trash directory first (created by the administrator) */
  dir = g_build_filename (top_dir, ".Trash", NULL);
  if (g_lstat (dir, &statb) == 0)
    {
      /* must be a directory owned by root with sticky bit and wx for 'others' */
      if (statb.st_uid == 0 && S_ISDIR (statb.st_mode) && (statb.st_mode & (S_ISVTX | S_IWOTH | S_IXOTH)) == (S_ISVTX | S_IWOTH | S_IXOTH))
        {
          /* check if $top_dir/.Trash/$uid exists */
          trash_dir = g_strdup_printf ("%s" G_DIR_SEPARATOR_S "%u", dir, (guint) uid);
          if (g_lstat (trash_dir, &statb) == 0)
            {
              /* must be a directory owned by user with rwx only for user */
              if (statb.st_uid == uid && S_ISDIR (statb.st_mode) && (statb.st_mode & 0777) == 0700)
                {
use_trash_dir:    /* release the root dir */
                  g_free (dir);

                  /* jap, that's our trash directory then */
                  return trash_dir;
                }
              else
                {
#ifdef HAVE_SYSLOG
                  /* the specification says the system administrator SHOULD be informed, and so we do */
                  syslog (LOG_PID | LOG_USER | LOG_WARNING, "Trash directory %s exists, but didn't pass the security checks, can't use it", trash_dir);
#endif
                }
            }
          else if (create && errno == ENOENT && tvit_initialize_trash_dir (trash_dir))
            {
              /* jap, successfully created */
              goto use_trash_dir;
            }
          g_free (trash_dir);
        }
      else
        {
#ifdef HAVE_SYSLOG
          /* the specification says the system administrator SHOULD be informed, and so we do */
          syslog (LOG_PID | LOG_USER | LOG_WARNING, "Root trash directory %s exists, but didn't pass the security checks, can't use it", dir);
#endif
        }
    }
  g_free (dir);

  /* try with $top_dir/.Trash-$uid instead */
  trash_dir = g_strdup_printf ("%s" G_DIR_SEPARATOR_S ".Trash-%u", top_dir, (guint) uid);
  if (g_lstat (trash_dir, &statb) == 0)
    {
      /* must be a directory owned by user with rwx only for user */
      if (statb.st_uid == uid && S_ISDIR (statb.st_mode) && (statb.st_mode & 0777) == 0700)
        {
          /* jap, found it */
          return trash_dir;
        }
      else
        {
#ifdef HAVE_SYSLOG
          /* the specification says the system administrator SHOULD be informed, and so we do */
          syslog (LOG_PID | LOG_USER | LOG_WARNING, "Trash directory %s exists, but didn't pass the security checks, can't use it", trash_dir);
#endif
        }
    }
  else if (create && errno == ENOENT && tvit_initialize_trash_dir (trash_dir))
    {
      /* jap, found it */
      return trash_dir;
    }
  g_free (trash_dir);

  /* no usable trash dir */
  return NULL;
}



/**
 * _thunar_vfs_io_trash_scan:
 *
 * Initially scans the trash directories if _thunar_vfs_io_trash_rescan()
 * was not called so far.
 **/
void
_thunar_vfs_io_trash_scan (void)
{
  /* scan the trash if not already done */
  if (G_UNLIKELY (_thunar_vfs_io_trash_timer_id == 0))
    _thunar_vfs_io_trash_rescan ();
}



/**
 * _thunar_vfs_io_trash_rescan:
 *
 * Rescans all trash directories and emits a changed event for
 * the trash root folder if any of the trash directories changed.
 *
 * Return value: always %TRUE due to implementation details.
 **/
gboolean
_thunar_vfs_io_trash_rescan (void)
{
  struct stat statb;
  time_t      mtime;
  gchar      *files_dir;
  guint       n;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_io_trashes != NULL, FALSE);

  /* acquire the trash subsystem lock */
  G_LOCK (_thunar_vfs_io_trash);

  /* check if we already scheduled the trash scan timer */
  if (G_UNLIKELY (_thunar_vfs_io_trash_timer_id == 0))
    {
      /* initially scan the active mount points */
      tvit_rescan_mount_points ();

      /* schedule a timer to regularly scan the trash directories */
      _thunar_vfs_io_trash_timer_id = g_timeout_add (5 * 1000, (GSourceFunc) _thunar_vfs_io_trash_rescan, NULL);
    }

  /* scan all known trash directories */
  for (n = 0; n < _thunar_vfs_io_n_trashes; ++n)
    {
      /* determine the absolute path to the "files" directory */
      files_dir = g_build_filename (_thunar_vfs_io_trashes[n].trash_dir, "files", NULL);

      /* check if the "files" directory was changed */
      mtime = (g_lstat (files_dir, &statb) == 0) ? statb.st_mtime : 0;
      if (_thunar_vfs_io_trashes[n].mtime != mtime)
        {
          /* remember the new mtime */
          _thunar_vfs_io_trashes[n].mtime = mtime;

          /* check if the directory is empty now */
          _thunar_vfs_io_trashes[n].empty = _thunar_vfs_os_is_dir_empty (files_dir);

          /* schedule a "changed" event for the trash root folder */
          thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_CHANGED, _thunar_vfs_path_trash_root);
        }

      /* cleanup */
      g_free (files_dir);
    }

  /* release the trash subsystem lock */
  G_UNLOCK (_thunar_vfs_io_trash);

  /* keep the timer alive */
  return TRUE;
}



/**
 * _thunar_vfs_io_trash_rescan_mounts:
 *
 * Rescans all currently mounted devices to see if a new trash directory
 * is available on one of the active mount points. This method is called
 * by the #ThunarVfsVolume<!---->s whenever a change is noticed.
 **/
void
_thunar_vfs_io_trash_rescan_mounts (void)
{
  G_LOCK (_thunar_vfs_io_trash);
  tvit_rescan_mount_points ();
  G_UNLOCK (_thunar_vfs_io_trash);
}



/**
 * _thunar_vfs_io_trash_get_top_dir:
 * @trash_id : the id of the trash, whose absolute toplevel directory to return.
 * @error    : return location for errors or %NULL.
 *
 * Returns the absolute path to the toplevel directory for the
 * trash with the specified @trash_id, or %NULL if the @trash_id
 * is invalid.
 *
 * The caller is responsible to free the returned string using
 * g_free() when no longer needed.
 *
 * Return value: the absolute path to the toplevel directory for
 *               the trash with the specified @trash_id, or %NULL
 *               in case of an error.
 **/
gchar*
_thunar_vfs_io_trash_get_top_dir (guint    trash_id,
                                  GError **error)
{
  gchar *top_dir;

  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* acquire the trash subsystem lock */
  G_LOCK (_thunar_vfs_io_trash);

  /* check if the trash-id is valid */
  if (G_LIKELY (trash_id < _thunar_vfs_io_n_trashes))
    {
      /* just return the absolute path to the trash's top dir */
      top_dir = g_strdup (_thunar_vfs_io_trashes[trash_id].top_dir);
    }
  else
    {
      /* the trash-id is invalid */
      _thunar_vfs_set_g_error_from_errno (error, ENOENT);
      top_dir = NULL;
    }

  /* release the trash subsystem lock */
  G_UNLOCK (_thunar_vfs_io_trash);

  return top_dir;
}



/**
 * _thunar_vfs_io_trash_get_trash_dir:
 * @trash_id : the id of the trash, whose absolute path to return.
 * @error    : return location for errors or %NULL.
 *
 * Returns the absolute path to the trash directory for the trash
 * with the specified @trash_id, or %NULL if the @trash_id is
 * invalid.
 *
 * The caller is responsible to free the returned string using
 * g_free() when no longer needed.
 *
 * Return value: the absolute path to the trash directory with
 *               the specified @trash_id, or %NULL on error.
 **/
gchar*
_thunar_vfs_io_trash_get_trash_dir (guint    trash_id,
                                    GError **error)
{
  gchar *trash_dir;

  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* acquire the trash subsystem lock */
  G_LOCK (_thunar_vfs_io_trash);

  /* check if the trash-id is valid */
  if (G_LIKELY (trash_id < _thunar_vfs_io_n_trashes))
    {
      /* just return the absolute path to the trash */
      trash_dir = g_strdup (_thunar_vfs_io_trashes[trash_id].trash_dir);
    }
  else
    {
      /* the trash-id is invalid */
      _thunar_vfs_set_g_error_from_errno (error, ENOENT);
      trash_dir = NULL;
    }

  /* release the trash subsystem lock */
  G_UNLOCK (_thunar_vfs_io_trash);

  return trash_dir;
}



/**
 * _thunar_vfs_io_trash_get_trash_info:
 * @path                 : a valid trash:-URI, which is not the trash root path.
 * @original_path_return : return location for the original path or %NULL.
 * @deletion_date_return : return location for the deletion date or %NULL.
 * @error                : return location for errors or %NULL.
 *
 * Reads the <filename>.trashinfo</filename> file for the @path and sets
 * @original_path_return and @deletion_date_return appropriately.
 *
 * Return value: %TRUE if the info was determined successfully, %FALSE on error.
 **/
gboolean
_thunar_vfs_io_trash_get_trash_info (const ThunarVfsPath *path,
                                     gchar              **original_path_return,
                                     gchar              **deletion_date_return,
                                     GError             **error)
{
  GKeyFile *key_file;
  GError   *err = NULL;
  gchar    *original_path;
  gchar    *relative_path;
  gchar    *info_file;
  gchar    *trash_dir;
  gchar    *file_id = NULL;
  guint     trash_id;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_trash (path), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* try to parse the trash path */
  if (!_thunar_vfs_io_trash_path_parse (path, &trash_id, &file_id, &relative_path, error))
    return FALSE;

  /* determine the .trashinfo file for the path */
  trash_dir = _thunar_vfs_io_trash_get_trash_dir (trash_id, &err);
  info_file = (trash_dir != NULL) ? g_strconcat (trash_dir, G_DIR_SEPARATOR_S "info" G_DIR_SEPARATOR_S, file_id, ".trashinfo", NULL) : NULL;
  g_free (trash_dir);

  /* check if we have a path */
  if (G_LIKELY (info_file != NULL))
    {
      /* parse the .trashinfo file */
      key_file = g_key_file_new ();
      if (g_key_file_load_from_file (key_file, info_file, 0, &err))
        {
          /* query the original path if requested */
          if (G_LIKELY (original_path_return != NULL))
            {
              /* read the original path from the .trashinfo file */
              original_path = g_key_file_get_string (key_file, "Trash Info", "Path", &err);
              if (G_LIKELY (original_path != NULL))
                {
                  /* determine the absolute path for the original path */
                  *original_path_return = tvit_resolve_original_path (trash_id, original_path, relative_path, &err);
                  g_free (original_path);
                }
            }

          /* query the deletion date if requested */
          if (err == NULL && deletion_date_return != NULL)
            *deletion_date_return = g_key_file_get_string (key_file, "Trash Info", "DeletionDate", &err);
        }
      g_key_file_free (key_file);
    }

  /* propagate a possible error */
  if (G_UNLIKELY (err != NULL))
    g_propagate_error (error, err);

  /* cleanup */
  g_free (relative_path);
  g_free (info_file);
  g_free (file_id);

  return (err == NULL);
}



/**
 * _thunar_vfs_io_trash_new_trash_info:
 * @original_path   : the original, local path of the file to delete.
 * @trash_id_return : return location for the trash id.
 * @file_id_return  : return location for the file id.
 * @error           : return location for errors or %NULL.
 *
 * Generates a new <filename>.trashinfo</filename> file for the file
 * at the @original_path and returns the trash-id of the target trash
 * in @trash_id_return and the file-id of the target trash file in
 * @file_id_return. The caller is responsible to free the string
 * returned in @file_id_return using g_free() when no longer needed.
 *
 * Return value: %TRUE if the <filename>.trashinfo</filename> was
 *               successfully generated, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_trash_new_trash_info (const ThunarVfsPath *original_path,
                                     guint               *trash_id_return,
                                     gchar              **file_id_return,
                                     GError             **error)
{
  const gchar *original_name = thunar_vfs_path_get_name (original_path);
  struct stat  statb;
  gchar        absolute_path[THUNAR_VFS_PATH_MAXSTRLEN];
  gchar        deletion_date[128];
  gchar       *mount_point = NULL;
  gchar       *trash_dir = NULL;
  gchar       *original_uri;
  gchar       *info_dir;
  gchar       *content;
  guint        trash_id = 0;
  guint        n;
  gint         fd;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_local (original_path), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (trash_id_return != NULL, FALSE);
  _thunar_vfs_return_val_if_fail (file_id_return != NULL, FALSE);

  /* determine the absolute path of the original file */
  if (thunar_vfs_path_to_string (original_path, absolute_path, sizeof (absolute_path), error) < 0)
    return FALSE;

  /* acquire the trash lock */
  G_LOCK (_thunar_vfs_io_trash);

  /* check if this is file should be trashed to the home trash (also done if stat fails) */
  if (g_lstat (absolute_path, &statb) == 0 && statb.st_dev != _thunar_vfs_io_trash_homedev)
    {
      /* determine the mount point for the original file which is
       * about to be deleted, and from the mount point we try to
       * resolve the ID of the responsible trash directory.
       */
#if defined(HAVE_STATFS) && defined(HAVE_STRUCT_STATFS_F_MNTONNAME)
      /* this is rather easy on BSDs (surprise)... */
      struct statfs statfsb;
      if (statfs (absolute_path, &statfsb) == 0)
        mount_point = g_strdup (statfsb.f_mntonname);
#else
      /* ...and really messy otherwise (surprise!) */
      GSList *mount_points, *lp;
      dev_t   dev = statb.st_dev;

      /* check if any of the mount points matches (really should) */
      mount_points = exo_mount_point_list_active (NULL);
      for (lp = mount_points; lp != NULL; lp = lp->next)
        {
          /* stat this mount point, and check if it's the device we're searching */
          if (stat (((ExoMountPoint *) lp->data)->folder, &statb) == 0 && (statb.st_dev == dev))
            {
              /* got it, remember the folder of the mount point */
              mount_point = g_strdup (((ExoMountPoint *) lp->data)->folder);
              break;
            }
        }
      g_slist_foreach (mount_points, (GFunc) exo_mount_point_free, NULL);
      g_slist_free (mount_points);
#endif

      /* check if we have a mount point */
      if (G_LIKELY (mount_point != NULL))
        {
          /* determine the trash directory for the mount point,
           * creating the directory if it does not already exists
           */
          trash_dir = tvit_trash_dir_for_mount_point (mount_point, TRUE);
          if (G_LIKELY (trash_dir != NULL))
            trash_id = tvit_resolve_trash_dir_to_id (trash_dir);
        }

      /* check if rescanning might help */
      if (trash_dir != NULL && trash_id == 0)
        {
          /* maybe we need to rescan the mount points */
          tvit_rescan_mount_points ();

          /* try to lookup the trash-id again */
          trash_id = tvit_resolve_trash_dir_to_id (trash_dir);
        }

      /* cleanup */
      g_free (mount_point);
      g_free (trash_dir);
    }

  /* validate the trash-id to ensure we won't crash */
  _thunar_vfs_assert (trash_id < _thunar_vfs_io_n_trashes);

  /* determine the info sub directory for this trash */
  info_dir = g_build_filename (_thunar_vfs_io_trashes[trash_id].trash_dir, "info", NULL);

  /* release the trash lock */
  G_UNLOCK (_thunar_vfs_io_trash);

  /* generate a unique file-id */
  g_snprintf (absolute_path, sizeof (absolute_path), "%s" G_DIR_SEPARATOR_S "%s.trashinfo", info_dir, original_name);
  for (n = 1;; ++n)
    {
      /* exclusively create the .trashinfo file */
      fd = g_open (absolute_path, O_CREAT | O_EXCL | O_WRONLY, 0600);
      if (G_LIKELY (fd >= 0))
        break;

      /* check if we need to create the info directory */
      if (G_UNLIKELY (errno == ENOENT))
        {
          /* try to create the info directory */
          if (!xfce_mkdirhier (info_dir, 0700, error))
            goto err1;

          /* try again with same .trashinfo name */
          continue;
        }

      /* check if the error cannot be recovered */
      if (G_UNLIKELY (errno != EEXIST))
        {
err0:     /* spit out a useful error message */
          content = g_filename_display_name (absolute_path);
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_IO, _("Failed to open \"%s\" for writing"), content);
          g_free (content);
err1:
          g_free (info_dir);
          return FALSE;
        }

      /* generate a new unique .trashinfo file name and try again */
      g_snprintf (absolute_path, sizeof (absolute_path), "%s" G_DIR_SEPARATOR_S "%s$%u.trashinfo", info_dir, original_name, n);
    }

  /* stat the file to get the deletion date from the filesystem (NFS, ...) */
  if (fstat (fd, &statb) < 0)
    {
err2: /* should not happen */
      g_unlink (absolute_path);
      close (fd);
      goto err0;
    }

  /* generate the .trashinfo content */
  original_uri = thunar_vfs_path_dup_uri (original_path);
  strftime (deletion_date, sizeof (deletion_date), "%FT%T", localtime (&statb.st_mtime));
  content = g_strdup_printf ("[Trash Info]\nPath=%s\nDeletionDate=%s\n",
                             original_uri + (sizeof ("file://") - 1),
                             deletion_date);
  g_free (original_uri);

  /* try to save the content (all at once) */
  if (write (fd, content, strlen (content)) != (ssize_t) strlen (content))
    {
      /* no space left, etc. */
      g_free (content);
      goto err2;
    }

  /* strip off the .trashinfo from the info_file */
  absolute_path[strlen (absolute_path) - (sizeof (".trashinfo") - 1)] = '\0';

  /* return the file-id and trash-id */
  *file_id_return = g_path_get_basename (absolute_path);
  *trash_id_return = trash_id;

  /* cleanup */
  g_free (content);
  g_free (info_dir);
  close (fd);

  return TRUE;
}



/**
 * _thunar_vfs_io_trash_path_new
 * @trash_id      : the id of the trash can.
 * @file_id       : the id of the file in the trash.
 * @relative_path : the relative path or the empty string.
 *
 * The caller is responsible to free the returned #ThunarVfsPath using
 * thunar_vfs_path_unref() when no longer needed.
 *
 * Returns a new #ThunarVfsPath that represents the trash resource in
 * the trash directory with the specified @trash_id, whose basename is
 * @file_id, at the given @relative_path.
 *
 * Return value: the #ThunarVfsPath for the specified trash resource.
 **/
ThunarVfsPath*
_thunar_vfs_io_trash_path_new (guint        trash_id,
                               const gchar *file_id,
                               const gchar *relative_path)
{
  ThunarVfsPath *parent;
  ThunarVfsPath *path;
  gchar         *name;

  _thunar_vfs_return_val_if_fail (strchr (file_id, '/') == NULL, NULL);
  _thunar_vfs_return_val_if_fail (relative_path != NULL, NULL);

  /* generate the path to the trash subfolder */
  name = g_strdup_printf ("%u-%s", trash_id, file_id);
  parent = _thunar_vfs_path_new_relative (_thunar_vfs_path_trash_root, name);
  g_free (name);

  /* generate the relative path */
  path = _thunar_vfs_path_new_relative (parent, relative_path);
  _thunar_vfs_path_unref_nofree (parent);

  return path;
}



/**
 * _thunar_vfs_io_trash_path_parse:
 * @path                  : a valid trash:-URI path, but not the "trash:///" root path.
 * @trash_id_return       : return location for the trash-id or %NULL.
 * @file_id_return        : return location for the file-id or %NULL.
 * @relative_path_return  : return location for the relative part or %NULL.
 * @error                 : return location for errors or %NULL.
 *
 * The relative component of @path may be %NULL if the @path refers to a toplevel
 * trash directory, i.e. "trash:///1-file". In this case %TRUE is returned, and
 * @relative_path_return will be set to %NULL.
 *
 * The caller is responsible to free the strings placed in @file_id_return and
 * @relative_path_return using g_free() when no longer needed.
 *
 * Return value: %TRUE if the @path was parsed successfully, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_trash_path_parse (const ThunarVfsPath *path,
                                 guint               *trash_id_return,
                                 gchar              **file_id_return,
                                 gchar              **relative_path_return,
                                 GError             **error)
{
  typedef struct _TrashPathItem
  {
    const ThunarVfsPath   *path;
    struct _TrashPathItem *next;
  } TrashPathItem;

  const ThunarVfsPath *parent;
  TrashPathItem       *item;
  TrashPathItem       *root = NULL;
  const gchar         *name;
  const gchar         *s;
  gchar               *t;
  gchar               *uri;
  guint                trash_id;
  guint                n;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_trash (path), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* cannot parse the trash root path */
  if (G_UNLIKELY (path->parent == NULL))
    {
invalid_uri:
      uri = thunar_vfs_path_dup_uri (path);
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_INVAL, _("The URI \"%s\" does not refer to a valid resource in the trash"), uri);
      g_free (uri);
      return FALSE;
    }

  /* determine the paths up to, but not including, the root path */
  for (parent = path; parent->parent != NULL; parent = parent->parent)
    {
      item = g_newa (TrashPathItem, 1);
      item->path = parent;
      item->next = root;
      root = item;
    }

  /* try to parse the first path component (<trashId>-<fileId>) */
  name = thunar_vfs_path_get_name (root->path);
  trash_id = strtoul (name, (gpointer) &s, 10);
  if (G_UNLIKELY (s == name || s[0] != '-' || s[1] == '\0'))
    goto invalid_uri;

  /* return the trash/file id */
  if (G_LIKELY (trash_id_return != NULL))
    *trash_id_return = trash_id;
  if (G_LIKELY (file_id_return != NULL))
    *file_id_return = g_strdup (s + 1);

  /* return the relative path if requested */
  if (G_LIKELY (relative_path_return != NULL))
    {
      /* check if we have a relative component */
      if (G_LIKELY (path != root->path))
        {
          /* we don't care for the root item anymore */
          root = root->next;

          /* allocate memory to store the relative part */
          for (item = root, n = 0; item != NULL; item = item->next)
            n += 1 + strlen (thunar_vfs_path_get_name (item->path));
          *relative_path_return = g_malloc (n);

          /* actually store the relative part */
          for (item = root, t = *relative_path_return; item != NULL; item = item->next)
            {
              if (item != root)
                *t++ = G_DIR_SEPARATOR;
              for (s = thunar_vfs_path_get_name (item->path); *s != '\0'; )
                *t++ = *s++;
            }
          *t = '\0';
        }
      else
        {
          /* no relative component */
          *relative_path_return = NULL;
        }
    }

  return TRUE;
}



/**
 * _thunar_vfs_io_trash_path_resolve:
 * @path  : a valid trash:-URI, but not the trash root path.
 * @error : return location for errors or %NULL.
 *
 * Resolves the trash @path to the absolute local path to the
 * specified resource. Returns %NULL if the @path is invalid.
 *
 * The caller is responsible to free the returned string using
 * g_free() when no longer needed.
 *
 * Return value: the absolute, local path to the trash resource
 *               at the given @path, or %NULL on error.
 **/
gchar*
_thunar_vfs_io_trash_path_resolve (const ThunarVfsPath *path,
                                   GError             **error)
{
  gchar *absolute_path = NULL;
  gchar *relative_path;
  gchar *trash_dir;
  gchar *file_id;
  guint  trash_id;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_trash (path), NULL);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* try to parse the trash path */
  if (_thunar_vfs_io_trash_path_parse (path, &trash_id, &file_id, &relative_path, error))
    {
      /* lookup the trash directory for the trash-id */
      trash_dir = _thunar_vfs_io_trash_get_trash_dir (trash_id, error);
      if (G_LIKELY (trash_dir != NULL))
        {
          /* generate the absolute path to the file in the trash */
          absolute_path = g_build_filename (trash_dir, "files", file_id, relative_path, NULL);
          g_free (trash_dir);
        }

      /* cleanup */
      g_free (relative_path);
      g_free (file_id);
    }

  return absolute_path;
}



/**
 * _thunar_vfs_io_trash_get_info:
 * @path  : a valid trash:-URI for which to determine the file info.
 * @error : return location for errors or %NULL.
 *
 * Determines the #ThunarVfsInfo for the trashed file at the specified
 * @path.
 *
 * The caller is responsible to free the returned #ThunarVfsInfo
 * using thunar_vfs_info_unref() when no longer needed.
 *
 * Return value: the #ThunarVfsInfo for the specified @path, or %NULL
 *               in case of an error.
 **/
ThunarVfsInfo*
_thunar_vfs_io_trash_get_info (ThunarVfsPath *path,
                               GError       **error)
{
  ThunarVfsFileTime mtime;
  ThunarVfsInfo    *info;
  gboolean          empty;
  gchar            *absolute_path;
  gchar            *original_path;
  guint             n;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_trash (path), NULL);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* check if path is the trash root directory */
  if (thunar_vfs_path_is_root (path))
    {
      /* innitially scan the trash directories */
      _thunar_vfs_io_trash_scan ();

      /* acquire the trash subsystem lock */
      G_LOCK (_thunar_vfs_io_trash);

      /* determine the mtime of the trash and check if its empty */
      for (empty = TRUE, mtime = 0, n = 0; n < _thunar_vfs_io_n_trashes; ++n)
        {
          /* atleast one trash dir not empty -> trash not empty */
          if (G_LIKELY (!_thunar_vfs_io_trashes[n].empty))
            empty = FALSE;

          /* most recent mtime -> mtime for the trash */
          if (G_LIKELY (_thunar_vfs_io_trashes[n].mtime > mtime))
            mtime = _thunar_vfs_io_trashes[n].mtime;
        }

      /* release the trash subsystem lock */
      G_UNLOCK (_thunar_vfs_io_trash);

      /* allocate info for the trash root folder */
      info = _thunar_vfs_slice_new0 (ThunarVfsInfo);
      info->type = THUNAR_VFS_FILE_TYPE_DIRECTORY;
      info->mode = THUNAR_VFS_FILE_MODE_USR_ALL;
      info->flags = THUNAR_VFS_FILE_FLAGS_READABLE | THUNAR_VFS_FILE_FLAGS_WRITABLE;
      info->uid = getuid ();
      info->gid = getgid ();
      info->size = empty ? 0 : 4096;
      info->atime = mtime;
      info->mtime = mtime;
      info->ctime = mtime;
      info->mime_info = thunar_vfs_mime_info_ref (_thunar_vfs_mime_inode_directory);
      info->path = thunar_vfs_path_ref (_thunar_vfs_path_trash_root);
      info->custom_icon = g_strdup (empty ? "gnome-fs-trash-empty" : "gnome-fs-trash-full");
      info->display_name = g_strdup (_("Trash"));
      info->ref_count = 1;
    }
  else
    {
      /* read the information from the disk using the absolute path */
      absolute_path = _thunar_vfs_io_trash_path_resolve (path, error);
      info = (absolute_path != NULL) ? _thunar_vfs_io_local_get_info (path, absolute_path, error) : NULL;
      g_free (absolute_path);

      /* check if we can adjust the name to the original one */
      if (info != NULL && thunar_vfs_path_is_root (path->parent) && info->display_name == thunar_vfs_path_get_name (path))
        {
          /* try to determine the original path */
          if (_thunar_vfs_io_trash_get_trash_info (path, &original_path, NULL, NULL))
            {
              /* use the original name as display name for the file */
              info->display_name = g_path_get_basename (original_path);
              g_free (original_path);
            }
        }
    }

  return info;
}



/**
 * _thunar_vfs_io_trash_get_metadata:
 * @path     : the #ThunarVfsPath of the file for which to return the @metadata.
 * @metadata : the #ThunarVfsInfoMetadata you are interested in.
 * @error    : return location for errors or %NULL.
 *
 * Similar to _thunar_vfs_io_local_get_metadata(), but works on trashed resources.
 *
 * The caller is responsible to free the returned string using g_free() when no
 * longer needed.
 *
 * Return value: the @metadata string or %NULL.
 **/
gchar*
_thunar_vfs_io_trash_get_metadata (ThunarVfsPath        *path,
                                   ThunarVfsInfoMetadata metadata,
                                   GError              **error)
{
  ThunarVfsPath *local_path;
  gchar         *result = NULL;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_trash (path), NULL);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);
  
  switch (metadata)
    {
    case THUNAR_VFS_INFO_METADATA_FILE_LINK_TARGET:
      local_path = _thunar_vfs_path_translate (path, THUNAR_VFS_PATH_SCHEME_FILE, error);
      if (G_LIKELY (local_path != NULL))
        {
          result = _thunar_vfs_io_local_get_metadata (local_path, metadata, error);
          thunar_vfs_path_unref (local_path);
        }
      break;

    case THUNAR_VFS_INFO_METADATA_TRASH_ORIGINAL_PATH:
      if (!_thunar_vfs_io_trash_get_trash_info (path, &result, NULL, error))
        result = NULL;
      break;

    case THUNAR_VFS_INFO_METADATA_TRASH_DELETION_DATE:
      if (!_thunar_vfs_io_trash_get_trash_info (path, NULL, &result, error))
        result = NULL;
      break;

    default:
      _thunar_vfs_set_g_error_not_supported (error);
      break;
    }

  return result;
}



/**
 * _thunar_vfs_io_trash_listdir:
 * @path      : a valid trash:-URI for the trash folder, whose contents to list.
 * @error     : return location for errors, or %NULL.
 *
 * Returns the files in the trash folder @path or %NULL if either no files
 * are present in the folder at @path, or an error occurred, in which case
 * the @error will be set to point to a #GError describing the cause.
 *
 * The caller is responsible to free the returned #GList of #ThunarVfsInfo<!---->s
 * using thunar_vfs_info_list_free() when no longer needed.
 *
 * Return value: the #ThunarVfsInfo<!---->s for the files in the folder at @path.
 **/
GList*
_thunar_vfs_io_trash_listdir (ThunarVfsPath *path,
                              GError       **error)
{
  ThunarVfsInfo *info;
  GList         *list;
  GList         *sp;
  GList         *tp;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_trash (path), NULL);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* scan the paths in the folder */
  list = _thunar_vfs_io_trash_scandir (path, TRUE, NULL, error);
  if (G_LIKELY (list != NULL))
    {
      /* associate file infos with the paths in the folder */
      for (sp = tp = list; sp != NULL; sp = sp->next)
        {
          /* try to determine the file info */
          info = _thunar_vfs_io_trash_get_info (sp->data, NULL);

          /* replace the path with the info on the list */
          if (G_LIKELY (info != NULL))
            {
              /* just decrease the ref_count on path (info holds a reference now) */
              _thunar_vfs_path_unref_nofree (sp->data);

              /* add the info to the list */
              tp->data = info;
              tp = tp->next;
            }
          else
            {
              /* no info, may need to free the path */
              thunar_vfs_path_unref (sp->data);
            }
        }

      /* release the not-filled list items (only non-NULL in case of an info error) */
      if (G_UNLIKELY (tp != NULL))
        {
          if (G_LIKELY (tp->prev != NULL))
            tp->prev->next = NULL;
          else
            list = NULL;
          g_list_free (tp);
        }
    }

  return list;
}



/**
 * _thunar_vfs_io_trash_copy_file:
 * @source_path        : the #ThunarVfsPath of the source file.
 * @target_path        : the #ThunarVfsPath of the target location.
 * @target_path_return : return location for the final #ThunarVfsPath to which
 *                       the file was copied.
 * @callback           : see #ThunarVfsIOOpsProgressCallback.
 * @callback_data      : additional user data for @callback.
 * @error              : return location for errors or %NULL.
 *
 * Copies a file from or to the trash. Cannot currently handle copying files
 * within the trash.
 *
 * See the description of _thunar_vfs_io_ops_copy_file() for further details.
 *
 * Return value: %TRUE if succeed, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_trash_copy_file (ThunarVfsPath                 *source_path,
                                ThunarVfsPath                 *target_path,
                                ThunarVfsPath                **target_path_return,
                                ThunarVfsIOOpsProgressCallback callback,
                                gpointer                       callback_data,
                                GError                       **error)
{
  ThunarVfsPath *source_local_path;
  ThunarVfsPath *target_local_path;
  gboolean       succeed = FALSE;
  gchar         *file_id;
  guint          trash_id;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_trash (source_path) || _thunar_vfs_path_is_trash (target_path), FALSE);
  _thunar_vfs_return_val_if_fail (!thunar_vfs_path_is_root (source_path), FALSE);
  _thunar_vfs_return_val_if_fail (!thunar_vfs_path_is_root (target_path), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (target_path_return != NULL, FALSE);
  _thunar_vfs_return_val_if_fail (callback != NULL, FALSE);

  /* check if we're moving to or from the trash */
  if (_thunar_vfs_path_is_trash (source_path) && _thunar_vfs_path_is_trash (target_path))
    {
      /* we don't support copying files within the trash */
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_IO, _("Cannot move or copy files within the trash"));
    }
  else if (_thunar_vfs_path_is_trash (source_path)) /* copying out of the trash */
    {
      /* translate the source path to a local path */
      source_local_path = _thunar_vfs_path_translate (source_path, THUNAR_VFS_PATH_SCHEME_FILE, error);
      if (G_LIKELY (source_local_path != NULL))
        {
          /* try to copy the file out of the trash using the generic copy logic for local files */
          succeed = _thunar_vfs_io_ops_copy_file (source_local_path, target_path, NULL, callback, callback_data, error);
          thunar_vfs_path_unref (source_local_path);

          /* target path is the same for local paths */
          if (G_LIKELY (succeed))
            *target_path_return = thunar_vfs_path_ref (target_path);
        }
    }
  else if (!thunar_vfs_path_is_root (target_path->parent)) /* copying into a trash subfolder */
    {
      /* translate the target path to a local path */
      target_local_path = _thunar_vfs_path_translate (target_path, THUNAR_VFS_PATH_SCHEME_FILE, error);
      if (G_LIKELY (target_local_path != NULL))
        {
          /* try to copy the file into the trash */
          succeed = _thunar_vfs_io_ops_copy_file (source_path, target_local_path, NULL, callback, callback_data, error);
          thunar_vfs_path_unref (target_local_path);

          /* target path is the same for non-toplevel trash */
          if (G_LIKELY (succeed))
            *target_path_return = thunar_vfs_path_ref (target_path);
        }
    }
  else /* copying to the toplevel trash folder */
    {
      /* generate a new .trashinfo file */
      if (_thunar_vfs_io_trash_new_trash_info (source_path, &trash_id, &file_id, error))
        {
          /* determine the new target path in the trash */
          target_path = _thunar_vfs_io_trash_path_new (trash_id, file_id, "");

          /* translate the target path to a local path */
          target_local_path = _thunar_vfs_path_translate (target_path, THUNAR_VFS_PATH_SCHEME_FILE, error);
          if (G_LIKELY (target_local_path != NULL))
            {
              /* try to copy the file info the trash (ensuring that the "files" directory exists) */
              succeed = (_thunar_vfs_io_ops_mkdir (target_local_path->parent, 0700, THUNAR_VFS_IO_OPS_IGNORE_EEXIST, error)
                      && _thunar_vfs_io_ops_copy_file (source_path, target_local_path, NULL, callback, callback_data, error));
              thunar_vfs_path_unref (target_local_path);
            }

          /* check if we failed */
          if (G_UNLIKELY (!succeed))
            {
              /* drop the file from the trash and release the target path */
              if (!_thunar_vfs_io_trash_remove (target_path, NULL)) 
                g_warning ("Failed to remove stale trash handle %s in %u", file_id, trash_id);
              thunar_vfs_path_unref (target_path);
            }
          else
            {
              /* schedule a changed notification on the trash root folder (parent of this path) */
              thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_CHANGED, target_path->parent);

              /* return the new target path */
              *target_path_return = target_path;
            }

          /* cleanup */
          g_free (file_id);
        }
    }

  return succeed;
}



/**
 * _thunar_vfs_io_trash_move_file:
 * @source_path        : the #ThunarVfsPath of the source file.
 * @target_path        : the #ThunarVfsPath of the target location.
 * @target_path_return : return location for the final #ThunarVfsPath to which
 *                       the file was moved.
 * @error              : return location for errors or %NULL.
 *
 * Moves a file from or to the trash. Cannot currently handle moving files
 * within the trash.
 *
 * See the description of _thunar_vfs_io_ops_move_file() for further details.
 *
 * Return value: %TRUE if succeed, %FALSE otherwise.
 **/
gboolean
_thunar_vfs_io_trash_move_file (ThunarVfsPath  *source_path,
                                ThunarVfsPath  *target_path,
                                ThunarVfsPath **target_path_return,
                                GError        **error)
{
  ThunarVfsPath *source_local_path;
  ThunarVfsPath *target_local_path;
  gboolean       succeed = FALSE;
  gchar         *file_id;
  guint          trash_id;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_trash (source_path) || _thunar_vfs_path_is_trash (target_path), FALSE);
  _thunar_vfs_return_val_if_fail (!thunar_vfs_path_is_root (source_path), FALSE);
  _thunar_vfs_return_val_if_fail (!thunar_vfs_path_is_root (target_path), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  _thunar_vfs_return_val_if_fail (target_path_return != NULL, FALSE);

  /* check if we're moving to or from the trash */
  if (_thunar_vfs_path_is_trash (source_path) && _thunar_vfs_path_is_trash (target_path))
    {
      /* we don't support moving files within the trash */
      g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_IO, _("Cannot move or copy files within the trash"));
    }
  else if (_thunar_vfs_path_is_trash (source_path)) /* moving out of the trash */
    {
      /* translate the source path to a local path */
      source_local_path = _thunar_vfs_path_translate (source_path, THUNAR_VFS_PATH_SCHEME_FILE, error);
      if (G_LIKELY (source_local_path != NULL))
        {
          /* try to move the file out of the trash and remove the associated .trashinfo file */
          succeed = (_thunar_vfs_io_local_move_file (source_local_path, target_path, error)
                  && _thunar_vfs_io_trash_remove (source_path, error));
          thunar_vfs_path_unref (source_local_path);

          /* target path is the same for local paths */
          if (G_LIKELY (succeed))
            *target_path_return = thunar_vfs_path_ref (target_path);
        }
    }
  else if (!thunar_vfs_path_is_root (target_path->parent)) /* moving into a trash subfolder */
    {
      /* translate the target path to a local path */
      target_local_path = _thunar_vfs_path_translate (target_path, THUNAR_VFS_PATH_SCHEME_FILE, error);
      if (G_LIKELY (target_local_path != NULL))
        {
          /* try to move the file into the trash */
          succeed = _thunar_vfs_io_local_move_file (source_path, target_local_path, error);
          thunar_vfs_path_unref (target_local_path);

          /* target path is the same for non-toplevel trash */
          if (G_LIKELY (succeed))
            *target_path_return = thunar_vfs_path_ref (target_path);
        }
    }
  else /* moving to the toplevel trash folder */
    {
      /* generate a new .trashinfo file */
      if (_thunar_vfs_io_trash_new_trash_info (source_path, &trash_id, &file_id, error))
        {
          /* determine the new target path in the trash */
          target_path = _thunar_vfs_io_trash_path_new (trash_id, file_id, "");

          /* translate the target path to a local path */
          target_local_path = _thunar_vfs_path_translate (target_path, THUNAR_VFS_PATH_SCHEME_FILE, error);
          if (G_LIKELY (target_local_path != NULL))
            {
              /* try to move the file info the trash (ensuring that the "files" directory exists) */
              succeed = (_thunar_vfs_io_ops_mkdir (target_local_path->parent, 0700, THUNAR_VFS_IO_OPS_IGNORE_EEXIST, error)
                      && _thunar_vfs_io_local_move_file (source_path, target_local_path, error));
              thunar_vfs_path_unref (target_local_path);
            }

          /* check if we failed */
          if (G_UNLIKELY (!succeed))
            {
              /* drop the file from the trash and release the target path */
              if (!_thunar_vfs_io_trash_remove (target_path, NULL)) 
                g_warning ("Failed to remove stale trash handle %s in %u", file_id, trash_id);
              thunar_vfs_path_unref (target_path);
            }
          else
            {
              /* schedule a changed notification on the trash root folder (parent of this path) */
              thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_CHANGED, target_path->parent);

              /* return the new target path */
              *target_path_return = target_path;
            }

          /* cleanup */
          g_free (file_id);
        }
    }

  return succeed;
}



/**
 * _thunar_vfs_io_trash_remove:
 * @path  : the #ThunarVfsPath to the trash resource to remove.
 * @error : return location for errors or %NULL.
 *
 * Removes the file or folder from the trash that is identified
 * by the specified @path.
 *
 * Return value: %TRUE if the removal was successfull, %FALSE
 *               otherwise.
 **/
gboolean
_thunar_vfs_io_trash_remove (ThunarVfsPath *path,
                             GError       **error)
{
  GError *err = NULL;
  gchar  *absolute_path;
  gchar  *relative_path;
  gchar  *trash_dir;
  gchar  *file_id;
  guint   trash_id;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_trash (path), FALSE);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, FALSE);

  /* try to parse the trash path */
  if (!_thunar_vfs_io_trash_path_parse (path, &trash_id, &file_id, &relative_path, error))
    return FALSE;

  /* determine the trash directory for the trash-id */
  trash_dir = _thunar_vfs_io_trash_get_trash_dir (trash_id, &err);
  if (G_LIKELY (trash_dir != NULL))
    {
      /* try to remove the file or folder in the trash */
      absolute_path = g_build_filename (trash_dir, "files", file_id, relative_path, NULL);
      if (g_remove (absolute_path) < 0 && errno != ENOENT)
        _thunar_vfs_set_g_error_from_errno3 (&err);
      g_free (absolute_path);

      /* check if need to remove the matching .trashinfo file */
      if (G_LIKELY (err == NULL) && relative_path == NULL)
        {
          /* determine the path for the .trashinfo file and unlink it (ignoring errors) */
          absolute_path = g_strconcat (trash_dir, G_DIR_SEPARATOR_S "info" G_DIR_SEPARATOR_S, file_id, ".trashinfo", NULL);
          g_unlink (absolute_path);
          g_free (absolute_path);

          /* schedule a changed notification on the trash root folder (parent of this path) */
          thunar_vfs_monitor_feed (_thunar_vfs_monitor, THUNAR_VFS_MONITOR_EVENT_CHANGED, path->parent);
        }
    }

  /* cleanup */
  g_free (relative_path);
  g_free (trash_dir);
  g_free (file_id);

  /* check if we failed */
  if (G_UNLIKELY (err != NULL))
    {
      /* propagate the error */
      g_propagate_error (error, err);
      return FALSE;
    }

  return TRUE;
}



/**
 * _thunar_vfs_io_trash_scandir:
 * @path               : the #ThunarVfsPath to a directory in the trash. May
 *                       also be the trash root folder itself.
 * @follow_links       : %TRUE to follow symlinks to directories.
 * @directories_return : pointer to a list into which the direct subfolders
 *                       found during scanning will be placed (for recursive
 *                       scanning), or %NULL if you are not interested in a
 *                       separate list of subfolders. Note that the returned
 *                       list items need to be freed, but the #ThunarVfsPath<!---->s
 *                       in the list do not have an extra reference.
 * @error              : return location for errors or %NULL.
 *
 * Scans the trash directory at @path and returns the #ThunarVfsPath<!---->s in the
 * directory.
 *
 * The list returned in @directories_return, if not %NULL, must be freed using
 * g_list_free() when no longer needed.
 *
 * The returned list of #ThunarVfsPath<!---->s must be freed by the caller using
 * thunar_vfs_path_list_unref() when no longer needed.
 *
 * Return value: the list of #ThunarVfsPath<!---->s in the folder at the @path,
 *               or %NULL in case of an error. Note that %NULL may also mean that
 *               the folder is empty.
 **/
GList*
_thunar_vfs_io_trash_scandir (ThunarVfsPath *path,
                              gboolean       follow_links,
                              GList        **directories_return,
                              GError       **error)
{
  const gchar *name;
  gchar       *absolute_path;
  gchar       *file_path;
  GList       *path_list = NULL;
  guint        n;
  GDir        *dp;

  _thunar_vfs_return_val_if_fail (_thunar_vfs_path_is_trash (path), NULL);
  _thunar_vfs_return_val_if_fail (error == NULL || *error == NULL, NULL);

  /* scanning a directory in the trash is easy */
  if (!thunar_vfs_path_is_root (path))
    {
      /* determine the absolute local path to the directory */
      absolute_path = _thunar_vfs_io_trash_path_resolve (path, error);
      if (G_LIKELY (absolute_path != NULL))
        {
          /* scan the local directory, generating trash paths */
          path_list = _thunar_vfs_os_scandir (path, absolute_path, follow_links, directories_return, error);
          g_free (absolute_path);
        }
    }
  else
    {
      /* unconditionally scan for trash directories to notice new
       * (hot-plugged) trash directories on removable drives and media.
       */
      _thunar_vfs_io_trash_rescan_mounts ();

      /* unconditionally rescan the trash directories */
      _thunar_vfs_io_trash_rescan ();

      /* acquire the trash subsystem lock */
      G_LOCK (_thunar_vfs_io_trash);

      /* process all trash directories */
      for (n = 0; n < _thunar_vfs_io_n_trashes; ++n)
        {
          /* determine the paths for the trashed files in this trash directory */
          absolute_path = g_build_filename (_thunar_vfs_io_trashes[n].trash_dir, "files", NULL);
          dp = g_dir_open (absolute_path, 0, NULL);
          if (G_LIKELY (dp != NULL))
            {
              /* process all items in this folder */
              for (;;)
                {
                  /* determine the name of the next item */
                  name = g_dir_read_name (dp);
                  if (G_UNLIKELY (name == NULL))
                    break;

                  /* add a path for this item */
                  path_list = g_list_prepend (path_list, _thunar_vfs_io_trash_path_new (n, name, ""));

                  /* check if we should return directories in a special list */
                  if (G_UNLIKELY (directories_return != NULL))
                    {
                      /* check if we have a directory (according to the follow_links policy) */
                      file_path = g_build_filename (absolute_path, name, NULL);
                      if (g_file_test (file_path, G_FILE_TEST_IS_DIR) && (follow_links || !g_file_test (file_path, G_FILE_TEST_IS_SYMLINK)))
                        *directories_return = g_list_prepend (*directories_return, path_list->data);
                      g_free (file_path);
                    }
                }
              g_dir_close (dp);
            }
          g_free (absolute_path);
        }

      /* release the trash subsystem lock */
      G_UNLOCK (_thunar_vfs_io_trash);
    }

  return path_list;
}



/**
 * _thunar_vfs_io_trash_init:
 *
 * Initializes the trash subsystem.
 **/
void
_thunar_vfs_io_trash_init (void)
{
  const gchar *home_dir;
  struct stat  statb;

  /* determine the home folder path */
  home_dir = g_get_home_dir ();

  /* determine the device of the home folder */
  if (stat (home_dir, &statb) == 0)
    _thunar_vfs_io_trash_homedev = statb.st_dev;

  /* setup the home trash */
  _thunar_vfs_io_n_trashes = 1;
  _thunar_vfs_io_trashes = g_new (ThunarVfsIOTrash, 1);
  _thunar_vfs_io_trashes->top_dir = g_strdup (home_dir);
  _thunar_vfs_io_trashes->trash_dir = g_build_filename (g_get_user_data_dir (), "Trash", NULL);
  _thunar_vfs_io_trashes->mtime = 0;
  _thunar_vfs_io_trashes->empty = TRUE;
}



/**
 * _thunar_vfs_io_trash_shutdown:
 *
 * Shuts down the trash subsystem.
 **/
void
_thunar_vfs_io_trash_shutdown (void)
{
  /* check if we have a pending trash rescan timer */
  if (G_LIKELY (_thunar_vfs_io_trash_timer_id != 0))
    {
      /* kill the pending trash rescan timer */
      g_source_remove (_thunar_vfs_io_trash_timer_id);
      _thunar_vfs_io_trash_timer_id = 0;
    }

  /* free the active trashes */
  while (_thunar_vfs_io_n_trashes-- > 0)
    {
      /* release top and trash dir */
      g_free (_thunar_vfs_io_trashes[_thunar_vfs_io_n_trashes].top_dir);
      g_free (_thunar_vfs_io_trashes[_thunar_vfs_io_n_trashes].trash_dir);
    }
  g_free (_thunar_vfs_io_trashes);
  _thunar_vfs_io_trashes = NULL;
}



#define __THUNAR_VFS_IO_TRASH_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
