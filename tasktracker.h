#ifndef TASK_TRACKER
#define TASK_TRACKER

class ClientTT
{ // clients of tasktracker
  uint32_t vqpn;
  int wr_fd;

public:
  ClientTT(uint32_t iv, int iw) : vqpn(iv), wr_fd(iw) {}
};

class Worker{
  RockContext* rct;
  std::thread t;
  std::set<ClientTT> clients;
  std::string name;
  std::mutex mutex;
  EventCenter center;
  bool done;
  public:
  Worker(RockContext* ir) : rct(ir), t(run),name("@Worker"), center(ir), done(true) {}
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
    int r =center.add_event(t.get_wr_fd(), process_work_requests);
    assert(r == 0);
  }
  void process_worke_requests(int fd);
};

class TaskTracker
{
  RockContext* rct;
  std::map<Target, Worker*> book;
  std::vector<Worker> workers;
  std::map<uint32_t, AppShmInfo*> ;//pid : shared memory info of applications
  std::mutex mutex;
public:
  TaskTracker(RockContext* ir):rct(ir) {
    for(int i = 0; i < rct.task_tracker_workers_num; ++i) {
      workers.emplace_back(ir);
    }
  }
  ~TaskTracker() {
    //TODO
  }
  void dispatch(Target& t, int fd, uint32_t vqpn) {
    ClientTT c(vqpn, fd);
    workers[0].add_client(c);
  }
};
#endif