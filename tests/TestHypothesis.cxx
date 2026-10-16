// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
//
// CartesianGridParameters hypothesis: modes, grid building, validation, persistence

#include "CartesianGridPlugin_Hypothesis.hxx"

#include <Utils_SALOME_Exception.hxx>

#include <sys/resource.h>

#include <cmath>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

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
  CartesianGridPlugin_Hypothesis h(0, nullptr);
  CHECK(std::string(h.GetName()) == "CartesianGridParameters");
  for (int a = 0; a < 3; ++a)
    CHECK(h.GetMode(a) == CartesianGridPlugin_Hypothesis::NB_CELLS && h.GetNbCells(a) == 10);
  CHECK(!h.GetKeepOutsideCells());

  // modes and grid
  h.SetNbCells(0, 4);
  h.SetStep(1, 3.);     // extent 10 -> 3 cells, spacing 10/3
  h.SetStep(2, 30.);    // step > extent -> 1 cell
  CHECK(h.GetMode(1) == CartesianGridPlugin_Hypothesis::STEP && h.GetStep(1) == 3.);
  const double mn[3] = { 0, 0, 0 }, mx[3] = { 8, 10, 10 };
  CartesianGridPlugin_Grid g = h.MakeGrid(mn, mx);
  CHECK(g.NbCells(0) == 4 && g.NbCells(1) == 3 && g.NbCells(2) == 1);
  CHECK(g.coords[1].back() == 10. && std::abs(g.coords[1][1] - 10. / 3.) < 1e-12);
  long long nb[3];
  h.NbCellsOfGrid(mn, mx, nb);
  CHECK(nb[0] == 4 && nb[1] == 3 && nb[2] == 1);
  {
    CartesianGridPlugin_Hypothesis tiny(2, nullptr);
    tiny.SetStep(0, 1e-12);
    tiny.NbCellsOfGrid(mn, mx, nb);
    CHECK(nb[0] == CartesianGridPlugin_Grid::MaxNbCellsPerAxis + 1);
  }

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
  CartesianGridPlugin_Hypothesis h2(1, nullptr);
  h2.LoadFrom(s);
  CHECK(!s.fail());
  CHECK(h2.GetKeepOutsideCells());
  CHECK(h2.GetMode(0) == CartesianGridPlugin_Hypothesis::STEP && h2.GetStep(0) == 0.1 + 0.2);
  CHECK(h2.GetMode(1) == CartesianGridPlugin_Hypothesis::STEP && h2.GetStep(1) == 3.);
  CHECK(h2.GetMode(2) == CartesianGridPlugin_Hypothesis::STEP && h2.GetStep(2) == 30.);

  // unknown version is rejected and leaves defaults
  std::stringstream bad("99 0 1 1 0 1 1 0 1 1 0");
  CartesianGridPlugin_Hypothesis h3(2, nullptr);
  h3.LoadFrom(bad);
  CHECK(bad.fail() && h3.GetNbCells(0) == 10);

  // ---- mode Coordinates
  {
    CartesianGridPlugin_Hypothesis c(10, nullptr);
    c.SetCoordinates(0, { 10, 0, 5, 2 });
    CHECK(c.GetMode(0) == CartesianGridPlugin_Hypothesis::COORDINATES);
    CHECK((c.GetCoordinates(0) == std::vector<double>{ 0, 2, 5, 10 }));
    CHECK(throws([&] { c.SetCoordinates(0, { 1 }); }));
    CHECK(throws([&] { c.SetCoordinates(0, { 1, 1 }); }));
    CHECK(throws([&] { c.SetCoordinates(0, { 0, std::numeric_limits<double>::quiet_NaN() }); }));
    CHECK(throws([&] { c.SetCoordinates(0, { 0, std::numeric_limits<double>::infinity() }); }));
    CHECK(throws([&] { c.SetCoordinates(3, { 0, 1 }); }));
    CHECK((c.GetCoordinates(0) == std::vector<double>{ 0, 2, 5, 10 })); // unchanged by failures

    // grid ignores the bounding box along a Coordinates axis
    const double bmn[3] = { -100, 0, 0 }, bmx[3] = { 100, 10, 10 };
    CartesianGridPlugin_Grid g = c.MakeGrid(bmn, bmx);
    CHECK((g.coords[0] == std::vector<double>{ 0, 2, 5, 10 }));
    CHECK(g.NbCells(1) == 10 && g.coords[1].back() == 10.);
    long long n[3];
    c.NbCellsOfGrid(bmn, bmx, n);
    CHECK(n[0] == 3 && n[1] == 10 && n[2] == 10);

    // mode change keeps the coordinates; same values switch the mode back
    c.SetNbCells(0, 7);
    CHECK(c.GetMode(0) == CartesianGridPlugin_Hypothesis::NB_CELLS && c.GetCoordinates(0).size() == 4);
    c.SetCoordinates(0, { 0, 2, 5, 10 });
    CHECK(c.GetMode(0) == CartesianGridPlugin_Hypothesis::COORDINATES);

    // coverage warning
    const double smn[3] = { 0, 0, 0 }, smx[3] = { 20, 10, 10 };
    g = c.MakeGrid(smn, smx);
    const std::string w = c.CoverageWarning(g, smn, smx);
    CHECK(w.find("X planes cover [0, 10], solids span [0, 20]") != std::string::npos);
    CHECK(w.find("Y") == std::string::npos);
    const std::vector<CartesianGridPlugin_Hypothesis::CoverageGap> gaps = c.CoverageGaps(g, smn, smx);
    CHECK(gaps.size() == 1 && gaps[0].axis == 0 && gaps[0].first == 0 && gaps[0].last == 10
          && gaps[0].min == 0 && gaps[0].max == 20);
    const double inmx[3] = { 10, 10, 10 };
    CHECK(c.CoverageWarning(c.MakeGrid(smn, inmx), smn, inmx).empty());
  }

  // ---- mode Spacing
  {
    CartesianGridPlugin_Hypothesis s(11, nullptr);
    s.SetSpacing(2, { "0.5", "2" }, { 0.5 });
    CHECK(s.GetMode(2) == CartesianGridPlugin_Hypothesis::SPACING);
    CHECK((s.GetSpacingFunctions(2) == std::vector<std::string>{ "0.5", "2" }));
    CHECK((s.GetSpacingPoints(2) == std::vector<double>{ 0.5 }));
    CHECK(throws([&] { s.SetSpacing(2, { "0.5-t" }, {}); }));   // spacing <= 0 at t >= 0.5
    CHECK(throws([&] { s.SetSpacing(2, { "t+" }, {}); }));      // unparsable
    CHECK(throws([&] { s.SetSpacing(2, { "1", "1" }, {}); }));  // count mismatch
    CHECK(throws([&] { s.SetSpacing(2, { "1" }, { 0.5 }); }));  // count mismatch
    CHECK(throws([&] { s.SetSpacing(2, { "1", "1" }, { 1.2 }); })); // point outside ]0,1[
    CHECK(throws([&] { s.SetSpacing(2, {}, {}); }));
    CHECK((s.GetSpacingFunctions(2) == std::vector<std::string>{ "0.5", "2" })); // unchanged

    // node at the internal point, fine below, coarse above
    const std::vector<double> z = s.AxisCoordinates(2, 0., 10.);
    CHECK(z.front() == 0. && z.back() == 10.);
    bool at5 = false;
    for (std::size_t i = 1; i < z.size(); ++i)
    {
      at5 = at5 || std::abs(z[i] - 5.) < 1e-9;
      if (z[i] <= 5. + 1e-9)
        CHECK(z[i] - z[i - 1] < 0.6);
      else
        CHECK(z[i] - z[i - 1] > 1.5);
    }
    CHECK(at5);

    // a constant function c gives the Step c grid
    CartesianGridPlugin_Hypothesis st(12, nullptr);
    st.SetStep(0, 2.);
    s.SetSpacing(0, { "2" }, {});
    const std::vector<double> a = s.AxisCoordinates(0, 0., 10.), b = st.AxisCoordinates(0, 0., 10.);
    CHECK(a.size() == b.size());
    for (std::size_t i = 0; i < a.size() && a.size() == b.size(); ++i)
      CHECK(std::abs(a[i] - b[i]) < 1e-9);
    const double mn3[3] = { 0, 0, 0 }, mx3[3] = { 10, 10, 10 };
    long long n[3];
    s.NbCellsOfGrid(mn3, mx3, n);
    CHECK(n[0] == 5 && n[2] == (long long)z.size() - 1);
  }

  // ---- persistence v2 round trip, all modes, function with spaces
  {
    CartesianGridPlugin_Hypothesis p(13, nullptr);
    p.SetCoordinates(0, { 0, 0.1 + 0.2, 10 });
    p.SetStep(1, 2.5);
    p.SetSpacing(2, { "0.5 + 0 * t", "2" }, { 0.5 });
    p.SetKeepOutsideCells(true);
    std::stringstream ss;
    p.SaveTo(ss);
    CartesianGridPlugin_Hypothesis q(14, nullptr);
    q.LoadFrom(ss);
    CHECK(!ss.fail());
    CHECK(q.GetMode(0) == CartesianGridPlugin_Hypothesis::COORDINATES);
    CHECK((q.GetCoordinates(0) == std::vector<double>{ 0, 0.1 + 0.2, 10 }));
    CHECK(q.GetMode(1) == CartesianGridPlugin_Hypothesis::STEP && q.GetStep(1) == 2.5);
    CHECK(q.GetMode(2) == CartesianGridPlugin_Hypothesis::SPACING);
    CHECK((q.GetSpacingFunctions(2) == std::vector<std::string>{ "0.5 + 0 * t", "2" }));
    CHECK((q.GetSpacingPoints(2) == std::vector<double>{ 0.5 }));
    CHECK(q.GetKeepOutsideCells());
  }

  // ---- version 1 string still loads
  {
    std::stringstream v1("1 0 4 1 1 10 3 0 10 1 1");
    CartesianGridPlugin_Hypothesis r(15, nullptr);
    r.LoadFrom(v1);
    CHECK(!v1.fail());
    CHECK(r.GetMode(0) == CartesianGridPlugin_Hypothesis::NB_CELLS && r.GetNbCells(0) == 4);
    CHECK(r.GetMode(1) == CartesianGridPlugin_Hypothesis::STEP && r.GetStep(1) == 3.);
    CHECK(r.GetKeepOutsideCells());
  }

  // ---- corrupted v2: truncated function, absurd sizes -> failbit, defaults kept
  {
    const char* bad[] = {
      "2 2 10 1 2 0 1 1 500 abc 0 0 10 1 2 0 1 1 1 1 0 0 10 1 2 0 1 1 1 1 0 0", // function length past data
      "2 2 10 1 999999999999 0",                                                // absurd coordinate count
      "2 4 10 1 2 0 1 1 1 1 0 0 10 1 2 0 1 1 1 1 0 0 10 1 2 0 1 1 1 1 0 0",     // unknown mode 4
      "2 2 10 1 2 1 0 1 1 1 0 0 10 1 2 0 1 1 1 1 0 0 10 1 2 0 1 1 1 1 0 0",     // unsorted coordinates
      "2 3 10 1 2 0 1 2 1 1 1 1 0 0 10 1 2 0 1 1 1 1 0 0 10 1 2 0 1 1 1 1 0 0", // 2 functions, 0 points
    };
    for (const char* text : bad)
    {
      std::stringstream b(text);
      CartesianGridPlugin_Hypothesis d(16, nullptr);
      d.LoadFrom(b);
      CHECK(b.fail());
      CHECK(d.GetMode(0) == CartesianGridPlugin_Hypothesis::NB_CELLS && d.GetNbCells(0) == 10);
    }
  }

  // ---- corrupted v2 announcing 1e8 coordinates: fails without allocating them (800 MB)
  {
    rusage before, after;
    getrusage(RUSAGE_SELF, &before);
    std::stringstream b("2 2 10 1 100000000 0");
    CartesianGridPlugin_Hypothesis d(17, nullptr);
    d.LoadFrom(b);
    getrusage(RUSAGE_SELF, &after);
    CHECK(b.fail());
    CHECK(after.ru_maxrss - before.ru_maxrss < 100 * 1024); // kB
  }

  std::cout << (nbFailures ? "FAILED: " : "OK ") << nbFailures << " failure(s)\n";
  return nbFailures ? 1 : 0;
}
