#ifndef SENSOR_H
#define SENSOR_H   

#include <string>
#include <vector>

#include "mechanics/alignment.h"

namespace Mechanics {

class Device;

/**
  * Defines a sensor plane by describing its detector attributes and spatial
  * information. The sensor has some number of channels along x and y, a 
  * channel size, and a position in space.
  *
  * @author Garrin McGoldrick (garrin.mcgoldrick@cern.ch)
  */
class Sensor : public Alignment {
protected:  // accessed by Device
  /** Pointer to the device to which this sensor belongs (optional) */
  Device* m_device;
  /** No assignment */
  Sensor& operator=(const Sensor&);

public:
  /** Name propagates to plots and infomration about this sensor */
  std::string m_name;
  /** The number of channel rows (number of pixels along y) */
  unsigned m_nrows;
  /** The number of channel columns (number of pixels along x) */
  unsigned m_ncols;
  /** The spatial extent of a column along y (defines units) */
  double m_rowPitch;
  /** The spatial extent of a column along x (defines units) */
  double m_colPitch;
  /** The accumulated number of radiation lengths from the beam origin */
  double m_xox0;

  /** Vector of noisy hit rate for each pixel. Pixel order is given by 
    * `getPixelIndex` */
  std::vector<double> m_noiseProfile;
  /** Vector indicating which pixels are masked (true if masked). Pixel order
    * is given by `getPixelIndex`. */
  std::vector<bool> m_noiseMask;
  
  Sensor();
  Sensor(const Sensor& copy);
  ~Sensor() {}

  /** Print sensor information to cout */
  void print() const;

  /** Transform a pixel coordinate (in units of channels along x and y), to a
    * global coordinate. */
  void pixelToSpace(
      double col,
      double row,
      double& x,
      double& y,
      double& z) const;

  /** Compute the size of the pixel errors in global coordinates */
  void pixelErrToSpace(
      double colErr,
      double rowErr,
      double& xe,
      double& ye,
      double& ze) const;

  /** Transform a global coordinate to a pixel coordinate. Projects along the
    * z-axis, after applying the rotation. */
  void spaceToPixel(
      double x,
      double y,
      double z,
      double& col,
      double& row) const;

  /** Get the pointer to the parent device, or 0 if none exists */
  Device* getDevice() const { return m_device; }
  
  /** Transform a row and column number to a flattened index */
  inline unsigned getPixelIndex(unsigned row, unsigned col) const;
  /** Return the masked value of a pixel. throws and exception if no mask has
    * been provided */
  bool getPixelMask(unsigned row, unsigned col) const;
  /** Return the noise rate of a pixel. throws and exception if no mask has
    * been provided */
  bool getPixelNoise(unsigned row, unsigned col) const;

  /** Compute and return the normal unit vector. */
  void getNormal(double& x, double&y, double& z) const;
  /** Compute the pixel bounding box size with rotation */
  void getPixBox(double& x, double& y) const;
  /** Compute the sensor bounding box size with rotation */
  void getSensorBox(double& x, double& y) const;
  /** Compute the sensor bounding box edges with rotation. x1 < x2, y1 < y2 */
  void getSensorBox(double& x1,double& y1,double& x2, double& y2) const;

  // Allow Device objects to access the `m_device` member
  friend Device;
};

}

#endif  // SENSOR_H
