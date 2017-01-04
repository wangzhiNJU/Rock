#include "queues.h"

CQ::CQ(RockContext* ir, ibv_context* ctxt, int id) : rct(ir), depth(id) {
  cq = ibv_create_cq(ctxt, depth, nullptr, nullptr, 0);
  assert(cq);
}

SRQ::SRQ(RockContext* ir, ibv_context* ctxt, ibv_pd* pd) : rct(ir) {
  ibv_srq_init_attr sia;
  memset(&sia, 0, sizeof(sia));
  sia.srq_context = ctxt;
  sia.attr.max_wr = rct->srq_max_wr;
  sia.attr.max_sge = rct->srq_max_sge;
  assert(!ibv_create_srq(pd, &sia));
}

QP::QP(RockContext* ir, Device* id, shared_ptr<SRQ> is, shared_ptr<CQ> ic, ibv_qp_type qp_type) : rct(ir), device(id), srq(is), cq(ic), ib(rct->get_ib()) {
  ibv_qp_init_attr qpia;
  memset(&qpia, 0, sizeof(qpia));
  qpia.send_cq = cq->get_cq();
  qpia.recv_cq = cq->get_cq();
  qpia.srq = srq->get_srq();
  qpia.cap.max_send_wr = rct->qp_max_send_wr;
  qpia.cap.max_send_sge = rct->qp_max_send_sge;
  qpia.cap.max_inline_data = rct->qp_max_inline_data;
  qpia.qp_type = qp_type;
  qpia.sq_sig_all = 0;

  qp = ibv_create_qp(device->get_pd(), &qpia);
  assert(qp);

  local_syn.qpn = qp->qp_num;
  local_syn.local_psn = lrand48() & 0xffffff;
  local_syn.lid = ib->get_lid();
  local_syn.gid = ib->get_gid();
  local_syn.dest_psn = 0;
}

int QP::init() {
  // RESET to INIT
  ibv_qp_attr qpa;
  memset(&qpa, 0, sizeof(qpa));
  qpa.qp_state = IBV_QPS_INIT;
  qpa.pkey_index = 0;
  qpa.port_num = rct->active_port_num;
  qpa.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE;

  int mask = IBV_QP_STATE | IBV_QP_PORT;
  switch (type) {
    case IBV_QPT_RC:
      mask |= IBV_QP_ACCESS_FLAGS;
      mask |= IBV_QP_PKEY_INDEX;
      break;
    case IBV_QPT_UD:
      mask |= IBV_QP_QKEY;
      mask |= IBV_QP_PKEY_INDEX;
      break;
    case IBV_QPT_RAW_PACKET:
      break;
    default:
      assert(0);
  }
  
  int r = ibv_modify_qp(qp, &qpa, mask);
  assert(!r);
  return 0;
}

int QP::activate(IBSynMsg& id) {
  dest_syn = id;
  //move to RTR
  ibv_qp_attr qpa;
  memset(&qpa, 0, sizeof(qpa));
  qpa.qp_state = IBV_QPS_RTR;
  qpa.path_mtu = IBV_MTU_1024;
  qpa.dest_qp_num = dest_syn.qpn;
  qpa.rq_psn = dest_syn.local_psn;
  qpa.max_dest_rd_atomic = rct->qp_dest_rd_atomic;
  qpa.min_rnr_timer = 12;

  qpa.ah_attr.is_global = 1;
  qpa.ah_attr.grh.dgid = id.gid;

  qpa.ah_attr.port_num = rct->active_port_num;

  int r = ibv_modify_qp(qp, &qpa,IBV_QP_STATE |
          IBV_QP_PATH_MTU |
          IBV_QP_DEST_QPN |
          IBV_QP_RQ_PSN |
          IBV_QP_MAX_DEST_RD_ATOMIC |
          IBV_QP_MIN_RNR_TIMER);
  assert(!r);

  //move to RTS
  qpa.qp_state = IBV_QPS_RTS;
  qpa.timeout = 14;
  qpa.retry_cnt = 7;
  qpa.rnr_retry = 7;
  qpa.sq_psn = local_syn.local_psn;
  qpa.max_rd_atomic = rct->qp_rd_atomic;
  r = ibv_modify_qp(qp, &qpa, IBV_QP_STATE |
      IBV_QP_TIMEOUT |
      IBV_QP_RETRY_CNT |
      IBV_QP_RNR_RETRY |
      IBV_QP_SQ_PSN |
      IBV_QP_MAX_QP_RD_ATOMIC);
  assert(!r);

  return 0;
}
