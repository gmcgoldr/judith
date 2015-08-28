#include <iostream>
#include <stdexcept>
#include <cmath>

#include <TH1.h>
#include <TF1.h>
#include <TFitResult.h>
#include <TFitResultPtr.h>

#include "utils.h"

namespace Utils {

void preFitGausBg(
    TH1& hist,
    double& mode,
    double& hwhm,
    double& norm,
    double& bg) {
  const Int_t nbins = hist.GetNbinsX();
  const Int_t imax = hist.GetMaximumBin();

  // Get a guess at the scale factor and mode
  const double scale = hist.GetBinContent(imax);
  mode = hist.GetBinCenter(imax);

  // Increase the width until half of maximum is reached
  Int_t width = 1;
  while (true) {
    // Once both sides half reached half max or overshot, then keep that width
    if ((imax+width > nbins || hist.GetBinContent(imax+width) <= scale/2.) &&
        (imax-width < 1 || hist.GetBinContent(imax-width) <= scale/2.))
        break;
    // Increment width for next iteration
    width += 1;
  }
  hwhm = width * hist.GetBinWidth(imax);

  // Also get an estimate of the background by going out to 5 sigma and looking
  // at the content
  const Int_t up = std::min(imax+5*width, nbins);
  const Int_t down = std::max(imax-5*width, 1);
  bg = hist.GetBinContent(up)/2. + hist.GetBinContent(down)/2.;

  // Compute the normal distribution normalization from the scale
  norm = scale * 2.50662827463 * hwhm;
}

void fitGausBg(
    TH1& hist,
    double& mean,
    double& sigma,
    double& norm,
    double& bg,
    bool prefit,
    bool display,
    double fitRange) {
  // Gaussian distrubiton fitting function
  TF1 gaus(
      "g1", 
      "[0]+[1]/([3]*2.50662827463)*exp(-(x-[2])*(x-[2])/(2*[3]*[3]))",
      -5, 5);

  // Guess at starting parameters if requested
  if (prefit) preFitGausBg(hist, mean, sigma, norm, bg);

  // Set the parameters to initial values
  gaus.SetParameter(0, bg);
  gaus.SetParameter(1, norm);
  gaus.SetParameter(2, mean);
  gaus.SetParameter(3, sigma);

  // Bounds will be set just outside range
  const double xmin = hist.GetXaxis()->GetXmin()*1.01;
  const double xmax = hist.GetXaxis()->GetXmax()*1.01;
  gaus.SetParLimits(0, 0, 2*norm);
  gaus.SetParLimits(1, 0, 2*norm);
  gaus.SetParLimits(2, xmin, xmax);
  gaus.SetParLimits(3, 0, xmax-xmin);

  gaus.SetRange(mean-fitRange*sigma, mean+fitRange*sigma);

  // N: no plotting
  // Q: quiet
  // S: store results
  // L: likelihood (instead of chi^2)
  // R: use function range
  TFitResultPtr fit = hist.Fit(&gaus, display ? "QLSR" : "NQLSR");

  mean = fit->Parameter(2);
  sigma = fit->Parameter(3);
  norm = fit->Parameter(1);
  bg = fit->Parameter(0);

  // Normalize to bin width to get proper PDF values * number
  norm /= hist.GetBinWidth(1);
  bg /= hist.GetBinWidth(1);
}

void linearFit(
    const unsigned n,
    const double* x,
    const double* y,
    const double* ye,
    double& p0,
    double& p1,
    double& p0e,
    double& p1e,
    double& cov,
    double& chi2) {
  p0 = 0;
  p1 = 0;
  p0e = 0;
  p1e = 0;
  cov = 0;
  chi2 = 0;

  // Regression variables
  double ss = 0;
  double sx = 0;
  double sy = 0;
  double sxoss = 0;
  double st2 = 0;

  for (unsigned int i = 0; i < n; i++) {
    const double wt = 1. / (ye[i]*ye[i]);
    if (!std::isfinite(wt))
      throw std::runtime_error("Utils: linearFit: invalid weight");
    ss += wt;
    sx += x[i] * wt;
    sy += y[i] * wt;
  }

  sxoss = sx / ss;

  for (unsigned i = 0; i < n; i++) {
    const double t = (x[i]-sxoss) / ye[i];
    st2 += t*t;
    p1 += t*y[i]/ye[i];
  }

  p1 /= st2;
  p0 = (sy - sx*p1) / ss;

  p1e = std::sqrt(1./st2);
  p0e = sqrt((1. + sx*sx / (ss*st2)) / ss);

  for (unsigned i = 0; i < n; i++)
    chi2 += std::pow((y[i] - (p0 + p1*x[i])) / ye[i], 2);

  cov = -sx / (ss * st2);
}

void linePlaneIntercept(
    double p0x,
    double p1x,
    double p0y,
    double p1y,
    double originX,
    double originY,
    double originZ,
    double normalX,
    double normalY,
    double normalZ,
    double& x,
    double& y,
    double& z) {
  // p0: point on plane, l0: point on line, l: line vector, n normal

  // (p0 - l0) dot n
  const double num =
      (originX-p0x) * normalX +
      (originY-p0y) * normalY +
      (originZ-0.0) * normalZ;

  // l dot n
  const double denum = 
      p1x * normalX +
      p1y * normalY +
      1.0 * normalZ;

  // The distance along the track to the intercept
  const double d = num / denum;

  // Extrapolate to the intercept
  x = d * p1x + p0x;
  y = d * p1y + p0y;
  z = d * 1.0 + 0.0;
}

void synchronize(
    const std::vector<ULong64_t>& times1,
    const std::vector<ULong64_t>& times2,
    std::vector<bool>& write1,
    std::vector<bool>& write2,
    double ratio,
    double threshold,
    unsigned nbuffer) {
  const size_t nevents = times1.size();
  if (times2.size() != nevents)
    throw std::runtime_error("Untils: synchronize: inconsistent nevents");

  // By default, all events are not being written, until they are assessed to
  // be good
  write1.assign(nevents, false);
  write2.assign(nevents, false);

  // Keep track of which events initially passed synchronization
  std::vector<bool> buffPass(nbuffer, false);
  // Index of event to be written from device 1
  std::vector<size_t> buffIndex1(nbuffer, 0);
  // Index of event to be written from device 2
  std::vector<size_t> buffIndex2(nbuffer, 0);
  size_t npass = 0;  // number of events passing in buffer
  size_t ibuff = 0;  // current buffer index

  // Delta of an event is defined as the time to the next trigger

  // Index of event whose delta is being evaluated
  size_t i1 = 0;
  size_t i2 = 0;

  while (i1+1 < nevents && i2+1 < nevents) {
    // Setp to the next event
    size_t next1 = 1;
    size_t next2 = 1;
    // Ensure there is another event from which to compute delta
    if (i1+next1 >= nevents || i2+next2 >= nevents) break;
    // Compute the difference from the current event to the next
    const double delta1 = times1[i1+next1] - times1[i1];
    const double delta2 = times2[i2+next2] - times2[i2];
    // evaluate how closely the two differences agree
    double diff = delta2 - delta1 * ratio;

    // Update npass to remove the one being overwritten
    npass -= buffPass[ibuff];
    // Store if this event passed or failed
    buffPass[ibuff] = diff < threshold;
    // Count this event into the number of good events in buffer
    npass += diff < threshold;

    // If the difference is above threshold, try to fix the problem
    if (diff >= threshold) {
      size_t ioff = 0;  // index of offset trial

      while (true) {
        // 0, 3 ... try moving times1 up
        if ((ioff+0) % 3 == 0) {
          next1 = 1 + (ioff+3)/3;
          next2 = 1;
        }
        // 1, 4, ... try moving times2 up
        else if ((ioff+2) % 3 == 0) {
          next1 = 1;
          next2 = 1 + (ioff+3)/3;
        }
        // 2, 5, ... try moving both up
        else if ((ioff+1) % 3 == 0) {
          next1 = 1 + (ioff+3)/3;
          next2 = 1 + (ioff+3)/3;
        }

        // Ensure there is another event from which to compute delta
        if (i1+next1 >= nevents || i2+next2 >= nevents) break;

        // Update the difference with the new offset
        const double delta1 = times1[i1+next1] - times1[i1];
        const double delta2 = times2[i2+next2] - times2[i2];
        diff = delta2 - delta1 * ratio;

        // Found a good offset
        if (diff < threshold) break;

        ioff += 1;
      }
    }

    // Otherwise simply push the indices into the buffer
    else {
      buffIndex1[ibuff] = i1;
      buffIndex2[ibuff] = i2;
    }

    // If the buffer is fully synchronized, then the events at the back are
    // good for writing

    if (npass == nbuffer) {
      write1[buffIndex1[(ibuff+1) % nbuffer]] = true;
      write2[buffIndex2[(ibuff+1) % nbuffer]] = true;
    }

    // Move to the next buffer spot, and the next set of timestamps
    ibuff = (ibuff+1) % nbuffer;
    i1 += next1;
    i2 += next2;
  }
}

}

