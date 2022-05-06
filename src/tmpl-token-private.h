/* tmpl-token-private.h
 *
 * Copyright (C) 2016 Christian Hergert <chergert@redhat.com>
 *
 * This file is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This file is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef TMPL_TOKEN_PRIVATE_H
#define TMPL_TOKEN_PRIVATE_H

#include "tmpl-token.h"

G_BEGIN_DECLS

struct _TmplToken
{
  TmplTokenType type;
  gchar *text;
};

G_END_DECLS

#endif /* TMPL_TOKEN_PRIVATE_H */
