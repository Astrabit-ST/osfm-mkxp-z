#ifndef JOURNAL_COMMON_H
#define JOURNAL_COMMON_H

#include <cassert>
#include <cstdint>
#include <boost/interprocess/mapped_region.hpp>
#include <boost/interprocess/shared_memory_object.hpp>
#include <boost/interprocess/sync/interprocess_condition.hpp>
#include <boost/interprocess/sync/interprocess_mutex.hpp>

#define JOURNAL_VERSION 1
#define JOURNAL_SHM_NAME "osfm_journal"

struct Journal {
  friend struct JournalGuard;

  uint64_t version;

  // Number of journals connected
  size_t active_count;

  struct {
    // This gets incremented each time this struct is updated
    uint64_t nonce;
    // Path of the image the game window wants the journal window to display, or an empty string for no image
    char path[4096];
  } image;

  struct {
    // This gets incremented each time the journal is requested to close
    uint64_t nonce;
  } close;

  struct {
    // Position of the journal window
    int x, y;
  } get_journal_position;

  struct {
    // Size of the journal window
    int w, h;
  } get_journal_size;

  struct {
    // This gets incremented each time this struct is updated
    uint64_t nonce;
    // Position of the journal window
    int x, y;
  } set_journal_position;

  struct {
    // This gets incremented each time this struct is updated
    uint64_t nonce;
    // Size of the journal window
    int w, h;
  } set_journal_size;

  boost::interprocess::interprocess_condition cond;

private:
  boost::interprocess::interprocess_mutex mutex;

public:
  Journal() : version(JOURNAL_VERSION), active_count(0) {
    *image.path = 0;
  }
};

struct JournalGuard {
private:
 struct Journal &journal;
 bool locked;

public:
  typedef boost::interprocess::interprocess_mutex mutex_type;

  mutex_type *mutex() noexcept {
    return &journal.mutex;
  }

  operator bool() noexcept {
    return locked;
  }

  void lock() {
    assert(!locked);
    try {
      journal.mutex.lock();
    } catch (boost::interprocess::interprocess_exception &) {
      // If a process locks a mutex and then dies without unlocking it, the mutex is permanently poisoned on both POSIX and Windows, so nuke the shared memory
      boost::interprocess::shared_memory_object::remove(JOURNAL_SHM_NAME);
      throw;
    }
    locked = true;
  }

  void unlock() {
    assert(locked);
    journal.mutex.unlock();
    locked = false;
  }

  JournalGuard(struct Journal &journal, bool write) : journal(journal), locked(false) {
    lock();
    if (write) {
      journal.cond.notify_all();
    }
  }

  JournalGuard(const struct JournalGuard &guard) = delete;

  JournalGuard(struct JournalGuard &&guard) noexcept = delete;

  struct JournalGuard &operator=(const struct JournalGuard &guard) = delete;

  struct JournalGuard &operator=(struct JournalGuard &&guard) noexcept = delete;

  ~JournalGuard() {
    if (locked) {
      unlock();
    }
  }
};

static inline void deinit_journal(boost::interprocess::shared_memory_object &journal_shm, boost::interprocess::mapped_region &journal_region, struct Journal *&journal) {
  journal = nullptr;
  journal_region = boost::interprocess::mapped_region();
  journal_shm = boost::interprocess::shared_memory_object();
}

static inline void init_journal(boost::interprocess::shared_memory_object &journal_shm, boost::interprocess::mapped_region &journal_region, struct Journal *&journal) {
  deinit_journal(journal_shm, journal_region, journal);

  bool journal_needs_create = false;

  try {
    journal_shm = boost::interprocess::shared_memory_object(boost::interprocess::open_only, JOURNAL_SHM_NAME, boost::interprocess::read_write);
  } catch (boost::interprocess::interprocess_exception &) {
    journal_needs_create = true;
  }

  if (!journal_needs_create) {
    journal_shm = boost::interprocess::shared_memory_object(boost::interprocess::open_only, JOURNAL_SHM_NAME, boost::interprocess::read_write);
    journal_region = boost::interprocess::mapped_region(journal_shm, boost::interprocess::read_write);
    if (journal_region.get_size() != sizeof(struct Journal) || (journal = (struct Journal *)journal_region.get_address())->version != JOURNAL_VERSION) {
      journal_needs_create = true;
      journal = nullptr;
      journal_region = boost::interprocess::mapped_region();
      journal_shm = boost::interprocess::shared_memory_object();
    }
  }

  if (journal_needs_create) {
    boost::interprocess::shared_memory_object::remove(JOURNAL_SHM_NAME);
    journal_shm = boost::interprocess::shared_memory_object(boost::interprocess::create_only, JOURNAL_SHM_NAME, boost::interprocess::read_write);
    journal_shm.truncate(sizeof(struct Journal));
    journal_region = boost::interprocess::mapped_region(journal_shm, boost::interprocess::read_write);
    journal = new(journal_region.get_address()) Journal;
  }
}

#endif
