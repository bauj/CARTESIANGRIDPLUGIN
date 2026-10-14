// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#ifndef _VoxelPlugin_Hypothesis_HXX_
#define _VoxelPlugin_Hypothesis_HXX_

#include "VoxelPlugin_Defs.hxx"
#include "VoxelPlugin_Grid.hxx"

#include <SMESH_Hypothesis.hxx>

#include <string>
#include <vector>

/*!
 * \brief "VoxelGrid": per axis number of cells, wished step, node coordinates or spacing
 *        functions, and outside cells policy
 */
class VOXELPLUGIN_EXPORT VoxelPlugin_Hypothesis : public SMESH_Hypothesis
{
public:
  enum Mode { NB_CELLS = 0, STEP = 1, COORDINATES = 2, SPACING = 3 };

  VoxelPlugin_Hypothesis(int hypId, SMESH_Gen* gen);

  static const char* GetHypType() { return "VoxelGrid"; }

  void   SetNbCells(int axis, int nb);
  void   SetStep(int axis, double step);
  Mode   GetMode(int axis) const;
  int    GetNbCells(int axis) const;
  double GetStep(int axis) const;

  //! absolute node coordinates (mode COORDINATES); sorted; >= 2 finite, distinct values
  void SetCoordinates(int axis, const std::vector<double>& coords);
  const std::vector<double>& GetCoordinates(int axis) const;

  //! spacing functions f(t), t in [0,1] over the bounding box, and the internal points
  //! (strictly increasing in ]0,1[, one less than functions) splitting it (mode SPACING)
  void SetSpacing(int axis, const std::vector<std::string>& functions, const std::vector<double>& points);
  const std::vector<std::string>& GetSpacingFunctions(int axis) const;
  const std::vector<double>&      GetSpacingPoints(int axis) const;

  //! node coordinates of spacing functions over [theMin, theMax], first and last exactly
  //! theMin and theMax; throws SALOME_Exception on invalid functions or points
  static std::vector<double> SpacingCoordinates(double theMin, double theMax,
                                                const std::vector<std::string>& theFunctions,
                                                const std::vector<double>& thePoints, int theAxis);

  //! node coordinates along axis for any mode; modes NB_CELLS/STEP: check NbCellsOfGrid() first
  std::vector<double> AxisCoordinates(int axis, double theMin, double theMax) const;

  //! warning text for COORDINATES axes of theGrid not covering [theMin, theMax]; empty if none
  std::string CoverageWarning(const VoxelPlugin_Grid& theGrid, const double theMin[3],
                              const double theMax[3]) const;

  void SetKeepOutsideCells(bool keep);
  bool GetKeepOutsideCells() const { return myKeepOutside; }

  //! Number of cells per axis of the grid covering [theMin, theMax]
  void NbCellsOfGrid(const double theMin[3], const double theMax[3], long long theNb[3]) const;

  //! Grid covering [theMin, theMax] exactly; check NbCellsOfGrid() against
  //! VoxelPlugin_Grid::MaxNbCells first
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
  std::vector<double>      myCoords[3];
  std::vector<std::string> myFunctions[3];
  std::vector<double>      myPoints[3];
  bool   myKeepOutside;
};

#endif
