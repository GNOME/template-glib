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

#include "config.h"

#include "tmpl-gi-private.h"
#include "tmpl-scope.h"
#include "tmpl-symbol.h"
#include "tmpl-util-private.h"

struct _TmplScope
{
  volatile gint      ref_count;
  TmplScope         *parent;
  GHashTable        *symbols;
  TmplScopeResolver  resolver;
  gpointer           resolver_data;
  GDestroyNotify     resolver_destroy;
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
      if (self->resolver_destroy)
        g_clear_pointer (&self->resolver_data, self->resolver_destroy);
      self->resolver = NULL;
      self->resolver_destroy = NULL;
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
  TmplScope *parent;

  g_return_val_if_fail (self != NULL, NULL);

  /* See if this scope has the symbol */
  if (self->symbols != NULL)
    {
      if ((symbol = g_hash_table_lookup (self->symbols, name)))
        return symbol;
    }

  /* Try to locate the symbol in a parent scope */
  for (parent = self->parent; parent != NULL; parent = parent->parent)
    {
      if (parent->symbols != NULL)
        {
          if ((symbol = g_hash_table_lookup (parent->symbols, name)))
            return symbol;
        }
    }

  /* Call our resolver helper to locate the symbol */
  for (parent = self; parent != NULL; parent = parent->parent)
    {
      if (parent->resolver != NULL)
        {
          if (parent->resolver (parent, name, &symbol, parent->resolver_data) && symbol)
            {
              /* Pass ownership to our scope, and return a weak ref */
              tmpl_scope_take (self, name, symbol);
              return symbol;
            }
        }
    }

  if (create)
    {
      /* Define the symbol in this scope */
      symbol = tmpl_symbol_new ();
      tmpl_scope_take (self, name, symbol);
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
 * tmpl_scope_take:
 * @self: A #TmplScope
 * @name: The name of the symbol
 * @symbol: (nullable) (transfer full): A #TmplSymbol or %NULL
 *
 * Sets the symbol named @name to @symbol in @scope.
 *
 * This differs from tmpl_scope_set() in that it takes ownership
 * of @symbol.
 */
void
tmpl_scope_take (TmplScope   *self,
                 const gchar *name,
                 TmplSymbol  *symbol)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (name != NULL);

  if G_UNLIKELY (symbol == NULL)
    {
      if G_LIKELY (self->symbols != NULL)
        g_hash_table_remove (self->symbols, name);
      return;
    }

  if (self->symbols == NULL)
    self->symbols = g_hash_table_new_full (g_str_hash,
                                           g_str_equal,
                                           g_free,
                                           (GDestroyNotify) tmpl_symbol_unref);

  g_hash_table_insert (self->symbols, g_strdup (name), symbol);
}

/**
 * tmpl_scope_set:
 * @self: A #TmplScope
 * @name: the name of the symbol
 * @symbol: (nullable) (transfer none): An #TmplSymbol or %NULL.
 *
 * If the symbol already exists, it will be overwritten.
 *
 * If @symbol is %NULL, the symbol will be removed from scope.
 */
void
tmpl_scope_set (TmplScope   *self,
                const gchar *name,
                TmplSymbol  *symbol)
{
  g_return_if_fail (self != NULL);

  if (symbol != NULL)
    tmpl_symbol_ref (symbol);

  tmpl_scope_take (self, name, symbol);
}

/**
 * tmpl_scope_set_value:
 * @self: A #TmplScope
 * @name: a name for the symbol
 * @value: (nullable): A #GValue or %NULL
 *
 * Sets the contents of the symbol named @name to the value @value.
 */
void
tmpl_scope_set_value (TmplScope    *self,
                      const gchar  *name,
                      const GValue *value)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (name != NULL);

  tmpl_symbol_assign_value (tmpl_scope_get_full (self, name, TRUE), value);
}

/**
 * tmpl_scope_set_boolean:
 * @self: A #TmplScope
 * @name: a name for the symbol
 * @value: a #gboolean
 *
 * Sets the value of the symbol named @name to a gboolean value of @value.
 */
void
tmpl_scope_set_boolean (TmplScope   *self,
                        const gchar *name,
                        gboolean     value)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (name != NULL);

  tmpl_symbol_assign_boolean (tmpl_scope_get_full (self, name, TRUE), value);
}

/**
 * tmpl_scope_set_double:
 * @self: A #TmplScope
 * @name: a name for the symbol
 * @value: a #gdouble
 *
 * Sets the value of the symbol named @name to a gdouble value of @value.
 */
void
tmpl_scope_set_double (TmplScope   *self,
                       const gchar *name,
                       gdouble      value)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (name != NULL);

  tmpl_symbol_assign_double (tmpl_scope_get_full (self, name, TRUE), value);
}

/**
 * tmpl_scope_set_object:
 * @self: A #TmplScope
 * @name: a name for the symbol
 * @value: (type GObject.Object) (nullable): a #GObject or %NULL.
 *
 * Sets the value of the symbol named @name to the object @value.
 */
void
tmpl_scope_set_object (TmplScope   *self,
                       const gchar *name,
                       gpointer     value)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (name != NULL);
  g_return_if_fail (!value || G_IS_OBJECT (value));

  tmpl_symbol_assign_object (tmpl_scope_get_full (self, name, TRUE), value);
}

/**
 * tmpl_scope_set_variant:
 * @self: A #TmplScope
 * @name: a name for the symbol
 * @value: (nullable): the variant to set it to, or %NULL
 *
 * Sets the value of the symbol named @name to the variant @value.
 *
 * If @value has a floating reference, it is consumed.
 */
void
tmpl_scope_set_variant (TmplScope   *self,
                        const gchar *name,
                        GVariant    *value)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (name != NULL);

  tmpl_symbol_assign_variant (tmpl_scope_get_full (self, name, TRUE),
                              value);
}

/**
 * tmpl_scope_set_strv:
 * @self: A #TmplScope
 * @name: a name for the symbol
 * @value: (nullable) (array zero-terminated=1): the value to set it to, or %NULL
 *
 * Sets the value of the symbol named @name to the strv @value.
 */
void
tmpl_scope_set_strv (TmplScope   *self,
                     const gchar *name,
                     const gchar **value)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (name != NULL);

  tmpl_symbol_assign_variant (tmpl_scope_get_full (self, name, TRUE),
                              g_variant_new_strv (value, -1));
}

/**
 * tmpl_scope_set_string:
 * @self: A #TmplScope
 * @name: a name for the symbol
 * @value: (nullable): A string or %NULL.
 *
 * Sets the value of the symbol named @name to a string matching @value.
 */
void
tmpl_scope_set_string (TmplScope   *self,
                       const gchar *name,
                       const gchar *value)
{
  g_return_if_fail (self != NULL);
  g_return_if_fail (name != NULL);

  tmpl_symbol_assign_string (tmpl_scope_get_full (self, name, TRUE), value);
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
  g_return_val_if_fail (self != NULL, NULL);
  g_return_val_if_fail (name != NULL, NULL);

  return tmpl_scope_get_full (self, name, FALSE);
}

void
tmpl_scope_set_resolver (TmplScope         *self,
                         TmplScopeResolver  resolver,
                         gpointer           user_data,
                         GDestroyNotify     destroy)
{
  g_return_if_fail (self != NULL);

  if (resolver != self->resolver ||
      user_data != self->resolver_data ||
      destroy != self->resolver_destroy)
    {
      if (self->resolver && self->resolver_destroy && self->resolver_data)
        {
          g_clear_pointer (&self->resolver_data, self->resolver_destroy);
          self->resolver_destroy = NULL;
          self->resolver = NULL;
        }

      self->resolver = resolver;
      self->resolver_data = user_data;
      self->resolver_destroy = destroy;
    }
}

/**
 * tmpl_scope_require:
 * @self: a #TmplScope
 * @namespace_: the namespace to import into the scope
 * @version: (nullable): the version of @namespace_ to import
 *
 * Imports @namespace_ into @self so it can be used by expressions.
 *
 * Returns: %TRUE if successful; otherwise %FALSE
 */
gboolean
tmpl_scope_require (TmplScope    *self,
                    const char   *namespace_,
                    const char   *version)
{
  GITypelib *typelib;
  GValue value = G_VALUE_INIT;

  g_return_val_if_fail (self != NULL, FALSE);
  g_return_val_if_fail (namespace_ != NULL, FALSE);

  if (!(typelib = gi_repository_require (tmpl_repository_get_default (), namespace_, version, 0, NULL)))
    return FALSE;

  g_value_init (&value, TMPL_TYPE_TYPELIB);
  g_value_set_pointer (&value, typelib);
  tmpl_scope_set_value (self, namespace_, &value);
  g_value_unset (&value);

  return TRUE;
}

static void
tmpl_scope_list_symbols_internal (TmplScope *self,
                                  GPtrArray *ar,
                                  gboolean   recursive)
{
  GHashTableIter iter;
  const char *key;

  g_assert (self != NULL);
  g_assert (ar != NULL);

  g_hash_table_iter_init (&iter, self->symbols);
  while (g_hash_table_iter_next (&iter, (gpointer *)&key, NULL))
    g_ptr_array_add (ar, g_strdup (key));

  if (recursive && self->parent)
    tmpl_scope_list_symbols_internal (self->parent, ar, recursive);
}

/**
 * tmpl_scope_list_symbols:
 * @self: a #TmplScope
 * @recursive: if the parent scopes should be included
 *
 * Gets the names of all symbols within the scope.
 *
 * Returns: (array zero-terminated=1) (element-type utf8) (transfer full):
 *   an array containing the names of all symbols within the scope.
 */
char **
tmpl_scope_list_symbols (TmplScope *self,
                         gboolean   recursive)
{
  GPtrArray *ar;

  g_return_val_if_fail (self != NULL, NULL);

  ar = g_ptr_array_new ();
  tmpl_scope_list_symbols_internal (self, ar, recursive);
  g_ptr_array_add (ar, NULL);

  return (char **)g_ptr_array_free (ar, FALSE);
}

void
tmpl_scope_set_null (TmplScope  *self,
                     const char *name)
{
  GValue value = G_VALUE_INIT;

  g_value_init (&value, G_TYPE_POINTER);
  g_value_set_pointer (&value, NULL);

  tmpl_scope_set_value (self, name, &value);
}

/**
 * tmpl_scope_dup_string:
 * @self: a #TmplScope
 *
 * Gets a string if the symbol @name is a string.
 *
 * Otherwise, %NULL is returned.
 *
 * Returns: (transfer full) (nullable): a string or %NULL
 *
 * Since: 3.36
 */
char *
tmpl_scope_dup_string (TmplScope  *self,
                       const char *name)
{
  GValue value = G_VALUE_INIT;
  TmplSymbol *symbol;
  char *ret = NULL;

  if (!(symbol = tmpl_scope_peek (self, name)))
    return NULL;

  tmpl_symbol_get_value (symbol, &value);
  if (G_VALUE_HOLDS_STRING (&value))
    ret = g_value_dup_string (&value);
  g_value_unset (&value);

  return ret;
}
