// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#ifndef _VoxelPlugin_Voxel_3D_HXX_
#define _VoxelPlugin_Voxel_3D_HXX_

#include "VoxelPlugin_Defs.hxx"

#include "VoxelPlugin_Grid.hxx"

#include <SMESH_Algo.hxx>

#include <vector>

class VoxelPlugin_Classifier;
class VoxelPlugin_Hypothesis;

/*!
 * \brief "Voxel_3D": one HEXA8 per Cartesian cell, bound to the smallest solid containing
 *        the cell center. Meshes all solids of the main shape at once.
 */
class VOXELPLUGIN_EXPORT VoxelPlugin_Voxel_3D : public SMESH_3D_Algo
{
public:
  VoxelPlugin_Voxel_3D(int hypId, SMESH_Gen* gen);

  bool CheckHypothesis(SMESH_Mesh& aMesh, const TopoDS_Shape& aShape,
                       Hypothesis_Status& aStatus) override;

  bool Compute(SMESH_Mesh& aMesh, const TopoDS_Shape& aShape) override;

  bool Evaluate(SMESH_Mesh& aMesh, const TopoDS_Shape& aShape,
                MapShapeNbElems& aResMap) override;

  void SetEventListener(SMESH_subMesh* subMesh) override;

private:
  void removeMeshOnShape(SMESH_Mesh& aMesh, const TopoDS_Shape& aShape);

  //! Checks shared by Compute and Evaluate, then cell owners of the grid covering all solids
  //! of the main shape; false with error() set on failure
  bool prepare(SMESH_Mesh& aMesh, VoxelPlugin_Classifier& classifier, VoxelPlugin_Grid& grid,
               std::vector<int>& owners, std::size_t* nbUnknown = nullptr);

  const VoxelPlugin_Hypothesis* _hyp;
};

#endif
