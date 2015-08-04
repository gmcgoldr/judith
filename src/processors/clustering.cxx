#include <iostream>
#include <vector>
#include <stdexcept>
#include <list>
#include <stack>
#include <cmath>
#include <climits>
#include <algorithm>
#include <cassert>

#include "storage/hit.h"
#include "storage/cluster.h"
#include "storage/plane.h"
#include "storage/event.h"
#include "processors/clustering.h"

namespace Processors {

void Clustering::clusterSeed(
    Storage::Hit& seed,
    std::list<Storage::Hit*>& hits,
    std::list<Storage::Hit*>& clustered) {
  // List of hist for which to search for neighbours
  std::stack<Storage::Hit*> search;

  clustered.push_back(&seed);  // add seed to the cluster
  search.push(&seed);  // add seed to neighbour search

  // For each clustered hit, search for its neighbours
  while (!search.empty()) {
    // Target hit about which to find neighbours
    const Storage::Hit* target = search.top();
    search.pop();

    // Look through remaining unclustered hits for neighbouring ones
    for (std::list<Storage::Hit*>::iterator it = hits.begin();
        it != hits.end(); ++it) {
      Storage::Hit* hit = *it;  // candidate neighbour
      // If this hit isn't close enough to the target, move on
      if (std::abs(hit->getPixX()-target->getPixX()) > m_maxRows ||
          std::abs(hit->getPixY()-target->getPixY()) > m_maxCols)
        continue;
      clustered.push_back(hit);
      // Erease from the list of unclustered list. This returns the next
      // iterator. Move it back so that the loop moves forward correctly.
      it = --(hits.erase(it));
      // This hit must now also be included in the neighbour search
      search.push(hit);
    }
  }
}

void Clustering::buildCluster(
    Storage::Cluster& cluster,
    std::list<Storage::Hit*>& clustered) {
  const size_t nhits = clustered.size();
  assert(nhits >= 1 && "Building empty cluster");
  cluster.reserveHits(nhits);

  // Variables for weighted incremental variance computation
  // http://en.wikipedia.org/wiki/Algorithms_for_calculating_variance#Online_algorithm
  double sumw = 0;
  double meanX = 0;
  double meanY = 0;
  double m2X = 0;  // sum squared of differences to mean
  double m2Y = 0;

  // Keep track of cluster range to know if using 1/sqrt(12). Don't rely on
  // m2's since they are subject to be close to 0 if small weights occur.
  int minX = INT_MAX;
  int maxX = INT_MIN;
  int minY = INT_MAX;
  int maxY = INT_MIN;

  for (std::list<Storage::Hit*>::iterator it = clustered.begin();
      it != clustered.end(); ++it) {
    // Add the hit to the cluster
    Storage::Hit& hit = **it;
    cluster.addHit(hit);

    // Weight the hits by their value if that mode is turned on
    const double weight = m_weighted ? hit.getValue() : 1;
    const double tmp = weight + sumw;

    minX = std::min(hit.getPixX(), minX);
    maxX = std::max(hit.getPixX(), maxX);
    minY = std::min(hit.getPixY(), minY);
    maxY = std::max(hit.getPixY(), maxY);

    // Single pass mean and variance computation
    const double deltaX = hit.getPixX() - meanX;
    const double rX = deltaX * weight / tmp;
    meanX += rX;
    m2X += sumw * deltaX * rX;  // incremental formulation

    // Again in y
    const double deltaY = hit.getPixY() - meanY;
    const double rY = deltaY * weight / tmp;
    meanY += rY;
    m2Y += sumw * deltaY * rY;

    sumw = tmp;
  }

  // Set the cluster mean
  cluster.setPix(meanX, meanY);

  const double errX = 
      (maxX - minX < 1) ?  // check if hits span more than 1 pixel
      1./std::sqrt(12) :  // they don't, so use RMS of uniform distribution
      std::sqrt(m2X/sumw * nhits / (double)(nhits-1));  // use RMS

  const double errY = 
      (maxY - minY < 1) ?
      1./std::sqrt(12) :
      std::sqrt(m2Y/sumw * nhits / (double)(nhits-1));

  cluster.setPixErr(errX, errY);
}

void Clustering::processEvent(Storage::Event& event) {
  // Don't add new clusters atop existing ones in an event
  if (event.getNumClusters())
    throw std::runtime_error("Clustering::process: event is already clustered");

  // Cluster each plane's hits, plane by plane
  const size_t nplanes = event.getNumPlanes();
  for (size_t iplane = 0; iplane < nplanes; iplane++) {
    const Storage::Plane& plane = event.getPlane(iplane);
    if (plane.getNumHits() == 0) continue;

    // Store all hits in this plane in a list
    std::list<Storage::Hit*> hits(
        plane.getHits().cbegin(), plane.getHits().cend());

    while (!hits.empty()) {
      // Use the last hit as the seed hit, and remove from hits to cluster
      Storage::Hit& seed = *hits.back();
      hits.pop_back();
      // Build the cluster, removing all clustered hits along the way
      std::list<Storage::Hit*> clustered;
      clusterSeed(seed, hits, clustered);
      // Compute the cluster's values and store in cluster object
      Storage::Cluster& cluster = event.newCluster(iplane);
      buildCluster(cluster, clustered);
      // Note that the clustered hits are no longer in the `hits` list, so the
      // next pass will pick the next hit which wasn't clustered in this one
    }
  }
}

void Clustering::process() {
  for (std::vector<Storage::Event*>::iterator it = m_events.begin();
      it != m_events.end(); ++it)
    processEvent(**it);
}

}

