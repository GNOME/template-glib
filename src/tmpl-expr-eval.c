/* tmpl-expr-eval.c
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

#include <math.h>
#include <string.h>

#include <girepository/girepository.h>

#include "tmpl-error.h"
#include "tmpl-expr.h"
#include "tmpl-expr-private.h"
#include "tmpl-gi-private.h"
#include "tmpl-scope.h"
#include "tmpl-symbol.h"
#include "tmpl-util-private.h"

#define DECLARE_BUILTIN(name) \
  static gboolean builtin_##name (const GValue *, GValue *, GError **);

typedef gboolean (*BuiltinFunc)  (const GValue  *value,
                                  GValue        *return_value,
                                  GError       **error);
typedef gboolean (*FastDispatch) (const GValue  *left,
                                  const GValue  *right,
                                  GValue        *return_value,
                                  GError       **error);

static gboolean tmpl_expr_eval_internal  (TmplExpr  *node,
                                              TmplScope      *scope,
                                              GValue        *return_value,
                                              GError       **error);
static gboolean throw_type_mismatch          (GError       **error,
                                              const GValue  *left,
                                              const GValue  *right,
                                              const gchar   *message);
static gboolean eq_enum_string               (const GValue  *left,
                                              const GValue  *right,
                                              GValue        *return_value,
                                              GError       **error);
static gboolean ne_enum_string               (const GValue  *left,
                                              const GValue  *right,
                                              GValue        *return_value,
                                              GError       **error);
static gboolean add_string_string_slow       (const GValue  *left,
                                              const GValue  *right,
                                              GValue        *return_value,
                                              GError       **error);

DECLARE_BUILTIN (abs)
DECLARE_BUILTIN (assert)
DECLARE_BUILTIN (ceil)
DECLARE_BUILTIN (floor)
DECLARE_BUILTIN (hex)
DECLARE_BUILTIN (log)
DECLARE_BUILTIN (print)
DECLARE_BUILTIN (printerr)
DECLARE_BUILTIN (repr)
DECLARE_BUILTIN (sqrt)
DECLARE_BUILTIN (sin)
DECLARE_BUILTIN (tan)
DECLARE_BUILTIN (cos)
DECLARE_BUILTIN (typeof)
DECLARE_BUILTIN (cast_byte)
DECLARE_BUILTIN (cast_char)
DECLARE_BUILTIN (cast_i32)
DECLARE_BUILTIN (cast_u32)
DECLARE_BUILTIN (cast_i64)
DECLARE_BUILTIN (cast_u64)
DECLARE_BUILTIN (cast_float)
DECLARE_BUILTIN (cast_double)
DECLARE_BUILTIN (cast_bool)

static GHashTable *fast_dispatch;
static BuiltinFunc builtin_funcs [] = {
  builtin_abs,
  builtin_ceil,
  builtin_floor,
  builtin_hex,
  builtin_log,
  builtin_print,
  builtin_repr,
  builtin_sqrt,
  builtin_typeof,
  builtin_assert,
  builtin_sin,
  builtin_tan,
  builtin_cos,
  builtin_printerr,
  builtin_cast_byte,
  builtin_cast_char,
  builtin_cast_i32,
  builtin_cast_u32,
  builtin_cast_i64,
  builtin_cast_u64,
  builtin_cast_float,
  builtin_cast_double,
  builtin_cast_bool,
};

static inline guint
build_hash (TmplExprType type,
            GType        left,
            GType        right)
{
  if (left && !G_TYPE_IS_FUNDAMENTAL (left))
    return 0;

  if (right && !G_TYPE_IS_FUNDAMENTAL (right))
    return 0;

  return type | (left << 16) | (right << 24);
}

static gboolean
eq_gtype_gtype (const GValue  *left,
                const GValue  *right,
                GValue        *return_value,
                GError       **error)
{
  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value,
                       g_value_get_gtype (left) == g_value_get_gtype (right) ||
                       g_type_is_a (g_value_get_gtype (right), g_value_get_gtype (left)));
  return TRUE;
}

static gboolean
ne_gtype_gtype (const GValue  *left,
                const GValue  *right,
                GValue        *return_value,
                GError       **error)
{
  if (eq_gtype_gtype (left, right, return_value, error))
    {
      g_value_set_boolean (return_value, !g_value_get_boolean (return_value));
      return TRUE;
    }

  return FALSE;
}

static gboolean
eq_null (const GValue  *left,
         const GValue  *right,
         GValue        *return_value,
         GError       **error)
{
  const GValue *val;

  g_value_init (return_value, G_TYPE_BOOLEAN);

  if (G_VALUE_HOLDS_POINTER (left) && g_value_get_pointer (left) == NULL)
    val = right;
  else
    val = left;

  if (val->g_type == G_TYPE_INVALID ||
      G_VALUE_HOLDS_POINTER (val) ||
      G_VALUE_HOLDS_STRING (val) ||
      G_VALUE_HOLDS_OBJECT (val) ||
      G_VALUE_HOLDS_BOXED (val) ||
      G_VALUE_HOLDS_GTYPE (val) ||
      G_VALUE_HOLDS_VARIANT (val))
    {
      g_value_set_boolean (return_value, val->data[0].v_pointer == NULL);
      return TRUE;
    }

  g_set_error (error,
               TMPL_ERROR,
               TMPL_ERROR_TYPE_MISMATCH,
               "Cannot compare %s for null equality",
               G_VALUE_TYPE_NAME (val));

  return FALSE;
}

static gboolean
ne_null (const GValue  *left,
         const GValue  *right,
         GValue        *return_value,
         GError       **error)
{
  if (eq_null (left, right, return_value, error))
    {
      g_value_set_boolean (return_value, !g_value_get_boolean (return_value));
      return TRUE;
    }

  return FALSE;
}

static gboolean
strv_eq (const GValue  *left,
         const GValue  *right,
         GValue        *return_value,
         GError       **error)
{
  g_value_init (return_value, G_TYPE_BOOLEAN);

  if (g_value_get_boxed (left) == g_value_get_boxed (right) ||
      (g_value_get_boxed (left) && g_value_get_boxed (right) && g_strv_equal (g_value_get_boxed (left), g_value_get_boxed (right))))
    g_value_set_boolean (return_value, TRUE);
  else
    g_value_set_boolean (return_value, FALSE);

  return TRUE;
}

static gboolean
strv_ne (const GValue  *left,
         const GValue  *right,
         GValue        *return_value,
         GError       **error)
{
  if (strv_eq (left, right, return_value, error))
    {
      g_value_set_boolean (return_value, !g_value_get_boolean (return_value));
      return TRUE;
    }

  return FALSE;
}

static gboolean
throw_type_mismatch (GError       **error,
                     const GValue  *left,
                     const GValue  *right,
                     const gchar   *message)
{
  if (right != NULL)
    g_set_error (error,
                 TMPL_ERROR,
                 TMPL_ERROR_TYPE_MISMATCH,
                 "%s: %s and %s",
                 message,
                 G_VALUE_TYPE_NAME (left),
                 G_VALUE_TYPE_NAME (right));
  else
    g_set_error (error,
                 TMPL_ERROR,
                 TMPL_ERROR_TYPE_MISMATCH,
                 "%s: %s", message, G_VALUE_TYPE_NAME (left));

  return TRUE;
}

#define SIMPLE_NUMBER_OP(op, left, right, return_value, error) \
  G_STMT_START { \
    if (G_VALUE_HOLDS (left, G_VALUE_TYPE (right))) \
      { \
        if (G_VALUE_HOLDS (left, G_TYPE_DOUBLE)) \
          { \
            g_value_init (return_value, G_TYPE_DOUBLE); \
            g_value_set_double (return_value, \
                                g_value_get_double (left) \
                                op \
                                g_value_get_double (right)); \
            return TRUE; \
          } \
      } \
    return throw_type_mismatch (error, left, right, "invalid op " #op); \
  } G_STMT_END

static FastDispatch
find_dispatch_slow (TmplExprSimple *node,
                    const GValue   *left,
                    const GValue   *right)
{
  if (node->type == TMPL_EXPR_EQ)
    {
      if ((G_VALUE_HOLDS_STRING (left) && G_VALUE_HOLDS_ENUM (right)) ||
          (G_VALUE_HOLDS_STRING (right) && G_VALUE_HOLDS_ENUM (left)))
        return eq_enum_string;

      if (G_VALUE_HOLDS_GTYPE (left) && G_VALUE_HOLDS_GTYPE (right))
        return eq_gtype_gtype;

      if ((G_VALUE_HOLDS_POINTER (left) && g_value_get_pointer (left) == NULL) ||
          (G_VALUE_HOLDS_POINTER (right) && g_value_get_pointer (right) == NULL))
        return eq_null;

      if (G_VALUE_HOLDS (left, G_TYPE_STRV) && G_VALUE_HOLDS (right, G_TYPE_STRV))
        return strv_eq;
    }

  if (node->type == TMPL_EXPR_NE)
    {
      if ((G_VALUE_HOLDS_STRING (left) && G_VALUE_HOLDS_ENUM (right)) ||
          (G_VALUE_HOLDS_STRING (right) && G_VALUE_HOLDS_ENUM (left)))
        return ne_enum_string;

      if (G_VALUE_HOLDS_GTYPE (left) && G_VALUE_HOLDS_GTYPE (right))
        return ne_gtype_gtype;

      if ((G_VALUE_HOLDS_POINTER (left) && g_value_get_pointer (left) == NULL) ||
          (G_VALUE_HOLDS_POINTER (right) && g_value_get_pointer (right) == NULL))
        return ne_null;

      if (G_VALUE_HOLDS (left, G_TYPE_STRV) && G_VALUE_HOLDS (right, G_TYPE_STRV))
        return strv_ne;
    }

  if (node->type == TMPL_EXPR_ADD)
    {
      if (G_VALUE_HOLDS_STRING (left) || G_VALUE_HOLDS_STRING (right))
        return add_string_string_slow;
    }

  return NULL;
}

static gboolean
tmpl_expr_simple_eval (TmplExprSimple  *node,
                       TmplScope       *scope,
                       GValue          *return_value,
                       GError         **error)
{
  GValue left = G_VALUE_INIT;
  GValue right = G_VALUE_INIT;
  gboolean ret = FALSE;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (tmpl_expr_eval_internal (node->left, scope, &left, error) &&
      ((node->right == NULL) ||
       tmpl_expr_eval_internal (node->right, scope, &right, error)))
    {
      FastDispatch dispatch = NULL;
      guint hash;

      hash = build_hash (node->type, G_VALUE_TYPE (&left), G_VALUE_TYPE (&right));

      if (hash != 0)
        dispatch = g_hash_table_lookup (fast_dispatch, GINT_TO_POINTER (hash));

      if G_UNLIKELY (dispatch == NULL)
        {
          dispatch = find_dispatch_slow (node, &left, &right);

          if (dispatch == NULL)
            {
              g_autofree gchar *msg = g_strdup_printf ("type mismatch (%d)", node->type);
              throw_type_mismatch (error, &left, &right, msg);
              goto cleanup;
            }
        }

      ret = dispatch (&left, &right, return_value, error);
    }

cleanup:
  TMPL_CLEAR_VALUE (&left);
  TMPL_CLEAR_VALUE (&right);

  return ret;
}

static gboolean
tmpl_expr_simple_eval_logical (TmplExprSimple  *node,
                               TmplScope       *scope,
                               GValue          *return_value,
                               GError         **error)
{
  GValue left = G_VALUE_INIT;
  GValue right = G_VALUE_INIT;
  gboolean ret = FALSE;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  g_value_init (return_value, G_TYPE_BOOLEAN);

  if (!tmpl_expr_eval_internal (node->left, scope, &left, error))
    goto failure;

  switch ((int)node->type)
    {
    case TMPL_EXPR_AND:
      if (!tmpl_value_as_boolean (&left))
        {
          g_value_set_boolean (return_value, FALSE);
          ret = TRUE;
          break;
        }
      if (!tmpl_expr_eval_internal (node->right, scope, &right, error))
        goto failure;
      g_value_set_boolean (return_value, tmpl_value_as_boolean (&right));
      ret = TRUE;
      break;

    case TMPL_EXPR_OR:
      if (tmpl_value_as_boolean (&left))
        {
          g_value_set_boolean (return_value, TRUE);
          ret = TRUE;
          break;
        }
      if (!tmpl_expr_eval_internal (node->right, scope, &right, error))
        goto failure;
      g_value_set_boolean (return_value, tmpl_value_as_boolean (&right));
      ret = TRUE;
      break;

    default:
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_RUNTIME_ERROR,
                   "Unknown logical operator type: %d", node->type);
      break;
    }

failure:
  TMPL_CLEAR_VALUE (&left);
  TMPL_CLEAR_VALUE (&right);

  return ret;
}

static gboolean
tmpl_expr_fn_call_eval (TmplExprFnCall  *node,
                       TmplScope       *scope,
                       GValue         *return_value,
                       GError        **error)
{
  GValue left = G_VALUE_INIT;
  gboolean ret = FALSE;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (tmpl_expr_eval_internal (node->param, scope, &left, error))
    ret = builtin_funcs [node->builtin] (&left, return_value, error);

  TMPL_CLEAR_VALUE (&left);

  return ret;
}

static gboolean
tmpl_expr_flow_eval (TmplExprFlow  *node,
                    TmplScope     *scope,
                    GValue       *return_value,
                    GError      **error)
{
  GValue cond = G_VALUE_INIT;
  gboolean ret = FALSE;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (!tmpl_expr_eval_internal (node->condition, scope, &cond, error))
    goto cleanup;

  if (node->type == TMPL_EXPR_IF)
    {
      if (tmpl_value_as_boolean (&cond))
        {
          if (node->primary != NULL)
            ret = tmpl_expr_eval_internal (node->primary, scope, return_value, error);
          else
            ret = TRUE;
          goto cleanup;
        }
      else
        {
          if (node->secondary != NULL)
            ret = tmpl_expr_eval_internal (node->secondary, scope, return_value, error);
          else
            ret = TRUE;
          goto cleanup;
        }
    }
  else if (node->type == TMPL_EXPR_WHILE)
    {
      if (node->primary != NULL)
        {
          while (tmpl_value_as_boolean (&cond))
            {
              /* last iteration is result value */
              g_value_unset (return_value);
              if (!tmpl_expr_eval_internal (node->primary, scope, return_value, error))
                goto cleanup;

              g_value_unset (&cond);
              if (!tmpl_expr_eval_internal (node->condition, scope, &cond, error))
                goto cleanup;
            }
        }
    }

  g_set_error (error,
               TMPL_ERROR,
               TMPL_ERROR_INVALID_STATE,
               "Invalid AST");

cleanup:
  TMPL_CLEAR_VALUE (&cond);

  return ret;
}

static gboolean
tmpl_expr_stmt_list_eval (TmplExprStmtList  *node,
                          TmplScope         *scope,
                          GValue            *return_value,
                          GError           **error)
{
  GValue last = G_VALUE_INIT;

  if G_UNLIKELY (node->stmts == NULL || node->stmts->len == 0)
    {
      g_set_error_literal (error,
                           TMPL_ERROR,
                           TMPL_ERROR_RUNTIME_ERROR,
                           "Runtime Error: implausible TmplExprStmtList");
      return FALSE;
    }

  for (guint i = 0; i < node->stmts->len; i++)
    {
      TmplExpr *stmt = g_ptr_array_index (node->stmts, i);

      TMPL_CLEAR_VALUE (&last);

      if (!tmpl_expr_eval_internal (stmt, scope, &last, error))
        {
          TMPL_CLEAR_VALUE (&last);
          return FALSE;
        }
    }

  *return_value = last;

  return TRUE;
}

static gboolean
tmpl_expr_args_eval (TmplExprSimple  *node,
                     TmplScope       *scope,
                     GValue          *return_value,
                     GError         **error)
{
  GValue left = G_VALUE_INIT;
  gboolean ret = FALSE;

  if (!tmpl_expr_eval_internal (node->left, scope, &left, error))
    goto cleanup;

  if (!tmpl_expr_eval_internal (node->left, scope, return_value, error))
    goto cleanup;

  ret = TRUE;

cleanup:
  TMPL_CLEAR_VALUE (&left);

  return ret;
}

static gboolean
tmpl_expr_symbol_ref_eval (TmplExprSymbolRef  *node,
                           TmplScope          *scope,
                           GValue             *return_value,
                           GError            **error)
{
  TmplSymbol *symbol;

  g_assert (node != NULL);
  g_assert (scope != NULL);

  symbol = tmpl_scope_peek (scope, node->symbol);

  if (symbol == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_MISSING_SYMBOL,
                   "No such symbol \"%s\" in scope",
                   node->symbol);
      return FALSE;
    }

  if (tmpl_symbol_get_symbol_type (symbol) == TMPL_SYMBOL_VALUE)
    {
      tmpl_symbol_get_value (symbol, return_value);
      return TRUE;
    }

  g_set_error (error,
               TMPL_ERROR,
               TMPL_ERROR_NOT_A_VALUE,
               "The symbol \"%s\" is not a value",
               node->symbol);

  return FALSE;
}

static gboolean
tmpl_expr_symbol_assign_eval (TmplExprSymbolAssign  *node,
                              TmplScope             *scope,
                              GValue                *return_value,
                              GError               **error)
{
  TmplSymbol *symbol;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (!tmpl_expr_eval_internal (node->right, scope, return_value, error))
    return FALSE;

  symbol = tmpl_scope_get (scope, node->symbol);
  tmpl_symbol_assign_value (symbol, return_value);

  return TRUE;
}

static gboolean
tmpl_expr_getattr_eval (TmplExprGetattr  *node,
                        TmplScope        *scope,
                        GValue           *return_value,
                        GError          **error)
{
  GValue left = G_VALUE_INIT;
  GParamSpec *pspec;
  GObject *object;
  gboolean ret = FALSE;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (!tmpl_expr_eval_internal (node->left, scope, &left, error))
    goto cleanup;

  if (G_VALUE_HOLDS (&left, TMPL_TYPE_TYPELIB) &&
      g_value_get_pointer (&left) != NULL)
    {
      GIRepository *repository = tmpl_repository_get_default ();
      GITypelib *typelib = g_value_get_pointer (&left);
      const gchar *ns = gi_typelib_get_namespace (typelib);
      GIBaseInfo *base_info;

      /* Maybe we can resolve this dot accessor (.foo) using GObject
       * Introspection from the first object.
       */

      base_info = gi_repository_find_by_name (repository, ns, node->attr);

      if (base_info == NULL)
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_GI_FAILURE,
                       "Failed to locate %s within %s",
                       node->attr, ns);
          goto cleanup;
        }

      g_value_init (return_value, TMPL_TYPE_BASE_INFO);
      g_value_take_boxed (return_value, base_info);

      ret = TRUE;

      goto cleanup;
    }

  if (!G_VALUE_HOLDS_OBJECT (&left))
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NOT_AN_OBJECT,
                   "Cannot access property \"%s\" of non-object \"%s\"",
                   node->attr, G_VALUE_TYPE_NAME (&left));
      goto cleanup;
    }

  object = g_value_get_object (&left);

  if (object == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NULL_POINTER,
                   "Cannot access property of null object");
      goto cleanup;
    }

  if (!(pspec = g_object_class_find_property (G_OBJECT_GET_CLASS (object), node->attr)))
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NO_SUCH_PROPERTY,
                   "No such property \"%s\" on object \"%s\"",
                   node->attr, G_OBJECT_TYPE_NAME (object));
      goto cleanup;
    }

  g_value_init (return_value, pspec->value_type);
  g_object_get_property (object, node->attr, return_value);

  ret = TRUE;

cleanup:
  TMPL_CLEAR_VALUE (&left);

  return ret;
}

static gboolean
tmpl_expr_setattr_eval (TmplExprSetattr  *node,
                        TmplScope        *scope,
                        GValue           *return_value,
                        GError          **error)
{
  GValue left = G_VALUE_INIT;
  GValue right = G_VALUE_INIT;
  GObject *object;
  gboolean ret = FALSE;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (!tmpl_expr_eval_internal (node->left, scope, &left, error))
    goto cleanup;

  if (!G_VALUE_HOLDS_OBJECT (&left))
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NOT_AN_OBJECT,
                   "Cannot access property \"%s\" of non-object \"%s\"",
                   node->attr, G_VALUE_TYPE_NAME (&left));
      goto cleanup;
    }

  object = g_value_get_object (&left);

  if (object == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NULL_POINTER,
                   "Cannot access property of null object");
      goto cleanup;
    }

  if (!g_object_class_find_property (G_OBJECT_GET_CLASS (object), node->attr))
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NO_SUCH_PROPERTY,
                   "No such property \"%s\" on object \"%s\"",
                   node->attr, G_OBJECT_TYPE_NAME (object));
      goto cleanup;
    }

  if (!tmpl_expr_eval_internal (node->right, scope, &right, error))
    goto cleanup;

  g_object_set_property (object, node->attr, &right);

  g_value_init (return_value, G_VALUE_TYPE (&right));
  g_value_copy (&right, return_value);

  ret = TRUE;

cleanup:
  TMPL_CLEAR_VALUE (&left);
  TMPL_CLEAR_VALUE (&right);

  g_assert (ret == TRUE || (error == NULL || *error != NULL));

  return ret;
}

/* Based on gtkbuilderscope.c */
static char *
make_mangle (const char *name)
{
  gboolean split_first_cap = TRUE;
  GString *symbol_name = g_string_new ("");
  int i;

  for (i = 0; name[i] != '\0'; i++)
    {
      /* skip if uppercase, first or previous is uppercase */
      if ((name[i] == g_ascii_toupper (name[i]) &&
             ((i > 0 && name[i-1] != g_ascii_toupper (name[i-1])) ||
              (i == 1 && name[0] == g_ascii_toupper (name[0]) && split_first_cap))) ||
           (i > 2 && name[i]  == g_ascii_toupper (name[i]) &&
           name[i-1] == g_ascii_toupper (name[i-1]) &&
           name[i-2] == g_ascii_toupper (name[i-2])))
        g_string_append_c (symbol_name, '_');
      g_string_append_c (symbol_name, g_ascii_tolower (name[i]));
    }

  return g_string_free (symbol_name, FALSE);
}

static gchar *
make_title (const gchar *str)
{
  GString *ret;

  g_assert (str != NULL);

  ret = g_string_new (NULL);

  for (; *str; str = g_utf8_next_char (str))
    {
      gunichar ch = g_utf8_get_char (str);

      if (!g_unichar_isalnum (ch))
        {
          if (ret->len && ret->str[ret->len - 1] != ' ')
            g_string_append_c (ret, ' ');
          continue;
        }

      if (ret->len && ret->str[ret->len - 1] != ' ')
        g_string_append_unichar (ret, ch);
      else
        g_string_append_unichar (ret, g_unichar_toupper (ch));
    }

  return g_string_free (ret, FALSE);
}

static GIBaseInfo *
find_by_gtype (GIRepository *repository,
               GType         type)
{
  while ((type != G_TYPE_INVALID))
    {
      GIBaseInfo *info = gi_repository_find_by_gtype (repository, type);

      if (info != NULL)
        return info;

      type = g_type_parent (type);
    }

  return NULL;
}

static gboolean
tmpl_expr_gi_call_eval (TmplExprGiCall  *node,
                        TmplScope       *scope,
                        GValue          *return_value,
                        GError         **error)
{
  GValue left = G_VALUE_INIT;
  GValue right = G_VALUE_INIT;
  GIRepository *repository;
  GIBaseInfo *base_info;
  GIFunctionInfo *function = NULL;
  GIArgument return_value_arg = { 0 };
  GITypeInfo return_value_type;
  GIArgument *dispatch_args = NULL;
  GITransfer xfer = 0;
  TmplExpr *args;
  GObject *object;
  gboolean ret = FALSE;
  GArray *in_args = NULL;
  GArray *values = NULL;
  GType type;
  guint dispatch_len = 0;
  guint n_args;
  guint offset = 0;
  guint i;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (!tmpl_expr_eval_internal (node->object, scope, &left, error))
    goto cleanup;

  if (G_VALUE_HOLDS_STRING (&left))
    {
      const gchar *str = g_value_get_string (&left) ?: "";

      /*
       * TODO: This should be abstracted somewhere else rather than our G-I call.
       *       Basically we are adding useful string functions like:
       *
       *       "foo".upper()
       *       "foo".lower()
       *       "foo".casefold()
       *       "foo".reverse()
       *       "foo".len()
       *       "foo".title()
       */
      if (FALSE) {}
      else if (g_str_equal (node->name, "upper"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_take_string (return_value, g_utf8_strup (str, -1));
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "lower"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_take_string (return_value, g_utf8_strdown (str, -1));
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "casefold"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_take_string (return_value, g_utf8_casefold (str, -1));
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "reverse"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_take_string (return_value, g_utf8_strreverse (str, -1));
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "len"))
        {
          g_value_init (return_value, G_TYPE_UINT);
          g_value_set_uint (return_value, strlen (str));
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "escape"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_take_string (return_value, g_strescape (str, NULL));
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "escape_markup"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_take_string (return_value, g_markup_escape_text (str, -1));
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "space"))
        {
          gchar *space;
          guint len = strlen (str);

          g_value_init (return_value, G_TYPE_STRING);
          space = g_malloc (len + 1);
          memset (space, ' ', len);
          space[len] = '\0';
          g_value_take_string (return_value, space);
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "title"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_take_string (return_value, make_title (str));
          ret = TRUE;
        }
      else if (g_str_equal (node->name, "mangle"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_take_string (return_value, make_mangle (str));
          ret = TRUE;
        }
      else
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_GI_FAILURE,
                       "No such method %s for string",
                       node->name);
        }

      goto cleanup;
    }

  if (G_VALUE_HOLDS_ENUM (&left))
    {
      if (FALSE) {}
      else if (g_str_equal (node->name, "nick"))
        {
          GEnumClass *enum_class = g_type_class_peek (G_VALUE_TYPE (&left));
          GEnumValue *enum_value = g_enum_get_value (enum_class, g_value_get_enum (&left));

          g_value_init (return_value, G_TYPE_STRING);

          if (enum_value != NULL)
            g_value_set_static_string (return_value, enum_value->value_nick);

          ret = TRUE;
        }
      else
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_GI_FAILURE,
                       "No such method %s for enum",
                       node->name);
        }

      goto cleanup;
    }

  if (G_VALUE_HOLDS_GTYPE (&left))
    {
      if (FALSE) {}
      else if (g_str_equal (node->name, "name"))
        {
          g_value_init (return_value, G_TYPE_STRING);
          g_value_set_static_string (return_value, g_type_name (g_value_get_gtype (&left)));
          ret = TRUE;
          goto cleanup;
        }
      else if (g_str_equal (node->name, "is_a"))
        {
          if (node->params != NULL)
            {
              GValue param1 = G_VALUE_INIT;

              if (!tmpl_expr_eval_internal (node->params, scope, &param1, error))
                goto cleanup;

              if (!G_VALUE_HOLDS_GTYPE (&param1))
                {
                  g_set_error (error,
                               TMPL_ERROR,
                               TMPL_ERROR_TYPE_MISMATCH,
                               "%s is not a GType",
                               G_VALUE_TYPE_NAME (&param1));
                  TMPL_CLEAR_VALUE (&param1);
                  goto cleanup;
                }

              g_value_init (return_value, G_TYPE_BOOLEAN);
              g_value_set_boolean (return_value,
                                   g_type_is_a (g_value_get_gtype (&left),
                                                g_value_get_gtype (&param1)));

              TMPL_CLEAR_VALUE (&param1);

              ret = TRUE;

              goto cleanup;
            }
        }

      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_GI_FAILURE,
                   "No such method %s of GType",
                   node->name);

      goto cleanup;
    }

  repository = tmpl_repository_get_default ();

  if (G_VALUE_HOLDS (&left, TMPL_TYPE_TYPELIB) &&
      g_value_get_pointer (&left) != NULL)
    {
      GITypelib *typelib = g_value_get_pointer (&left);
      const gchar *ns = gi_typelib_get_namespace (typelib);

      base_info = gi_repository_find_by_name (repository, ns, node->name);

      if (base_info == NULL || !GI_IS_FUNCTION_INFO (base_info))
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_GI_FAILURE,
                       "%s is not a function in %s",
                       node->name, ns);
          goto cleanup;
        }

      function = (GIFunctionInfo *)base_info;

      n_args = gi_callable_info_get_n_args ((GICallableInfo *)function);

      values = g_array_new (FALSE, TRUE, sizeof (GValue));
      g_array_set_clear_func (values, (GDestroyNotify)g_value_unset);
      g_array_set_size (values, n_args);

      in_args = g_array_new (FALSE, TRUE, sizeof (GIArgument));
      g_array_set_size (in_args, n_args + 1);

      /* Skip past first param, which is used for Object below */
      dispatch_args = ((GIArgument *)(gpointer)in_args->data);
      dispatch_len = n_args;

      goto apply_args;
    }

  if (G_VALUE_HOLDS (&left, TMPL_TYPE_BASE_INFO) &&
      (base_info = g_value_get_boxed (&left)) &&
      GI_IS_OBJECT_INFO (base_info))
    {
      TmplGTypeFunc gtype_func = tmpl_gi_get_gtype_func (base_info);

      if (gtype_func != NULL &&
          (type = gtype_func ()) &&
          g_type_is_a (type, G_TYPE_OBJECT))
        {
          object = NULL;
          goto lookup_for_object;
        }

      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NOT_AN_OBJECT,
                   "Failed to locate GType function for object");
      goto cleanup;
    }

  if (!G_VALUE_HOLDS_OBJECT (&left))
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NOT_AN_OBJECT,
                   "Cannot access function \"%s\" of non-object \"%s\"",
                   node->name, G_VALUE_TYPE_NAME (&left));
      goto cleanup;
    }

  object = g_value_get_object (&left);

  if (object == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NULL_POINTER,
                   "Cannot access function of null object");
      goto cleanup;
    }

  type = G_OBJECT_TYPE (object);

lookup_for_object:
  while (g_type_is_a (type, G_TYPE_OBJECT))
    {
      guint n_ifaces;

      base_info = find_by_gtype (repository, type);

      if (base_info == NULL)
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_GI_FAILURE,
                       "Failed to locate GObject Introspection data for %s. "
                       "Consider importing required module.",
                       g_type_name (type));
          goto cleanup;
        }

      /* First locate the function in the object */
      function = gi_object_info_find_method ((GIObjectInfo *)base_info, node->name);
      if (function != NULL)
        break;

      /* Maybe the function is found in an interface */
      n_ifaces = gi_object_info_get_n_interfaces ((GIObjectInfo *)base_info);
      for (i = 0; function == NULL && i < n_ifaces; i++)
        {
          GIInterfaceInfo *iface_info = NULL;

          iface_info = gi_object_info_get_interface ((GIObjectInfo *)base_info, i);
          function = gi_interface_info_find_method (iface_info, node->name);

          g_clear_pointer (&iface_info, gi_base_info_unref);
        }

      if (function != NULL)
        break;

      type = g_type_parent (type);
    }

  if (function == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_GI_FAILURE,
                   "No such method \"%s\" on object \"%s\"",
                   node->name, G_OBJECT_TYPE_NAME (object));
      goto cleanup;
    }

  n_args = gi_callable_info_get_n_args ((GICallableInfo *)function);

  values = g_array_new (FALSE, TRUE, sizeof (GValue));
  g_array_set_clear_func (values, (GDestroyNotify)g_value_unset);
  g_array_set_size (values, n_args);

  in_args = g_array_new (FALSE, TRUE, sizeof (GIArgument));
  g_array_set_size (in_args, n_args + 1);

  if (object != NULL)
    {
      g_array_index (in_args, GIArgument, 0).v_pointer = object;
      offset = 1;
    }
  else
    {
      in_args->len--;
    }

  dispatch_args = (GIArgument *)(gpointer)in_args->data;
  dispatch_len = in_args->len;

apply_args:
  args = node->params;

  for (i = 0; i < n_args; i++)
    {
      g_autoptr(GIArgInfo) arg_info = gi_callable_info_get_arg ((GICallableInfo *)function, i);
      GIArgument *arg = &g_array_index (in_args, GIArgument, i + offset);
      GValue *value = &g_array_index (values, GValue, i);
      GITypeInfo type_info = { 0 };

      if (gi_arg_info_get_direction (arg_info) != GI_DIRECTION_IN)
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_RUNTIME_ERROR,
                       "Only \"in\" parameters are supported");
          goto cleanup;
        }

      if (args == NULL)
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_SYNTAX_ERROR,
                       "Too few arguments to function \"%s\"",
                       node->name);
          goto cleanup;
        }

      if (args->any.type == TMPL_EXPR_ARGS)
        {
          if (!tmpl_expr_eval_internal (((TmplExprSimple *)args)->left, scope, value, error))
            goto cleanup;

          args = ((TmplExprSimple *)args)->right;
        }
      else
        {
          if (!tmpl_expr_eval_internal (args, scope, value, error))
            goto cleanup;

          args = NULL;
        }

      gi_arg_info_load_type_info (arg_info, &type_info);

      if (!tmpl_gi_argument_from_g_value (value, &type_info, arg_info, arg, error))
        {
          gi_base_info_clear (&type_info);
          goto cleanup;
        }

      gi_base_info_clear (&type_info);
    }

  if ((args != NULL) && (n_args > 0))
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_SYNTAX_ERROR,
                   "Too many arguments to function \"%s\"",
                   node->name);
      goto cleanup;
    }

  if (!gi_function_info_invoke (function,
                                dispatch_args,
                                dispatch_len,
                                NULL,
                                0,
                                &return_value_arg,
                                error))
    goto cleanup;

  gi_callable_info_load_return_type ((GICallableInfo *)function, &return_value_type);

  xfer = gi_callable_info_get_caller_owns ((GICallableInfo *)function);
  ret = tmpl_gi_argument_to_g_value (return_value, &return_value_type, &return_value_arg, xfer, error);

  gi_base_info_clear (&return_value_type);

cleanup:
  g_clear_pointer (&in_args, g_array_unref);

  if (values != NULL)
    {
      for (i = 0; i < values->len; i++)
        {
          GValue *value = &g_array_index (values, GValue, i);

          if (G_VALUE_TYPE (value) != G_TYPE_INVALID)
            g_value_unset (value);
        }

      g_clear_pointer (&values, g_array_unref);
    }

  TMPL_CLEAR_VALUE (&left);
  TMPL_CLEAR_VALUE (&right);

  g_clear_pointer (&function, gi_base_info_unref);

  return ret;
}

static gboolean
tmpl_expr_anon_fn_call_eval (TmplExprAnonFnCall  *node,
                             TmplScope           *scope,
                             GValue              *return_value,
                             GError             **error)
{
  char **args;
  TmplExpr *params = NULL;
  TmplScope *local_scope = NULL;
  gboolean ret = FALSE;
  gint n_args = 0;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  if (node->anon->any.type != TMPL_EXPR_FUNC)
    {
      g_set_error_literal (error,
                           TMPL_ERROR,
                           TMPL_ERROR_NOT_A_FUNCTION,
                           "Not a function to evaluate");
      return FALSE;
    }

  args = node->anon->func.symlist;
  n_args = args ? g_strv_length (args) : 0;
  local_scope = tmpl_scope_new_with_parent (scope);
  params = node->params;

  for (guint i = 0; i < n_args; i++)
    {
      const gchar *arg = args[i];
      GValue value = G_VALUE_INIT;

      if (params == NULL)
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_SYNTAX_ERROR,
                       "Function takes %d arguments, got %d",
                       n_args, i);
          return FALSE;
        }

      if (params->any.type == TMPL_EXPR_ARGS)
        {
          TmplExprSimple *simple = (TmplExprSimple *)params;

          if (!tmpl_expr_eval_internal (simple->left, local_scope, &value, error))
            goto cleanup;

          params = simple->right;
        }
      else
        {
          if (!tmpl_expr_eval_internal (params, local_scope, &value, error))
            goto cleanup;

          params = NULL;
        }

      tmpl_scope_set_value (local_scope, arg, &value);
      TMPL_CLEAR_VALUE (&value);
    }

  if (params != NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_SYNTAX_ERROR,
                   "Function takes %d params",
                   n_args);
      goto cleanup;
    }

  if (!tmpl_expr_eval_internal (node->anon, local_scope, return_value, error))
    goto cleanup;

  ret = TRUE;

cleanup:
  g_clear_pointer (&local_scope, tmpl_scope_unref);

  return ret;
}

static gboolean
tmpl_expr_user_fn_call_eval (TmplExprUserFnCall  *node,
                             TmplScope           *scope,
                             GValue              *return_value,
                             GError             **error)
{
  GPtrArray *args_ar = NULL;
  TmplExpr *expr = NULL;
  TmplExpr *params = NULL;
  TmplScope *local_scope = NULL;
  TmplSymbol *symbol;
  const char * const *args = NULL;
  gboolean ret = FALSE;
  gint n_args = 0;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  symbol = tmpl_scope_peek (scope, node->symbol);

  if (symbol == NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_MISSING_SYMBOL,
                   "No such function \"%s\"",
                   node->symbol);
      return FALSE;
    }

  if (tmpl_symbol_get_symbol_type (symbol) != TMPL_SYMBOL_EXPR)
    {
      /* Check for anonymous function */
      if (tmpl_symbol_holds (symbol, TMPL_TYPE_EXPR) &&
          (expr = tmpl_symbol_get_boxed (symbol)) &&
          expr->any.type == TMPL_EXPR_FUNC)
        {
          args = (const char * const *)expr->func.symlist;
          n_args = args ? g_strv_length ((char **)args) : 0;
          expr = expr->func.list;
          goto prepare;
        }

      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_NOT_A_FUNCTION,
                   "\"%s\" is not a function within scope",
                   node->symbol);
      return FALSE;
    }

  expr = tmpl_symbol_get_expr (symbol, &args_ar);

  if (args_ar == NULL)
    n_args = 0, args = NULL;
  else
    n_args = args_ar->len, args = (const char * const *)(gpointer)args_ar->pdata;

prepare:
  g_assert (expr != NULL);
  g_assert (n_args == 0 || args != NULL);

  local_scope = tmpl_scope_new_with_parent (scope);
  params = node->params;

  for (guint i = 0; i < n_args; i++)
    {
      const gchar *arg = args[i];
      GValue value = G_VALUE_INIT;

      g_assert (arg != NULL);

      if (params == NULL)
        {
          g_set_error (error,
                       TMPL_ERROR,
                       TMPL_ERROR_SYNTAX_ERROR,
                       "\"%s\" takes %d arguments, not %d",
                       node->symbol, n_args, i);
          return FALSE;
        }

      if (params->any.type == TMPL_EXPR_ARGS)
        {
          TmplExprSimple *simple = (TmplExprSimple *)params;

          if (!tmpl_expr_eval_internal (simple->left, local_scope, &value, error))
            goto cleanup;

          params = simple->right;
        }
      else
        {
          if (!tmpl_expr_eval_internal (params, local_scope, &value, error))
            goto cleanup;

          params = NULL;
        }

      symbol = tmpl_scope_get (local_scope, arg);
      tmpl_symbol_assign_value (symbol, &value);

      TMPL_CLEAR_VALUE (&value);
    }

  if (params != NULL)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_SYNTAX_ERROR,
                   "\"%s\" takes %d params",
                   node->symbol, n_args);
      goto cleanup;
    }

  if (!tmpl_expr_eval_internal (expr, local_scope, return_value, error))
    goto cleanup;

  ret = TRUE;

cleanup:
  g_clear_pointer (&local_scope, tmpl_scope_unref);

  return ret;
}

static gboolean
tmpl_expr_require_eval (TmplExprRequire  *node,
                        TmplScope        *scope,
                        GValue           *return_value,
                        GError          **error)
{
  GITypelib *typelib;
  GError *local_error = NULL;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  typelib = gi_repository_require (tmpl_repository_get_default (),
                                   node->name,
                                   node->version,
                                   0,
                                   &local_error);

  g_assert (typelib != NULL || local_error != NULL);

  if (typelib == NULL)
    {
      g_propagate_error (error, local_error);
      return FALSE;
    }

  g_value_init (return_value, TMPL_TYPE_TYPELIB);
  g_value_set_pointer (return_value, typelib);
  tmpl_scope_set_value (scope, node->name, return_value);

  return TRUE;
}

static gboolean
tmpl_expr_func_eval (TmplExprFunc  *node,
                     TmplScope     *scope,
                     GValue        *return_value,
                     GError       **error)
{
  GPtrArray *args = NULL;
  TmplSymbol *symbol;

  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  /* We just need to insert a symbol into @scope that includes
   * the function defined here. If the symbol already exists,
   * it will be replaced with this function.
   */

  if (node->symlist != NULL)
    {
      args = g_ptr_array_new_with_free_func (g_free);
      for (guint i = 0; node->symlist[i]; i++)
        g_ptr_array_add (args, g_strdup (node->symlist[i]));
    }

  if (node->name != NULL)
    {
      symbol = tmpl_scope_get (scope, node->name);
      tmpl_symbol_assign_expr (symbol, node->list, args);
      g_clear_pointer (&args, g_ptr_array_unref);
    }
  else
    {
      g_value_init (return_value, TMPL_TYPE_EXPR);
      g_value_set_boxed (return_value, node);
    }

  g_clear_pointer (&args, g_ptr_array_unref);

  return TRUE;
}

static gboolean
tmpl_expr_eval_internal (TmplExpr   *node,
                         TmplScope  *scope,
                         GValue     *return_value,
                         GError    **error)
{
  g_assert (node != NULL);
  g_assert (scope != NULL);
  g_assert (return_value != NULL);

  switch (node->any.type)
    {
    case TMPL_EXPR_ADD:
    case TMPL_EXPR_SUB:
    case TMPL_EXPR_MUL:
    case TMPL_EXPR_DIV:
    case TMPL_EXPR_UNARY_MINUS:
    case TMPL_EXPR_GT:
    case TMPL_EXPR_LT:
    case TMPL_EXPR_NE:
    case TMPL_EXPR_EQ:
    case TMPL_EXPR_GTE:
    case TMPL_EXPR_LTE:
      return tmpl_expr_simple_eval ((TmplExprSimple *)node, scope, return_value, error);

    case TMPL_EXPR_AND:
    case TMPL_EXPR_OR:
      return tmpl_expr_simple_eval_logical ((TmplExprSimple *)node, scope, return_value, error);

    case TMPL_EXPR_NUMBER:
      g_value_init (return_value, G_TYPE_DOUBLE);
      g_value_set_double (return_value, ((TmplExprNumber *)node)->number);
      return TRUE;

    case TMPL_EXPR_BOOLEAN:
      g_value_init (return_value, G_TYPE_BOOLEAN);
      g_value_set_boolean (return_value, ((TmplExprBoolean *)node)->value);
      return TRUE;

    case TMPL_EXPR_STRING:
      g_value_init (return_value, G_TYPE_STRING);
      g_value_set_string (return_value, ((TmplExprString *)node)->value);
      return TRUE;

    case TMPL_EXPR_ARGS:
      return tmpl_expr_args_eval ((TmplExprSimple *)node, scope, return_value, error);

    case TMPL_EXPR_STMT_LIST:
      return tmpl_expr_stmt_list_eval ((TmplExprStmtList *)node, scope, return_value, error);

    case TMPL_EXPR_IF:
    case TMPL_EXPR_WHILE:
      return tmpl_expr_flow_eval ((TmplExprFlow *)node, scope, return_value, error);

    case TMPL_EXPR_SYMBOL_REF:
      return tmpl_expr_symbol_ref_eval ((TmplExprSymbolRef *)node, scope, return_value, error);

    case TMPL_EXPR_SYMBOL_ASSIGN:
      return tmpl_expr_symbol_assign_eval ((TmplExprSymbolAssign *)node, scope, return_value, error);

    case TMPL_EXPR_FN_CALL:
      return tmpl_expr_fn_call_eval ((TmplExprFnCall *)node, scope, return_value, error);

    case TMPL_EXPR_ANON_FN_CALL:
      return tmpl_expr_anon_fn_call_eval ((TmplExprAnonFnCall *)node, scope, return_value, error);

    case TMPL_EXPR_USER_FN_CALL:
      return tmpl_expr_user_fn_call_eval ((TmplExprUserFnCall *)node, scope, return_value, error);

    case TMPL_EXPR_GI_CALL:
      return tmpl_expr_gi_call_eval ((TmplExprGiCall *)node, scope, return_value, error);

    case TMPL_EXPR_GETATTR:
      return tmpl_expr_getattr_eval ((TmplExprGetattr *)node, scope, return_value, error);

    case TMPL_EXPR_SETATTR:
      return tmpl_expr_setattr_eval ((TmplExprSetattr *)node, scope, return_value, error);

    case TMPL_EXPR_REQUIRE:
      return tmpl_expr_require_eval ((TmplExprRequire *)node, scope, return_value, error);

    case TMPL_EXPR_INVERT_BOOLEAN:
      {
        GValue tmp = G_VALUE_INIT;
        gboolean ret;

        ret = tmpl_expr_eval_internal (((TmplExprSimple *)node)->left, scope, &tmp, error);

        if (ret)
          {
            g_value_init (return_value, G_TYPE_BOOLEAN);
            g_value_set_boolean (return_value, !tmpl_value_as_boolean (&tmp));
          }

        TMPL_CLEAR_VALUE (&tmp);

        g_assert (ret == TRUE || (error == NULL || *error != NULL));

        return ret;
      }

    case TMPL_EXPR_FUNC:
      return tmpl_expr_func_eval ((TmplExprFunc *)node, scope, return_value, error);

    case TMPL_EXPR_NOP:
      return TRUE;

    case TMPL_EXPR_NULL:
      g_value_init (return_value, G_TYPE_POINTER);
      g_value_set_pointer (return_value, NULL);
      return TRUE;

    default:
      break;
    }

  g_set_error (error,
               TMPL_ERROR,
               TMPL_ERROR_INVALID_OP_CODE,
               "invalid opcode: %04x", node->any.type);

  return FALSE;
}

static gboolean
div_double_double (const GValue  *left,
                   const GValue  *right,
                   GValue        *return_value,
                   GError       **error)
{
  gdouble denom = g_value_get_double (right);

  if (denom == 0.0)
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_DIVIDE_BY_ZERO,
                   "divide by zero");
      return FALSE;
    }

  g_value_init (return_value, G_TYPE_DOUBLE);
  g_value_set_double (return_value, g_value_get_double (left) / denom);

  return TRUE;
}

static gboolean
unary_minus_double (const GValue  *left,
                    const GValue  *right,
                    GValue        *return_value,
                    GError       **error)
{
  g_value_init (return_value, G_TYPE_DOUBLE);
  g_value_set_double (return_value, -g_value_get_double (left));
  return TRUE;
}

static gboolean
mul_double_string (const GValue  *left,
                   const GValue  *right,
                   GValue        *return_value,
                   GError       **error)
{
  GString *str;
  gint v;
  gint i;

  str = g_string_new (NULL);
  v = g_value_get_double (left);

  for (i = 0; i < v; i++)
    g_string_append (str, g_value_get_string (right));

  g_value_init (return_value, G_TYPE_STRING);
  g_value_take_string (return_value, g_string_free (str, FALSE));

  return TRUE;
}

static gboolean
mul_string_double (const GValue  *left,
                   const GValue  *right,
                   GValue        *return_value,
                   GError       **error)
{
  return mul_double_string (right, left, return_value, error);
}

static gboolean
add_string_string (const GValue  *left,
                   const GValue  *right,
                   GValue        *return_value,
                   GError       **error)
{
  g_value_init (return_value, G_TYPE_STRING);
  g_value_take_string (return_value,
                       g_strdup_printf ("%s%s",
                                        g_value_get_string (left),
                                        g_value_get_string (right)));
  return TRUE;
}

static gboolean
add_string_string_slow (const GValue  *left,
                        const GValue  *right,
                        GValue        *return_value,
                        GError       **error)
{
  GValue trans = G_VALUE_INIT;

  g_value_init (&trans, G_TYPE_STRING);
  g_value_init (return_value, G_TYPE_STRING);

  if (G_VALUE_HOLDS_STRING (left))
    {
      if (!g_value_transform (right, &trans))
        return FALSE;
      right = &trans;
    }
  else
    {
      if (!g_value_transform (left, &trans))
        return FALSE;
      left = &trans;
    }

  g_value_take_string (return_value,
                       g_strdup_printf ("%s%s",
                                        g_value_get_string (left),
                                        g_value_get_string (right)));

  return TRUE;
}

static gboolean
eq_string_string (const GValue  *left,
                  const GValue  *right,
                  GValue        *return_value,
                  GError       **error)
{
  const gchar *left_str = g_value_get_string (left);
  const gchar *right_str = g_value_get_string (right);

  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value, 0 == g_strcmp0 (left_str, right_str));

  return TRUE;
}

static gboolean
ne_string_string (const GValue  *left,
                  const GValue  *right,
                  GValue        *return_value,
                  GError       **error)
{
  const gchar *left_str = g_value_get_string (left);
  const gchar *right_str = g_value_get_string (right);

  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value, 0 != g_strcmp0 (left_str, right_str));

  return TRUE;
}

static gboolean
eq_boolean_boolean (const GValue  *left,
                    const GValue  *right,
                    GValue        *return_value,
                    GError       **error)
{
  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value,
                       g_value_get_boolean (left) == g_value_get_boolean (right));
  return TRUE;
}

static gboolean
ne_boolean_boolean (const GValue  *left,
                    const GValue  *right,
                    GValue        *return_value,
                    GError       **error)
{
  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value,
                       g_value_get_boolean (left) != g_value_get_boolean (right));
  return TRUE;
}

static gboolean
eq_pointer_pointer (const GValue  *left,
                    const GValue  *right,
                    GValue        *return_value,
                    GError       **error)
{
  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value,
                       g_value_get_pointer (left) == g_value_get_pointer (right));
  return TRUE;
}

static gboolean
ne_pointer_pointer (const GValue  *left,
                    const GValue  *right,
                    GValue        *return_value,
                    GError       **error)
{
  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value,
                       g_value_get_pointer (left) != g_value_get_pointer (right));
  return TRUE;
}

static gboolean
eq_enum_string (const GValue  *left,
                const GValue  *right,
                GValue        *return_value,
                GError       **error)
{
  const gchar *str;
  GEnumClass *klass;
  const GEnumValue *val;
  GType type;
  gint eval;

  if (G_VALUE_HOLDS_STRING (left))
    {
      str = g_value_get_string (left);
      eval = g_value_get_enum (right);
      type = G_VALUE_TYPE (right);
    }
  else
    {
      str = g_value_get_string (right);
      eval = g_value_get_enum (left);
      type = G_VALUE_TYPE (left);
    }

  klass = g_type_class_peek (type);
  val = g_enum_get_value ((GEnumClass *)klass, eval);

  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value, 0 == g_strcmp0 (str, val->value_nick));

  return TRUE;
}

static gboolean
ne_enum_string (const GValue  *left,
                const GValue  *right,
                GValue        *return_value,
                GError       **error)
{
  if (eq_enum_string (left, right, return_value, error))
    {
      g_value_set_boolean (return_value, !g_value_get_boolean (return_value));
      return TRUE;
    }

  return FALSE;
}

#define SIMPLE_OP_FUNC(func_name, ret_type, set_func, get_left, op, get_right)  \
static gboolean                                                                 \
func_name (const GValue  *left,                                                 \
           const GValue  *right,                                                \
           GValue        *return_value,                                         \
           GError       **error)                                                \
{                                                                               \
  g_value_init (return_value, ret_type);                                        \
  g_value_##set_func (return_value,                                             \
                      g_value_##get_left (left)                                 \
                      op                                                        \
                      g_value_##get_right (right));                             \
  return TRUE;                                                                  \
}

SIMPLE_OP_FUNC (add_double_double, G_TYPE_DOUBLE,  set_double,  get_double, +,  get_double)
SIMPLE_OP_FUNC (sub_double_double, G_TYPE_DOUBLE,  set_double,  get_double, -,  get_double)
SIMPLE_OP_FUNC (mul_double_double, G_TYPE_DOUBLE,  set_double,  get_double, *,  get_double)
SIMPLE_OP_FUNC (lt_double_double,  G_TYPE_BOOLEAN, set_boolean, get_double, <,  get_double)
SIMPLE_OP_FUNC (lte_double_double, G_TYPE_BOOLEAN, set_boolean, get_double, <=, get_double)
SIMPLE_OP_FUNC (gt_double_double,  G_TYPE_BOOLEAN, set_boolean, get_double, >,  get_double)
SIMPLE_OP_FUNC (eq_double_double,  G_TYPE_BOOLEAN, set_boolean, get_double, ==, get_double)
SIMPLE_OP_FUNC (ne_double_double,  G_TYPE_BOOLEAN, set_boolean, get_double, !=, get_double)
SIMPLE_OP_FUNC (gte_double_double, G_TYPE_BOOLEAN, set_boolean, get_double, >=, get_double)

SIMPLE_OP_FUNC (eq_double_int,     G_TYPE_BOOLEAN, set_boolean, get_double, ==, get_int)
SIMPLE_OP_FUNC (eq_double_uint,    G_TYPE_BOOLEAN, set_boolean, get_double, ==, get_uint)
SIMPLE_OP_FUNC (eq_int_double,     G_TYPE_BOOLEAN, set_boolean, get_int,    ==, get_double)
SIMPLE_OP_FUNC (eq_uint_double,    G_TYPE_BOOLEAN, set_boolean, get_uint,   ==, get_double)
SIMPLE_OP_FUNC (gt_double_int,     G_TYPE_BOOLEAN, set_boolean, get_double,  >, get_int)
SIMPLE_OP_FUNC (gt_double_uint,    G_TYPE_BOOLEAN, set_boolean, get_double,  >, get_uint)
SIMPLE_OP_FUNC (gt_int_double,     G_TYPE_BOOLEAN, set_boolean, get_int,     >, get_double)
SIMPLE_OP_FUNC (gt_uint_double,    G_TYPE_BOOLEAN, set_boolean, get_uint,    >, get_double)
SIMPLE_OP_FUNC (lt_double_int,     G_TYPE_BOOLEAN, set_boolean, get_double,  <, get_int)
SIMPLE_OP_FUNC (lt_double_uint,    G_TYPE_BOOLEAN, set_boolean, get_double,  <, get_uint)
SIMPLE_OP_FUNC (lt_int_double,     G_TYPE_BOOLEAN, set_boolean, get_int,     <, get_double)
SIMPLE_OP_FUNC (lt_uint_double,    G_TYPE_BOOLEAN, set_boolean, get_uint,    <, get_double)
SIMPLE_OP_FUNC (ne_double_int,     G_TYPE_BOOLEAN, set_boolean, get_double, !=, get_uint)
SIMPLE_OP_FUNC (ne_double_uint,    G_TYPE_BOOLEAN, set_boolean, get_double, !=, get_uint)
SIMPLE_OP_FUNC (ne_int_double,     G_TYPE_BOOLEAN, set_boolean, get_uint,   !=, get_double)
SIMPLE_OP_FUNC (ne_uint_double,    G_TYPE_BOOLEAN, set_boolean, get_uint,   !=, get_double)

#undef SIMPLE_OP_FUNC

static GHashTable *
build_dispatch_table (void)
{
  GHashTable *table;

  table = g_hash_table_new (NULL, NULL);

#define ADD_DISPATCH_FUNC(type, left, right, func) \
  g_hash_table_insert(table, \
                      GINT_TO_POINTER(build_hash(type, left, right)),\
                      func)

  ADD_DISPATCH_FUNC (TMPL_EXPR_ADD,         G_TYPE_DOUBLE, G_TYPE_DOUBLE, add_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_ADD,         G_TYPE_STRING, G_TYPE_STRING, add_string_string);
  ADD_DISPATCH_FUNC (TMPL_EXPR_SUB,         G_TYPE_DOUBLE, G_TYPE_DOUBLE, sub_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_MUL,         G_TYPE_DOUBLE, G_TYPE_DOUBLE, mul_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_DIV,         G_TYPE_DOUBLE, G_TYPE_DOUBLE, div_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_UNARY_MINUS, G_TYPE_DOUBLE, 0,             unary_minus_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_LT,          G_TYPE_DOUBLE, G_TYPE_DOUBLE, lt_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_GT,          G_TYPE_DOUBLE, G_TYPE_DOUBLE, gt_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_NE,          G_TYPE_DOUBLE, G_TYPE_DOUBLE, ne_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_LTE,         G_TYPE_DOUBLE, G_TYPE_DOUBLE, lte_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_GTE,         G_TYPE_DOUBLE, G_TYPE_DOUBLE, gte_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_EQ,          G_TYPE_DOUBLE, G_TYPE_DOUBLE, eq_double_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_MUL,         G_TYPE_STRING, G_TYPE_DOUBLE, mul_string_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_MUL,         G_TYPE_DOUBLE, G_TYPE_STRING, mul_double_string);
  ADD_DISPATCH_FUNC (TMPL_EXPR_EQ,          G_TYPE_STRING, G_TYPE_STRING, eq_string_string);
  ADD_DISPATCH_FUNC (TMPL_EXPR_NE,          G_TYPE_STRING, G_TYPE_STRING, ne_string_string);

  ADD_DISPATCH_FUNC (TMPL_EXPR_EQ,          G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, eq_boolean_boolean);
  ADD_DISPATCH_FUNC (TMPL_EXPR_NE,          G_TYPE_BOOLEAN, G_TYPE_BOOLEAN, ne_boolean_boolean);

  ADD_DISPATCH_FUNC (TMPL_EXPR_EQ,          G_TYPE_POINTER, G_TYPE_POINTER, eq_pointer_pointer);
  ADD_DISPATCH_FUNC (TMPL_EXPR_NE,          G_TYPE_POINTER, G_TYPE_POINTER, ne_pointer_pointer);

  ADD_DISPATCH_FUNC (TMPL_EXPR_EQ,          G_TYPE_UINT,   G_TYPE_DOUBLE, eq_uint_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_EQ,          G_TYPE_DOUBLE, G_TYPE_UINT,   eq_double_uint);
  ADD_DISPATCH_FUNC (TMPL_EXPR_NE,          G_TYPE_UINT,   G_TYPE_DOUBLE, ne_uint_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_NE,          G_TYPE_DOUBLE, G_TYPE_UINT,   ne_double_uint);
  ADD_DISPATCH_FUNC (TMPL_EXPR_GT,          G_TYPE_UINT,   G_TYPE_DOUBLE, gt_uint_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_GT,          G_TYPE_DOUBLE, G_TYPE_UINT,   gt_double_uint);
  ADD_DISPATCH_FUNC (TMPL_EXPR_LT,          G_TYPE_UINT,   G_TYPE_DOUBLE, lt_uint_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_LT,          G_TYPE_DOUBLE, G_TYPE_UINT,   lt_double_uint);

  ADD_DISPATCH_FUNC (TMPL_EXPR_EQ,          G_TYPE_INT,    G_TYPE_DOUBLE, eq_int_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_EQ,          G_TYPE_DOUBLE, G_TYPE_INT,    eq_double_int);
  ADD_DISPATCH_FUNC (TMPL_EXPR_NE,          G_TYPE_INT,    G_TYPE_DOUBLE, ne_int_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_NE,          G_TYPE_DOUBLE, G_TYPE_INT,    ne_double_int);
  ADD_DISPATCH_FUNC (TMPL_EXPR_GT,          G_TYPE_INT,    G_TYPE_DOUBLE, gt_int_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_GT,          G_TYPE_DOUBLE, G_TYPE_INT,    gt_double_int);
  ADD_DISPATCH_FUNC (TMPL_EXPR_LT,          G_TYPE_INT,    G_TYPE_DOUBLE, lt_int_double);
  ADD_DISPATCH_FUNC (TMPL_EXPR_LT,          G_TYPE_DOUBLE, G_TYPE_INT,    lt_double_int);

#undef ADD_DISPATCH_FUNC

  return table;
}

gboolean
tmpl_expr_eval (TmplExpr   *node,
                TmplScope  *scope,
                GValue     *return_value,
                GError    **error)
{
  gboolean ret;

  g_return_val_if_fail (node != NULL, FALSE);
  g_return_val_if_fail (scope != NULL, FALSE);
  g_return_val_if_fail (return_value != NULL, FALSE);
  g_return_val_if_fail (G_VALUE_TYPE (return_value) == G_TYPE_INVALID, FALSE);

  if (g_once_init_enter (&fast_dispatch))
    g_once_init_leave (&fast_dispatch, build_dispatch_table ());

  ret = tmpl_expr_eval_internal (node, scope, return_value, error);

  g_assert (ret == TRUE || (error == NULL || *error != NULL));

  return ret;
}

static gboolean
builtin_abs (const GValue  *value,
             GValue        *return_value,
             GError       **error)
{
  char *errmsg;

  if (G_VALUE_HOLDS_DOUBLE (value))
    {
      g_value_init (return_value, G_TYPE_DOUBLE);
      g_value_set_double (return_value, ABS (g_value_get_double (value)));
      return TRUE;
    }
  else if (G_VALUE_HOLDS_FLOAT (value))
    {
      g_value_init (return_value, G_TYPE_FLOAT);
      g_value_set_float (return_value, ABS (g_value_get_float (value)));
      return TRUE;
    }
  else if (G_VALUE_HOLDS_INT (value))
    {
      g_value_init (return_value, G_TYPE_INT);
      g_value_set_int (return_value, ABS (g_value_get_int (value)));
      return TRUE;
    }
  else if (G_VALUE_HOLDS_INT64 (value))
    {
      g_value_init (return_value, G_TYPE_INT64);
      g_value_set_int64 (return_value, ABS (g_value_get_int64 (value)));
      return TRUE;
    }
  else if (G_VALUE_HOLDS_UINT (value) ||
           G_VALUE_HOLDS_UINT64 (value) ||
           G_VALUE_HOLDS_UCHAR (value))
    {
      *return_value = *value;
      return TRUE;
    }

  errmsg = g_strdup_printf ("Cannot abs() type %s", G_VALUE_TYPE_NAME (value));
  throw_type_mismatch (error, value, NULL, errmsg);
  g_free (errmsg);

  return FALSE;
}

static gboolean
builtin_assert (const GValue  *value,
                GValue        *return_value,
                GError       **error)
{
  if (G_VALUE_TYPE (value) == G_TYPE_INVALID)
    goto failure;

  if (G_VALUE_HOLDS_BOOLEAN (value))
    {
      if (g_value_get_boolean (value) == FALSE)
        goto failure;
    }
  else if (G_VALUE_HOLDS_STRING (value))
    {
      if (g_value_get_string (value) == NULL)
        goto failure;
    }
  else if (G_VALUE_HOLDS_POINTER (value))
    {
      if (g_value_get_pointer (value) == NULL)
        goto failure;
    }
  else if (G_VALUE_HOLDS_OBJECT (value))
    {
      if (g_value_get_object (value) == NULL)
        goto failure;
    }
  else if (G_VALUE_HOLDS_INT (value))
    {
      if (g_value_get_int (value) == 0)
        goto failure;
    }
  else if (G_VALUE_HOLDS_UINT (value))
    {
      if (g_value_get_uint (value) == 0)
        goto failure;
    }
  else if (G_VALUE_HOLDS_INT64 (value))
    {
      if (g_value_get_int64 (value) == 0)
        goto failure;
    }
  else if (G_VALUE_HOLDS_UINT64 (value))
    {
      if (g_value_get_uint64 (value) == 0)
        goto failure;
    }
  else if (G_VALUE_HOLDS_DOUBLE (value))
    {
      if (g_value_get_double (value) == .0)
        goto failure;
    }
  else if (G_VALUE_HOLDS_FLOAT (value))
    {
      if (g_value_get_float (value) == .0f)
        goto failure;
    }
  else if (G_VALUE_HOLDS_GTYPE (value))
    {
      if (g_value_get_gtype (value) == G_TYPE_INVALID)
        goto failure;
    }
  else
    {
      goto failure;
    }

  return TRUE;

failure:
  g_set_error_literal (error,
                       TMPL_ERROR,
                       TMPL_ERROR_RUNTIME_ERROR,
                       "Assertion failed");
  return FALSE;
}

#define BUILTIN_MATH(func)                                              \
static gboolean                                                         \
builtin_##func (const GValue  *value,                                   \
                GValue        *return_value,                            \
                GError       **error)                                   \
{                                                                       \
  GValue translated = G_VALUE_INIT;                                     \
                                                                        \
  if (!G_VALUE_HOLDS_DOUBLE (value))                                    \
    {                                                                   \
      g_value_init (&translated, G_TYPE_DOUBLE);                        \
      if (!g_value_transform (value, &translated))                      \
        {                                                               \
          g_set_error (error,                                           \
                       TMPL_ERROR,                                      \
                       TMPL_ERROR_RUNTIME_ERROR,                        \
                       "Cannot convert %s to double",                   \
                       G_VALUE_TYPE_NAME (value));                      \
          return FALSE;                                                 \
        }                                                               \
      value = &translated;                                              \
    }                                                                   \
                                                                        \
  g_value_init (return_value, G_TYPE_DOUBLE);                           \
  g_value_set_double (return_value, func (g_value_get_double (value))); \
  return TRUE;                                                          \
}

BUILTIN_MATH (ceil)
BUILTIN_MATH (floor)
BUILTIN_MATH (log)
BUILTIN_MATH (sqrt)
BUILTIN_MATH (sin)
BUILTIN_MATH (tan)
BUILTIN_MATH (cos)

#define BUILTIN_CAST(func,type)                   \
static gboolean                                   \
builtin_cast_##func (const GValue  *value,        \
                     GValue        *return_value, \
                     GError       **error)        \
{                                                 \
  g_value_init (return_value, type);              \
                                                  \
  if (!g_value_transform (value, return_value))   \
    {                                             \
      g_set_error (error,                         \
                   TMPL_ERROR,                    \
                   TMPL_ERROR_RUNTIME_ERROR,      \
                   "Cannot convert %s to %s",     \
                   G_VALUE_TYPE_NAME (value),     \
                   g_type_name (type));           \
      return FALSE;                               \
    }                                             \
                                                  \
  return TRUE;                                    \
}

BUILTIN_CAST (byte, G_TYPE_UCHAR)
BUILTIN_CAST (char, G_TYPE_CHAR)
BUILTIN_CAST (i32, G_TYPE_INT)
BUILTIN_CAST (u32, G_TYPE_UINT)
BUILTIN_CAST (i64, G_TYPE_INT64)
BUILTIN_CAST (u64, G_TYPE_UINT64)
BUILTIN_CAST (float, G_TYPE_FLOAT)
BUILTIN_CAST (double, G_TYPE_DOUBLE)

static gboolean
builtin_cast_bool (const GValue  *value,
                   GValue        *return_value,
                   GError       **error)
{
  g_value_init (return_value, G_TYPE_BOOLEAN);

#define BOOL_CAST(type, getter, compare) \
  else if (G_VALUE_HOLDS (value, type)) \
    g_value_set_boolean (return_value, g_value_get_##getter(value) != compare);

  if (0) {}
  BOOL_CAST (G_TYPE_BOOLEAN, boolean, FALSE)
  BOOL_CAST (G_TYPE_DOUBLE, double, .0)
  BOOL_CAST (G_TYPE_FLOAT, float, .0f)
  BOOL_CAST (G_TYPE_INT, int, 0)
  BOOL_CAST (G_TYPE_UINT, uint, 0)
  BOOL_CAST (G_TYPE_CHAR, schar, 0)
  BOOL_CAST (G_TYPE_UCHAR, uchar, 0)
  BOOL_CAST (G_TYPE_STRING, string, NULL)
  BOOL_CAST (G_TYPE_POINTER, pointer, NULL)
  else if (!g_value_transform (value, return_value))
    {
      g_set_error (error,
                   TMPL_ERROR,
                   TMPL_ERROR_RUNTIME_ERROR,
                   "Cannot convert %s to bool",
                   G_VALUE_TYPE_NAME (value));
      return FALSE;
    }

#undef BOOL_CAST

  return TRUE;
}

static gboolean
builtin_print (const GValue  *value,
               GValue        *return_value,
               GError       **error)
{
  gchar *repr;

  repr = tmpl_value_repr (value);
  g_print ("%s\n", repr);
  g_free (repr);

  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value, TRUE);

  return TRUE;
}

static gboolean
builtin_printerr (const GValue  *value,
                  GValue        *return_value,
                  GError       **error)
{
  gchar *repr;

  repr = tmpl_value_repr (value);
  g_printerr ("%s\n", repr);
  g_free (repr);

  g_value_init (return_value, G_TYPE_BOOLEAN);
  g_value_set_boolean (return_value, TRUE);

  return TRUE;
}

static gboolean
builtin_typeof (const GValue  *value,
                GValue        *return_value,
                GError       **error)
{
  g_value_init (return_value, G_TYPE_GTYPE);

  if (G_VALUE_HOLDS (value, TMPL_TYPE_BASE_INFO) &&
      g_value_get_boxed (value) != NULL &&
      GI_IS_REGISTERED_TYPE_INFO (g_value_get_boxed (value)))
    g_value_set_gtype (return_value,
                       gi_registered_type_info_get_g_type (g_value_get_boxed (value)));
  else if (G_VALUE_HOLDS_OBJECT (value) &&
           g_value_get_object (value) != NULL)
    g_value_set_gtype (return_value, G_OBJECT_TYPE (g_value_get_object (value)));
  else
    g_value_set_gtype (return_value, G_VALUE_TYPE (value));

  return TRUE;
}

static gboolean
builtin_hex (const GValue  *value,
             GValue        *return_value,
             GError       **error)
{
  if (G_VALUE_HOLDS_DOUBLE (value))
    {
      gchar *str = g_strdup_printf ("0x%" G_GINT64_MODIFIER "x",
                                    (gint64)g_value_get_double (value));
      g_value_init (return_value, G_TYPE_STRING);
      g_value_take_string (return_value, str);
      return TRUE;
    }

  throw_type_mismatch (error, value, NULL, "requires number parameter");

  return FALSE;
}

static gboolean
builtin_repr (const GValue  *value,
              GValue        *return_value,
              GError       **error)
{
  g_value_init (return_value, G_TYPE_STRING);
  g_value_take_string (return_value, tmpl_value_repr (value));
  return TRUE;
}
