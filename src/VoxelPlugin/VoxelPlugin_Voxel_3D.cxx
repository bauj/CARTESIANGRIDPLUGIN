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

void VoxelPlugin_Voxel_3D::removeMeshOnMainShape(SMESH_Mesh& aMesh)
{
  // outside cells kept by a previous compute are bound to the main shape
  SMESHDS_Mesh* meshDS = aMesh.GetMeshDS();
  SMESHDS_SubMesh* sm = meshDS->MeshElements(aMesh.GetShapeToMesh());
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

bool VoxelPlugin_Voxel_3D::Compute(SMESH_Mesh& aMesh, const TopoDS_Shape& aShape)
{
  const TopoDS_Shape mainShape = aMesh.GetShapeToMesh();
  SMESHDS_Mesh* meshDS = aMesh.GetMeshDS();

  // the algorithm must mesh all solids of the main shape
  TopTools_IndexedMapOfShape mainSolids, mySolids;
  TopExp::MapShapes(mainShape, TopAbs_SOLID, mainSolids);
  TopExp::MapShapes(aShape, TopAbs_SOLID, mySolids);
  if (mainSolids.IsEmpty())
    return error(COMPERR_BAD_SHAPE,
                 SMESH_Comment("No solid in the shape (shape type ") << shapeTypeName(mainShape.ShapeType()) << ")");
  if (mySolids.Extent() != mainSolids.Extent())
    return error(COMPERR_BAD_SHAPE, "Voxel_3D must be assigned to the main shape (all solids)");

  const bool keepOutside = _hyp->GetKeepOutsideCells();
  if (keepOutside && mainShape.ShapeType() != TopAbs_COMPOUND)
    return error(COMPERR_BAD_PARMETERS, "Keep outside cells requires a compound main shape");

  removeMeshOnMainShape(aMesh);

  try
  {
    VoxelPlugin_Classifier classifier;
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
    const VoxelPlugin_Grid grid = _hyp->MakeGrid(minC, maxC);
    const std::vector<int> owners = classifier.Classify(grid);

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
    auto addHexa = [&](int i, int j, int k, int shapeIndex)
    {
      SMDS_MeshVolume* hexa = meshDS->AddVolume(
        node(i, j, k, shapeIndex),         node(i, j + 1, k, shapeIndex),
        node(i + 1, j + 1, k, shapeIndex), node(i + 1, j, k, shapeIndex),
        node(i, j, k + 1, shapeIndex),     node(i, j + 1, k + 1, shapeIndex),
        node(i + 1, j + 1, k + 1, shapeIndex), node(i + 1, j, k + 1, shapeIndex));
      meshDS->SetMeshElementOnShape(hexa, shapeIndex);
    };

    // owned cells first, then outside cells
    std::vector<int> nbCellsOfSolid(classifier.NbSolids(), 0);
    std::size_t nbOutside = 0;
    for (int k = 0; k < nz; ++k)
      for (int j = 0; j < ny; ++j)
        for (int i = 0; i < nx; ++i)
        {
          const int owner = owners[grid.CellIndex(i, j, k)];
          if (owner < 0)
            ++nbOutside;
          else
          {
            addHexa(i, j, k, solidIndex[owner]);
            ++nbCellsOfSolid[owner];
          }
        }
    if (keepOutside && nbOutside > 0)
      for (int k = 0; k < nz; ++k)
        for (int j = 0; j < ny; ++j)
          for (int i = 0; i < nx; ++i)
            if (owners[grid.CellIndex(i, j, k)] < 0)
              addHexa(i, j, k, mainIndex);

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

    SMESH_Comment warning;
    if (keepOutside && nbOutside > 0)
      warning << nbOutside << " cell(s) outside all solids kept on the main shape. ";
    if (nbEmpty > 0)
      warning << nbEmpty << " solid(s) own no cell, refine the grid.";
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

bool VoxelPlugin_Voxel_3D::Evaluate(SMESH_Mesh& /*aMesh*/, const TopoDS_Shape& /*aShape*/,
                                    MapShapeNbElems& /*aResMap*/)
{
  return true;
}

void VoxelPlugin_Voxel_3D::SetEventListener(SMESH_subMesh* subMesh)
{
  _EventListener::SetOn(subMesh, GetName());
}
