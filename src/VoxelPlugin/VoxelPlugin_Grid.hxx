// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#ifndef _VoxelPlugin_Grid_HXX_
#define _VoxelPlugin_Grid_HXX_

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <string>
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

  //! upper limit on the total number of cells of a grid (~76 GB of SMDS hexahedra)
  static constexpr long long MaxNbCells = 100000000LL;

  //! number of cells giving a spacing closest to the wished step: max(1, round(extent/step)),
  //! saturated to MaxNbCells + 1 so that a tiny step cannot overflow
  static long long NbCellsForStep(double extent, double step)
  {
    if (!(step > 0.))
      return 1;
    const double n = std::round(extent / step);
    if (!(n <= (double)MaxNbCells)) // also catches inf and nan
      return MaxNbCells + 1;
    return std::max(1LL, (long long)n);
  }

  //! Sorts theCoords; false with theError when there are fewer than 2 values, a non-finite
  //! value, or two values closer than 1e-12 x (last - first)
  static bool SortAndCheckCoordinates(std::vector<double>& theCoords, std::string& theError)
  {
    for (double c : theCoords)
      if (!std::isfinite(c))
      {
        theError = "Coordinates must be finite numbers";
        return false;
      }
    if (theCoords.size() < 2)
    {
      theError = "At least 2 coordinates are needed";
      return false;
    }
    std::sort(theCoords.begin(), theCoords.end());
    const double tol = 1e-12 * (theCoords.back() - theCoords.front());
    for (std::size_t i = 1; i < theCoords.size(); ++i)
      if (theCoords[i] - theCoords[i - 1] <= tol)
      {
        theError = "Duplicate coordinates";
        return false;
      }
    return true;
  }
};

#endif
