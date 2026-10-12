// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
//
// VoxelGrid hypothesis: modes, grid building, validation, persistence

#include "VoxelPlugin_Hypothesis.hxx"

#include <Utils_SALOME_Exception.hxx>

#include <cmath>
#include <iostream>
#include <sstream>

static int nbFailures = 0;
#define CHECK(cond)                                                     \
  do { if (!(cond)) { ++nbFailures;                                     \
      std::cerr << __FILE__ << ":" << __LINE__ << " FAILED: " #cond "\n"; } } while (0)

template <class F> static bool throws(F f)
{
  try { f(); } catch (const SALOME_Exception&) { return true; }
  return false;
}

int main()
{
  VoxelPlugin_Hypothesis h(0, nullptr);
  CHECK(std::string(h.GetName()) == "VoxelGrid");
  for (int a = 0; a < 3; ++a)
    CHECK(h.GetMode(a) == VoxelPlugin_Hypothesis::NB_CELLS && h.GetNbCells(a) == 10);
  CHECK(!h.GetKeepOutsideCells());

  // modes and grid
  h.SetNbCells(0, 4);
  h.SetStep(1, 3.);     // extent 10 -> 3 cells, spacing 10/3
  h.SetStep(2, 30.);    // step > extent -> 1 cell
  CHECK(h.GetMode(1) == VoxelPlugin_Hypothesis::STEP && h.GetStep(1) == 3.);
  const double mn[3] = { 0, 0, 0 }, mx[3] = { 8, 10, 10 };
  VoxelPlugin_Grid g = h.MakeGrid(mn, mx);
  CHECK(g.NbCells(0) == 4 && g.NbCells(1) == 3 && g.NbCells(2) == 1);
  CHECK(g.coords[1].back() == 10. && std::abs(g.coords[1][1] - 10. / 3.) < 1e-12);

  // validation
  CHECK(throws([&] { h.SetNbCells(3, 5); }));
  CHECK(throws([&] { h.SetNbCells(0, 0); }));
  CHECK(throws([&] { h.SetStep(-1, 1.); }));
  CHECK(throws([&] { h.SetStep(0, 0.); }));
  CHECK(throws([&] { h.GetMode(5); }));

  // persistence round trip
  h.SetKeepOutsideCells(true);
  h.SetStep(0, 0.1 + 0.2); // needs full precision
  std::stringstream s;
  h.SaveTo(s);
  VoxelPlugin_Hypothesis h2(1, nullptr);
  h2.LoadFrom(s);
  CHECK(!s.fail());
  CHECK(h2.GetKeepOutsideCells());
  CHECK(h2.GetMode(0) == VoxelPlugin_Hypothesis::STEP && h2.GetStep(0) == 0.1 + 0.2);
  CHECK(h2.GetMode(1) == VoxelPlugin_Hypothesis::STEP && h2.GetStep(1) == 3.);
  CHECK(h2.GetMode(2) == VoxelPlugin_Hypothesis::STEP && h2.GetStep(2) == 30.);

  // unknown version is rejected and leaves defaults
  std::stringstream bad("99 0 1 1 0 1 1 0 1 1 0");
  VoxelPlugin_Hypothesis h3(2, nullptr);
  h3.LoadFrom(bad);
  CHECK(bad.fail() && h3.GetNbCells(0) == 10);

  std::cout << (nbFailures ? "FAILED: " : "OK ") << nbFailures << " failure(s)\n";
  return nbFailures ? 1 : 0;
}
