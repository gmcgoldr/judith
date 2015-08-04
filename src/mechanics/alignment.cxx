#include <iostream>
#include <math.h>

#include "mechanics/alignment.h"

namespace Mechanics {

Alignment::Alignment() {
  // Initialize the alignment values to 0
  for (unsigned i = 0; i < 6; i++)
    m_alignment[i] = 0;
  // Initialize the rotation matrix to a diagonal one
  for (unsigned i = 0; i < 3; i++)
    for (unsigned j = 0; j < 3; j++)
      m_matrix[i][j] = (i==j) ? 1 : 0;
}

void Alignment::calculate() {
  // Shorter names make it clearer
  const double rx = m_alignment[ROTX];
  const double ry = m_alignment[ROTY];
  const double rz = m_alignment[ROTZ];

  // Rotation matrix Rz * Ry * Rx, note the order matters
  m_matrix[0][0] = cos(ry) * cos(rz);
  m_matrix[0][1] = -cos(rx) * sin(rz) + sin(rx) * sin(ry) * cos(rz);
  m_matrix[0][2] = sin(rx) * sin(rz) + cos(rx) * sin(ry) * cos(rz);
  m_matrix[1][0] = cos(ry) * sin(rz);
  m_matrix[1][1] = cos(rx) * cos(rz) + sin(rx) * sin(ry) * sin(rz);
  m_matrix[1][2] = -sin(rx) * cos(rz) + cos(rx) * sin(ry) * sin(rz);
  m_matrix[2][0] = -sin(ry);
  m_matrix[2][1] = sin(rx) * cos(ry);
  m_matrix[2][2] = cos(rx) * cos(ry);
}

void Alignment::rotate(double& x, double& y, double& z) const {
  double values[3] = {x, y, z};
  double buffer[3] = { 0 };

  // Compute the rotation into the buffer
  for (unsigned i = 0; i < 3; i++)
    for (unsigned j = 0; j < 3; j++)
      buffer[i] += values[j] * m_matrix[i][j];

  x = buffer[0];
  y = buffer[1];
  z = buffer[2];
}

void Alignment::unrotate(double& x, double& y, double& z) const {
  double values[3] = {x, y, z};
  double buffer[3] = { 0 };

  for (unsigned i = 0; i < 3; i++)
    for (unsigned j = 0; j < 3; j++)
      // Inverse rotation matrix is just its transpose
      buffer[i] += values[j] * m_matrix[j][i];

  x = buffer[0];
  y = buffer[1];
  z = buffer[2];
}

void Alignment::transform(double& x, double& y, double& z) const {
  double values[3] = {x, y, z};
  double buffer[3] = { 0 };

    for (unsigned i = 0; i < 3; i++)
      for (unsigned j = 0; j < 3; j++)
        buffer[i] += values[j] * m_matrix[i][j];

  x = buffer[0] + m_alignment[OFFX];
  y = buffer[1] + m_alignment[OFFY];
  z = buffer[2] + m_alignment[OFFZ];
}

void Alignment::untransform(double& x, double& y, double& z) const {
  double values[3] = {
      x - m_alignment[OFFX], 
      y - m_alignment[OFFY], 
      z - m_alignment[OFFZ]
  };
  double buffer[3] = { 0 };

  for (unsigned i = 0; i < 3; i++)
    for (unsigned j = 0; j < 3; j++)
      buffer[i] += values[j] * m_matrix[j][i];

  x = buffer[0];
  y = buffer[1];
  z = buffer[2];
}

// NOTE: all these methods need to call the `calculate` method to update the
// rotation matrix

void Alignment::setAlignment(const double* values) {
  for (unsigned i = 0; i < 6; i++)
    m_alignment[i] = values[i];
  calculate();
}

void Alignment::setAlignment(AlignAxis axis, double value) {
  m_alignment[axis] = value;
  calculate();
}

void Alignment::setOffX(double value) { m_alignment[OFFX] = value; calculate(); }
void Alignment::setOffY(double value) { m_alignment[OFFY] = value; calculate(); }
void Alignment::setOffZ(double value) { m_alignment[OFFZ] = value; calculate(); }
void Alignment::setRotX(double value) { m_alignment[ROTX] = value; calculate(); }
void Alignment::setRotY(double value) { m_alignment[ROTY] = value; calculate(); }
void Alignment::setRotZ(double value) { m_alignment[ROTZ] = value; calculate(); }

}

