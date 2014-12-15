#include "daq_worker_drs4.hh"

namespace daq {

DaqWorkerDrs4::DaqWorkerDrs4(string name, string conf) : DaqWorkerBase<drs4>(name, conf)
{
  LoadConfig();

  board_->StartClearCycle();
  board_->FinishClearCycle();
  board_->Reinit();
  board_->StartDomino();
}

void DaqWorkerDrs4::LoadConfig()
{ 
  // Open the configuration file.
  boost::property_tree::ptree conf;
  boost::property_tree::read_json(conf_file_, conf);

  // Load the drs board.
  drs_ = new DRS();
  board_ = drs_->GetBoard(conf.get<int>("board_number", 0));

  // Reset the usb device in case it was shutdown improperly.
  libusb_reset_device(board_->GetUSBInterface()->dev);

  // Initialize the board.
  board_->Init();

  // Start setting config file parameters.
  board_->SetFrequency(conf.get<double>("sampling_rate", 5.12), true);
  board_->SetInputRange(conf.get<double>("voltage_range", 0.0)); //central V

  if (conf.get<bool>("trigger_ext")) {

    // The following sets the trigger to external for DRS4
    board_->EnableTrigger(1, 0); // enable hardware trigger
    board_->SetTriggerSource(1<<4); // set external trigger as source

  } else {

    int ch = conf.get<int>("trigger_chan", 1);
    double thresh = conf.get<double>("trigger_thresh", 0.05);
    positive_trg_ = conf.get<bool>("trigger_pos", false);

    // This sets a hardware trigger on Ch_1 at 50 mV positive edge
    board_->SetTranspMode(1); // Needed for analog trigger
    if (board_->GetBoardType() == 8) { // Evaluaiton Board V4

      board_->EnableTrigger(1, 0); // enable hardware trigger
      board_->SetTriggerSource(ch); // set CH1 as source

    } else { // Evaluation Board V3

      board_->EnableTrigger(0, 1); // lemo off, analog trigger on
      board_->SetTriggerSource(ch); // use CH1 as source

    }

    board_->SetTriggerLevel(thresh);         // -0.05 V
    board_->SetTriggerLevel(!positive_trg_); // neg edge
  }

  int trg_delay = conf.get<int>("trigger_delay");
  board_->SetTriggerDelayNs(trg_delay); // 100 ns = 30 ns before trigger

} // LoadConfig

void DaqWorkerDrs4::WorkLoop()
{
  t0_ = high_resolution_clock::now();

  while (thread_live_) {

    while (go_time_) {

      if (EventAvailable()) {

        static drs4 bundle;
        GetEvent(bundle);

        queue_mutex_.lock();
        data_queue_.push(bundle);
        has_event_ = true;
        queue_mutex_.unlock();

      } else {
	
        std::this_thread::yield();
    	  usleep(daq::short_sleep);

      }
    }

    std::this_thread::yield();
    usleep(daq::long_sleep);
  }
}

drs4 DaqWorkerDrs4::PopEvent()
{
  static drs4 data;

  if (data_queue_.empty()) {
    drs4 str;
    return str;
  }

  queue_mutex_.lock();

  // Copy the data.
  data = data_queue_.front();
  data_queue_.pop();

  // Check if this is that last event.
  if (data_queue_.size() == 0) has_event_ = false;

  queue_mutex_.unlock();
  return data;
}


bool DaqWorkerDrs4::EventAvailable()
{
  return !board_->IsBusy();
}

// Pull the event.
void DaqWorkerDrs4::GetEvent(drs4 &bundle)
{
  int ch, offset, ret = 0;
  bool is_event = true;

  // Get the system time.
  auto t1 = high_resolution_clock::now();
  auto dtn = t1.time_since_epoch() - t0_.time_since_epoch();
  bundle.system_clock = duration_cast<milliseconds>(dtn).count();

  //move waves into buffer
  board_->TransferWaves(0, DRS4_CH*2);
  
  // Now read the raw waveform arrays
  for (int i = 0; i < DRS4_CH; i++){
    board_->DecodeWave(0, i*2, bundle.trace[i]);
  }

  // reinitialize the drs so that we don't read the same data again.
  board_->StartDomino();
}

} // ::daq