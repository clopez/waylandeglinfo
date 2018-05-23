/*
 * Copyright (C) 2008  Tunsgten Graphics,Inc.   All Rights Reserved.
 * Copyright (C) 2011  Benjamin Franzke
 * Copyright (C) 2011  Collabora, Ltd.
 * Copyright (C) 2016  Igalia S.L.
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

/*
 * List OpenGL ES extensions.
 * Print ES 1 or ES 2 extensions depending on the executable name.
 */
#define GL_GLEXT_PROTOTYPES

#include <assert.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <wayland-client.h>
#include <wayland-egl.h>

#include <GLES2/gl2.h>
#include <EGL/egl.h>

struct display {
	struct wl_display *display;
	struct wl_registry *registry;
	struct wl_compositor *compositor;
	struct {
		EGLDisplay dpy;
		EGLContext ctx;
		EGLConfig conf;
	} egl;
	uint32_t mask;
};

struct window {
	struct display *display;
	struct wl_surface *surface;
	struct wl_egl_window *native;
	EGLSurface egl_surface;
};

/*
 * Print a list of extensions, with word-wrapping.
 */
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
			/* found end of an extension name */
			const int len = j - i;
			if (width + len > max) {
				/* start a new line */
				printf("\n");
				width = indent;
				printf("%s", indentString);
			}
			/* print the extension name between ext[i] and ext[j] */
			while (i < j) {
				printf("%c", ext[i]);
				i++;
			}
			/* either we're all done, or we'll continue with next extension */
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


static void
info(EGLDisplay egl_dpy)
{
	const char *s;

	s = eglQueryString(egl_dpy, EGL_VERSION);
	printf("EGL_VERSION = %s\n", s);

	s = eglQueryString(egl_dpy, EGL_VENDOR);
	printf("EGL_VENDOR = %s\n", s);

	s = eglQueryString(egl_dpy, EGL_EXTENSIONS);
	printf("EGL_EXTENSIONS = %s\n", s);

	s = eglQueryString(egl_dpy, EGL_CLIENT_APIS);
	printf("EGL_CLIENT_APIS = %s\n", s);

	printf("GL_VERSION: %s\n", (char *) glGetString(GL_VERSION));
	printf("GL_RENDERER: %s\n", (char *) glGetString(GL_RENDERER));
	printf("GL_EXTENSIONS:\n");
	print_extension_list((char *) glGetString(GL_EXTENSIONS));
}


static void
init_egl(struct display *display, EGLint es_ver)
{
	EGLint attribs[] = {
		EGL_RENDERABLE_TYPE, 0x0,
		EGL_RED_SIZE, 1,
		EGL_GREEN_SIZE, 1,
		EGL_BLUE_SIZE, 1,
		EGL_NONE
	};
	EGLint ctx_attribs[] = {
		EGL_CONTEXT_CLIENT_VERSION, 0,
		EGL_NONE
	};

	EGLint major, minor, n = 0;
	EGLBoolean ret;

	display->egl.dpy = eglGetDisplay(display->display);
	if (!display->egl.dpy) {
		printf("Error: eglGetDisplay() failed\n");
		exit(1);
	}

	ret = eglInitialize(display->egl.dpy, &major, &minor);
	if (ret != EGL_TRUE) {
		printf("Error: eglInitialise() failed\n");
		exit(1);
	}

	ret = eglBindAPI(EGL_OPENGL_ES_API);
	if (ret != EGL_TRUE) {
		printf("Error: eglBindAPI() failed\n");
		exit(1);
	}

	if (es_ver == 1)
		attribs[1] = EGL_OPENGL_ES_BIT;
	else
		attribs[1] = EGL_OPENGL_ES2_BIT;
	ctx_attribs[1] = es_ver;

	ret = eglChooseConfig(display->egl.dpy, attribs,
	                      &display->egl.conf, 1, &n);
	if (!ret || n == 0) {
		printf("Error: couldn't get an EGL visual config\n");
		exit(1);
	}

	display->egl.ctx = eglCreateContext(display->egl.dpy,
					    display->egl.conf,
					    EGL_NO_CONTEXT, ctx_attribs);
	if (!display->egl.ctx) {
		printf("Error: eglCreateContext failed\n");
		exit(1);
	}
}

static void init_egl_surface(struct window *w)
{
	struct display *d = w->display;
	EGLBoolean ret;

	w->surface = wl_compositor_create_surface(d->compositor);
	w->native = wl_egl_window_create(w->surface, 400, 300);
	w->egl_surface = eglCreateWindowSurface(d->egl.dpy,
						d->egl.conf,
						w->native, NULL);
	ret = eglMakeCurrent(d->egl.dpy, w->egl_surface, w->egl_surface,
			     d->egl.ctx);
	if (ret != EGL_TRUE) {
		printf("Error: eglMakeCurrent failed\n");
		exit(1);
	}
}

static void destroy_egl_surface(struct window *w)
{
	struct display *d = w->display;
	eglMakeCurrent(d->egl.dpy, EGL_NO_SURFACE, EGL_NO_SURFACE,
		       EGL_NO_CONTEXT);
	if (w->egl_surface)
		eglDestroySurface(d->egl.dpy, w->egl_surface);
	if (w->native)
		wl_egl_window_destroy(w->native);
	wl_surface_destroy(w->surface);
	memset(w, 0, sizeof *w);
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

static int
event_mask_update(uint32_t mask, void *data)
{
	struct display *d = data;

	d->mask = mask;

	return 0;
}


static const struct wl_registry_listener registry_listener = {
       display_handle_global
};


int
main(int argc, char *argv[])
{
	struct display display = { 0 };
	struct window window = { 0 };
	EGLint es_ver = 1;

	/* decide the version from the executable's name */
	if (argc > 0 && argv[0] && strstr(argv[0], "es2"))
		es_ver = 2;

	display.display = wl_display_connect(NULL);
	if (!display.display) {
		fprintf(stderr, "failed to connect to display: %m\n");
		return 1;
	}
	window.display = &display;

	display.registry = wl_display_get_registry(display.display);

	wl_registry_add_listener(display.registry,
				       &registry_listener, &display);



	wl_display_get_fd(display.display);//, event_mask_update, &display);
	wl_display_dispatch(display.display);

	init_egl(&display, es_ver);
	init_egl_surface(&window);

	info(display.egl.dpy);

	destroy_egl_surface(&window);

	eglDestroyContext(display.egl.dpy, display.egl.ctx);
	eglTerminate(display.egl.dpy);
	wl_compositor_destroy(display.compositor);

	// segfaults? wl_display_destroy(display.display);

	return 0;
}
