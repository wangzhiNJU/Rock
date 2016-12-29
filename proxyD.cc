#include <stddef.h>
#include <sys/socket.h>
#include <sys/un.h>
#include "ipc.h"
#include "proxyD.h"

ProxyD::start()
{
  listener_fd = ipc.unix_socket_listen("./conf/server_socket");
  assert(listener_fd >= 0);
  worker.join();
}

ProxyD::run()
{
  done = false;
  int new_fd;
  char buf[128];
  uint32_t pid, ctlkey, bufkey;
  while (!done)
  {
    new_fd = ipc.unix_socket_accept(listener_fd, nullptr);
    ssize_t n = read(fd, buf, 128);
    assert(n >= 30);
    sscanf(buf, "%010u%010u%010u", &pid, &ctlkey, &bufkey);
    connector.add_client(new_fd, pid);
    AppShmInfo* info = new AppShmInfo(rct, (key_t)ctlkey, (key_t)bufkey, pid);
    apps_mm.add(info);
  }
}

void Connector::accept_request(int fd)
{
  char buf[app_req_size];
  ssize_t r;
  r = read(fd, (void *)buf, app_req_size);
  if (r < 0)
  {
    if (errno == EAGAIN || errno == EINTR)
    {
      loginfo(1) << " no data."; //not enough, but give it up
    }
    else
      loginfo(1) << " read failed"; //this fd go wrong
  }

  if (offset == app_req_size)
    parse_app_req(buf, fd);
  loginfo(1) << " uncomplete msg.";
}

void Connector::process_request(int fd)
{
  char buf[3];
  while (true)
  {
    int n = read(fd, buf, 3);
    if (n == 0)
    {
      loginfo(1) << " peer node closed.";
      return;
    }
    else if (n == -1)
    {
      if (errno == EAGAIN || errno == EINTER)
      {
        loginfo(1) << " not enough data.";
        return;
      }
      assert(0);
    }

    int type = 0;
    sscanf(buf, "%03d", &type);
    if (type == BUILD_QP_REQ || type == BUILD_QP_RSP)
      build_qp(fd, type);
    else if (type == OPT_VQPN_REQ)
      opt_vqpn_req(fd);
    else if (type == OPT_VQPN_RSP)
      opt_vqpn_rsp(fd);
    else
    {
      loginfo(1) << " messing message.";
      assert(0);
    }
  }
}

void Connector::build_qp(int fd, int type)
{
  char buf[SYN_LEN];
  int r = read(fd, buf, SYN_LEN);
  if (r != SYN_LEN)
  {
    loginfo(1) << " read error.";
  }

  IBSynMsg peer;
  ib->get_msg_from_wire(&peer);
  QP *qp = nullptr;
  {
    std::lock_guard<std::mutex> m(mutex);
    qp = bridges[fd];
  }
  assert(qp);
  int r = qp->activate(peer);
  assert(r == 0);
  if (type == BUILD_QP_RSP)
  {
    int n;
    uint32_t *ids;
    Target t = qp->get_target();
    {
      std::lock_guard<std::mutex> m(mutex);
      n = connecting_qps.count(t);
      if (n == 0)
      {
        loginfo(1) << " late message maybe?";
        assert(0);
      }
      auto it = connecting_qps.equal_range(t);
      ids = new int[n * 2];
      for (int i = 0; it.first != it.end(); ++it.first)
      {
        id[i++] = (it.first)->get_pid();
        id[i++] = (it.first)->get_key();
      }
    }
    send_req_vqpns(fd, t, ids, n);
  }
  else
  {
    //send local syn back
    char buf[3 + SYN_LEN];
    sprintf(buf, "%03d", BUILD_QP_RSP);
    ib->get_syn_msg(buf + 3, qp->get_local_syn());
    write(fd, buf, SYN_LEN);
  }
}

void Connector::opt_vqpn_req(int fd)
{
  char buf[30]; //vqpn:10 + pid:10 + key:10
  int nread = ::read(fd, buf, 30);
  assert(nread == 30);
  uint32_t vqpn, pid, key;
  sscanf(buf, "%010u%010u%010u", &vqpn, &pid, &key);
  bool exist = false;
  {
    std::lock_guard<std::mutex> m(mutex);
    exist = (vqpns.find(vqpn) == vqpns.end()) ? false : true;
  }
  if (exist)
    sprintf(buf, "%010u%010u%010u", 0, pid, key);
  write(fd, buf, 30);
}

void Connector::opt_vqpn_rsp(int fd)
{
  char buf[33]; //vqpn:10 + pid:10 + local_key:10
  sprintf(buf, "%03d", CONN_RSP_TO_APP);
  int nread = ::read(fd, buf + 3, 30);
  if (nread != 30)
  {
    loginfo(1) << " something wrong!";
    assert(0);
  }
  uint32_t vqpn = 0, pid, key;
  sscanf(buf, "%010u%010u%010u", &vqpn, &pid, &key);
  Target t;
  if (vqpn == 0)
  {
    
    {
      std::lock_guard<std::mutex> m(mutex);
      for (auto it = connecting_qps.begin(); it != connecting_qps.end(); ++it)
      {
        if (it->get_pid() == pid && it->get_key() == key)
          t = *it;
      }
    }
    uint32_t id[2] = {pid, key};
    send_req_vqpns(fd, t, id, 1);
    return;
  }
  int app_fd = -1;
  {
    std::lock_guard<std::mutex> m(mutex);
    auto iter = local_map.find(pid);
    assert(iter != local_map.end());
    app_fd = iter->second;
  }
  write(app_fd, buf, 33);
  int wr_fd = ipc.recv_fd(app_fd);
  task_tracker.dispatch(t, vqpn, wr_fd);
 // int cqe_fd = ipc.recv_fd(app_fd);
}

void Connector::parse_app_req(char *buf, int fd) // type:1 pid:10 key:10 peer_addr:22
{
  char type;
  uint32_t pid, key;
  char addr[app_req_size];
  memset(addr, '\0', app_req_size);
  sscanf(buf, "%c%010u%010u%s", &type, &pid, &key, addr);

  if (type == 'c') //close connection
    close_connection();
  if (type != 'n') // != new connection
    assert(0);

  Target t(pid, key);
  bool rb = t.parse(addr);
  if (!rb)
  {
    loginfo(1) << " parse addr failed.";
    return;
  }
  try_build_tcp(t);
}

void Connector::try_build_tcp(Target &t)
{
  bool ea = false, eb = false;
  {
    std::lock_guard<std::mutex> lock(mutex);
    ea = (connected_qps.find(t) != connected_qps.end());
    if (!ea)
      eb = (connecting_qps.find(t) != connecting_qps.end());
  }
  if (!ea)
  { //no QP there
    if (eb)
    {
      std::lock_guard<std::mutex> lock(mutex);
      connecting_qps.insert(t);
      loginfo(5) << " new reqs coming when qp half created";
      return;
    }

    QP *qp = new QP(rct, ib->get_device(), ib->get_srq(), ib->get_cq());
    qp->init();
    Target target(t);
    t.set_port(233233);
    int fd = ipc.connect(t);
    assert(fd >= 0);
    this->add_remote(fd, t);
    char buf[3 + SYN_LEN];
    sprintf(buf, "%03d", BUILD_QP_REQ);
    ib->get_syn_msg(buf + 3, qp->get_local_syn());
    write(fd, buf, SYN_LEN);
    return;
  }
  uint32_t id[2] = {t.get_pid(), t.get_key()};
  req_vqpns(fd, t, &id, 1);
}

int Connector::send_req_vqpns(int fd, Target &t, uint32_t *id, int n)
{
  uint32_t buf[n];
  int r = choose_vqpns(t, &buf, n);
  if (r < 0)
    loginfo(1) << +" failed to choose vqpn";

  size_t len = 18 * n + 1;
  char out[len];
  char *s;
  for (s = out, int i = 0; i < n; i += 2)
  {
    sprintf(s, "%03d%010u%010u%010u", OPT_VQPN_REQ, buf[i], id[i], id[i + 1]);
    s += 18;
  } // type:3 + vqpn:10 + port:5

  r = write(fd, out, len - 1);
  assert(r != -1);
  if (r != len)
  {
    loginfo(1) << " sended part message.";
    assert(0);
  }
  return 0;
}

void Acceptor::start()
{
  Target t;
  bool tb = t.parse(rct.listen_host);
  assert(tb);
  int fd = ipc.create_socket(t.get_family());
  assert(fd >= 0);

  done = false;
  int new_fd;
  struct sockaddr addr;
  socklen_t addr_len;
  while (!done)
  {
    new_fd = ::accept(fd, &addr, &addr_len); //
    Target t;
    t.set_sockaddr(addr);
    bool exist = false;
    ;
    {
      std::lock_guard<std::mutex> m(mutex);
      if (connected_qps.find(t) != connected_qps.end() ||
          connecting_qps.find(t) != connecting_qps.end())
        exist = true;
    }
    if (exist)
      close(new_fd); //means that peer already open an fd to here
    else
    {
      connector.add_remote(new_fd, t);
    }
  }
}
