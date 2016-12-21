#include "proxy.h"

int Proxy::start()
{
  app_fd = ipc.unix_socket_connect(act->app_unix_path, act->raas_unix_path);
  assert(app_fd);
  char buf[11];
  sprintf(buf, "%010u", pid);
  write(app_fd, buf, 10);
}

int Proxy::run()
{
  done = false;
  while (!done)
  {
    center.process_events();
  }
}

int Proxy::connect(Target &t)
{
  char buf[app_req_size];
  uint32_t key = get_local_key();
  memset(buf, '\0', app_req_size);
  addr[0] = 'n';
  buf += 1;
  sprintf(buf, "%010u", pid));
  buf += 10;
  sprintf(buf, "%010u", key);
  buf += 10;
  sprintf(buf, "%s", t.get_wire_addr().c_str());
  assert(write(app_fd, buf, app_req_size) == app_req_size);
  check_connected(key);

  int fd = -1;
  {
    std::lock_guard<std::mutex> m(mutex);
    auto iter = poster.find(key);
    assert(iter != poster.end());
    fd = *iter;
    poster.erase(iter);
  }
  return fd;
}

void Proxy::check_connected(uint32_t key)
{
  std::unique_lock<std::mutex> lock(cond_mutex);
  std::condition_variable cv;
  while (!finished(key))
    cv.wait(lock);
}

void Proxy::read_response()
{
  char buf[33];
  int nread = ::read(app_fd, buf, 33);
  assert(nread == 33);
  int type;
  uint32_t tpid, tkey, tvqpn;
  sscanf(buf + 3, "%010u%010u%010u", &tpid, &tkey, &tvqpn);
  assert(tpid == pid);
  Connection *conn = new Connection(tvqpn);
  {
    std::lock_guard<std::mutex> m(mutex);
    assert(poster.find(key) == poster.end());
    poster[key] = conn.get_cqe_fd();
  }
  //exchange 2 shm & 2 fd
  int r = ipc.send_fd(app_fd, conn->get_wr_fd());
  r = ipc.send_fd(app_fd, conn->get_cqe_fd());
}
