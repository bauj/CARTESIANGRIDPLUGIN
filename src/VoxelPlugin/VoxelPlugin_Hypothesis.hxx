// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#ifndef _VoxelPlugin_Hypothesis_HXX_
#define _VoxelPlugin_Hypothesis_HXX_

#include "VoxelPlugin_Defs.hxx"
#include "VoxelPlugin_Grid.hxx"

#include <SMESH_Hypothesis.hxx>

/*!
 * \brief "VoxelGrid": per axis number of cells or wished step, and outside cells policy
 */
class VOXELPLUGIN_EXPORT VoxelPlugin_Hypothesis : public SMESH_Hypothesis
{
public:
  enum Mode { NB_CELLS = 0, STEP = 1 };

  VoxelPlugin_Hypothesis(int hypId, SMESH_Gen* gen);

  static const char* GetHypType() { return "VoxelGrid"; }

  void   SetNbCells(int axis, int nb);
  void   SetStep(int axis, double step);
  Mode   GetMode(int axis) const;
  int    GetNbCells(int axis) const;
  double GetStep(int axis) const;

  void SetKeepOutsideCells(bool keep);
  bool GetKeepOutsideCells() const { return myKeepOutside; }

  //! Grid covering [theMin, theMax] exactly
  VoxelPlugin_Grid MakeGrid(const double theMin[3], const double theMax[3]) const;

  std::ostream& SaveTo(std::ostream& save) override;
  std::istream& LoadFrom(std::istream& load) override;

  bool SetParametersByMesh(const SMESH_Mesh*, const TopoDS_Shape&) override { return false; }
  bool SetParametersByDefaults(const TDefaults&, const SMESH_Mesh* = 0) override { return false; }

private:
  static void checkAxis(int axis);

  int    myMode[3];
  int    myNbCells[3];
  double myStep[3];
  bool   myKeepOutside;
};

#endif
