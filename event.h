#ifndef EVENT_H
#define EVENT_H
#include <sys/epoll.h>
#include "RockContext.h"

class EventCenter {
  RockContext* rct;
  int epfd;
  struct epoll_event* events;
  int nevent;
  std::map<int, Callback*> file_events;
  std::mutex mutex;
  bool done;
 public:
  EventCenter(RockContext* ir, int in = 1000) : rct(ir), nevent(in), done(false) {
    epfd = epoll_create1(0);
    assert(epfd != -1);

    events = new epoll_event[nevent];
  }
  ~EventCenter() {
    delete[] events;
  }
  int add_event(int fd, Callback*);
  int del_event();
  int process_events();
};

#endif
