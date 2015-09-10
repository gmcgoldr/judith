#ifndef LOOPSYNCHRONIZE_H
#define LOOPSYNCHRONIZE_H

#include "analyzers/synchronization.h"
#include "loopers/looper.h"

namespace Storage { class StorageI; }
namespace Storage { class StorageO; }

namespace Loopers {

/**
  * For all events in a pair of inputs, remove those that are present only in
  * one input but not the other. Store the corresponding events in outputs.
  *
  * @author Garrin McGoldrick (garrin.mcgoldrick@cern.ch)
  */
class LoopSynchronize : public Looper {
private:
  /** Outputs in which to re-write the events */
  const std::vector<Storage::StorageO*> m_outputs;
  /** Flag indicating if the current loop is reading time stamps or writing
    * events to the outputs */
  bool m_storing;

  /** Analyzer which performs the event-by-event computations */
  Analyzers::Synchronization m_synchronization;

  void preLoop();

public:
  LoopSynchronize(
      const std::vector<Storage::StorageI*>& inputs,
      const std::vector<Storage::StorageO*>& outputs);
  ~LoopSynchronize() {}

  /** Execute read event time stamsp or write to the output */
  void execute();
  
  /** Synchronize the read time stamps and re-execute loop to store */
  void finalize();

  /** Provide access to synchronization object for configuring */
  Analyzers::Synchronization& getAnalyzer() { return m_synchronization; }
};

}

#endif  // LOOPSYNCHRONIZE_H

