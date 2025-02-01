/*
** keybindings.cpp
**
** This file is part of mkxp.
**
** Copyright (C) 2014 - 2021 Amaryllis Kulla <ancurio@mapleshrine.eu>
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

#include "keybindings.h"

#include "config.h"
#include "util.h"

#include <stdio.h>

struct KbBindingData {
  SDL_Scancode source;
  Input::ButtonCode target;

  void add(BDescVec &d) const {
    SourceDesc src;
    src.type = Key;
    src.d.scan = source;

    BindingDesc desc;
    desc.src = src;
    desc.target = target;

    d.push_back(desc);
  }
};

struct CtrlBindingData {
  SDL_GamepadButton source;
  Input::ButtonCode target;

  void add(BDescVec &d) const {
    SourceDesc src;
    src.type = CButton;
    src.d.cb = source;

    BindingDesc desc;
    desc.src = src;
    desc.target = target;

    d.push_back(desc);
  }
};

/* Common */
static const KbBindingData defaultKbBindings[] = {
    {SDL_SCANCODE_LEFT, Input::Left},    {SDL_SCANCODE_RIGHT, Input::Right},
    {SDL_SCANCODE_UP, Input::Up},        {SDL_SCANCODE_DOWN, Input::Down},

    {SDL_SCANCODE_Z, Input::Action},     {SDL_SCANCODE_SPACE, Input::Action},
    {SDL_SCANCODE_X, Input::Cancel},     {SDL_SCANCODE_ESCAPE, Input::Cancel},
    {SDL_SCANCODE_A, Input::Menu},       {SDL_SCANCODE_RETURN, Input::Menu},
    {SDL_SCANCODE_S, Input::Items},      {SDL_SCANCODE_LSHIFT, Input::Run},
    {SDL_SCANCODE_C, Input::Deactivate}, {SDL_SCANCODE_Q, Input::L},
    {SDL_SCANCODE_W, Input::R}};

static elementsN(defaultKbBindings);

static const CtrlBindingData defaultCtrlBindings[] = {
    {SDL_GAMEPAD_BUTTON_DPAD_LEFT, Input::Left},
    {SDL_GAMEPAD_BUTTON_DPAD_RIGHT, Input::Right},
    {SDL_GAMEPAD_BUTTON_DPAD_UP, Input::Up},
    {SDL_GAMEPAD_BUTTON_DPAD_DOWN, Input::Down},
    {SDL_GAMEPAD_BUTTON_SOUTH, Input::Action},
    {SDL_GAMEPAD_BUTTON_EAST, Input::Cancel},
    {SDL_GAMEPAD_BUTTON_WEST, Input::Run},
    {SDL_GAMEPAD_BUTTON_NORTH, Input::Items},
    {SDL_GAMEPAD_BUTTON_START, Input::Menu},
    {SDL_GAMEPAD_BUTTON_BACK, Input::Deactivate},
    {SDL_GAMEPAD_BUTTON_LEFT_SHOULDER, Input::L},
    {SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER, Input::R}};

static elementsN(defaultCtrlBindings);

static void addAxisBinding(BDescVec &d, SDL_GamepadAxis axis, AxisDir dir,
                           Input::ButtonCode target) {
  SourceDesc src;
  src.type = CAxis;
  src.d.ca.axis = axis;
  src.d.ca.dir = dir;

  BindingDesc desc;
  desc.src = src;
  desc.target = target;

  d.push_back(desc);
}

BDescVec genDefaultBindings(const Config &conf) {
  BDescVec d;

  for (size_t i = 0; i < defaultKbBindingsN; ++i)
    defaultKbBindings[i].add(d);

  for (size_t i = 0; i < defaultCtrlBindingsN; ++i)
    defaultCtrlBindings[i].add(d);

  addAxisBinding(d, SDL_GAMEPAD_AXIS_LEFTX, Negative, Input::Left);
  addAxisBinding(d, SDL_GAMEPAD_AXIS_LEFTX, Positive, Input::Right);
  addAxisBinding(d, SDL_GAMEPAD_AXIS_LEFTY, Negative, Input::Up);
  addAxisBinding(d, SDL_GAMEPAD_AXIS_LEFTY, Positive, Input::Down);
  addAxisBinding(d, SDL_GAMEPAD_AXIS_LEFT_TRIGGER, Positive, Input::Deactivate);
  addAxisBinding(d, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER, Positive, Input::Run);

  return d;
}

#define FORMAT_VER 3

struct Header {
  uint32_t formVer;
  uint32_t rgssVer;
  uint32_t count;
};

static void buildPath(const std::string &dir, uint32_t rgssVersion, char *out,
                      size_t outSize) {
  snprintf(out, outSize, "%skeybindings.mkxp%u", dir.c_str(), rgssVersion);
}

static bool writeBindings(const BDescVec &d, const std::string &dir,
                          uint32_t rgssVersion) {
  if (dir.empty())
    return false;

  char path[1024];
  buildPath(dir, rgssVersion, path, sizeof(path));

  FILE *f = fopen(path, "wb");

  if (!f)
    return false;

  Header hd;
  hd.formVer = FORMAT_VER;
  hd.rgssVer = rgssVersion;
  hd.count = d.size();

  if (fwrite(&hd, sizeof(hd), 1, f) < 1) {
    fclose(f);
    return false;
  }

  if (fwrite(&d[0], sizeof(d[0]), hd.count, f) < hd.count) {
    fclose(f);
    return false;
  }

  fclose(f);
  return true;
}

void storeBindings(const BDescVec &d, const Config &conf) {
  writeBindings(d, conf.customDataPath, conf.rgssVersion);
}

#define READ(ptr, size, n, f)                                                  \
  if (fread(ptr, size, n, f) < n)                                              \
  return false

static bool verifyDesc(const BindingDesc &desc) {
  const Input::ButtonCode codes[] = {
      Input::None,       Input::Down,   Input::Left, Input::Right, Input::Up,
      Input::Action,     Input::Cancel, Input::Menu, Input::Items, Input::Run,
      Input::Deactivate, Input::L,      Input::R,    Input::F5,    Input::F6,
      Input::F7,         Input::F8,     Input::F9};

  elementsN(codes);
  size_t i;

  for (i = 0; i < codesN; ++i)
    if (desc.target == codes[i])
      break;

  if (i == codesN)
    return false;

  const SourceDesc &src = desc.src;

  switch (src.type) {
  case Invalid:
    return true;
  case Key:
    return src.d.scan < SDL_SCANCODE_COUNT;

  case CButton:
    return true;

  case CAxis:
    return src.d.ca.dir == Negative || src.d.ca.dir == Positive;
  default:
    return false;
  }
}

static bool readBindings(BDescVec &out, const std::string &dir,
                         uint32_t rgssVersion) {
  if (dir.empty())
    return false;

  char path[1024];
  buildPath(dir, rgssVersion, path, sizeof(path));

  FILE *f = fopen(path, "rb");

  if (!f)
    return false;

  Header hd;
  if (fread(&hd, sizeof(hd), 1, f) < 1) {
    fclose(f);
    return false;
  }

  if (hd.formVer != FORMAT_VER)
    return false;
  if (hd.rgssVer != rgssVersion)
    return false;
  /* Arbitrary max value */
  if (hd.count > 1024)
    return false;

  out.resize(hd.count);
  if (fread(&out[0], sizeof(out[0]), hd.count, f) < hd.count) {
    fclose(f);
    return false;
  }

  for (size_t i = 0; i < hd.count; ++i)
    if (!verifyDesc(out[i]))
      return false;

  return true;
}

BDescVec loadBindings(const Config &conf) {
  BDescVec d;

  if (readBindings(d, conf.customDataPath, conf.rgssVersion))
    return d;

  return genDefaultBindings(conf);
}
