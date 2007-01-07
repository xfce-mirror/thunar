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

#include <thunar-vfs/thunar-vfs-enum-types.h>
#include <thunar-vfs/thunar-vfs-private.h>
#include <thunar-vfs/thunar-vfs-volume-private.h>
#include <thunar-vfs/thunar-vfs-alias.h>



/* Signal identifiers */
enum
{
  CHANGED,
  MOUNTED,
  PRE_UNMOUNT,
  UNMOUNTED,
  LAST_SIGNAL,
};



static void thunar_vfs_volume_class_init (ThunarVfsVolumeClass *klass);



static guint volume_signals[LAST_SIGNAL];



GType
thunar_vfs_volume_get_type (void)
{
  static GType type = G_TYPE_INVALID;

  if (G_UNLIKELY (type == G_TYPE_INVALID))
    {
      type = _thunar_vfs_g_type_register_simple (G_TYPE_OBJECT,
                                                 "ThunarVfsVolume",
                                                 sizeof (ThunarVfsVolumeClass),
                                                 thunar_vfs_volume_class_init,
                                                 sizeof (ThunarVfsVolume),
                                                 NULL,
                                                 G_TYPE_FLAG_ABSTRACT);
    }

  return type;
}



static void
thunar_vfs_volume_class_init (ThunarVfsVolumeClass *klass)
{
  /**
   * ThunarVfsVolume::changed:
   * @volume : the #ThunarVfsVolume instance.
   *
   * Emitted whenever the state of @volume changed.
   **/
  volume_signals[CHANGED] =
    g_signal_new (I_("changed"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarVfsVolumeClass, changed),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ThunarVfsVolume::mounted:
   * @volume : the #ThunarVfsVolume instance.
   *
   * Emitted by @volume after a successfull mount
   * operation.
   **/
  volume_signals[MOUNTED] =
    g_signal_new (I_("mounted"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarVfsVolumeClass, mounted),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ThunarVfsVolume::pre-unmount:
   * @volume : the #ThunarVfsVolume instance.
   *
   * Emitted by @volume right before an attempt
   * is made to unmount the @volume.
   **/
  volume_signals[PRE_UNMOUNT] =
    g_signal_new (I_("pre-unmount"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarVfsVolumeClass, pre_unmount),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);

  /**
   * ThunarVfsVolume::unmounted:
   * @volume : the #ThunarVfsVolume instance.
   *
   * Emitted by @volume right after the @volume
   * was successfully unmounted.
   **/
  volume_signals[UNMOUNTED] =
    g_signal_new (I_("unmounted"),
                  G_TYPE_FROM_CLASS (klass),
                  G_SIGNAL_RUN_LAST,
                  G_STRUCT_OFFSET (ThunarVfsVolumeClass, unmounted),
                  NULL, NULL,
                  g_cclosure_marshal_VOID__VOID,
                  G_TYPE_NONE, 0);
}



/**
 * thunar_vfs_volume_get_kind:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Returns the kind of drive/device representd by @volume.
 *
 * Return value: the kind of @volume.
 **/
ThunarVfsVolumeKind
thunar_vfs_volume_get_kind (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), THUNAR_VFS_VOLUME_KIND_UNKNOWN);
  return (*THUNAR_VFS_VOLUME_GET_CLASS (volume)->get_kind) (volume);
}



/**
 * thunar_vfs_volume_get_name;
 * @volume : a #ThunarVfsVolume instance.
 *
 * Returns the name of the @volume. This is usually the
 * name of the device or the label of the medium, if a
 * medium is present.
 *
 * Return value: the name of @volume.
 **/
const gchar*
thunar_vfs_volume_get_name (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), NULL);
  return (*THUNAR_VFS_VOLUME_GET_CLASS (volume)->get_name) (volume);
}



/**
 * thunar_vfs_volume_get_status:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Determines the current status of the @volume, e.g. whether
 * or not the @volume is currently mounted, or whether a
 * medium is present.
 *
 * Return value: the status for @volume.
 **/
ThunarVfsVolumeStatus
thunar_vfs_volume_get_status (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), 0);
  return (*THUNAR_VFS_VOLUME_GET_CLASS (volume)->get_status) (volume);
}



/**
 * thunar_vfs_volume_get_mount_point:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Determines the current mount point for @volume. If @volume
 * is mounted this will be the location at which it is currently
 * mounted. Else it will be the location where @volume is most
 * probably being mounted. Note that this location may change
 * during a call to thunar_vfs_volume_mount(), so be sure to
 * check the mount point after the call to thunar_vfs_volume_mount().
 *
 * Return value: the path which identifies the path where
 *               the volume will be mounted (or is already
 *               mounted).
 **/
ThunarVfsPath*
thunar_vfs_volume_get_mount_point (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), NULL);
  return (*THUNAR_VFS_VOLUME_GET_CLASS (volume)->get_mount_point) (volume);
}



/**
 * thunar_vfs_volume_is_disc:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Determines whether @volume is a disc, which can be ejected.
 * Applications should use this method to determine whether to
 * thunar_vfs_volume_eject() or thunar_vfs_volume_unmount().
 *
 * Return value: %TRUE if @volume is a disc.
 **/
gboolean
thunar_vfs_volume_is_disc (ThunarVfsVolume *volume)
{
  ThunarVfsVolumeKind kind;

  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), FALSE);

  kind = thunar_vfs_volume_get_kind (volume);

  return (kind >= THUNAR_VFS_VOLUME_KIND_CDROM && kind <= THUNAR_VFS_VOLUME_KIND_DVDPLUSRW)
      || (kind == THUNAR_VFS_VOLUME_KIND_AUDIO_CD);
}



/**
 * thunar_vfs_volume_is_mounted:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Determines whether @volume is currently mounted into the
 * filesystem hierarchy.
 *
 * Return value: %TRUE if @volume is mounted, else %FALSE.
 **/
gboolean
thunar_vfs_volume_is_mounted (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), FALSE);
  return (*THUNAR_VFS_VOLUME_GET_CLASS (volume)->get_status) (volume) & THUNAR_VFS_VOLUME_STATUS_MOUNTED;
}



/**
 * thunar_vfs_volume_is_present:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Determines whether a medium is currently inserted for
 * @volume, e.g. for a CD-ROM drive, this will be %TRUE
 * only if a disc is present in the slot.
 *
 * Return value: %TRUE if @volume is present, else %FALSE.
 **/
gboolean
thunar_vfs_volume_is_present (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), FALSE);
  return (*THUNAR_VFS_VOLUME_GET_CLASS (volume)->get_status) (volume) & THUNAR_VFS_VOLUME_STATUS_PRESENT;
}



/**
 * thunar_vfs_volume_is_ejectable:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Determines whether the current user is allowed to eject the medium
 * for @volume. This method should return %TRUE only if a medium is
 * present and the @volume is removable. Still, there's no warranty
 * that a call to thunar_vfs_volume_eject() will succeed.
 *
 * Return value: whether the medium for @volume can be ejected.
 **/
gboolean
thunar_vfs_volume_is_ejectable (ThunarVfsVolume *volume)
{
  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), FALSE);

  /* we can only eject removable media, that are present (surprise, surprise) */
  return (thunar_vfs_volume_is_present (volume) && thunar_vfs_volume_is_removable (volume));
}



/**
 * thunar_vfs_volume_is_removable:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Determines whether @volume is a removable device, for example
 * a CD-ROM, an USB stick or a floppy drive.
 *
 * Return value: %TRUE if @volume is a removable device, else %FALSE.
 **/
gboolean
thunar_vfs_volume_is_removable (ThunarVfsVolume *volume)
{
  ThunarVfsVolumeKind kind;

  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), FALSE);

  kind = thunar_vfs_volume_get_kind (volume);

  switch (kind)
    {
    case THUNAR_VFS_VOLUME_KIND_CDROM:
    case THUNAR_VFS_VOLUME_KIND_CDR:
    case THUNAR_VFS_VOLUME_KIND_CDRW:
    case THUNAR_VFS_VOLUME_KIND_DVDROM:
    case THUNAR_VFS_VOLUME_KIND_DVDRAM:
    case THUNAR_VFS_VOLUME_KIND_DVDR:
    case THUNAR_VFS_VOLUME_KIND_DVDRW:
    case THUNAR_VFS_VOLUME_KIND_DVDPLUSR:
    case THUNAR_VFS_VOLUME_KIND_DVDPLUSRW:
    case THUNAR_VFS_VOLUME_KIND_FLOPPY:
    case THUNAR_VFS_VOLUME_KIND_USBSTICK:
    case THUNAR_VFS_VOLUME_KIND_AUDIO_PLAYER:
    case THUNAR_VFS_VOLUME_KIND_AUDIO_CD:
      return TRUE;

    default:
      return FALSE;
    }
}



/**
 * thunar_vfs_volume_lookup_icon_name:
 * @volume     : a #ThunarVfsVolume instance.
 * @icon_theme : a #GtkIconTheme instance.
 *
 * Tries to find a suitable icon for @volume in the given @icon_theme and
 * returns its name. If no suitable icon is found in @icon_theme, then
 * a fallback icon name will be returned. This way you can always count
 * on this method to return a valid string.
 *
 * Return value: the icon name.
 **/
const gchar*
thunar_vfs_volume_lookup_icon_name (ThunarVfsVolume *volume,
                                    GtkIconTheme    *icon_theme)
{
  ThunarVfsVolumeClass *klass;
  ThunarVfsVolumeKind   kind;
  const gchar          *icon_name;

  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), NULL);
  g_return_val_if_fail (GTK_IS_ICON_THEME (icon_theme), NULL);

  /* allow the implementing class to provide a custom icon */
  klass = THUNAR_VFS_VOLUME_GET_CLASS (volume);
  if (klass->lookup_icon_name != NULL)
    {
      icon_name = (*klass->lookup_icon_name) (volume, icon_theme);
      if (G_LIKELY (icon_name != NULL))
        return icon_name;
    }

  kind = thunar_vfs_volume_get_kind (volume);
  switch (kind)
    {
cdrom:
    case THUNAR_VFS_VOLUME_KIND_CDROM:
      if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-cdrom"))
        return "gnome-dev-cdrom";
      break;

    case THUNAR_VFS_VOLUME_KIND_CDR:
      if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-disc-cdr"))
        return "gnome-dev-disc-cdr";
      goto cdrom;

    case THUNAR_VFS_VOLUME_KIND_CDRW:
      if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-disc-cdrw"))
        return "gnome-dev-disc-cdrw";
      goto cdrom;

dvdrom:
    case THUNAR_VFS_VOLUME_KIND_DVDROM:
      if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-disc-dvdrom"))
        return "gnome-dev-disc-dvdrom";
      else if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-dvd"))
        return "gnome-dev-dvd";
      goto cdrom;

    case THUNAR_VFS_VOLUME_KIND_DVDRAM:
      if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-disc-dvdram"))
        return "gnome-dev-disc-dvdram";
      goto dvdrom;

dvdr:
    case THUNAR_VFS_VOLUME_KIND_DVDR:
      if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-disc-dvdr"))
        return "gnome-dev-disc-dvdr";
      goto dvdrom;

    case THUNAR_VFS_VOLUME_KIND_DVDRW:
    case THUNAR_VFS_VOLUME_KIND_DVDPLUSRW:
      if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-disc-dvdrw"))
        return "gnome-dev-disc-dvdrw";
      goto dvdrom;

    case THUNAR_VFS_VOLUME_KIND_DVDPLUSR:
      if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-disc-dvdr-plus"))
        return "gnome-dev-disc-dvdr-plus";
      goto dvdr;

    case THUNAR_VFS_VOLUME_KIND_FLOPPY:
      if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-floppy"))
        return "gnome-dev-floppy";
      break;

    case THUNAR_VFS_VOLUME_KIND_HARDDISK:
      if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-harddisk"))
        return "gnome-dev-harddisk";
      break;

    case THUNAR_VFS_VOLUME_KIND_USBSTICK:
      if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-removable-usb"))
        return "gnome-dev-removable-usb";
      else if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-harddisk-usb"))
        return "gnome-dev-harddisk-usb";
      break;

    case THUNAR_VFS_VOLUME_KIND_AUDIO_PLAYER:
      if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-ipod"))
        return "gnome-dev-ipod";
      break;

    case THUNAR_VFS_VOLUME_KIND_AUDIO_CD:
      if (gtk_icon_theme_has_icon (icon_theme, "gnome-dev-cdrom-audio"))
        return "gnome-dev-cdrom-audio";
      goto cdrom;

    default:
      break;
    }

  return "gnome-fs-blockdev";
}



/**
 * thunar_vfs_volume_eject:
 * @volume : a #ThunarVfsVolume instance.
 * @window : a #GtkWindow or %NULL.
 * @error  : return location for errors or %NULL.
 *
 * Tries to eject the medium present for @volume (or atleast to
 * unmount the @volume).
 *
 * If ejecting @volume requires some complex user interaction
 * (basicly everything else than displaying an error dialog), it
 * should popup a modal dialog on @window (or on the default
 * #GdkScreen if @window is %NULL). But be aware, that if an
 * implementation of #ThunarVfsVolume performs user interaction
 * during a call to this method, it must implement this method
 * in a reentrant fashion!
 *
 * Return value: %TRUE if the medium for @volume was successfully
 *               ejected (or atleast the @volume was unmounted
 *               successfully), else %FALSE.
 **/
gboolean
thunar_vfs_volume_eject (ThunarVfsVolume *volume,
                         GtkWidget       *window,
                         GError         **error)
{
  GdkCursor *cursor;
  gboolean   result;

  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (window == NULL || GTK_IS_WINDOW (window), FALSE);

  /* we're about to unmount (and eject) the volume */
  g_signal_emit (G_OBJECT (volume), volume_signals[PRE_UNMOUNT], 0);

  /* setup a watch cursor on the window */
  if (window != NULL && GTK_WIDGET_REALIZED (window))
    {
      /* setup the watch cursor */
      cursor = gdk_cursor_new (GDK_WATCH);
      gdk_window_set_cursor (window->window, cursor);
      gdk_cursor_unref (cursor);

      /* flush the changes */
      gdk_flush ();
    }

  /* try to mount the volume */
  result = (*THUNAR_VFS_VOLUME_GET_CLASS (volume)->eject) (volume, window, error);

  /* reset the cursor */
  if (window != NULL && GTK_WIDGET_REALIZED (window))
    gdk_window_set_cursor (window->window, NULL);

  /* check if we unmounted successfully */
  if (G_LIKELY (result))
    g_signal_emit (G_OBJECT (volume), volume_signals[UNMOUNTED], 0);

  return result;
}



/**
 * thunar_vfs_volume_mount:
 * @volume : a #ThunarVfsVolume instance.
 * @window : a #GtkWindow or %NULL.
 * @error  : return location for errors or %NULL.
 *
 * Tries to mount @volume. Will return %TRUE if either @volume
 * was already mounted previously to this method invokation or
 * @volume was successfully mounted now.
 *
 * If mounting @volume requires some complex user interaction
 * (basicly everything else than displaying an error dialog), it
 * should popup a modal dialog on @window (or on the default
 * #GdkScreen if @window is %NULL). But be aware, that if an
 * implementation of #ThunarVfsVolume performs user interaction
 * during a call to this method, it must implement this method
 * in a reentrant fashion!
 *
 * Return value: %TRUE if the medium for @volume was successfully
 *               mounted or was already mounted previously, else
 *               %FALSE.
 **/
gboolean
thunar_vfs_volume_mount (ThunarVfsVolume *volume,
                         GtkWidget       *window,
                         GError         **error)
{
  GdkCursor *cursor;
  gboolean   result;

  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (window == NULL || GTK_IS_WINDOW (window), FALSE);

  /* setup a watch cursor on the window */
  if (window != NULL && GTK_WIDGET_REALIZED (window))
    {
      /* setup the watch cursor */
      cursor = gdk_cursor_new (GDK_WATCH);
      gdk_window_set_cursor (window->window, cursor);
      gdk_cursor_unref (cursor);

      /* flush the changes */
      gdk_flush ();
    }

  /* try to mount the volume */
  result = (*THUNAR_VFS_VOLUME_GET_CLASS (volume)->mount) (volume, window, error);

  /* reset the cursor */
  if (window != NULL && GTK_WIDGET_REALIZED (window))
    gdk_window_set_cursor (window->window, NULL);

  /* check if we successfully mounted the volume */
  if (G_LIKELY (result))
    g_signal_emit (G_OBJECT (volume), volume_signals[MOUNTED], 0);

  return result;
}



/**
 * thunar_vfs_volume_unmount:
 * @volume : a #ThunarVfsVolume instance.
 * @window : a #GtkWindow or %NULL.
 * @error  : return location for errors or %NULL.
 *
 * Tries to unmount @volume. Will return %TRUE if either @volume
 * was already unmounted previously to this method invokation or
 * @volume was successfully unmounted now.
 *
 * If unmounting @volume requires some complex user interaction
 * (basicly everything else than displaying an error dialog), it
 * should popup a modal dialog on @window (or on the default
 * #GdkScreen if @window is %NULL). But be aware, that if an
 * implementation of #ThunarVfsVolume performs user interaction
 * during a call to this method, it must implement this method
 * in a reentrant fashion!
 *
 * Return value: %TRUE if the medium for @volume was successfully
 *               unmounted or wasn't mounted previously, else
 *               %FALSE.
 **/
gboolean
thunar_vfs_volume_unmount (ThunarVfsVolume *volume,
                           GtkWidget       *window,
                           GError         **error)
{
  GdkCursor *cursor;
  gboolean   result;

  g_return_val_if_fail (THUNAR_VFS_IS_VOLUME (volume), FALSE);
  g_return_val_if_fail (error == NULL || *error == NULL, FALSE);
  g_return_val_if_fail (window == NULL || GTK_IS_WINDOW (window), FALSE);

  /* we're about to unmount the volume */
  g_signal_emit (G_OBJECT (volume), volume_signals[PRE_UNMOUNT], 0);

  /* setup a watch cursor on the window */
  if (window != NULL && GTK_WIDGET_REALIZED (window))
    {
      /* setup the watch cursor */
      cursor = gdk_cursor_new (GDK_WATCH);
      gdk_window_set_cursor (window->window, cursor);
      gdk_cursor_unref (cursor);

      /* flush the changes */
      gdk_flush ();
    }

  /* try to mount the volume */
  result = (*THUNAR_VFS_VOLUME_GET_CLASS (volume)->unmount) (volume, window, error);

  /* reset the cursor */
  if (window != NULL && GTK_WIDGET_REALIZED (window))
    gdk_window_set_cursor (window->window, NULL);

  /* check if we unmounted successfully */
  if (G_LIKELY (result))
    g_signal_emit (G_OBJECT (volume), volume_signals[UNMOUNTED], 0);

  return result;
}



/**
 * thunar_vfs_volume_changed:
 * @volume : a #ThunarVfsVolume instance.
 *
 * Emits the "changed" signal on @volume. This function should
 * only be used by implementations of the #ThunarVfsVolume
 * interface.
 **/
void
thunar_vfs_volume_changed (ThunarVfsVolume *volume)
{
  g_return_if_fail (THUNAR_VFS_IS_VOLUME (volume));
  g_signal_emit (G_OBJECT (volume), volume_signals[CHANGED], 0);
}




#define __THUNAR_VFS_VOLUME_C__
#include <thunar-vfs/thunar-vfs-aliasdef.c>
