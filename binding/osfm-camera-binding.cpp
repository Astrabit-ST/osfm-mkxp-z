
#include "binding-util.h"
#include "bitmap.h"
#include "graphics.h"

#include <SDL3/SDL.h>

typedef SDL_Camera *Camera;

void free_camera(void *camera) { SDL_CloseCamera((Camera)camera); }

DEF_TYPE_CUSTOMFREE(Camera, free_camera);

RB_METHOD(camera_get_cameras) {
  RB_UNUSED_PARAM;

  int count = 0;
  SDL_CameraID *ids = SDL_GetCameras(&count);

  VALUE array = rb_ary_new_capa(count);
  for (int i = 0; i < count; i++)
    rb_ary_push(array, INT2NUM(ids[i]));
  SDL_free(ids);

  return array;
}

RB_METHOD(camera_get_drivers) {
  RB_UNUSED_PARAM;

  int count = SDL_GetNumCameraDrivers();

  VALUE array = rb_ary_new_capa(count);
  for (int i = 0; i < count; i++) {
    const char *driver = SDL_GetCameraDriver(i);
    rb_ary_push(array, rb_str_new_cstr(driver));
  }

  return array;
}

RB_METHOD(camera_get_current_driver) {
  RB_UNUSED_PARAM;

  const char *driver = SDL_GetCurrentCameraDriver();

  return rb_str_new_cstr(driver);
}

#define GUARD_DISPOSED(w)                                                      \
  if (!w)                                                                      \
    rb_raise(rb_eRuntimeError, "Camera already closed!");

RB_METHOD(camera_get_name) {
  RB_UNUSED_PARAM;

  unsigned int id;
  rb_get_args(argc, argv, "i", &id RB_ARG_END);

  const char *name = SDL_GetCameraName(id);
  return rb_str_new_cstr(name);
}

RB_METHOD(camera_get_position) {
  RB_UNUSED_PARAM;

  unsigned int id;
  rb_get_args(argc, argv, "i", &id RB_ARG_END);

  int position = SDL_GetCameraPosition(id);

  return INT2NUM(position);
}

VALUE spec_to_hash(SDL_CameraSpec *spec) {
  VALUE hash = rb_hash_new();
  rb_hash_aset(hash, ID2SYM(rb_intern("format")), INT2NUM(spec->format));
  rb_hash_aset(hash, ID2SYM(rb_intern("colorspace")),
               INT2NUM(spec->colorspace));
  rb_hash_aset(hash, ID2SYM(rb_intern("width")), INT2NUM(spec->width));
  rb_hash_aset(hash, ID2SYM(rb_intern("height")), INT2NUM(spec->height));
  rb_hash_aset(hash, ID2SYM(rb_intern("framerate_numerator")),
               INT2NUM(spec->framerate_numerator));
  rb_hash_aset(hash, ID2SYM(rb_intern("framerate_denominator")),
               INT2NUM(spec->framerate_denominator));
  return hash;
}

SDL_CameraSpec spec_from_hash(VALUE hash) {
  SDL_CameraSpec spec;

  VALUE format = rb_hash_aref(hash, ID2SYM(rb_intern("format")));
  spec.format = (SDL_PixelFormat)NUM2INT(format);
  VALUE colorspace = rb_hash_aref(hash, ID2SYM(rb_intern("colorspace")));
  spec.colorspace = (SDL_Colorspace)NUM2INT(colorspace);
  VALUE width = rb_hash_aref(hash, ID2SYM(rb_intern("width")));
  spec.width = NUM2INT(width);
  VALUE height = rb_hash_aref(hash, ID2SYM(rb_intern("height")));
  spec.height = NUM2INT(height);
  VALUE framerate_numerator =
      rb_hash_aref(hash, ID2SYM(rb_intern("framerate_numerator")));
  spec.framerate_numerator = NUM2INT(framerate_numerator);
  VALUE framerate_denominator =
      rb_hash_aref(hash, ID2SYM(rb_intern("framerate_denominator")));
  spec.framerate_denominator = NUM2INT(framerate_denominator);

  return spec;
}

RB_METHOD(camera_get_supported_formats) {
  RB_UNUSED_PARAM;

  unsigned int id;
  rb_get_args(argc, argv, "i", &id RB_ARG_END);

  int count = 0;
  SDL_CameraSpec **specs = SDL_GetCameraSupportedFormats(id, &count);

  VALUE array = rb_ary_new_capa(count);

  for (int i = 0; i < count; i++) {
    SDL_CameraSpec *spec = specs[i];
    VALUE hash = spec_to_hash(spec);
    rb_ary_push(array, hash);
  }
  SDL_free(specs);

  return array;
}

// ------- obj functions ---------

RB_METHOD(camera_initialize) {
  RB_UNUSED_PARAM;

  unsigned int id;
  VALUE hash = 0;
  rb_get_args(argc, argv, "i|o", &id, &hash RB_ARG_END);

  SDL_CameraSpec spec;
  if (hash)
    spec = spec_from_hash(hash);

  SDL_Camera *camera;
  if (hash)
    camera = SDL_OpenCamera(id, &spec);
  else
    camera = SDL_OpenCamera(id, NULL);

  setPrivateData(self, camera);

  return Qnil;
}

RB_METHOD(camera_acquire_frame) {
  RB_UNUSED_PARAM;

  SDL_Camera *camera = getPrivateData<SDL_Camera>(self);
  GUARD_DISPOSED(camera);

  SDL_Surface *camera_surf = SDL_AcquireCameraFrame(camera, NULL);
  if (!camera_surf)
    return Qnil;

  // we duplicate the surface because bitmap takes ownership of it
  SDL_Surface *surf = SDL_DuplicateSurface(camera_surf);
  Bitmap *bitmap = new Bitmap(surf, NULL, false);
  SDL_ReleaseCameraFrame(camera, camera_surf);

  VALUE bitmap_class = rb_const_get(rb_cObject, rb_intern("Bitmap"));
  VALUE bitmap_obj = rb_obj_alloc(bitmap_class);
  setPrivateData(bitmap_obj, bitmap);

  return bitmap_obj;
}

RB_METHOD(camera_dispose) {
  RB_UNUSED_PARAM;

  SDL_Camera *camera = getPrivateData<SDL_Camera>(self);
  GUARD_DISPOSED(camera);

  setPrivateData(self, NULL);

  return Qnil;
}

RB_METHOD(camera_get_format) {
  RB_UNUSED_PARAM;

  SDL_Camera *camera = getPrivateData<SDL_Camera>(self);
  GUARD_DISPOSED(camera);

  SDL_CameraSpec spec;
  SDL_GetCameraFormat(camera, &spec);
  return spec_to_hash(&spec);
}

RB_METHOD(camera_get_id) {
  RB_UNUSED_PARAM;

  SDL_Camera *camera = getPrivateData<SDL_Camera>(self);
  GUARD_DISPOSED(camera);

  SDL_CameraID id = SDL_GetCameraID(camera);
  return INT2NUM(id);
}

RB_METHOD(camera_get_permission_state) {
  RB_UNUSED_PARAM;

  SDL_Camera *camera = getPrivateData<SDL_Camera>(self);
  GUARD_DISPOSED(camera);

  int permission = SDL_GetCameraPermissionState(camera);
  return INT2NUM(permission);
}

void osfmCameraBindingInit() {
  VALUE klass = rb_define_class("Camera", rb_cObject);
  rb_define_alloc_func(klass, classAllocate<&CameraType>);

  _rb_define_module_function(klass, "available_cameras", camera_get_cameras);
  _rb_define_module_function(klass, "drivers", camera_get_drivers);
  _rb_define_module_function(klass, "current_driver",
                             camera_get_current_driver);
  _rb_define_module_function(klass, "name_for", camera_get_name);
  _rb_define_module_function(klass, "position_of", camera_get_position);
  _rb_define_module_function(klass, "supported_formats",
                             camera_get_supported_formats);

  _rb_define_method(klass, "initialize", camera_initialize);
  _rb_define_method(klass, "get_frame", camera_acquire_frame);
  _rb_define_method(klass, "dispose", camera_dispose);
  _rb_define_method(klass, "format", camera_get_format);
  _rb_define_method(klass, "id", camera_get_id);
  _rb_define_method(klass, "permission_state", camera_get_permission_state);
}