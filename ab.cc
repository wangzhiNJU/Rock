#include "ab.h"

Port::Port(RockContext* ir, uint8_t ip, ibv_context* ic) : rct(ir), port_num(ip), ctxt(ic), port_attr(new ibv_port_attr) {
  assert(port_attr);

  int r = ibv_query_port(ctxt, port_num, port_attr);
  assert(r == 0);

  lid = port_attr->lid;
  r = ibv_query_gid(ctxt, port_num, 0, &gid);
  assert(r == 0);
}

Device::Device(RockContext* ir, ibv_device* ii) : rct(ir), device(ii), device_attr(new ibv_device_attr) {
  assert(device_attr);

  name = ibv_get_device_name(device);
  ctxt = ibv_open_device(device);
  assert(ctxt);
  pd = ibv_alloc_pd(ctxt);
  assert(pd);

  int r = ibv_query_device(ctxt, device_attr);
  assert(r == 0);

  for(uint8_t i = 0; i < device_attr->phys_port_cnt; ++i) {
    unique_ptr<Port> tp(new Port(rct, i+1, ctxt));
    ports.push_back(std::move(tp));
  }
}

DeviceList::DeviceList(RockContext* ir) : rct(ir), device_list(ibv_get_device_list(&num)) {
  assert(device_list && num != 0);

  for(int i = 0; i < num; ++i) {
    unique_ptr<Device> td(new Device(ir, device_list[i]));
    devices.push_back(std::move(td));  
  }
}
