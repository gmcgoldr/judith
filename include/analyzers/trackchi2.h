#ifndef ANA_TRACKCHI2_H
#define ANA_TRACKCHI2_H

#include <string>
#include <vector>
#include <list>

#include "analyzers/analyzer.h"

namespace Mechanics { class Device; }
namespace Mechanics { class Sensor; }

namespace Analyzers {

/**
  * Store all clusters and group them into tracks for a given looper run.
  * The clusters and tracks are light-weight objects containing the minimum
  * information needed to compute the track chi^2 under new alignment.
  *
  * Provides a list of clusters and tracks, intended to be used in a minmizer
  * routine for alignment.
  *
  * @author Garrin McGoldrick (garrin.mcgoldrick@cern.ch)
  */
class TrackChi2 : public Analyzer {
public:
  /** Cluster information required for chi^2 alignment */
  struct Cluster {
    /** Pointer to the sensor which aligns this cluster */
    const Mechanics::Sensor* sensor;
    /** Cluster location in local pixel coordinates */
    const double pixX;
    const double pixY;
    const double pixErrX;
    const double pixErrY;
    /** Initialization of members on construction */
    Cluster(
        const Mechanics::Sensor& sensor, 
        double pixX,
        double pixY,
        double pixErrX,
        double pixErrY) :
        sensor(&sensor),
        pixX(pixX),
        pixY(pixY),
        pixErrX(pixErrX),
        pixErrY(pixErrY) {}
  };

  /** Track information, given as a range of indices of clusters used by this
    * track, from the list of all clusters */
  struct Track {
    /** Position in list of clusters where this track starts */
    size_t m_istart;
    /** Number of clusters to read for the track clusters */
    size_t m_nclusters;
    /** Number of matched clusters following the constituents */
    size_t m_nmatches;
    Track(size_t istart) : m_istart(istart), m_nclusters(0), m_nmatches(0) {}
  };

private:
  TrackChi2(const TrackChi2&);
  TrackChi2& operator=(const TrackChi2&);

  /** List of all tracks built */
  std::list<Track> m_tracks;
  /** List of all clusters in the tracks built */
  std::list<Cluster> m_clusters;
  /** Keep track of cluster index for last added */
  size_t m_icluster;

  /** Number of planes in the reference device */
  size_t m_numRefPlanes;
  /** Number of planes in the DUT device */
  size_t m_numDUTPlanes;

  /** Constructor calls this to initialize memory */
  void initialize();

  /** Base virtual method defined, gives code to run at each loop */
  void process();

public:
  /** Automatically calls the correct base constuctor */
  template <class T>
  TrackChi2(const T& t) :
      Analyzer(t),
      m_icluster(0),
      m_numRefPlanes(0),
      m_numDUTPlanes(0) {
    initialize();
  }
  ~TrackChi2() {}

  void setOutput(TDirectory* dir, const std::string& name="TrackChi2") {
    // Just adds the default name
    Analyzer::setOutput(dir, name);
  }

  /** Get the list of all procssed tracks */
  const std::list<Track>& getTracks() const { return m_tracks; }
  /** Get the list of all processed clusters */
  const std::list<Cluster>& getClusters() const { return m_clusters; }
};

}

#endif  // ANA_TRACKCHI2_H

