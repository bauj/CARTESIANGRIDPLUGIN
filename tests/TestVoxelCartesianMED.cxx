// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
//
// MED Cartesian writer: coordinates, cell families and zone groups read back with MEDLoader

#include "VoxelPlugin_CartesianMED.hxx"

#include <MEDCouplingCMesh.hxx>
#include <MEDCouplingMemArray.hxx>
#include <MEDFileMesh.hxx>

#include <cstdio>
#include <iostream>
#include <stdexcept>

static int nbFailures = 0;
#define CHECK(cond)                                                     \
  do { if (!(cond)) { ++nbFailures;                                     \
      std::cerr << __FILE__ << ":" << __LINE__ << " FAILED: " #cond "\n"; } } while (0)

using namespace MEDCoupling;

int main()
{
  const std::string file = "TestVoxelCartesianMED.med";
  const std::vector<double> c[3] = { { 0, 1, 3, 6 }, { 0, 2, 5 }, { -1, 1, 2 } }; // 3 x 2 x 2
  std::vector<int> zone(12, 0);
  zone[0] = 1; zone[1] = 2; zone[5] = 2; zone[11] = 1;
  VoxelPlugin_CartesianMED::Write(file, "grid", c, zone, { "MAT_A", "MAT_B" });

  MCAuto<MEDFileMesh> m = MEDFileMesh::New(file);
  MEDFileCMesh* cm = dynamic_cast<MEDFileCMesh*>((MEDFileMesh*)m);
  CHECK(cm);
  if (cm)
  {
    CHECK(cm->getName() == "grid");
    const MEDCouplingCMesh* mesh = cm->getMesh();
    for (int a = 0; a < 3; ++a)
    {
      const DataArrayDouble* x = mesh->getCoordsAt(a);
      CHECK(x->getNumberOfTuples() == (mcIdType)c[a].size());
      CHECK(std::vector<double>(x->begin(), x->end()) == c[a]);
      CHECK(x->getInfoOnComponent(0) == std::string(1, "XYZ"[a]));
    }
    const DataArrayIdType* fam = cm->getFamilyFieldAtLevel(0);
    CHECK(fam && fam->getNumberOfTuples() == 12);
    if (fam)
    {
      const mcIdType* f = fam->begin();
      CHECK(f[0] == -1 && f[1] == -2 && f[5] == -2 && f[11] == -1 && f[2] == 0 && f[10] == 0);
    }
    CHECK((cm->getGroupsNames() == std::vector<std::string>{ "MAT_A", "MAT_B" }));
    CHECK((cm->getFamiliesOnGroup("MAT_A") == std::vector<std::string>{ "FAM_MAT_A" }));
    CHECK(cm->getFamilyId("FAM_MAT_B") == -2);
    MCAuto<DataArrayIdType> b = cm->getGroupArr(0, "MAT_B");
    CHECK(b->getNumberOfTuples() == 2 && b->getIJ(0, 0) == 1 && b->getIJ(1, 0) == 5);
  }

  // mesh name longer than MED allows (64 characters): truncated, file written
  VoxelPlugin_CartesianMED::Write(file, std::string(70, 'm'), c, zone, { "MAT_A", "MAT_B" });
  {
    MCAuto<MEDFileMesh> lm = MEDFileMesh::New(file);
    CHECK(lm->getName() == std::string(64, 'm'));
  }

  // bad input: nothing written, std::invalid_argument
  auto refused = [&](const std::vector<int>& z, const std::vector<std::string>& names)
  {
    try { VoxelPlugin_CartesianMED::Write("never.med", "g", c, z, names); }
    catch (const std::invalid_argument&) { return true; }
    return false;
  };
  CHECK(refused(std::vector<int>(5, 0), { "A" }));               // wrong number of cells
  CHECK(refused(std::vector<int>(12, 2), { "A" }));              // zone without name
  CHECK(refused(std::vector<int>(12, -1), { "A" }));             // negative zone
  CHECK(refused(std::vector<int>(12, 1), { std::string(61, 'x') })); // FAM_ + 61 > 64 chars
  CHECK(refused(std::vector<int>(12, 0), { "A", "A" }));         // duplicate names
  CHECK(std::fopen("never.med", "r") == nullptr);

  std::remove(file.c_str());
  std::cout << (nbFailures ? "FAILED: " : "OK ") << nbFailures << " failure(s)\n";
  return nbFailures ? 1 : 0;
}
