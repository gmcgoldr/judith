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

#include "utils.h"
#include "storage/event.h"
#include "storage/plane.h"
#include "storage/cluster.h"
#include "storage/track.h"
#include "mechanics/device.h"
#include "mechanics/sensor.h"
#include "analyzers/synchronization.h"

namespace Analyzers {

void Synchronization::process() {
  const Storage::Event& event1 = *m_events[0];
  const Storage::Event& event2 = *m_events[1];

  m_times1[m_nprocessed] = event1.getTimeStamp();
  m_times2[m_nprocessed] = event2.getTimeStamp();

  m_nprocessed += 1;

  // Note: want to know the number of *differences* counted, which is one less
  // than the number of events processed
  const size_t n = m_nprocessed-1;
  if (n == 0) return;

  const double delta1 = m_times1[n] - m_times1[n-1];
  const double delta2 = m_times2[n] - m_times2[n-1];

  // Stop computing ratios if the events are desynchronized
  if (m_desynchronized) return;

  const double ratio = delta2 / delta1;
  m_ratioMean += (ratio - m_ratioMean) / n;

  const double diff = std::fabs(delta2 - delta1 * m_ratioMean);
  const double ddiff = diff - m_diffMean;
  m_diffMean += ddiff / n;
  m_diffVariance += ddiff * (diff - m_diffMean);

  // Check if this event ratio is far from the mean, if enough ratios have been
  // processed to get a good estimate of the mean
  if (n >= m_minStats) {
    const double scale = std::sqrt(m_diffVariance/(double)(n-1));
    if (std::fabs(diff/scale) >= m_threshold) {
      m_desynchronized = true;
      m_diffVariance -= ddiff * (diff - m_diffMean);
      m_diffMean -= ddiff / n;
      return;
    }
  }

  m_preSpacings.push_back(std::fabs(delta1));
  m_preDiffs.push_back(std::fabs(diff));

  m_ndiffs = n;
}

void Synchronization::finalize() {
  Analyzer::finalize();

  if (m_ndiffs < m_minStats) throw std::runtime_error(
        "Synchronization::finalize: not enough events processed");

  // Finalize the ratio and scale computations, if not provided
  if (m_ratio == 0) m_ratio = m_ratioMean;
  if (m_scale == 0) m_scale = std::sqrt(m_diffVariance/(double)(m_ndiffs-1));

  std::printf(
      "Threshold: %.2e\n",
      m_threshold*m_scale);
  std::printf(
      "False positive rate: %.2e\n",
      0.5*(1-std::erf(m_threshold*0.7071)));

  std::fflush(stdout);

  // This isn't efficient, but this algorithm is probably fast anyway
  std::vector<double> preSpacings(m_preSpacings.begin(), m_preSpacings.end());
  std::vector<double> preDiffs(m_preDiffs.begin(), m_preDiffs.end());

  std::sort(preSpacings.begin(), preSpacings.end());
  const double range = preSpacings[(1-0.1)*m_ndiffs-1];

  if (!m_histPreSpacings) {
    m_histograms.push_back(new TH1D(
        "SyncSpacing", "SyncSpacing",
        50, 0, range));
    m_histPreSpacings = (TH1D*)m_histograms.back();
  }

  if (!m_histPreDiffs) {
    m_histograms.push_back(new TH1D(
        "SyncDiffs", "SyncDiffs",
        50, 0, range));
    m_histPreDiffs = (TH1D*)m_histograms.back();
    m_histPreDiffs->SetDirectory(0);
  }

  for (size_t i = 0; i < m_ndiffs; i++) {
    m_histPreSpacings->Fill(preSpacings[i]);
    m_histPreDiffs->Fill(preDiffs[i]);
    m_histPreSpacings->SetDirectory(0);
  }

  TCanvas* can = new TCanvas("Canvas", "Canvas", 800, 600);

  m_histPreDiffs->SetLineWidth(2);
  m_histPreDiffs->SetLineColor(kBlue);
  m_histPreSpacings->SetLineWidth(2);
  m_histPreSpacings->SetLineColor(kRed);

  m_histPreDiffs->Draw();
  m_histPreSpacings->Draw("SAME");
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

