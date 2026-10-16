// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#include "CartesianGridPlugin_Hypothesis.hxx"

#include <StdMeshers_CartesianParameters3D.hxx>
#include <Utils_SALOME_Exception.hxx>

#include <Standard_Failure.hxx>

#include <sstream>

namespace
{
  const int theVersion = 2; //!< persistence format version (1: modes NB_CELLS and STEP only)
  const char* theAxisNames[3] = { "X", "Y", "Z" };

  //! reads a count in [0, theMax]; failbit otherwise
  bool readSize(std::istream& load, std::size_t& n, std::size_t theMax)
  {
    long long v = -1;
    if (!(load >> v) || v < 0 || v > (long long)theMax)
    {
      load.setstate(std::ios::failbit);
      return false;
    }
    n = (std::size_t)v;
    return true;
  }

  //! reads a count, then that many values; memory grows with the values actually read, so a
  //! corrupted count cannot allocate much before the stream fails
  bool readValues(std::istream& load, std::vector<double>& values, std::size_t theMax)
  {
    std::size_t n = 0;
    if (!readSize(load, n, theMax))
      return false;
    values.clear();
    double v;
    for (std::size_t i = 0; i < n && load >> v; ++i)
      values.push_back(v);
    return bool(load);
  }
}

CartesianGridPlugin_Hypothesis::CartesianGridPlugin_Hypothesis(int hypId, SMESH_Gen* gen)
  : SMESH_Hypothesis(hypId, gen), myKeepOutside(false)
{
  _name = GetHypType();
  _param_algo_dim = 3;
  for (int a = 0; a < 3; ++a)
  {
    myMode[a] = NB_CELLS;
    myNbCells[a] = 10;
    myStep[a] = 1.;
    myCoords[a] = { 0., 1. };
    myFunctions[a] = { "1" };
  }
}

void CartesianGridPlugin_Hypothesis::checkAxis(int axis)
{
  if (axis < 0 || axis > 2)
    throw SALOME_Exception("Axis must be 0 (X), 1 (Y) or 2 (Z)");
}

void CartesianGridPlugin_Hypothesis::SetNbCells(int axis, int nb)
{
  checkAxis(axis);
  if (nb < 1)
    throw SALOME_Exception("Number of cells must be at least 1");
  if (myMode[axis] != NB_CELLS || myNbCells[axis] != nb)
  {
    myMode[axis] = NB_CELLS;
    myNbCells[axis] = nb;
    NotifySubMeshesHypothesisModification();
  }
}

void CartesianGridPlugin_Hypothesis::SetStep(int axis, double step)
{
  checkAxis(axis);
  if (!(step > 0.))
    throw SALOME_Exception("Step must be positive");
  if (myMode[axis] != STEP || myStep[axis] != step)
  {
    myMode[axis] = STEP;
    myStep[axis] = step;
    NotifySubMeshesHypothesisModification();
  }
}

CartesianGridPlugin_Hypothesis::Mode CartesianGridPlugin_Hypothesis::GetMode(int axis) const
{
  checkAxis(axis);
  return Mode(myMode[axis]);
}

int CartesianGridPlugin_Hypothesis::GetNbCells(int axis) const
{
  checkAxis(axis);
  return myNbCells[axis];
}

double CartesianGridPlugin_Hypothesis::GetStep(int axis) const
{
  checkAxis(axis);
  return myStep[axis];
}

void CartesianGridPlugin_Hypothesis::SetCoordinates(int axis, const std::vector<double>& coords)
{
  checkAxis(axis);
  std::vector<double> c(coords);
  std::string error;
  if (!CartesianGridPlugin_Grid::SortAndCheckCoordinates(c, error))
    throw SALOME_Exception(error.c_str());
  if (myMode[axis] != COORDINATES || myCoords[axis] != c)
  {
    myMode[axis] = COORDINATES;
    myCoords[axis] = c;
    NotifySubMeshesHypothesisModification();
  }
}

const std::vector<double>& CartesianGridPlugin_Hypothesis::GetCoordinates(int axis) const
{
  checkAxis(axis);
  return myCoords[axis];
}

std::vector<double> CartesianGridPlugin_Hypothesis::SpacingCoordinates(double theMin, double theMax,
                                                               const std::vector<std::string>& theFunctions,
                                                               const std::vector<double>& thePoints,
                                                               int theAxis)
{
  checkAxis(theAxis);
  if (theFunctions.empty())
    throw SALOME_Exception("At least one spacing function is needed");
  if (thePoints.size() + 1 != theFunctions.size())
    throw SALOME_Exception("Number of internal points must be the number of functions minus one");
  for (double t : thePoints)
    if (!(t > 0. && t < 1.))
      throw SALOME_Exception("Internal points must be inside ]0, 1[");
  std::vector<std::string> functions(theFunctions); // ComputeCoordinates takes non-const refs
  std::vector<double> points(thePoints), coords;
  try
  {
    StdMeshers_CartesianParameters3D::ComputeCoordinates(theMin, theMax, functions, points, coords,
                                                         theAxisNames[theAxis]);
  }
  catch (const Standard_Failure&)
  {
    throw SALOME_Exception("Invalid spacing function");
  }
  if (coords.size() < 2)
    throw SALOME_Exception("Invalid spacing function");
  coords.front() = theMin;
  coords.back() = theMax;
  return coords;
}

void CartesianGridPlugin_Hypothesis::SetSpacing(int axis, const std::vector<std::string>& functions,
                                        const std::vector<double>& points)
{
  SpacingCoordinates(0., 1., functions, points, axis); // validation, throws
  if (myMode[axis] != SPACING || myFunctions[axis] != functions || myPoints[axis] != points)
  {
    myMode[axis] = SPACING;
    myFunctions[axis] = functions;
    myPoints[axis] = points;
    NotifySubMeshesHypothesisModification();
  }
}

const std::vector<std::string>& CartesianGridPlugin_Hypothesis::GetSpacingFunctions(int axis) const
{
  checkAxis(axis);
  return myFunctions[axis];
}

const std::vector<double>& CartesianGridPlugin_Hypothesis::GetSpacingPoints(int axis) const
{
  checkAxis(axis);
  return myPoints[axis];
}

std::vector<double> CartesianGridPlugin_Hypothesis::AxisCoordinates(int axis, double theMin, double theMax) const
{
  checkAxis(axis);
  switch (myMode[axis])
  {
  case COORDINATES:
    return myCoords[axis];
  case SPACING:
    return SpacingCoordinates(theMin, theMax, myFunctions[axis], myPoints[axis], axis);
  case STEP:
    return CartesianGridPlugin_Grid::UniformAxis(
      theMin, theMax, (int)CartesianGridPlugin_Grid::NbCellsForStep(theMax - theMin, myStep[axis]));
  default:
    return CartesianGridPlugin_Grid::UniformAxis(theMin, theMax, myNbCells[axis]);
  }
}

std::vector<CartesianGridPlugin_Hypothesis::CoverageGap>
CartesianGridPlugin_Hypothesis::CoverageGaps(const CartesianGridPlugin_Grid& theGrid,
                                             const double theMin[3], const double theMax[3]) const
{
  std::vector<CoverageGap> gaps;
  for (int a = 0; a < 3; ++a)
  {
    if (myMode[a] != COORDINATES || theGrid.coords[a].empty())
      continue;
    const double tol = 1e-9 * (theMax[a] - theMin[a]);
    const double first = theGrid.coords[a].front(), last = theGrid.coords[a].back();
    if (first > theMin[a] + tol || last < theMax[a] - tol)
      gaps.push_back({ a, first, last, theMin[a], theMax[a] });
  }
  return gaps;
}

std::string CartesianGridPlugin_Hypothesis::CoverageWarning(const CartesianGridPlugin_Grid& theGrid,
                                                    const double theMin[3], const double theMax[3]) const
{
  std::ostringstream w;
  for (const CoverageGap& g : CoverageGaps(theGrid, theMin, theMax))
    w << theAxisNames[g.axis] << " planes cover [" << g.first << ", " << g.last << "], solids span ["
      << g.min << ", " << g.max << "]: cells outside are not meshed. ";
  return w.str();
}

void CartesianGridPlugin_Hypothesis::SetKeepOutsideCells(bool keep)
{
  if (myKeepOutside != keep)
  {
    myKeepOutside = keep;
    NotifySubMeshesHypothesisModification();
  }
}

void CartesianGridPlugin_Hypothesis::NbCellsOfGrid(const double theMin[3], const double theMax[3],
                                           long long theNb[3]) const
{
  for (int a = 0; a < 3; ++a)
    switch (myMode[a])
    {
    case STEP:
      theNb[a] = CartesianGridPlugin_Grid::NbCellsForStep(theMax[a] - theMin[a], myStep[a]);
      break;
    case NB_CELLS:
      theNb[a] = myNbCells[a];
      break;
    default: // COORDINATES, SPACING: bounded number of coordinates
      theNb[a] = (long long)AxisCoordinates(a, theMin[a], theMax[a]).size() - 1;
    }
}

CartesianGridPlugin_Grid CartesianGridPlugin_Hypothesis::MakeGrid(const double theMin[3], const double theMax[3]) const
{
  CartesianGridPlugin_Grid grid;
  for (int a = 0; a < 3; ++a)
    grid.coords[a] = AxisCoordinates(a, theMin[a], theMax[a]);
  return grid;
}

std::ostream& CartesianGridPlugin_Hypothesis::SaveTo(std::ostream& save)
{
  const std::streamsize precision = save.precision(17);
  save << theVersion;
  for (int a = 0; a < 3; ++a)
  {
    save << ' ' << myMode[a] << ' ' << myNbCells[a] << ' ' << myStep[a];
    save << ' ' << myCoords[a].size();
    for (double c : myCoords[a])
      save << ' ' << c;
    save << ' ' << myFunctions[a].size();
    for (const std::string& f : myFunctions[a])
      save << ' ' << f.size() << ' ' << f; // length prefix: a function may contain spaces
    save << ' ' << myPoints[a].size();
    for (double t : myPoints[a])
      save << ' ' << t;
  }
  save << ' ' << int(myKeepOutside);
  save.precision(precision);
  return save;
}

std::istream& CartesianGridPlugin_Hypothesis::LoadFrom(std::istream& load)
{
  int version = 0;
  if (!(load >> version) || (version != 1 && version != 2))
  {
    load.setstate(std::ios::failbit);
    return load;
  }
  int mode[3], nb[3], keep = 0;
  double step[3];
  std::vector<double> coords[3], points[3];
  std::vector<std::string> functions[3];
  for (int a = 0; a < 3; ++a)
  {
    coords[a] = myCoords[a];
    functions[a] = myFunctions[a];
    points[a] = myPoints[a];
    if (!(load >> mode[a] >> nb[a] >> step[a]))
      return load;
    if (version < 2)
      continue;
    if (!readValues(load, coords[a], (std::size_t)CartesianGridPlugin_Grid::MaxNbCellsPerAxis + 1))
      return load;
    std::size_t n = 0;
    if (!readSize(load, n, 10000))
      return load;
    functions[a].resize(n);
    for (std::string& f : functions[a])
    {
      std::size_t len = 0;
      if (!readSize(load, len, 100000) || load.get() != ' ')
      {
        load.setstate(std::ios::failbit);
        return load;
      }
      f.resize(len);
      if (len > 0 && !load.read(&f[0], (std::streamsize)len))
        return load;
    }
    if (!readValues(load, points[a], 10000))
      return load;
  }
  load >> keep;
  if (!load)
    return load;
  for (int a = 0; a < 3; ++a)
  {
    std::vector<double> sorted(coords[a]);
    std::string error;
    if (mode[a] < NB_CELLS || mode[a] > SPACING || nb[a] < 1 || !(step[a] > 0.) ||
        !CartesianGridPlugin_Grid::SortAndCheckCoordinates(sorted, error) || sorted != coords[a] ||
        functions[a].empty() || points[a].size() + 1 != functions[a].size())
    {
      load.setstate(std::ios::failbit);
      return load;
    }
  }
  for (int a = 0; a < 3; ++a)
  {
    myMode[a] = mode[a];
    myNbCells[a] = nb[a];
    myStep[a] = step[a];
    myCoords[a] = coords[a];
    myFunctions[a] = functions[a];
    myPoints[a] = points[a];
  }
  myKeepOutside = keep != 0;
  return load;
}
