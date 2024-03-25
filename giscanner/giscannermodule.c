/* GObject introspection: scanner
 *
 * Copyright (C) 2008  Johan Dahlin <johan@gnome.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the
 * Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 * Boston, MA 02111-1307, USA.
 *
 */

#ifdef HAVE_CONFIG_H
#  include "config.h"
#endif
#define PY_SSIZE_T_CLEAN
#include <Python.h>
#include "sourcescanner.h"

#include <glib-object.h>

/* forward declaration */
_GI_EXTERN PyMODINIT_FUNC PyInit__giscanner(void);

#define NEW_CLASS(ctype, name, cname, num_methods)	      \
static const PyMethodDef _Py##cname##_methods[num_methods] G_GNUC_UNUSED;    \
PyTypeObject Py##cname##_Type = {             \
    PyVarObject_HEAD_INIT(NULL, 0)            \
    .tp_name = "scanner." name,               \
    .tp_basicsize = sizeof(ctype),            \
    .tp_itemsize = 0,                         \
    .tp_alloc = PyType_GenericAlloc,          \
    .tp_new = PyType_GenericNew,              \
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE, \
}

#if PY_VERSION_HEX < 0x03090000
static inline int
PyModule_AddType(PyObject *module, PyTypeObject *type)
{
  if (PyType_Ready (type) < 0)
    return -1;

  const char *name = _PyType_Name(type);

  Py_INCREF (type);
  if (PyModule_AddObject (module, name, (PyObject *)type) < 0)
    {
      Py_DECREF (type);
      return -1;
    }

  return 0;
}
#endif

typedef struct {
  PyObject_HEAD
  GISourceType *type;
} PyGISourceType;

static PyObject * pygi_source_type_new (GISourceType *type);

typedef struct {
  PyObject_HEAD
  GISourceSymbol *symbol;
} PyGISourceSymbol;

typedef struct {
  PyObject_HEAD
  GISourceScanner *scanner;
} PyGISourceScanner;

NEW_CLASS (PyGISourceSymbol, "SourceSymbol", GISourceSymbol, 10);
NEW_CLASS (PyGISourceType, "SourceType", GISourceType, 9);
NEW_CLASS (PyGISourceScanner, "SourceScanner", GISourceScanner, 9);


/* Symbol */

static PyObject *
pygi_source_symbol_new (GISourceSymbol *symbol)
{
  PyGISourceSymbol *self;

  if (symbol == NULL)
    {
      Py_INCREF (Py_None);
      return Py_None;
    }

  self = (PyGISourceSymbol *)PyObject_New (PyGISourceSymbol,
					   &PyGISourceSymbol_Type);
  self->symbol = symbol;
  return (PyObject*)self;
}

static PyObject *
symbol_get_type (PyGISourceSymbol *self,
		 void             *context)
{
  return PyLong_FromLong (self->symbol->type);
}

static PyObject *
symbol_get_line (PyGISourceSymbol *self,
		 void             *context)
{
  return PyLong_FromLong (self->symbol->line);
}

static PyObject *
symbol_get_private (PyGISourceSymbol *self,
                    void             *context)
{
  return PyBool_FromLong (self->symbol->private);
}

static PyObject *
symbol_get_ident (PyGISourceSymbol *self,
		  void            *context)
{

  if (!self->symbol->ident)
    {
      Py_INCREF(Py_None);
      return Py_None;
    }

  return PyUnicode_FromString (self->symbol->ident);
}

static PyObject *
symbol_get_base_type (PyGISourceSymbol *self,
		      void             *context)
{
  return pygi_source_type_new (self->symbol->base_type);
}

static PyObject *
symbol_get_const_int (PyGISourceSymbol *self,
		      void             *context)
{
  if (!self->symbol->const_int_set)
    {
      Py_INCREF(Py_None);
      return Py_None;
    }

  if (self->symbol->const_int_is_unsigned)
    return PyLong_FromUnsignedLongLong ((unsigned long long)self->symbol->const_int);
  else
    return PyLong_FromLongLong ((long long)self->symbol->const_int);
}

static PyObject *
symbol_get_const_double (PyGISourceSymbol *self,
                         void             *context)
{
  if (!self->symbol->const_double_set)
    {
      Py_INCREF(Py_None);
      return Py_None;
    }
  return PyFloat_FromDouble (self->symbol->const_double);
}

static PyObject *
symbol_get_const_string (PyGISourceSymbol *self,
			 void             *context)
{
  if (!self->symbol->const_string)
    {
      Py_INCREF(Py_None);
      return Py_None;
    }

  return PyUnicode_FromString (self->symbol->const_string);
}

static PyObject *
symbol_get_const_boolean (PyGISourceSymbol *self,
			  void             *context)
{
  if (!self->symbol->const_boolean_set)
    {
      Py_INCREF(Py_None);
      return Py_None;
    }

  return PyBool_FromLong (self->symbol->const_boolean);
}

static PyObject *
symbol_get_source_filename (PyGISourceSymbol *self,
                            void             *context)
{
  if (!self->symbol->source_filename)
    {
      Py_INCREF(Py_None);
      return Py_None;
    }

  return PyUnicode_FromString (self->symbol->source_filename);
}

static const PyGetSetDef _PyGISourceSymbol_getsets[] = {
  /* int ref_count; */
  { .name = "type", .get = (getter)symbol_get_type, },
  /* int id; */
  { .name = "ident", .get = (getter)symbol_get_ident, },
  { .name = "base_type", .get = (getter)symbol_get_base_type, },
  /* gboolean const_int_set; */
  { .name = "const_int", .get = (getter)symbol_get_const_int, },
  /* gboolean const_double_set; */
  { .name = "const_double", .get = (getter)symbol_get_const_double, },
  { .name = "const_string", .get = (getter)symbol_get_const_string, },
  /* gboolean const_boolean_set; */
  { .name = "const_boolean", .get = (getter)symbol_get_const_boolean, },
  { .name = "source_filename", .get = (getter)symbol_get_source_filename, },
  { .name = "line", .get = (getter)symbol_get_line, },
  { .name = "private", .get = (getter)symbol_get_private, },
  { 0 }
};



/* Type */

static PyObject *
pygi_source_type_new (GISourceType *type)
{
  PyGISourceType *self;

  if (type == NULL)
    {
      Py_INCREF (Py_None);
      return Py_None;
    }

  self = (PyGISourceType *)PyObject_New (PyGISourceType,
					 &PyGISourceType_Type);
  self->type = type;
  return (PyObject*)self;
}

static PyObject *
type_get_type (PyGISourceType *self,
	       void           *context)
{
  return PyLong_FromLong (self->type->type);
}

static PyObject *
type_get_storage_class_specifier (PyGISourceType *self,
				  void           *context)
{
  return PyLong_FromLong (self->type->storage_class_specifier);
}

static PyObject *
type_get_type_qualifier (PyGISourceType *self,
			 void           *context)
{
  return PyLong_FromLong (self->type->type_qualifier);
}

static PyObject *
type_get_function_specifier (PyGISourceType *self,
			     void           *context)
{
  return PyLong_FromLong (self->type->function_specifier);
}

static PyObject *
type_get_name (PyGISourceType *self,
	       void           *context)
{
  if (!self->type->name)
    {
      Py_INCREF (Py_None);
      return Py_None;
    }

  return PyUnicode_FromString (self->type->name);
}

static PyObject *
type_get_base_type (PyGISourceType *self,
		    void           *context)
{
  return pygi_source_type_new (self->type->base_type);
}

static PyObject *
type_get_child_list (PyGISourceType *self,
		     void           *context)
{
  GList *l;
  PyObject *list;
  int i = 0;

  if (!self->type)
    return Py_BuildValue("[]");

  list = PyList_New (g_list_length (self->type->child_list));

  for (l = self->type->child_list; l; l = l->next)
    {
      PyObject *item = pygi_source_symbol_new (l->data);
      PyList_SetItem (list, i++, item);
    }

  Py_INCREF (list);
  return list;
}

static PyObject *
type_get_is_bitfield (PyGISourceType *self,
			     void           *context)
{
  return PyLong_FromLong (self->type->is_bitfield);
}

static const PyGetSetDef _PyGISourceType_getsets[] = {
  { .name = "type", .get = (getter)type_get_type, },
  { .name = "storage_class_specifier", .get = (getter)type_get_storage_class_specifier, },
  { .name = "type_qualifier", .get = (getter)type_get_type_qualifier, },
  { .name = "function_specifier", .get = (getter)type_get_function_specifier, },
  { .name = "name", .get = (getter)type_get_name, },
  { .name = "base_type", .get = (getter)type_get_base_type, },
  { .name = "child_list", .get = (getter)type_get_child_list, },
  { .name = "is_bitfield", .get = (getter)type_get_is_bitfield, },
  { 0 }
};



/* Scanner */

static int
pygi_source_scanner_init (PyGISourceScanner *self,
			  PyObject  	    *args,
			  PyObject 	    *kwargs)
{
  if (!PyArg_ParseTuple (args, ":SourceScanner.__init__"))
    return -1;

  self->scanner = gi_source_scanner_new ();

  return 0;
}

static PyObject *
pygi_source_scanner_append_filename (PyGISourceScanner *self,
				     PyObject          *args)
{
  char *filename;
  GFile *file;

  if (!PyArg_ParseTuple (args, "s:SourceScanner.append_filename", &filename))
    return NULL;

  file = g_file_new_for_path (filename);
  g_hash_table_add (self->scanner->files, file);

  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
pygi_source_scanner_parse_macros (PyGISourceScanner *self,
                                  PyObject          *args)
{
  GList *filenames;
  int i;
  PyObject *list;

  list = PyTuple_GET_ITEM (args, 0);

  if (!PyList_Check (list))
    {
      PyErr_SetString (PyExc_RuntimeError, "parse macro takes a list of filenames");
      return NULL;
    }

  filenames = NULL;
  for (i = 0; i < PyList_Size (list); ++i)
    {
      PyObject *obj;
      char *filename = NULL;

      obj = PyList_GetItem (list, i);
      if (PyUnicode_Check (obj))
        {
          PyObject *s = PyUnicode_AsUTF8String (obj);
          filename = g_strdup (PyBytes_AsString (s));
          Py_DECREF (s);
        }
      else if (PyBytes_Check (obj))
        {
          filename = g_strdup (PyBytes_AsString (obj));
        }

      if (filename == NULL)
        {
          PyErr_Format (PyExc_RuntimeError,
                        "Expected string but got %s",
                        (Py_TYPE(obj))->tp_name);
          g_list_free_full (filenames, g_free);
          return NULL;
        }

      filenames = g_list_append (filenames, filename);
    }

  gi_source_scanner_parse_macros (self->scanner, filenames);
  g_list_free_full (filenames, g_free);

  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
pygi_source_scanner_parse_file (PyGISourceScanner *self,
				PyObject          *args)
{
  char *filename;

  if (!PyArg_ParseTuple (args, "s:SourceScanner.parse_file", &filename))
    return NULL;

  if (!gi_source_scanner_parse_file (self->scanner, filename))
    {
      g_print ("Something went wrong during parsing.\n");
      return NULL;
    }

  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
pygi_source_scanner_lex_filename (PyGISourceScanner *self,
				  PyObject          *args)
{
  char *filename;
  GFile *file;

  if (!PyArg_ParseTuple (args, "s:SourceScanner.lex_filename", &filename))
    return NULL;

  self->scanner->current_file = g_file_new_for_path (filename);
  if (!gi_source_scanner_lex_filename (self->scanner, filename))
    {
      g_print ("Something went wrong during lexing.\n");
      return NULL;
    }
  file = g_file_new_for_path (filename);
  g_hash_table_add (self->scanner->files, file);

  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
pygi_source_scanner_set_macro_scan (PyGISourceScanner *self,
				    PyObject          *args)
{
  int macro_scan;

  if (!PyArg_ParseTuple (args, "b:SourceScanner.set_macro_scan", &macro_scan))
    return NULL;

  gi_source_scanner_set_macro_scan (self->scanner, macro_scan);

  Py_INCREF (Py_None);
  return Py_None;
}

static PyObject *
pygi_source_scanner_get_symbols (PyGISourceScanner *self, G_GNUC_UNUSED PyObject *unused)
{
  GPtrArray *symbols;
  PyObject *list;
  guint i = 0;

  symbols = gi_source_scanner_get_symbols (self->scanner);
  list = PyList_New (symbols->len);

  for (i = 0; i != symbols->len; ++i)
    {
      PyObject *item = pygi_source_symbol_new (g_ptr_array_index (symbols, i));
      PyList_SetItem (list, i, item);
    }

  return list;
}

static PyObject *
pygi_source_scanner_get_errors (PyGISourceScanner *self, G_GNUC_UNUSED PyObject *unused)
{
  GPtrArray *errors;
  PyObject *list;
  guint i = 0;

  errors = gi_source_scanner_get_errors (self->scanner);
  list = PyList_New (errors->len);

  for (i = 0; i != errors->len; ++i)
    {
      PyObject *item = PyUnicode_FromString (g_ptr_array_index (errors, i));
      PyList_SetItem (list, i, item);
    }

  return list;
}

static PyObject *
pygi_source_scanner_get_comments (PyGISourceScanner *self, G_GNUC_UNUSED PyObject *unused)
{
  GPtrArray *comments;
  PyObject *list;
  guint i = 0;

  comments = gi_source_scanner_get_comments (self->scanner);
  list = PyList_New (comments->len);

  for (i = 0; i != comments->len; ++i)
    {
      GISourceComment *comment = g_ptr_array_index (comments, i);
      PyObject *comment_obj;
      PyObject *filename_obj;
      PyObject *item;

      if (comment->comment)
        {
          comment_obj = PyUnicode_FromString (comment->comment);
          if (!comment_obj)
            {
              g_print ("Comment is not valid Unicode in %s line %d\n", comment->filename, comment->line);
              Py_INCREF (Py_None);
              comment_obj = Py_None;
            }
        }
      else
        {
          Py_INCREF (Py_None);
          comment_obj = Py_None;
        }

      if (comment->filename)
        {
          filename_obj = PyUnicode_FromString (comment->filename);
        }
      else
        {
          Py_INCREF (Py_None);
          filename_obj = Py_None;
        }

      item = Py_BuildValue ("(OOi)", comment_obj, filename_obj, comment->line);
      PyList_SetItem (list, i, item);

      Py_DECREF (comment_obj);
      Py_DECREF (filename_obj);
    }

  return list;
}

static const PyMethodDef _PyGISourceScanner_methods[] = {
  { "get_errors", (PyCFunction) pygi_source_scanner_get_errors, METH_NOARGS },
  { "get_comments", (PyCFunction) pygi_source_scanner_get_comments, METH_NOARGS },
  { "get_symbols", (PyCFunction) pygi_source_scanner_get_symbols, METH_NOARGS },
  { "append_filename", (PyCFunction) pygi_source_scanner_append_filename, METH_VARARGS },
  { "parse_file", (PyCFunction) pygi_source_scanner_parse_file, METH_VARARGS },
  { "parse_macros", (PyCFunction) pygi_source_scanner_parse_macros, METH_VARARGS },
  { "lex_filename", (PyCFunction) pygi_source_scanner_lex_filename, METH_VARARGS },
  { "set_macro_scan", (PyCFunction) pygi_source_scanner_set_macro_scan, METH_VARARGS },
  { NULL, NULL, 0 }
};

/* Module */

static struct PyModuleDef moduledef = {
	PyModuleDef_HEAD_INIT,
	.m_name = "giscanner._giscanner",
};


PyMODINIT_FUNC PyInit__giscanner(void)
{
    PyObject *m;

    m = PyModule_Create (&moduledef);

    PyGISourceScanner_Type.tp_init = (initproc)pygi_source_scanner_init;
    PyGISourceScanner_Type.tp_methods = (PyMethodDef*)_PyGISourceScanner_methods;
    if (PyModule_AddType(m, &PyGISourceScanner_Type) < 0)
      {
        Py_DECREF (m);
        return NULL;
      }

    PyGISourceSymbol_Type.tp_getset = (PyGetSetDef*)_PyGISourceSymbol_getsets;
    if (PyModule_AddType(m, &PyGISourceSymbol_Type) < 0)
      {
        Py_DECREF (m);
        return NULL;
      }

    PyGISourceType_Type.tp_getset = (PyGetSetDef*)_PyGISourceType_getsets;
    if (PyModule_AddType(m, &PyGISourceType_Type) < 0)
      {
        Py_DECREF (m);
        return NULL;
      }

    return m;
}
