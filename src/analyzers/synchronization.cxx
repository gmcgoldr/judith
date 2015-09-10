#include <iostream>
#include <stdexcept>
#include <cassert>
#include <string>
#include <algorithm>
#include <vector>
#include <list>
#include <cmath>
#include <cstdio>

#include <TCanvas.h>
#include <TH1D.h>
#include <TLegend.h>

#include "rootstyle.h"
#include "utils.h"
#include "storage/event.h"
#include "storage/plane.h"
#include "storage/cluster.h"
#include "storage/track.h"
#include "mechanics/device.h"
#include "mechanics/sensor.h"
#include "analyzers/synchronization.h"

namespace Analyzers {

void Synchronization::reserve(size_t nevents) {
  m_times1.assign(nevents, 0);
  m_times2.assign(nevents, 0);
  m_write1.assign(nevents, false);
  m_write2.assign(nevents, false);
  m_preSpacings.reserve(s_maxStats);
  m_preDiffs.reserve(s_maxStats);
  m_reserved = true;
}

void Synchronization::process() {
  if (!m_reserved) throw std::runtime_error(
        "Synchronization::process: didn't reserve memory");

  const Storage::Event& event1 = *m_events[0];
  const Storage::Event& event2 = *m_events[1];

  // Collect the time stamps, so that the synchronization routine can be run
  // on all of them in memory at once
  m_times1[m_nprocessed] = event1.getTimeStamp();
  m_times2[m_nprocessed] = event2.getTimeStamp();

  m_nprocessed += 1;

  // The following code tries to compute some statistics on trigger spacing and
  // agreement between the two devices for synchronized events.

  // Note: want to know the number of *differences* counted, which is one less
  // than the number of events processed
  const size_t n = m_nprocessed-1;

  // Need at least 1 difference computed, don't keep computing if an event
  // has been seen as desychronized, and stop once enough stats have been
  // accumulated
  if (n > s_maxStats || m_desynchronized || n == 0) return;

  // The spacing from the last event to this one, for both devices
  const double delta1 = m_times1[n] - m_times1[n-1];
  const double delta2 = m_times2[n] - m_times2[n-1];

  // Update the mean ratio of the two device clocks
  const double ratio = delta2 / delta1;
  m_ratioMean += (ratio - m_ratioMean) / n;

  // Update the variance of the agreement between the two clocks
  const double diff = delta2 - delta1 * m_ratioMean;
  m_diffVariance += diff*diff;

  // Check if this event ratio is far from the mean, if enough ratios have been
  // processed to get a good estimate of the mean
  if (n >= m_minStats) {
    const double scale = std::sqrt(m_diffVariance/(double)(n-1));
    if (std::fabs(diff/scale) >= m_threshold) {
      m_desynchronized = true;
      // Up-update the mean and variance, this event is bad
      m_ratioMean = (n*m_ratioMean - ratio) / (n-1);
      m_diffVariance -= diff*diff;
      return;
    }
  }

  // Keep track of the inter-trigger spacings
  m_preSpacings.push_back(std::fabs(delta1));
  // Keep track of the inter-trigger spacing differences
  m_preDiffs.push_back(std::fabs(diff));

  // And the number of differences collected
  m_ndiffs = n;
}

void Synchronization::finalize() {
  Analyzer::finalize();

  // Finalize the ratio and scale computations, if not provided
  if (m_ratio == 0) m_ratio = m_ratioMean;
  if (m_scale == 0) m_scale = std::sqrt(m_diffVariance/(double)(m_ndiffs-1));

  std::printf(
      "Threshold: %.2e\n",
      m_threshold*m_scale);
  // Note two-sided tails: RMS computed from differences about 0, so anytime
  // the absolute value of a difference exceeds threshold, it fails
  std::printf(
      "False positive rate: %.2e\n",
      (1-std::erf(m_threshold*0.7071)));

  std::fflush(stdout);

  // Some spacings can be very large which will make the plot useless. Sort
  // and use only the first 95%
  std::sort(m_preSpacings.begin(), m_preSpacings.end());
  const double range = m_preSpacings[0.95*m_ndiffs-1];

  // Build the histogram of the inter-trigger spacing (for first 95%)
  if (!m_histPreSpacings) {
    m_histograms.push_back(new TH1D(
        "SyncSpacing", "SyncSpacing", 50, 0, range));
    m_histPreSpacings = (TH1D*)m_histograms.back();
    m_histPreSpacings->SetDirectory(0);
  }

  // Also the histogram of differences between spacing for both devices, on
  // the same axis
  if (!m_histPreDiffs) {
    m_histograms.push_back(new TH1D(
        "SyncDiffs", "SyncDiffs", 50, 0, range));
    m_histPreDiffs = (TH1D*)m_histograms.back();
    m_histPreDiffs->SetDirectory(0);
  }

  // Fill the histogram with the stored values
  for (size_t i = 0; i < m_ndiffs; i++) {
    m_histPreSpacings->Fill(m_preSpacings[i]);
    m_histPreDiffs->Fill(m_preDiffs[i]);
  }

  TCanvas* can = new TCanvas("Canvas", "Canvas", 800, 600);

  m_histPreDiffs->SetLineColor(RS::blue);
  m_histPreSpacings->SetLineColor(RS::red);

  m_histPreDiffs->GetXaxis()->SetTitle("Device 1 Time");
  m_histPreDiffs->GetYaxis()->SetTitle("Events");

  m_histPreDiffs->Draw();
  m_histPreSpacings->Draw("SAME");

  TLegend legend(0.6, 0.9, 0.9, 0.9-0.045*2);
  legend.AddEntry(m_histPreSpacings, "Trigger spacing", "l");
  legend.AddEntry(m_histPreDiffs, "Spacing difference", "l");
  legend.Draw("SAME");

  can->SetLogy();

  can->WaitPrimitive();

  Utils::synchronize(
      m_times1,
      m_times2,
      m_write1,
      m_write2,
      m_ratio,
      m_threshold*m_scale,
      m_nconsecutive);
}

}

