// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
//
// Unit tests of the OCCT core: grid helpers and cell owner classifier

#include "VoxelPlugin_Classifier.hxx"
#include "VoxelPlugin_Grid.hxx"

#include <BRepAlgoAPI_Cut.hxx>
#include <BRepBuilderAPI_MakeShell.hxx>
#include <BRepBuilderAPI_Transform.hxx>
#include <BRepBuilderAPI_MakeVertex.hxx>
#include <BRepPrimAPI_MakeBox.hxx>
#include <BRepPrimAPI_MakeCone.hxx>
#include <BRepPrimAPI_MakeCylinder.hxx>
#include <BRepPrimAPI_MakeSphere.hxx>
#include <BRepPrimAPI_MakeTorus.hxx>
#include <BRep_Builder.hxx>
#include <TopExp_Explorer.hxx>
#include <TopoDS.hxx>
#include <TopoDS_Compound.hxx>
#include <TopoDS_Shell.hxx>
#include <TopoDS_Solid.hxx>
#include <gp_Ax1.hxx>
#include <gp_Ax2.hxx>
#include <gp_Dir.hxx>
#include <gp_Pnt.hxx>
#include <gp_Trsf.hxx>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <iostream>

static int nbFailures = 0;
#define CHECK(cond)                                                     \
  do { if (!(cond)) { ++nbFailures;                                     \
      std::cerr << __FILE__ << ":" << __LINE__ << " FAILED: " #cond "\n"; } } while (0)

static TopoDS_Shape box(double x0, double y0, double z0, double x1, double y1, double z1)
{
  return BRepPrimAPI_MakeBox(gp_Pnt(x0, y0, z0), gp_Pnt(x1, y1, z1)).Shape();
}

static TopoDS_Compound compound(std::initializer_list<TopoDS_Shape> shapes)
{
  BRep_Builder b;
  TopoDS_Compound c;
  b.MakeCompound(c);
  for (const TopoDS_Shape& s : shapes)
    b.Add(c, s);
  return c;
}

static VoxelPlugin_Grid uniformGrid(const double min[3], const double max[3], int nx, int ny, int nz)
{
  VoxelPlugin_Grid g;
  const int n[3] = { nx, ny, nz };
  for (int a = 0; a < 3; ++a)
    g.coords[a] = VoxelPlugin_Grid::UniformAxis(min[a], max[a], n[a]);
  return g;
}

static void gridHelpers()
{
  std::vector<double> c = VoxelPlugin_Grid::UniformAxis(-1., 1., 4);
  CHECK(c.size() == 5 && c[0] == -1. && c[2] == 0. && c[4] == 1.);
  CHECK(VoxelPlugin_Grid::NbCellsForStep(10., 3.) == 3);
  CHECK(VoxelPlugin_Grid::NbCellsForStep(10., 30.) == 1);
  CHECK(VoxelPlugin_Grid::NbCellsForStep(10., 0.) == 1);
  // tiny step saturates instead of overflowing
  CHECK(VoxelPlugin_Grid::NbCellsForStep(1e9, 1e-9) == VoxelPlugin_Grid::MaxNbCells + 1);
  CHECK(VoxelPlugin_Grid::NbCellsForStep(1., 1e-300) == VoxelPlugin_Grid::MaxNbCells + 1);
  const double mn[3] = { 0, 0, 0 }, mx[3] = { 3, 2, 1 };
  VoxelPlugin_Grid g = uniformGrid(mn, mx, 3, 2, 1);
  CHECK(g.NbCells() == 6);
  CHECK(g.CellIndex(2, 1, 0) == 5);
  CHECK(g.NodeIndex(3, 2, 1) == 4 * 3 * 2 - 1);
  CHECK(std::abs(g.Center(0, 1) - 1.5) < 1e-12);
}

static void singleBox()
{
  VoxelPlugin_Classifier c;
  CHECK(c.Init(box(0, 0, 0, 10, 10, 10)) == VoxelPlugin_Classifier::OK);
  CHECK(c.NbSolids() == 1);
  double mn[3], mx[3];
  CHECK(VoxelPlugin_Classifier::BoundingBox(c.Solid(0), mn, mx));
  std::vector<int> owners = c.Classify(uniformGrid(mn, mx, 10, 10, 10));
  CHECK(owners.size() == 1000);
  int nbIn = 0;
  for (int o : owners) nbIn += (o == 0);
  CHECK(nbIn == 1000);
}

static void sphereVolumeAndTightBox()
{
  TopoDS_Shape sphere = BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, 0), 10.).Shape();
  double mn[3], mx[3];
  CHECK(VoxelPlugin_Classifier::BoundingBox(sphere, mn, mx));
  for (int a = 0; a < 3; ++a)
    CHECK(std::abs(mn[a] + 10.) < 1e-6 && std::abs(mx[a] - 10.) < 1e-6);
  VoxelPlugin_Classifier c;
  CHECK(c.Init(sphere) == VoxelPlugin_Classifier::OK);
  std::size_t nbUnknown = 1;
  std::vector<int> owners = c.Classify(uniformGrid(mn, mx, 40, 40, 40), 0, &nbUnknown);
  CHECK(nbUnknown == 0);
  int nbIn = 0;
  for (int o : owners) nbIn += (o == 0);
  const double cellVolume = 0.5 * 0.5 * 0.5, exact = 4. / 3. * M_PI * 1000.;
  CHECK(std::abs(nbIn * cellVolume - exact) < 0.03 * exact);
}

static void nestedSmallestWins()
{
  VoxelPlugin_Classifier c;
  CHECK(c.Init(compound({ box(0, 0, 0, 10, 10, 10), box(2, 2, 2, 4, 4, 4) })) == VoxelPlugin_Classifier::OK);
  CHECK(c.NbSolids() == 2);
  const double mn[3] = { 0, 0, 0 }, mx[3] = { 10, 10, 10 };
  VoxelPlugin_Grid g = uniformGrid(mn, mx, 10, 10, 10);
  std::vector<int> owners = c.Classify(g);
  int nb[2] = { 0, 0 };
  for (int o : owners) { CHECK(o == 0 || o == 1); if (o >= 0) ++nb[o]; }
  CHECK(nb[1] == 8);
  CHECK(nb[0] == 992);
  CHECK(owners[g.CellIndex(2, 3, 2)] == 1);
}

static void disjointAndOutside()
{
  VoxelPlugin_Classifier c;
  CHECK(c.Init(compound({ box(0, 0, 0, 1, 1, 1), box(2, 0, 0, 3, 1, 1) })) == VoxelPlugin_Classifier::OK);
  const double mn[3] = { 0, 0, 0 }, mx[3] = { 3, 1, 1 };
  std::vector<int> owners = c.Classify(uniformGrid(mn, mx, 3, 1, 1));
  CHECK(owners.size() == 3 && owners[0] == 0 && owners[1] == -1 && owners[2] == 1);
}

static void touchingFaceOnCenter()
{
  // second box is bigger; the only cell center lies on the shared face x=1
  VoxelPlugin_Classifier c;
  CHECK(c.Init(compound({ box(1, 0, 0, 3, 1, 1), box(0, 0, 0, 1, 1, 1) })) == VoxelPlugin_Classifier::OK);
  VoxelPlugin_Grid g;
  g.coords[0] = { 0., 2. };
  g.coords[1] = { 0., 1. };
  g.coords[2] = { 0., 1. };
  std::vector<int> owners = c.Classify(g);
  CHECK(owners.size() == 1 && owners[0] == 1); // smaller solid (volume 1) wins
}

static void locatedInstances()
{
  TopoDS_Shape b = box(0, 0, 0, 1, 1, 1);
  gp_Trsf t;
  t.SetTranslation(gp_Vec(2, 0, 0));
  TopoDS_Shape moved = b.Moved(TopLoc_Location(t));
  VoxelPlugin_Classifier c;
  CHECK(c.Init(compound({ b, moved })) == VoxelPlugin_Classifier::OK);
  CHECK(c.NbSolids() == 2);
  const double mn[3] = { 0, 0, 0 }, mx[3] = { 3, 1, 1 };
  std::vector<int> owners = c.Classify(uniformGrid(mn, mx, 3, 1, 1));
  CHECK(owners.size() == 3 && owners[0] == 0 && owners[1] == -1 && owners[2] == 1);
}

static void noSolid()
{
  TopExp_Explorer f(box(0, 0, 0, 1, 1, 1), TopAbs_FACE);
  VoxelPlugin_Classifier c;
  CHECK(c.Init(compound({ f.Current() })) == VoxelPlugin_Classifier::NO_SOLID);
}

static void openSolid()
{
  BRep_Builder b;
  TopoDS_Shell shell;
  b.MakeShell(shell);
  int i = 0;
  for (TopExp_Explorer f(box(0, 0, 0, 1, 1, 1), TopAbs_FACE); f.More(); f.Next())
    if (i++ > 0) // skip first face
      b.Add(shell, f.Current());
  TopoDS_Solid solid;
  b.MakeSolid(solid);
  b.Add(solid, shell);
  VoxelPlugin_Classifier c;
  CHECK(c.Init(compound({ box(5, 5, 5, 6, 6, 6), solid })) == VoxelPlugin_Classifier::OPEN_SOLID);
  CHECK(c.BadSolid() == 1);
}

static void boxIgnoresNonSolids()
{
  // a construction vertex far away must not enlarge the grid extent
  TopoDS_Shape vertex = BRepBuilderAPI_MakeVertex(gp_Pnt(100, 0, 0)).Shape();
  double mn[3], mx[3];
  CHECK(VoxelPlugin_Classifier::BoundingBox(compound({ box(0, 0, 0, 10, 10, 10), vertex }), mn, mx));
  CHECK(std::abs(mx[0] - 10.) < 1e-6);
  CHECK(!VoxelPlugin_Classifier::BoundingBox(compound({ vertex }), mn, mx));
}

static void threadsAgree()
{
  VoxelPlugin_Classifier c;
  CHECK(c.Init(compound({ BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, 0), 10.).Shape(),
                          BRepPrimAPI_MakeSphere(gp_Pnt(3, 0, 0), 4.).Shape() })) == VoxelPlugin_Classifier::OK);
  const double mn[3] = { -10, -10, -10 }, mx[3] = { 10, 10, 10 };
  VoxelPlugin_Grid g = uniformGrid(mn, mx, 24, 24, 24);
  CHECK(c.Classify(g, 1) == c.Classify(g, 4));
}

static void cancelAndProgress()
{
  VoxelPlugin_Classifier c;
  CHECK(c.Init(BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, 0), 10.).Shape()) == VoxelPlugin_Classifier::OK);
  const double mn[3] = { -10, -10, -10 }, mx[3] = { 10, 10, 10 };
  VoxelPlugin_Grid g = uniformGrid(mn, mx, 8, 8, 12);
  std::atomic<int> nbDone(0);
  bool canceled = false;
  std::vector<int> owners = c.Classify(g, 3, nullptr, &canceled, &nbDone);
  CHECK(nbDone == 12);
  CHECK(owners == c.Classify(g, 1));
  // canceled before start: nothing classified
  canceled = true;
  nbDone = 0;
  owners = c.Classify(g, 3, nullptr, &canceled, &nbDone);
  CHECK(nbDone == 0);
  CHECK(std::count(owners.begin(), owners.end(), -1) == (long)owners.size());
}

static TopoDS_Shape rotated(const TopoDS_Shape& shape, const gp_Ax1& axis, double angle)
{
  gp_Trsf t;
  t.SetRotation(axis, angle);
  return BRepBuilderAPI_Transform(shape, t, /*copy=*/true).Shape();
}

static void scanlineMatchesExact()
{
  const gp_Ax2 ax(gp_Pnt(0, 0, -10), gp_Dir(0, 0, 1));
  const gp_Ax1 skew(gp_Pnt(0, 0, 0), gp_Dir(1, 1, 1));
  const TopoDS_Shape sphere = BRepPrimAPI_MakeSphere(gp_Pnt(0, 0, 0), 10.).Shape();
  const std::vector<std::pair<const char*, TopoDS_Shape>> shapes = {
    { "sphere", sphere },
    { "torus", BRepPrimAPI_MakeTorus(gp_Ax2(gp_Pnt(0, 0, 0), gp_Dir(0, 0, 1)), 10., 3.).Shape() },
    { "skew cylinder", rotated(BRepPrimAPI_MakeCylinder(ax, 5., 20.).Shape(), skew, 0.7) },
    { "cone apex", BRepPrimAPI_MakeCone(ax, 6., 0., 20.).Shape() },
    { "box with hole", BRepAlgoAPI_Cut(box(-10, -10, -10, 10, 10, 10),
                                       BRepPrimAPI_MakeCylinder(ax, 4., 20.).Shape()).Shape() },
    { "rotated box", rotated(box(-6, -6, -6, 6, 6, 6), skew, 0.5) },
    { "nested and overlapping", compound({ sphere, box(-3, -3, -3, 3, 3, 3),
                                           BRepPrimAPI_MakeSphere(gp_Pnt(8, 0, 0), 5.).Shape() }) },
    { "touching boxes", compound({ box(-10, -10, -10, 0, 10, 10), box(0, -10, -10, 10, 10, 10) }) },
  };
  const int sizes[][3] = { { 20, 20, 20 }, { 17, 19, 23 }, { 31, 8, 13 } };
  for (const auto& named : shapes)
  {
    VoxelPlugin_Classifier c;
    CHECK(c.Init(named.second) == VoxelPlugin_Classifier::OK);
    double mn[3], mx[3];
    CHECK(VoxelPlugin_Classifier::BoundingBox(named.second, mn, mx));
    for (int a = 0; a < 3; ++a) { mn[a] -= 1.; mx[a] += 1.; }
    for (const int* n : sizes)
    {
      const VoxelPlugin_Grid g = uniformGrid(mn, mx, n[0], n[1], n[2]);
      c.SetMethod(VoxelPlugin_Classifier::EXACT);
      const std::vector<int> exact = c.Classify(g);
      c.SetMethod(VoxelPlugin_Classifier::SCANLINE);
      const std::vector<int> scan = c.Classify(g);
      std::size_t nbDiff = 0;
      for (std::size_t i = 0; i < exact.size(); ++i)
        nbDiff += exact[i] != scan[i];
      if (nbDiff)
        std::cerr << named.first << " " << n[0] << "x" << n[1] << "x" << n[2] << ": "
                  << nbDiff << " cells differ\n";
      CHECK(nbDiff == 0);
    }
  }
}

int main()
{
  gridHelpers();
  singleBox();
  sphereVolumeAndTightBox();
  nestedSmallestWins();
  disjointAndOutside();
  touchingFaceOnCenter();
  locatedInstances();
  noSolid();
  openSolid();
  threadsAgree();
  cancelAndProgress();
  scanlineMatchesExact();
  boxIgnoresNonSolids();
  std::cout << (nbFailures ? "FAILED: " : "OK ") << nbFailures << " failure(s)\n";
  return nbFailures ? 1 : 0;
}
