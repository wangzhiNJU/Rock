#ifndef PROXYD_H
#define PROXYD_H
#include "ab.h"
#include "event.h"

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

public:
  Connector(RockContext *ir) : rct(ir), app_req_size(ir.app_req_size), name("@Connector:") {}
  ~Connector() {}
  void add_client(int fd)
  {
    center.add_event(fd, accept_request);
    char buf[25];
    ssize_t n = read(fd, buf, 25);
    assert(n >= 0);
    uint32_t pid = 0;
    sscanf(buf, "%010u", &pid);
    std::lock_guard<std::mutex> m(mutex);
    assert(local_map.find(pid) == local_map.end());
    local_map[pid] = fd;
  }
  void add_remote(int fd, Target &t)
  {
    eventCenter.add_event(fd, process_request);
    QP *qp = new QP(rct, ib->get_device(), ib->get_srq(), ib->get_cq());
    qp->init();
    std::lock_guard<std::mutex> m(mutex);
    connecting_qps.insert(t);
    bridges.insert(make_pair(fd, qp));
  }
  void accept_request(int);
  void process_request(int);
};

class Acceptor
{
  RockContext *rct;
  shared_ptr<Connector> connector;
  bool done;

public:
  Acceptor(RockContext *ir) : rct(ir) {}
  void start();
  void
};

class ProxyD
{
  RockContext *rct;
  shared_ptr<Connector> connector;
  int listener_fd;
  std::thread worker;
  bool done;
  IPC ipc;

public:
  ProxyD(RockContext *ir) : rct(ir), connector(new Connector(ir)),worker(run) {}
  int start();
  void run();
};
#endif
