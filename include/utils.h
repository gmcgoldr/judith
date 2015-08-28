#ifndef UTILS_H
#define UTILS_H

#include <Rtypes.h>
#include <TH1.h>
#include <TF1.h>

namespace Utils {

void preFitGausBg(
    TH1& hist,
    double& mode,
    double& hwhm,
    double& norm,
    double& bg);

void fitGausBg(
    TH1& hist,
    double& mean,
    double& sigma,
    double& norm,
    double& bg,
    bool prefit=false,
    bool display=false,
    double fitRange=5);

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
    double& chi2);

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
    double& X,
    double& y,
    double& z);

/** Given two vectors of event time stamps, fill two bool vectors with the
  * synchronization status of each event. False means the event should not
  * be used, if synchronization is to be kept. */
void synchronize(
    // device 1 time stamps
    const std::vector<ULong64_t>& times1,
    // device 2 time stamsp
    const std::vector<ULong64_t>& times2,
    // status of events in device 1
    std::vector<bool>& write1,
    // status of events in device 2
    std::vector<bool>& write2,
    // Ratio of device2 / device1 time units
    double ratio,
    // Threshold difference of deltas at which events are not synchronized
    double threshold,
    // Number of events that need to pass after the one to be written
    unsigned nbuffer);

}

#endif  // UTILS_H

