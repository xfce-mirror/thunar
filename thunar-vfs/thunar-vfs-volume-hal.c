/* $Id$ */
/*-
 * Copyright (c) 2005-2006 Benedikt Meurer <benny@xfce.org>
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

#ifdef HAVE_ERRNO_H
#include <errno.h>
#endif
#ifdef HAVE_MNTENT_H
#include <mntent.h>
#endif
#include <stdio.h>
#ifdef HAVE_STDLIB_H
#include <stdlib.h>
#endif

#include <dbus/dbus-glib-lowlevel.h>

#include <libhal.h>
#include <libhal-storage.h>

#include <thunar-vfs/thunar-vfs-exec.h>
#include <thunar-vfs/thunar-vfs-volume-hal.h>
#include <thunar-vfs/thunar-vfs-volume-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



static void                  thunar_vfs_volume_hal_class_init       (ThunarVfsVolumeHalClass *klass);
static void                  thunar_vfs_volume_hal_finalize         (GObject                 *object);
static ThunarVfsVolumeKind   thunar_vfs_volume_hal_get_kind         (ThunarVfsVolume         *volume);
static const gchar          *thunar_vfs_volume_hal_get_name         (ThunarVfsVolume         *volume);
static ThunarVfsVolumeStatus thunar_vfs_volume_hal_get_status       (ThunarVfsVolume         *volume);
static ThunarVfsPath        *thunar_vfs_volume_hal_get_mount_point  (ThunarVfsVolume         *volume);
static gboolean              thunar_vfs_volume_hal_eject            (ThunarVfsVolume         *volume,
                                                                     GtkWidget               *window,
                                                                     GError                 **error);
static gboolean              thunar_vfs_volume_hal_mount            (ThunarVfsVolume         *volume,
                                                                     GtkWidget               *window,
                                                                     GError                 **error);
static gboolean              thunar_vfs_volume_hal_unmount          (ThunarVfsVolume         *volume,
                                                                     GtkWidget               *window,
                                                                     GError                 **error);
static ThunarVfsPath        *thunar_vfs_volume_hal_find_mount_point (ThunarVfsVolumeHal      *volume_hal,
                                                                     const gchar             *file);
static void                  thunar_vfs_volume_hal_update           (ThunarVfsVolumeHal      *volume_hal,
                                                                     LibHalContext           *context,
                                                                     LibHalVolume            *hv,
                                                                     LibHalDrive             *hd);



struct _ThunarVfsVolumeHalClass
{
  ThunarVfsVolumeClass __parent__;
};

struct _ThunarVfsVolumeHal
{
  ThunarVfsVolume       __parent__;

  gchar                *udi;
  gchar                *drive_udi;

  gchar                *device_file;
  gchar                *device_label;
  ThunarVfsPath        *mount_point;
  ThunarVfsVolumeKind   kind;
  ThunarVfsVolumeStatus status;
};



static GObjectClass *thunar_vfs_volume_hal_parent_class;



GType
thunar_vfs_volume_hal_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsVolumeHalClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_volume_hal_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsVolumeHal),
        0,
        NULL,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_VOLUME, I_("ThunarVfsVolumeHal"), &info, 0);
    }

  return type;
}



static void
thunar_vfs_volume_hal_class_init (ThunarVfsVolumeHalClass *klass)
{
  ThunarVfsVolumeClass *thunarvfs_volume_class;
  GObjectClass         *gobject_class;

  /* determine the parent type class */
  thunar_vfs_volume_hal_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_volume_hal_finalize;

  thunarvfs_volume_class = THUNAR_VFS_VOLUME_CLASS (klass);
  thunarvfs_volume_class->get_kind = thunar_vfs_volume_hal_get_kind;
  thunarvfs_volume_class->get_name = thunar_vfs_volume_hal_get_name;
  thunarvfs_volume_class->get_status = thunar_vfs_volume_hal_get_status;
  thunarvfs_volume_class->get_mount_point = thunar_vfs_volume_hal_get_mount_point;
  thunarvfs_volume_class->eject = thunar_vfs_volume_hal_eject;
  thunarvfs_volume_class->mount = thunar_vfs_volume_hal_mount;
  thunarvfs_volume_class->unmount = thunar_vfs_volume_hal_unmount;
}



static void
thunar_vfs_volume_hal_finalize (GObject *object)
{
  ThunarVfsVolumeHal *volume_hal = THUNAR_VFS_VOLUME_HAL (object);

  g_free (volume_hal->udi);
  g_free (volume_hal->drive_udi);

  g_free (volume_hal->device_file);
  g_free (volume_hal->device_label);

  /* release the mount point (if any) */
  if (G_LIKELY (volume_hal->mount_point != NULL))
    thunar_vfs_path_unref (volume_hal->mount_point);

  (*G_OBJECT_CLASS (thunar_vfs_volume_hal_parent_class)->finalize) (object);
}



static ThunarVfsVolumeKind
thunar_vfs_volume_hal_get_kind (ThunarVfsVolume *volume)
{
  return THUNAR_VFS_VOLUME_HAL (volume)->kind;
}



static const gchar*
thunar_vfs_volume_hal_get_name (ThunarVfsVolume *volume)
{
  return THUNAR_VFS_VOLUME_HAL (volume)->device_label;
}



static ThunarVfsVolumeStatus
thunar_vfs_volume_hal_get_status (ThunarVfsVolume *volume)
{
  return THUNAR_VFS_VOLUME_HAL (volume)->status;
}



static ThunarVfsPath*
thunar_vfs_volume_hal_get_mount_point (ThunarVfsVolume *volume)
{
  return THUNAR_VFS_VOLUME_HAL (volume)->mount_point;
}



static gboolean
thunar_vfs_volume_hal_eject (ThunarVfsVolume *volume,
                             GtkWidget       *window,
                             GError         **error)
{
  ThunarVfsVolumeHal *volume_hal = THUNAR_VFS_VOLUME_HAL (volume);
  gboolean            result;
  gchar              *program;
  gchar              *quoted;

  /* check if gnome-eject is present */
  program = g_find_program_in_path ("gnome-eject");
  if (G_LIKELY (program != NULL))
    {
      /* gnome-eject doesn't seem to unmount properly first */
      result = thunar_vfs_volume_hal_unmount (volume, window, error);
      if (G_LIKELY (result))
        {
          /* try to use gnome-eject then */
          quoted = g_shell_quote (volume_hal->udi);
          result = thunar_vfs_exec_sync ("%s -t -h %s", error, program, quoted);
          g_free (quoted);
        }

      /* cleanup */
      g_free (program);
    }
  else
    {
      /* use eject */
      quoted = g_path_get_basename (volume_hal->device_file);
      result = thunar_vfs_exec_sync ("eject %s", error, quoted);
      g_free (quoted);
    }

  /* check if we were successfull */
  if (G_LIKELY (result))
    {
      /* reset the status */
      volume_hal->status &= ~(THUNAR_VFS_VOLUME_STATUS_MOUNTED | THUNAR_VFS_VOLUME_STATUS_PRESENT);

      /* emit "changed" on the volume */
      thunar_vfs_volume_changed (THUNAR_VFS_VOLUME (volume_hal));
    }

  return result;
}



static gboolean
thunar_vfs_volume_hal_mount (ThunarVfsVolume *volume,
                             GtkWidget       *window,
                             GError         **error)
{
  ThunarVfsVolumeHal *volume_hal = THUNAR_VFS_VOLUME_HAL (volume);
  ThunarVfsPath      *path;
  gboolean            result;
  gchar              *mount_point;
  gchar              *program;
  gchar              *quoted;

  /* check if pmount-hal is present */
  program = g_find_program_in_path ("pmount-hal");
  if (G_LIKELY (program != NULL))
    {
      /* try to use pmount-hal then */
      quoted = g_shell_quote (volume_hal->udi);
      thunar_vfs_exec_sync ("%s %s", NULL, program, quoted);
      g_free (program);
      g_free (quoted);

      /* pmount-hal returns error if already mounted, so
       * we set result to TRUE, which means it will always
       * be looked up in /proc/mounts below.
       * See http://bugzilla.xfce.org/show_bug.cgi?id=1833.
       */
      result = TRUE;
    }
  else
    {
      /* check if gnome-mount is present */
      program = g_find_program_in_path ("gnome-mount");
      if (G_LIKELY (program != NULL))
        {
          /* try to use gnome-mount then */
          quoted = g_shell_quote (volume_hal->udi);
          result = thunar_vfs_exec_sync ("%s -t -h %s", error, program, quoted);
          g_free (program);
          g_free (quoted);
        }
      else
        {
          /* fallback to plain mount */
          mount_point = thunar_vfs_path_dup_string (volume_hal->mount_point);
          quoted = g_shell_quote (mount_point);
          result = thunar_vfs_exec_sync ("mount %s", error, quoted);
          g_free (mount_point);
          g_free (quoted);
        }
    }

  /* check if we were successfull */
  if (G_LIKELY (result))
    {
      /* try to figure out where the device was mounted */
      path = thunar_vfs_volume_hal_find_mount_point (volume_hal, "/proc/mounts");
      if (G_LIKELY (path != NULL))
        {
          /* we must have been mounted successfully */
          volume_hal->status |= THUNAR_VFS_VOLUME_STATUS_MOUNTED | THUNAR_VFS_VOLUME_STATUS_PRESENT;

          /* replace the existing mount point */
          thunar_vfs_path_unref (volume_hal->mount_point);
          volume_hal->mount_point = path;

          /* tell everybody that we have a new state */
          thunar_vfs_volume_changed (THUNAR_VFS_VOLUME (volume_hal));
        }
      else
        {
          /* something went wrong, for sure */
          g_set_error (error, G_FILE_ERROR, G_FILE_ERROR_FAILED, _("Failed to determine the mount point for %s"), volume_hal->device_file);
          result = FALSE;
        }
    }

  return result;
}



static gboolean
thunar_vfs_volume_hal_unmount (ThunarVfsVolume *volume,
                               GtkWidget       *window,
                               GError         **error)
{
  ThunarVfsVolumeHal *volume_hal = THUNAR_VFS_VOLUME_HAL (volume);
  gboolean            result;
  gchar              *mount_point;
  gchar              *program;
  gchar              *quoted;

  /* check if pumount is present */
  program = g_find_program_in_path ("pumount");
  if (G_LIKELY (program != NULL))
    {
      /* try to use pumount then */
      mount_point = thunar_vfs_path_dup_string (volume_hal->mount_point);
      quoted = g_shell_quote (mount_point);
      result = thunar_vfs_exec_sync ("%s %s", error, program, quoted);
      g_free (mount_point);
      g_free (program);
      g_free (quoted);
    }
  else
    {
      /* check if gnome-umount is present */
      program = g_find_program_in_path ("gnome-umount");
      if (G_LIKELY (program != NULL))
        {
          /* try to use gnome-umount then */
          quoted = g_shell_quote (volume_hal->udi);
          result = thunar_vfs_exec_sync ("%s -t -h %s", error, program, quoted);
          g_free (program);
          g_free (quoted);
        }
      else
        {
          /* fallback to plain umount */
          mount_point = thunar_vfs_path_dup_string (volume_hal->mount_point);
          quoted = g_shell_quote (mount_point);
          result = thunar_vfs_exec_sync ("umount %s", error, quoted);
          g_free (mount_point);
          g_free (quoted);
        }
    }

  /* check if we were successfull */
  if (G_LIKELY (result))
    {
      /* reset the status */
      volume_hal->status &= ~THUNAR_VFS_VOLUME_STATUS_MOUNTED;

      /* emit "changed" on the volume */
      thunar_vfs_volume_changed (THUNAR_VFS_VOLUME (volume_hal));
    }

  return result;
}



static ThunarVfsPath*
thunar_vfs_volume_hal_find_mount_point (ThunarVfsVolumeHal *volume_hal,
                                        const gchar        *file)
{
  ThunarVfsPath *mount_point = NULL;
  struct mntent *mntent;
  FILE          *fp;

  /* try to open that file as mnt entry list */
  fp = setmntent (file, "r");
  if (G_LIKELY (fp != NULL))
    {
      /* process all mnt entries */
      while (mount_point == NULL)
        {
          /* read the next entry */
          mntent = getmntent (fp);
          if (mntent == NULL)
            break;

          /* check if this is the entry we are looking for */
          if (exo_str_is_equal (mntent->mnt_fsname, volume_hal->device_file))
            {
              /* and there's our mount point */
              mount_point = thunar_vfs_path_new (mntent->mnt_dir, NULL);
            }
        }

      /* close the file handle */
      endmntent (fp);
    }

  return mount_point;
}



static void
thunar_vfs_volume_hal_update (ThunarVfsVolumeHal *volume_hal,
                              LibHalContext      *context,
                              LibHalVolume       *hv,
                              LibHalDrive        *hd)
{
  LibHalStoragePolicy *policy;
  const gchar         *desired_mount_point;
  const gchar         *volume_label;
  gchar               *mount_root;
  gchar               *basename;
  gchar               *filename;

  g_return_if_fail (THUNAR_VFS_IS_VOLUME_HAL (volume_hal));
  g_return_if_fail (hv != NULL);
  g_return_if_fail (hd != NULL);

  /* just allocate a policy (doesn't seem to be very useful) */
  policy = libhal_storage_policy_new ();

  /* reset the volume status */
  volume_hal->status = 0;

  /* determine the new device file */
  g_free (volume_hal->device_file);
  volume_hal->device_file = g_strdup (libhal_volume_get_device_file (hv));

  /* determine the new label */
  g_free (volume_hal->device_label);
  volume_label = libhal_volume_get_label (hv);
  if (G_LIKELY (volume_label != NULL && *volume_label != '\0'))
    {
      /* just use the label provided by HAL */
      volume_hal->device_label = g_strdup (volume_label);
    }
  else
    {
      /* use the basename of the device file as label */
      volume_hal->device_label = g_path_get_basename (volume_hal->device_file);
    }

  /* release the previous mount point (if any) */
  if (G_LIKELY (volume_hal->mount_point != NULL))
    {
      thunar_vfs_path_unref (volume_hal->mount_point);
      volume_hal->mount_point = NULL;
    }

  /* determine the type of the volume */
  switch (libhal_drive_get_type (hd))
    {
    case LIBHAL_DRIVE_TYPE_CDROM:
      /* check which kind of CD-ROM/DVD we have */
      switch (libhal_volume_get_disc_type (hv))
        {
        case LIBHAL_VOLUME_DISC_TYPE_CDROM:
          volume_hal->kind = THUNAR_VFS_VOLUME_KIND_CDROM;
          break;

        case LIBHAL_VOLUME_DISC_TYPE_CDR:
          volume_hal->kind = THUNAR_VFS_VOLUME_KIND_CDR;
          break;

        case LIBHAL_VOLUME_DISC_TYPE_CDRW:
          volume_hal->kind = THUNAR_VFS_VOLUME_KIND_CDRW;
          break;

        case LIBHAL_VOLUME_DISC_TYPE_DVDROM:
          volume_hal->kind = THUNAR_VFS_VOLUME_KIND_DVDROM;
          break;

        case LIBHAL_VOLUME_DISC_TYPE_DVDRAM:
          volume_hal->kind = THUNAR_VFS_VOLUME_KIND_DVDRAM;
          break;

        case LIBHAL_VOLUME_DISC_TYPE_DVDR:
          volume_hal->kind = THUNAR_VFS_VOLUME_KIND_DVDR;
          break;

        case LIBHAL_VOLUME_DISC_TYPE_DVDRW:
          volume_hal->kind = THUNAR_VFS_VOLUME_KIND_DVDRW;
          break;

        case LIBHAL_VOLUME_DISC_TYPE_DVDPLUSR:
          volume_hal->kind = THUNAR_VFS_VOLUME_KIND_DVDPLUSR;
          break;

        case LIBHAL_VOLUME_DISC_TYPE_DVDPLUSRW:
          volume_hal->kind = THUNAR_VFS_VOLUME_KIND_DVDPLUSRW;
          break;

        default:
          /* unsupported disc type */
          volume_hal->kind = THUNAR_VFS_VOLUME_KIND_UNKNOWN;
          break;
        }
      break;

    case LIBHAL_DRIVE_TYPE_FLOPPY:
      volume_hal->kind = THUNAR_VFS_VOLUME_KIND_FLOPPY;
      break;

    case LIBHAL_DRIVE_TYPE_PORTABLE_AUDIO_PLAYER:
      volume_hal->kind = THUNAR_VFS_VOLUME_KIND_AUDIO_PLAYER;
      break;

    default:
      /* check if the drive is connected to the USB bus */
      if (libhal_drive_get_bus (hd) == LIBHAL_DRIVE_BUS_USB)
        {
          /* we consider the drive to be an USB stick */
          volume_hal->kind = THUNAR_VFS_VOLUME_KIND_USBSTICK;
        }
      else
        {
          /* fallback to harddisk drive */
          volume_hal->kind = THUNAR_VFS_VOLUME_KIND_HARDDISK;
        }
      break;
    }

  /* non-disc drives are always present, otherwise it must be a data disc to be usable */
  if (!libhal_volume_is_disc (hv) || libhal_volume_disc_has_data (hv))
    volume_hal->status |= THUNAR_VFS_VOLUME_STATUS_PRESENT;

  /* check if the volume is currently mounted */
  if (libhal_volume_is_mounted (hv))
    {
      /* try to determine the new mount point */
      volume_hal->mount_point = thunar_vfs_path_new (libhal_volume_get_mount_point (hv), NULL);

      /* we only mark the volume as mounted if we have a valid mount point */
      if (G_LIKELY (volume_hal->mount_point != NULL))
        volume_hal->status |= THUNAR_VFS_VOLUME_STATUS_MOUNTED | THUNAR_VFS_VOLUME_STATUS_PRESENT;
    }
  else
    {
      /* we don't trust HAL, so let's see what /proc/mounts says about the volume */
      volume_hal->mount_point = thunar_vfs_volume_hal_find_mount_point (volume_hal, "/proc/mounts");

      /* we must have been mounted successfully if we have a mount point */
      if (G_LIKELY (volume_hal->mount_point != NULL))
        volume_hal->status |= THUNAR_VFS_VOLUME_STATUS_MOUNTED | THUNAR_VFS_VOLUME_STATUS_PRESENT;
    }

  /* check if we have to figure out the mount point ourself */
  if (G_UNLIKELY (volume_hal->mount_point == NULL))
    {
      /* ask HAL for the default mount root (fallback to /media) */
      mount_root = libhal_drive_policy_default_get_mount_root (context);
      if (G_UNLIKELY (mount_root == NULL || !g_path_is_absolute (mount_root)))
        {
          /* fallback to /media (seems to be sane) */
          g_free (mount_root);
          mount_root = g_strdup ("/media");
        }

      /* lets see, maybe /etc/fstab knows where to mount */
      volume_hal->mount_point = thunar_vfs_volume_hal_find_mount_point (volume_hal, "/etc/fstab");

      /* if we still don't have a mount point, ask HAL */
      if (G_UNLIKELY (volume_hal->mount_point == NULL))
        {
          /* determine the desired mount point and prepend the mount root */
          desired_mount_point = libhal_volume_policy_get_desired_mount_point (hd, hv, policy);
          if (G_LIKELY (desired_mount_point != NULL && *desired_mount_point != '\0'))
            {
              filename = g_build_filename (mount_root, desired_mount_point, NULL);
              volume_hal->mount_point = thunar_vfs_path_new (filename, NULL);
              g_free (filename);
            }
        }

      /* ok, last fallback, just use <mount-root>/<device> */
      if (G_UNLIKELY (volume_hal->mount_point == NULL))
        {
          /* <mount-root>/<device> looks like a good idea */
          basename = g_path_get_basename (volume_hal->device_file);
          filename = g_build_filename (mount_root, basename, NULL);
          volume_hal->mount_point = thunar_vfs_path_new (filename, NULL);
          g_free (filename);
          g_free (basename);
        }

      /* release the mount root */
      g_free (mount_root);
    }

  /* if we get here, we must have a valid mount point */
  g_assert (volume_hal->mount_point != NULL);

  /* emit the "changed" signal */
  thunar_vfs_volume_changed (THUNAR_VFS_VOLUME (volume_hal));

  /* and release the policy again */
  libhal_storage_policy_free (policy);
}




static void                thunar_vfs_volume_manager_hal_class_init               (ThunarVfsVolumeManagerHalClass *klass);
static void                thunar_vfs_volume_manager_hal_init                     (ThunarVfsVolumeManagerHal      *manager_hal);
static void                thunar_vfs_volume_manager_hal_finalize                 (GObject                        *object);
static ThunarVfsVolumeHal *thunar_vfs_volume_manager_hal_get_volume_by_udi        (ThunarVfsVolumeManagerHal      *manager_hal,
                                                                                   const gchar                    *udi);
static void                thunar_vfs_volume_manager_hal_update_volume_by_udi     (ThunarVfsVolumeManagerHal      *manager_hal,
                                                                                   const gchar                    *udi);
static void                thunar_vfs_volume_manager_hal_device_added             (LibHalContext                  *context,
                                                                                   const gchar                    *udi);
static void                thunar_vfs_volume_manager_hal_device_removed           (LibHalContext                  *context,
                                                                                   const gchar                    *udi);
static void                thunar_vfs_volume_manager_hal_device_new_capability    (LibHalContext                  *context,
                                                                                   const gchar                    *udi,
                                                                                   const gchar                    *capability);
static void                thunar_vfs_volume_manager_hal_device_lost_capability   (LibHalContext                  *context,
                                                                                   const gchar                    *udi,
                                                                                   const gchar                    *capability);
static void                thunar_vfs_volume_manager_hal_device_property_modified (LibHalContext                  *context,
                                                                                   const gchar                    *udi,
                                                                                   const gchar                    *key,
                                                                                   dbus_bool_t                     is_removed,
                                                                                   dbus_bool_t                     is_added);



struct _ThunarVfsVolumeManagerHalClass
{
  ThunarVfsVolumeManagerClass __parent__;
};

struct _ThunarVfsVolumeManagerHal
{
  ThunarVfsVolumeManager __parent__;
  DBusConnection        *dbus_connection;
  LibHalContext         *context;
  GList                 *volumes;
};



static GObjectClass *thunar_vfs_volume_manager_hal_parent_class;



GType
thunar_vfs_volume_manager_hal_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      static const GTypeInfo info =
      {
        sizeof (ThunarVfsVolumeManagerHalClass),
        NULL,
        NULL,
        (GClassInitFunc) thunar_vfs_volume_manager_hal_class_init,
        NULL,
        NULL,
        sizeof (ThunarVfsVolumeManagerHal),
        0,
        (GInstanceInitFunc) thunar_vfs_volume_manager_hal_init,
        NULL,
      };

      type = g_type_register_static (THUNAR_VFS_TYPE_VOLUME_MANAGER, I_("ThunarVfsVolumeManagerHal"), &info, 0);
    }

  return type;
}



static void
thunar_vfs_volume_manager_hal_class_init (ThunarVfsVolumeManagerHalClass *klass)
{
  GObjectClass *gobject_class;

  /* determine the parent type class */
  thunar_vfs_volume_manager_hal_parent_class = g_type_class_peek_parent (klass);

  gobject_class = G_OBJECT_CLASS (klass);
  gobject_class->finalize = thunar_vfs_volume_manager_hal_finalize;
}



static void
thunar_vfs_volume_manager_hal_init (ThunarVfsVolumeManagerHal *manager_hal)
{
  LibHalDrive *hd;
  DBusError    error;
  gchar      **drive_udis;
  gchar      **udis;
  gint         n_drive_udis;
  gint         n_udis;
  gint         n, m;

  /* initialize the D-BUS error */
  dbus_error_init (&error);

  /* allocate a HAL context */
  manager_hal->context = libhal_ctx_new ();
  if (G_UNLIKELY (manager_hal->context == NULL))
    return;

  /* try to connect to the system bus */
  manager_hal->dbus_connection = dbus_bus_get (DBUS_BUS_SYSTEM, &error);
  if (G_UNLIKELY (manager_hal->dbus_connection == NULL))
    goto failed;

  /* setup the D-BUS connection for the HAL context */
  libhal_ctx_set_dbus_connection (manager_hal->context, manager_hal->dbus_connection);

  /* connect our manager object to the HAL context */
  libhal_ctx_set_user_data (manager_hal->context, manager_hal);

  /* setup callbacks */
  libhal_ctx_set_device_added (manager_hal->context, thunar_vfs_volume_manager_hal_device_added);
  libhal_ctx_set_device_removed (manager_hal->context, thunar_vfs_volume_manager_hal_device_removed);
  libhal_ctx_set_device_new_capability (manager_hal->context, thunar_vfs_volume_manager_hal_device_new_capability);
  libhal_ctx_set_device_lost_capability (manager_hal->context, thunar_vfs_volume_manager_hal_device_lost_capability);
  libhal_ctx_set_device_property_modified (manager_hal->context, thunar_vfs_volume_manager_hal_device_property_modified);

  /* try to initialize the HAL context */
  if (!libhal_ctx_init (manager_hal->context, &error))
    goto failed;

  /* setup the D-BUS connection with the GLib main loop */
  dbus_connection_setup_with_g_main (manager_hal->dbus_connection, NULL);

  /* lookup all drives currently known to HAL */
  drive_udis = libhal_find_device_by_capability (manager_hal->context, "storage", &n_drive_udis, &error);
  if (G_LIKELY (drive_udis != NULL))
    {
      /* process all drives UDIs */
      for (m = 0; m < n_drive_udis; ++m)
        {
          /* determine the LibHalDrive for the drive UDI */
          hd = libhal_drive_from_udi (manager_hal->context, drive_udis[m]);
          if (G_UNLIKELY (hd == NULL))
            continue;

          /* determine all volumes for the given drive */
          udis = libhal_drive_find_all_volumes (manager_hal->context, hd, &n_udis);
          if (G_LIKELY (udis != NULL))
            {
              /* add volumes for all given UDIs */
              for (n = 0; n < n_udis; ++n)
                {
                  /* add the volume based on the UDI */
                  thunar_vfs_volume_manager_hal_device_added (manager_hal->context, udis[n]);
                  
                  /* release the UDI (HAL bug #5279) */
                  free (udis[n]);
                }

              /* release the UDIs array (HAL bug #5279) */
              free (udis);
            }

          /* release the hal drive */
          libhal_drive_free (hd);
        }

      /* release the drive UDIs */
      libhal_free_string_array (drive_udis);
    }

  /* watch all devices for changes */
  if (!libhal_device_property_watch_all (manager_hal->context, &error))
    goto failed;

  return;

failed:
  /* release the HAL context */
  if (G_LIKELY (manager_hal->context != NULL))
    {
      libhal_ctx_free (manager_hal->context);
      manager_hal->context = NULL;
    }

  /* print a warning message */
  if (dbus_error_is_set (&error))
    {
      g_warning (_("Failed to connect to the HAL daemon: %s"), error.message);
      dbus_error_free (&error);
    }
}



static void
thunar_vfs_volume_manager_hal_finalize (GObject *object)
{
  ThunarVfsVolumeManagerHal *manager_hal = THUNAR_VFS_VOLUME_MANAGER_HAL (object);
  GList                     *lp;

  /* release all active volumes */
  for (lp = manager_hal->volumes; lp != NULL; lp = lp->next)
    g_object_unref (G_OBJECT (lp->data));
  g_list_free (manager_hal->volumes);

  /* shutdown the HAL context */
  if (G_LIKELY (manager_hal->context != NULL))
    {
      libhal_ctx_shutdown (manager_hal->context, NULL);
      libhal_ctx_free (manager_hal->context);
    }

  /* shutdown the D-BUS connection */
  if (G_LIKELY (manager_hal->dbus_connection != NULL))
    dbus_connection_unref (manager_hal->dbus_connection);

  (*G_OBJECT_CLASS (thunar_vfs_volume_manager_hal_parent_class)->finalize) (object);
}



static ThunarVfsVolumeHal*
thunar_vfs_volume_manager_hal_get_volume_by_udi (ThunarVfsVolumeManagerHal *manager_hal,
                                                 const gchar               *udi)
{
  GList *lp;

  for (lp = THUNAR_VFS_VOLUME_MANAGER (manager_hal)->volumes; lp != NULL; lp = lp->next)
    if (exo_str_is_equal (THUNAR_VFS_VOLUME_HAL (lp->data)->udi, udi))
      return THUNAR_VFS_VOLUME_HAL (lp->data);

  return NULL;
}



static void
thunar_vfs_volume_manager_hal_update_volume_by_udi (ThunarVfsVolumeManagerHal *manager_hal,
                                                    const gchar               *udi)
{
  ThunarVfsVolumeHal *volume_hal;
  LibHalVolume       *hv = NULL;
  LibHalDrive        *hd = NULL;
  const gchar        *drive_udi;

  /* check if we have a volume for the UDI */
  volume_hal = thunar_vfs_volume_manager_hal_get_volume_by_udi (manager_hal, udi);
  if (G_UNLIKELY (volume_hal == NULL))
    return;

  /* check if we have a volume here */
  hv = libhal_volume_from_udi (manager_hal->context, udi);
  if (G_UNLIKELY (hv == NULL))
    {
      /* the device is no longer a volume, so drop it */
      thunar_vfs_volume_manager_hal_device_removed (manager_hal->context, udi);
      return;
    }

  /* determine the UDI of the drive to which this volume belongs */
  drive_udi = libhal_volume_get_storage_device_udi (hv);
  if (G_LIKELY (drive_udi != NULL))
    {
      /* determine the drive for the volume */
      hd = libhal_drive_from_udi (manager_hal->context, drive_udi);
    }

  /* check if we have the drive for the volume */
  if (G_LIKELY (hd != NULL))
    {
      /* update the volume with the new HAL drive/volume */
      thunar_vfs_volume_hal_update (volume_hal, manager_hal->context, hv, hd);

      /* release the drive */
      libhal_drive_free (hd);
    }
  else
    {
      /* unable to determine the drive, volume gone? */
      thunar_vfs_volume_manager_hal_device_removed (manager_hal->context, udi);
    }

  /* release the volume */
  libhal_volume_free (hv);
}



static void
thunar_vfs_volume_manager_hal_device_added (LibHalContext *context,
                                            const gchar   *udi)
{
  ThunarVfsVolumeManagerHal *manager_hal = libhal_ctx_get_user_data (context);
  ThunarVfsVolumeHal        *volume_hal;
  LibHalVolume              *hv;
  LibHalDrive               *hd;
  const gchar               *drive_udi;

  g_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER_HAL (manager_hal));
  g_return_if_fail (manager_hal->context == context);

  /* check if we have a volume here */
  hv = libhal_volume_from_udi (context, udi);
  if (G_UNLIKELY (hv == NULL))
    return;

  /* we don't care for anything other than mountable filesystems */
  if (G_UNLIKELY (libhal_volume_get_fsusage (hv) != LIBHAL_VOLUME_USAGE_MOUNTABLE_FILESYSTEM))
    {
      libhal_volume_free (hv);
      return;
    }

  /* determine the UDI of the drive to which this volume belongs */
  drive_udi = libhal_volume_get_storage_device_udi (hv);
  if (G_LIKELY (drive_udi != NULL))
    {
      /* determine the drive for the volume */
      hd = libhal_drive_from_udi (context, drive_udi);
      if (G_LIKELY (hd != NULL))
        {
          /* check if we already have a volume object for the UDI */
          volume_hal = thunar_vfs_volume_manager_hal_get_volume_by_udi (manager_hal, udi);
          if (G_LIKELY (volume_hal == NULL))
            {
              /* otherwise, we allocate a new volume object */
              volume_hal = g_object_new (THUNAR_VFS_TYPE_VOLUME_HAL, NULL);
              volume_hal->udi = g_strdup (udi);
              volume_hal->drive_udi = g_strdup (drive_udi);
            }

          /* update the volume object with the new data from the HAL volume/drive */
          thunar_vfs_volume_hal_update (volume_hal, context, hv, hd);

          /* add the volume object to our list if we allocated a new one */
          if (g_list_find (THUNAR_VFS_VOLUME_MANAGER (manager_hal)->volumes, volume_hal) == NULL)
            {
              /* add the volume to the volume manager */
              thunar_vfs_volume_manager_add (THUNAR_VFS_VOLUME_MANAGER (manager_hal), THUNAR_VFS_VOLUME (volume_hal));

              /* release the reference on the volume */
              g_object_unref (G_OBJECT (volume_hal));
            }

          /* release the HAL drive */
          libhal_drive_free (hd);
        }
    }

  /* release the HAL volume */
  libhal_volume_free (hv);
}



static void
thunar_vfs_volume_manager_hal_device_removed (LibHalContext *context,
                                              const gchar   *udi)
{
  ThunarVfsVolumeManagerHal *manager_hal = libhal_ctx_get_user_data (context);
  ThunarVfsVolumeHal        *volume_hal;

  g_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER_HAL (manager_hal));
  g_return_if_fail (manager_hal->context == context);

  /* check if we already have a volume object for the UDI */
  volume_hal = thunar_vfs_volume_manager_hal_get_volume_by_udi (manager_hal, udi);
  if (G_LIKELY (volume_hal != NULL))
    {
      /* remove the volume from the volume manager */
      thunar_vfs_volume_manager_remove (THUNAR_VFS_VOLUME_MANAGER (manager_hal), THUNAR_VFS_VOLUME (volume_hal));
    }
}



static void
thunar_vfs_volume_manager_hal_device_new_capability (LibHalContext *context,
                                                     const gchar   *udi,
                                                     const gchar   *capability)
{
  ThunarVfsVolumeManagerHal *manager_hal = libhal_ctx_get_user_data (context);

  g_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER_HAL (manager_hal));
  g_return_if_fail (manager_hal->context == context);

  /* update the volume for the device (if any) */
  thunar_vfs_volume_manager_hal_update_volume_by_udi (manager_hal, udi);
}



static void
thunar_vfs_volume_manager_hal_device_lost_capability (LibHalContext *context,
                                                      const gchar   *udi,
                                                      const gchar   *capability)
{
  ThunarVfsVolumeManagerHal *manager_hal = libhal_ctx_get_user_data (context);

  g_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER_HAL (manager_hal));
  g_return_if_fail (manager_hal->context == context);

  /* update the volume for the device (if any) */
  thunar_vfs_volume_manager_hal_update_volume_by_udi (manager_hal, udi);
}



static void
thunar_vfs_volume_manager_hal_device_property_modified (LibHalContext *context,
                                                        const gchar   *udi,
                                                        const gchar   *key,
                                                        dbus_bool_t    is_removed,
                                                        dbus_bool_t    is_added)
{
  ThunarVfsVolumeManagerHal *manager_hal = libhal_ctx_get_user_data (context);

  g_return_if_fail (THUNAR_VFS_IS_VOLUME_MANAGER_HAL (manager_hal));
  g_return_if_fail (manager_hal->context == context);

  /* update the volume for the device (if any) */
  thunar_vfs_volume_manager_hal_update_volume_by_udi (manager_hal, udi);
}



#define __THUNAR_VFS_VOLUME_HAL_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
