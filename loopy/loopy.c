/* -*- Mode: C; c-basic-offset: 4; indent-tabs-mode: nil -*- */ static char* _doc =
"                                                                            \n"
" Loopy - the Linker-assisted OpenGL Overlay for Python                      \n"
" Copyright (C) 2005 Micah Dowty <micah@navi.cx>                             \n"
"                                                                            \n"
" Loopy is a way to embed Python code into most unmodified OpenGL programs,  \n"
" take control periodically, and draw your own graphics on top of the        \n"
" original output of the target program. Loopy does not provide OpenGL       \n"
" bindings of its own, but it is compatible with other Python modules that   \n"
" do, such as PyOpenGL or Soya 3D.                                           \n"
"                                                                            \n"
" Loopy has two main jobs: it must run arbitrary Python code in the address  \n"
" space of an unmodified application, and it must track enough of the OpenGL \n"
" state to prevent the overlay program from interfering with the target      \n"
" program or vice versa. It uses dynamic linker tricks to accomplish both    \n"
" of these goals. For this reason, it is only compatible with systems that   \n"
" run glibc2.                                                                \n"
"                                                                            \n"
" Loopy may be used as a normal Python module: 'import loopy'. This is       \n"
" handy for inspecting the built-in documentation with help() or pydoc, and  \n"
" it can be convenient for application development. To really get any work   \n"
" done, however, Loopy needs to be loaded inside the target application.     \n"
" From there Loopy starts a normal Python interpreter, registers the 'loopy' \n"
" module as a builtin, then runs your code. This is all set up using two     \n"
" environment variables. For example:                                        \n"
"                                                                            \n"
"     LD_PRELOAD=./loopy.so LOOPY_MAIN=hello.py glxgears                     \n"
"                                                                            \n"
" LD_PRELOAD instructs the dynamic linker to load Loopy itself, then Loopy   \n"
" runs your 'hello' Python program. Once 'hello' has finished executing, the \n"
" 'glxgears' target program starts running.                                  \n"
"                                                                            \n"
" It may seem like hello.py and glxgears would never be running at the same  \n"
" time. This is true- however hello.py can import the 'loopy' module and use \n"
" it to set up objects which later on affect the execution of glxgears and   \n"
" even re-enter Python code.                                                 \n"
"                                                                            \n"
" See the included examples and the rest of the built-in documentation for   \n"
" more information.                                                          \n"
"                                                                            \n"
"  This library is free software; you can redistribute it and/or             \n"
"  modify it under the terms of the GNU Lesser General Public                \n"
"  License as published by the Free Software Foundation; either              \n"
"  version 2.1 of the License, or (at your option) any later version.        \n"
"                                                                            \n"
"  This library is distributed in the hope that it will be useful,           \n"
"  but WITHOUT ANY WARRANTY; without even the implied warranty of            \n"
"  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU         \n"
"  Lesser General Public License for more details.                           \n"
"                                                                            \n"
"  You should have received a copy of the GNU Lesser General Public          \n"
"  License along with this library; if not, write to the Free Software       \n"
"  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA \n"
"                                                                            \n";

#include <Python.h>
#include <structmember.h>  /* PyMemberDef and friends */

#ifndef _GNU_SOURCE        /* Python.h might be setting this already   */
#define _GNU_SOURCE        /* RTLD_NEXT and other goodies              */
#endif
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#define GLSTATE_CAPABILITIES     (1  << 1 )
#define GLSTATE_TEXTURE_BINDING  (1  << 2 )
#define GLSTATE_COLOR            (1  << 3 )
#define GLSTATE_LIGHTS           (1  << 4 )
#define GLSTATE_MATERIALS        (1  << 5 )
#define GLSTATE_ALL              ((1 << 6 )-1)

typedef struct {
    PyObject_HEAD
    unsigned int trackingFlags;
    unsigned int restoreFlags;
    PyObject *capabilities;
    PyObject *textureBindings;
} GLState;

PyMODINIT_FUNC initloopy(void);
static GLState* glstate_new(void);

/* targetGLState tracks the state for our target application. It
 * is not created until just before control is transferred back
 * to the target app.
 */
static GLState *target_glstate = NULL;

/* currentGLState is the state associated with currently executing
 * code. While we're running the target app this should be another
 * reference to targetGLState. While we're running an overlay, it's
 * a reference to that overlay's GLState. Before the target app
 * begins or while Loopy is running internal OpenGL code, this must
 * be NULL.
 */
static GLState *current_glstate = NULL;

/* Pointers to dynamically-resolved original functions corresponding
 * to the functions we override in this library.
 */
static void* (*dlsym_p)(void*, __const char*) = NULL;
static void* (*glXGetProcAddress_p)(char *) = NULL;
static void* (*glXGetProcAddressARB_p)(char *) = NULL;
static void (*glXSwapBuffers_p)(void*, void*) = NULL;
static void (*glViewport_p)(int, int, int, int) = NULL;
static void (*glEnable_p)(int) = NULL;
static void (*glDisable_p)(int) = NULL;
static void (*glBindTexture_p)(int, int) = NULL;

/************************************************************************/
/********************************************** Dynamic Linker Mojo *****/
/************************************************************************/

/* A macro to resolve dlsym() itself at runtime. To avoid the
 * chicken-and-egg problem, this reads dlsym() as a versioned symbol
 * rather than a regular one.
 */
#define RESOLVE_DLSYM() \
    do { \
        if (!dlsym_p) \
            dlsym_p = dlvsym(RTLD_NEXT, "dlsym", "GLIBC_2.0"); \
    } while (0);

/* A macro to resolve one of the above original symbols at runtime */
#define RESOLVE(sym) \
    do { \
        if (!sym ## _p) { \
            RESOLVE_DLSYM(); \
            sym ## _p = dlsym_p(RTLD_NEXT, #sym); \
        } \
    } while (0);

/*
 * Handle python errors we can't pass back to the user module.
 * This includes errors importing the user module, and errors
 * in running hook functions.
 */
static void handle_py_error() {
    PyErr_Print();
    exit(1);
}

/*
 * This initialization function is called by the dynamic
 * linker- if we aren't already running inside python,
 * this sets up the interpreter and our extension module.
 */
static void __attribute__ ((constructor)) init() {
    char *main_filename;

    if (Py_IsInitialized())
        return;

    /* In Python 2.4 or later, we can use Py_InitializeEx() to
     * skip installing signal handlers- this lets us ctrl-C
     * the target app cleanly. If we don't have Python 2.4,
     * no big deal.
     */
#if (PY_VERSION_HEX >= 0x02040000)
    Py_InitializeEx(0);
#else
    Py_Initialize();
#endif

    /* Create the extension module */
    initloopy();

    /* LOOPY_MAIN specifies the name of a file to run code from.
     * It should nearly always be specified, but technically it's
     * optional.
     */
    main_filename = getenv("LOOPY_MAIN");
    if (main_filename) {
        FILE *main_file = fopen(main_filename, "r");
        if (!main_file) {
            perror(main_filename);
            exit(1);
        }

        /* Run the user program until it finishes */
        if (PyRun_SimpleFile(main_file, main_filename) < 0)
            handle_py_error();

        fclose(main_file);
    }

    /* We're about to start running the target app- allocate
     * a GLState and start tracking it.
     */
    current_glstate = target_glstate = glstate_new();
    Py_INCREF(current_glstate);
}

/* Any time the target application uses dlsym(), glXGetProcAddress(),
 * or glXGetProcAddressARB() we want to be able to override the resulting
 * symbol with one of ours dynamically. Returns a non-NULL pointer to
 * override the address of the specified symbol.
 */
static void *check_dynamic_symbol(const char *name) {
    /* Don't even try to look up non-OpenGL symbols.
     * When we search our own symbol namespace below,
     * we're searching a lot of libraries other than this
     * one and OpenGL: this can really screw up an
     * app's plugin loader, for example.
     */
    if (name[0] != 'g' || name[1] != 'l')
        return NULL;

    RESOLVE_DLSYM();
    return dlsym_p(RTLD_DEFAULT, name);
}

/************************************************************************/
/*************************************************** GLState Object *****/
/************************************************************************/

static PyMemberDef glstate_members[] = {
    { "trackingFlags", T_UINT, offsetof(GLState, trackingFlags), 0,
      "A set of flags indicating which aspects of the OpenGL\n"
      "state get tracked while this GLState is current.\n"
    },
    { "restoreFlags", T_UINT, offsetof(GLState, restoreFlags), 0,
      "A set of flags indicating which aspects of the OpenGL\n"
      "state are restored when this GLState becomes current.\n"
    },
    { "capabilities", T_OBJECT, offsetof(GLState, capabilities), READONLY,
      "A dictionary mapping OpenGL capability numbers to either\n"
      "True (glEnable) or False (glDisable). Missing capabilities\n"
      "haven't been touched by either glEnable or glDisable.\n"
    },
    { "textureBindings", T_OBJECT, offsetof(GLState, textureBindings), READONLY,
      "A dictionary mapping targets (like GL_TEXTURE_2D) to\n"
      "texture IDs. Missing targets haven't been bound to a texture\n"
      "ID. This will track traditional 1D/2D/3D targets, plus new\n"
      "targets defined by OpenGL extensions.\n"
    },
    {0}
};

static int glstate_init(GLState *self, PyObject *args, PyObject *kw) {
    if (!PyArg_ParseTuple(args, ""))
        return -1;

    self->trackingFlags = GLSTATE_ALL;
    self->restoreFlags = GLSTATE_ALL;

    self->capabilities = PyDict_New();
    self->textureBindings = PyDict_New();
    return 0;
}

static void glstate_dealloc(GLState *self) {
    Py_XDECREF(self->capabilities);
    Py_XDECREF(self->textureBindings);
    self->ob_type->tp_free((PyObject *)self);
}

static PyTypeObject glstate_type = {
    PyObject_HEAD_INIT(0)
    .tp_name = "loopy.GLState",
    .tp_basicsize = sizeof(GLState),
    .tp_flags = Py_TPFLAGS_DEFAULT,
    .tp_members = glstate_members,
    .tp_init = (initproc) glstate_init,
    .tp_dealloc = (destructor) glstate_dealloc,
    .tp_doc = "GLState()\n"
              "\n"
              "GLState instances represent a subset of the state\n"
              "stored in the current OpenGL context. When a GLState\n"
              "object is made current by the Loopy backend, it will\n"
              "be automatically updated to reflect changes to the real\n"
              "OpenGL state.\n"
              "\n"
              "The primary purpose of a GLState object is to provide a\n"
              "way to perform 'context switches' between the OpenGL\n"
              "environments expected by different applications. This\n"
              "is done by Loopy automatically behind the scenes.\n"
              "\n"
              "The Python interface to GLState can be used to control\n"
              "which parts of the OpenGL state get saved and restored,\n"
              "and to access or modify this saved state.\n"
};

static void glstate_add_const(const char *name, int value) {
    PyObject *py_value = PyInt_FromLong(value);
    PyDict_SetItemString(glstate_type.tp_dict, name, py_value);
    Py_DECREF(py_value);
}

static GLState* glstate_new(void) {
    GLState *self = PyObject_New(GLState, &glstate_type);
    PyObject *args = PyTuple_New(0);
    glstate_init(self, args, NULL);
    Py_DECREF(args);
    return self;
}

static void glstate_type_init(PyObject *module) {
    Py_INCREF(&glstate_type);

    glstate_type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&glstate_type) < 0)
        return;

    glstate_add_const("CAPABILITIES",     GLSTATE_CAPABILITIES);
    glstate_add_const("TEXTURE_BINDING",  GLSTATE_TEXTURE_BINDING);
    glstate_add_const("COLOR",            GLSTATE_COLOR);
    glstate_add_const("LIGHTS",           GLSTATE_LIGHTS);
    glstate_add_const("MATERIALS",        GLSTATE_MATERIALS);
    glstate_add_const("ALL",              GLSTATE_ALL);
    PyModule_AddObject(module, "GLState", (PyObject *) &glstate_type);
}

/************************************************************************/
/****************************************** OpenGL State Management *****/
/************************************************************************/

/* Perform an OpenGL state transition. The current_glstate as well as
 * the next state must be non-NULL. Normally current_glstate should
 * only be NULL while a switch is actually in progress.
 */
static void glstate_switch(GLState *next) {
    /* Steal current_glstate's reference, and signal that we
     * have a switch in progress.
     */
    GLState *previous = current_glstate;
    if (next == previous) return;
    current_glstate = NULL;

    /* FIXME */

    /* Complete the switch, drop the reference we stole from
     * current_glstate, then grab a new reference for our new
     * state.
     */
    Py_INCREF(next);
    Py_DECREF(previous);
    current_glstate = next;
}

/* Track changes to GL capabilities */
static __inline__ void glstate_track_capability(int capability, int status) {
    if (current_glstate &&
        (current_glstate->trackingFlags & GLSTATE_CAPABILITIES)) {

        PyObject *py_cap = PyInt_FromLong(capability);
        PyDict_SetItem(current_glstate->capabilities,
                       py_cap, status ? Py_True : Py_False);
        Py_DECREF(py_cap);
    }
}

/************************************************************************/
/*************************************************** Overlay Object *****/
/************************************************************************/

static PyObject *overlay_list;

struct ivector2 {
    int x, y;
};

typedef struct {
    PyObject_HEAD
    int enabled;
    GLState *glState;
    PyObject *py_resolution;

    /* Hidden from Python */
    int initialized;
    struct ivector2 c_resolution;
} Overlay;

static PyObject* return_none(PyObject* self) {
    Py_INCREF(Py_None);
    return Py_None;
}

static PyMethodDef overlay_methods[] = {
    { "render", (PyCFunction) return_none, METH_NOARGS,
      "render() -> None\n"
      "\n"
      "Subclasses override this function to render their OpenGL scene.\n"
      "For all enabled Overlay instances in Loopy.overlays, this is called\n"
      "just before the target application is allowed to swap buffers.\n"
      "The default implementation does nothing.\n"
    },
    { "setup", (PyCFunction) return_none, METH_NOARGS,
      "setup() -> None\n"
      "\n"
      "This is called only once, after the OpenGL context is valid and\n"
      "we know the output resolution, but before render() can be invoked.\n"
      "Subclasses override this to perform their 3D engine initialization.\n"
      "The default implementation does nothing.\n"
    },
    { "resized", (PyCFunction) return_none, METH_NOARGS,
      "resized() -> None\n"
      "\n"
      "This is called any time the output resolution changes after the\n"
      "initial call to setup(). The default implementation does nothing.\n"
    },
    {0}
};

static PyMemberDef overlay_members[] = {
    { "enabled", T_INT, offsetof(Overlay, enabled), 0,
      "This Overlay can only be run by Loopy when 'enabled' is nonzero.\n"
    },
    { "glState", T_OBJECT, offsetof(Overlay, glState), READONLY,
      "The GLState instance tracking OpenGL state changes that occur\n"
      "while this Overlay is running. It is automatically made current\n"
      "when hook functions begin running code from this Overlay.\n"
    },
    { "resolution", T_OBJECT, offsetof(Overlay, py_resolution), READONLY,
      "This is a (width, height) tuple containing the current output\n"
      "resolution, if it's known. If not, this is None. This is guaranteed\n"
      "to be valid before a call to setup() or resize().\n"
    },
    {0}
};

static int overlay_init(Overlay *self, PyObject *args, PyObject *kw) {
    if (!PyArg_ParseTuple(args, ""))
        return -1;

    self->glState = glstate_new();
    self->enabled = 1;
    return 0;
}

static void overlay_dealloc(Overlay *self) {
    Py_XDECREF(self->glState);
    self->ob_type->tp_free((PyObject *)self);
}

static PyTypeObject overlay_type = {
    PyObject_HEAD_INIT(0)
    .tp_name = "loopy.Overlay",
    .tp_basicsize = sizeof(Overlay),
    .tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
    .tp_members = overlay_members,
    .tp_methods = overlay_methods,
    .tp_init = (initproc) overlay_init,
    .tp_dealloc = (destructor) overlay_dealloc,
    .tp_doc = "Overlay()\n"
              "\n"
              "An Overlay is a partition created within the target\n"
              "application's OpenGL context, performing extra rendering\n"
              "after the target app renders each frame. Zero or more\n"
              "overlays can be stacked onto the target app, and each\n"
              "can be individually enabled or disabled.\n"
              "\n"
              "Subclass Overlay and override render() to start writing\n"
              "your own Loopy overlays. To install the overlay, append\n"
              "it to the Loopy.overlays list.\n"
};

static void overlay_type_init(PyObject *module) {
    Py_INCREF(&overlay_type);

    overlay_type.tp_new = PyType_GenericNew;
    if (PyType_Ready(&overlay_type) < 0)
        return;

    PyModule_AddObject(module, "Overlay", (PyObject *) &overlay_type);

    overlay_list = PyList_New(0);
    PyModule_AddObject(module, "overlays", overlay_list);
}

/*
 * Loop over all enabled overlays, with attention to errors
 * that might be caused by users. On failure, returns -1
 * with a Python exception. The callback may return -1 to
 * propagate a Python exception.
 */
static int foreach_overlay(int (*callback)(Overlay*, void*), void *user_data) {
    int i, result, n_overlays;

    n_overlays = PyList_Size(overlay_list);
    if (n_overlays < 0)
        return -1;

    for (i=0; i<n_overlays; i++) {
        /* Note that this could fail if the list changed size while
         * we're traversing it. It's safe to append overlays to the
         * list from within this loop, but reordering or deleting
         * entries will cause unexpected results.
         */
        Overlay *item = (Overlay*) PyList_GetItem(overlay_list, i);
        if (!item)
            return -1;

        if (!PyObject_IsInstance((PyObject*) item, (PyObject*) &overlay_type)) {
            PyErr_SetString(PyExc_TypeError,
                            "Only Overlay instances are allowed in Loopy.overlays");
            return -1;
        }

        if (!item->enabled)
            continue;

        /* Our item is a borrowed reference- hold onto it while
         * we're executing user code to avoid errors caused by
         * modifying the list.
         */
        Py_INCREF(item);
        result = callback(item, user_data);
        Py_DECREF(item);

        if (result < 0)
            return -1;
    }
    return 0;
}

/* If this overlay has been initialized, switch to
 * its GLState and render() it.
 */
static int overlay_render(Overlay *self, void* user_data) {
    PyObject *result;

    if (!self->initialized) {
        glstate_switch(self->glState);
        result = PyObject_CallMethod((PyObject*) self, "render", NULL);
        if (!result)
            return -1;
        Py_DECREF(result);
    }
    return 0;
}

/* Propagate a new screen resolution to this overlay.
 * If the overlay hasn't yet been initialized, it calls setup().
 * Otherwise we call resize() if and only if the resolution
 * has changed. user_data is an ivector2.
 */
static int overlay_resize(Overlay *self, void* user_data) {
    struct ivector2 *current_res = (struct ivector2*) user_data;
    PyObject *result;
    char *method;

    if (self->initialized) {
        if (current_res->x == self->c_resolution.x &&
            current_res->y == self->c_resolution.y)
            return 0;
        method = "resized";
    }
    else {
        method = "setup";
    }

    Py_XDECREF(self->py_resolution);
    self->c_resolution = *current_res;
    self->py_resolution = Py_BuildValue("(ii)", current_res->x, current_res->y);

    glstate_switch(self->glState);
    result = PyObject_CallMethod((PyObject*) self, method, NULL);
    if (!result)
        return -1;
    Py_DECREF(result);

    self->initialized = 1;
    return 0;
}

/************************************************************************/
/**************************************************** Python Module *****/
/************************************************************************/

PyObject *get_target_glstate(PyObject *self) {
    if (target_glstate) {
        Py_INCREF(target_glstate);
        return (PyObject*) target_glstate;
    }
    else {
        Py_INCREF(Py_None);
        return Py_None;
    }
}

static PyMethodDef module_methods[] = {
    { "getTargetGLState", (PyCFunction) get_target_glstate, METH_NOARGS,
      "getTargetGLState() -> GLState\n"
      "\n"
      "Return the GLState used for tracking the target application.\n"
      "If Loopy is running without a target application, or the target\n"
      "hasn't started executing yet, this returns None.\n"
    },
    {0}
};

/*
 * Initialize the 'loopy' extension module. This is normally called
 * internally for the Python interpreter we embed in the target app,
 * but this library can also be used as a normal extension module.
 */
PyMODINIT_FUNC initloopy(void)
{
    PyObject *m = Py_InitModule3("loopy", module_methods, _doc);

    glstate_type_init(m);
    overlay_type_init(m);
}

/************************************************************************/
/*********************************************** Override Functions *****/
/************************************************************************/

void glXSwapBuffers(void *display, void *drawable) {
    PyObject *result;
    int i, len;

    /* Give each overlay a chance to run, then swap back */
    if (foreach_overlay(overlay_render, NULL) < 0)
        handle_py_error();
    glstate_switch(target_glstate);

    RESOLVE(glXSwapBuffers);
    glXSwapBuffers_p(display, drawable);
}

void glViewport(int x, int y, int width, int height) {
    PyObject *result;

    RESOLVE(glViewport);
    glViewport_p(x, y, width, height);

    /* This isn't fool-proof, but we detect the current
     * window resolution by looking for glViewport() calls
     * with x,y == 0,0. If this turns out to be a problem,
     * the only alternative might be hooking Xlib. Yech.
     */
    if (x==0 && y==0) {
        struct ivector2 res = {width, height};
        if (foreach_overlay(overlay_resize, &res) < 0)
            handle_py_error();
        glstate_switch(target_glstate);
    }
}

void glEnable(int cap) {
    RESOLVE(glEnable);
    glEnable_p(cap);
    glstate_track_capability(cap, 1);
}

void glDisable(int cap) {
    RESOLVE(glDisable);
    glDisable_p(cap);
    glstate_track_capability(cap, 0);
}

void glBindTexture(int target, int texture) {
    RESOLVE(glBindTexture);
    glBindTexture_p(target, texture);
    //    glstate_track_texture_binding(target, texture);
}

void glBindTextureEXT(int target, int texture) {
    glBindTexture(target, texture);
}

void *dlsym (void *__restrict handle, __const char *__restrict name) {
    void *override = check_dynamic_symbol(name);
    if (override)
        return override;
    RESOLVE_DLSYM();
    return dlsym_p(handle, name);
}

void *glXGetProcAddress(char *name) {
    void *override = check_dynamic_symbol(name);
    if (override)
        return override;
    RESOLVE(glXGetProcAddress);
    return glXGetProcAddress_p(name);
}

void *glXGetProcAddressARB(char *name) {
    void *override = check_dynamic_symbol(name);
    if (override)
        return override;
    RESOLVE(glXGetProcAddressARB);
    return glXGetProcAddressARB_p(name);
}

/* The End */