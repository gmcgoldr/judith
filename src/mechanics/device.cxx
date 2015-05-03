#include <iostream>
#include <cassert>
#include <vector>
#include <string>
#include <cstdio>

#include <Rtypes.h>

#include "mechanics/alignment.h"
#include "mechanics/sensor.h"
#include "mechanics/device.h"

namespace Mechanics {

Device::Device(size_t numSensors) :
    Alignment(),
    m_numSensors(numSensors),
    m_sensors(m_numSensors),
    m_name(),
    m_clockRate(0),
    m_readOutWindow(0),
    m_spaceUnit(),
    m_timeUnit(),
    m_timeStart(0),
    m_timeEnd(0) {
  // Generate the required number of sensors
  for (std::vector<Sensor*>::iterator it = m_sensors.begin();
      it != m_sensors.end(); ++it) {
    *it = new Sensor();
    // Associate those sensors to this device
    (*it)->m_device = this;
  }
}

Device::~Device() {
  for (std::vector<Sensor*>::iterator it = m_sensors.begin();
      it != m_sensors.end(); ++it)
    delete *it;
}

void Device::print() const {
  std::printf(
      "\n---\nDevice: %s\n---\n"
      "  clock rate:      %8.2e\n"
      "  read out window: %d\n"
      "  sensors:         %ld\n",
      m_name.c_str(),
      m_clockRate,
      m_readOutWindow,
      m_numSensors);

  for (size_t i = 0; i < getNumSensors(); i++)
    m_sensors[i]->print();

  std::cout << std::flush;
}

double Device::tsToTime(ULong64_t timeStamp) const {
  return (timeStamp-m_timeStart) / (double)m_clockRate;
}

void Device::pixelToSpace(
    unsigned col,
    unsigned row,
    unsigned nsensor,
    double& x,
    double& y,
    double& z) const {
  // pixelToSpace of the sensor will automatically call this device's transform
  // to go to global coodrinates, so just call that method.
  m_sensors[nsensor]->pixelToSpace(col, row, x, y, z);
}

}
