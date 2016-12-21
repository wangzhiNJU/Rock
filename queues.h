#ifndef QUEUES_H
#include "ab.h"

//completion queue
class CQ {
  RockConext* rct;
  ibv_cq* cq;
  int depth;
 public:
  CQ(RockConext*, ibv_context*, int depth);
  ibv_cq* get_cq() { return cq; }
  ~CQ() {
    assert(!ibv_destroy_cq(cq));    
  }
};

//shared receive queue
class SRQ {
  RockConext* rct;
  ibv_srq* srq;
 public:
  SRQ(RockConext*, ibv_context*, ibv_pd*);
  ~SRQ() {
    assert(!destroy_srq(srq));
  }
  ibv_srq* get_srq() { return srq; }
};

//queue pair
class QP {
  RockConext* rct;
  ibv_qp* qp;
  shared_ptr<Device> device;
  shared_ptr<SRQ> srq;
  shared_ptr<CQ> cq;
  IBSynMsg local_syn;
  IBSynMsg dest_syn;
  Target target;
 public:
  QP(RockConext*, Device* d, SRQ*, CQ*, ibv_qp_type t = IBV_OPT_RC);
  ibv_qp* get_qp() { return qp; }
  ~QP {
    assert(!ibv_destroy_qp(qp));
  }
  int init();
  int activate();
  void set_target(Target &t) { target = t; }
  Target& get_target() { return target; }
};

class IB {
  RockConext* rct;
  DeviceList device_list;
  Device* device;
  shared_ptr<CQ> cq;
  shared_ptr<SRQ> srq;
 public:
  IB(RockConext* ir) : rct(ir) {
    device = device_list.get_device(rct->device_name);
    assert(device);
    cq = new CQ(rct, device->get_context(), rct->cq_depth);
    srq = new SRQ(rct, device->get_context(), device->get_pd());
  }
  shared_ptr<CQ> get_cq() {
    return cq;
  }
  shared_ptr<SRQ> get_srq() {
    return srq;
  }
};
#endif
