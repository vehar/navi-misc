/* ----------------------------------------------------------------------------
 * This file was automatically generated by SWIG (http://www.swig.org).
 * Version 1.3.19
 * 
 * This file is not intended to be easily readable and contains a number of 
 * coding conventions designed to improve portability and efficiency. Do not make
 * changes to this file unless you know what you are doing--modify the SWIG 
 * interface file instead. 
 * ----------------------------------------------------------------------------- */

#define SWIGPYTHON

#include "Python.h"

/***********************************************************************
 * common.swg
 *
 *     This file contains generic SWIG runtime support for pointer
 *     type checking as well as a few commonly used macros to control
 *     external linkage.
 *
 * Author : David Beazley (beazley@cs.uchicago.edu)
 *
 * Copyright (c) 1999-2000, The University of Chicago
 * 
 * This file may be freely redistributed without license or fee provided
 * this copyright message remains intact.
 ************************************************************************/

#include <string.h>

#if defined(_WIN32) || defined(__WIN32__)
#       if defined(_MSC_VER)
#               if defined(STATIC_LINKED)
#                       define SWIGEXPORT(a) a
#                       define SWIGIMPORT(a) extern a
#               else
#                       define SWIGEXPORT(a) __declspec(dllexport) a
#                       define SWIGIMPORT(a) extern a
#               endif
#       else
#               if defined(__BORLANDC__)
#                       define SWIGEXPORT(a) a _export
#                       define SWIGIMPORT(a) a _export
#               else
#                       define SWIGEXPORT(a) a
#                       define SWIGIMPORT(a) a
#               endif
#       endif
#else
#       define SWIGEXPORT(a) a
#       define SWIGIMPORT(a) a
#endif

#ifdef SWIG_GLOBAL
#define SWIGRUNTIME(a) SWIGEXPORT(a)
#else
#define SWIGRUNTIME(a) static a
#endif

#ifdef __cplusplus
extern "C" {
#endif

typedef void *(*swig_converter_func)(void *);
typedef struct swig_type_info *(*swig_dycast_func)(void **);

typedef struct swig_type_info {
  const char             *name;                 
  swig_converter_func     converter;
  const char             *str;
  void                   *clientdata;	
  swig_dycast_func        dcast;
  struct swig_type_info  *next;
  struct swig_type_info  *prev;
} swig_type_info;

#ifdef SWIG_NOINCLUDE

SWIGIMPORT(swig_type_info *) SWIG_TypeRegister(swig_type_info *);
SWIGIMPORT(swig_type_info *) SWIG_TypeCheck(char *c, swig_type_info *);
SWIGIMPORT(void *)           SWIG_TypeCast(swig_type_info *, void *);
SWIGIMPORT(swig_type_info *) SWIG_TypeDynamicCast(swig_type_info *, void **);
SWIGIMPORT(const char *)     SWIG_TypeName(const swig_type_info *);
SWIGIMPORT(swig_type_info *) SWIG_TypeQuery(const char *);
SWIGIMPORT(void)             SWIG_TypeClientData(swig_type_info *, void *);

#else

static swig_type_info *swig_type_list = 0;

/* Register a type mapping with the type-checking */
SWIGRUNTIME(swig_type_info *)
SWIG_TypeRegister(swig_type_info *ti)
{
  swig_type_info *tc, *head, *ret, *next;
  /* Check to see if this type has already been registered */
  tc = swig_type_list;
  while (tc) {
    if (strcmp(tc->name, ti->name) == 0) {
      /* Already exists in the table.  Just add additional types to the list */
      if (tc->clientdata) ti->clientdata = tc->clientdata;	
      head = tc;
      next = tc->next;
      goto l1;
    }
    tc = tc->prev;
  }
  head = ti;
  next = 0;

  /* Place in list */
  ti->prev = swig_type_list;
  swig_type_list = ti;

  /* Build linked lists */
 l1:
  ret = head;
  tc = ti + 1;
  /* Patch up the rest of the links */
  while (tc->name) {
    head->next = tc;
    tc->prev = head;
    head = tc;
    tc++;
  }
  if (next) next->prev = head;  /**/
  head->next = next;
  return ret;
}

/* Check the typename */
SWIGRUNTIME(swig_type_info *) 
SWIG_TypeCheck(char *c, swig_type_info *ty)
{
  swig_type_info *s;
  if (!ty) return 0;        /* Void pointer */
  s = ty->next;             /* First element always just a name */
  do {
    if (strcmp(s->name,c) == 0) {
      if (s == ty->next) return s;
      /* Move s to the top of the linked list */
      s->prev->next = s->next;
      if (s->next) {
	s->next->prev = s->prev;
      }
      /* Insert s as second element in the list */
      s->next = ty->next;
      if (ty->next) ty->next->prev = s;
      ty->next = s;
      s->prev = ty;  /**/
      return s;
    }
    s = s->next;
  } while (s && (s != ty->next));
  return 0;
}

/* Cast a pointer up an inheritance hierarchy */
SWIGRUNTIME(void *) 
SWIG_TypeCast(swig_type_info *ty, void *ptr) 
{
  if ((!ty) || (!ty->converter)) return ptr;
  return (*ty->converter)(ptr);
}

/* Dynamic pointer casting. Down an inheritance hierarchy */
SWIGRUNTIME(swig_type_info *) 
SWIG_TypeDynamicCast(swig_type_info *ty, void **ptr) 
{
  swig_type_info *lastty = ty;
  if (!ty || !ty->dcast) return ty;
  while (ty && (ty->dcast)) {
     ty = (*ty->dcast)(ptr);
     if (ty) lastty = ty;
  }
  return lastty;
}

/* Return the name associated with this type */
SWIGRUNTIME(const char *)
SWIG_TypeName(const swig_type_info *ty) {
  return ty->name;
}

/* Search for a swig_type_info structure */
SWIGRUNTIME(swig_type_info *)
SWIG_TypeQuery(const char *name) {
  swig_type_info *ty = swig_type_list;
  while (ty) {
    if (ty->str && (strcmp(name,ty->str) == 0)) return ty;
    if (ty->name && (strcmp(name,ty->name) == 0)) return ty;
    ty = ty->prev;
  }
  return 0;
}

/* Set the clientdata field for a type */
SWIGRUNTIME(void)
SWIG_TypeClientData(swig_type_info *ti, void *clientdata) {
  swig_type_info *tc, *equiv;
  if (ti->clientdata == clientdata) return;
  ti->clientdata = clientdata;
  equiv = ti->next;
  while (equiv) {
    if (!equiv->converter) {
      tc = swig_type_list;
      while (tc) {
	if ((strcmp(tc->name, equiv->name) == 0))
	  SWIG_TypeClientData(tc,clientdata);
	tc = tc->prev;
      }
    }
    equiv = equiv->next;
  }
}
#endif

#ifdef __cplusplus
}

#endif

/***********************************************************************
 * python.swg
 *
 *     This file contains the runtime support for Python modules
 *     and includes code for managing global variables and pointer
 *     type checking.
 *
 * Author : David Beazley (beazley@cs.uchicago.edu)
 ************************************************************************/

#include "Python.h"

#ifdef __cplusplus
extern "C" {
#endif

#define SWIG_PY_INT     1
#define SWIG_PY_FLOAT   2
#define SWIG_PY_STRING  3
#define SWIG_PY_POINTER 4
#define SWIG_PY_BINARY  5

/* Flags for pointer conversion */

#define SWIG_POINTER_EXCEPTION     0x1
#define SWIG_POINTER_DISOWN        0x2

/* Exception handling in wrappers */
#define SWIG_fail   goto fail

/* Constant information structure */
typedef struct swig_const_info {
    int type;
    char *name;
    long lvalue;
    double dvalue;
    void   *pvalue;
    swig_type_info **ptype;
} swig_const_info;

#ifdef SWIG_NOINCLUDE

SWIGEXPORT(PyObject *)        SWIG_newvarlink(void);
SWIGEXPORT(void)              SWIG_addvarlink(PyObject *, char *, PyObject *(*)(void), int (*)(PyObject *));
SWIGEXPORT(int)               SWIG_ConvertPtr(PyObject *, void **, swig_type_info *, int);
SWIGEXPORT(int)               SWIG_ConvertPacked(PyObject *, void *, int sz, swig_type_info *, int);
SWIGEXPORT(char *)            SWIG_PackData(char *c, void *, int);
SWIGEXPORT(char *)            SWIG_UnpackData(char *c, void *, int);
SWIGEXPORT(PyObject *)        SWIG_NewPointerObj(void *, swig_type_info *,int own);
SWIGEXPORT(PyObject *)        SWIG_NewPackedObj(void *, int sz, swig_type_info *);
SWIGEXPORT(void)              SWIG_InstallConstants(PyObject *d, swig_const_info constants[]);
#else

/* -----------------------------------------------------------------------------
 * global variable support code.
 * ----------------------------------------------------------------------------- */

typedef struct swig_globalvar {   
  char       *name;                  /* Name of global variable */
  PyObject *(*get_attr)(void);       /* Return the current value */
  int       (*set_attr)(PyObject *); /* Set the value */
  struct swig_globalvar *next;
} swig_globalvar;

typedef struct swig_varlinkobject {
  PyObject_HEAD
  swig_globalvar *vars;
} swig_varlinkobject;

static PyObject *
swig_varlink_repr(swig_varlinkobject *v) {
  v = v;
  return PyString_FromString("<Global variables>");
}

static int
swig_varlink_print(swig_varlinkobject *v, FILE *fp, int flags) {
  swig_globalvar  *var;
  flags = flags;
  fprintf(fp,"Global variables { ");
  for (var = v->vars; var; var=var->next) {
    fprintf(fp,"%s", var->name);
    if (var->next) fprintf(fp,", ");
  }
  fprintf(fp," }\n");
  return 0;
}

static PyObject *
swig_varlink_getattr(swig_varlinkobject *v, char *n) {
  swig_globalvar *var = v->vars;
  while (var) {
    if (strcmp(var->name,n) == 0) {
      return (*var->get_attr)();
    }
    var = var->next;
  }
  PyErr_SetString(PyExc_NameError,"Unknown C global variable");
  return NULL;
}

static int
swig_varlink_setattr(swig_varlinkobject *v, char *n, PyObject *p) {
  swig_globalvar *var = v->vars;
  while (var) {
    if (strcmp(var->name,n) == 0) {
      return (*var->set_attr)(p);
    }
    var = var->next;
  }
  PyErr_SetString(PyExc_NameError,"Unknown C global variable");
  return 1;
}

statichere PyTypeObject varlinktype = {
  PyObject_HEAD_INIT(0)              
  0,
  (char *)"swigvarlink",                      /* Type name    */
  sizeof(swig_varlinkobject),         /* Basic size   */
  0,                                  /* Itemsize     */
  0,                                  /* Deallocator  */ 
  (printfunc) swig_varlink_print,     /* Print        */
  (getattrfunc) swig_varlink_getattr, /* get attr     */
  (setattrfunc) swig_varlink_setattr, /* Set attr     */
  0,                                  /* tp_compare   */
  (reprfunc) swig_varlink_repr,       /* tp_repr      */    
  0,                                  /* tp_as_number */
  0,                                  /* tp_as_mapping*/
  0,                                  /* tp_hash      */
};

/* Create a variable linking object for use later */
SWIGRUNTIME(PyObject *)
SWIG_newvarlink(void) {
  swig_varlinkobject *result = 0;
  result = PyMem_NEW(swig_varlinkobject,1);
  varlinktype.ob_type = &PyType_Type;    /* Patch varlinktype into a PyType */
  result->ob_type = &varlinktype;
  result->vars = 0;
  result->ob_refcnt = 0;
  Py_XINCREF((PyObject *) result);
  return ((PyObject*) result);
}

SWIGRUNTIME(void)
SWIG_addvarlink(PyObject *p, char *name,
	   PyObject *(*get_attr)(void), int (*set_attr)(PyObject *p)) {
  swig_varlinkobject *v;
  swig_globalvar *gv;
  v= (swig_varlinkobject *) p;
  gv = (swig_globalvar *) malloc(sizeof(swig_globalvar));
  gv->name = (char *) malloc(strlen(name)+1);
  strcpy(gv->name,name);
  gv->get_attr = get_attr;
  gv->set_attr = set_attr;
  gv->next = v->vars;
  v->vars = gv;
}

/* Pack binary data into a string */
SWIGRUNTIME(char *)
SWIG_PackData(char *c, void *ptr, int sz) {
  static char hex[17] = "0123456789abcdef";
  int i;
  unsigned char *u = (unsigned char *) ptr;
  register unsigned char uu;
  for (i = 0; i < sz; i++,u++) {
    uu = *u;
    *(c++) = hex[(uu & 0xf0) >> 4];
    *(c++) = hex[uu & 0xf];
  }
  return c;
}

/* Unpack binary data from a string */
SWIGRUNTIME(char *)
SWIG_UnpackData(char *c, void *ptr, int sz) {
  register unsigned char uu = 0;
  register int d;
  unsigned char *u = (unsigned char *) ptr;
  int i;
  for (i = 0; i < sz; i++, u++) {
    d = *(c++);
    if ((d >= '0') && (d <= '9'))
      uu = ((d - '0') << 4);
    else if ((d >= 'a') && (d <= 'f'))
      uu = ((d - ('a'-10)) << 4);
    d = *(c++);
    if ((d >= '0') && (d <= '9'))
      uu |= (d - '0');
    else if ((d >= 'a') && (d <= 'f'))
      uu |= (d - ('a'-10));
    *u = uu;
  }
  return c;
}

/* Convert a pointer value */
SWIGRUNTIME(int)
SWIG_ConvertPtr(PyObject *obj, void **ptr, swig_type_info *ty, int flags) {
  swig_type_info *tc;
  char  *c;
  static PyObject *SWIG_this = 0;
  int    newref = 0;
  PyObject  *pyobj = 0;

  if (!obj) return 0;
  if (obj == Py_None) {
    *ptr = 0;
    return 0;
  }
#ifdef SWIG_COBJECT_TYPES
  if (!(PyCObject_Check(obj))) {
    if (!SWIG_this)
      SWIG_this = PyString_FromString("this");
    pyobj = obj;
    obj = PyObject_GetAttr(obj,SWIG_this);
    newref = 1;
    if (!obj) goto type_error;
    if (!PyCObject_Check(obj)) {
      Py_DECREF(obj);
      goto type_error;
    }
  }  
  *ptr = PyCObject_AsVoidPtr(obj);
  c = (char *) PyCObject_GetDesc(obj);
  if (newref) Py_DECREF(obj);
  goto cobject;
#else
  if (!(PyString_Check(obj))) {
    if (!SWIG_this)
      SWIG_this = PyString_FromString("this");
    pyobj = obj;
    obj = PyObject_GetAttr(obj,SWIG_this);
    newref = 1;
    if (!obj) goto type_error;
    if (!PyString_Check(obj)) {
      Py_DECREF(obj);
      goto type_error;
    }
  } 
  c = PyString_AsString(obj);
  /* Pointer values must start with leading underscore */
  if (*c != '_') {
    *ptr = (void *) 0;
    if (strcmp(c,"NULL") == 0) {
      if (newref) { Py_DECREF(obj); }
      return 0;
    } else {
      if (newref) { Py_DECREF(obj); }
      goto type_error;
    }
  }
  c++;
  c = SWIG_UnpackData(c,ptr,sizeof(void *));
  if (newref) { Py_DECREF(obj); }
#endif

#ifdef SWIG_COBJECT_TYPES
cobject:
#endif

  if (ty) {
    tc = SWIG_TypeCheck(c,ty);
    if (!tc) goto type_error;
    *ptr = SWIG_TypeCast(tc,(void*) *ptr);
  }

  if ((pyobj) && (flags & SWIG_POINTER_DISOWN)) {
      PyObject *zero = PyInt_FromLong(0);
      PyObject_SetAttrString(pyobj,(char*)"thisown",zero);
      Py_DECREF(zero);
  }
  return 0;

type_error:
  if (flags & SWIG_POINTER_EXCEPTION) {
    if (ty) {
      char *temp = (char *) malloc(64+strlen(ty->name));
      sprintf(temp,"Type error. Expected %s", ty->name);
      PyErr_SetString(PyExc_TypeError, temp);
      free((char *) temp);
    } else {
      PyErr_SetString(PyExc_TypeError,"Expected a pointer");
    }
  }
  return -1;
}

/* Convert a packed value value */
SWIGRUNTIME(int)
SWIG_ConvertPacked(PyObject *obj, void *ptr, int sz, swig_type_info *ty, int flags) {
  swig_type_info *tc;
  char  *c;

  if ((!obj) || (!PyString_Check(obj))) goto type_error;
  c = PyString_AsString(obj);
  /* Pointer values must start with leading underscore */
  if (*c != '_') goto type_error;
  c++;
  c = SWIG_UnpackData(c,ptr,sz);
  if (ty) {
    tc = SWIG_TypeCheck(c,ty);
    if (!tc) goto type_error;
  }
  return 0;

type_error:

  if (flags) {
    if (ty) {
      char *temp = (char *) malloc(64+strlen(ty->name));
      sprintf(temp,"Type error. Expected %s", ty->name);
      PyErr_SetString(PyExc_TypeError, temp);
      free((char *) temp);
    } else {
      PyErr_SetString(PyExc_TypeError,"Expected a pointer");
    }
  }
  return -1;
}

/* Create a new pointer object */
SWIGRUNTIME(PyObject *)
SWIG_NewPointerObj(void *ptr, swig_type_info *type, int own) {
  PyObject *robj;
  if (!ptr) {
    Py_INCREF(Py_None);
    return Py_None;
  }
#ifdef SWIG_COBJECT_TYPES
  robj = PyCObject_FromVoidPtrAndDesc((void *) ptr, (char *) type->name, NULL);
#else
  {
    char result[1024];
    char *r = result;
    *(r++) = '_';
    r = SWIG_PackData(r,&ptr,sizeof(void *));
    strcpy(r,type->name);
    robj = PyString_FromString(result);
  }
#endif
  if (!robj || (robj == Py_None)) return robj;
  if (type->clientdata) {
    PyObject *inst;
    PyObject *args = Py_BuildValue((char*)"(O)", robj);
    Py_DECREF(robj);
    inst = PyObject_CallObject((PyObject *) type->clientdata, args);
    Py_DECREF(args);
    if (inst) {
      if (own) {
	PyObject *n = PyInt_FromLong(1);
	PyObject_SetAttrString(inst,(char*)"thisown",n);
	Py_DECREF(n);
      }
      robj = inst;
    }
  }
  return robj;
}

SWIGRUNTIME(PyObject *)
SWIG_NewPackedObj(void *ptr, int sz, swig_type_info *type) {
  char result[1024];
  char *r = result;
  if ((2*sz + 1 + strlen(type->name)) > 1000) return 0;
  *(r++) = '_';
  r = SWIG_PackData(r,ptr,sz);
  strcpy(r,type->name);
  return PyString_FromString(result);
}

/* Install Constants */
SWIGRUNTIME(void)
SWIG_InstallConstants(PyObject *d, swig_const_info constants[]) {
  int i;
  PyObject *obj;
  for (i = 0; constants[i].type; i++) {
    switch(constants[i].type) {
    case SWIG_PY_INT:
      obj = PyInt_FromLong(constants[i].lvalue);
      break;
    case SWIG_PY_FLOAT:
      obj = PyFloat_FromDouble(constants[i].dvalue);
      break;
    case SWIG_PY_STRING:
      obj = PyString_FromString((char *) constants[i].pvalue);
      break;
    case SWIG_PY_POINTER:
      obj = SWIG_NewPointerObj(constants[i].pvalue, *(constants[i]).ptype,0);
      break;
    case SWIG_PY_BINARY:
      obj = SWIG_NewPackedObj(constants[i].pvalue, constants[i].lvalue, *(constants[i].ptype));
      break;
    default:
      obj = 0;
      break;
    }
    if (obj) {
      PyDict_SetItemString(d,constants[i].name,obj);
      Py_DECREF(obj);
    }
  }
}

#endif

#ifdef __cplusplus
}
#endif








/* -------- TYPES TABLE (BEGIN) -------- */

#define  SWIGTYPE_p_f_p_gint_p_gint_gpointer__void swig_types[0] 
#define  SWIGTYPE_p_p_GError swig_types[1] 
#define  SWIGTYPE_p_GQuark swig_types[2] 
#define  SWIGTYPE_p_RsvgHandle swig_types[3] 
#define  SWIGTYPE_p_gpointer swig_types[4] 
#define  SWIGTYPE_p_gsize swig_types[5] 
#define  SWIGTYPE_p_gboolean swig_types[6] 
#define  SWIGTYPE_p_GdkPixbuf swig_types[7] 
#define  SWIGTYPE_p_guchar swig_types[8] 
#define  SWIGTYPE_p_RsvgSizeFunc swig_types[9] 
#define  SWIGTYPE_p_GDestroyNotify swig_types[10] 
static swig_type_info *swig_types[12];

/* -------- TYPES TABLE (END) -------- */


/*-----------------------------------------------
              @(target):= _librsvg.so
  ------------------------------------------------*/
#define SWIG_init    init_librsvg

#define SWIG_name    "_librsvg"

	#include <librsvg.h>

extern void (*RsvgSizeFunc)(gint *,gint *,gpointer);
extern void rsvg_set_default_dpi(double);
extern RsvgHandle *rsvg_handle_new(void);
extern void rsvg_handle_set_dpi(RsvgHandle *,gpointer,GDestroyNotify);
extern void rsvg_handle_set_size_callback(RsvgHandle *,RsvgSizeFunc,gpointer,GDestroyNotify);
extern gboolean rsvg_handle_write(RsvgHandle *,guchar const *,gsize,GError **);
extern gboolean rsvg_handle_close(RsvgHandle *,GError **);
extern GdkPixbuf *rsvg_handle_get_pixbuf(RsvgHandle *);
#ifdef __cplusplus
extern "C" {
#endif
static PyObject *_wrap_rsvg_error_quark(PyObject *self, PyObject *args) {
    PyObject *resultobj;
    GQuark result;
    
    if(!PyArg_ParseTuple(args,(char *)":rsvg_error_quark")) goto fail;
    result = rsvg_error_quark();
    
    {
        GQuark * resultptr;
        resultptr = (GQuark *) malloc(sizeof(GQuark));
        memmove(resultptr, &result, sizeof(GQuark));
        resultobj = SWIG_NewPointerObj((void *) resultptr, SWIGTYPE_p_GQuark, 1);
    }
    return resultobj;
    fail:
    return NULL;
}


static int _wrap_RsvgSizeFunc_set(PyObject *_val) {
    {
        void *temp;
        if ((SWIG_ConvertPtr(_val,(void **) &temp, SWIGTYPE_p_f_p_gint_p_gint_gpointer__void, SWIG_POINTER_EXCEPTION | SWIG_POINTER_DISOWN)) == -1) {
            PyErr_SetString(PyExc_TypeError, "C variable 'RsvgSizeFunc (void (*)(gint *,gint *,gpointer))'");
            return 1;
        }
        RsvgSizeFunc = (void (*)(gint *,gint *,gpointer)) temp;
    }
    return 0;
}


static PyObject *_wrap_RsvgSizeFunc_get() {
    PyObject *pyobj;
    
    pyobj = SWIG_NewPointerObj((void *) RsvgSizeFunc, SWIGTYPE_p_f_p_gint_p_gint_gpointer__void, 0);
    return pyobj;
}


static PyObject *_wrap_rsvg_set_default_dpi(PyObject *self, PyObject *args) {
    PyObject *resultobj;
    double arg1 ;
    
    if(!PyArg_ParseTuple(args,(char *)"d:rsvg_set_default_dpi",&arg1)) goto fail;
    rsvg_set_default_dpi(arg1);
    
    Py_INCREF(Py_None); resultobj = Py_None;
    return resultobj;
    fail:
    return NULL;
}


static PyObject *_wrap_rsvg_handle_new(PyObject *self, PyObject *args) {
    PyObject *resultobj;
    RsvgHandle *result;
    
    if(!PyArg_ParseTuple(args,(char *)":rsvg_handle_new")) goto fail;
    result = (RsvgHandle *)rsvg_handle_new();
    
    resultobj = SWIG_NewPointerObj((void *) result, SWIGTYPE_p_RsvgHandle, 0);
    return resultobj;
    fail:
    return NULL;
}


static PyObject *_wrap_rsvg_handle_set_dpi(PyObject *self, PyObject *args) {
    PyObject *resultobj;
    RsvgHandle *arg1 = (RsvgHandle *) 0 ;
    gpointer arg2 ;
    GDestroyNotify arg3 ;
    gpointer *argp2 ;
    GDestroyNotify *argp3 ;
    PyObject * obj0 = 0 ;
    PyObject * obj1 = 0 ;
    PyObject * obj2 = 0 ;
    
    if(!PyArg_ParseTuple(args,(char *)"OOO:rsvg_handle_set_dpi",&obj0,&obj1,&obj2)) goto fail;
    if ((SWIG_ConvertPtr(obj0,(void **) &arg1, SWIGTYPE_p_RsvgHandle,SWIG_POINTER_EXCEPTION | 0 )) == -1) SWIG_fail;
    if ((SWIG_ConvertPtr(obj1,(void **) &argp2, SWIGTYPE_p_gpointer,SWIG_POINTER_EXCEPTION) == -1)) SWIG_fail;
    arg2 = *argp2; 
    if ((SWIG_ConvertPtr(obj2,(void **) &argp3, SWIGTYPE_p_GDestroyNotify,SWIG_POINTER_EXCEPTION) == -1)) SWIG_fail;
    arg3 = *argp3; 
    rsvg_handle_set_dpi(arg1,arg2,arg3);
    
    Py_INCREF(Py_None); resultobj = Py_None;
    return resultobj;
    fail:
    return NULL;
}


static PyObject *_wrap_rsvg_handle_set_size_callback(PyObject *self, PyObject *args) {
    PyObject *resultobj;
    RsvgHandle *arg1 = (RsvgHandle *) 0 ;
    RsvgSizeFunc arg2 ;
    gpointer arg3 ;
    GDestroyNotify arg4 ;
    RsvgSizeFunc *argp2 ;
    gpointer *argp3 ;
    GDestroyNotify *argp4 ;
    PyObject * obj0 = 0 ;
    PyObject * obj1 = 0 ;
    PyObject * obj2 = 0 ;
    PyObject * obj3 = 0 ;
    
    if(!PyArg_ParseTuple(args,(char *)"OOOO:rsvg_handle_set_size_callback",&obj0,&obj1,&obj2,&obj3)) goto fail;
    if ((SWIG_ConvertPtr(obj0,(void **) &arg1, SWIGTYPE_p_RsvgHandle,SWIG_POINTER_EXCEPTION | 0 )) == -1) SWIG_fail;
    if ((SWIG_ConvertPtr(obj1,(void **) &argp2, SWIGTYPE_p_RsvgSizeFunc,SWIG_POINTER_EXCEPTION) == -1)) SWIG_fail;
    arg2 = *argp2; 
    if ((SWIG_ConvertPtr(obj2,(void **) &argp3, SWIGTYPE_p_gpointer,SWIG_POINTER_EXCEPTION) == -1)) SWIG_fail;
    arg3 = *argp3; 
    if ((SWIG_ConvertPtr(obj3,(void **) &argp4, SWIGTYPE_p_GDestroyNotify,SWIG_POINTER_EXCEPTION) == -1)) SWIG_fail;
    arg4 = *argp4; 
    rsvg_handle_set_size_callback(arg1,arg2,arg3,arg4);
    
    Py_INCREF(Py_None); resultobj = Py_None;
    return resultobj;
    fail:
    return NULL;
}


static PyObject *_wrap_rsvg_handle_write(PyObject *self, PyObject *args) {
    PyObject *resultobj;
    RsvgHandle *arg1 = (RsvgHandle *) 0 ;
    guchar *arg2 = (guchar *) 0 ;
    gsize arg3 ;
    GError **arg4 = (GError **) 0 ;
    gboolean result;
    gsize *argp3 ;
    PyObject * obj0 = 0 ;
    PyObject * obj1 = 0 ;
    PyObject * obj2 = 0 ;
    PyObject * obj3 = 0 ;
    
    if(!PyArg_ParseTuple(args,(char *)"OOOO:rsvg_handle_write",&obj0,&obj1,&obj2,&obj3)) goto fail;
    if ((SWIG_ConvertPtr(obj0,(void **) &arg1, SWIGTYPE_p_RsvgHandle,SWIG_POINTER_EXCEPTION | 0 )) == -1) SWIG_fail;
    if ((SWIG_ConvertPtr(obj1,(void **) &arg2, SWIGTYPE_p_guchar,SWIG_POINTER_EXCEPTION | 0 )) == -1) SWIG_fail;
    if ((SWIG_ConvertPtr(obj2,(void **) &argp3, SWIGTYPE_p_gsize,SWIG_POINTER_EXCEPTION) == -1)) SWIG_fail;
    arg3 = *argp3; 
    if ((SWIG_ConvertPtr(obj3,(void **) &arg4, SWIGTYPE_p_p_GError,SWIG_POINTER_EXCEPTION | 0 )) == -1) SWIG_fail;
    result = rsvg_handle_write(arg1,(guchar const *)arg2,arg3,arg4);
    
    {
        gboolean * resultptr;
        resultptr = (gboolean *) malloc(sizeof(gboolean));
        memmove(resultptr, &result, sizeof(gboolean));
        resultobj = SWIG_NewPointerObj((void *) resultptr, SWIGTYPE_p_gboolean, 1);
    }
    return resultobj;
    fail:
    return NULL;
}


static PyObject *_wrap_rsvg_handle_close(PyObject *self, PyObject *args) {
    PyObject *resultobj;
    RsvgHandle *arg1 = (RsvgHandle *) 0 ;
    GError **arg2 = (GError **) 0 ;
    gboolean result;
    PyObject * obj0 = 0 ;
    PyObject * obj1 = 0 ;
    
    if(!PyArg_ParseTuple(args,(char *)"OO:rsvg_handle_close",&obj0,&obj1)) goto fail;
    if ((SWIG_ConvertPtr(obj0,(void **) &arg1, SWIGTYPE_p_RsvgHandle,SWIG_POINTER_EXCEPTION | 0 )) == -1) SWIG_fail;
    if ((SWIG_ConvertPtr(obj1,(void **) &arg2, SWIGTYPE_p_p_GError,SWIG_POINTER_EXCEPTION | 0 )) == -1) SWIG_fail;
    result = rsvg_handle_close(arg1,arg2);
    
    {
        gboolean * resultptr;
        resultptr = (gboolean *) malloc(sizeof(gboolean));
        memmove(resultptr, &result, sizeof(gboolean));
        resultobj = SWIG_NewPointerObj((void *) resultptr, SWIGTYPE_p_gboolean, 1);
    }
    return resultobj;
    fail:
    return NULL;
}


static PyObject *_wrap_rsvg_handle_get_pixbuf(PyObject *self, PyObject *args) {
    PyObject *resultobj;
    RsvgHandle *arg1 = (RsvgHandle *) 0 ;
    GdkPixbuf *result;
    PyObject * obj0 = 0 ;
    
    if(!PyArg_ParseTuple(args,(char *)"O:rsvg_handle_get_pixbuf",&obj0)) goto fail;
    if ((SWIG_ConvertPtr(obj0,(void **) &arg1, SWIGTYPE_p_RsvgHandle,SWIG_POINTER_EXCEPTION | 0 )) == -1) SWIG_fail;
    result = (GdkPixbuf *)rsvg_handle_get_pixbuf(arg1);
    
    resultobj = SWIG_NewPointerObj((void *) result, SWIGTYPE_p_GdkPixbuf, 0);
    return resultobj;
    fail:
    return NULL;
}


static PyObject *_wrap_rsvg_handle_free(PyObject *self, PyObject *args) {
    PyObject *resultobj;
    RsvgHandle *arg1 = (RsvgHandle *) 0 ;
    PyObject * obj0 = 0 ;
    
    if(!PyArg_ParseTuple(args,(char *)"O:rsvg_handle_free",&obj0)) goto fail;
    if ((SWIG_ConvertPtr(obj0,(void **) &arg1, SWIGTYPE_p_RsvgHandle,SWIG_POINTER_EXCEPTION | 0 )) == -1) SWIG_fail;
    rsvg_handle_free(arg1);
    
    Py_INCREF(Py_None); resultobj = Py_None;
    return resultobj;
    fail:
    return NULL;
}


static PyMethodDef SwigMethods[] = {
	 { (char *)"rsvg_error_quark", _wrap_rsvg_error_quark, METH_VARARGS },
	 { (char *)"rsvg_set_default_dpi", _wrap_rsvg_set_default_dpi, METH_VARARGS },
	 { (char *)"rsvg_handle_new", _wrap_rsvg_handle_new, METH_VARARGS },
	 { (char *)"rsvg_handle_set_dpi", _wrap_rsvg_handle_set_dpi, METH_VARARGS },
	 { (char *)"rsvg_handle_set_size_callback", _wrap_rsvg_handle_set_size_callback, METH_VARARGS },
	 { (char *)"rsvg_handle_write", _wrap_rsvg_handle_write, METH_VARARGS },
	 { (char *)"rsvg_handle_close", _wrap_rsvg_handle_close, METH_VARARGS },
	 { (char *)"rsvg_handle_get_pixbuf", _wrap_rsvg_handle_get_pixbuf, METH_VARARGS },
	 { (char *)"rsvg_handle_free", _wrap_rsvg_handle_free, METH_VARARGS },
	 { NULL, NULL }
};


/* -------- TYPE CONVERSION AND EQUIVALENCE RULES (BEGIN) -------- */

static swig_type_info _swigt__p_f_p_gint_p_gint_gpointer__void[] = {{"_p_f_p_gint_p_gint_gpointer__void", 0, "void (*)(gint *,gint *,gpointer)", 0},{"_p_f_p_gint_p_gint_gpointer__void"},{0}};
static swig_type_info _swigt__p_p_GError[] = {{"_p_p_GError", 0, "GError **", 0},{"_p_p_GError"},{0}};
static swig_type_info _swigt__p_GQuark[] = {{"_p_GQuark", 0, "GQuark *", 0},{"_p_GQuark"},{0}};
static swig_type_info _swigt__p_RsvgHandle[] = {{"_p_RsvgHandle", 0, "RsvgHandle *", 0},{"_p_RsvgHandle"},{0}};
static swig_type_info _swigt__p_gpointer[] = {{"_p_gpointer", 0, "gpointer *", 0},{"_p_gpointer"},{0}};
static swig_type_info _swigt__p_gsize[] = {{"_p_gsize", 0, "gsize *", 0},{"_p_gsize"},{0}};
static swig_type_info _swigt__p_gboolean[] = {{"_p_gboolean", 0, "gboolean *", 0},{"_p_gboolean"},{0}};
static swig_type_info _swigt__p_GdkPixbuf[] = {{"_p_GdkPixbuf", 0, "GdkPixbuf *", 0},{"_p_GdkPixbuf"},{0}};
static swig_type_info _swigt__p_guchar[] = {{"_p_guchar", 0, "guchar *", 0},{"_p_guchar"},{0}};
static swig_type_info _swigt__p_RsvgSizeFunc[] = {{"_p_RsvgSizeFunc", 0, "RsvgSizeFunc *", 0},{"_p_RsvgSizeFunc"},{0}};
static swig_type_info _swigt__p_GDestroyNotify[] = {{"_p_GDestroyNotify", 0, "GDestroyNotify *", 0},{"_p_GDestroyNotify"},{0}};

static swig_type_info *swig_types_initial[] = {
_swigt__p_f_p_gint_p_gint_gpointer__void, 
_swigt__p_p_GError, 
_swigt__p_GQuark, 
_swigt__p_RsvgHandle, 
_swigt__p_gpointer, 
_swigt__p_gsize, 
_swigt__p_gboolean, 
_swigt__p_GdkPixbuf, 
_swigt__p_guchar, 
_swigt__p_RsvgSizeFunc, 
_swigt__p_GDestroyNotify, 
0
};


/* -------- TYPE CONVERSION AND EQUIVALENCE RULES (END) -------- */

static swig_const_info swig_const_table[] = {
{0}};

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C"
#endif
SWIGEXPORT(void) SWIG_init(void) {
    static PyObject *SWIG_globals = 0; 
    static int       typeinit = 0;
    PyObject *m, *d;
    int       i;
    if (!SWIG_globals) SWIG_globals = SWIG_newvarlink();
    m = Py_InitModule((char *) SWIG_name, SwigMethods);
    d = PyModule_GetDict(m);
    
    if (!typeinit) {
        for (i = 0; swig_types_initial[i]; i++) {
            swig_types[i] = SWIG_TypeRegister(swig_types_initial[i]);
        }
        typeinit = 1;
    }
    SWIG_InstallConstants(d,swig_const_table);
    
    PyDict_SetItemString(d,(char*)"cvar", SWIG_globals);
    SWIG_addvarlink(SWIG_globals,(char*)"RsvgSizeFunc",_wrap_RsvgSizeFunc_get, _wrap_RsvgSizeFunc_set);
}

