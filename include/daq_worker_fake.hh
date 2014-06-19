#ifndef SLAC_DAQ_INCLUDE_DAQ_WORKER_FAKE_HH_
#define SLAC_DAQ_INCLUDE_DAQ_WORKER_FAKE_HH_

//--- std includes ----------------------------------------------------------//
#include <cmath>
#include <ctime>

//--- other includes --------------------------------------------------------//

//--- project includes ------------------------------------------------------//
#include "daq_worker_base.hh"
#include "daq_structs.hh"


// This class produces fake data to test functionality
namespace daq {

typedef sis_3350 event_struct;

class DaqWorkerFake : public DaqWorkerBase<event_struct> {

  public:

    // ctor
    DaqWorkerFake(string name, string conf);

    void LoadConfig();
    void WorkLoop();
    event_struct PopEvent();

  private:

    // Fake data variables
    int num_ch_;
    int len_tr_;
    std::atomic<bool> has_fake_event_;
    double rate_;
    double jitter_;
    double drop_rate_;
    event_struct event_data_;
    std::thread event_thread_;

    bool EventAvailable() { return has_fake_event_; };
    void GetEvent(event_struct);


    // The function generates fake data.
    void GenerateEvent();
};

} // daq

#endif