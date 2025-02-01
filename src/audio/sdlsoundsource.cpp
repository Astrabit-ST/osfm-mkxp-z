/*
** sdlsoundsource.cpp
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

#include "SDL3/SDL_audio.h"
#include "SDL3/SDL_iostream.h"
#include "aldatasource.h"
#include "exception.h"

#include <SDL3_sound/SDL_sound.h>

static int SDL_RWopsCloseNoop(SDL_IOStream *ops) { return 0; }

struct SDLSoundSource : ALDataSource {
  Sound_Sample *sample;
  SDL_IOStream *srcOps;
  SDL_IOStream *unclosableOps;
  uint8_t sampleSize;
  bool looped;

  ALenum alFormat;
  ALsizei alFreq;

  SDLSoundSource(SDL_IOStream *ops, const char *extension, uint32_t maxBufSize,
                 bool looped)
      : srcOps(ops), looped(looped) {
    SDL_IOStreamInterface interface;
    interface.read = [](void *context, void *ptr, size_t size,
                        SDL_IOStatus *status) -> size_t {
      SDL_IOStream *ops = static_cast<SDL_IOStream *>(context);
      size_t read = SDL_ReadIO(ops, ptr, size);
      *status = SDL_GetIOStatus(ops);
      return read;
    };
    interface.write = [](void *context, const void *ptr, size_t size,
                         SDL_IOStatus *status) -> size_t {
      SDL_IOStream *ops = static_cast<SDL_IOStream *>(context);
      size_t written = SDL_WriteIO(ops, ptr, size);
      *status = SDL_GetIOStatus(ops);
      return written;
    };
    interface.seek = [](void *context, int64_t offset,
                        SDL_IOWhence whence) -> int64_t {
      SDL_IOStream *ops = static_cast<SDL_IOStream *>(context);
      int64_t pos = SDL_SeekIO(ops, offset, whence);
      return pos;
    };
    interface.close = [](void *context) -> bool {
      SDL_IOStream *ops = static_cast<SDL_IOStream *>(context);
      return SDL_RWopsCloseNoop(ops);
    };
    unclosableOps = SDL_OpenIO(&interface, srcOps);

    sample = Sound_NewSample(unclosableOps, extension, 0, maxBufSize);

    if (!sample) {
      SDL_CloseIO(srcOps);
      throw Exception(Exception::SDLError, "SDL_sound: %s", Sound_GetError());
    }

    bool validFormat = true;

    switch (sample->actual.format) {
    // OpenAL Soft doesn't support S32 formats.
    // https://github.com/kcat/openal-soft/issues/934
    case SDL_AUDIO_S32LE:
    case SDL_AUDIO_S32BE:
      validFormat = false;
    }

    if (!validFormat) {
      // Unfortunately there's no way to change the desired format of a sample.
      // https://github.com/icculus/SDL_sound/issues/91
      // So we just have to close the sample (which closes the file too),
      // and retry with a new desired format.
      Sound_FreeSample(sample);
      SDL_SeekIO(unclosableOps, 0, SDL_IO_SEEK_SET);

      Sound_AudioInfo desired;
      SDL_memset(&desired, '\0', sizeof(Sound_AudioInfo));
      desired.format = SDL_AUDIO_F32;

      sample = Sound_NewSample(unclosableOps, extension, &desired, maxBufSize);

      if (!sample) {
        SDL_CloseIO(srcOps);
        throw Exception(Exception::SDLError, "SDL_sound: %s", Sound_GetError());
      }
    }

    sampleSize = formatSampleSize(sample->actual.format);

    alFormat = chooseALFormat(sampleSize, sample->actual.channels);
    alFreq = sample->actual.rate;
  }

  ~SDLSoundSource() {
    Sound_FreeSample(sample);
    SDL_CloseIO(srcOps);
  }

  Status fillBuffer(AL::Buffer::ID alBuffer) {
    uint32_t decoded = Sound_Decode(sample);

    if (sample->flags & SOUND_SAMPLEFLAG_EAGAIN) {
      /* Try to decode one more time on EAGAIN */
      decoded = Sound_Decode(sample);

      /* Give up */
      if (sample->flags & SOUND_SAMPLEFLAG_EAGAIN)
        return ALDataSource::Error;
    }

    if (sample->flags & SOUND_SAMPLEFLAG_ERROR)
      return ALDataSource::Error;

    AL::Buffer::uploadData(alBuffer, alFormat, sample->buffer, decoded, alFreq);

    if (sample->flags & SOUND_SAMPLEFLAG_EOF) {
      if (looped) {
        Sound_Rewind(sample);
        return ALDataSource::WrapAround;
      } else {
        return ALDataSource::EndOfStream;
      }
    }

    return ALDataSource::NoError;
  }

  int sampleRate() { return sample->actual.rate; }

  void seekToOffset(float seconds) {
    if (seconds <= 0)
      Sound_Rewind(sample);
    else
      Sound_Seek(sample, static_cast<uint32_t>(seconds * 1000));
  }

  uint32_t loopStartFrames() {
    /* Loops from the beginning of the file */
    return 0;
  }

  bool setPitch(float) { return false; }
};

ALDataSource *createSDLSource(SDL_IOStream *ops, const char *extension,
                              uint32_t maxBufSize, bool looped) {
  return new SDLSoundSource(ops, extension, maxBufSize, looped);
}
