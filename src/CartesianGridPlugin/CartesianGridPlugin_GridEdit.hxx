// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#ifndef _CartesianGridPlugin_GridEdit_HXX_
#define _CartesianGridPlugin_GridEdit_HXX_

#include <algorithm>
#include <cmath>
#include <string>
#include <vector>

/*!
 * \brief Editing operations of the GUI on the grid modes Coordinates and Spacing.
 *        Spacing: functions f[i] on ranges [RangeStart(i), RangeEnd(i)] of t in [0, 1],
 *        bounded by the internal points (one less than functions).
 */
namespace CartesianGridPlugin_GridEdit
{
  //! smallest distance between two internal points (as SMESH Body Fitting)
  const double theMinRange = 1e-3;

  //! inserts theCoords[theIndex] + theStep (after the last value when theIndex is out of
  //! range) keeping the values sorted; index of the new value, -1 if it is not finite or
  //! already present
  inline int InsertCoordinate(std::vector<double>& theCoords, int theIndex, double theStep)
  {
    if (theCoords.empty())
      return -1;
    if (theIndex < 0 || theIndex >= (int)theCoords.size())
      theIndex = (int)theCoords.size() - 1;
    const double v = theCoords[theIndex] + theStep;
    if (!std::isfinite(v))
      return -1;
    const double tol = 1e-12 * std::max(1., std::abs(theCoords.back() - theCoords.front()));
    for (double c : theCoords)
      if (std::abs(c - v) <= tol)
        return -1;
    const auto pos = std::upper_bound(theCoords.begin(), theCoords.end(), v);
    const int index = (int)(pos - theCoords.begin());
    theCoords.insert(pos, v);
    return index;
  }

  //! removes theCoords[theIndex]; false if it is out of range or only 2 values remain
  inline bool DeleteCoordinate(std::vector<double>& theCoords, int theIndex)
  {
    if (theIndex < 0 || theIndex >= (int)theCoords.size() || theCoords.size() <= 2)
      return false;
    theCoords.erase(theCoords.begin() + theIndex);
    return true;
  }

  inline double RangeStart(const std::vector<double>& thePoints, int theIndex)
  {
    return theIndex == 0 ? 0. : thePoints[theIndex - 1];
  }

  inline double RangeEnd(const std::vector<double>& thePoints, int theIndex)
  {
    return theIndex == (int)thePoints.size() ? 1. : thePoints[theIndex];
  }

  //! splits range theIndex at its middle, both halves keeping its function; index of the
  //! second half, -1 if the index is out of range or the halves would be too small
  inline int SplitRange(std::vector<std::string>& theFunctions, std::vector<double>& thePoints,
                        int theIndex)
  {
    if (theIndex < 0 || theIndex >= (int)theFunctions.size())
      return -1;
    const double t0 = RangeStart(thePoints, theIndex), t1 = RangeEnd(thePoints, theIndex);
    if (t1 - t0 < 2 * theMinRange)
      return -1;
    thePoints.insert(thePoints.begin() + theIndex, 0.5 * (t0 + t1));
    theFunctions.insert(theFunctions.begin() + theIndex + 1, theFunctions[theIndex]);
    return theIndex + 1;
  }

  //! merges range theIndex with the next one (the previous one for the last range), keeping
  //! the function of theIndex; false if the index is out of range or only one range remains
  inline bool MergeRange(std::vector<std::string>& theFunctions, std::vector<double>& thePoints,
                         int theIndex)
  {
    const int n = (int)theFunctions.size();
    if (theIndex < 0 || theIndex >= n || n == 1)
      return false;
    if (theIndex < n - 1)
    {
      thePoints.erase(thePoints.begin() + theIndex);
      theFunctions.erase(theFunctions.begin() + theIndex + 1);
    }
    else
    {
      thePoints.erase(thePoints.begin() + theIndex - 1);
      theFunctions.erase(theFunctions.begin() + theIndex - 1);
    }
    return true;
  }

  //! moves the end of range theIndex (not the last one) to theT, at least theMinRange inside
  //! its neighbour bounds; false with theError otherwise
  inline bool SetRangeEnd(std::vector<double>& thePoints, int theIndex, double theT,
                          std::string& theError)
  {
    if (theIndex < 0 || theIndex >= (int)thePoints.size())
    {
      theError = "The last range always ends at 1";
      return false;
    }
    const double lo = RangeStart(thePoints, theIndex), hi = RangeEnd(thePoints, theIndex + 1);
    if (!(theT >= lo + theMinRange && theT <= hi - theMinRange))
    {
      theError = "The end of a range must lie between the neighbouring bounds";
      return false;
    }
    thePoints[theIndex] = theT;
    return true;
  }
}

#endif
