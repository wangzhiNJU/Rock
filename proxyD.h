#ifndef PROXYD_H
#define PROXYD_H
#include "RockContext.h"
#include "ab.h"
#include "event.h"
#include "queues.h"
#include "Memory.h"
#include "tasktracker.h"
#include "poller.h"

class QP;
class TaskTracker;
class Connector
{
  RockContext *rct;
  EventCenter center;
  std::map<Target, QP *> connected_qps;
  std::multiset<Target> connecting_qps;
  std::map<int, QP *> bridges;
  size_t app_req_size;
  string name;
  std::mutex mutex;
  std::set<uint32_t> vqpns;
  std::map<uint32_t, int> local_map; //pid : app_fd
  enum
  {
    CONN_RSP_TO_APP = 11, //
    BUILD_QP_REQ = 123,
    BUILD_QP_RSP = 169,
    OPT_VQPN_REQ = 246,
    OPT_VQPN_RSP = 369
  };
  Callback* ar_callback;
  Callback* pr_callback;
  IB* ib;
  IPC ipc;
  TaskTracker *tasktracker;
  Poller* poller;
  AppsMemoryManager* apps_mm;

  class AcceptRequestCallback : public Callback {
    Connector* ctor;
   public:
    AcceptRequestCallback(Connector* c) : ctor(c) {}
    void callback(int fd) {
      ctor->accept_request(fd);  
    }
  };

  class ProcessRequestCallback : public Callback {
    Connector* ctor;
   public:
    ProcessRequestCallback(Connector* c) : ctor(c) {}
    void callback(int fd) {
      ctor->process_request(fd);  
    }
  };

public:
  Connector(RockContext *ir) : rct(ir), app_req_size(ir->app_req_size), name("@Connector:"),
      center(ir), ar_callback(new AcceptRequestCallback(this)), pr_callback(new ProcessRequestCallback(this)),
      ib(rct->get_ib()), ipc(ir), tasktracker(rct->get_tasktracker()), poller(rct->get_poller()), apps_mm(rct->get_AppsMemoryManager()) {}
  ~Connector() {}
  void add_client(int fd, uint32_t pid)
  {
    center.add_event(fd, ar_callback);
    std::lock_guard<std::mutex> m(mutex);
    assert(local_map.find(pid) == local_map.end());
    local_map[pid] = fd;
  }
  void add_remote(int fd, Target &t)
  {
    center.add_event(fd, pr_callback);
    QP *qp = rct->ib->create_qp();
    std::lock_guard<std::mutex> m(mutex);
    connecting_qps.insert(t);
    bridges.insert(std::make_pair(fd, qp));
  }
  void accept_request(int);
  void process_request(int);
  void build_qp(int, int);
  void opt_vqpn_req(int);
  void opt_vqpn_rsp(int);
  void parse_app_req(char*, int);
  void try_build_tcp(Target&, int);
  int send_req_vqpns(int, Target&, uint32_t*, int);
  void close_connection(uint32_t, uint32_t);
  int choose_vqpns(Target&, uint32_t*, int);
};

class Acceptor
{
  RockContext *rct;
  shared_ptr<Connector> connector;
  bool done;
  IPC ipc;
public:
  Acceptor(RockContext *ir) : rct(ir), ipc(ir) {}
  void start();
};

class ProxyD
{
  RockContext *rct;
  shared_ptr<Connector> connector;
  int listener_fd;
  std::thread worker;
  bool done;
  IPC ipc;
  int app_req_size;
  AppsMemoryManager* apps_mm;
public:
  ProxyD(RockContext *ir) : rct(ir), connector(new Connector(ir)),worker(&ProxyD::run, this), 
                            ipc(ir), app_req_size(rct->app_req_size), apps_mm(rct->get_AppsMemoryManager()) {
    worker = std::thread(&ProxyD::run, this);                            
  }
  int start();
  void run();
};
#endif
