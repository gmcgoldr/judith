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
    m_storing(false) {}

void LoopSynchronize::preLoop() {
  // If about to loop over time stamps, reserve memory for their values
  if (!m_storing) m_synchronization.reserve(m_nprocess/m_nstep);
}

void LoopSynchronize::execute() {
  Looper::execute();  // run the processors

  // If in storing mode, write the synchronized events back
  if (m_storing) {
    if (m_synchronization.writeStatus1(m_ievent))
      m_outputs[0]->writeEvent(*m_events[0]);
    if (m_synchronization.writeStatus2(m_ievent))
      m_outputs[1]->writeEvent(*m_events[1]);
  }

  // Otherwise, store time stamps from which synchronization will be computed
  else {
    m_synchronization.execute(m_events);
  }
}

void LoopSynchronize::finalize() {
  // Compute the synchronization (which events two skip in order to maintain
  // synchronization)
  m_synchronization.finalize();

  // Loop, but this time storing events while following the synchronization
  // output
  m_storing = true;
  loop();
}

}

