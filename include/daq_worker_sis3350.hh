#ifndef SLAC_DAQ_INCLUDE_DAQ_WORKER_SIS3350_HH_
#define SLAC_DAQ_INCLUDE_DAQ_WORKER_SIS3350_HH_

//--- std includes ----------------------------------------------------------//
#include <ctime>
#include <iostream>
using std::cout;
using std::cerr;
using std::endl;

//--- other includes --------------------------------------------------------//
#include "vme/sis3100_vme_calls.h"

//--- project includes ------------------------------------------------------//
#include "daq_worker_vme.hh"
#include "daq_structs.hh"

// This class pulls data from a sis_3350 device.
namespace daq {

class DaqWorkerSis3350 : public DaqWorkerVme<sis_3350> {

public:
  
  // ctor
  DaqWorkerSis3350(string name, string conf);
  
  void LoadConfig();
  void WorkLoop();
  sis_3350 PopEvent();
  
private:
  
  bool EventAvailable();
  void GetEvent(sis_3350 &bundle);

};

} // ::daq

#endif
