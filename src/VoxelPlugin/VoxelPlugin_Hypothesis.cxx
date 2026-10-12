// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#include "VoxelPlugin_Hypothesis.hxx"

#include <Utils_SALOME_Exception.hxx>

namespace
{
  const int theVersion = 1; //!< persistence format version
}

VoxelPlugin_Hypothesis::VoxelPlugin_Hypothesis(int hypId, SMESH_Gen* gen)
  : SMESH_Hypothesis(hypId, gen), myKeepOutside(false)
{
  _name = GetHypType();
  _param_algo_dim = 3;
  for (int a = 0; a < 3; ++a)
  {
    myMode[a] = NB_CELLS;
    myNbCells[a] = 10;
    myStep[a] = 1.;
  }
}

void VoxelPlugin_Hypothesis::checkAxis(int axis)
{
  if (axis < 0 || axis > 2)
    throw SALOME_Exception("Axis must be 0 (X), 1 (Y) or 2 (Z)");
}

void VoxelPlugin_Hypothesis::SetNbCells(int axis, int nb)
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

void VoxelPlugin_Hypothesis::SetStep(int axis, double step)
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

VoxelPlugin_Hypothesis::Mode VoxelPlugin_Hypothesis::GetMode(int axis) const
{
  checkAxis(axis);
  return Mode(myMode[axis]);
}

int VoxelPlugin_Hypothesis::GetNbCells(int axis) const
{
  checkAxis(axis);
  return myNbCells[axis];
}

double VoxelPlugin_Hypothesis::GetStep(int axis) const
{
  checkAxis(axis);
  return myStep[axis];
}

void VoxelPlugin_Hypothesis::SetKeepOutsideCells(bool keep)
{
  if (myKeepOutside != keep)
  {
    myKeepOutside = keep;
    NotifySubMeshesHypothesisModification();
  }
}

VoxelPlugin_Grid VoxelPlugin_Hypothesis::MakeGrid(const double theMin[3], const double theMax[3]) const
{
  VoxelPlugin_Grid grid;
  for (int a = 0; a < 3; ++a)
  {
    const double extent = theMax[a] - theMin[a];
    const int n = myMode[a] == STEP ? VoxelPlugin_Grid::NbCellsForStep(extent, myStep[a])
                                    : myNbCells[a];
    grid.coords[a] = VoxelPlugin_Grid::UniformAxis(theMin[a], theMax[a], n);
  }
  return grid;
}

std::ostream& VoxelPlugin_Hypothesis::SaveTo(std::ostream& save)
{
  const std::streamsize precision = save.precision(17);
  save << theVersion;
  for (int a = 0; a < 3; ++a)
    save << ' ' << myMode[a] << ' ' << myNbCells[a] << ' ' << myStep[a];
  save << ' ' << int(myKeepOutside);
  save.precision(precision);
  return save;
}

std::istream& VoxelPlugin_Hypothesis::LoadFrom(std::istream& load)
{
  int version = 0;
  if (!(load >> version) || version != theVersion)
  {
    load.setstate(std::ios::failbit);
    return load;
  }
  int mode[3], nb[3], keep = 0;
  double step[3];
  for (int a = 0; a < 3; ++a)
    load >> mode[a] >> nb[a] >> step[a];
  load >> keep;
  if (!load)
    return load;
  for (int a = 0; a < 3; ++a)
    if ((mode[a] != NB_CELLS && mode[a] != STEP) || nb[a] < 1 || !(step[a] > 0.))
    {
      load.setstate(std::ios::failbit);
      return load;
    }
  for (int a = 0; a < 3; ++a)
  {
    myMode[a] = mode[a];
    myNbCells[a] = nb[a];
    myStep[a] = step[a];
  }
  myKeepOutside = keep != 0;
  return load;
}
