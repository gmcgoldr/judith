#include <iostream>
#include <stdexcept>
#include <cassert>

#include "storage/event.h"
#include "storage/storageo.h"
#include "processors/clustering.h"
#include "processors/aligning.h"
#include "analyzers/synchronization.h"
#include "loopers/loopsynchronize.h"

namespace Loopers {

LoopSynchronize::LoopSynchronize(
    const std::vector<Storage::StorageI*>& inputs,
    const std::vector<Storage::StorageO*>& outputs) :
    Looper(inputs),
    m_outputs(outputs),
    m_storing(false),
    m_synchronization(0) {}

LoopSynchronize::~LoopSynchronize() {
  if (m_synchronization) delete m_synchronization;
}

void LoopSynchronize::preLoop() {
  if (!m_synchronization) {
    m_synchronization = new Analyzers::Synchronization(m_nprocess/m_nstep);
    m_synchronization->m_minStats = 10;
    m_synchronization->m_threshold = 5;
    m_synchronization->m_nconsecutive = 3;
  }
}

void LoopSynchronize::execute() {
  Looper::execute();  // run the processors

  assert(m_synchronization && "No synchronization object constructed");

  // If in storing mode, write the synchronized events back
  if (m_storing) {
    if (m_synchronization->writeStatus1(m_ievent))
      m_outputs[0]->writeEvent(*m_events[0]);
    if (m_synchronization->writeStatus2(m_ievent))
      m_outputs[1]->writeEvent(*m_events[1]);
  }

  // Otherwise, just store time stamps from which synchronization is computed
  else
    m_synchronization->execute(m_events);
}

void LoopSynchronize::finalize() {
  assert(m_synchronization && "No synchronization object constructed");

  // Compute the synchronization
  m_synchronization->finalize();

  // Write back the events
  m_storing = true;
  loop();
}

}

