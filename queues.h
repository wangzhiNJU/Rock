#ifndef QUEUES_H
#define QUEUES_H
#include "ab.h"
#include "ipc.h"

//completion queue
class CQ {
  RockContext* rct;
  ibv_cq* cq;
  int depth;
 public:
  CQ(RockContext*, ibv_context*, int depth);
  ibv_cq* get_cq() { return cq; }
  ~CQ() {
    assert(!ibv_destroy_cq(cq));    
  }
};

//shared receive queue
class SRQ {
  RockContext* rct;
  ibv_srq* srq;
 public:
  SRQ(RockContext*, ibv_context*, ibv_pd*);
  ~SRQ() {
    assert(!ibv_destroy_srq(srq));
  }
  ibv_srq* get_srq() { return srq; }
};

//queue pair
class QP {
  RockContext* rct;
  ibv_qp* qp;
  shared_ptr<Device> device;
  shared_ptr<SRQ> srq;
  shared_ptr<CQ> cq;
  IBSynMsg local_syn;
  IBSynMsg dest_syn;
  Target target;
  ibv_qp_type type;
  IB* ib;
 public:
  QP(RockContext*, Device* d, shared_ptr<SRQ>, shared_ptr<CQ>, ibv_qp_type t = IBV_QPT_RC);
  ibv_qp* get_qp() { return qp; }
  ~QP() {
    assert(!ibv_destroy_qp(qp));
  }
  int init();
  int activate(IBSynMsg&);
  void set_target(Target &t) { target = t; }
  Target& get_target() { return target; }
  IBSynMsg& get_local_syn();
};

class IB {
  RockContext* rct;
  DeviceList device_list;
  Device* device;
  shared_ptr<CQ> cq;
  shared_ptr<SRQ> srq;
 public:
  IB(RockContext* ir) : rct(ir), device_list(ir),
    cq(new CQ(rct, device->get_context(), rct->cq_depth)),
    srq(new SRQ(rct, device->get_context(), device->get_pd())) {
    device = device_list.get_device(rct->device_name);
    assert(device);
  }
  shared_ptr<CQ> get_cq() {
    return cq;
  }
  shared_ptr<SRQ> get_srq() {
    return srq;
  }
  QP* create_qp();
  Device* get_device() {
    return device;  
  }
  void get_msg_from_wire(IBSynMsg*);
  void get_wire_from_msg(char*, IBSynMsg&);
  uint16_t get_lid();
  ibv_gid get_gid();
};
#endif
