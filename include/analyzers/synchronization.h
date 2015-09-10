#ifndef ANA_SYNCHRONIZATION_H
#define ANA_SYNCHRONIZATION_H

#include <string>
#include <vector>
#include <list>

#include <Rtypes.h>
#include <TH1D.h>

#include "analyzers/analyzer.h"

namespace Mechanics { class Device; }
namespace Mechanics { class Sensor; }

namespace Analyzers {

/**
  * Build up a list of all time stamps for two inputs. Provides methods to
  * find and correct for discrepancies.
  *
  * @author Garrin McGoldrick (garrin.mcgoldrick@cern.ch)
  */
class Synchronization : public Analyzer {
private:
  const static size_t s_maxStats = 1000;

  /** Time difference between consecutive triggers for device 1 */
  std::vector<ULong64_t> m_times1;
  /** Time difference between consecutive triggers for device 2 */
  std::vector<ULong64_t> m_times2;
  /** Which device 1 events to write out */
  std::vector<bool> m_write1;
  /** Which device 2 events to write out */
  std::vector<bool> m_write2;

  /** Pre-sampled inter-trigger spacing */
  std::vector<double> m_preSpacings;
  /** Pre-sampled inter-device differences */
  std::vector<double> m_preDiffs;

  TH1D* m_histPreSpacings;
  TH1D* m_histPreDiffs;

  bool m_reserved;

  size_t m_nprocessed;
  double m_ratioMean;
  double m_diffVariance;

  size_t m_ndiffs;
  bool m_desynchronized;
  
  Synchronization(const Synchronization&);
  Synchronization& operator=(const Synchronization&);

  /** Base virtual method defined, gives code to run at each loop */
  void process();

public:
  /** Ratio of device 2 time to device 1 time */
  double m_ratio;
  /** RMS in difference between the two device's time steps */
  double m_scale;
  /** Threshold difference above which events are desynchronized, in units
    * of variance. */
  double m_threshold;
  /** Minimum events collected to evaluate the ratio and variance */
  unsigned m_minStats;
  /** Number of consecutive passes needed to consider an event good */
  unsigned m_nconsecutive;

  Synchronization() :
      Analyzer(2),  // 2 device analyzer
      m_histPreSpacings(0),
      m_histPreDiffs(0),
      m_reserved(false),
      m_nprocessed(0),
      m_ratioMean(0),
      m_diffVariance(0),
      m_ndiffs(0),
      m_desynchronized(false),
      m_ratio(0),
      m_scale(0),
      m_threshold(5),
      m_minStats(10),
      m_nconsecutive(5) {}
  ~Synchronization() {}

  /** Reserve the memory to store the time stamps and build up statistics */
  void reserve(size_t nevents);

  void setOutput(TDirectory* dir, const std::string& name="Synchronization") {
    // Just adds the default name
    Analyzer::setOutput(dir, name);
  }

  /** Process the time staps for synchronization status */
  void finalize();

  /** Returns whether or not `ievent` should be written from device 1 */
  bool writeStatus1(size_t ievent) const { return m_write1[ievent]; }
  /** Returns whether or not `ievent` should be written from device 2 */
  bool writeStatus2(size_t ievent) const { return m_write2[ievent]; }
};

}

#endif  // ANA_SYNCHRONIZATION_H

