// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#ifndef _VoxelPlugin_Classifier_HXX_
#define _VoxelPlugin_Classifier_HXX_

#include "VoxelPlugin_Defs.hxx"
#include "VoxelPlugin_Grid.hxx"

#include <Bnd_Box.hxx>
#include <TopoDS_Shape.hxx>

#include <vector>

/*!
 * \brief Finds the owner solid of each cell of a Cartesian grid.
 *
 * Owner = smallest-volume solid containing the cell center (ON counts as IN);
 * equal volumes: lower solid index wins. Pure OCCT, no SMESH dependency.
 */
class VOXELPLUGIN_EXPORT VoxelPlugin_Classifier
{
public:
  enum Status { OK, NO_SOLID, OPEN_SOLID };

  //! Collect the solids of theShape (located sub-shapes, TopExp::MapShapes order) and check them
  Status Init(const TopoDS_Shape& theShape);

  int NbSolids() const { return (int)mySolids.size(); }
  const TopoDS_Shape& Solid(int theIndex) const { return mySolids[theIndex]; }

  //! Index (MapShapes order, 0-based) of the first open solid when Init() returned OPEN_SOLID
  int BadSolid() const { return myBadSolid; }

  //! Owner solid index per cell (VoxelPlugin_Grid::CellIndex order), -1 outside all solids.
  //! theNbThreads <= 0: hardware concurrency. Rethrows the first exception of a worker.
  //! A center the OCCT classifier cannot locate in a solid (UNKNOWN state, even at a slightly
  //! shifted point) is taken as outside that solid and counted in theNbUnknown.
  std::vector<int> Classify(const VoxelPlugin_Grid& theGrid, int theNbThreads = 0,
                            std::size_t* theNbUnknown = nullptr) const;

  //! Tight bounding box of the solids of theShape (exact geometry, no tolerance enlargement);
  //! false if there is no solid
  static bool BoundingBox(const TopoDS_Shape& theShape, double theMin[3], double theMax[3]);

private:
  std::vector<TopoDS_Shape> mySolids;
  std::vector<double>       myVolumes;
  std::vector<Bnd_Box>      myBoxes;   //!< per solid, enlarged by shape tolerance
  std::vector<int>          myOrder;   //!< solid indices by increasing volume
  int                       myBadSolid = -1;
};

#endif
