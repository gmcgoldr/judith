#include <iostream>
#include <stdio.h>
#include <stdexcept>
#include <string>
#include <vector>
#include <set>
#include <map>

#include <TApplication.h>

#include "options.h"
#include "storage/storagei.h"
#include "storage/storageo.h"
#include "mechanics/device.h"
#include "mechanics/mechparsers.h"
#include "processors/clustering.h"
#include "processors/tracking.h"
#include "processors/aligning.h"
#include "loopers/looper.h"
#include "loopers/loopprocess.h"
#include "loopers/loopaligncorr.h"
#include "loopers/looptransfers.h"
#include "loopers/loopaligntracks.h"

void printHelp() {
  printf("usage: judith <command> [<args>]\n");

  printf("\nArguments:\n");
  printf("  %2s %-15s %s\n", "-h", "--help", "Display this information");
  printf("  %2s %-15s %s\n", "-i", "--input", "Path to input file(s)");
  printf("  %2s %-15s %s\n", "-o", "--output", "Path to output file");
  printf("  %2s %-15s %s\n", "-s", "--settings", "Path to settings file (default: configs/settings.cfg)");
  printf("  %2s %-15s %s\n", "-r", "--results", "Path to results file");
  printf("  %2s %-15s %s\n", "-d", "--device", "Path to device configuration(s)");
  printf("  %2s %-15s %s\n", "-f", "--first", "Number of first event to process");
  printf("  %2s %-15s %s\n", "-n", "--events", "Process up to this many events past first");
  printf("  %2s %-15s %s\n", "-k", "--skip", "Skip this many events at each loop iteration");
  printf("  %2s %-15s %s\n", "", "--progress", "Display progress at this interval (0 is off)");
  printf("  %2s %-15s %s\n", "", "--draw", "Give visual feedback when availalbe (e.g. fits)");

  printf("\nCommands:\n");
  printf("  %-15s %s\n", "process", "Generate clusters and tracks from the given input");
  printf("  %-15s %s\n", "align-corr", "Align the sensors by plane correlations");
  printf("  %-15s %s\n", "align-tracks", "Align the sensors using track residuals");
  std::cout << std::endl;
}

void fillBranchMasks(
    const Options& options,
    std::set<std::string> hitBranchesOff,
    std::set<std::string> clusterBranchesOff,
    std::set<std::string> trackBranchesOff,
    std::set<std::string> eventInfoBranchesOff) {
  // Setup an enum to loop over the sets to fill
  enum Trees { HIT, CLUSTER, TRACK, EVENTINFO };

  // Loop over the sets to fill
  for (int tree = HIT; tree <= EVENTINFO; tree++) {
    std::string key;  // options key listing branches
    std::set<std::string>* branches;  // pointer to the set to fill
    switch (tree) {
    case HIT:
      // Look for options with this key
      key = "hit-branch-off";
      // And fill this set with what is found
      branches = &hitBranchesOff;
      break;
    case CLUSTER:
      key = "cluster-branch-off";
      branches = &clusterBranchesOff;
      break;
    case TRACK:
      key = "track-branch-off";
      branches = &trackBranchesOff;
      break;
    case EVENTINFO:
      key = "eventInfo-branch-off";
      branches = &eventInfoBranchesOff;
      break;
    }

    // Get the branch names to be turned off for the given key
    const Options::Values& values = options.getValues(key);
    for (Options::Values::const_iterator it = values.begin();
        it != values.end(); it++)
      // Add all values for this key to the set of branches to turn off
      branches->insert(*it);
  }
}

void generateDevices(const Options& options, Mechanics::Devices& devices) {
  // The values are paths to files that can be parsed into devices
  const Options::Values& values = options.getValues("device");
  for (Options::Values::const_iterator it = values.begin();
      it != values.end(); ++it)
    // Parse the file path at the given value, and add the resulting device
    // to thd `devices` object which manages its memory
    devices.addDevice(Mechanics::parseDevice(*it));
}

void maskPlanes(const Options& options, Mechanics::Devices& devices) {
  const Options::Values& values = options.getValues("mask-plane");
  // Iterate two at a time, the first value is the device name, second is the
  // plane index
  for (size_t ival = 0; ival < values.size(); ival += 2) {
    const std::string& deviceName = values[ival];
    const int nplane = strToInt(values[ival+1]);
    devices[deviceName].maskSensor(nplane);
  }
}

void configureLooper(const Options& options, Loopers::Looper& looper) {
  // Configure a base `Looper` object from standard options
  if (options.hasArg("first"))
    looper.m_start = strToInt(options.getValue("first"));
  if (options.hasArg("events"))
    looper.m_nprocess = strToInt(options.getValue("events"));
  if (options.hasArg("skip"))
    looper.m_nstep = strToInt(options.getValue("skip"));
  if (options.hasArg("progress"))
    looper.m_printInterval = strToInt(options.getValue("progress"));
  looper.m_draw = options.evalBoolArg("draw");
}

int main(int argc, const char** argv) {
  std::cout << "\nStarting Judith\n" << std::endl;

  TApplication app("App", 0, 0);

  Options options;
  options.defineShort('i', "input");
  options.defineShort('o', "output");
  options.defineShort('s', "settings");
  options.defineShort('r', "results");
  options.defineShort('d', "device");
  options.defineShort('f', "first");
  options.defineShort('n', "events");
  options.defineShort('k', "skip");

  // First command line options
  options.parseArgs(argc, argv);
  // Check if help is requested and stop execution if so
  if (options.hasArg("help")) {
    printHelp();
    return 0;
  }
  // Default settings path (overwritten if provided in command line arguments)
  options.addPair("settings", "configs/settings.cfg");
  // Parse settings file
  options.parseFile(options.getValue("settings"));
  
  // If no additional arguments are given, or the first is an argument, then
  // no command was provided
  if (argc < 2 || argv[1][0] == '-') {
    std::cerr << "ERROR: no command provided" << std::endl;
    printHelp();
    return -1;
  }

  // Get lists of branches to turn off from configuration
  std::set<std::string> hitBranchesOff;
  std::set<std::string> clusterBranchesOff;
  std::set<std::string> trackBranchesOff;
  std::set<std::string> eventInfoBranchesOff;
  fillBranchMasks(
      options,
      hitBranchesOff,
      clusterBranchesOff,
      trackBranchesOff,
      eventInfoBranchesOff);

  // Build the device(s) from the given files. Note: devices is a wrapper for
  // a vector of devices, with name mapping.
  Mechanics::Devices devices;
  generateDevices(options, devices);

  // Remove masked planes from the devices
  maskPlanes(options, devices);

  const std::string command = argv[1];

  /////////////////////////////////////////////////////////////////////////////
  // Processing

  if (command == "process") {
    if (!options.hasArg("input") || !options.hasArg("output")) {
      std::cerr << "ERROR: process requires and input and output arguments"
          << std::endl;
      return -1;
    }

    if (devices.getNumDevices() != 1) {
      std::cerr << "ERROR: exactly one device accepted when processing" << std::endl;
      return -1;
    }

    // List of TTree branches to turn off on read back
    std::set<std::string> inHitsOff;
    inHitsOff.insert("PosX");
    inHitsOff.insert("PosY");
    inHitsOff.insert("PosZ");

    Storage::StorageI input(
        options.getValue("input"),
        // Don't read back clusters and tracks since they are not used
        Storage::StorageIO::CLUSTERS | Storage::StorageIO::TRACKS,
        // Turn off reading masked sensors
        &devices[0].getSensorMask(),
        // Don't read hit global positions since they will be re-generated
        &inHitsOff);

    int outTreeMask = 0;

    if (!options.evalBoolArg("process-clusters"))
      outTreeMask |= Storage::StorageIO::CLUSTERS;
    if (!options.evalBoolArg("process-tracks"))
      outTreeMask |= Storage::StorageIO::TRACKS;

    // Match the active branches from the input to those in the output
    if (input.isHitsBranchOff("Value")) {
      hitBranchesOff.insert("Value");
      clusterBranchesOff.insert("Value");
    }
    if (input.isHitsBranchOff("Timing")) {
      hitBranchesOff.insert("Timing");
      clusterBranchesOff.insert("Timing");
    }

    Storage::StorageO output(
        options.getValue("output"),
        input.getNumPlanes(),
        outTreeMask,
        &hitBranchesOff,
        &clusterBranchesOff,
        &trackBranchesOff,
        &eventInfoBranchesOff);

    // Build an alignment object from the device
    Processors::Aligning aligning(devices[0]);

    // Build a clustering object from the options
    Processors::Clustering clustering;
    if (options.hasArg("process-clusters-nrows"))
      clustering.m_maxRows = strToInt(options.getValue("process-clusters-nrows"));
    if (options.hasArg("process-clusters-ncols"))
      clustering.m_maxRows = strToInt(options.getValue("process-clusters-ncols"));

    // Build a tracking object from the options
    Processors::Tracking tracking(devices[0].getNumSensors());
    if (options.hasArg("process-tracks-radius"))
      tracking.m_radius = strToFloat(
          options.getValue("process-tracks-radius"));
    if (options.hasArg("process-tracks-minclusters"))
      tracking.m_minClusters = strToInt(
          options.getValue("process-tracks-minclusters"));
    // If transfers were requested, then do a pre-run to get transfer scales
    if (options.evalBoolArg("process-tracks-transfers")) {
      // Setup the pre-looper to measure transfers for only the reference
      Loopers::LoopTransfers preLooper(input, devices[0]);
      preLooper.addProcessor(clustering);
      preLooper.addProcessor(aligning);
      configureLooper(options, preLooper);
      // Run it
      preLooper.loop();
      preLooper.finalize();
      // Set tracking transfer scales based on residual distributions
      preLooper.apply(tracking);
    }

    // Prepare a processing looper to loop the input and write to the output
    Loopers::LoopProcess looper(input, output);

    // Note the order here: first processor clusters hits, next performs 
    // alignment (computes global values for the clusters), and then tracking
    // can be performed (using clusters' global values)

    // If a clustering is requested, give it a clustering processor
    if (options.evalBoolArg("process-clusters"))
      looper.addProcessor(clustering);

    // Give it the aligning object to compute and store global positions
    looper.addProcessor(aligning);

    // Likewise for tracking
    if (options.evalBoolArg("process-tracks"))
      looper.addProcessor(tracking);

    // Apply generic looping options to the looper
    configureLooper(options, looper);

    // Run the looper
    looper.loop();
    looper.finalize();
  }

  /////////////////////////////////////////////////////////////////////////////
  // Correlation alignment

  else if (command == "align-corr") {
    const Options::Values& inputNames = options.getValues("input");
    if (inputNames.size() < 1) {
      std::cerr << "ERROR: need at least 1 input" << std::endl;
      return -1;
    }
    if (inputNames.size() != devices.getNumDevices()) {
      std::cerr << "ERROR: need one device for each input" << std::endl;
      return -1;
    }

    // Build the input storages for the devices to align
    std::vector<Storage::StorageI*> inputs;
    for (size_t i = 0; i < inputNames.size(); i++) {
      Storage::StorageI* input = new Storage::StorageI(
          inputNames[i],
          // Don't read tracks or clusters, new clusters will be made
          Storage::StorageIO::TRACKS | Storage::StorageIO::CLUSTERS,
          &devices[i].getSensorMask());
      inputs.push_back(input);
    }

    // Prepare a processing looper with the devices which it will align
    Loopers::LoopAlignCorr looper(inputs, devices.getVector());

    Processors::Clustering clustering;
    if (options.hasArg("process-clusters-nrows"))
      clustering.m_maxRows = strToInt(options.getValue("process-clusters-nrows"));
    if (options.hasArg("process-clusters-ncols"))
      clustering.m_maxRows = strToInt(options.getValue("process-clusters-ncols"));
    looper.addProcessor(clustering);

    // Alignment also needs to compute the spatial positions of the clusters
    Processors::Aligning aligning(devices.getVector());
    looper.addProcessor(aligning);

    // Apply generic looping options to the looper
    configureLooper(options, looper);

    // Run the looper
    looper.loop();
    looper.finalize();

    // Write out the alignment to file
    for (size_t i = 0; i < devices.getNumDevices(); i++)
      Mechanics::writeAlignment(devices[i]);

    // Clear the inputs from memory
    for (std::vector<Storage::StorageI*>::iterator it = inputs.begin();
        it != inputs.end(); ++it)
      delete *it;
  }

  /////////////////////////////////////////////////////////////////////////////
  // Tack alignment

  else if (command == "align-tracks") {
    const Options::Values& inputNames = options.getValues("input");
    if (inputNames.size() < 1) {
      std::cerr << "ERROR: need at least 1 input" << std::endl;
      return -1;
    }
    if (inputNames.size() > 2) {
      std::cerr << "ERROR: uses 2 devices at most" << std::endl;
      return -1;
    }
    if (inputNames.size() != devices.getNumDevices()) {
      std::cerr << "ERROR: need one device for each input" << std::endl;
      return -1;
    }

    // Build the input storages for the devices to align
    std::vector<Storage::StorageI*> inputs;
    for (size_t i = 0; i < inputNames.size(); i++) {
      Storage::StorageI* input = new Storage::StorageI(
          inputNames[i],  // ith input file
          Storage::StorageIO::TRACKS | Storage::StorageIO::CLUSTERS,
          &devices[i].getSensorMask());
      inputs.push_back(input);
    }

    // Prepare a processing looper with the devices which it will align
    Loopers::LoopAlignTracks looper(inputs, devices.getVector());

    // Alignment needs clusters
    Processors::Clustering clustering;
    if (options.hasArg("process-clusters-nrows"))
      clustering.m_maxRows = strToInt(options.getValue("process-clusters-nrows"));
    if (options.hasArg("process-clusters-ncols"))
      clustering.m_maxRows = strToInt(options.getValue("process-clusters-ncols"));
    looper.addProcessor(clustering);

    // Need to align clusters to global coordinates
    Processors::Aligning aligning(devices.getVector());
    looper.addProcessor(aligning);

    // Note that the looper has its own tracking processor, since it has to
    // know how to handle the reference / dut devices differently

    // Configure the alignment's tracking processor
    if (options.hasArg("process-tracks-radius"))
      looper.m_tracking.m_radius = strToFloat(
          options.getValue("process-tracks-radius"));
    if (options.hasArg("process-tracks-minclusters"))
      looper.m_tracking.m_minClusters = strToInt(
          options.getValue("process-tracks-minclusters"));
    // If transfers were requested, then do a pre-run to get transfer scales
    if (options.evalBoolArg("process-tracks-transfers")) {
      // Setup the pre-looper to measure transfers for only the reference
      Loopers::LoopTransfers preLooper(*inputs[0], devices[0]);
      preLooper.addProcessor(clustering);
      preLooper.addProcessor(aligning);
      configureLooper(options, preLooper);
      // Run it
      preLooper.loop();
      preLooper.finalize();
      // Set tracking transfer scales based on residual distributions
      preLooper.apply(looper.m_tracking);
    }

    // Parse track alignment specific options
    if (options.hasArg("align-tracks-translation-scale"))
      looper.m_translationScale = strToFloat(
          options.getValue("align-tracks-translation-scale"));
    if (options.hasArg("align-tracks-translation-limit"))
      looper.m_translationLimit = strToFloat(
          options.getValue("align-tracks-translation-limit"));
    if (options.hasArg("align-tracks-rotation-scale"))
      looper.m_rotationScale = strToFloat(
          options.getValue("align-tracks-rotation-scale"));
    if (options.hasArg("align-tracks-rotation-limit"))
      looper.m_rotationLimit = strToFloat(
          options.getValue("align-tracks-rotation-limit"));
    if (options.hasArg("align-tracks-tolerance"))
      looper.m_tolerance = strToFloat(
          options.getValue("align-tracks-tolerance"));
    looper.m_inPlane = options.evalBoolArg("align-tracks-inplane");

    // Apply generic looping options to the looper
    configureLooper(options, looper);

    // Run the looper
    looper.loop();
    looper.finalize();

    // Write out the alignment to file
    for (size_t i = 0; i < devices.getNumDevices(); i++)
      Mechanics::writeAlignment(devices[i]);

    // Clear the inputs from memory
    for (std::vector<Storage::StorageI*>::iterator it = inputs.begin();
        it != inputs.end(); ++it)
      delete *it;
  }

  else {
    std::cerr << "ERROR: unknown command " << command << std::endl;
    printHelp();
    return -1;
  }

  std::cout << "\nEnding Judith\n" << std::endl;

  return 0;
}
