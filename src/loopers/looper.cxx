#include <iostream>
#include <stdexcept>
#include <algorithm>

#include <TStopwatch.h>

#include "storage/storagei.h"
#include "storage/event.h"
#include "mechanics/device.h"
#include "processors/processor.h"
#include "analyzers/analyzer.h"
#include "loopers/looper.h"

namespace Loopers {

Looper::Looper(const std::vector<Storage::StorageI*>& inputs) :
    m_inputs(inputs),
    m_events(m_inputs.size()),  // reserve event vector size
    m_devices(),
    m_maxEvents(0),
    m_minEvents(-1),  // largest unsigned integer
    m_finalized(false),
    m_ievent(0),
    m_lastTime(0),
    m_start(0),
    m_nprocess(-1),  // causes iteration over entire range
    m_nstep(1),
    m_printInterval(1E4),
    m_draw(false) {
  // Keep track of the smallest and largest event indices at end of inputs
  for (size_t i = 0; i < m_inputs.size(); i++) {
    const Storage::StorageI& input = *m_inputs[i];
    m_minEvents = std::min(m_minEvents, (ULong64_t)input.getNumEvents());
    m_maxEvents = std::max(m_maxEvents, (ULong64_t)input.getNumEvents());
  }
}

Looper::Looper(
    const std::vector<Storage::StorageI*>& inputs,
    const std::vector<Mechanics::Device*>& devices) :
    m_inputs(inputs),
    m_events(m_inputs.size()),  // reserve event vector size
    m_devices(devices),
    m_maxEvents(0),
    m_minEvents(-1),  // largest unsigned integer
    m_finalized(false),
    m_ievent(0),
    m_start(0),
    m_nprocess(-1),  // causes iteration over entire range
    m_nstep(1),
    m_printInterval(1E4),
    m_draw(false) {
  // Keep track of the smallest and largest event indices at end of inputs
  for (size_t i = 0; i < m_inputs.size(); i++) {
    const Storage::StorageI& input = *m_inputs[i];
    m_minEvents = std::min(m_minEvents, (ULong64_t)input.getNumEvents());
    m_maxEvents = std::max(m_maxEvents, (ULong64_t)input.getNumEvents());
  }
  // Need at least one device
  if (m_devices.empty())
    throw std::runtime_error("Looper::Looper: no devices");
  // Check that there is 1 input per device
  if (m_devices.size() != m_inputs.size())
    throw std::runtime_error("Looper::Looper: device/inputs mismatch");
  // Check that the device sensors match the input planes
  for (size_t i = 0; i < m_devices.size(); i++)
    if (m_devices[i]->getNumSensors() != m_inputs[i]->getNumPlanes())
      throw std::runtime_error("Looper::Looper: device/inputs planes mismatch");
}

Looper::Looper(Storage::StorageI& input) :
    // Single input vector, filled with input address
    m_inputs(1, &input),
    m_events(m_inputs.size()),
    m_maxEvents(0),
    m_minEvents(-1),  // largest unsigned integer
    m_finalized(false),
    m_ievent(0),
    m_start(0),
    m_nprocess(-1),  // causes iteration over entire range
    m_nstep(1),
    m_printInterval(1E4),
    m_draw(false) {
  m_minEvents = (ULong64_t)input.getNumEvents();
  m_maxEvents = (ULong64_t)input.getNumEvents();
}

Looper::Looper(Storage::StorageI& input, Mechanics::Device& device) :
    // Single input vector, filled with input address
    m_inputs(1, &input),
    m_events(m_inputs.size()),
    // Single vector again
    m_devices(1, &device),
    m_maxEvents(0),
    m_minEvents(-1),  // largest unsigned integer
    m_finalized(false),
    m_ievent(0),
    m_start(0),
    m_nprocess(-1),  // causes iteration over entire range
    m_nstep(1),
    m_printInterval(1E4),
    m_draw(false) {
  m_minEvents = (ULong64_t)input.getNumEvents();
  m_maxEvents = (ULong64_t)input.getNumEvents();
  if (m_devices[0]->getNumSensors() != m_inputs[0]->getNumPlanes())
    throw std::runtime_error("Looper::Looper: device/inputs planes mismatch");
}

void Looper::printProgress() {
  const ULong64_t nelapsed = m_ievent - m_start;
  const double telapsed = m_timer.RealTime();
  m_timer.Continue();
  const double tinst = telapsed - m_lastTime;
  m_lastTime = telapsed;

  const double bandwidth = (tinst)*1E6 / (double)m_printInterval;
  const double progress = nelapsed / (double)m_nprocess;
  
  std::printf("\r[");  // \r overwrite the last line
  const size_t size = 50;  // number of = symbols in bar
  for (size_t i = 0; i < size; i++)
    // print = for the positions below the current progress
    std::printf((progress*50>i) ? "=" : " ");
  std::printf("] ");
  // Print also the % done, and the bandwidth
  std::printf("%3d%%, %5.1f us  ", (int)(progress*100), bandwidth);
  // Show on the output (don't buffer)
  std::cout << std::flush;
}

void Looper::loop() {
  // If no number of events is requested, default to all
  if (m_nprocess == (ULong64_t)(-1))
    m_nprocess = m_minEvents - m_start;

  // Check that the given event range works
  if (m_start >= m_minEvents)
    throw std::runtime_error("Looper::loop: start event out of range");
  if (m_start+m_nprocess > m_minEvents)
    throw std::runtime_error("Looper::loop: nprocess exceeds range");
  if (m_nstep < 1)
    throw std::runtime_error("Looper::loop: step size can't be smaller than 1");

  // Don't want to split event range computation from looping, since the public
  // start, process... variables could change. Instead, if derived class needs
  // these computed and verified values, then it can overwrite preLoop.
  preLoop();
  
  for (m_ievent = m_start; m_ievent < m_start+m_nprocess; m_ievent += m_nstep) {
    // If a print interval is given, and this event is on it, print progress
    if (m_printInterval && ((m_ievent-m_start) % m_printInterval == 0))
      printProgress();
    bool invalid = false;
    // Read this event from each input file
    for (size_t i = 0; i < m_inputs.size(); i++) {
      m_events[i] = &m_inputs[i]->readEvent(m_ievent);
      // Don't try to do anything if any device has an invalid event (the event
      // might have bad data)
      invalid |= m_events[i]->getInvalid();
      if (invalid) break;
    }
    if (invalid) continue;
    // Execute this looper's event code
    execute();
  }
  // Print the 100% progress and finish that line
  printProgress();
  std::cout << std::endl;
}

void Looper::execute() {
  // Execute the processors first since they can modify the event
  for (std::vector<Processors::Processor*>::iterator it = m_processors.begin();
      it != m_processors.end(); ++it)
    (*it)->execute(m_events);
  // Then build up analysis from the event data
  for (std::vector<Analyzers::Analyzer*>::iterator it = m_analyzers.begin();
      it != m_analyzers.end(); ++it)
    (*it)->execute(m_events);
}

void Looper::finalize() {
  if (m_finalized)
    throw std::runtime_error("Looper::finalize: looper already finalized");
  m_finalized = true;

  // Do post-processing as needed
  for (std::vector<Analyzers::Analyzer*>::iterator it = m_analyzers.begin();
      it != m_analyzers.end(); ++it)
    (*it)->finalize();
}

void Looper::addProcessor(Processors::Processor& processor) {
  m_processors.push_back(&processor);
}

void Looper::addAnalyzer(Analyzers::Analyzer& analyzer) {
  m_analyzers.push_back(&analyzer);
}

}

