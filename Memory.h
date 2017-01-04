#ifndef MEMORY_H
#define MEMORY_H

#include "RockContext.h"

class SharedChunks
{
  RockContext *rct;
  key_t key;
  void *ptr;
  std::string name;
  uint32_t amount;

public:
  SharedChunks(RockContext *ir, key_t ik, uint32_t ia)
      : rct(ir), amount(ia), key(ik), name("@SharedChunks:")
  {
    int shmid = shmget(key, 0, 0);
    if (shmid == -1)
    {
      loginfo(1) << "shmget failed.";
      assert(0);
    }
    ptr = shmat(shmid, 0, 0);
    if (ptr == (void *)-1)
    {
      loginfo(1) << "shmat failed.";
      assert(0);
    }
  }
  ~SharedChunks()
  {
    shmdt(ptr);
  }
};

class AppShmInfo
{
  RockContext *rct;
  std::vector<SharedChunks *> ctl_chunks;
  std::vector<SharedChunks *> data_chunks;
  int app_pid;
  string name;

public:
  explicit AppShmInfo(RockContext *ir, key_t ik1, key_t ik2, int iap) : rct(ir),
                                                               app_pid(iap), name("@AppShmInfo:")
  {
    SharedChunks *sc = new SharedChunks(rct, ik1, rct->app_ctl_shm_amount);
    ctl_chunks.push_back(sc);
    sc = new SharedChunks(rct, ik2, rct->app_data_shm_amount);
    data_chunks.push_back(sc);
  }
};

class AppsMemoryManager{
  RockContext* rct;
  string name;
  std::map<uint32_t, AppShmInfo*> book;// pid : process's shared memory 
  std::mutex mutex;
  public:
  AppsMemoryManager(RockContext* ir) : rct(ir), name("@AppsMemoryManager:") {}
  void add(uint32_t pid, AppShmInfo* asi) {
    std::lock_guard<std::mutex> m(mutex);
    assert(book.find(pid) == book.end());
    book[pid] = asi;
  }

  char* get_ctl_addr(uint32_t, int);
};
#endif
