/* vi:set et ai sw=2 sts=2 ts=2: */
/*-
 * Copyright (c) 2022 Pratyaksh Gautam <pratyakshgautam11@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program; if not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __THUNAR_JOB_OPERATION_H__
#define __THUNAR_JOB_OPERATION_H__

#include <gio/gio.h>
#include <thunar/thunar-enum-types.h>

G_BEGIN_DECLS

#define THUNAR_TYPE_JOB_OPERATION (thunar_job_operation_get_type ())
G_DECLARE_FINAL_TYPE (ThunarJobOperation, thunar_job_operation, THUNAR, JOB_OPERATION, GObject)

G_END_DECLS

ThunarJobOperation    *thunar_job_operation_new          (ThunarJobOperationKind kind);
void                   thunar_job_operation_add          (ThunarJobOperation    *job_operation,
                                                          GFile                 *source_file,
                                                          GFile                 *target_file);
void                   thunar_job_operation_commit       (ThunarJobOperation    *job_operation);
void                   thunar_job_operation_undo         (void);

#endif /* __THUNAR_JOB_OPERATION_H__ */
