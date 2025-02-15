/*
** audiochannels.cpp
**
** This file is part of ModShot.
*/

#include "audiochannels.h"
AudioChannels::AudioChannels(ALStream::LoopMode loopMode,
                             const std::string &threadId, unsigned int count)
    : loopMode(loopMode), threadId(threadId), globalVolume(1.0f) {
  for (unsigned int i = 0; i < count; i++) {
    AudioStream *s =
        new AudioStream(loopMode, threadId + "_" + std::to_string(i));
    streams.push_back(s);
  }
}

unsigned int AudioChannels::size() { return streams.size(); }

void AudioChannels::resize(unsigned int size) {
  if (size < streams.size()) {
    for (std::vector<AudioStream *>::iterator i = streams.begin() + size;
         i != streams.end(); i++)
      delete *i;
    streams.erase(streams.begin() + size, streams.end());
  } else {
    for (unsigned int i = streams.size(); i < size; i++) {
      AudioStream *s =
          new AudioStream(loopMode, threadId + "_" + std::to_string(i));
      streams.push_back(s);
    }
  }
}

float AudioChannels::getGlobalVolume() { return globalVolume; }

void AudioChannels::setGlobalVolume(float volume) {
  for (AudioStream *&stream : streams)
    stream->setVolume(AudioStream::Base, stream->getVolume(AudioStream::Base) *
                                             volume / globalVolume);
  globalVolume = volume;
}

void AudioChannels::play(unsigned int id, const std::string filename,
                         int volume, int pitch, float offset) {
  if (id >= streams.size()) {
    return;
  }
  streams[id]->play(filename, volume, pitch, offset);
}

// void AudioChannels::crossfade(unsigned int id,
//                               const std::string &filename,
// 			                  float time,
// 			                  int volume,
// 				              int pitch,
// 				              float offset) {
//     if (id >= streams.size()) {
//         return;
//     }
//     streams[id]->crossfade(filename, time, volume, pitch, offset);
// }

void AudioChannels::pause(unsigned int id) {
  if (id >= streams.size()) {
    return;
  }
  streams[id]->pause();
}

void AudioChannels::stop(unsigned int id) {
  if (id >= streams.size()) {
    return;
  }
  streams[id]->stop();
}

void AudioChannels::stopall() {
  for (AudioStream *&stream : streams)
    stream->stop();
}

void AudioChannels::fadeOut(unsigned int id, int duration) {
  if (id >= streams.size()) {
    return;
  }
  streams[id]->fadeOut(duration);
}

void AudioChannels::setVolume(unsigned int id, AudioStream::VolumeType type,
                              float value) {
  if (id >= streams.size()) {
    return;
  }
  streams[id]->setVolume(type, value);
}

float AudioChannels::getVolume(unsigned int id, AudioStream::VolumeType type) {
  if (id >= streams.size()) {
    return 0;
  }
  return streams[id]->getVolume(type);
}

void AudioChannels::setPitch(unsigned int id, float value) {
  if (id >= streams.size()) {
    return;
  }
  streams[id]->setPitch(value);
}

float AudioChannels::getPitch(unsigned int id) {
  if (id >= streams.size()) {
    return 0;
  }
  return streams[id]->getPitch();
}

float AudioChannels::playingOffset(unsigned int id) {
  if (id >= streams.size()) {
    return 0;
  }
  return streams[id]->playingOffset();
}

ALStream::State AudioChannels::queryState(unsigned int id) {
  if (id >= streams.size()) {
    return ALStream::State::Closed;
  }
  return streams[id]->stream.queryState();
}

void AudioChannels::setALFilter(unsigned int id, AL::Filter::ID filter) {
  if (id >= streams.size()) {
    return;
  }
  streams[id]->setALFilter(filter);
}

void AudioChannels::setALEffect(unsigned int id, ALuint effect) {
  if (id >= streams.size()) {
    return;
  }
  streams[id]->setALEffect(effect);
}