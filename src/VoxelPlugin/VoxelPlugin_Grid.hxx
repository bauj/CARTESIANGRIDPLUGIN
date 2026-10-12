// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#ifndef _VoxelPlugin_Grid_HXX_
#define _VoxelPlugin_Grid_HXX_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <vector>

/*!
 * \brief Cartesian grid given by its node coordinates along X, Y and Z.
 *        Cell (i,j,k) spans [coords[0][i], coords[0][i+1]] x ... ; i varies fastest.
 */
struct VoxelPlugin_Grid
{
  std::vector<double> coords[3]; //!< sorted node coordinates per axis, size = nb cells + 1

  int NbCells(int axis) const { return std::max(0, (int)coords[axis].size() - 1); }

  std::size_t NbCells() const
  {
    return (std::size_t)NbCells(0) * (std::size_t)NbCells(1) * (std::size_t)NbCells(2);
  }

  std::size_t CellIndex(int i, int j, int k) const
  {
    return i + (std::size_t)NbCells(0) * (j + (std::size_t)NbCells(1) * k);
  }

  std::size_t NodeIndex(int i, int j, int k) const
  {
    return i + (std::size_t)(NbCells(0) + 1) * (j + (std::size_t)(NbCells(1) + 1) * k);
  }

  double Center(int axis, int i) const { return 0.5 * (coords[axis][i] + coords[axis][i + 1]); }

  //! n+1 equally spaced coordinates from min to max, last one exactly max
  static std::vector<double> UniformAxis(double min, double max, int n)
  {
    std::vector<double> c(n + 1);
    for (int i = 0; i < n; ++i)
      c[i] = min + (max - min) * i / n;
    c[n] = max;
    return c;
  }

  //! number of cells giving a spacing closest to the wished step: max(1, round(extent/step))
  static int NbCellsForStep(double extent, double step)
  {
    if (step <= 0.)
      return 1;
    return std::max(1, (int)std::lround(extent / step));
  }
};

#endif
