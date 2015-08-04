#include <iostream>
#include <stdexcept>
#include <cassert>
#include <cmath>

#include "storage/hit.h"
#include "storage/cluster.h"
#include "storage/plane.h"
#include "storage/event.h"
#include "mechanics/device.h"
#include "processors/aligning.h"

namespace Processors {

void Aligning::processEvent(
    Storage::Event& event,
    const Mechanics::Device& device) {
  // Ensure the event has the same planes as the device
  if (event.getNumPlanes() != device.getNumSensors())
    throw std::runtime_error("Aligning::process: plane/sensor mismatch");

  // Get hits and clusters one plane at a time, and apply alignment
  for (size_t iplane = 0; iplane < event.getNumPlanes(); iplane++) {
    const Storage::Plane& plane = event.getPlane(iplane);

    // Iterate through all hits in this place
    const std::vector<Storage::Hit*>& hits = plane.getHits();
    for (std::vector<Storage::Hit*>::const_iterator it = hits.begin();
        it != hits.end(); ++it) {
      double x, y, z;
      Storage::Hit& hit = **it;
      // Ask the device to apply first local, then global transformation to
      // the hit's pixel coordinates
      device[iplane].pixelToSpace(
          hit.getPixX(), hit.getPixY(), x, y, z);
      hit.setPos(x, y, z);
    }

    // Iterate through all clusters in this plane
    const std::vector<Storage::Cluster*>& clusters = plane.getClusters();
    for (std::vector<Storage::Cluster*>::const_iterator it = clusters.begin();
        it != clusters.end(); ++it) {
      Storage::Cluster& cluster = **it;
      double x, y, z;
      device[iplane].pixelToSpace(
          cluster.getPixX(), cluster.getPixY(), x, y, z);
      cluster.setPos(x, y, z);
      double ex, ey, ez;
      device[iplane].pixelErrToSpace(
          cluster.getPixErrX(), cluster.getPixErrY(), ex, ey, ez);
      cluster.setPosErr(ex, ey, ez);
    }
  }
}

void Aligning::process() {
  assert(!m_devices.empty() && "Can't construct with no devices");
  for (size_t i = 0; i < m_ndevices; i++) {
    processEvent(*m_events[i], *m_devices[i]);
  }
}

}

