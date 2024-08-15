/*
 ** filesystem-binding.cpp
 **
 ** This file is part of mkxp.
 **
 ** Copyright (C) 2013 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
 **
 ** mkxp is free software: you can redistribute it and/or modify
 ** it under the terms of the GNU General Public License as published by
 ** the Free Software Foundation, either version 2 of the License, or
 ** (at your option) any later version.
 **
 ** mkxp is distributed in the hope that it will be useful,
 ** but WITHOUT ANY WARRANTY; without even the implied warranty of
 ** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 ** GNU General Public License for more details.
 **
 ** You should have received a copy of the GNU General Public License
 ** along with mkxp.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "SDL3/SDL_iostream.h"
#include "src/config.h"

#include "binding-util.h"

#include "filesystem.h"
#include "sharedstate.h"
#include "src/util/util.h"

#if RAPI_FULL > 187
#include "ruby/encoding.h"
#include "ruby/intern.h"
#else
#include "intern.h"
#endif

#if RAPI_MAJOR >= 2
#include <ruby/thread.h>
#endif

static void fileIntFreeInstance(void *inst) {
    SDL_IOStream *ops = static_cast<SDL_IOStream *>(inst);
    
    SDL_CloseIO(ops);
}

#if RAPI_FULL > 187
DEF_TYPE_CUSTOMFREE(FileInt, fileIntFreeInstance);
#else
DEF_ALLOCFUNC_CUSTOMFREE(FileInt, fileIntFreeInstance);
#endif

static VALUE fileIntForPath(const char *path, bool rubyExc) {
    SDL_IOStream *ops;
    
    try {
        ops = shState->fileSystem().openReadRaw(path);
    } catch (const Exception &e) {
        if (rubyExc)
            raiseRbExc(e);
        else
            throw e;
    }
    
    VALUE klass = rb_const_get(rb_cObject, rb_intern("FileInt"));
    
    VALUE obj = rb_obj_alloc(klass);
    
    setPrivateData(obj, ops);
    
    return obj;
}

#if RAPI_MAJOR >= 2
typedef struct {
    SDL_IOStream *ops;
    void *dst;
    int length;
} fileIntReadCbArgs;

void call_RWread_cb(fileIntReadCbArgs *args) {
    SDL_ReadIO(args->ops, args->dst, args->length);
}
#endif

RB_METHOD(fileIntRead) {
    
    int length = -1;
    rb_get_args(argc, argv, "|i", &length RB_ARG_END);
    
    SDL_IOStream *ops = getPrivateData<SDL_IOStream>(self);
    
    if (length == -1) {
        Sint64 cur = SDL_TellIO(ops);
        Sint64 end = SDL_SeekIO(ops, 0, SDL_IO_SEEK_END);
        
        // Sometimes SDL_SeekIO will fail for no reason
        // with encrypted archives, so let's just ask
        // for the size up front
        if (end < 0)
            end = SDL_GetIOSize(ops);
        
        length = end - cur;
        SDL_SeekIO(ops, cur, SDL_IO_SEEK_SET);
    }
    
    if (length == 0)
        return Qnil;
    
    VALUE data = rb_str_new(0, length);
    
    
    
#if RAPI_MAJOR >= 2
    fileIntReadCbArgs cbargs {ops, RSTRING_PTR(data), length};
    rb_thread_call_without_gvl([](void* args) -> void* {
        call_RWread_cb((fileIntReadCbArgs*)args);
        return 0;
    }, (void*)&cbargs, 0, 0);
#else
    SDL_ReadIO(ops, RSTRING_PTR(data), 1, length);
#endif
    
    return data;
}

RB_METHOD(fileIntClose) {
    RB_UNUSED_PARAM;
    
    SDL_IOStream *ops = getPrivateData<SDL_IOStream>(self);
    SDL_CloseIO(ops);
    
    return Qnil;
}

RB_METHOD(fileIntGetByte) {
    RB_UNUSED_PARAM;
    
    SDL_IOStream *ops = getPrivateData<SDL_IOStream>(self);
    
    unsigned char byte;
    size_t result = SDL_ReadIO(ops, &byte, 1);
    
    return (result == 1) ? rb_fix_new(byte) : Qnil;
}

RB_METHOD(fileIntBinmode) {
    RB_UNUSED_PARAM;
    
    return Qnil;
}

#if RAPI_FULL <= 187
RB_METHOD(fileIntPos) {
    SDL_IOStream *ops = getPrivateData<SDL_IOStream>(self);
    
    long long pos = SDL_TellIO(ops); // Will return -1 if it doesn't work
    return LL2NUM(pos);
}
#endif

VALUE
kernelLoadDataInt(const char *filename, bool rubyExc, bool raw) {
    //rb_gc_start();
    
    VALUE port = fileIntForPath(filename, rubyExc);
    VALUE result;
    if (!raw) {
        VALUE marsh = rb_const_get(rb_cObject, rb_intern("Marshal"));
        
        // FIXME need to catch exceptions here with begin rescue
        VALUE data = fileIntRead(0, 0, port);
        result = rb_funcall2(marsh, rb_intern("load"), 1, &data);
    } else {
        result = fileIntRead(0, 0, port);
    }
    
    rb_funcall2(port, rb_intern("close"), 0, NULL);
    
    return result;
}

RB_METHOD(kernelLoadData) {
    RB_UNUSED_PARAM;
    
    VALUE filename;
    VALUE raw;
    rb_scan_args(argc, argv, "11", &filename, &raw);
    SafeStringValue(filename);
    
    bool rawv;
    rb_bool_arg(raw, &rawv);
    return kernelLoadDataInt(RSTRING_PTR(filename), true, rawv);
}

RB_METHOD(kernelSaveData) {
    RB_UNUSED_PARAM;
    
    VALUE obj;
    VALUE filename;
    
    rb_get_args(argc, argv, "oS", &obj, &filename RB_ARG_END);
    
    VALUE file = rb_file_open_str(filename, "wb");
    
    VALUE marsh = rb_const_get(rb_cObject, rb_intern("Marshal"));
    
    VALUE v[] = {obj, file};
    rb_funcall2(marsh, rb_intern("dump"), ARRAY_SIZE(v), v);
    
    rb_io_close(file);
    
    return Qnil;
}
#if RAPI_FULL > 187
#if RAPI_FULL < 270
static VALUE stringForceUTF8(VALUE arg)
#else
static VALUE stringForceUTF8(RB_BLOCK_CALL_FUNC_ARGLIST(arg, callback_arg))
#endif
{
    if (RB_TYPE_P(arg, RUBY_T_STRING) && ENCODING_IS_ASCII8BIT(arg))
        rb_enc_associate_index(arg, rb_utf8_encindex());
    
    return arg;
}

#if RAPI_FULL < 270
static VALUE customProc(VALUE arg, VALUE proc) {
    VALUE obj = stringForceUTF8(arg);
    obj = rb_funcall2(proc, rb_intern("call"), 1, &obj);
    
    return obj;
}
#endif

RB_METHOD(_marshalLoad) {
    RB_UNUSED_PARAM;
#if RAPI_FULL < 270
    VALUE port, proc = Qnil;
    rb_get_args(argc, argv, "o|o", &port, &proc RB_ARG_END);
#else
    VALUE port;
    rb_get_args(argc, argv, "o", &port RB_ARG_END);
#endif
    
    VALUE utf8Proc;
#if RAPI_FULL < 270
    if (NIL_P(proc))
        
        utf8Proc = rb_proc_new(RUBY_METHOD_FUNC(stringForceUTF8), Qnil);
    else
        utf8Proc = rb_proc_new(RUBY_METHOD_FUNC(customProc), proc);
#else
    utf8Proc = rb_proc_new(stringForceUTF8, Qnil);
#endif
    
    VALUE marsh = rb_const_get(rb_cObject, rb_intern("Marshal"));
    
    VALUE v[] = {port, utf8Proc};
    return rb_funcall2(marsh, rb_intern("_mkxp_load_alias"), ARRAY_SIZE(v), v);
}
#endif

void fileIntBindingInit() {
    VALUE klass = rb_define_class("FileInt", rb_cIO);
#if RAPI_FULL > 187
    rb_define_alloc_func(klass, classAllocate<&FileIntType>);
#else
    rb_define_alloc_func(klass, FileIntAllocate);
#endif
    
    _rb_define_method(klass, "read", fileIntRead);
    _rb_define_method(klass, "getbyte", fileIntGetByte);
#if RAPI_FULL <= 187
    // Ruby doesn't see this as an initialized stream,
    // so either that has to be fixed or necessary
    // IO functions have to be overridden
    rb_define_alias(klass, "getc", "getbyte");
    _rb_define_method(klass, "pos", fileIntPos);
#endif
    _rb_define_method(klass, "binmode", fileIntBinmode);
    _rb_define_method(klass, "close", fileIntClose);
    
    _rb_define_module_function(rb_mKernel, "load_data", kernelLoadData);
    _rb_define_module_function(rb_mKernel, "save_data", kernelSaveData);
    
#if RAPI_FULL > 187
    /* We overload the built-in 'Marshal::load()' function to silently
     * insert our utf8proc that ensures all read strings will be
     * UTF-8 encoded */
    VALUE marsh = rb_const_get(rb_cObject, rb_intern("Marshal"));
    rb_define_alias(rb_singleton_class(marsh), "_mkxp_load_alias", "load");
    _rb_define_module_function(marsh, "load", _marshalLoad);
#endif
}
