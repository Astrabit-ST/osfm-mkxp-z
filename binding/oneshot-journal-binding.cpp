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

static boost::interprocess::shared_memory_object journal_shm;
static boost::interprocess::mapped_region journal_region;
static Journal *journal = nullptr;

RB_METHOD(journalSet) {
  RB_UNUSED_PARAM;
  const char *name;
  rb_get_args(argc, argv, "z", &name RB_ARG_END);

  // replicate old journal behaviour where calling set() with "" closes the
  // journal
  if (strlen(name) == 0) {
    JournalGuard guard(*journal, true);
    ++journal->close.nonce;
    *journal->image.path = 0;
    return Qnil;
  }

  auto pwd = std::filesystem::current_path();
  std::string dir = pwd.string();

  dir += "/Graphics/Journal/";
  dir += name;
  dir += ".png";

  Debug() << "Sending:" << dir;

  JournalGuard guard(*journal, true);
  ++journal->image.nonce;
  std::strncpy(journal->image.path, dir.c_str(), sizeof journal->image.path - 1);
  journal->image.path[sizeof journal->image.path - 1] = 0;
  return Qnil;
}

RB_METHOD(journalSetLang) {
  RB_UNUSED_PARAM;
  const char *lang;
  rb_get_args(argc, argv, "z", &lang RB_ARG_END);

  // FIXME language handling

  return Qnil;
}

static bool journal_active(const JournalGuard &) {
  return journal->active_count;
}

RB_METHOD(journalActive) {
  RB_UNUSED_PARAM;
  JournalGuard guard(*journal, false);
  return journal_active(guard) ? Qtrue : Qfalse;
}

RB_METHOD(journalPosition) {
  RB_UNUSED_PARAM;

  JournalGuard guard(*journal, false);

  if (!journal_active(guard))
    return Qnil;

  return rb_ary_new_from_args(2, INT2FIX(journal->get_journal_position.x), RB_INT2FIX(journal->get_journal_position.y));
}

RB_METHOD(setJournalPosition) {
  int x, y;
  rb_get_args(argc, argv, "ii", &x, &y);

  JournalGuard guard(*journal, true);
  ++journal->set_journal_position.nonce;
  journal->set_journal_position.x = x;
  journal->set_journal_position.y = y;
  return Qnil;
}

RB_METHOD(journalQuit) {
  Debug() << "sending quit";

  JournalGuard guard(*journal, true);
  ++journal->close.nonce;
  return Qnil;
}

void cleanup_journal_stuff() {
  deinit_journal(journal_shm, journal_region, journal);

  boost::interprocess::shared_memory_object::remove(JOURNAL_SHM_NAME);
}

void oneshotJournalBindingInit() {
  init_journal(journal_shm, journal_region, journal);

  VALUE module = rb_define_module("Journal");
  _rb_define_module_function(module, "set", journalSet);
  _rb_define_module_function(module, "active?", journalActive);
  _rb_define_module_function(module, "setLang", journalSetLang);
  _rb_define_module_function(module, "journal_position", journalPosition);
  _rb_define_module_function(module, "set_journal_position",
                             setJournalPosition);
  _rb_define_module_function(module, "quit", journalQuit);
}
