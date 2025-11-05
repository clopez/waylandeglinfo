/*
 * Copyright (C) 2008  Tunsgten Graphics,Inc.   All Rights Reserved.
 * Copyright (C) 2011  Benjamin Franzke
 * Copyright (C) 2011  Collabora, Ltd.
 * Copyright (C) 2016-2025  Igalia S.L.
 *
 * Permission to use, copy, modify, distribute, and sell this software and its
 * documentation for any purpose is hereby granted without fee, provided that
 * the above copyright notice appear in all copies and that both that copyright
 * notice and this permission notice appear in supporting documentation, and
 * that the name of the copyright holders not be used in advertising or
 * publicity pertaining to distribution of the software without specific,
 * written prior permission.  The copyright holders make no representations
 * about the suitability of this software for any purpose.  It is provided "as
 * is" without express or implied warranty.
 *
 * THE COPYRIGHT HOLDERS DISCLAIM ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
 * INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
 * EVENT SHALL THE COPYRIGHT HOLDERS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
 * CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE,
 * DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */


#define GL_GLEXT_PROTOTYPES

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include <stdbool.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#if defined(WAYLANDEGLINFO_USE_GLES) && defined(WAYLANDEGLINFO_USE_GL)
#error "Define exactly one of WAYLANDEGLINFO_USE_GLES or WAYLANDEGLINFO_USE_GL"
#endif

#if defined(WAYLANDEGLINFO_USE_GLES)
#include <GLES2/gl2.h>
#define GL_API_NAME "OpenGL ES"
// old implementations may not have the ES3_BIT, use the ES2 one which is equivalent
#ifndef EGL_OPENGL_ES3_BIT
#define EGL_OPENGL_ES3_BIT EGL_OPENGL_ES2_BIT
#endif
#elif defined(WAYLANDEGLINFO_USE_GL)
#include <GL/gl.h>
#define GL_API_NAME "OpenGL"
#else
#error "Build must define WAYLANDEGLINFO_USE_GLES or WAYLANDEGLINFO_USE_GL"
#endif

#include <EGL/egl.h>
#include <EGL/eglext.h>

#ifndef EGL_VERSION_1_5
#error "EGL 1.5 headers are required to build this tool"
#endif

#ifndef GL_NUM_EXTENSIONS
#define GL_NUM_EXTENSIONS 0x821D
#endif

#if defined(WAYLANDEGLINFO_USE_GL)
#include <GL/glext.h>  // for PFNGLGETSTRINGIPROC
#ifndef PFNGLGETSTRINGIPROC
typedef const GLubyte* (APIENTRYP PFNGLGETSTRINGIPROC)(GLenum, GLuint);
#endif
static PFNGLGETSTRINGIPROC pglGetStringi = NULL;
#endif

struct display {
    struct wl_display *display;
    struct wl_registry *registry;
    struct wl_compositor *compositor;
    struct {
        EGLDisplay dpy;
        EGLContext ctx;
        EGLConfig conf;
    } egl;
};

struct window {
    struct display *display;
    struct wl_surface *surface;
    struct wl_egl_window *native;
    EGLSurface egl_surface;
};

static const char *
egl_error_string(EGLint error)
{
    switch (error) {
        case EGL_SUCCESS:             return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED:     return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS:          return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC:           return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE:       return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT:         return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG:          return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY:         return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE:         return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH:           return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER:       return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP:   return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW:   return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST:        return "EGL_CONTEXT_LOST";
        default:                      return "Unknown EGL error";
    }
}

// Print a list of extensions, with word-wrapping.
static void
print_extension_list(const char *ext)
{
    const char indentString[] = "    ";
    const int indent = 4;
    const int max = 79;
    int width, i, j;

    if (!ext || !ext[0])
        return;

    width = indent;
    printf("%s", indentString);
    i = j = 0;
    while (1) {
        if (ext[j] == ' ' || ext[j] == 0) {
            // found end of an extension name
            const int len = j - i;
            if (width + len > max) {
                // start a new line
                printf("\n");
                width = indent;
                printf("%s", indentString);
            }
            // print the extension name between ext[i] and ext[j]
            while (i < j) {
                printf("%c", ext[i]);
                i++;
            }
            // either we're all done, or we'll continue with next extension
            width += len + 1;
            if (ext[j] == 0) {
                break;
            }
            else {
                i++;
                j++;
                if (ext[j] == 0)
                    break;
                printf(", ");
                width += 2;
            }
        }
        j++;
    }
    printf("\n");
}

static inline void get_and_print_egl_string(EGLDisplay dpy, EGLint name, const char *label)
{
    const char *s = eglQueryString(dpy, name);
    if (s) {
        if (name == EGL_EXTENSIONS) {
            printf("EGL_EXTENSIONS:\n");
            print_extension_list(s);
        } else {
            printf("%s = %s\n", label ? label : "(null-key)", s);
        }
    } else {
        EGLint err = eglGetError();
        printf("%s = (null) ; eglGetError=0x%04x (%s)\n",
               label ? label : "(null-key)", err, egl_error_string(err));
    }
}

#define PRINT_EGL_STRING(dpy, NAME) get_and_print_egl_string((dpy), (NAME), #NAME)

static inline void get_and_print_gl_string(GLenum name, const char *label)
{
    const char *s = (const char *)glGetString(name);
    if (s) {
        printf("%s = %s\n", label ? label : "(null-key)", s);
    } else {
        /* Likely no current context or invalid enum */
        GLenum err = glGetError(); /* consumes the error flag */
        printf("%s = (null) ; glGetError=0x%04x\n",
               label ? label : "(null-key)", (unsigned)err);
    }
}
#define PRINT_GL_STRING(NAME)  get_and_print_gl_string((NAME), #NAME)


static inline void print_gl_extensions() {
    GLint num_extensions = 0;
    bool found_extensions = false;
    const char *s;

    printf("GL_EXTENSIONS:\n");
    s = (const char *) glGetString(GL_EXTENSIONS);
    if (s) {
        // Old method (OpenGL < 3.0 or OpenGL ES)
        print_extension_list(s);
        found_extensions = true;
    }
#if defined(WAYLANDEGLINFO_USE_GL)
    else {
        GLint i;
        // For OpenGL 3.0+, use glGetStringi instead of glGetString
        glGetIntegerv(GL_NUM_EXTENSIONS, &num_extensions);
        if (num_extensions > 0) {
            if (!pglGetStringi)
                pglGetStringi = (PFNGLGETSTRINGIPROC) eglGetProcAddress("glGetStringi");
            if (!pglGetStringi) {
                fprintf(stderr, "glGetStringi not available; cannot enumerate GL extensions.\n");
            } else {
                const char indentString[] = "    ";
                const int indent = 4;
                const int max = 79;
                int width = indent;

                printf("%s", indentString);
                for (i = 0; i < num_extensions; i++) {
                    const char *ext = (const char *)pglGetStringi(GL_EXTENSIONS, i);
                    if (!ext)
                        continue;
                    int len = (int)strlen(ext);

                    if (width + len > max) {
                        printf("\n%s", indentString);
                        width = indent;
                    }

                    printf("%s", ext);
                    if (i < num_extensions - 1) {
                        printf(", ");
                        width += len + 2;
                    } else {
                        width += len;
                    }
                    found_extensions = true;
                }
                printf("\n");
            }
        }
    }
#endif

    if (!found_extensions)
        printf("WARNING: NO GL_EXTENSIONS Extensions found!\n");

}

static inline void print_egl_client_extensions(void) {
    const char *s = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    printf("EGL_CLIENT_EXTENSIONS:\n");
    if (s) print_extension_list(s); else printf("    (null)\n");
}

static void
info(EGLDisplay egl_dpy, EGLContext ctx)
{

    EGLint context_client_version_major = 0;
    EGLint context_client_version_minor = 0;

    PRINT_EGL_STRING(egl_dpy, EGL_VERSION);
    PRINT_EGL_STRING(egl_dpy, EGL_VENDOR);
    PRINT_EGL_STRING(egl_dpy, EGL_CLIENT_APIS);
    // Query the actual context version that was created
    eglQueryContext(egl_dpy, ctx, EGL_CONTEXT_MAJOR_VERSION, &context_client_version_major);
    printf("EGL_CONTEXT_MAJOR_VERSION = %d\n", context_client_version_major);
    eglQueryContext(egl_dpy, ctx, EGL_CONTEXT_MINOR_VERSION, &context_client_version_minor);
    printf("EGL_CONTEXT_MINOR_VERSION = %d\n", context_client_version_minor);
    PRINT_EGL_STRING(egl_dpy, EGL_EXTENSIONS);
    print_egl_client_extensions();

    printf("\n");
    PRINT_GL_STRING(GL_VERSION);
    PRINT_GL_STRING(GL_VENDOR);
    PRINT_GL_STRING(GL_RENDERER);
    PRINT_GL_STRING(GL_SHADING_LANGUAGE_VERSION);
    print_gl_extensions();
}


static bool
init_egl(struct display *display, EGLint egl_major, EGLint egl_minor)
{
    EGLint attribs[] = {
        EGL_RENDERABLE_TYPE, 0x0,
        EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
        EGL_RED_SIZE, 1,
        EGL_GREEN_SIZE, 1,
        EGL_BLUE_SIZE, 1,
        EGL_NONE
    };
    EGLint ctx_attribs[] = {
        EGL_CONTEXT_MAJOR_VERSION, 0,
        EGL_CONTEXT_MINOR_VERSION, 0,
        EGL_NONE
    };
    EGLint major, minor, n = 0;
    EGLBoolean ret;

    EGLenum EGL_API =
#if defined(WAYLANDEGLINFO_USE_GLES)
        EGL_OPENGL_ES_API;
#else
        EGL_OPENGL_API;
#endif

    display->egl.dpy = NULL;
    // try wayland platform display first
    EGLenum wayland_plat =
#ifdef EGL_PLATFORM_WAYLAND_KHR
        EGL_PLATFORM_WAYLAND_KHR;
#else
        EGL_PLATFORM_WAYLAND_EXT;
#endif

    const char *client_exts = eglQueryString(EGL_NO_DISPLAY, EGL_EXTENSIONS);
    if (client_exts && strstr(client_exts, "EGL_EXT_platform_base") && (strstr(client_exts, "EGL_KHR_platform_wayland") || strstr(client_exts, "EGL_EXT_platform_wayland"))) {
        display->egl.dpy = eglGetPlatformDisplay(wayland_plat, display->display, NULL);
    }

    // fallback to generic display
    if (!display->egl.dpy)
        display->egl.dpy = eglGetDisplay(display->display);
    if (!display->egl.dpy) {
        fprintf(stderr, "Error: eglGetDisplay() failed to connect to the EGL display.\n");
        return false;
    }

    ret = eglInitialize(display->egl.dpy, &major, &minor);
    if (ret != EGL_TRUE) {
        fprintf(stderr, "Error: eglInitialize() failed: %s\n", egl_error_string(eglGetError()));
        return false;
    }

    if ((major < 1) || (major == 1 && minor < 5)) {
        fprintf(stderr, "Error: supported EGL version \"%d.%d\" is lower than 1.5\n", major, minor);
        return false;
    }

    ret = eglBindAPI(EGL_API);
    if (ret != EGL_TRUE) {
        fprintf(stderr, "Error: eglBindAPI() failed: %s\n", egl_error_string(eglGetError()));
        return false;
    }

#if defined(WAYLANDEGLINFO_USE_GLES)
    switch (egl_major) {
        case 1:
            attribs[1] = EGL_OPENGL_ES_BIT;
            break;
        case 2:
            attribs[1] = EGL_OPENGL_ES2_BIT;
            break;
        case 3:
            attribs[1] = EGL_OPENGL_ES3_BIT;
            break;
        default:
            fprintf(stderr, "Error: Unsupported OpenGL ES major version %d\n", egl_major);
            return false;
    }
#else
    attribs[1] = EGL_OPENGL_BIT;
#endif

    ctx_attribs[1] = egl_major;
    ctx_attribs[3] = egl_minor;

    ret = eglChooseConfig(display->egl.dpy, attribs,
                          &display->egl.conf, 1, &n);
    if (!ret || n == 0) {
        fprintf(stderr, "Error: couldn't get an EGL visual config: %s\n", egl_error_string(eglGetError()));
        return false;
    }

    display->egl.ctx = eglCreateContext(display->egl.dpy,
                        display->egl.conf,
                        EGL_NO_CONTEXT, ctx_attribs);
    if (!display->egl.ctx) {
        fprintf(stderr, "Error: eglCreateContext failed: %s\n", egl_error_string(eglGetError()));
        return false;
    }
    return true;
}

static bool init_egl_surface(struct window *w)
{
    struct display *d = w->display;
    EGLBoolean ret;

    w->surface = wl_compositor_create_surface(d->compositor);
    if (!w->surface) {
        fprintf(stderr, "Wayland: wl_compositor_create_surface failed.\n");
        return false;
    }
    w->native = wl_egl_window_create(w->surface, 400, 300);
    if (!w->native) {
        fprintf(stderr, "Wayland: wl_egl_window_create failed.\n");
        return false;
    }
    w->egl_surface = eglCreateWindowSurface(d->egl.dpy,
                        d->egl.conf,
                        w->native, NULL);
    if (w->egl_surface == EGL_NO_SURFACE) {
        fprintf(stderr, "Error: eglCreateWindowSurface failed: %s\n", egl_error_string(eglGetError()));
        return false;
    }
    ret = eglMakeCurrent(d->egl.dpy, w->egl_surface, w->egl_surface,
                 d->egl.ctx);
    if (ret != EGL_TRUE) {
        fprintf(stderr, "Error: eglMakeCurrent failed: %s\n", egl_error_string(eglGetError()));
        return false;
    }
    return true;
}

static void destroy_egl_surface(struct window *w)
{
    struct display *d = w->display;
    if (d && d->egl.dpy) {
        eglMakeCurrent(d->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (w->egl_surface) {
            eglDestroySurface(d->egl.dpy, w->egl_surface);
            w->egl_surface = EGL_NO_SURFACE;
        }
    }
    if (w->native) {
        wl_egl_window_destroy(w->native);
        w->native = NULL;
    }
    if (w->surface) {
        wl_surface_destroy(w->surface);
         w->surface = NULL;
    }
}

static void
display_handle_global(void *data, struct wl_registry *registry, uint32_t id,
              const char *interface, uint32_t version)
{
    struct display *d = data;

    if (strcmp(interface, "wl_compositor") == 0) {
        d->compositor =
            wl_registry_bind(registry, id, &wl_compositor_interface, 1);
    }
}


static const struct wl_registry_listener registry_listener = { display_handle_global, NULL};


static void
print_help(const char *prog_name)
{
    printf("Usage: %s [OPTIONS]\n", prog_name);
    printf("\n");
    printf("%s Wayland info program\n", GL_API_NAME);
    printf("\n");
    printf("Options:\n");
    printf("  --glver VERSION  Specify %s Version (Default: 2.0)\n", GL_API_NAME);
    printf("  --help                Show this help message\n");
    printf("\n");
}

int
main(int argc, char *argv[])
{
    struct display display = { 0 };
    struct window window = { 0 };
    int major_version = 2;
    int minor_version = 0;
    int i;
    int rc = 1;

    // Parse command line arguments
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            print_help(argv[0] ? argv[0] : "program");
            return 0;
        } else if (strcmp(argv[i], "--glver") == 0) {
            if (i + 1 < argc) {
                if (sscanf(argv[++i], "%d.%d", &major_version, &minor_version) < 1) {
                    fprintf(stderr, "Error: Invalid version format '%s'. Use format like '2.0' or '3.1'\n", argv[i]);
                    return 1;
                }
            } else {
                fprintf(stderr, "Error: --glver requires an argument\n");
                print_help(argv[0] ? argv[0] : "program");
                return 1;
            }
        } else {
            fprintf(stderr, "Error: Unknown option '%s'\n", argv[i]);
            print_help(argv[0] ? argv[0] : "program");
            return 1;
        }
    }

    display.display = wl_display_connect(NULL);
    if (!display.display) {
        perror("Failed to connect to Wayland display");
        return 1;
    }
    window.display = &display;

    display.registry = wl_display_get_registry(display.display);

    wl_registry_add_listener(display.registry, &registry_listener, &display);

    wl_display_roundtrip(display.display);

    if (!display.compositor) {
        fprintf(stderr, "Wayland: wl_compositor not advertised/bound.\n");
        goto cleanup;
    }

    printf("=== %s %d.%d Info (Wayland) ===\n", GL_API_NAME, major_version, minor_version);

    if (!init_egl(&display, major_version, minor_version)) {
        fprintf(stderr, "Failed to initialize EGL. Please try passing another value for --glver or try with the -gl -es program variant\n");
        goto cleanup;
    }

    if (!init_egl_surface(&window))
        goto cleanup;
    info(display.egl.dpy, display.egl.ctx);
    rc = 0;


cleanup:
    if (window.egl_surface || window.native || window.surface)
        destroy_egl_surface(&window);
    if (display.egl.ctx)
        eglDestroyContext(display.egl.dpy, display.egl.ctx);
    if (display.egl.dpy)
        eglTerminate(display.egl.dpy);
    if (display.compositor)
        wl_compositor_destroy(display.compositor);
    if (display.registry)
        wl_registry_destroy(display.registry);
    if (display.display)
        wl_display_disconnect(display.display);
    return rc;
}
