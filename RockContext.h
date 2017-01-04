#ifndef ROCKCONTEXT_H
#define ROCKCONTEXT_H
#include <mutex>
#include <map> 
#include <functional>
#include <string>
#include <set>
#include <thread>

#include <assert.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <stddef.h>
#include <sys/stat.h>
#include <glog/logging.h>
#include <fcntl.h>
#include <infiniband/verbs.h>
#include <string.h>
#include <sys/shm.h>
#include <errno.h>


using std::string;

#define loginfo(n) LOG_IF(INFO, rct->loglevel > 1) << __func__ << name 

class IB;
class TaskTracker;
class AppsMemoryManager;
class Poller;

class RockContext{
  TaskTracker* tasktracker;
  AppsMemoryManager* apps_mm;
  Poller* poller;
  public:
  int loglevel;
  int acceptor_qlen;
  int app_req_size;
  IB* ib;
  char* device_name;
  uint8_t active_port_num;
  int app_ctl_shm_amount;
  int app_data_shm_amount;
  char* listen_host;
  int task_tracker_workers_num;

  int cq_depth;
  int srq_max_wr;//16000
  int srq_max_sge;//1
  int qp_max_send_wr;//16000
  int qp_max_send_sge;//1
  int qp_max_inline_data;//256
  int qp_dest_rd_atomic;
  int qp_rd_atomic;

  TaskTracker* get_tasktracker();
  AppsMemoryManager* get_AppsMemoryManager();
  IB* get_ib();
  Poller* get_poller();
};

class Callback{
  public:
  virtual void callback(int) = 0;
  virtual ~Callback() {}
};
#endif
