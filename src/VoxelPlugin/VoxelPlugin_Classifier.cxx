// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#include "VoxelPlugin_Classifier.hxx"

#include <BRepBndLib.hxx>
#include <BRepClass3d_SolidClassifier.hxx>
#include <IntCurvesFace_ShapeIntersector.hxx>
#include <BRepGProp.hxx>
#include <BRep_Tool.hxx>
#include <GProp_GProps.hxx>
#include <TopExp.hxx>
#include <TopExp_Explorer.hxx>
#include <TopTools_IndexedMapOfShape.hxx>
#include <gp_Dir.hxx>
#include <gp_Lin.hxx>
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

  // X coordinate and width of the cells, for the scanline
  std::vector<double> cx(nx), dx(nx);
  for (int i = 0; i < nx; ++i)
  {
    cx[i] = theGrid.Center(0, i);
    dx[i] = theGrid.coords[0][i + 1] - theGrid.coords[0][i];
  }

  auto work = [&]()
  {
    try
    {
      // one classifier and one line intersector per solid and per thread, built on first use
      std::vector<std::unique_ptr<BRepClass3d_SolidClassifier>>     classifiers(mySolids.size());
      std::vector<std::unique_ptr<IntCurvesFace_ShapeIntersector>> intersectors(mySolids.size());
      auto classifier = [&](int s) -> BRepClass3d_SolidClassifier&
      {
        if (!classifiers[s])
          classifiers[s].reset(new BRepClass3d_SolidClassifier(mySolids[s]));
        return *classifiers[s];
      };
      // exact state of a cell center, UNKNOWN retried at a shifted point then counted
      auto exactInside = [&](int s, const gp_Pnt& p) -> bool
      {
        BRepClass3d_SolidClassifier& c = classifier(s);
        c.Perform(p, tol);
        TopAbs_State state = c.State();
        if (state == TopAbs_UNKNOWN)
        {
          c.Perform(p.Translated(shift), tol);
          state = c.State();
          if (state == TopAbs_UNKNOWN)
            ++nbUnknown;
        }
        return state == TopAbs_IN || state == TopAbs_ON;
      };

      std::vector<double>      hits;
      std::vector<signed char> segmentState; // -1 not computed, 0 OUT, 1 IN, 2 undecided
      for (int k = nextK++; k < nz; k = nextK++)
      {
        for (int j = 0; j < ny; ++j)
        {
          if (theCanceled && *theCanceled)
          {
            nextK = nz;
            return;
          }
          const double y = theGrid.Center(1, j), z = theGrid.Center(2, k);
          int* rowOwners = &owners[theGrid.CellIndex(0, j, k)];
          if (myMethod == EXACT)
          {
            for (int i = 0; i < nx; ++i)
            {
              const gp_Pnt p(cx[i], y, z);
              for (int s : myOrder)
                if (!myBoxes[s].IsOut(p) && exactInside(s, p))
                {
                  rowOwners[i] = s;
                  break;
                }
            }
            continue;
          }

          int nbFree = nx;
          for (int s : myOrder)
          {
            if (nbFree == 0)
              break;
            double xMin, yMin, zMin, xMax, yMax, zMax;
            myBoxes[s].Get(xMin, yMin, zMin, xMax, yMax, zMax);
            if (y < yMin || y > yMax || z < zMin || z > zMax)
              continue;
            const int i0 = int(std::lower_bound(cx.begin(), cx.end(), xMin) - cx.begin());
            const int i1 = int(std::upper_bound(cx.begin(), cx.end(), xMax) - cx.begin());
            if (i0 >= i1)
              continue;

            // crossings of the row line with the solid boundary
            const double lineMin = xMin - minStep, lineMax = xMax + minStep;
            if (!intersectors[s])
            {
              intersectors[s].reset(new IntCurvesFace_ShapeIntersector());
              intersectors[s]->Load(mySolids[s], tol);
            }
            IntCurvesFace_ShapeIntersector& inter = *intersectors[s];
            inter.Perform(gp_Lin(gp_Pnt(0., y, z), gp_Dir(1., 0., 0.)), lineMin, lineMax);
            const bool lineDone = inter.IsDone();
            hits.clear();
            if (lineDone)
              for (int h = 1; h <= inter.NbPnt(); ++h)
                hits.push_back(inter.WParameter(h));
            std::sort(hits.begin(), hits.end());
            // a crossing on a seam or a shared edge is found once per face
            hits.erase(std::unique(hits.begin(), hits.end(),
                                   [&](double a, double b) { return b - a < 1e-3 * minStep; }),
                       hits.end());
            segmentState.assign(hits.size() + 1, -1);

            // stretches between crossings are entirely IN or OUT: classify their midpoint
            auto segmentInside = [&](std::size_t q, const gp_Pnt& p) -> bool
            {
              if (segmentState[q] < 0)
              {
                const double a = q == 0 ? lineMin : hits[q - 1];
                const double b = q == hits.size() ? lineMax : hits[q];
                BRepClass3d_SolidClassifier& c = classifier(s);
                c.Perform(gp_Pnt(0.5 * (a + b), y, z), tol);
                segmentState[q] = c.State() == TopAbs_IN ? 1 : c.State() == TopAbs_OUT ? 0 : 2;
              }
              return segmentState[q] == 2 ? exactInside(s, p) : segmentState[q] == 1;
            };

            for (int i = i0; i < i1; ++i)
            {
              if (rowOwners[i] >= 0)
                continue; // owned by a smaller solid
              const gp_Pnt p(cx[i], y, z);
              const std::size_t q = std::upper_bound(hits.begin(), hits.end(), cx[i]) - hits.begin();
              const bool nearCrossing = (q > 0 && cx[i] - hits[q - 1] < dx[i]) ||
                                        (q < hits.size() && hits[q] - cx[i] < dx[i]);
              const bool inside = !lineDone || nearCrossing ? exactInside(s, p) : segmentInside(q, p);
              if (inside)
              {
                rowOwners[i] = s;
                --nbFree;
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
