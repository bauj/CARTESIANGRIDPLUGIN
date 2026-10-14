// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#include "VoxelPlugin_Voxel_3D.hxx"

#include "VoxelPlugin_Classifier.hxx"
#include "VoxelPlugin_Hypothesis.hxx"

#include <SMDS_MeshNode.hxx>
#include <SMESHDS_Mesh.hxx>
#include <SMESHDS_SubMesh.hxx>
#include <SMESH_Comment.hxx>
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

#include <vector>

namespace
{
  const char* shapeTypeName(TopAbs_ShapeEnum type)
  {
    static const char* names[] = { "COMPOUND", "COMPSOLID", "SOLID", "SHELL",
                                   "FACE", "WIRE", "EDGE", "VERTEX", "SHAPE" };
    return names[type];
  }

  //! Calls f(i, j, k, owner) for each hexahedron to create: owned cells first,
  //! then outside cells (owner -1) when they are kept
  template <class F>
  void forEachHexa(const VoxelPlugin_Grid& grid, const std::vector<int>& owners, bool keepOutside, F f)
  {
    const int nx = grid.NbCells(0), ny = grid.NbCells(1), nz = grid.NbCells(2);
    for (int pass = 0; pass < (keepOutside ? 2 : 1); ++pass)
      for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
          for (int i = 0; i < nx; ++i)
          {
            const int owner = owners[grid.CellIndex(i, j, k)];
            if ((owner >= 0) == (pass == 0))
              f(i, j, k, owner);
          }
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
  : SMESH_3D_Algo(hypId, gen), _hyp(nullptr)
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
                                   std::size_t* nbUnknown)
{
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
  owners = classifier.Classify(grid, /*theNbThreads=*/0, nbUnknown);
  return true;
}

bool VoxelPlugin_Voxel_3D::Compute(SMESH_Mesh& aMesh, const TopoDS_Shape& aShape)
{
  const TopoDS_Shape mainShape = aMesh.GetShapeToMesh();
  SMESHDS_Mesh* meshDS = aMesh.GetMeshDS();

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
    if (!prepare(aMesh, classifier, grid, owners, &nbUnknown))
      return false;
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

    std::vector<int> nbCellsOfSolid(classifier.NbSolids(), 0);
    std::size_t nbOutside = 0;
    forEachHexa(grid, owners, keepOutside, [&](int i, int j, int k, int owner)
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
    });

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

void VoxelPlugin_Voxel_3D::SetEventListener(SMESH_subMesh* subMesh)
{
  _EventListener::SetOn(subMesh, GetName());
}
