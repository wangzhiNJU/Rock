#include "event.h"

int EventCenter::add_event(int fd, Callback* callback)
{
  if (file_events.find(fd) != file_events.end())
    assert(0);
  {
    std::lock_guard<std::mutex> lock(mutex);
    file_events[fd] = callback;
  }

  struct epoll_event ee;
  ee.events = EPOLLET;
  ee.data.u64 = 0;
  ee.data.fd = fd;

  if (epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &ee) == -1)
  {
    return -1;  
  }
  return 0;
}

int EventCenter::process_events()
{
  int retval, i, tpfd;
  while(!done)
  {
    retval = epoll_wait(epfd, events, nevent, -1);
    std::lock_guard<std::mutex> m(mutex);
    for(i = 0; i < retval; ++i)
    {
      tpfd = events[i].data.fd;
      Callback* f = file_events[tpfd];
      f->callback(tpfd);
    }
  }
}
