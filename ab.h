#ifndef AB_H
#define AB_H

#include <infiniband/verbs.h>
#include <vector>
#include <iostream>
#include <memory>
#include <assert.h>
#include <algorithm>
#include "RockContext.h"

using std::vector;
using std::string;
using std::unique_ptr;
using std::shared_ptr;

struct IBSynMsg {
  uint16_t lid;
  uint32_t qpn;
  uint32_t local_psn;
  uint32_t dest_psn;
  union ibv_gid gid;  
} __attribute__((packed));

static const uint32_t SYN_LEN = sizeof("0000:00000000:00000000:00000000:00000000000000000000000000000000");

class RockContext;
class Port {
  RockContext* rct;
  uint8_t port_num;
  uint16_t lid;
  union ibv_gid gid;
  ibv_port_attr* port_attr;
  ibv_context* ctxt;
  
 public:
  Port(RockContext*, uint8_t, ibv_context*);
  ~Port() {
    delete port_attr;  
  }
  uint16_t get_lid() { return lid; }
  ibv_gid get_gid() { return gid; }
  void debug() {
    int b = port_num;
    std::cout << " port_num: "  << b << ", state: " << port_attr->state << std::endl;
  }
};

class Device {
  RockContext* rct;
  ibv_device* device;
  ibv_device_attr* device_attr;
  ibv_context* ctxt;
  ibv_pd* pd;
  string name;
  vector<unique_ptr<Port> > ports;
 public:
  Device(RockContext*, ibv_device*);
  ~Device() {
    delete device_attr;
    assert(!ibv_close_device(ctxt));
    assert(!ibv_dealloc_pd(pd));
  }
  string get_name() { return name; }
  ibv_context* get_context() { return ctxt; }
  ibv_pd* get_pd() { return pd; }
  void debug() {
    std::cout << "device name : " << name << std::endl;
    for(auto& p : ports)
      p->debug();
  }
};

class DeviceList {
  RockContext* rct;
  vector<Device*> devices;
  ibv_device** device_list;
  int num;
 public:
  DeviceList(RockContext*);
  ~DeviceList() {
    ibv_free_device_list(device_list);  
  }
  void debug() {
    for(auto& d : devices)
      d->debug();
  }
  Device* get_device(const char*);
};

#endif
