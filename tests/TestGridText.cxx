// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
//
// Coordinate checks, text format and GUI editing helpers of the grid modes Coordinates and Spacing

#include "CartesianGridPlugin_Grid.hxx"
#include "CartesianGridPlugin_GridEdit.hxx"
#include "CartesianGridPlugin_GridText.hxx"

#include <clocale>
#include <cmath>
#include <iostream>
#include <limits>

static int nbFailures = 0;
#define CHECK(cond)                                                     \
  do { if (!(cond)) { ++nbFailures;                                     \
      std::cerr << __FILE__ << ":" << __LINE__ << " FAILED: " #cond "\n"; } } while (0)

int main()
{
  using namespace CartesianGridPlugin_GridText;
  std::string err;

  // SortAndCheckCoordinates
  std::vector<double> c = { 10, 0, 5, 2 };
  CHECK(CartesianGridPlugin_Grid::SortAndCheckCoordinates(c, err));
  CHECK((c == std::vector<double>{ 0, 2, 5, 10 }));
  c = { 1 };
  CHECK(!CartesianGridPlugin_Grid::SortAndCheckCoordinates(c, err) && !err.empty());
  c = { 0, 1, 1 };
  CHECK(!CartesianGridPlugin_Grid::SortAndCheckCoordinates(c, err));
  c = { 3, 3 };
  CHECK(!CartesianGridPlugin_Grid::SortAndCheckCoordinates(c, err));
  c = { 0, std::numeric_limits<double>::quiet_NaN() };
  CHECK(!CartesianGridPlugin_Grid::SortAndCheckCoordinates(c, err));
  c = { 0, std::numeric_limits<double>::infinity() };
  CHECK(!CartesianGridPlugin_Grid::SortAndCheckCoordinates(c, err));
  c = { 0, 1e-13, 1 }; // closer than 1e-12 x extent
  CHECK(!CartesianGridPlugin_Grid::SortAndCheckCoordinates(c, err));

  // ParseNumbers: any count, separated by spaces, tabs, new lines, commas or semicolons
  CHECK(ParseNumbers(" 0 1,2.5\t, 10 ;\r\n12\n", c, err));
  CHECK((c == std::vector<double>{ 0, 1, 2.5, 10, 12 }));
  CHECK(ParseNumbers("-1e3", c, err) && c.size() == 1 && c[0] == -1000.);
  // FormatNumbers: copied list read back exactly
  const std::vector<double> copied = { -100, 0.1, 1. / 3., 2.5e-7 };
  CHECK(FormatNumbers(copied) == "-100 0.1 0.3333333333333333 2.5e-07");
  CHECK(ParseNumbers(FormatNumbers(copied), c, err) && c == copied);
  CHECK(ParseNumbers("+4", c, err) && c[0] == 4.);
  CHECK(ParseNumbers("", c, err) && c.empty());
  CHECK(!ParseNumbers("0 1 abc", c, err) && err.find("abc") != std::string::npos);
  CHECK(!ParseNumbers("0 1x", c, err));
  CHECK(!ParseNumbers("0 nan", c, err));
  CHECK(!ParseNumbers("0 inf", c, err));

  // FormatNumber: shortest text reading back exactly
  CHECK(FormatNumber(2.5) == "2.5" && FormatNumber(10) == "10" && FormatNumber(-0.25) == "-0.25");
  double v = 0;
  CHECK(ParseNumber(FormatNumber(0.1 + 0.2), v) && v == 0.1 + 0.2);

  // ---- GUI editing helpers
  {
    using namespace CartesianGridPlugin_GridEdit;
    // coordinates: insert selected value + step, kept sorted; delete keeps 2 values
    std::vector<double> x = { 0, 5, 10 };
    CHECK(InsertCoordinate(x, 0, 1.) == 1 && (x == std::vector<double>{ 0, 1, 5, 10 }));
    CHECK(InsertCoordinate(x, 3, 2.5) == 4 && x.back() == 12.5);
    CHECK(InsertCoordinate(x, 1, 20.) == 5 && x.back() == 21.);  // re-sorted: 1 + 20 goes last
    CHECK(InsertCoordinate(x, 1, 4.) == -1 && x.size() == 6);    // 1 + 4 = 5 exists already
    CHECK(InsertCoordinate(x, -1, 1.) == 6 && x.back() == 22.);  // no selection: after the last
    CHECK(InsertCoordinate(x, 0, std::numeric_limits<double>::quiet_NaN()) == -1);
    x = { 0, 5, 10 };
    CHECK(DeleteCoordinate(x, 1) && (x == std::vector<double>{ 0, 10 }));
    CHECK(!DeleteCoordinate(x, 0) && x.size() == 2);             // keeps 2 values
    CHECK(!DeleteCoordinate(x, 7));
    x = { 0, 1, 2, 3, 4 };
    CHECK(DeleteCoordinates(x, { 3, 1, 3 }) && (x == std::vector<double>{ 0, 2, 4 }));
    CHECK(!DeleteCoordinates(x, { 0, 1 }) && x.size() == 3);     // keeps 2 values
    CHECK(!DeleteCoordinates(x, { 0, 5 }) && x.size() == 3);     // out of range: nothing removed
    CHECK(!DeleteCoordinates(x, {}));
    CHECK(IsDefaultCoordinates({ 0, 1 }) && !IsDefaultCoordinates({ 0, 2 }));

    // spacing ranges: functions f[i] on [t(i), t(i+1)], t = 0, points..., 1
    std::vector<std::string> fn = { "1" };
    std::vector<double> pt;
    CHECK(SplitRange(fn, pt, 0) == 1);
    CHECK((fn == std::vector<std::string>{ "1", "1" }) && (pt == std::vector<double>{ 0.5 }));
    fn[1] = "2";
    CHECK(SplitRange(fn, pt, 1) == 2);                           // [0.5, 1] split at 0.75
    CHECK((fn == std::vector<std::string>{ "1", "2", "2" }) && (pt == std::vector<double>{ 0.5, 0.75 }));
    CHECK(RangeStart(pt, 0) == 0. && RangeEnd(pt, 0) == 0.5 && RangeEnd(pt, 2) == 1.);
    // merge keeps the selected range's function, with the next range (previous for the last)
    fn = { "a", "b", "c" };
    pt = { 0.5, 0.75 };
    CHECK(MergeRange(fn, pt, 0) && (fn == std::vector<std::string>{ "a", "c" }) &&
          (pt == std::vector<double>{ 0.75 }));
    CHECK(MergeRange(fn, pt, 1) && (fn == std::vector<std::string>{ "c" }) && pt.empty());
    CHECK(!MergeRange(fn, pt, 0));                                // keeps 1 range
    // too small range is not split (SMESH needs internal points 1e-3 apart)
    fn = { "1", "1" };
    pt = { 0.0015 };
    CHECK(SplitRange(fn, pt, 0) == -1 && fn.size() == 2);
    // end of a range: strictly inside its neighbours, 1e-3 apart; last range always ends at 1
    fn = { "a", "b", "c" };
    pt = { 0.5, 0.75 };
    CHECK(SetRangeEnd(pt, 0, 0.6, err) && pt[0] == 0.6);
    CHECK(!SetRangeEnd(pt, 0, 0.75, err) && !err.empty() && pt[0] == 0.6);
    CHECK(!SetRangeEnd(pt, 1, 0.6, err));
    CHECK(!SetRangeEnd(pt, 1, 1., err));
    CHECK(!SetRangeEnd(pt, 2, 0.9, err));                         // last range
    CHECK(SetRangeEnd(pt, 1, 0.9, err) && pt[1] == 0.9);
  }

  // decimal point whatever the locale (the GUI runs under the user's locale, e.g. fr_FR)
  if (std::setlocale(LC_NUMERIC, "fr_FR.UTF-8") || std::setlocale(LC_NUMERIC, "fr_FR.utf8"))
  {
    CHECK(ParseNumbers("0 2.5 10", c, err) && c.size() == 3 && c[1] == 2.5);
    CHECK(ParseNumber("0.5", v) && v == 0.5 && !ParseNumber("0,5", v));
    CHECK(FormatNumber(0.5) == "0.5");
    std::setlocale(LC_NUMERIC, "C");
  }
  else
    std::cout << "fr_FR locale not available, locale checks skipped\n";

  std::cout << (nbFailures ? "FAILED: " : "OK ") << nbFailures << " failure(s)\n";
  return nbFailures ? 1 : 0;
}
