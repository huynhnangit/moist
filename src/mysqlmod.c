/* -*- mode: C; indent-tabs-mode: t; c-basic-offset: 8; -*- */

#include "mysqlmod.h"

PyObject *_mysql_MySQLError;
 PyObject *_mysql_Warning;
 PyObject *_mysql_Error;
  PyObject *_mysql_DatabaseError;
  PyObject *_mysql_InterfaceError;
  PyObject *_mysql_DataError;
  PyObject *_mysql_OperationalError;
  PyObject *_mysql_IntegrityError;
  PyObject *_mysql_InternalError;
  PyObject *_mysql_ProgrammingError;
  PyObject *_mysql_NotSupportedError;
PyObject *_mysql_error_map;

int _mysql_server_init_done = 0;

PyObject *
_mysql_Exception(_mysql_ConnectionObject *c)
{
	PyObject *t, *e;
	int merr;

	if (!(t = PyTuple_New(2))) return NULL;
	if (!_mysql_server_init_done) {
		e = _mysql_InternalError;
		PyTuple_SET_ITEM(t, 0, PyInt_FromLong(-1L));
		PyTuple_SET_ITEM(t, 1, PyString_FromString("server not initialized"));
		PyErr_SetObject(e, t);
		Py_DECREF(t);
		return NULL;
	}
	merr = mysql_errno(&(c->connection));
	if (!merr)
		e = _mysql_InterfaceError;
	else if (merr > CR_MAX_ERROR) {
		PyTuple_SET_ITEM(t, 0, PyInt_FromLong(-1L));
		PyTuple_SET_ITEM(t, 1, PyString_FromString("error totally whack"));
		PyErr_SetObject(_mysql_InterfaceError, t);
		Py_DECREF(t);
		return NULL;
	}
	else {
		PyObject *py_merr = PyInt_FromLong(merr);
		e = PyDict_GetItem(_mysql_error_map, py_merr);
		Py_DECREF(py_merr);
		if (!e) {
			if (merr < 1000) e = _mysql_InternalError;
			else e = _mysql_OperationalError;
		}
	}
	PyTuple_SET_ITEM(t, 0, PyInt_FromLong((long)merr));
	PyTuple_SET_ITEM(t, 1, PyString_FromString(mysql_error(&(c->connection))));
	PyErr_SetObject(e, t);
	Py_DECREF(t);
	return NULL;
}

static char _mysql_server_init__doc__[] =
"Initialize embedded server. If this client is not linked against\n\
the embedded server library, this function does nothing.\n\
\n\
args -- sequence of command-line arguments\n\
groups -- sequence of groups to use in defaults files\n\
";

static PyObject *_mysql_server_init(
	PyObject *self,
	PyObject *args,
	PyObject *kwargs) {
	static char *kwlist[] = {"args", "groups", NULL};
	char **cmd_args_c=NULL, **groups_c=NULL, *s;
	Py_ssize_t cmd_argc=0, i, groupc;
	PyObject *cmd_args=NULL, *groups=NULL, *ret=NULL, *item;

	if (_mysql_server_init_done) {
		PyErr_SetString(_mysql_ProgrammingError,
				"already initialized");
		return NULL;
	}

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "|OO", kwlist,
					 &cmd_args, &groups))
		return NULL;

#if MYSQL_VERSION_ID >= 40000
	if (cmd_args) {
		if (!PySequence_Check(cmd_args)) {
			PyErr_SetString(PyExc_TypeError,
					"args must be a sequence");
			goto finish;
		}
		cmd_argc = PySequence_Size(cmd_args);
		if (cmd_argc == -1) {
			PyErr_SetString(PyExc_TypeError,
					"args could not be sized");
			goto finish;
		}
		cmd_args_c = (char **) PyMem_Malloc(cmd_argc*sizeof(char *));
		for (i=0; i< cmd_argc; i++) {
			item = PySequence_GetItem(cmd_args, i);
			s = PyString_AsString(item);
			Py_DECREF(item);
			if (!s) {
				PyErr_SetString(PyExc_TypeError,
						"args must contain strings");
				goto finish;
			}
			cmd_args_c[i] = s;
		}
	}
	if (groups) {
		if (!PySequence_Check(groups)) {
			PyErr_SetString(PyExc_TypeError,
					"groups must be a sequence");
			goto finish;
		}
		groupc = PySequence_Size(groups);
		if (groupc == -1) {
			PyErr_SetString(PyExc_TypeError,
					"groups could not be sized");
			goto finish;
		}
		groups_c = (char **) PyMem_Malloc((1+groupc)*sizeof(char *));
		for (i=0; i< groupc; i++) {
			item = PySequence_GetItem(groups, i);
			s = PyString_AsString(item);
			Py_DECREF(item);
			if (!s) {
				PyErr_SetString(PyExc_TypeError,
						"groups must contain strings");
				goto finish;
			}
			groups_c[i] = s;
		}
		groups_c[groupc] = (char *)NULL;
	}
	/* even though this may block, don't give up the interpreter lock
	   so that the server can't be initialized multiple times. */
	if (mysql_server_init(cmd_argc, cmd_args_c, groups_c)) {
		_mysql_Exception(NULL);
		goto finish;
	}
#endif
	ret = Py_None;
	Py_INCREF(Py_None);
	_mysql_server_init_done = 1;
  finish:
	PyMem_Free(groups_c);
	PyMem_Free(cmd_args_c);
	return ret;
}

static char _mysql_server_end__doc__[] =
"Shut down embedded server. If not using an embedded server, this\n\
does nothing.";

static PyObject *_mysql_server_end(
	PyObject *self,
	PyObject *unused) {
	if (_mysql_server_init_done) {
#if MYSQL_VERSION_ID >= 40000
		mysql_server_end();
#endif
		_mysql_server_init_done = 0;
		Py_INCREF(Py_None);
		return Py_None;
	}
	return _mysql_Exception(NULL);
}

#if MYSQL_VERSION_ID >= 32314
static char _mysql_thread_safe__doc__[] =
"Indicates whether the client is compiled as thread-safe.";

static PyObject *_mysql_thread_safe(
	PyObject *self,
	PyObject *unused) {

	check_server_init(NULL);
	return PyInt_FromLong((long)mysql_thread_safe());
}
#endif

extern char _mysql_connect__doc__[];
PyObject *
_mysql_connect(
	PyObject *self,
	PyObject *args,
	PyObject *kwargs);

static char _mysql_debug__doc__[] =
"Does a DBUG_PUSH with the given string.\n\
mysql_debug() uses the Fred Fish debug library.\n\
To use this function, you must compile the client library to\n\
support debugging.\n\
";
static PyObject *
_mysql_debug(
	PyObject *self,
	PyObject *args)
{
	char *debug;
	if (!PyArg_ParseTuple(args, "s", &debug)) return NULL;
	mysql_debug(debug);
	Py_INCREF(Py_None);
	return Py_None;
}

extern char _mysql_escape_string__doc__[];
PyObject *
_mysql_escape_string(
	_mysql_ConnectionObject *self,
	PyObject *args);

extern char _mysql_string_literal__doc__[];
PyObject *
_mysql_string_literal(
	_mysql_ConnectionObject *self,
	PyObject *args);

static PyObject *_mysql_NULL;

PyObject *
_escape_item(
	PyObject *item,
	PyObject *d)
{
	PyObject *quoted=NULL, *itemtype, *itemconv;
	if (!(itemtype = PyObject_Type(item)))
		goto error;
	itemconv = PyObject_GetItem(d, itemtype);
	Py_DECREF(itemtype);
	if (!itemconv) {
		PyErr_Clear();
		itemconv = PyObject_GetItem(d,
				 (PyObject *) &PyString_Type);
	}
	if (!itemconv) {
		PyErr_SetString(PyExc_TypeError,
				"no default type converter defined");
		goto error;
	}
	quoted = PyObject_CallFunction(itemconv, "OO", item, d);
	Py_DECREF(itemconv);
error:
	return quoted;
}

extern char _mysql_escape__doc__[];
PyObject *
_mysql_escape(
	PyObject *self,
	PyObject *args);


static char _mysql_get_client_info__doc__[] =
"get_client_info() -- Returns a string that represents\n\
the client library version.";
static PyObject *
_mysql_get_client_info(
	PyObject *self,
	PyObject *unused)
{
	check_server_init(NULL);
	return PyString_FromString(mysql_get_client_info());
}

extern PyTypeObject _mysql_ConnectionObject_Type;
extern PyTypeObject _mysql_ResultObject_Type;

static PyMethodDef
_mysql_methods[] = {
	{
		"connect",
		(PyCFunction)_mysql_connect,
		METH_VARARGS | METH_KEYWORDS,
		_mysql_connect__doc__
	},
	{
		"debug",
		(PyCFunction)_mysql_debug,
		METH_VARARGS,
		_mysql_debug__doc__
	},
	{
		"get_client_info",
		(PyCFunction)_mysql_get_client_info,
		METH_NOARGS,
		_mysql_get_client_info__doc__
	},
#if MYSQL_VERSION_ID >= 32314
	{
		"thread_safe",
		(PyCFunction)_mysql_thread_safe,
		METH_NOARGS,
		_mysql_thread_safe__doc__
	},
#endif
	{
		"server_init",
		(PyCFunction)_mysql_server_init,
		METH_VARARGS | METH_KEYWORDS,
		_mysql_server_init__doc__
	},
	{
		"server_end",
		(PyCFunction)_mysql_server_end,
		METH_NOARGS,
		_mysql_server_end__doc__
	},
	{NULL, NULL} /* sentinel */
};

static PyObject *
_mysql_NewException(
	PyObject *dict,
	PyObject *edict,
	char *name)
{
	PyObject *e;

	if (!(e = PyDict_GetItemString(edict, name)))
		return NULL;
	if (PyDict_SetItemString(dict, name, e)) return NULL;
	return e;
}

#define QUOTE(X) _QUOTE(X)
#define _QUOTE(X) #X

static char _mysql___doc__[] =
"an adaptation of the MySQL C API (mostly)\n\
\n\
You probably are better off using MySQLdb instead of using this\n\
module directly.\n\
\n\
In general, renaming goes from mysql_* to _mysql.*. _mysql.connect()\n\
returns a connection object (MYSQL). Functions which expect MYSQL * as\n\
an argument are now methods of the connection object. A number of things\n\
return result objects (MYSQL_RES). Functions which expect MYSQL_RES * as\n\
an argument are now methods of the result object. Deprecated functions\n\
(as of 3.23) are NOT implemented.\n\
";

PyMODINIT_FUNC
init_mysql(void)
{
	PyObject *dict, *module, *emod, *edict, *version_tuple;

	module = Py_InitModule3("_mysql", _mysql_methods, _mysql___doc__);
	if (!module)
		return; /* this really should never happen */

	/* Populate final object settings */
	_mysql_ConnectionObject_Type.ob_type = &PyType_Type;
	_mysql_ResultObject_Type.ob_type = &PyType_Type;
	_mysql_FieldObject_Type.ob_type = &PyType_Type;
	_mysql_ConnectionObject_Type.tp_alloc = PyType_GenericAlloc;
	_mysql_ConnectionObject_Type.tp_new = PyType_GenericNew;
	_mysql_ConnectionObject_Type.tp_free = _PyObject_GC_Del;
	_mysql_ResultObject_Type.tp_alloc = PyType_GenericAlloc;
	_mysql_ResultObject_Type.tp_new = PyType_GenericNew;
	_mysql_ResultObject_Type.tp_free = _PyObject_GC_Del;
	_mysql_FieldObject_Type.tp_alloc = PyType_GenericAlloc;
	_mysql_FieldObject_Type.tp_new = PyType_GenericNew;
	_mysql_FieldObject_Type.tp_free = _PyObject_GC_Del;

	if (!(dict = PyModule_GetDict(module)))
		goto error;

	/* Module constants */
	version_tuple = PyRun_String(QUOTE(version_info), Py_eval_input,
				     dict, dict);
	if (PyModule_AddObject(module, "version_info", version_tuple) < 0)
		goto error;
	if (PyModule_AddStringConstant(module, "__version__",
				       QUOTE(__version__)) < 0)
		goto error;
	if (PyModule_AddStringConstant(module, "NULL", "NULL") < 0)
		goto error;


	/* Register types */
	if (PyDict_SetItemString(dict, "connection",
			       (PyObject *)&_mysql_ConnectionObject_Type))
		goto error;
	Py_INCREF(&_mysql_ConnectionObject_Type);
	if (PyDict_SetItemString(dict, "result",
			       (PyObject *)&_mysql_ResultObject_Type))
		goto error;
	Py_INCREF(&_mysql_ResultObject_Type);
	if (PyDict_SetItemString(dict, "field",
			       (PyObject *)&_mysql_FieldObject_Type))
		goto error;
	Py_INCREF(&_mysql_FieldObject_Type);

	/* Reach into the exceptions module. */
	if (!(emod = PyImport_ImportModule("MySQLdb.exceptions")))
		goto error;
	if (!(edict = PyModule_GetDict(emod))) goto error;
	if (!(_mysql_MySQLError =
	      _mysql_NewException(dict, edict, "MySQLError")))
		goto error;
	if (!(_mysql_Warning =
	      _mysql_NewException(dict, edict, "Warning")))
		goto error;
	if (!(_mysql_Error =
	      _mysql_NewException(dict, edict, "Error")))
		goto error;
	if (!(_mysql_InterfaceError =
	      _mysql_NewException(dict, edict, "InterfaceError")))
		goto error;
	if (!(_mysql_DatabaseError =
	      _mysql_NewException(dict, edict, "DatabaseError")))
		goto error;
	if (!(_mysql_DataError =
	      _mysql_NewException(dict, edict, "DataError")))
		goto error;
	if (!(_mysql_OperationalError =
	      _mysql_NewException(dict, edict, "OperationalError")))
		goto error;
	if (!(_mysql_IntegrityError =
	      _mysql_NewException(dict, edict, "IntegrityError")))
		goto error;
	if (!(_mysql_InternalError =
	      _mysql_NewException(dict, edict, "InternalError")))
		goto error;
	if (!(_mysql_ProgrammingError =
	      _mysql_NewException(dict, edict, "ProgrammingError")))
		goto error;
	if (!(_mysql_NotSupportedError =
	      _mysql_NewException(dict, edict, "NotSupportedError")))
		goto error;
	if (!(_mysql_error_map = PyDict_GetItemString(edict, "error_map")))
		goto error;
	Py_DECREF(emod);

  error:
	if (PyErr_Occurred())
		PyErr_SetString(PyExc_ImportError,
				"_mysql: init failed");
	return;
}


