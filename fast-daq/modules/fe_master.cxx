// Master Frontend

//--- std includes ----------------------------------------------------------//
#include <cassert>
#include <ctime>
#include <cstdio>
#include <iostream>
#include <vector>
using std::cout;
using std::endl;
using std::vector;
using std::string;

//--- other includes --------------------------------------------------------//
#include <boost/foreach.hpp>
#include <boost/variant.hpp>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
using namespace boost::property_tree;

#include <zmq.hpp>

//--- project includes -----------------------------------------------------//
#include "daq_worker_list.hh"
#include "daq_writer_online.hh"
#include "daq_writer_root.hh"
#include "event_builder.hh"
#include "daq_structs.hh"

using namespace daq;

int daq::vme::device = -1;

// Anonymous namespace for "global" parameters
namespace {
  
  // simple declarations
  bool is_running = false;
  bool rc = 0;

  // std declarations
  string msg_string;
  string conf_file;
  string tmp_conf_file(tmpnam(nullptr));

  // zmq declarations
  zmq::context_t master_ctx(1);
  zmq::socket_t trigger_sck(master_ctx, ZMQ_SUB);
  zmq::socket_t handshake_sck(master_ctx, ZMQ_REP);
  zmq::message_t message(10);
  zmq::message_t message_2(10);

  // project declarations
  DaqWorkerList daq_workers;
  vector<DaqWriterBase *> daq_writers;
  EventBuilder *event_builder = nullptr;
}

// Function declarations
int LoadConfig(); // just the file
int SetupConfig(); // all the workers and writers
int FreeConfig(); // free all the workers and writers
int StartRun();
int StopRun();
void HandshakeLoop();

// The main loop
int main(int argc, char *argv[])
{
  // If there was a command line argument, grab that file.
  if (argc > 1) {

    conf_file = string(argv[1]);

  } else {

    conf_file = string("config/.default_master.json"); // default

  }

  // Load the configuration
  LoadConfig();
  
  ptree conf;
  read_json(tmp_conf_file, conf);

  // Connect the sockets since they shouldn't ever change.
  trigger_sck.bind(conf.get<string>("trigger_port").c_str());
  trigger_sck.setsockopt(ZMQ_SUBSCRIBE, "", 0);
  handshake_sck.bind(conf.get<string>("handshake_port").c_str());

  // Launch the thread that confirms a running frontend.
  std::thread handshake_thread(HandshakeLoop);

  while (true) {

    // Check for a message.
    rc = trigger_sck.recv(&message);

    if (rc == true) {

      // Process the message.
      std::istringstream ss(static_cast<char *>(message.data()));
      std::getline(ss, msg_string, ':');

      if (msg_string == string("START") && !is_running) {
	
	// Reload the external config.
	LoadConfig();

	// Change the run number.
	string file_name("data/run_");
	std::getline(ss, msg_string, ':');
	file_name.append(msg_string);
	file_name.append(".root");

	// Save the internal config.
	ptree conf;
	read_json(tmp_conf_file, conf);
	conf.put("writers.root.file", file_name);
	write_json(tmp_conf_file, conf);

	// Setup the config and run.
	SetupConfig();
        StartRun();

      } else if (msg_string == string("STOP") && is_running) {

        StopRun();
	FreeConfig();

      }
    }

    usleep(daq::long_sleep);
  }

  return 0;
}

int LoadConfig() 
{
  // Load up the configuration file to refresh the internal one.
  cout << "Opening config file: " << conf_file << endl;
  ptree conf;
  read_json(conf_file, conf);
  write_json(tmp_conf_file, conf);

  return 0;
}


int SetupConfig()
{
  // Load the internal config file.
  ptree conf;
  read_json(tmp_conf_file, conf);

  // Get the fake data writers (for testing).
  BOOST_FOREACH(const ptree::value_type &v, conf.get_child("devices.fake")) {
    
    string name(v.first);
    string dev_conf_file(v.second.data());

    daq_workers.PushBack(new DaqWorkerFake(name, dev_conf_file));
  } 

  // Set up the sis3350 devices.
  BOOST_FOREACH(const ptree::value_type &v, 
		            conf.get_child("devices.sis_3350")) {

    string name(v.first);
    string dev_conf_file(v.second.data());

    daq_workers.PushBack(new DaqWorkerSis3350(name, dev_conf_file));
  }  

  // Set up the sis3302 devices.
  BOOST_FOREACH(const ptree::value_type &v, 
                conf.get_child("devices.sis_3302")) {

    string name(v.first);
    string dev_conf_file(v.second.data());

    daq_workers.PushBack(new DaqWorkerSis3302(name, dev_conf_file));
  }

  // Set up the caen1785 devices.
  BOOST_FOREACH(const ptree::value_type &v, 
                conf.get_child("devices.caen_1785")) {

    string name(v.first);
    string dev_conf_file(v.second.data());

    daq_workers.PushBack(new DaqWorkerCaen1785(name, dev_conf_file));
  }

  // Set up the caen6742 devices.
  BOOST_FOREACH(const ptree::value_type &v, 
                conf.get_child("devices.caen_6742")) {

    string name(v.first);
    string dev_conf_file(v.second.data());

    daq_workers.PushBack(new DaqWorkerCaen6742(name, dev_conf_file));
  }

  // Set up the writers.
  BOOST_FOREACH(const ptree::value_type &v,
                conf.get_child("writers")) {
 
    if (string(v.first) == string("root") && v.second.get<bool>("in_use")) {
   
      daq_writers.push_back(new DaqWriterRoot(tmp_conf_file));
   
    } else if (string(v.first) == string("online") 
	       && v.second.get<bool>("in_use")) {
   
      daq_writers.push_back(new DaqWriterOnline(tmp_conf_file));
   
    } else if (string(v.first) == string("midas")
	       && v.second.get<bool>("in_use")) {
      
      //      daq_writers.push_back(new DaqWriterMidas(tmp_conf_file));
    }
  }

  // Set up the event builder.
  event_builder = new EventBuilder(daq_workers, daq_writers, tmp_conf_file);

  return 0;
}

int FreeConfig() 
{
  // Delete the allocated workers.
  daq_workers.FreeList();

  // Delete the allocated writers.
  for (auto &writer : daq_writers) {
    delete writer;
  }
  daq_writers.resize(0);

  delete event_builder;

  return 0;
}

// Flush the buffers and start data taking.
int StartRun() {
  cout << "Starting run." << endl;
  is_running = true;

  // Start the event builder
  event_builder->StartBuilder();

  // Start the writers
  for (auto it = daq_writers.begin(); it != daq_writers.end(); ++it) {
    (*it)->StartWriter();
  }

  // Start the data gatherers
  daq_workers.StartRun();

  return 0;
}

// Write the data file and reset workers.
int StopRun() {
  cout << "Stopping run." << endl;
  is_running = false;

  // Stop the event builder
  event_builder->StopBuilder();

  while (!event_builder->FinishedRun());

  // Stop the writers
  for (auto it = daq_writers.begin(); it != daq_writers.end(); ++it) {
    (*it)->StopWriter();
  }

  // Stop the data gatherers
  daq_workers.StopRun();

  return 0;
}

void HandshakeLoop()
{
  string msg_string;
  bool rc = false;
  
  while (true) {
    rc = handshake_sck.recv(&message_2);
  
    if (rc == true) {

      handshake_sck.send(message_2);

    }
  }
}
