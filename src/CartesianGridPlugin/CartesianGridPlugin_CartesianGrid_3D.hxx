// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#ifndef _CartesianGridPlugin_CartesianGrid_3D_HXX_
#define _CartesianGridPlugin_CartesianGrid_3D_HXX_

#include "CartesianGridPlugin_Defs.hxx"

#include "CartesianGridPlugin_Grid.hxx"

#include <SMESH_Algo.hxx>

#include <atomic>
#include <string>
#include <vector>

class CartesianGridPlugin_Classifier;
class CartesianGridPlugin_Hypothesis;

//! Zone of the Cartesian MED export: group name and, for the geometry source, the sub-shape
//! ids (main shape indices, as SMESHDS_Mesh::ShapeToIndex) of its solids
struct CartesianGridPlugin_ExportZone
{
  std::string      name;
  std::vector<int> ids;
};

/*!
 * \brief "CartesianGrid_3D": one HEXA8 per Cartesian cell, bound to the smallest solid containing
 *        the cell center. Meshes all solids of the main shape at once.
 */
class CARTESIANGRIDPLUGIN_EXPORT CartesianGridPlugin_CartesianGrid_3D : public SMESH_3D_Algo
{
public:
  CartesianGridPlugin_CartesianGrid_3D(int hypId, SMESH_Gen* gen);

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
                                 std::vector<CartesianGridPlugin_ExportZone> zones);
  //! Classification progress of the running export, 0..1
  double ExportProgress() const;
  //! Stops the running export (classification), which then throws "Export canceled"
  void CancelExport() { _computeCanceled = true; }
  //! Sub-shape ids of the solids of the main shape, in TopExp::MapShapes order (classifier order)
  static std::vector<int> SolidIds(SMESH_Mesh& mesh);

private:
  //! zone of each cell from the hexahedra of the mesh volume groups named as theZones
  void zonesFromMesh(SMESH_Mesh& aMesh, const CartesianGridPlugin_Grid& grid,
                     const std::vector<CartesianGridPlugin_ExportZone>& theZones, std::vector<int>& zoneOfCell);

  void removeMeshOnShape(SMESH_Mesh& aMesh, const TopoDS_Shape& aShape);

  //! Checks shared by Compute and Evaluate, then cell owners of the grid covering all solids
  //! of the main shape; false with error() set on failure. forCompute: cancelable, progress;
  //! classify false: grid only, owners left empty.
  bool prepare(SMESH_Mesh& aMesh, CartesianGridPlugin_Classifier& classifier, CartesianGridPlugin_Grid& grid,
               std::vector<int>& owners, std::size_t* nbUnknown = nullptr, bool forCompute = false,
               bool classify = true);

  const CartesianGridPlugin_Hypothesis* _hyp;
  std::atomic<int>              _nbDoneSlabs; //!< classified k-slabs of the current compute
  int                           _nbSlabs;
  std::string                   _coverageWarning; //!< set by prepare(): grid planes not covering the solids
};

#endif
