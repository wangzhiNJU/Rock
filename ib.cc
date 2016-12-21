// -*- mode:C++; tab-width:8; c-basic-offset:2; indent-tabs-mode:t -*- 
// vim: ts=8 sw=2 smarttab
/*
 * Ceph - scalable distributed file system
 *
 * Copyright (C) 2016 XSKY <haomai@xsky.com>
 *
 * Author: Haomai Wang <haomaiwang@gmail.com>
 *
 * This is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License version 2.1, as published by the Free Software
 * Foundation.  See file COPYING.
 *
 */

#include "ib.h"

static const uint32_t MAX_SHARED_RX_SGE_COUNT = 1;
static const uint32_t MAX_INLINE_DATA = 128;
static const uint32_t UDP_MSG_LEN = sizeof("0000:00000000:00000000:00000000:00000000000000000000000000000000");
static const uint32_t CQ_DEPTH = 30000;

Device::Device(RDMAContext *c, ibv_device* d): cct(c), device(d), device_attr(new ibv_device_attr), active_port(nullptr)
{
  if (device == nullptr) {
    LOG(INFO) << __func__ << "device == nullptr";
    assert(0);
  }
  name = ibv_get_device_name(device);
  ctxt = ibv_open_device(device);
  if (ctxt == nullptr) {
    LOG(INFO) << __func__ << "open rdma device failed. ";
    assert(0);
  }
  int r = ibv_query_device(ctxt, device_attr);
  if (r == -1) {
    LOG(INFO) << __func__ << " failed to query rdma device. ";
    assert(0);
  }
}
void Device::binding_port(uint8_t port_num) {
  port_cnt = device_attr->phys_port_cnt;
  ports = new Port*[port_cnt];
  for (uint8_t i = 0; i < port_cnt; ++i) {
    ports[i] = new Port(cct, ctxt, i+1);
    if (i+1 == port_num && ports[i]->get_port_attr()->state == IBV_PORT_ACTIVE) {
      active_port = ports[i];
      LOG(INFO) << __func__ << " found active port " << i+1;
      return ;
    } else {
      LOG(INFO) << __func__ << " port " << i+1 << " is not what we want. state: " << ports[i]->get_port_attr()->state << ")";
    }
  }
  if (nullptr == active_port) {
    LOG(INFO) << __func__ << "  port not found";
    assert(active_port);
  }
}

Infiniband::Infiniband(RDMAContext *c, const std::string &device_name, uint8_t port_num): cct(c), device_list(c), net(c)
{
  device = device_list.get_device(device_name.c_str());
  device->binding_port(port_num);
  assert(device);
  ib_physical_port = device->active_port->get_port_num();
  pd = new ProtectionDomain(cct, device);
  assert(net.set_nonblock(device->ctxt->async_fd) == 0);

  max_recv_wr = device->device_attr->max_srq_wr;
  if (max_recv_wr < cct->rdma_receive_buffers) {
    LOG(INFO) << __func__ << " max allowed receive buffers is " << max_recv_wr << " use this instead.";
    max_recv_wr = cct->rdma_receive_buffers;
  }
  max_send_wr = device->device_attr->max_qp_wr;
  if (max_send_wr < cct->rdma_send_buffers) {
    LOG(INFO) << __func__ << " max allowed send buffers is " << max_send_wr << " use this instead.";
    max_send_wr = cct->rdma_send_buffers;
  }

  LOG(INFO) << __func__ << " device allow " << device->device_attr->max_cqe
                << " completion entries";

  memory_manager = new MemoryManager(cct, device, pd);
  memory_manager->register_rx_tx(
      cct->rdma_buffer_size,
      cct->rdma_receive_buffers,
      cct->rdma_send_buffers);

  srq = create_shared_receive_queue(max_recv_wr, MAX_SHARED_RX_SGE_COUNT);
  post_channel_cluster();
}

/**
 * Create a shared receive queue. This basically wraps the verbs call. 
 *
 * \param[in] max_wr
 *      The max number of outstanding work requests in the SRQ.
 * \param[in] max_sge
 *      The max number of scatter elements per WR.
 * \return
 *      A valid ibv_srq pointer, or NULL on error.
 */
ibv_srq* Infiniband::create_shared_receive_queue(uint32_t max_wr, uint32_t max_sge)
{
  LOG_IF(INFO, cct->loglevel < 20) << __func__ << " max_wr=" << max_wr << " max_sge=" << max_sge;
  ibv_srq_init_attr sia;
  memset(&sia, 0, sizeof(sia));
  sia.srq_context = device->ctxt;
  sia.attr.max_wr = max_wr;
  sia.attr.max_sge = max_sge;
  return ibv_create_srq(pd->pd, &sia);
}

/**
 * Create a new QueuePair. This factory should be used in preference to
 * the QueuePair constructor directly, since this lets derivatives of
 * Infiniband, e.g. MockInfiniband (if it existed),
 * return mocked out QueuePair derivatives.
 *
 * \return
 *      QueuePair on success or NULL if init fails
 * See QueuePair::QueuePair for parameter documentation.
 */
Infiniband::QueuePair* Infiniband::create_queue_pair(CompletionQueue *tx, CompletionQueue* rx, ibv_qp_type type)
{
  Infiniband::QueuePair *qp = new QueuePair(*this, type, ib_physical_port, srq, tx, rx, max_send_wr, max_recv_wr);
  if (qp->init()) {
    delete qp;
    return nullptr;
  }
  return qp;
}

int Infiniband::QueuePair::init()
{
  LOG_IF(INFO, infiniband.cct->loglevel < 20) << __func__ << " started.";
  ibv_qp_init_attr qpia;
  memset(&qpia, 0, sizeof(qpia));
  qpia.send_cq = txcq->get_cq();
  qpia.recv_cq = rxcq->get_cq();
  qpia.srq = srq;                      // use the same shared receive queue
  qpia.cap.max_send_wr  = max_send_wr; // max outstanding send requests
  qpia.cap.max_send_sge = 1;           // max send scatter-gather elements
  qpia.cap.max_inline_data = MAX_INLINE_DATA;          // max bytes of immediate data on send q
  qpia.qp_type = type;                 // RC, UC, UD, or XRC
  qpia.sq_sig_all = 0;                 // only generate CQEs on requested WQEs

  qp = ibv_create_qp(pd, &qpia);
  if (qp == NULL) {
    LOG(INFO) << __func__ << " failed to create queue pair";
    return -1;
  }

  LOG_IF(INFO, infiniband.cct->loglevel < 20) << __func__ << " successfully create queue pair: "
    << "qp=" << qp;

  // move from RESET to INIT state
  ibv_qp_attr qpa;
  memset(&qpa, 0, sizeof(qpa));
  qpa.qp_state   = IBV_QPS_INIT;
  qpa.pkey_index = 0;
  qpa.port_num   = (uint8_t)(ib_physical_port);
  qpa.qp_access_flags = IBV_ACCESS_REMOTE_WRITE | IBV_ACCESS_LOCAL_WRITE;
  qpa.qkey       = q_key;

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

  int ret = ibv_modify_qp(qp, &qpa, mask);
  if (ret) {
    ibv_destroy_qp(qp);
    LOG(INFO) << __func__ << " failed to transition to INIT state";
    return -1;
  }
  LOG(INFO) << __func__ << " successfully change queue pair to INIT:"
    << " qp=" << qp;
  return 0;
}

/**
 * Change RC QueuePair into the ERROR state. This is necessary modify
 * the Queue Pair into the Error state and poll all of the relevant
 * Work Completions prior to destroying a Queue Pair.
 * Since destroying a Queue Pair does not guarantee that its Work
 * Completions are removed from the CQ upon destruction. Even if the
 * Work Completions are already in the CQ, it might not be possible to
 * retrieve them. If the Queue Pair is associated with an SRQ, it is
 * recommended wait for the affiliated event IBV_EVENT_QP_LAST_WQE_REACHED
 *
 * \return
 *      -errno if the QueuePair can't switch to ERROR
 *      0 for success.
 */
int Infiniband::QueuePair::to_dead()
{
  if (dead)
    return 0;
  ibv_qp_attr qpa;
  memset(&qpa, 0, sizeof(qpa));
  qpa.qp_state = IBV_QPS_ERR;

  int mask = IBV_QP_STATE;
  int ret = ibv_modify_qp(qp, &qpa, mask);
  if (ret) {
    LOG(INFO) << __func__ << " failed to transition to ERROR state: ";
    return -errno;
  }
  dead = true;
  return ret;
}

int Infiniband::post_chunk(Chunk* chunk)
{
  ibv_sge isge;
  isge.addr = reinterpret_cast<uint64_t>(chunk->buffer);
  isge.length = chunk->bytes;
  isge.lkey = chunk->mr->lkey;
  ibv_recv_wr rx_work_request;

  memset(&rx_work_request, 0, sizeof(rx_work_request));
  rx_work_request.wr_id = reinterpret_cast<uint64_t>(chunk);// stash descriptor ptr
  rx_work_request.next = NULL;
  rx_work_request.sg_list = &isge;
  rx_work_request.num_sge = 1;

  ibv_recv_wr *badWorkRequest;
  int ret = ibv_post_srq_recv(srq, &rx_work_request, &badWorkRequest);
  if (ret) {
    LOG(INFO) << __func__ << " ib_post_srq_recv failed on post ";
    return -1;
  }
  return 0;
}

int Infiniband::post_channel_cluster()
{
  vector<Chunk*> free_chunks;
  int r = memory_manager->get_channel_buffers(free_chunks, 0);
  assert(r == 0);
  for (vector<Chunk*>::iterator iter = free_chunks.begin(); iter != free_chunks.end(); ++iter) {
    r = post_chunk(*iter);
    assert(r == 0);
  }
  LOG_IF(INFO, cct->loglevel < 20) << __func__ << " posted buffers to srq.";
  return 0;
}

Infiniband::CompletionChannel* Infiniband::create_comp_channel()
{
  LOG_IF(INFO, cct->loglevel < 20) << __func__ << " started.";
  Infiniband::CompletionChannel *cc = new Infiniband::CompletionChannel(*this);
  if (cc->init()) {
    delete cc;
    return nullptr;
  }
  return cc;
}

Infiniband::CompletionQueue* Infiniband::create_comp_queue(CompletionChannel *cc)
{
  Infiniband::CompletionQueue *cq = new Infiniband::CompletionQueue(*this, CQ_DEPTH, cc);
  if (cq->init()) {
    delete cq;
    return nullptr;
  }
  return cq;
}


Infiniband::QueuePair::QueuePair(
    Infiniband& infiniband, ibv_qp_type type, int port, ibv_srq *srq,
    Infiniband::CompletionQueue* txcq, Infiniband::CompletionQueue* rxcq,
    uint32_t max_send_wr, uint32_t max_recv_wr, uint32_t q_key)
: infiniband(infiniband),
  type(type),
  ctxt(infiniband.device->ctxt),
  ib_physical_port(port),
  pd(infiniband.pd->pd),
  srq(srq),
  qp(nullptr),
  txcq(txcq),
  rxcq(rxcq),
  initial_psn(0),
  max_send_wr(max_send_wr),
  max_recv_wr(max_recv_wr),
  q_key(q_key),
  dead(false)
{
  initial_psn = lrand48() & 0xffffff;
  if (type != IBV_QPT_RC && type != IBV_QPT_UD && type != IBV_QPT_RAW_PACKET) {
    LOG(INFO) << __func__ << "invalid queue pair type";
    assert(0);
  }
  pd = infiniband.pd->pd;
}

// 1 means no valid buffer read, 0 means got enough buffer
// else return < 0 means error
int Infiniband::recv_msg(int sd, IBSYNMsg& im)
{
  assert(sd >= 0);
  ssize_t r;
  char msg[UDP_MSG_LEN];
  char gid[33];
  r = ::read(sd, &msg, sizeof(msg));
  if (r == -1) {
    LOG(INFO) << __func__ << " recv got error " << errno;
    return -1;
  } else if ((size_t)r != sizeof(msg)) { // valid message length
    LOG(INFO) << __func__ << " recv got bad length (" << r << ").";
    return 1;
  } else { // valid message
    sscanf(msg, "%x:%x:%x:%x:%s", &(im.lid), &(im.qpn), &(im.psn), &(im.peer_qpn),gid);
    wire_gid_to_gid(gid, &(im.gid));
    LOG_IF(INFO, cct->loglevel < 20) << __func__ << " recevd: " << im.lid << ", " << im.qpn << ", " << im.psn << ", " << im.peer_qpn << ", " << gid;
    return 0;
  }
}

int Infiniband::send_msg(int sd, IBSYNMsg& im)
{
  assert(sd >= 0);
  int retry = 0;
  ssize_t r;

  char msg[UDP_MSG_LEN];
  char gid[33];
retry:
  gid_to_wire_gid(&(im.gid), gid);
  sprintf(msg, "%04x:%08x:%08x:%08x:%s", im.lid, im.qpn, im.psn, im.peer_qpn, gid);
  LOG(INFO) << __func__ << " sending: " << im.lid << ", " << im.qpn << ", " << im.psn << ", " << im.peer_qpn << ", "  << gid;
  r = ::write(sd, msg, sizeof(msg));
  LOG(INFO) << __func__ << " r: " << r;
  if ((size_t)r != sizeof(msg)) {
    if (r < 0 && (errno == EINTR || errno == EAGAIN) && retry < 3) {
      retry++;
      goto retry;
    }
    if (r < 0)
      LOG(INFO) << __func__ << " send returned error " << errno;
    else
      LOG(INFO) << __func__ << " send got bad length (" << r << ") ";
    return -1;
  }
  return 0;
}

void Infiniband::wire_gid_to_gid(const char *wgid, union ibv_gid *gid)
{
  char tmp[9];
  uint32_t v32;
  int i;

  for (tmp[8] = 0, i = 0; i < 4; ++i) {
    memcpy(tmp, wgid + i * 8, 8);
    sscanf(tmp, "%x", &v32);
    *(uint32_t *)(&gid->raw[i * 4]) = ntohl(v32);
  }
}

void Infiniband::gid_to_wire_gid(const union ibv_gid *gid, char wgid[])
{
  for (int i = 0; i < 4; ++i)
    sprintf(&wgid[i * 8], "%08x", htonl(*(uint32_t *)(gid->raw + i * 4)));
}

Infiniband::QueuePair::~QueuePair()
{
  if (qp)
    assert(!ibv_destroy_qp(qp));
  LOG_IF(INFO, infiniband.cct->loglevel < 20) << __func__ << " successfully destroyed QueuePair:" << this;
}

Infiniband::CompletionChannel::~CompletionChannel()
{
  if (channel) {
    int r = ibv_destroy_comp_channel(channel);
    LOG_IF(INFO, infiniband.cct->loglevel < 20) << __func__ << " r: " << r;
    assert(r == 0);
  }
  LOG_IF(INFO, infiniband.cct->loglevel < 20) << __func__ << " successfully destroyed CompletionChannel.";
}

Infiniband::CompletionQueue::~CompletionQueue()
{
  if (cq) {
    int r = ibv_destroy_cq(cq);
    LOG_IF(INFO, infiniband.cct->loglevel < 20) << __func__ << " destroyed CompletionQueue " << this << ", r: " << r;
    assert(r == 0);
  }
  LOG_IF(INFO, infiniband.cct->loglevel < 20) << __func__ << " successfully destroyed CompletionQueue.";
}

int Infiniband::CompletionQueue::rearm_notify(bool solicite_only)
{
  LOG_IF(INFO, infiniband.cct->loglevel < 20) << __func__ << " started.";
  int r = ibv_req_notify_cq(cq, 0);
  if (r) {
    LOG(INFO) << __func__ << " failed to notify cq";
  }
  return r;
}

int Infiniband::CompletionQueue::poll_cq(int num_entries, ibv_wc *ret_wc_array) {
  int r = ibv_poll_cq(cq, num_entries, ret_wc_array);
  if (r < 0) {
    LOG(INFO) << __func__ << " poll_completion_queue occur met error";
    return -1;
  }
  return r;
}

bool Infiniband::CompletionChannel::get_cq_event()
{
  //  ldout(infiniband.cct, 21) << __func__ << " started." << dendl;
  ibv_cq *cq = NULL;
  void *ev_ctx;
  if (ibv_get_cq_event(channel, &cq, &ev_ctx)) {
    if (errno != EAGAIN && errno != EINTR)
      LOG(INFO) << __func__ << "failed to retrieve CQ event";
    return false;
  }

  /* accumulate number of cq events that need to
   *    * be acked, and periodically ack them
   *       */
  if (++cq_events_that_need_ack == MAX_ACK_EVENT) {
    LOG_IF(INFO, infiniband.cct->loglevel < 20) << __func__ << " ack aq events.";
    ibv_ack_cq_events(cq, MAX_ACK_EVENT);
    cq_events_that_need_ack = 0;
  }

  return true;
}

int Infiniband::CompletionQueue::init()
{
  cq = ibv_create_cq(infiniband.device->ctxt, queue_depth, this, channel->get_channel(), 0);
  if (!cq) {
    LOG(INFO) << __func__ << " failed to create receive completion queue";
    return -1;
  }

  if (ibv_req_notify_cq(cq, 0)) {
    LOG(INFO) << __func__ << " ibv_req_notify_cq failed";
    ibv_destroy_cq(cq);
    return -1;
  }

  channel->bind_cq(cq);
  LOG_IF(INFO, infiniband.cct->loglevel < 20) << __func__ << " successfully create cq=" << cq;
  return 0;
}

int Infiniband::CompletionChannel::init()
{
  LOG_IF(INFO, infiniband.cct->loglevel < 20) << __func__ << " started.";
  channel = ibv_create_comp_channel(infiniband.device->ctxt);
  if (!channel) {
    LOG(INFO) << __func__ << " failed to create receive completion channel";
    return -1;
  }
  int rc = infiniband.net.set_nonblock(channel->fd);
  if (rc < 0) {
    ibv_destroy_comp_channel(channel);
    return -1;
  }
  return 0;
}

/**
 * Given a string representation of the `status' field from Verbs
 * struct `ibv_wc'.
 *
 * \param[in] status
 *      The integer status obtained in ibv_wc.status.
 * \return
 *      A string corresponding to the given status.
 */
const char* Infiniband::wc_status_to_string(int status)
{
  static const char *lookup[] = {
      "SUCCESS",
      "LOC_LEN_ERR",
      "LOC_QP_OP_ERR",
      "LOC_EEC_OP_ERR",
      "LOC_PROT_ERR",
      "WR_FLUSH_ERR",
      "MW_BIND_ERR",
      "BAD_RESP_ERR",
      "LOC_ACCESS_ERR",
      "REM_INV_REQ_ERR",
      "REM_ACCESS_ERR",
      "REM_OP_ERR",
      "RETRY_EXC_ERR",
      "RNR_RETRY_EXC_ERR",
      "LOC_RDD_VIOL_ERR",
      "REM_INV_RD_REQ_ERR",
      "REM_ABORT_ERR",
      "INV_EECN_ERR",
      "INV_EEC_STATE_ERR",
      "FATAL_ERR",
      "RESP_TIMEOUT_ERR",
      "GENERAL_ERR"
  };

  if (status < IBV_WC_SUCCESS || status > IBV_WC_GENERAL_ERR)
      return "<status out of range!>";
  return lookup[status];
}

const char* Infiniband::qp_state_string(int status) {
  switch(status) {
    case IBV_QPS_RESET : return "IBV_QPS_RESET";
    case IBV_QPS_INIT  : return "IBV_QPS_INIT";
    case IBV_QPS_RTR   : return "IBV_QPS_RTR";
    case IBV_QPS_RTS   : return "IBV_QPS_RTS";
    case IBV_QPS_SQD   : return "IBV_QPS_SQD";
    case IBV_QPS_SQE   : return "IBV_QPS_SQE";
    case IBV_QPS_ERR   : return "IBV_QPS_ERR";
    default: return " out of range.";
  }
}
