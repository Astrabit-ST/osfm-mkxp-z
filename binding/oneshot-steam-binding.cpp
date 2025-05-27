// Copyright (C) 2024 Lily Lyons
// 
// This file is part of osfm-mkxp-z.
// 
// osfm-mkxp-z is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// osfm-mkxp-z is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with osfm-mkxp-z.  If not, see <https://www.gnu.org/licenses/>.
#include "binding-util.h"

RB_METHOD(steamEnabled)
{
	RB_UNUSED_PARAM;

	return Qfalse;
}

RB_METHOD(steamUnlock)
{
	RB_UNUSED_PARAM;

  const char *name;
	rb_get_args(argc, argv, "z", &name RB_ARG_END);

	return Qnil;
}

RB_METHOD(steamLock)
{
	RB_UNUSED_PARAM;

	const char *name;
	rb_get_args(argc, argv, "z", &name RB_ARG_END);

	return Qnil;
}

RB_METHOD(steamUnlocked)
{
	RB_UNUSED_PARAM;

	const char *name;
	rb_get_args(argc, argv, "z", &name RB_ARG_END);

	return Qfalse;
}

void oneshotSteamBindingInit() {
  // even if steam isnt enabled, oneshot always provides this module
  VALUE module = rb_define_module("Steam");

	/* Constants */
  // these constants are not really used by oneshot so we can get away with setting them to nil
	rb_const_set(module, rb_intern("USER_NAME"), Qnil);
	rb_const_set(module, rb_intern("LANG"), Qnil);

	/* Functions */
	_rb_define_module_function(module, "enabled?", steamEnabled);
	_rb_define_module_function(module, "unlock", steamUnlock);
	_rb_define_module_function(module, "lock", steamLock);
	_rb_define_module_function(module, "unlocked?", steamUnlocked);
}