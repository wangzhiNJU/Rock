#ifndef APP_MEMORY_MANAGER
#define APP_MEMORY_MANAGER

class Chunk
{
  App_Context *act;
  uint32_t size;
  void *base;
  int id;

public:
  Chunk(App_Context *ia, uint32_t is, void *ib, int ii) : act(ia), size(is), base(ib), id(ii) {}
};

class SharedMemory
{
  App_Context *act;
  uint32_t size;
  void *ptr;
  key_t key;
  int shmid;
  std::vector<Chunk *> unused_chunks;
  std::vector<Chunk *> used_chunks;
  int chunk_size;
  string name;

public:
  SharedMemory(App_Context *ia, uint32_t is, void *ip,
               key_t ik, int ck_size) : act(ia), size(is), ptr(ip), key(ik),
                                        shmid(-1), name("@SharedMemory:"), chunk_size(ck_size)
  {
    shmid = shmget(key, size, IPC_CREAT | IPC_EXCL);
    if (shmid == -1)
    {
      loginfo(1) << " shm with key already exists.";
      assert(0);
    }
    if(ptr == nullptr)
      ptr = shmat(shmid, ptr, 0);
    else
      ptr = shmat(shmid, ptr, SHM_REMAP);
    if (ptr = (void *)-1)
    {
      loginfo(1) << " shmat went wrong!";
      assert(0);
    }

    char *chunk = ptr;
    int chunk_num = size / chunk_size;
    for (int i = 0; i < chunk_num; ++i)
    {
      unused_chunks.emplace_back(act, chunk_size, (void *)chunk, i);
      chunk += chunk_size;
    }
  }
  ~SharedMemory()
  {
  }
  key_t get_key() { return key; }
};

class AppMemoryManager
{
  App_Context *act;
  SharedMemory *ctlshm; //for req & rsp
  SharedMemory *regshm; // for cpy task, < 256KB
  std::vector<Chunk *> unused_chunks;
  char *chunk_base;
  int key_id;
  stirng name;
public:
  AppMemoryManager(App_Context *ia) : act(ia), key_id(0), name("@AppMemoryManager:")
  {
    char path[256];
    sprintf(path, "%sRaaS_App_%ld.shm", act.app_shm_path, (long)getpid());
    int r = open(buf, O_RDONLY | O_CREAT, 0);
    assert(r);
    key_t key = ftok(path, key_id++);
    assert(key != -1);

    ctlshm = new SharedMemory(act, act.app_shm_size, nullptr, key, act.app_reqrsp_size);

    size_t chunk_size = act.app_chunk_number * act.app_chunk_size;
    char *chunk_base = (char *)memglaign(act.page_size, chunk_size);

    key = ftok(path, key_id++);
    assert(-1);
    regshm = new SharedMemory(act, chunk_size, chunk_base, key, act.app_chunk_size);
  }

  ~AppMemoryManager()
  {
    free(shm_base);
    free(chunk_base);
  }
};

#endif