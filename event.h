#ifndef EVENT_H
#define EVENT_H
#include <sys/epoll.h>

class EventCenter {
  Context* rct;
  int epfd;
  struct epoll_event* events;
  int nevent;
  std::map<int, std::function<void(int)> > file_events;
  std::mutex mutex;
  bool done;
 public:
  EventCenter(Context* ir, int in = 1000) : rct(ir), nevent(in), done(false) {
    epfd = epoll_create1(0);
    assert(epfd != -1);

    events = new epoll_event[nevent];
  }
  ~EventCenter() {
    delete[] events;
  }
  int add_event(int fd, std::function<void(int)> func);
  int del_event();
  int process_events();
};

#endif
