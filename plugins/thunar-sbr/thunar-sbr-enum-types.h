/* $Id$ */
/*-
 * Copyright (c) 2006 Benedikt Meurer <benny@xfce.org>
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

#ifndef __THUNAR_SBR_ENUM_TYPES_H__
#define __THUNAR_SBR_ENUM_TYPES_H__

#include <thunarx/thunarx.h>

G_BEGIN_DECLS;

#define THUNAR_SBR_TYPE_CASE_RENAMER_MODE (thunar_sbr_case_renamer_mode_get_type ())

/**
 * ThunarSbrCaseRenamerMode:
 * @THUNAR_SBR_CASE_RENAMER_MODE_LOWER : convert to lower case.
 * @THUNAR_SBR_CASE_RENAMER_MODE_UPPER : convert to upper case.
 * @THUNAR_SBR_CASE_RENAMER_MODE_CAMEL : convert to camel case.
 *
 * The oepration mode for the #ThunarSbrCaseRenamer.
 **/
typedef enum
{
  THUNAR_SBR_CASE_RENAMER_MODE_LOWER,
  THUNAR_SBR_CASE_RENAMER_MODE_UPPER,
  THUNAR_SBR_CASE_RENAMER_MODE_CAMEL,
} ThunarSbrCaseRenamerMode;

GType thunar_sbr_case_renamer_mode_get_type (void) G_GNUC_CONST G_GNUC_INTERNAL;


void thunar_sbr_register_enum_types (ThunarxProviderPlugin *plugin) G_GNUC_INTERNAL;

G_END_DECLS;

#endif /* !__THUNAR_SBR_ENUM_TYPES_H__ */
