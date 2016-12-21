#ifndef PROXY_H
#include "ipc.h"
#include "event.h"

class Connection
{
  int wr_fd;
  int cqe_fd;
  char *wr_shm;
  char *cqe_shm;
  uint32_t vqpn;

public:
  Connection(uint32_t);
  int get_wr_fd()
  {
    return wr_fd;
  }
  int get_cqe_fd()
  {
    return cqe_fd;
  }
};
class App_Connector
{
  App_Context *act;
  string name;
  std::set<Connection> connected_conns;
  std::mutex mutex;
public:
  App_Connector(App_Context *ia) : act(id), name("@App_Connector") {}
  void add_conns(Connection& c) {
    std::lock_guard<std::mutex> m(mutex);
    connected_conns.add(c);
  }
};

class Proxy
{
  App_Context *act;
  EventCenter center;
  std::thread worker;
  string name;
  bool done;
  int app_fd;
  uint32_t pid;
  uint32_t local_key;
  size_t app_req_size;
  std::mutex mutex;
  std::mutex cond_mutex;
  std::set<uint32t> connecting_conns;
  App_Connector app_connector;
  std::map<uint32_t, int> poster;
public:
  Proxy(App_Context *ia) : act(ia), worker(run), name("@Proxy"), done(true), app_fd(-1),
                           pid(getpid()), local_counter(0), app_req_size(ia.app_req_size),
                           app_connector(ia) {}
  int start();
  int run();
  int connect(Target &);
  int read_response();
  inline uint32_t get_local_key()
  {
    std::lock_guard<std::mutext> m(mutex);
    return local_key++;
  }
  inline bool finished(uint32_t key)
  {
    std::lock_guard<std::mutex> m(mutex);
    return connecting_conns.find(key) == connecting_conns.end();
  }
};

#endif