/*

    EGL loader generated by glad 0.1.36 on Sun Jan 29 02:02:36 2023.

    Language/Generator: C/C++
    Specification: egl
    APIs: egl=1.4
    Profile: -
    Extensions:
        EGL_EXT_image_dma_buf_import,
        EGL_EXT_image_dma_buf_import_modifiers,
        EGL_KHR_gl_renderbuffer_image,
        EGL_KHR_gl_texture_2D_image,
        EGL_KHR_gl_texture_3D_image,
        EGL_KHR_image,
        EGL_KHR_image_base,
        EGL_KHR_image_pixmap,
        EGL_MESA_drm_image,
        EGL_MESA_image_dma_buf_export,
        EGL_MESA_query_driver,
        EGL_WL_bind_wayland_display,
        EGL_WL_create_wayland_buffer_from_image
    Loader: True
    Local files: True
    Omit khrplatform: False
    Reproducible: False

    Commandline:
        --api="egl=1.4" --generator="c" --spec="egl" --local-files --extensions="EGL_EXT_image_dma_buf_import,EGL_EXT_image_dma_buf_import_modifiers,EGL_KHR_gl_renderbuffer_image,EGL_KHR_gl_texture_2D_image,EGL_KHR_gl_texture_3D_image,EGL_KHR_image,EGL_KHR_image_base,EGL_KHR_image_pixmap,EGL_MESA_drm_image,EGL_MESA_image_dma_buf_export,EGL_MESA_query_driver,EGL_WL_bind_wayland_display,EGL_WL_create_wayland_buffer_from_image"
    Online:
        https://glad.dav1d.de/#language=c&specification=egl&loader=on&api=egl%3D1.4&extensions=EGL_EXT_image_dma_buf_import&extensions=EGL_EXT_image_dma_buf_import_modifiers&extensions=EGL_KHR_gl_renderbuffer_image&extensions=EGL_KHR_gl_texture_2D_image&extensions=EGL_KHR_gl_texture_3D_image&extensions=EGL_KHR_image&extensions=EGL_KHR_image_base&extensions=EGL_KHR_image_pixmap&extensions=EGL_MESA_drm_image&extensions=EGL_MESA_image_dma_buf_export&extensions=EGL_MESA_query_driver&extensions=EGL_WL_bind_wayland_display&extensions=EGL_WL_create_wayland_buffer_from_image
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <glad/glad_egl.h>

int gladLoadEGL(void) {
    return gladLoadEGLLoader((GLADloadproc)eglGetProcAddress);
}

PFNEGLQUERYDMABUFFORMATSEXTPROC glad_eglQueryDmaBufFormatsEXT = NULL;
PFNEGLQUERYDMABUFMODIFIERSEXTPROC glad_eglQueryDmaBufModifiersEXT = NULL;
PFNEGLCREATEIMAGEKHRPROC glad_eglCreateImageKHR = NULL;
PFNEGLDESTROYIMAGEKHRPROC glad_eglDestroyImageKHR = NULL;
PFNEGLCREATEDRMIMAGEMESAPROC glad_eglCreateDRMImageMESA = NULL;
PFNEGLEXPORTDRMIMAGEMESAPROC glad_eglExportDRMImageMESA = NULL;
PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC glad_eglExportDMABUFImageQueryMESA = NULL;
PFNEGLEXPORTDMABUFIMAGEMESAPROC glad_eglExportDMABUFImageMESA = NULL;
PFNEGLGETDISPLAYDRIVERCONFIGPROC glad_eglGetDisplayDriverConfig = NULL;
PFNEGLGETDISPLAYDRIVERNAMEPROC glad_eglGetDisplayDriverName = NULL;
PFNEGLBINDWAYLANDDISPLAYWLPROC glad_eglBindWaylandDisplayWL = NULL;
PFNEGLUNBINDWAYLANDDISPLAYWLPROC glad_eglUnbindWaylandDisplayWL = NULL;
PFNEGLQUERYWAYLANDBUFFERWLPROC glad_eglQueryWaylandBufferWL = NULL;
PFNEGLCREATEWAYLANDBUFFERFROMIMAGEWLPROC glad_eglCreateWaylandBufferFromImageWL = NULL;
static void load_EGL_EXT_image_dma_buf_import_modifiers(GLADloadproc load) {
	glad_eglQueryDmaBufFormatsEXT = (PFNEGLQUERYDMABUFFORMATSEXTPROC)load("eglQueryDmaBufFormatsEXT");
	glad_eglQueryDmaBufModifiersEXT = (PFNEGLQUERYDMABUFMODIFIERSEXTPROC)load("eglQueryDmaBufModifiersEXT");
}
static void load_EGL_KHR_image(GLADloadproc load) {
	glad_eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)load("eglCreateImageKHR");
	glad_eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)load("eglDestroyImageKHR");
}
static void load_EGL_KHR_image_base(GLADloadproc load) {
	glad_eglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)load("eglCreateImageKHR");
	glad_eglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)load("eglDestroyImageKHR");
}
static void load_EGL_MESA_drm_image(GLADloadproc load) {
	glad_eglCreateDRMImageMESA = (PFNEGLCREATEDRMIMAGEMESAPROC)load("eglCreateDRMImageMESA");
	glad_eglExportDRMImageMESA = (PFNEGLEXPORTDRMIMAGEMESAPROC)load("eglExportDRMImageMESA");
}
static void load_EGL_MESA_image_dma_buf_export(GLADloadproc load) {
	glad_eglExportDMABUFImageQueryMESA = (PFNEGLEXPORTDMABUFIMAGEQUERYMESAPROC)load("eglExportDMABUFImageQueryMESA");
	glad_eglExportDMABUFImageMESA = (PFNEGLEXPORTDMABUFIMAGEMESAPROC)load("eglExportDMABUFImageMESA");
}
static void load_EGL_MESA_query_driver(GLADloadproc load) {
	glad_eglGetDisplayDriverConfig = (PFNEGLGETDISPLAYDRIVERCONFIGPROC)load("eglGetDisplayDriverConfig");
	glad_eglGetDisplayDriverName = (PFNEGLGETDISPLAYDRIVERNAMEPROC)load("eglGetDisplayDriverName");
}
static void load_EGL_WL_bind_wayland_display(GLADloadproc load) {
	glad_eglBindWaylandDisplayWL = (PFNEGLBINDWAYLANDDISPLAYWLPROC)load("eglBindWaylandDisplayWL");
	glad_eglUnbindWaylandDisplayWL = (PFNEGLUNBINDWAYLANDDISPLAYWLPROC)load("eglUnbindWaylandDisplayWL");
	glad_eglQueryWaylandBufferWL = (PFNEGLQUERYWAYLANDBUFFERWLPROC)load("eglQueryWaylandBufferWL");
}
static void load_EGL_WL_create_wayland_buffer_from_image(GLADloadproc load) {
	glad_eglCreateWaylandBufferFromImageWL = (PFNEGLCREATEWAYLANDBUFFERFROMIMAGEWLPROC)load("eglCreateWaylandBufferFromImageWL");
}
static int find_extensionsEGL(void) {
	return 1;
}

static void find_coreEGL(void) {
}

int gladLoadEGLLoader(GLADloadproc load) {
	(void) load;
	find_coreEGL();

	if (!find_extensionsEGL()) return 0;
	load_EGL_EXT_image_dma_buf_import_modifiers(load);
	load_EGL_KHR_image(load);
	load_EGL_KHR_image_base(load);
	load_EGL_MESA_drm_image(load);
	load_EGL_MESA_image_dma_buf_export(load);
	load_EGL_MESA_query_driver(load);
	load_EGL_WL_bind_wayland_display(load);
	load_EGL_WL_create_wayland_buffer_from_image(load);
	return 1;
}
