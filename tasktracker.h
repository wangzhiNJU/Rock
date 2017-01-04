#ifndef TASK_TRACKER
#define TASK_TRACKER

#include "RockContext.h"
#include "Memory.h"
#include "event.h"
#include "ipc.h"

class ClientTT
{ // clients of tasktracker
  uint32_t vqpn;
  int wr_fd;
  char* ptr;
public:
  ClientTT(uint32_t iv, int iw, char* ip) : vqpn(iv), wr_fd(iw), ptr(ip) {}
  int get_wr_fd() const {
    return wr_fd;  
  }
  uint32_t get_vqpn() const {
    return vqpn;  
  }
};

bool operator<(const ClientTT& t1, const ClientTT& t2) {
  return t1.get_vqpn() < t2.get_vqpn();    
}

class Worker{
  RockContext* rct;
  int id;
  std::thread t;
  std::set<ClientTT> clients;
  std::string name;
  std::mutex mutex;
  EventCenter center;
  bool done;
  Callback* pwr_callback;

  class ProcessWRCallback : public Callback {
    Worker* worker;
   public:
    ProcessWRCallback(Worker* w) : worker(w) {}
    void callback(int fd) {
      worker->process_work_requests(fd);  
    }
  };
  public:
  Worker(RockContext* ir, int ii) : rct(ir), id(ii),name("@Worker"), center(ir), done(true),
                            pwr_callback(new ProcessWRCallback(this)) {
    t = std::thread(&Worker::run, this);  
  }
  void run(){
    done = false;
    while(!done) {
      center.process_events();
    }
  }
  void add_client(ClientTT& t) {
    {
    std::lock_guard<std::mutex> m(mutex);
    assert(clients.find(t) == clients.end());
    clients.insert(t);}
    int r =center.add_event(t.get_wr_fd(), pwr_callback);
    assert(r == 0);
  }
  void process_work_requests(int fd);
};

class TaskTracker
{
  RockContext* rct;
  std::map<Target, Worker*> book;
  std::vector<Worker*> workers;
  std::map<uint32_t, AppShmInfo*> appsMemInfo;//pid : shared memory info of applications
  std::mutex mutex;
public:
  TaskTracker(RockContext* ir):rct(ir) {
    for(int i = 0; i < rct->task_tracker_workers_num; ++i) {
      workers.push_back(new Worker(ir, i));
    }
  }
  ~TaskTracker() {
    //TODO
  }
  void dispatch(Target& t, int fd, uint32_t vqpn, char* ptr) {
    ClientTT c(vqpn, fd, ptr);
    workers[0]->add_client(c);
  }
};
#endif
