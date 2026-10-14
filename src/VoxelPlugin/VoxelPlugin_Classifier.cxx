// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#include "VoxelPlugin_Classifier.hxx"

#include <BRepBndLib.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <gp_Pnt.hxx>
#include <gp_Vec.hxx>

#include <algorithm>
#include <atomic>
#include <exception>
#include <memory>
#include <mutex>
#include <numeric>
#include <system_error>
#include <thread>

VoxelPlugin_Classifier::Status VoxelPlugin_Classifier::Init(const TopoDS_Shape& theShape)
{
  mySolids.clear();
  myVolumes.clear();
  myBoxes.clear();
  myOrder.clear();
  myBadSolid = -1;

  TopTools_IndexedMapOfShape solids;
  TopExp::MapShapes(theShape, TopAbs_SOLID, solids);
  if (solids.IsEmpty())
    return NO_SOLID;

  for (int i = 1; i <= solids.Extent(); ++i)
  {
    const TopoDS_Shape& solid = solids(i);
    bool closed = false;
    for (TopExp_Explorer shell(solid, TopAbs_SHELL); shell.More(); shell.Next())
    {
      closed = BRep_Tool::IsClosed(shell.Current());
      if (!closed)
        break;
    }
    if (!closed)
    {
      myBadSolid = i - 1;
      return OPEN_SOLID;
    }
    GProp_GProps props;
    BRepGProp::VolumeProperties(solid, props);
    Bnd_Box box;
    BRepBndLib::AddOptimal(solid, box, /*useTriangulation=*/false, /*useShapeTolerance=*/true);
    mySolids.push_back(solid);
    myVolumes.push_back(std::abs(props.Mass()));
    myBoxes.push_back(box);
  }

  myOrder.resize(mySolids.size());
  std::iota(myOrder.begin(), myOrder.end(), 0);
  std::stable_sort(myOrder.begin(), myOrder.end(),
                   [this](int a, int b) { return myVolumes[a] < myVolumes[b]; });
  return OK;
}

std::vector<int> VoxelPlugin_Classifier::Classify(const VoxelPlugin_Grid& theGrid,
                                                  int                     theNbThreads,
                                                  std::size_t*            theNbUnknown,
                                                  const volatile bool*    theCanceled,
                                                  std::atomic<int>*       theNbDoneSlabs) const
{
  if (theNbUnknown)
    *theNbUnknown = 0;
  std::vector<int> owners(theGrid.NbCells(), -1);
  const int nx = theGrid.NbCells(0), ny = theGrid.NbCells(1), nz = theGrid.NbCells(2);
  if (owners.empty() || mySolids.empty())
    return owners;

  double minStep = 1e100;
  for (int a = 0; a < 3; ++a)
    for (int i = 0; i < theGrid.NbCells(a); ++i)
      minStep = std::min(minStep, theGrid.coords[a][i + 1] - theGrid.coords[a][i]);
  const double tol = 1e-6 * minStep;
  // retry of an UNKNOWN state: off the center along a skew direction, still inside the cell
  const gp_Vec shift = gp_Vec(1., 0.71, 0.37) * (1e-3 * minStep);

  int nbThreads = theNbThreads > 0 ? theNbThreads : (int)std::thread::hardware_concurrency();
  nbThreads = std::max(1, std::min(nbThreads, nz));

  std::atomic<int>         nextK(0);
  std::atomic<std::size_t> nbUnknown(0);
  std::exception_ptr firstError;
  std::mutex         errorMutex;

  auto work = [&]()
  {
    try
    {
      // one classifier per solid and per thread, built on first use
      std::vector<std::unique_ptr<BRepClass3d_SolidClassifier>> classifiers(mySolids.size());
      for (int k = nextK++; k < nz; k = nextK++)
      {
        for (int j = 0; j < ny; ++j)
        {
          if (theCanceled && *theCanceled)
          {
            nextK = nz;
            return;
          }
          for (int i = 0; i < nx; ++i)
          {
            const gp_Pnt p(theGrid.Center(0, i), theGrid.Center(1, j), theGrid.Center(2, k));
            for (int s : myOrder)
            {
              if (myBoxes[s].IsOut(p))
                continue;
              if (!classifiers[s])
                classifiers[s].reset(new BRepClass3d_SolidClassifier(mySolids[s]));
              classifiers[s]->Perform(p, tol);
              TopAbs_State state = classifiers[s]->State();
              if (state == TopAbs_UNKNOWN)
              {
                classifiers[s]->Perform(p.Translated(shift), tol);
                state = classifiers[s]->State();
                if (state == TopAbs_UNKNOWN)
                  ++nbUnknown;
              }
              if (state == TopAbs_IN || state == TopAbs_ON)
              {
                owners[theGrid.CellIndex(i, j, k)] = s;
                break;
              }
            }
          }
        }
        if (theNbDoneSlabs)
          ++*theNbDoneSlabs;
      }
    }
    catch (...)
    {
      std::lock_guard<std::mutex> lock(errorMutex);
      if (!firstError)
        firstError = std::current_exception();
      nextK = nz; // stop the other workers
    }
  };

  std::vector<std::thread> threads;
  try
  {
    for (int t = 1; t < nbThreads; ++t)
      threads.emplace_back(work);
  }
  catch (const std::system_error&)
  {
    // no more threads available: the started ones and this one share the slabs
  }
  work();
  for (std::thread& t : threads)
    t.join();
  if (firstError)
    std::rethrow_exception(firstError);
  if (theNbUnknown)
    *theNbUnknown = nbUnknown;
  return owners;
}

bool VoxelPlugin_Classifier::BoundingBox(const TopoDS_Shape& theShape,
                                         double              theMin[3],
                                         double              theMax[3])
{
  // solids only: construction vertices/edges/faces of a compound must not enlarge the grid
  Bnd_Box box;
  for (TopExp_Explorer solid(theShape, TopAbs_SOLID); solid.More(); solid.Next())
    BRepBndLib::AddOptimal(solid.Current(), box, /*useTriangulation=*/false, /*useShapeTolerance=*/false);
  if (box.IsVoid())
    return false;
  box.Get(theMin[0], theMin[1], theMin[2], theMax[0], theMax[1], theMax[2]);
  return true;
}
