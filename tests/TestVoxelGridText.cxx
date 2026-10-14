// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
//
// Coordinate checks and text format of the grid modes Coordinates and Spacing

#include "VoxelPlugin_Grid.hxx"
#include "VoxelPlugin_GridText.hxx"

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
  using namespace VoxelPlugin_GridText;
  std::string err;

  // SortAndCheckCoordinates
  std::vector<double> c = { 10, 0, 5, 2 };
  CHECK(VoxelPlugin_Grid::SortAndCheckCoordinates(c, err));
  CHECK((c == std::vector<double>{ 0, 2, 5, 10 }));
  c = { 1 };
  CHECK(!VoxelPlugin_Grid::SortAndCheckCoordinates(c, err) && !err.empty());
  c = { 0, 1, 1 };
  CHECK(!VoxelPlugin_Grid::SortAndCheckCoordinates(c, err));
  c = { 3, 3 };
  CHECK(!VoxelPlugin_Grid::SortAndCheckCoordinates(c, err));
  c = { 0, std::numeric_limits<double>::quiet_NaN() };
  CHECK(!VoxelPlugin_Grid::SortAndCheckCoordinates(c, err));
  c = { 0, std::numeric_limits<double>::infinity() };
  CHECK(!VoxelPlugin_Grid::SortAndCheckCoordinates(c, err));
  c = { 0, 1e-13, 1 }; // closer than 1e-12 x extent
  CHECK(!VoxelPlugin_Grid::SortAndCheckCoordinates(c, err));

  // ParseCoordinates: spaces, commas, tabs; order kept (sorting is the hypothesis' job)
  CHECK(ParseCoordinates(" 0 1,2.5\t, 10 ", c, err));
  CHECK((c == std::vector<double>{ 0, 1, 2.5, 10 }));
  CHECK(ParseCoordinates("-1e3 4", c, err) && c[0] == -1000.);
  CHECK(!ParseCoordinates("0 1 abc", c, err) && err.find("abc") != std::string::npos);
  CHECK(!ParseCoordinates("0 1x", c, err));
  CHECK(!ParseCoordinates("0 nan", c, err));
  CHECK(!ParseCoordinates("0 inf", c, err));
  CHECK(!ParseCoordinates("5", c, err));
  CHECK(!ParseCoordinates("", c, err));

  // FormatCoordinates round trip, shortest form when exact
  CHECK(FormatCoordinates({ 0, 2.5, 10 }) == "0 2.5 10");
  CHECK(ParseCoordinates(FormatCoordinates({ 0.1 + 0.2, 1 }), c, err) && c[0] == 0.1 + 0.2);

  // ParseSpacing: f1 ; t1 ; f2
  std::vector<std::string> f;
  std::vector<double> p;
  CHECK(ParseSpacing(" 0.2 ; 0.5 ; 0.2 + t ", f, p, err));
  CHECK((f == std::vector<std::string>{ "0.2", "0.2 + t" }) && (p == std::vector<double>{ 0.5 }));
  CHECK(ParseSpacing("1", f, p, err) && f.size() == 1 && p.empty());
  CHECK(!ParseSpacing("", f, p, err));
  CHECK(!ParseSpacing("1 ; 0.5", f, p, err));         // even number of fields
  CHECK(!ParseSpacing("1 ; x ; 2", f, p, err));       // bad point
  CHECK(!ParseSpacing("1 ;  ; 2", f, p, err));        // empty field
  CHECK(FormatSpacing({ "0.2", "0.2 + t" }, { 0.5 }) == "0.2 ; 0.5 ; 0.2 + t");
  CHECK(FormatSpacing({ "1" }, {}) == "1");

  // decimal point whatever the locale (the GUI runs under the user's locale, e.g. fr_FR)
  if (std::setlocale(LC_NUMERIC, "fr_FR.UTF-8") || std::setlocale(LC_NUMERIC, "fr_FR.utf8"))
  {
    CHECK(ParseCoordinates("0 2.5 10", c, err) && c.size() == 3 && c[1] == 2.5);
    CHECK(FormatCoordinates({ 0, 2.5, 10 }) == "0 2.5 10");
    CHECK(ParseSpacing("1 ; 0.5 ; 2", f, p, err) && p.size() == 1 && p[0] == 0.5);
    CHECK(FormatSpacing({ "1", "2" }, { 0.5 }) == "1 ; 0.5 ; 2");
    std::setlocale(LC_NUMERIC, "C");
  }
  else
    std::cout << "fr_FR locale not available, locale checks skipped\n";

  std::cout << (nbFailures ? "FAILED: " : "OK ") << nbFailures << " failure(s)\n";
  return nbFailures ? 1 : 0;
}
