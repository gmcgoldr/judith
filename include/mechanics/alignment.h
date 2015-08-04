#ifndef ALIGNMENT_H
#define ALIGNMENT_H

namespace Mechanics {

/**
  * Spatial alignment information, and transformation methods. This is meant to
  * be used as a base class for an object which is transformable. Keeps track of
  * its position in space, and can transform from a local coordinate system to
  * the global one within which the object is placed.
  *
  * @author Garrin McGoldrick (garrin.mcgoldrick@cern.ch)
  */
class Alignment {
public:
  /** Index positions within alignment array */
  enum AlignAxis {
    OFFX,  // Offset along x
    OFFY,
    OFFZ,
    ROTX,  // Rotation about x
    ROTY,
    ROTZ,
  };

private:
  /** Calculate the rotation matrix */
  void calculate();

protected:
  /** The alignment information (offsets and rotations) */
  double m_alignment[6];
  /** The rotation matrix */
  double m_matrix[3][3];

public:
  Alignment();
  virtual ~Alignment() {}

  void transform(double& x, double& y, double& z) const;
  void untransform(double& x, double& y, double& z) const;
  void rotate(double& x, double& y, double& z) const;
  void unrotate(double& x, double& y, double& z) const;

  /** Set the alignment from an array of 6 values whose indices are specified
    * by the `AlignAxis` enum */
  void setAlignment(const double* values);
  /** Set the value of a particular alignment */
  void setAlignment(AlignAxis axis, double value);
  /** Set the vaue of the alignment offset in x */
  void setOffX(double value);
  void setOffY(double value);
  void setOffZ(double value);
  /** Set the vaue of the alignment rotation about x */
  void setRotX(double value);
  void setRotY(double value);
  void setRotZ(double value);

  /** Get an alignment value */
  inline double getAlignment(AlignAxis axis) const { return m_alignment[axis]; }
  /** Get the value of the aligment offset in x */
  inline double getOffX() const { return m_alignment[OFFX]; }
  inline double getOffY() const { return m_alignment[OFFY]; }
  inline double getOffZ() const { return m_alignment[OFFZ]; }
  /** Get the value of the aligment rotation about x */
  inline double getRotX() const { return m_alignment[ROTX]; }
  inline double getRotY() const { return m_alignment[ROTY]; }
  inline double getRotZ() const { return m_alignment[ROTZ]; }
};

}

#endif  // ALIGNMENT_H

