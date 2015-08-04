#include <iostream>
#include <stdexcept>
#include <cassert>
#include <vector>
#include <list>
#include <memory>
#include <cmath>
#include <cstdio>

#include <Rtypes.h>
#include <Math/Minimizer.h>
#include <Math/Factory.h>

#include "utils.h"
#include "mechanics/alignment.h"
#include "mechanics/device.h"
#include "analyzers/analyzer.h"
#include "analyzers/trackchi2.h"
#include "loopers/loopaligntracks.h"

#define CHI2_MAX_TRACK_SIZE 100

namespace Loopers {

void LoopAlignTracks::Chi2Minimizer::computeTrackPars(
    const Analyzers::TrackChi2::Track& track,
    LoopAlignTracks::TrackPars& pars) const {
  const size_t nclusters = track.m_nclusters;
  const size_t istart = track.m_istart;

  if (nclusters > CHI2_MAX_TRACK_SIZE) throw std::runtime_error(
      "LoopAlignTracks::Chi2Minimizer::ComputeTrackPars: exceeded "
      "pre-allocated stack. Increase CHI2_MAX_TRACK_SIZE define");

  // Put the linear fit inputs on the stack for performance
  double x[CHI2_MAX_TRACK_SIZE];
  double xe[CHI2_MAX_TRACK_SIZE];
  double y[CHI2_MAX_TRACK_SIZE];
  double ye[CHI2_MAX_TRACK_SIZE];
  double z[CHI2_MAX_TRACK_SIZE];

  double dummy = 0;  // for references which are not needed

  // Compute the global position of all cluster in the track
  for (size_t i = 0; i < nclusters; i++) {
    const Analyzers::TrackChi2::Cluster& cluster = m_clusters[istart+i];
    cluster.sensor->pixelToSpace(
        cluster.pixX, cluster.pixY, 
        x[i], y[i], z[i]);
    cluster.sensor->pixelErrToSpace(
        cluster.pixErrX, cluster.pixErrY, 
        xe[i], ye[i], dummy);
  }

  double chi2 = 0;  // chi^2 is the sum of x and y

  Utils::linearFit(
      nclusters, &z[0], &x[0], &xe[0],
      pars.p0x,
      pars.p1x, 
      dummy, dummy, dummy,
      chi2);
  pars.chi2 += chi2;  // add x component to chi^2

  Utils::linearFit(
      nclusters, &z[0], &y[0], &ye[0],
      pars.p0y,
      pars.p1y, 
      dummy, dummy, dummy,
      chi2);
  pars.chi2 += chi2;

  pars.chi2 /= (double)nclusters;
}

LoopAlignTracks::Chi2Minimizer::Chi2Minimizer(
    Mechanics::Device& device,
    int flags,
    const std::list<Analyzers::TrackChi2::Cluster>& clusters,
    const std::list<Analyzers::TrackChi2::Track>& tracklets) :
    // Copying over the light weight objects needed for alignment. The
    // tracks refere to clusters by index, so the order is preserved.
    m_device(&device),
    m_flags(flags),
    // Copy the tracks and cluster from which to comute chi^2
    m_clusters(clusters.begin(), clusters.end()),
    m_tracks(tracklets.begin(), tracklets.end()),
    m_ndim(0),
    m_minimizer(0) {
  // Number of degrees of freedom to align per sensor. When in plane, do not
  // align the x and y rotations
  const unsigned ndof = (m_flags & INPLANE) ? 3 : 5;

  if (m_flags & REFERENCE) {
    // Don't align the first plane
    m_ndim = ndof * (m_device->getNumSensors()-1);
  }

  // Configuration for DUT alignment
  else {
    // Align all sensors
    m_ndim = ndof * m_device->getNumSensors();
    // Pre-compute the track parameters. They won't change since the DUT
    // clusters aren't used in the tracks
    m_trackPars.assign(m_tracks.size(), LoopAlignTracks::TrackPars());
    for (size_t i = 0; i < m_tracks.size(); i++)
      computeTrackPars(m_tracks[i], m_trackPars[i]);
  }
}

double LoopAlignTracks::Chi2Minimizer::DoEval(const double* pars) const {
  // Sum of chi^2 of tracks is computed here
  double sum = 0;

  // Set the alignment for each sensor from the new parameters
  size_t ipar = 0;  // keep track of the parameter being read
  const size_t nsensors = m_device->getNumSensors();
  for (size_t isensor = 0; isensor < nsensors; isensor++) {
    // Don't align the first plane if this is a reference device
    if ((m_flags & REFERENCE) && (isensor == 0)) continue;
    Mechanics::Sensor& sensor = m_device->getSensor(isensor);
    // The first two parameters for each sensor are the offset
    sensor.setAlignment(Mechanics::Alignment::OFFX, pars[ipar++]);
    sensor.setAlignment(Mechanics::Alignment::OFFY, pars[ipar++]);
    // The next two parameters are rotations in X and Y which can be disabled
    // since they don't always play nice
    if (!(m_flags & INPLANE)) {
      sensor.setAlignment(Mechanics::Alignment::ROTX, pars[ipar++]);
      sensor.setAlignment(Mechanics::Alignment::ROTY, pars[ipar++]);
    }
    // Finally the rotation in z
    sensor.setAlignment(Mechanics::Alignment::ROTZ, pars[ipar++]);
  }

  // Loop through all the tracks, computing their chi^2
  const size_t ntracks = m_tracks.size();
  for (size_t itrack = 0; itrack < ntracks; itrack++) {
    // For reference devices, re-compute the track parameters and sum chi^2
    if (m_flags & REFERENCE) {
      TrackPars pars;
      computeTrackPars(m_tracks[itrack], pars);
      sum += pars.chi2;
    }

    // For DUT devices, use the pre-computed track parameters, extrapolate to
    // each matching cluster, and add up the residuals as with chi^2
    else {
      assert(!m_trackPars.empty() && "Tracks not pre-computed");

      const Analyzers::TrackChi2::Track& track = m_tracks[itrack];
      const size_t istart = track.m_istart + track.m_nclusters;
      const size_t nmatches = track.m_nmatches;
      const TrackPars& pars = m_trackPars[itrack];

      // Project to the distance of each matched cluster
      for (size_t i = 0; i < nmatches; i++) {
        const Analyzers::TrackChi2::Cluster& match = m_clusters[istart+i];

        double x, y, z, ex, ey, dummy;
        match.sensor->pixelToSpace(
            match.pixX, match.pixY, 
            x, y, z);
        match.sensor->pixelErrToSpace(
            match.pixErrX, match.pixErrY,
            ex, ey, dummy);

        const double tx = pars.p0x + pars.p1x * z;
        const double ty = pars.p0y + pars.p1y * z;
        const double dx = (tx-x)/ex;
        const double dy = (ty-y)/ey;

        sum += dx*dx + dy*dy;
      }
      sum /= (double)nmatches;
    }
  }

  const double value = sum / (double)ntracks;

  if (m_minimizer)
    std::printf(
        "\rMinimization chi^2 and EDM: %.4e, %.1e",
        value, m_minimizer->Edm());
  else
    std::printf(
        "\rMinimization chi^2: %.4e", value);

  return value;
}

ROOT::Math::IBaseFunctionMultiDim* LoopAlignTracks::Chi2Minimizer::Clone() const {
  return new LoopAlignTracks::Chi2Minimizer(*this);
}

LoopAlignTracks::LoopAlignTracks(
    const std::vector<Storage::StorageI*>& inputs,
    const std::vector<Mechanics::Device*>& devices) :
    Looper(inputs, devices),
    m_trackChi2(devices),
    // Tracking for only the reference device. Note that Looper::Looper ensures
    // at least one device in the vector
    m_tracking(devices[0]->getNumSensors()),
    m_translationScale(1),  // 1 pixel translation scale
    m_translationLimit(-1),  // no need to really limit translations
    m_rotationScale(0.01),  // ~ half degree rotation scale
    m_rotationLimit(0.1),  // 5 degree window in which to align rotations
    m_inPlane(false),  // default to aligning all d.o.f's
    m_tolerance(1e-2) {  // good tolerance for minuit
  if (m_devices.size() > 2)
    throw std::runtime_error(
        "LoopAlignTracks::LoopAlignTracks: supports at most two devices");
  // Let the base class deal with the analyzer
  addAnalyzer(m_trackChi2);
}

LoopAlignTracks::LoopAlignTracks(
    Storage::StorageI& input,
    Mechanics::Device& device) :
    Looper(input, device),
    m_trackChi2(device),
    m_tracking(device.getNumSensors()),
    m_translationScale(1),
    m_translationLimit(-1),
    m_rotationScale(0.01),
    m_rotationLimit(0.1),
    m_inPlane(false),
    m_tolerance(1e-2) { 
  addAnalyzer(m_trackChi2);
}

void LoopAlignTracks::execute() {
  for (std::vector<Processors::Processor*>::iterator it = m_processors.begin();
      it != m_processors.end(); ++it)
    (*it)->execute(m_events);

  // Tracking needs to be called after any other processors, and only on the
  // reference device (event 0)
  m_tracking.execute(*m_events[0]);

  for (std::vector<Analyzers::Analyzer*>::iterator it = m_analyzers.begin();
      it != m_analyzers.end(); ++it)
    (*it)->execute(m_events);
}

void LoopAlignTracks::finalize() {
  Looper::finalize();
  assert(m_devices.size() >= 1 && "No devices to finalize");

  // The device to align (either the refernce if only 1 is given, or the DUT
  // when two are given)
  Mechanics::Device& device = *m_devices.back();
  const bool isRef = m_devices.size() == 1;

  int evalFlags = 0;
  if (isRef) evalFlags |= Chi2Minimizer::REFERENCE;
  if (m_inPlane) evalFlags |= Chi2Minimizer::INPLANE;

  // Build the min function using the list of light-weight clusters and tracks
  // build byt the trackChi2 analyzer. The minimzer will copy the objects into
  // continuous memory, which might help with caching.
  Chi2Minimizer minEval(
      device,
      evalFlags,
      m_trackChi2.getClusters(),
      m_trackChi2.getTracks());

  // Build the default minimizer (probably Minuit with Migrad). Note ROOT 
  // relinquishes owernship of the object, so put it in a smart pointer which
  // will delete it when out of scope (i.e. method ends or throws exception).
  std::unique_ptr<ROOT::Math::Minimizer> minimizer(
      ROOT::Math::Factory::CreateMinimizer("Minuit"));
  minimizer->SetFunction(minEval);

  minEval.m_minimizer = minimizer.get();

  size_t ipar = 0;  // keep track of the parameter index being setup

  // Set the minimizer parameters (degrees of freedom for each sensor)
  const size_t nsensors = device.getNumSensors();
  for (size_t isensor = 0; isensor < nsensors; isensor++) {
    if (isRef && isensor == 0) continue;  // reference doesn't align plane 1
    const Mechanics::Sensor& sensor = device[isensor];

    // Compute the spatial distance between (0,0) and (1,1)
    double x0, y0, z0;
    sensor.pixelToSpace(0,0,x0,y0,z0);
    double x1, y1, z1;
    sensor.pixelToSpace(1,1,x1,y1,z1);
    // Use to set the translation scale as 1 pixel
    const double sizeX = std::fabs(x0-x1);
    const double sizeY = std::fabs(y0-y1);

    // Set up the 5 parameters for this sensor
    for (int i = 0; i < 5; i++) {
      double value = 0;
      double scale = 0;
      double limit = 0;

      switch (i) {
      case 0:
        value = sensor.getOffX();
        scale = m_translationScale*sizeX;
        limit = m_translationLimit;
        break;
      case 1:
        value = sensor.getOffY();
        scale = m_translationScale*sizeY;
        limit = m_translationLimit;
        break;
      case 2:
        if (m_inPlane) continue;  // don't use this parameter if in plane
        value = sensor.getRotX();
        scale = m_rotationScale;
        limit = m_rotationLimit;
        break;
      case 3:
        if (m_inPlane) continue;
        value = sensor.getRotY();
        scale = m_rotationScale;
        limit = m_rotationLimit;
        break;
      case 4:
        value = sensor.getRotZ();
        scale = m_rotationScale;
        limit = m_rotationLimit;
        break;
      default:
        assert(false && "Wrong number of cases");
      }

      minimizer->SetVariable(ipar, "", value, scale);
      if (limit > 0)
        minimizer->SetVariableLimits(ipar, value-limit, value+limit);

      ipar += 1;
    }
  }

  minimizer->SetTolerance(m_tolerance);
  minimizer->Minimize();

  ipar = 0;
  const double* const pars = minimizer->X();

  for (size_t isensor = 0; isensor < nsensors; isensor++) {
    if (isRef && isensor == 0) continue;  // reference doesn't align plane 1
    Mechanics::Sensor& sensor = device.getSensor(isensor);
    sensor.setAlignment(Mechanics::Alignment::OFFX, pars[ipar++]);
    sensor.setAlignment(Mechanics::Alignment::OFFY, pars[ipar++]);
    if (!m_inPlane) {
      sensor.setAlignment(Mechanics::Alignment::ROTX, pars[ipar++]);
      sensor.setAlignment(Mechanics::Alignment::ROTY, pars[ipar++]);
    }
    sensor.setAlignment(Mechanics::Alignment::ROTZ, pars[ipar++]);
  }
}

}

