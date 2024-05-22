/*
** audio-binding.cpp
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

#include "audio.h"
#include "sharedstate.h"
#include "binding-util.h"


AL::Filter::ID constructALFilter(int argc, VALUE *argv) {
	int type;
	double gain, gainlf, gainhf;
	rb_get_args(argc, argv, "i|", &type RB_ARG_END);
	switch(type) {
		case 0: // lowpass
			rb_get_args(argc, argv, "iff", &type, &gain, &gainhf RB_ARG_END);
			return AL::Filter::createLowpassFilter(gain, gainhf);
		case 1: // highpass
			rb_get_args(argc, argv, "iff", &type, &gain, &gainlf RB_ARG_END);
			return AL::Filter::createHighpassFilter(gain, gainlf);
		case 2: // bandpass
			rb_get_args(argc, argv, "ifff", &type, &gain, &gainlf, &gainhf RB_ARG_END);
			return AL::Filter::createBandpassFilter(gain, gainlf, gainhf);
		default:
			rb_raise(rb_eArgError, "Unrecognized AL filter type");
	}
}


#define DEF_AUD_ALFILTER(entity) \
	RB_METHOD(audio_##entity##SetALFilter) { \
		AL::Filter::ID filter = constructALFilter(argc, argv); \
		shState->audio().entity##SetALFilter(filter); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##ClearALFilter) { \
		shState->audio().entity##ClearALFilter(); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##SetALEffect) { \
		VALUE effect_obj; \
		rb_get_args(argc, argv, "o", &effect_obj RB_ARG_END); \
		ALuint effect = NUM2INT(rb_funcall(effect_obj, rb_intern("create_underlying_effect"), 0)); \
		shState->audio().entity##SetALEffect(effect); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##ClearALEffect) { \
		shState->audio().entity##ClearALEffect(); \
		return Qnil; \
	}

#define DEF_PLAY_STOP_POS(entity) \
	RB_METHOD(audio_##entity##Play) \
	{ \
		RB_UNUSED_PARAM; \
		const char *filename; \
		int volume = 100; \
		int pitch = 100; \
		bool _unused = false; \
		double pos = 0.0; \
        rb_get_args(argc, argv, "z|iifb", &filename, &volume, &pitch, &pos, &_unused RB_ARG_END); \
		GUARD_EXC( shState->audio().entity##Play(filename, volume, pitch, pos); ) \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##Stop) \
	{ \
		RB_UNUSED_PARAM; \
		shState->audio().entity##Stop(); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##Pos) \
	{ \
		RB_UNUSED_PARAM; \
		return rb_float_new(shState->audio().entity##Pos()); \
	}

#define DEF_PLAY_STOP(entity) \
	RB_METHOD(audio_##entity##Play) \
	{ \
		RB_UNUSED_PARAM; \
		const char *filename; \
		int volume = 100; \
		int pitch = 100; \
		rb_get_args(argc, argv, "z|ii", &filename, &volume, &pitch RB_ARG_END); \
		GUARD_EXC( shState->audio().entity##Play(filename, volume, pitch); ) \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##Stop) \
	{ \
		RB_UNUSED_PARAM; \
		shState->audio().entity##Stop(); \
		return Qnil; \
	}

#define DEF_FADE(entity) \
RB_METHOD(audio_##entity##Fade) \
{ \
	RB_UNUSED_PARAM; \
	int time; \
	rb_get_args(argc, argv, "i", &time RB_ARG_END); \
	shState->audio().entity##Fade(time); \
	return Qnil; \
}

#define DEF_POS(entity) \
	RB_METHOD(audio_##entity##Pos) \
	{ \
		RB_UNUSED_PARAM; \
		return rb_float_new(shState->audio().entity##Pos()); \
	}


#define DEF_ISPLAYING(entity) \
	RB_METHOD(audio_##entity##IsPlaying) \
	{ \
		RB_UNUSED_PARAM; \
		return shState->audio().entity##IsPlaying() ? Qtrue : Qfalse; \
	}


#define DEF_AUD_ALFILTER(entity) \
	RB_METHOD(audio_##entity##SetALFilter) { \
		AL::Filter::ID filter = constructALFilter(argc, argv); \
		shState->audio().entity##SetALFilter(filter); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##ClearALFilter) { \
		shState->audio().entity##ClearALFilter(); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##SetALEffect) { \
		VALUE effect_obj; \
		rb_get_args(argc, argv, "o", &effect_obj RB_ARG_END); \
		ALuint effect = NUM2INT(rb_funcall(effect_obj, rb_intern("create_underlying_effect"), 0)); \
		shState->audio().entity##SetALEffect(effect); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##ClearALEffect) { \
		shState->audio().entity##ClearALEffect(); \
		return Qnil; \
	}

#define DEF_ALL_AUDIO_CH_FUNC(entity) \
	RB_METHOD(audio_##entity##Play) \
	{ \
		unsigned int id; \
		const char *filename; \
		int volume = 100; \
		int pitch = 100; \
		double pos = -1.0; \
		rb_get_args(argc, argv, "iz|iif", &id, &filename, &volume, &pitch, &pos RB_ARG_END); \
		GUARD_EXC( shState->audio().entity##Play(id, filename, volume, pitch, pos); ) \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##Stop) \
	{ \
		unsigned int id; \
		rb_get_args(argc, argv, "i", &id RB_ARG_END); \
		shState->audio().entity##Stop(id); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##Pos) \
	{ \
		unsigned int id; \
		rb_get_args(argc, argv, "i", &id RB_ARG_END); \
		return rb_float_new(shState->audio().entity##Pos(id)); \
	} \
	RB_METHOD(audio_##entity##Fade) \
	{ \
		unsigned int id; \
		int time; \
		rb_get_args(argc, argv, "ii", &id, &time RB_ARG_END); \
		shState->audio().entity##Fade(id, time); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##Crossfade) \
	{ \
		unsigned int id; \
		const char *filename; \
		double time = 2; \
		int volume = 100; \
		int pitch = 100; \
		double pos = -1.0; \
		rb_get_args(argc, argv, "iz|fiif", &id, &filename, &time, &volume, &pitch, &pos RB_ARG_END); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##IsPlaying) \
	{ \
		unsigned int id; \
		rb_get_args(argc, argv, "i", &id RB_ARG_END); \
		return shState->audio().entity##IsPlaying(id) ? Qtrue : Qfalse; \
	} \
	RB_METHOD(audio_##entity##getVolume) \
	{ \
		unsigned int id; \
		rb_get_args(argc, argv, "i", &id RB_ARG_END); \
		return rb_float_new(shState->audio().get##entity##Volume(id)); \
	} \
	RB_METHOD(audio_##entity##setVolume) \
	{ \
		unsigned int id; \
		double vol; \
		rb_get_args(argc, argv, "if", &id, &vol RB_ARG_END); \
		shState->audio().set##entity##Volume(id, vol); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##getGlobalVolume) \
	{ \
		RB_UNUSED_PARAM; \
		return rb_float_new(shState->audio().get##entity##GlobalVolume()); \
	} \
	RB_METHOD(audio_##entity##setGlobalVolume) \
	{ \
		double vol; \
		rb_get_args(argc, argv, "f", &vol RB_ARG_END); \
		shState->audio().set##entity##GlobalVolume(vol); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##getPitch) \
	{ \
		unsigned int id; \
		rb_get_args(argc, argv, "i", &id RB_ARG_END); \
		return rb_float_new(shState->audio().get##entity##Pitch(id)); \
	} \
	RB_METHOD(audio_##entity##setPitch) \
	{ \
		unsigned int id; \
		double pitch; \
		rb_get_args(argc, argv, "if", &id, &pitch RB_ARG_END); \
		shState->audio().set##entity##Pitch(id, pitch); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##Size) { \
		RB_UNUSED_PARAM; \
		return INT2NUM(shState->audio().entity##Size()); \
	} \
	RB_METHOD(audio_##entity##Resize) { \
		unsigned int size; \
		rb_get_args(argc, argv, "i", &size RB_ARG_END); \
		shState->audio().entity##Resize(size); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##SetALFilter) { \
		unsigned int id; \
		rb_get_args(argc, argv, "i|", &id RB_ARG_END); \
		argc--; \
		argv++; \
		AL::Filter::ID filter = constructALFilter(argc, argv); \
		shState->audio().entity##SetALFilter(id, filter); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##ClearALFilter) { \
		unsigned int id; \
		rb_get_args(argc, argv, "i", &id RB_ARG_END); \
		shState->audio().entity##ClearALFilter(id); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##SetALEffect) { \
		unsigned int id; \
		VALUE effect_obj; \
		rb_get_args(argc, argv, "io", &id, &effect_obj RB_ARG_END); \
		ALuint effect = NUM2INT(rb_funcall(effect_obj, rb_intern("create_underlying_effect"), 0)); \
		shState->audio().entity##SetALEffect(id, effect); \
		return Qnil; \
	} \
	RB_METHOD(audio_##entity##ClearALEffect) { \
		unsigned int id; \
		rb_get_args(argc, argv, "i", &id RB_ARG_END); \
		shState->audio().entity##ClearALEffect(id); \
		return Qnil; \
	}

// does nothing
#define DEF_CROSSFADE(entity) \
RB_METHOD(audio_##entity##Crossfade) \
{ \
	RB_UNUSED_PARAM; \
	const char *filename; \
	double time = 2; \
	int volume = 100; \
	int pitch = 100; \
	double pos = -1.0; \
	rb_get_args(argc, argv, "z|fiif", &filename, &time, &volume, &pitch, &pos RB_ARG_END); \
	return Qnil; \
}


DEF_PLAY_STOP_POS( bgm )
DEF_PLAY_STOP_POS( bgs )

DEF_PLAY_STOP( me )

DEF_FADE( bgm)
DEF_FADE( bgs )
DEF_FADE( me )

DEF_PLAY_STOP( se )

// modshot extensions
DEF_ISPLAYING( bgm )
DEF_ISPLAYING( bgs )
DEF_ISPLAYING( me )

DEF_CROSSFADE( bgm )
DEF_CROSSFADE( bgs )
DEF_CROSSFADE( me )

DEF_ALL_AUDIO_CH_FUNC(lch)
DEF_ALL_AUDIO_CH_FUNC(ch)

DEF_AUD_ALFILTER(bgm)
DEF_AUD_ALFILTER(bgs)
DEF_AUD_ALFILTER(me)
DEF_AUD_ALFILTER(se)

RB_METHOD(audioSetupMidi)
{
	RB_UNUSED_PARAM;

	shState->audio().setupMidi();

	return Qnil;
}

RB_METHOD(audioReset)
{
	RB_UNUSED_PARAM;

	shState->audio().reset();

	return Qnil;
}


#define BIND_PLAY_STOP(entity) \
	_rb_define_module_function(module, #entity "_play", audio_##entity##Play); \
	_rb_define_module_function(module, #entity "_stop", audio_##entity##Stop);

#define BIND_FADE(entity) \
	_rb_define_module_function(module, #entity "_fade", audio_##entity##Fade);

#define BIND_PLAY_STOP_FADE(entity) \
	BIND_PLAY_STOP(entity) \
	BIND_FADE(entity)

#define BIND_POS(entity) \
	_rb_define_module_function(module, #entity "_pos", audio_##entity##Pos);

#define BIND_IS_PLAYING(entity) \
	_rb_define_module_function(module, #entity "_playing?", audio_##entity##IsPlaying);

#define BIND_AUDIO_ALFILTER(entity) \
	_rb_define_module_function(module, #entity "_set_al_filter", audio_##entity##SetALFilter); \
	_rb_define_module_function(module, #entity "_clear_al_filter", audio_##entity##ClearALFilter); \
	_rb_define_module_function(module, #entity "_set_al_effect", audio_##entity##SetALEffect); \
	_rb_define_module_function(module, #entity "_clear_al_effect", audio_##entity##ClearALEffect);

#define BIND_ALL_AUDIO_CH_FUNC(entity) \
	_rb_define_module_function(module, #entity "_play", audio_##entity##Play); \
	_rb_define_module_function(module, #entity "_stop", audio_##entity##Stop); \
	_rb_define_module_function(module, #entity "_pos", audio_##entity##Pos); \
	_rb_define_module_function(module, #entity "_fade", audio_##entity##Fade); \
	_rb_define_module_function(module, #entity "_crossfade", audio_##entity##Crossfade); \
	_rb_define_module_function(module, #entity "_playing?", audio_##entity##IsPlaying); \
	_rb_define_module_function(module, #entity "_get_volume", audio_##entity##getVolume); \
	_rb_define_module_function(module, #entity "_set_volume", audio_##entity##setVolume); \
	_rb_define_module_function(module, #entity "_get_global_volume", audio_##entity##getGlobalVolume); \
	_rb_define_module_function(module, #entity "_set_global_volume", audio_##entity##setGlobalVolume); \
	_rb_define_module_function(module, #entity "_get_pitch", audio_##entity##getPitch); \
	_rb_define_module_function(module, #entity "_set_pitch", audio_##entity##setPitch); \
	_rb_define_module_function(module, #entity "_size", audio_##entity##Size); \
	_rb_define_module_function(module, #entity "_resize", audio_##entity##Resize); \
	_rb_define_module_function(module, #entity "_set_al_filter", audio_##entity##SetALFilter); \
	_rb_define_module_function(module, #entity "_clear_al_filter", audio_##entity##ClearALFilter); \
	_rb_define_module_function(module, #entity "_set_al_effect", audio_##entity##SetALEffect); \
	_rb_define_module_function(module, #entity "_clear_al_effect", audio_##entity##ClearALEffect);

#define BIND_CROSSFADE(entity) \
	_rb_define_module_function(module, #entity "_crossfade", audio_##entity##Crossfade);

void
audioBindingInit()
{
	VALUE module = rb_define_module("Audio");

	BIND_PLAY_STOP_FADE( bgm );
	BIND_PLAY_STOP_FADE( bgs );
	BIND_PLAY_STOP_FADE( me  );

	BIND_POS( bgm );
	BIND_POS( bgs );

	_rb_define_module_function(module, "setup_midi", audioSetupMidi);

	BIND_PLAY_STOP( se )

	_rb_define_module_function(module, "__reset__", audioReset);

	// modshot extensions

	BIND_CROSSFADE( bgm );
	BIND_CROSSFADE( bgs );
	BIND_CROSSFADE( me );

	BIND_IS_PLAYING( bgm );
	BIND_IS_PLAYING( bgs );
	BIND_IS_PLAYING( me );

	BIND_AUDIO_ALFILTER(bgm);
	BIND_AUDIO_ALFILTER(bgs);
	BIND_AUDIO_ALFILTER(me);
	BIND_AUDIO_ALFILTER(se);

	BIND_ALL_AUDIO_CH_FUNC(lch)
	BIND_ALL_AUDIO_CH_FUNC(ch)
}
