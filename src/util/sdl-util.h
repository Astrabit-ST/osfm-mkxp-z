#ifndef SDLUTIL_H
#define SDLUTIL_H

#include <SDL3/SDL_atomic.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_thread.h>

#include <iostream>
#include <string>
#include <unistd.h>

// SDL3 removed SDL_RWFromFP, so we need to make our own version
struct IOStreamStdioFPData {
  FILE *fp;
  bool autoclose;
};

static Sint64 SDLCALL stdio_seek(void *userdata, Sint64 offset,
                                 SDL_IOWhence whence) {
  FILE *fp = ((IOStreamStdioFPData *)userdata)->fp;
  int stdiowhence;

  switch (whence) {
  case SDL_IO_SEEK_SET:
    stdiowhence = SEEK_SET;
    break;
  case SDL_IO_SEEK_CUR:
    stdiowhence = SEEK_CUR;
    break;
  case SDL_IO_SEEK_END:
    stdiowhence = SEEK_END;
    break;
  default:
    return SDL_SetError("Unknown value for 'whence'");
  }

  if (fseek(fp, offset, stdiowhence) == 0) {
    const Sint64 pos = ftell(fp);
    if (pos < 0) {
      return SDL_SetError("Couldn't get stream offset");
    }
    return pos;
  }
  return SDL_SetError("Couldn't seek in stream");
}

static size_t SDLCALL stdio_read(void *userdata, void *ptr, size_t size,
                                 SDL_IOStatus *status) {
  FILE *fp = ((IOStreamStdioFPData *)userdata)->fp;
  const size_t bytes = fread(ptr, 1, size, fp);
  if (bytes == 0 && ferror(fp)) {
    SDL_SetError("Couldn't read from stream");
  }
  return bytes;
}

static size_t SDLCALL stdio_write(void *userdata, const void *ptr, size_t size,
                                  SDL_IOStatus *status) {
  FILE *fp = ((IOStreamStdioFPData *)userdata)->fp;
  const size_t bytes = fwrite(ptr, 1, size, fp);
  if (bytes == 0 && ferror(fp)) {
    SDL_SetError("Couldn't write to stream");
  }
  return bytes;
}

static bool SDLCALL stdio_close(void *userdata) {
  IOStreamStdioFPData *rwopsdata = (IOStreamStdioFPData *)userdata;
  bool status = false;
  if (rwopsdata->autoclose) {
    if (fclose(rwopsdata->fp) != 0) {
      status = SDL_SetError("Couldn't close stream");
    }
  }
  return status;
}

static SDL_IOStream *SDL_RWFromFP(FILE *fp, bool autoclose) {
  SDL_IOStreamInterface iface;
  IOStreamStdioFPData *rwopsdata;
  SDL_IOStream *rwops;

  rwopsdata = (IOStreamStdioFPData *)SDL_malloc(sizeof(*rwopsdata));
  if (!rwopsdata) {
    return NULL;
  }

  SDL_zero(iface);
  /* There's no stdio_size because SDL_GetIOSize emulates it the same way we'd
   * do it for stdio anyhow. */
  iface.seek = stdio_seek;
  iface.read = stdio_read;
  iface.write = stdio_write;
  iface.close = stdio_close;

  rwopsdata->fp = fp;
  rwopsdata->autoclose = autoclose;

  rwops = SDL_OpenIO(&iface, rwopsdata);
  if (!rwops) {
    iface.close(rwopsdata);
  }
  return rwops;
}

struct AtomicFlag {
  AtomicFlag() { clear(); }

  void set() { SDL_SetAtomicInt(&atom, 1); }

  void clear() { SDL_SetAtomicInt(&atom, 0); }

  void wait() {
    while (SDL_GetAtomicInt(&atom)) {
    }
  }

  void reset() {
    wait();
    set();
  }

  operator bool() const { return SDL_GetAtomicInt(&atom); }

private:
  mutable SDL_AtomicInt atom;
};

template <class C, void (C::*func)()> int __sdlThreadFun(void *obj) {
  (static_cast<C *>(obj)->*func)();
  return 0;
}

template <class C, void (C::*func)()>
SDL_Thread *createSDLThread(C *obj, const std::string &name = std::string()) {
  return SDL_CreateThread((__sdlThreadFun<C, func>), name.c_str(), obj);
}

/* On Android, SDL_IOFromFile always opens files from inside
 * the apk asset folder even when a file with same name exists
 * on the physical filesystem. This wrapper attempts to open a
 * real file first before falling back to the assets folder */
static inline SDL_IOStream *RWFromFile(const char *filename, const char *mode) {
  FILE *f = fopen(filename, mode);

  if (!f)
    return SDL_IOFromFile(filename, mode);

  return SDL_RWFromFP(f, true);
}

inline bool readFileSDL(const char *path, std::string &out) {
  SDL_IOStream *f = RWFromFile(path, "rb");

  if (!f)
    return false;

  long size = SDL_GetIOSize(f);
  size_t back = out.size();

  out.resize(back + size);
  size_t read = SDL_ReadIO(f, &out[back], size);
  SDL_CloseIO(f);

  if (read != (size_t)size)
    out.resize(back + read);

  return true;
}

template <size_t bufSize = 248, size_t pbSize = 8>
class SDLRWBuf : public std::streambuf {
public:
  SDLRWBuf(SDL_IOStream *ops) : ops(ops) {
    char *end = buf + bufSize + pbSize;
    setg(end, end, end);
  }

private:
  int_type underflow() {
    if (!ops)
      return traits_type::eof();

    if (gptr() < egptr())
      return traits_type::to_int_type(*gptr());

    char *base = buf;
    char *start = base;

    if (eback() == base) {
      memmove(base, egptr() - pbSize, pbSize);
      start += pbSize;
    }

    size_t n = SDL_ReadIO(ops, start, bufSize - (start - base));
    if (n == 0)
      return traits_type::eof();

    setg(base, start, start + n);

    return underflow();
  }

  SDL_IOStream *ops;
  char buf[bufSize + pbSize];
};

class SDLRWStream {
public:
  SDLRWStream(const char *filename, const char *mode)
      : ops(RWFromFile(filename, mode)), buf(ops), s(&buf) {}

  ~SDLRWStream() {
    if (ops)
      SDL_CloseIO(ops);
  }

  operator bool() const { return ops != 0; }

  std::istream &stream() { return s; }

private:
  SDL_IOStream *ops;
  SDLRWBuf<> buf;
  std::istream s;
};

#endif // SDLUTIL_H
