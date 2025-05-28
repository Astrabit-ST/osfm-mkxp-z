// ruby provides its own gettimeofday() in ruby/win32.h, so we have to disable
// that file somehow. Thanks ruby!
#define RBIMPL_INTERN_SELECT_H 1
#define RUBY_WIN32_H 1
#include "binding-util.h"
#include "debugwriter.h"
#include "i18n.h"

#include "journal_common.h"

#include <SDL3/SDL.h>
#include <cstring>
#include <filesystem>
#include <string>
#include <unistd.h>

#include <boost/interprocess/ipc/message_queue.hpp>

using namespace boost::interprocess;

static SDL_Thread *thread = NULL;
static SDL_Mutex *mutex = NULL;

static message_queue *oneshot_mq; // messages we send to journal
static message_queue *journal_mq; // messages journal sends to us

static volatile int active_count = 0;
static volatile int journal_x = 0;
static volatile int journal_y = 0;
static volatile bool server_active = true;

int server_thread_fn(void *data) {

  // ask any currently open journals to send a hello
  Message message;
  unsigned int priority;
  size_t recvd_size;

  try {
    // clear the queue by reading all the messages
    while (journal_mq->try_receive(&message, sizeof(message), recvd_size,
                                   priority)) {
      Debug() << "WARN:" << "Leftover message" << message.tag;
    }

    message.tag = Message::Hello;
    oneshot_mq->send(&message, sizeof(message), 255);
  } catch (...) {
    Debug() << "failed to recv/send to queue";
    message_queue::remove("oneshot_mq");
    message_queue::remove("journal_mq");

    oneshot_mq =
        new message_queue(open_or_create, "oneshot_mq", 100, sizeof(Message));
    journal_mq =
        new message_queue(open_or_create, "journal_mq", 100, sizeof(Message));
  }

  // set active if any journals responded with hello
  for (;;) {
    // for whatever reason a plain recieve() misses some messages. this doesn't
    // tho
    bool did_recv = false;
    while (!did_recv) {
      auto now = std::chrono::steady_clock::now();
      auto abs_time = now + std::chrono::milliseconds(8);
      did_recv = journal_mq->timed_receive(&message, sizeof(message),
                                           recvd_size, priority, abs_time);
      if (!server_active)
        return 0;
    }

    switch (message.tag) {
    case Message::Hello:
      active_count++;
      break;
    case Message::WindowPosition:
      journal_x = message.val.pos.x;
      journal_y = message.val.pos.y;
      break;
    case Message::Goodbye:
      active_count--;
      break;
    default:
      Debug() << "Unhandled message tag";
      break;
    }
  }

  return 0;
}

RB_METHOD(journalSet) {
  RB_UNUSED_PARAM;
  const char *name;
  rb_get_args(argc, argv, "z", &name RB_ARG_END);

  // if the journal is not active, return
  if (active_count == 0)
    return Qnil;

  Message message;

  // replicate old journal behaviour where calling set() with "" closes the
  // journal
  if (strlen(name) == 0) {
    message.tag = Message::Close;
    oneshot_mq->send(&message, sizeof(message), 255);

    active_count--;

    return Qnil;
  }

  auto pwd = std::filesystem::current_path();
  std::string dir = pwd.string();

  dir += "/Graphics/Journal/";
  dir += name;
  dir += ".png";

  // we have to chunk the image path
  message.tag = Message::ImagePath;
  // for i in ceil(dir.length() / 24)
  for (unsigned int i = 0; i < (dir.length() + 23) / 24; i++) {
    std::string substr = dir.substr(i * 24, 24);

    // copy substring into message
    message.val.text.len = substr.length();
    strncpy(message.val.text.chars, substr.c_str(), substr.length());

    oneshot_mq->send(&message, sizeof(message), 255);
  }

  // tell the journal we are finished sending the image path
  message.tag = Message::FinishImagePath;
  oneshot_mq->send(&message, sizeof(message), 255);

  return Qnil;
}

RB_METHOD(journalSetLang) {
  RB_UNUSED_PARAM;
  const char *lang;
  rb_get_args(argc, argv, "z", &lang RB_ARG_END);

  // FIXME language handling

  return Qnil;
}

RB_METHOD(journalActive) {
  RB_UNUSED_PARAM;
  return active_count > 0 ? Qtrue : Qfalse;
}

RB_METHOD(journalPosition) {
  RB_UNUSED_PARAM;

  if (active_count == 0)
    return Qnil;

  return rb_ary_new_from_args(2, INT2FIX(journal_x), RB_INT2FIX(journal_y));
}

RB_METHOD(setJournalPosition) {
  int x, y;
  rb_get_args(argc, argv, "ii", &x, &y);

  Message message;
  message.tag = Message::SetWindowPosition;
  message.val.pos = {x, y};
  oneshot_mq->send(&message, sizeof(message), 255);

  return Qnil;
}

RB_METHOD(journalQuit) {
  Message message;
  message.tag = Message::Close;
  oneshot_mq->send(&message, sizeof(message), 255);

  active_count--;

  Debug() << "sending quit";

  return Qnil;
}

void cleanup_journal_stuff() {
  server_active = false;
  SDL_WaitThread(thread, NULL);

  delete oneshot_mq;
  delete journal_mq;
}

void oneshotJournalBindingInit() {
  mutex = SDL_CreateMutex();

  try {
    oneshot_mq =
        new message_queue(open_or_create, "oneshot_mq", 100, sizeof(Message));
    journal_mq =
        new message_queue(open_or_create, "journal_mq", 100, sizeof(Message));
  } catch (...) {
    Debug() << "failed to open queue";
    message_queue::remove("oneshot_mq");
    message_queue::remove("journal_mq");

    oneshot_mq =
        new message_queue(open_or_create, "oneshot_mq", 100, sizeof(Message));
    journal_mq =
        new message_queue(open_or_create, "journal_mq", 100, sizeof(Message));
  }

  thread = SDL_CreateThread(server_thread_fn, "journal server thread", NULL);

  VALUE module = rb_define_module("Journal");
  _rb_define_module_function(module, "set", journalSet);
  _rb_define_module_function(module, "active?", journalActive);
  _rb_define_module_function(module, "setLang", journalSetLang);
  _rb_define_module_function(module, "journal_position", journalPosition);
  _rb_define_module_function(module, "set_journal_position",
                             setJournalPosition);
  _rb_define_module_function(module, "quit", journalQuit);
}
