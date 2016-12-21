#ifndef IPC_H
#define IPC_H
#include <sys/socket.h>
#include <sys/un.h>
#include <time.h>

class Target;
bool operator<(const Target &a, const Target &b)
{
  return a.less(b);
}

class IPC
{
  RockContext *rct;
  string name;
  int cm_len;
  struct cmsghdr *cmptr; //used to send fd
public:
  IPC(RockContext *ir) : rct(ir), name("@IPC"), cm_len(CMSG_LEN(sizeof(int))),
                         cmptr(malloc(cm_len)) {}
  ~IPC()
  {
    free(cmptr]);
  }
  int unix_socket_listen(const char *);
  int unix_socket_accept(int, uid_t *);
  int connect(Target &t);
  int unix_socket_connect(const char *, const char *);
  int send_fd(int, int;
  int recv_fd(int);
};

class Target
{
  union {
    sockaddr sa;
    sockaddr_in sin;
    sockaddr_in6 sin6;
  } u;
  string wire_addr;
  uint32_t pid; //initiator pid
  uint32_t key; //initiaotr local key in process of pid
public:
  Target(uint32_t ip = 0, uint32_t ik = 0) : pid(ip), key(ik)
  {
    memset(&u, 0, sizeof(u));
  }
  uint32_t get_pid()
  {
    return pid;
  }
  uint32_t get_key()
  {
    return key;
  }
  bool set_sockaddr(const struct sockaddr *sa)
  {
    switch (sa->sa_family)
    {
    case AF_INET:
      memcpy(&u.sin, sa, sizeof(u.sin));
      addr_len = sizeof(u.sin);
      break;
    case AF_INET6:
      memcpy(&u.sin6, sa, sizeof(u.sin6));
      addr_len = sizeof(u.sin6);
      break;
    default:
      return false;
    }
    return true;
  }
  bool less(const Target &b) const
  { //ignore port
    int r = u.sa.sa_family - b.u.sa.sa_family;
    if (r < 0)
      return true;
    else if (r > 0)
      return false;

    switch (u.sa.sa_family)
    {
    case AF_INET:
      return u.sin.sin_addr.s_addr - b.u.sin.sin_addr.s_addr < 0 ? true : false;
    case AF_INET6:
      return strncmp((char *)&u.sin6.sin6_addr.s6_addr, (char *)&b.u.sin6.sin6_addr.s6_addr, 16) < 0 ? true : false;
    default:
      assert(0);
    }
  }

  bool parse(const char *s)
  {
    char buf4[39];
    char *o = buf4;
    const char *p = s;
    while (o < buf4 + sizeof(buf4) &&
           *p && ((*p == '.') ||
                  (*p >= '0' && *p <= '9')))
    {
      *o++ = *p++;
    }
    *o = 0;

    char buf6[64];
    o = buf6;
    p = s;
    while (o < buf6 + sizeof(buf6) &&
           *p && ((*p == ':') ||
                  (*p >= '0' && *p <= '9') ||
                  (*p >= 'a' && *p <= 'f') ||
                  (*p >= 'A' && *p <= 'F')))
    {
      *o++ = *p++;
    }
    *o = 0;

    struct in_addr a4;
    struct in6_addr a6;
    if (inet_pton(AF_INET, buf4, &a4))
    {
      u.sin.sin_addr.s_addr = a4.s_addr;
      cout << __func__ << u.sin.sin_addr.s_addr << endl;
      u.sa.sa_family = AF_INET;
      p = s + strlen(buf4);
    }
    else if (inet_pton(AF_INET6, buf6, &a6))
    {
      u.sa.sa_family = AF_INET6;
      memcpy(&u.sin6.sin6_addr, &a6, sizeof(a6));
      p = s + strlen(buf6);
    }
    else
    {
      return false;
    }

    if (*p == ':')
    {
      p++;
      int port = atoi(p);
      set_port(port);
    }

    wire_addr = s;
    return true;
  }

  void set_port(int port)
  {
    switch (u.sa.sa_family)
    {
    case AF_INET:
      u.sin.sin_port = htons(port);
      break;
    case AF_INET6:
      u.sin6.sin6_port = htons(port);
      break;
    default:
      assert(0);
    }
  }
  int get_port() const
  {
    switch (u.sa.sa_family)
    {
    case AF_INET:
      return ntohs(u.sin.sin_port);
      break;
    case AF_INET6:
      return ntohs(u.sin6.sin6_port);
      break;
    }
    return 0;
  }
  int get_family() const
  {
    return u.sa.sa_family;
  }
  const sockaddr *get_sockaddr() const
  {
    return &u.sa;
  }
  size_t get_sockaddr_len() const
  {
    switch (u.sa.sa_family)
    {
    case AF_INET:
      return sizeof(u.sin);
    case AF_INET6:
      return sizeof(u.sin6);
    }
    return sizeof(u);
  }

  string get_wire_addr()
  {
    if (!wire_addr.empty())
      return wire_addr;

    char buf[25];
    const char *ptr = inet_ntop(AF_INET, &u, sin.sin_addr, 25);
    assert(ptr);
    int len = strlen(buf);
    sprintf(buf + len, ":%d", get_port());
    string tmp(buf);
    wire_addr.swap(tmp);
    return tmp;
  }
};
#endif
