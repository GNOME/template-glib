/* tmpl-scope.c
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

#include "tmpl-scope.h"
#include "tmpl-symbol.h"

struct _TmplScope
{
  volatile gint  ref_count;
  TmplScope     *parent;
  GHashTable    *symbols;
};

G_DEFINE_BOXED_TYPE (TmplScope, tmpl_scope, tmpl_scope_ref, tmpl_scope_unref)

TmplScope *
tmpl_scope_ref (TmplScope *self)
{
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (self->ref_count > 0, NULL);

  g_atomic_int_inc (&self->ref_count);

  return self;
}

void
tmpl_scope_unref (TmplScope *self)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (self->ref_count > 0);

  if (g_atomic_int_dec_and_test (&self->ref_count))
    {
      g_clear_pointer (&self->symbols, g_hash_table_unref);
      g_clear_pointer (&self->parent, tmpl_scope_unref);
      g_slice_free (TmplScope, self);
    }
}

/**
 * tmpl_scope_new:
 *
 * Creates a new scope to contain variables and custom expressions,
 *
 * Returns: (transfer full): A newly created #TmplScope.
 */
TmplScope *
tmpl_scope_new (void)
{
  TmplScope *self;

  self = g_slice_new0 (TmplScope);
  self->ref_count = 1;
  self->parent = NULL;

  return self;
}

/**
 * tmpl_scope_new_with_parent:
 * @parent: (nullable): An optional parent scope
 *
 * Creates a new scope to contain variables and custom expressions,
 * If @parent is set, the parent scope will be inherited.
 *
 * Returns: (transfer full): A newly created #TmplScope.
 */
TmplScope *
tmpl_scope_new_with_parent (TmplScope *parent)
{
  TmplScope *self;

  self = g_slice_new0 (TmplScope);
  self->ref_count = 1;
  self->parent = parent != NULL ? tmpl_scope_ref (parent) : NULL;

  return self;
}

static TmplSymbol *
tmpl_scope_get_full (TmplScope   *self,
                     const gchar *name,
                     gboolean     create)
{
  TmplSymbol *symbol = NULL;

  g_return_val_if_fail (self != NULL, NULL);

  /* See if this scope has the symbol */
  if (self->symbols != NULL)
    {
      if ((symbol = g_hash_table_lookup (self->symbols, name)))
        return symbol;
    }

  /* Try to locate the symbol in a parent scope */
  if (symbol == NULL)
    {
      TmplScope *parent;

      for (parent = self->parent; parent != NULL; parent = parent->parent)
        {
          if (parent->symbols != NULL)
            {
              if ((symbol = g_hash_table_lookup (parent->symbols, name)))
                return symbol;
            }
        }
    }

  if (create)
    {
      /* Define the symbol in this scope */
      symbol = tmpl_symbol_new ();
      if (self->symbols == NULL)
        self->symbols = g_hash_table_new_full (g_str_hash,
                                               g_str_equal,
                                               g_free,
                                               (GDestroyNotify)tmpl_symbol_unref);
      g_hash_table_insert (self->symbols, g_strdup (name), symbol);
    }

  return symbol;
}

/**
 * tmpl_scope_get:
 *
 * If the symbol could not be found, it will be allocated.
 *
 * Returns: (transfer none): A #TmplSymbol.
 */
TmplSymbol *
tmpl_scope_get (TmplScope   *self,
                const gchar *name)
{
  return tmpl_scope_get_full (self, name, TRUE);
}

/**
 * tmpl_scope_peek:
 *
 * If the symbol could not be found, %NULL is returned.
 *
 * Returns: (transfer none) (nullable): A #TmplSymbol or %NULL.
 */
TmplSymbol *
tmpl_scope_peek (TmplScope   *self,
                 const gchar *name)
{
  return tmpl_scope_get_full (self, name, FALSE);
}
