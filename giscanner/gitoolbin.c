#include "config.h"

#include <glib.h>

#define PY_SSIZE_T_CLEAN
#include <Python.h>

extern PyMODINIT_FUNC PyInit__giscanner(void);

static gboolean
gitool_fill_builtins (void)
{
	int rc;
	PyObject *builtins, *dict, *str;

	builtins = PyImport_ImportModule ("builtins");
	if (builtins == NULL)
		return FALSE;

	dict = PyModule_GetDict (builtins);
	if (dict == NULL)
		goto failure;

#define set_builtin(key, value) \
	str = PyUnicode_FromString ((value)); \
	if (str == NULL) \
		{ \
			g_critical ("Failed to set " key "builtin"); \
			goto failure; \
		} \
	rc = PyDict_SetItemString (dict, (key), str); \
	Py_DECREF (str); \
	if (rc < 0) \
		{ \
			g_critical ("Failed to set " key " builtin"); \
			goto failure; \
		}

	set_builtin ("DATADIR", GOBJECT_INTROSPECTION_DATADIR);
	set_builtin ("GIR_DIR", GIR_DIR);
	set_builtin ("GDUMP_PATH", GI_BUILD_DIR "/giscanner/gdump.c");

#undef set_builtin

	Py_DECREF (builtins);
	return TRUE;
failure:
	Py_DECREF (builtins);
	return FALSE;

}

static PyObject *
gitool_get_args (int argc, char *argv[])
{
	PyObject *args = PyList_New (argc - 1);
	for (int i = 0; i < argc; i++)
		{
			if (i == argc-1)
				{
					if (argv[i][0] == '@') // last item starts with @
						{
							PyObject *argmod, *argfun;
							PyObject *arg, *rspargs, *res;

							argmod = PyImport_ImportModule ("giscanner.rspfileargs");
							if (argmod == NULL)
								{
									g_critical ("Unable to load rsp argument parser");
									goto failure;
								}

							argfun = PyObject_GetAttrString (argmod, "get_rspfile_args");
							if (argfun == NULL || !PyCallable_Check (argfun))
								{
									g_critical ("Unable to find the rsp argument parser method");
									Py_DECREF (argmod);
									goto failure;
								}

							Py_DECREF (argmod);

							arg = PyUnicode_DecodeFSDefault (&argv[i][1]); // skip beginning @
							if (arg == NULL)
								{
									g_critical ("Failed decoding parameter \"%s\"", argv[i]);
									Py_DECREF (argfun);
									goto failure;
								}

							rspargs = PyObject_CallOneArg (argfun, arg);
							Py_DECREF (arg);
							Py_DECREF (argfun);
							if (rspargs == NULL)
								{
									g_critical ("Failed rsp param decoding failed");
									goto failure;
								}

							res = PyObject_CallMethod(args, "extend", "O", rspargs);
							g_assert (res == Py_None);

							Py_DECREF (res);
							Py_DECREF (rspargs);
						}
					else
						{
							PyObject *arg = PyUnicode_DecodeFSDefault (argv[i]);
							if (arg == NULL)
								{
									g_critical ("Failed decoding parameter \"%s\"", argv[i]);
									goto failure;
								}
							PyList_Append (args, arg);
						}
				}
			else
				{
					PyObject *arg = PyUnicode_DecodeFSDefault (argv[i]);
					if (arg == NULL)
						{
							g_critical ("Failed decoding parameter \"%s\"", argv[i]);
							goto failure;
						}
					PyList_SetItem (args, i, arg);
				}
		}

	return args;
failure:
	Py_DECREF (args);
	return NULL;
}

static struct gitool {
	const gchar* binary;
	const gchar* module;
	const gchar* func;
} gitools[] = {
	{ "g-ir-scanner", "giscanner.scannermain", "scanner_main" },
	{ "g-ir-annotation-tool", "giscanner.annotationmain", "annotation_main" }
};

int
main (int argc, char *argv[])
{
	gchar *basename;
	struct gitool *tool;
	int ret, rc;

	PyStatus status;
	PyConfig cfg;
	PyObject *path, *local;
	PyObject *native, *modules;
	PyObject *toolmod, *toolfun;
	PyObject *args, *res;

	if (argc < 1)
		abort ();
	basename = g_path_get_basename (argv[0]);

	tool = NULL;
	for (guint i = 0; i < G_N_ELEMENTS (gitools); i++)
		if (g_strcmp0(gitools[i].binary, basename) == 0)
			{
				tool = &gitools[i];
				break;
			}
	g_free (basename);
	if (tool == NULL)
		{
			g_critical ("Unsupported tool: \"%s\"", argv[0]);
			return 1;
		}

	PyImport_AppendInittab ("_giscanner", PyInit__giscanner);

	PyConfig_InitPythonConfig (&cfg);

	status = PyConfig_SetBytesString (&cfg, &cfg.program_name, argv[0]);
	if (PyStatus_Exception (status)) {
		PyConfig_Clear (&cfg);
		Py_ExitStatusException (status);
		abort ();
	}

	status = Py_InitializeFromConfig (&cfg);
	if (PyStatus_Exception (status))
		{
			g_critical ("Python initialization failed: %s", status.err_msg);
			PyConfig_Clear (&cfg);
			Py_ExitStatusException (status);
			abort ();
		}

	PyConfig_Clear (&cfg);

	if (!gitool_fill_builtins ())
		{
			ret = 1;
			goto err_post_init;
		}

	path = PySys_GetObject ("path");
	local = PyUnicode_FromString(GI_BUILD_DIR); // This is fine, as as this binary doesn't get installed
	PyList_Append (path, local);

	native = PyImport_ImportModule ("_giscanner");
	g_assert (native != NULL);
	modules = PyImport_GetModuleDict ();
	rc = PyDict_SetItemString (modules, "giscanner._giscanner", native);
	Py_DECREF (native);
	if (rc < 0)
		{
			ret = 1;
			goto err_post_init;
		}

	toolmod = PyImport_ImportModule (tool->module);
	if (toolmod == NULL)
		{
			g_critical ("Unable to load tool module");
			ret = 1;
			goto err_post_init;
		}

	toolfun = PyObject_GetAttrString (toolmod, tool->func);
	if (toolfun == NULL || !PyCallable_Check (toolfun))
		{
			g_critical ("Failed to find the tool main method");
			ret = 1;
			goto err_post_import;
		}

	args = gitool_get_args (argc, argv);
	if (args == NULL)
		{
			ret = 1;
			goto err_post_main_lookup;
		}

	res = PyObject_CallOneArg (toolfun, args);
	if (res == NULL)
		{
			/* giscanner is likely to already have printed an error message.
			 * don't g_critical an additional - more useless one - here.
			 */
			ret = 1;
			goto err_post_arg_parse;
		}
	ret = PyLong_AsLong(res);
	Py_DECREF (res);

err_post_arg_parse:
	Py_DECREF (args);
err_post_main_lookup:
	Py_DECREF (toolfun);
err_post_import:
        Py_DECREF (toolmod);
err_post_init:
	if (PyErr_Occurred ())
		PyErr_Print ();

	if (Py_FinalizeEx() < 0 && ret == 0)
		ret = 120;

	return ret;
}
