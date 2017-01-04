#ifndef POLLER_H
#define POLLER_H

#include "RockContext.h"
#include "queues.h"

class ClientPr{
  RockContext* rct;
  std::string name;
  uint32_t vqpn;
  int fd;
  char* ptr;
  public:
  ClientPr(RockContext* ir, uint32_t iv, int ifd, char* ip) :
    rct(ir), name("@ClientPr:"), vqpn(iv), fd(ifd), ptr(ip) {}
};

class Poller {
  RockContext* rct;
  std::string name;
  shared_ptr<CQ>  cq;
  std::thread t;
  std::mutex mutex;
  std::map<uint32_t, ClientPr*> book;
  IB* ib;
  public:
  Poller(RockContext* ir, shared_ptr<CQ> icq) : rct(ir), cq(ib->get_cq()), ib(rct->get_ib()) {
    t = std::thread(&Poller::run, this);  
  }
  void run();
  void dispatch(uint32_t vqpn, int fd, char* ptr) {
    std::lock_guard<std::mutex> m(mutex);
    assert(book.find(vqpn) == book.end());
    book[vqpn] = new ClientPr(rct, vqpn, fd, ptr);
  }
};

#endif
