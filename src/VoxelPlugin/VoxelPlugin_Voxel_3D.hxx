// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#ifndef _VoxelPlugin_Voxel_3D_HXX_
#define _VoxelPlugin_Voxel_3D_HXX_

#include "VoxelPlugin_Defs.hxx"

#include "VoxelPlugin_Grid.hxx"

#include <SMESH_Algo.hxx>

#include <atomic>
#include <string>
#include <vector>

class VoxelPlugin_Classifier;
class VoxelPlugin_Hypothesis;

//! Zone of the Cartesian MED export: group name and, for the geometry source, the sub-shape
//! ids (main shape indices, as SMESHDS_Mesh::ShapeToIndex) of its solids
struct VoxelPlugin_ExportZone
{
  std::string      name;
  std::vector<int> ids;
};

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

  //! Classification of the cells up to 0.2, then creation of nodes and hexahedra
  double GetProgress() const override;

  //! Writes the grid of the hypothesis to a MED Cartesian file with one group per zone;
  //! fromMesh false: owner solid of each cell (no element created); true: hexahedra of the
  //! mesh volume groups named as the zones. Returns the summary; throws SALOME_Exception.
  std::string ExportCartesianMED(SMESH_Mesh& mesh, const std::string& file,
                                 const std::string& meshName, bool fromMesh,
                                 std::vector<VoxelPlugin_ExportZone> zones);
  //! Classification progress of the running export, 0..1
  double ExportProgress() const;
  //! Stops the running export (classification), which then throws "Export canceled"
  void CancelExport() { _computeCanceled = true; }

private:
  //! zone of each cell from the hexahedra of the mesh volume groups named as theZones
  void zonesFromMesh(SMESH_Mesh& aMesh, const VoxelPlugin_Grid& grid,
                     const std::vector<VoxelPlugin_ExportZone>& theZones, std::vector<int>& zoneOfCell);

  void removeMeshOnShape(SMESH_Mesh& aMesh, const TopoDS_Shape& aShape);

  //! Checks shared by Compute and Evaluate, then cell owners of the grid covering all solids
  //! of the main shape; false with error() set on failure. forCompute: cancelable, progress;
  //! classify false: grid only, owners left empty.
  bool prepare(SMESH_Mesh& aMesh, VoxelPlugin_Classifier& classifier, VoxelPlugin_Grid& grid,
               std::vector<int>& owners, std::size_t* nbUnknown = nullptr, bool forCompute = false,
               bool classify = true);

  const VoxelPlugin_Hypothesis* _hyp;
  std::atomic<int>              _nbDoneSlabs; //!< classified k-slabs of the current compute
  int                           _nbSlabs;
  std::string                   _coverageWarning; //!< set by prepare(): grid planes not covering the solids
};

#endif
