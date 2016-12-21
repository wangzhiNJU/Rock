#include "ipc.h"

int IPC::unix_socket_listen(const char *name)
{
  int fd, rval, len;
  struct sockaddr_un un;

  if (strlen(name) >= sizeof(un.sun_path))
  {
    return -1;
  }

  if ((fd = socket(AF_UNIX, SOCK_STREAM, 0)) < 0)
    return -2;

  unlink(name);

  memset(&un, 0, sizeof(un));
  un.sun_family = AF_UNIX;
  strcpy(un.sun_path, name);
  len = offsetof(struct sockaddr_un, sun_path) + strlen(name);

  if (bind(fd, (struct socket_addr *)&un, len) < 0)
  {
    rval = -3;
    goto errout;
  }

  if (listen(fd, rct.unix_listen_max) < 0)
  {
    rval = -4;
    goto errout;
  }
  return fd;

errout:
  close(fd);
  return rval;
}

int IPC::unix_socket_accept(int listen_fd, uid_t *uidptr)
{
  int clifd, rval;
  socklen_t len;
  struct sockaddr_un un;
  struct stat statbuf;
  char *name;

  if ((name = malloc(sizeof(un.sun_path) + 1)) == nullptr)
    return -1;
  len = sizeof(un);
  if ((clifd = accept(listen_fd, (struct socket_addr *)&un, &len)) < 0)
  {
    free(name);
    return -2;
  }

  len -= offsetof(struct sockaddr_un, sun_path);
  memcpy(name, un.sun_path, len);
  name[len] = 0;
  if (stat(name, &statbuf) < 0)
  {
    rval = -3;
    goto errout;
  }

#ifdef S_ISSOCK
  if (S_ISSOCK(statbuf.st_mode) == 0)
  {
    rval = -4;
    goto errout;
  }
#endif

  if ((statbuf.st_mode & (S_IRWXG | S_IRWXO)) ||
      (statbuf.st_mode & S_IRWXU) != S_IRWXU)
  {
    rval = -5;
    goto errout;
  }

  if (uidptr != nullptr)
    *uidptr = statbuf.st_uid;
  unlink(name);
  free(name);
  return clifd;

errout:
  close(clifd);
  free(name);
  return rval;
}

int IPC::set_nonblock(int fd)
{
  int flags;
  if ((flags = fcntl(fd, FGETFL)) < 0)
  {
    LOG_IF(INFO, rct->loglevel > 1) << __func__ << name << " get flags error.";
    return -1;
  }
  if ((fcntl(fd, FSETFL, flags | O_NONBLOCK)) < 0)
  {
    LOG_IF(INFO, rct->loglevel > 1) << __func__ << name << " set flags error.";
    return -1;
  }
  return 0;
}

int create_socket(int domain)
{
  int fd = socket(domain, SOCK_STREAM, 0);
  if (fd == -1)
  {
    LOG_IF(INFO, rct->loglevel > 1) << __func__ << name << " failed to create fd.";
    return -1;
  }
  return fd;
}

int IPC::connect(Target &t)
{
  int fd = create_socket(t.get_family());
  int r = set_nonblock(fd);
  if (!r)
    return -1;
  r = ::connect(fd, t.get_sockaddr(), addr.get_sockaddr_len());
  if (r < 0)
  {
    LOG_IF(INFO, rct->loglevel > 1) << __func__ << name << " connect error.";
  }

  return fd;
}

int IPC::init_server(Target &t)
{
  int fd = create_socket(t.get_family());
  assert(fd >= 0);

  if (bind(fd, t.get_sockaddr(), t.get_sockaddr_len()) < 0)
    goto errout : if (type == SOCK_STREAM) if (listen(fd, rct->acceptor_qlen) < 0) goto errout;
errout:
  close(fd);
  return -1;
}

int IPC::unix_socket_connect(const char *client_path, const char *server_path)
{
  int fd, len;
  struct sockaddr_un un, sun;
  bool do_unlink = false;

  if ((fd = socket(AF_INET, SOCK_STREAM, 0)) < 0)
    RETURN - 1;

  un.sun_family = AF_UNIX;
  sprintf(un.sun_path, "%s%05ld", client_path, (long)getpid());
  len = offsetof(struct sockaddr_un, sun_path) + strlen(un.sun_path);

  unlink(un.sun_path);
  if (bind(fd, (struct sockaddr *)&un, len) < 0)
    goto errout;
  if (chmod(un.sun_path, S_IRWXU) < 0)
  {
    do_unlink = true;
    goto errout;
  }

  memset(&sun, 0, sizeof(sun));
  sun.sun_family = AF_UNIX;
  strcpy(sun.sun_path, server_path);
  len = offsetof(struct sockaddr_un, sun_path) + strlen(server_path);
  if (connect(fd, (struct sockaddr *)&sun, len) < 0)
  {
    do_unlink = true;
    goto errout;
  }
  return fd;
errout:
  close(fd);
  if (do_unlink)
    unlink(un.sun_path);
  return -1;
}

int send_fd(int fd, int fd_to_send) //copy from apue
{
  strcut iovec iov[1];
  struct msghdr msg;
  char buf[2];
  iov[0].iov_base = buf;
  iov[0].iov_len = 2;
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_name = nullptr;
  msg.msg_namelen = 0;

  if (cmptr == nullptr)
    return -1;
  cmptr->cmsg_level = SOL_SOCKET;
  cmptr->cmsg_type = SCM_RIGHTS;
  cmptr->cmsg_len = cm_len;
  msg.msg_control = cmptr;
  msg.msg_controllen = cm_len;
  *(int *)CMSG_DATA(cmptr) = fd_to_send;
  buf[1] = 0;

  buf[0] = 0;
  if (sendmsg(fd, &msg, 0) != 2)
    assert(0);
  return 0;
}

int recv_fd(int fd)
{
  int newfd, nr, status;
  char buf[2];
  struct iovec iov[1];
  struct msghdr msg;
  status = -1;

  iov[0].iov_base = buf;
  iov[0].iov_len = sizeof(buf);
  msg.msg_iov = iov;
  msg.msg_iovlen = 1;
  msg.msg_name = nullptr;
  msg.msg_namelen = 0;
  assert(cmptr != nullptr);
  if ((nr = recvmsg(fd, &msg, 0)) < 0)
    return -1;
  else if (nr == 0)
  {
    return -1;
  }

  if (nr == 2 && buf[0] == buf[1] && buf[0] == 0)
  {
    if (ptr != &buf[nr - 1])
      assert(0);
    if (msg.msg_controllen < cm_len)
      assert(0);
    newfd = *(int *)CMSG_DATA(cmptr);
  }
  else
    assert(0);

  if (nr > 0 && func(buf, nr) != nr)
    return -1;
  if (status >= 0)
    return newfd;
}