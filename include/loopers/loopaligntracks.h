#ifndef LOOPALIGNTRACKS_H
#define LOOPALIGNTRACKS_H

#include <vector>
#include <list>

#include <Math/IFunction.h>

#include "processors/tracking.h"
#include "analyzers/trackchi2.h"
#include "loopers/looper.h"

namespace Storage { class StorageI; }
namespace Mechanics { class Device; }

namespace Loopers {

/**
  * Align device planes based on track residuals.
  *
  * @author Garrin McGoldrick (garrin.mcgoldrick@cern.ch)
  */
class LoopAlignTracks : public Looper {
public:
  /** Linear regression parameters */
  struct TrackPars {
    double p0x;
    double p1x;
    double p0y;
    double p1y;
    double chi2;
    TrackPars() : p0x(0), p1x(0), p0y(0), p1y(0), chi2(0) {}
  };

  /** Deep within the belly of the beast, lies an interface to for a
    * multidimensional function with a method to compute gradients */
  class Chi2Minimizer : public ROOT::Math::IBaseFunctionMultiDim {
  public:
    enum Flags {
      REFERENCE = 1<<1,
      INPLANE = 1<<2,
    };

  private:
    // Device being aligned
    Mechanics::Device* m_device;
    // Flags determine what it is minimizing
    const int m_flags;
    // List of track parameters, pre-computed if not aligning a reference
    // device (in that case the alignment doesn't change the tracks, so no
    // point in re-computing at each minimization step)
    std::vector<TrackPars> m_trackPars;
    // List of clusters
    std::vector<Analyzers::TrackChi2::Cluster> m_clusters;
    // List of tracklets (sets of cluster in the same track)
    std::vector<Analyzers::TrackChi2::Track> m_tracks;
    // Pre-compute the dimensionality of the minimization
    unsigned m_ndim;

    // Compute the parameters for the set of cluster of the given track,
    // considering device alignment
    void computeTrackPars(
        const Analyzers::TrackChi2::Track& track,
        LoopAlignTracks::TrackPars& pars) const;

    double DoEval(const double* x) const;

  public:
    const ROOT::Math::Minimizer* m_minimizer;

    Chi2Minimizer(
        Mechanics::Device& device,
        int flags,
        const std::list<Analyzers::TrackChi2::Cluster>& clusters,
        const std::list<Analyzers::TrackChi2::Track>& tracklets);

    ROOT::Math::IBaseFunctionMultiDim* Clone() const;
    inline unsigned int NDim() const { return m_ndim; }
  };

private:
  /** Analyzer computes track residuals for each event. */
  Analyzers::TrackChi2 m_trackChi2;

public:
  /** Tracking processor to generate the tracks for alignment */
  Processors::Tracking m_tracking;

  double m_translationScale;
  double m_translationLimit;
  double m_rotationScale;
  double m_rotationLimit;
  bool m_inPlane;
  double m_tolerance;

  LoopAlignTracks(
      const std::vector<Storage::StorageI*>& inputs,
      const std::vector<Mechanics::Device*>& devices);
  LoopAlignTracks(
      Storage::StorageI& input,
      Mechanics::Device& device);
  ~LoopAlignTracks() {}

  /** Override default execute behaviour */
  void execute();

  /** Compute and apply alignment as post-processing step */
  void finalize();
};

}

#endif  // LOOPALIGNTRACKS_H

