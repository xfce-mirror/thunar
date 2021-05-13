/*
 * Copyright (c) 2021 Chigozirim Chukwu <noblechuk5[at]web[dot]de>
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

#ifndef __THUNAR_UCA_PARSER_H__
#define __THUNAR_UCA_PARSER_H__

#include <thunar-uca/thunar-uca-model.h>
#include <glib.h>

G_BEGIN_DECLS

typedef struct _ThunarUcaParser ThunarUcaParser;

gboolean thunar_uca_parser_read_uca_from_file(ThunarUcaModel	*model,
											  const gchar		*filename,
											  GError		   **error);

G_END_DECLS

#endif //__THUNAR_UCA_PARSER_H__
