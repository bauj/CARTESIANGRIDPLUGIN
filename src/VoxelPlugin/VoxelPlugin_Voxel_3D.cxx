// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#include "VoxelPlugin_Voxel_3D.hxx"

#include "VoxelPlugin_CartesianMED.hxx"
#include "VoxelPlugin_Classifier.hxx"
#include "VoxelPlugin_Hypothesis.hxx"

#include <SMDS_MeshNode.hxx>
#include <SMESHDS_GroupBase.hxx>
#include <SMESHDS_Mesh.hxx>
#include <SMESHDS_SubMesh.hxx>
#include <SMESH_Comment.hxx>
#include <SMESH_Group.hxx>
#include <SMESH_ComputeError.hxx>
#include <SMESH_Mesh.hxx>
#include <SMESH_MesherHelper.hxx>
#include <SMESH_subMesh.hxx>
#include <SMESH_subMeshEventListener.hxx>

#include <Standard_Failure.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <TopoDS.hxx>
#include <Utils_SALOME_Exception.hxx>

#include <algorithm>
#include <cmath>
#include <sstream>
#include <vector>

namespace
{
  //! part of the compute time spent classifying cells (measured: 17% on a sphere, 150^3 cells)
  const double theClassifyShare = 0.2;

  const char* shapeTypeName(TopAbs_ShapeEnum type)
  {
    static const char* names[] = { "COMPOUND", "COMPSOLID", "SOLID", "SHELL",
                                   "FACE", "WIRE", "EDGE", "VERTEX", "SHAPE" };
    return names[type];
  }

  //! Calls f(i, j, k, owner) for each hexahedron to create: owned cells first,
  //! then outside cells (owner -1) when they are kept. False if stopped by *canceled.
  template <class F>
  bool forEachHexa(const VoxelPlugin_Grid& grid, const std::vector<int>& owners, bool keepOutside, F f,
                   const volatile bool* canceled = nullptr)
  {
    const int nx = grid.NbCells(0), ny = grid.NbCells(1), nz = grid.NbCells(2);
    for (int pass = 0; pass < (keepOutside ? 2 : 1); ++pass)
      for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
        {
          if (canceled && *canceled)
            return false;
          for (int i = 0; i < nx; ++i)
          {
            const int owner = owners[grid.CellIndex(i, j, k)];
            if ((owner >= 0) == (pass == 0))
              f(i, j, k, owner);
          }
        }
    return true;
  }

  /*!
   * \brief Event listener setting/unsetting _alwaysComputed flag to sub-meshes of
   *        inferior levels (copied from StdMeshers_Cartesian_3D)
   */
  struct _EventListener : public SMESH_subMeshEventListener
  {
    std::string _algoName;

    _EventListener(const std::string& algoName)
      : SMESH_subMeshEventListener(/*isDeletable=*/true, "VoxelPlugin_Voxel_3D::_EventListener"),
        _algoName(algoName)
    {}

    static void setAlwaysComputed(const bool isComputed, SMESH_subMesh* subMeshOfSolid)
    {
      SMESH_subMeshIteratorPtr smIt =
        subMeshOfSolid->getDependsOnIterator(/*includeSelf=*/false, /*complexShapeFirst=*/false);
      while (smIt->more())
        smIt->next()->SetIsAlwaysComputed(isComputed);
      // CHECK_COMPUTE_STATE would turn FAILED_TO_COMPUTE into READY_TO_COMPUTE
      // and hide the compute error
      if (subMeshOfSolid->GetComputeState() != SMESH_subMesh::FAILED_TO_COMPUTE)
        subMeshOfSolid->ComputeStateEngine(SMESH_subMesh::CHECK_COMPUTE_STATE);
    }

    virtual void ProcessEvent(const int /*event*/, const int eventType,
                              SMESH_subMesh* subMeshOfSolid,
                              SMESH_subMeshEventListenerData* /*data*/,
                              const SMESH_Hypothesis* /*hyp*/ = 0)
    {
      if (eventType == SMESH_subMesh::COMPUTE_EVENT)
      {
        setAlwaysComputed(subMeshOfSolid->GetComputeState() == SMESH_subMesh::COMPUTE_OK,
                          subMeshOfSolid);
      }
      else
      {
        SMESH_Algo* algo3D = subMeshOfSolid->GetAlgo();
        if (!algo3D || _algoName != algo3D->GetName())
        {
          subMeshOfSolid->SetIsAlwaysComputed(false); // set on solids owning no cell
          setAlwaysComputed(false, subMeshOfSolid);
        }
      }
    }

    static void SetOn(SMESH_subMesh* subMeshOfSolid, const std::string& algoName)
    {
      subMeshOfSolid->SetEventListener(new _EventListener(algoName), /*data=*/0, subMeshOfSolid);
    }
  };
}

VoxelPlugin_Voxel_3D::VoxelPlugin_Voxel_3D(int hypId, SMESH_Gen* gen)
  : SMESH_3D_Algo(hypId, gen), _hyp(nullptr), _nbDoneSlabs(0), _nbSlabs(0)
{
  _name = "Voxel_3D";
  _shapeType = (1 << TopAbs_SOLID);
  _compatibleHypothesis.push_back(VoxelPlugin_Hypothesis::GetHypType());
  _onlyUnaryInput = false;          // mesh all solids at once
  _requireDiscreteBoundary = false; // no 2D mesh needed
  _supportSubmeshes = false;        // do not use any existing mesh
}

bool VoxelPlugin_Voxel_3D::CheckHypothesis(SMESH_Mesh& aMesh, const TopoDS_Shape& aShape,
                                           Hypothesis_Status& aStatus)
{
  _hyp = nullptr;
  aStatus = SMESH_Hypothesis::HYP_MISSING;
  const std::list<const SMESHDS_Hypothesis*>& hyps =
    GetUsedHypothesis(aMesh, aShape, /*skipAux=*/false);
  for (const SMESHDS_Hypothesis* h : hyps)
    if ((_hyp = dynamic_cast<const VoxelPlugin_Hypothesis*>(h)))
    {
      aStatus = SMESH_Hypothesis::HYP_OK;
      break;
    }
  return aStatus == SMESH_Hypothesis::HYP_OK;
}

void VoxelPlugin_Voxel_3D::removeMeshOnShape(SMESH_Mesh& aMesh, const TopoDS_Shape& aShape)
{
  SMESHDS_Mesh* meshDS = aMesh.GetMeshDS();
  SMESHDS_SubMesh* sm = meshDS->MeshElements(aShape);
  if (!sm)
    return;
  std::vector<const SMDS_MeshElement*> elems;
  for (SMDS_ElemIteratorPtr it = sm->GetElements(); it->more();)
    elems.push_back(it->next());
  for (const SMDS_MeshElement* e : elems)
    meshDS->RemoveFreeElement(e, sm, /*fromGroups=*/true);
  std::vector<const SMDS_MeshNode*> nodes;
  for (SMDS_NodeIteratorPtr it = sm->GetNodes(); it->more();)
    nodes.push_back(it->next());
  for (const SMDS_MeshNode* n : nodes)
    meshDS->RemoveNode(n); // removes the hexas of solids still using it
}

bool VoxelPlugin_Voxel_3D::prepare(SMESH_Mesh& aMesh, VoxelPlugin_Classifier& classifier,
                                   VoxelPlugin_Grid& grid, std::vector<int>& owners,
                                   std::size_t* nbUnknown, bool forCompute,
                                   bool classify)
{
  _coverageWarning.clear();
  const TopoDS_Shape mainShape = aMesh.GetShapeToMesh();
  SMESHDS_Mesh* meshDS = aMesh.GetMeshDS();

  // the algorithm must mesh all solids of the main shape
  TopTools_IndexedMapOfShape mainSolids;
  TopExp::MapShapes(mainShape, TopAbs_SOLID, mainSolids);
  if (mainSolids.IsEmpty())
    return error(COMPERR_BAD_SHAPE,
                 SMESH_Comment("No solid in the shape (shape type ") << shapeTypeName(mainShape.ShapeType()) << ")");
  for (int i = 1; i <= mainSolids.Extent(); ++i)
    if (aMesh.GetSubMesh(mainSolids(i))->GetAlgo() != this)
      return error(COMPERR_BAD_SHAPE, "Voxel_3D must be assigned to the main shape (all solids)");

  if (_hyp->GetKeepOutsideCells() && mainShape.ShapeType() != TopAbs_COMPOUND)
    return error(COMPERR_BAD_PARMETERS, "Keep outside cells requires a compound main shape");

  switch (classifier.Init(mainShape))
  {
  case VoxelPlugin_Classifier::NO_SOLID:
    return error(COMPERR_BAD_SHAPE,
                 SMESH_Comment("No solid in the shape (shape type ") << shapeTypeName(mainShape.ShapeType()) << ")");
  case VoxelPlugin_Classifier::OPEN_SOLID:
    return error(COMPERR_BAD_SHAPE,
                 SMESH_Comment("Solid #") << meshDS->ShapeToIndex(classifier.Solid(classifier.BadSolid()))
                 << " is not closed");
  case VoxelPlugin_Classifier::OK:
    break;
  }

  double minC[3], maxC[3];
  if (!VoxelPlugin_Classifier::BoundingBox(mainShape, minC, maxC))
    return error(COMPERR_BAD_SHAPE, "Empty bounding box");
  long long nb[3];
  _hyp->NbCellsOfGrid(minC, maxC, nb);
  if ((double)nb[0] * nb[1] * nb[2] > (double)VoxelPlugin_Grid::MaxNbCells)
    return error(COMPERR_BAD_PARMETERS,
                 SMESH_Comment("Grid of ") << nb[0] << " x " << nb[1] << " x " << nb[2]
                 << " cells exceeds the limit of " << VoxelPlugin_Grid::MaxNbCells << " cells");
  grid = _hyp->MakeGrid(minC, maxC);
  _coverageWarning = _hyp->CoverageWarning(grid, minC, maxC);
  if (!classify)
    return true;
  if (forCompute)
  {
    _nbDoneSlabs = 0;
    _nbSlabs = grid.NbCells(2);
    owners = classifier.Classify(grid, /*theNbThreads=*/0, nbUnknown, &_computeCanceled, &_nbDoneSlabs);
  }
  else
    owners = classifier.Classify(grid, /*theNbThreads=*/0, nbUnknown);
  return true;
}

bool VoxelPlugin_Voxel_3D::Compute(SMESH_Mesh& aMesh, const TopoDS_Shape& aShape)
{
  const TopoDS_Shape mainShape = aMesh.GetShapeToMesh();
  SMESHDS_Mesh* meshDS = aMesh.GetMeshDS();

  _nbSlabs = 0; // no progress of a previous compute

  // also on failure: no stale outside cells of a previous compute
  removeMeshOnShape(aMesh, mainShape);

  // SMESH passes the solids ready to compute only (e.g. not a solid still failed by a previous
  // compute, or computed while another one was cleared) but all of them are meshed together
  TopTools_IndexedMapOfShape solidsToCompute, solidsLeftOut;
  TopExp::MapShapes(aShape, TopAbs_SOLID, solidsToCompute);
  for (TopExp_Explorer solid(mainShape, TopAbs_SOLID); solid.More(); solid.Next())
    if (!solidsToCompute.Contains(solid.Current()))
      solidsLeftOut.Add(solid.Current());

  try
  {
    VoxelPlugin_Classifier classifier;
    VoxelPlugin_Grid grid;
    std::vector<int> owners;
    std::size_t nbUnknown = 0;
    if (!prepare(aMesh, classifier, grid, owners, &nbUnknown, /*forCompute=*/true))
      return false;
    if (_computeCanceled)
      return error(COMPERR_CANCELED, "Compute canceled");
    for (int i = 1; i <= solidsLeftOut.Extent(); ++i)
      removeMeshOnShape(aMesh, solidsLeftOut(i));
    const bool keepOutside = _hyp->GetKeepOutsideCells();

    // shape index of each solid in the mesh
    std::vector<int> solidIndex(classifier.NbSolids());
    for (int s = 0; s < classifier.NbSolids(); ++s)
      solidIndex[s] = meshDS->ShapeToIndex(classifier.Solid(s));
    const int mainIndex = meshDS->ShapeToIndex(mainShape);

    const int nx = grid.NbCells(0), ny = grid.NbCells(1), nz = grid.NbCells(2);
    std::vector<const SMDS_MeshNode*> nodes((std::size_t)(nx + 1) * (ny + 1) * (nz + 1), nullptr);
    auto node = [&](int i, int j, int k, int shapeIndex) -> const SMDS_MeshNode*
    {
      const SMDS_MeshNode*& n = nodes[grid.NodeIndex(i, j, k)];
      if (!n)
      {
        SMDS_MeshNode* newNode = meshDS->AddNode(grid.coords[0][i], grid.coords[1][j], grid.coords[2][k]);
        meshDS->SetNodeInVolume(newNode, shapeIndex);
        n = newNode;
      }
      return n;
    };

    std::size_t nbHexas = 0;
    for (int owner : owners)
      nbHexas += (owner >= 0 || keepOutside);
    std::vector<int> nbCellsOfSolid(classifier.NbSolids(), 0);
    std::size_t nbOutside = 0, nbDone = 0;
    const bool done = forEachHexa(grid, owners, keepOutside, [&](int i, int j, int k, int owner)
    {
      const int shapeIndex = owner < 0 ? mainIndex : solidIndex[owner];
      SMDS_MeshVolume* hexa = meshDS->AddVolume(
        node(i, j, k, shapeIndex),         node(i, j + 1, k, shapeIndex),
        node(i + 1, j + 1, k, shapeIndex), node(i + 1, j, k, shapeIndex),
        node(i, j, k + 1, shapeIndex),     node(i, j + 1, k + 1, shapeIndex),
        node(i + 1, j + 1, k + 1, shapeIndex), node(i + 1, j, k + 1, shapeIndex));
      meshDS->SetMeshElementOnShape(hexa, shapeIndex);
      if (owner < 0)
        ++nbOutside;
      else
        ++nbCellsOfSolid[owner];
      _progress = theClassifyShare + (1. - theClassifyShare) * (double)++nbDone / (double)nbHexas;
    }, &_computeCanceled);
    if (!done)
    {
      // nothing half done is left
      removeMeshOnShape(aMesh, mainShape);
      for (int s = 0; s < classifier.NbSolids(); ++s)
        removeMeshOnShape(aMesh, classifier.Solid(s));
      return error(COMPERR_CANCELED, "Compute canceled");
    }

    // solids owning no cell must not fail the compute
    int nbEmpty = 0;
    for (int s = 0; s < classifier.NbSolids(); ++s)
      if (nbCellsOfSolid[s] == 0)
      {
        ++nbEmpty;
        aMesh.GetSubMesh(classifier.Solid(s))->SetIsAlwaysComputed(true);
      }

    // let lower-dimension sub-shapes look computed
    for (TopExp_Explorer soExp(mainShape, TopAbs_SOLID); soExp.More(); soExp.Next())
      _EventListener::setAlwaysComputed(true, aMesh.GetSubMesh(soExp.Current()));
    // SMESH updates the state of the solids it passed only
    for (int i = 1; i <= solidsLeftOut.Extent(); ++i)
      aMesh.GetSubMesh(solidsLeftOut(i))->ComputeStateEngine(SMESH_subMesh::CHECK_COMPUTE_STATE);

    SMESH_Comment warning;
    warning << _coverageWarning;
    if (nbOutside > 0)
      warning << nbOutside << " cell(s) outside all solids kept on the main shape. ";
    if (nbEmpty > 0)
      warning << nbEmpty << " solid(s) own no cell, refine the grid. ";
    if (nbUnknown > 0)
      warning << nbUnknown << " cell center(s) could not be located in a solid and were taken"
              << " as outside it, check the geometry.";
    if (!warning.empty())
      error(COMPERR_WARNING, warning);
    return true;
  }
  catch (const Standard_Failure& ex)
  {
    return error(COMPERR_OCC_EXCEPTION, ex.GetMessageString());
  }
  catch (const std::exception& ex)
  {
    return error(COMPERR_STD_EXCEPTION, ex.what());
  }
}

bool VoxelPlugin_Voxel_3D::Evaluate(SMESH_Mesh& aMesh, const TopoDS_Shape& aShape,
                                    MapShapeNbElems& aResMap)
{
  InitComputeError();

  // SMESH evaluates solid by solid: the first call evaluates all solids of the main shape
  const TopoDS_Shape mainShape = aMesh.GetShapeToMesh();

  // no mesh on faces, edges and vertices (SMESH counts one node per vertex otherwise)
  const std::vector<smIdType> none(SMDSEntity_Last, 0);
  for (TopExp_Explorer solid(mainShape, TopAbs_SOLID); solid.More(); solid.Next())
  {
    SMESH_subMeshIteratorPtr smIt = aMesh.GetSubMesh(solid.Current())
      ->getDependsOnIterator(/*includeSelf=*/false, /*complexShapeFirst=*/false);
    while (smIt->more())
      aResMap[smIt->next()] = none;
  }

  bool ok = false;
  try
  {
    VoxelPlugin_Classifier classifier;
    VoxelPlugin_Grid grid;
    std::vector<int> owners;
    if (prepare(aMesh, classifier, grid, owners))
    {
      // same creation order as Compute: a node belongs to the shape of its first hexa
      const int nbSolids = classifier.NbSolids(), mainSlot = nbSolids;
      std::vector<smIdType> nbNodes(nbSolids + 1, 0), nbHexas(nbSolids + 1, 0);
      std::vector<int> nodeSlot((std::size_t)(grid.NbCells(0) + 1) * (grid.NbCells(1) + 1) *
                                (grid.NbCells(2) + 1), -1);
      forEachHexa(grid, owners, _hyp->GetKeepOutsideCells(), [&](int i, int j, int k, int owner)
      {
        const int slot = owner < 0 ? mainSlot : owner;
        ++nbHexas[slot];
        for (int dk = 0; dk < 2; ++dk)
          for (int dj = 0; dj < 2; ++dj)
            for (int di = 0; di < 2; ++di)
            {
              int& n = nodeSlot[grid.NodeIndex(i + di, j + dj, k + dk)];
              if (n < 0)
              {
                n = slot;
                ++nbNodes[slot];
              }
            }
      });
      for (int slot = 0; slot <= nbSolids; ++slot)
      {
        if (slot == mainSlot && nbHexas[slot] == 0)
          continue;
        std::vector<smIdType> nbEntities = none;
        nbEntities[SMDSEntity_Node] = nbNodes[slot];
        nbEntities[SMDSEntity_Hexa] = nbHexas[slot];
        const TopoDS_Shape& shape = slot == mainSlot ? mainShape : classifier.Solid(slot);
        aResMap[aMesh.GetSubMesh(shape)] = nbEntities;
      }
      ok = true;
    }
  }
  catch (const Standard_Failure& ex)
  {
    error(COMPERR_OCC_EXCEPTION, ex.GetMessageString());
  }
  catch (const std::exception& ex)
  {
    error(COMPERR_STD_EXCEPTION, ex.what());
  }
  if (!ok)
  {
    // the other solids are not evaluated again; SMESH would reset an error stored on them
    for (TopExp_Explorer solid(mainShape, TopAbs_SOLID); solid.More(); solid.Next())
      aResMap[aMesh.GetSubMesh(solid.Current())] = none;
    // a negative count makes SMESH_Gen_i::Evaluate report "Failed to evaluate"; our own
    // message cannot reach it, SMESH_Gen::Evaluate resets sub-mesh errors in its last pass
    std::vector<smIdType> failed = none;
    failed[SMDSEntity_Node] = -1;
    aResMap[aMesh.GetSubMesh(aShape)] = failed;
  }
  return ok;
}

double VoxelPlugin_Voxel_3D::GetProgress() const
{
  if (_progress > 0.)
    return _progress;
  return _nbSlabs > 0 ? theClassifyShare * _nbDoneSlabs / _nbSlabs : 0.;
}

void VoxelPlugin_Voxel_3D::SetEventListener(SMESH_subMesh* subMesh)
{
  _EventListener::SetOn(subMesh, GetName());
}

double VoxelPlugin_Voxel_3D::ExportProgress() const
{
  return _nbSlabs > 0 ? std::min(1., (double)_nbDoneSlabs / _nbSlabs) : 0.;
}

std::string VoxelPlugin_Voxel_3D::ExportCartesianMED(SMESH_Mesh& aMesh, const std::string& file,
                                                     const std::string& meshName, bool fromMesh,
                                                     std::vector<VoxelPlugin_ExportZone> zones)
{
  if (zones.empty())
    throw SALOME_Exception("No zone group");
  std::sort(zones.begin(), zones.end(),
            [](const VoxelPlugin_ExportZone& a, const VoxelPlugin_ExportZone& b) { return a.name < b.name; });
  const TopoDS_Shape mainShape = aMesh.GetShapeToMesh();
  Hypothesis_Status status;
  if (!CheckHypothesis(aMesh, mainShape, status) || !_hyp)
    throw SALOME_Exception("The mesh has no Voxel Grid hypothesis");

  InitComputeError();             // also resets _computeCanceled
  _nbDoneSlabs = 0;
  _nbSlabs = 0;
  VoxelPlugin_Classifier classifier;
  VoxelPlugin_Grid grid;
  std::vector<int> owners;
  if (!prepare(aMesh, classifier, grid, owners, nullptr, /*forCompute=*/false, /*classify=*/false))
    throw SALOME_Exception(_comment.c_str());

  SMESHDS_Mesh* meshDS = aMesh.GetMeshDS();
  std::vector<int> zoneOfCell(grid.NbCells(), 0);
  std::vector<int> solidsWithoutZone;
  std::size_t nbUnknown = 0;
  if (!fromMesh)
  {
    // zones of the solids first: an overlap is reported before the long classification
    std::vector<int> zoneOfSolid(classifier.NbSolids(), 0);
    for (int s = 0; s < classifier.NbSolids(); ++s)
    {
      const int id = meshDS->ShapeToIndex(classifier.Solid(s));
      for (std::size_t z = 0; z < zones.size(); ++z)
        if (std::find(zones[z].ids.begin(), zones[z].ids.end(), id) != zones[z].ids.end())
        {
          if (zoneOfSolid[s])
            throw SALOME_Exception((SMESH_Comment("Solid #") << id << " is in zone groups "
                                    << zones[zoneOfSolid[s] - 1].name << " and " << zones[z].name).c_str());
          zoneOfSolid[s] = (int)z + 1;
        }
      if (!zoneOfSolid[s])
        solidsWithoutZone.push_back(id);
    }
    _nbSlabs = grid.NbCells(2);
    owners = classifier.Classify(grid, /*theNbThreads=*/0, &nbUnknown, &_computeCanceled, &_nbDoneSlabs);
    if (_computeCanceled)
      throw SALOME_Exception("Export canceled");
    for (std::size_t c = 0; c < owners.size(); ++c)
      zoneOfCell[c] = owners[c] < 0 ? 0 : zoneOfSolid[owners[c]];
  }
  else
    zonesFromMesh(aMesh, grid, zones, zoneOfCell);   // Task 3

  const std::vector<double> coords[3] = { grid.coords[0], grid.coords[1], grid.coords[2] };
  std::vector<std::string> names;
  for (const VoxelPlugin_ExportZone& z : zones)
    names.push_back(z.name);
  try
  {
    VoxelPlugin_CartesianMED::Write(file, meshName, coords, zoneOfCell, names);
  }
  catch (const std::exception& ex)
  {
    throw SALOME_Exception(ex.what());
  }

  std::vector<std::size_t> count(zones.size() + 1, 0);
  for (int z : zoneOfCell)
    ++count[z];
  std::ostringstream summary;
  summary << "Cartesian grid " << grid.NbCells(0) << " x " << grid.NbCells(1) << " x " << grid.NbCells(2)
          << " = " << grid.NbCells() << " cells written to " << file << "\n";
  for (std::size_t z = 0; z < zones.size(); ++z)
    summary << "  " << zones[z].name << ": " << count[z + 1] << " cells\n";
  summary << "  no zone: " << count[0] << " cells\n";
  if (!solidsWithoutZone.empty())
  {
    summary << "  solids in no zone group:";
    for (std::size_t i = 0; i < solidsWithoutZone.size(); ++i)
      summary << (i ? ", #" : " #") << solidsWithoutZone[i];
    summary << "\n";
  }
  if (!_coverageWarning.empty())
    summary << "  warning: " << _coverageWarning << "\n";
  if (nbUnknown > 0)
    summary << "  warning: " << nbUnknown << " cell center(s) could not be located in a solid and"
            << " were taken as outside it, check the geometry\n";
  return summary.str();
}

void VoxelPlugin_Voxel_3D::zonesFromMesh(SMESH_Mesh& aMesh, const VoxelPlugin_Grid& grid,
                                         const std::vector<VoxelPlugin_ExportZone>& theZones,
                                         std::vector<int>& zoneOfCell)
{
  SMESHDS_Mesh* meshDS = aMesh.GetMeshDS();
  if (meshDS->NbVolumes() == 0)
    throw SALOME_Exception("The mesh is not computed");

  // index of the grid plane at x along axis, -1 if x is not a grid plane
  auto planeOf = [&grid](int axis, double x) -> int
  {
    const std::vector<double>& c = grid.coords[axis];
    const double tol = 1e-9 * (c.back() - c.front());
    const auto it = std::lower_bound(c.begin(), c.end(), x - tol);
    if (it == c.end() || std::abs(*it - x) > tol)
      return -1;
    return (int)(it - c.begin());
  };

  for (std::size_t z = 0; z < theZones.size(); ++z)
  {
    const SMESHDS_GroupBase* groupDS = nullptr;
    for (auto git = aMesh.GetGroups(); git->more(); )
    {
      SMESH_Group* g = git->next();
      if (g->GetGroupDS()->GetType() == SMDSAbs_Volume && theZones[z].name == g->GetName())
        groupDS = g->GetGroupDS();
    }
    if (!groupDS)
      throw SALOME_Exception(("No volume group " + theZones[z].name + " in the mesh").c_str());
    for (SMDS_ElemIteratorPtr eit = groupDS->GetElements(); eit->more(); )
    {
      const SMDS_MeshElement* e = eit->next();
      double lo[3] = { HUGE_VAL, HUGE_VAL, HUGE_VAL }, hi[3] = { -HUGE_VAL, -HUGE_VAL, -HUGE_VAL };
      for (SMDS_NodeIteratorPtr nit = e->nodeIterator(); nit->more(); )
      {
        const SMDS_MeshNode* n = nit->next();
        const double p[3] = { n->X(), n->Y(), n->Z() };
        for (int a = 0; a < 3; ++a)
        {
          lo[a] = std::min(lo[a], p[a]);
          hi[a] = std::max(hi[a], p[a]);
        }
      }
      int ijk[3];
      bool ok = e->GetEntityType() == SMDSEntity_Hexa;
      for (int a = 0; a < 3 && ok; ++a)
      {
        ijk[a] = planeOf(a, lo[a]);
        ok = ijk[a] >= 0 && planeOf(a, hi[a]) == ijk[a] + 1;
      }
      if (!ok)
        throw SALOME_Exception((SMESH_Comment("Element ") << e->GetID() << " of group " << theZones[z].name
                                << " is not a cell of the grid: the mesh does not match its hypothesis,"
                                   " compute it again").c_str());
      int& zc = zoneOfCell[grid.CellIndex(ijk[0], ijk[1], ijk[2])];
      if (zc && zc != (int)z + 1)
        throw SALOME_Exception((SMESH_Comment("Cell (") << ijk[0] << ", " << ijk[1] << ", " << ijk[2]
                                << ") is in zone groups " << theZones[zc - 1].name << " and "
                                << theZones[z].name).c_str());
      zc = (int)z + 1;
    }
  }
}
