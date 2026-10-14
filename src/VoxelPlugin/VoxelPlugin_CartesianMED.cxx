// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later

#include "VoxelPlugin_CartesianMED.hxx"

#include <InterpKernelException.hxx>
#include <MEDCouplingCMesh.hxx>
#include <MEDCouplingMemArray.hxx>
#include <MEDFileMesh.hxx>

#include <algorithm>
#include <cstdio>
#include <set>
#include <stdexcept>

using namespace MEDCoupling;

namespace
{
  const std::size_t theMaxName = 64; // MED_NAME_SIZE: mesh and family names
}

void VoxelPlugin_CartesianMED::Write(const std::string& theFile, const std::string& theMeshName,
                                     const std::vector<double> theCoords[3],
                                     const std::vector<int>& theZoneOfCell,
                                     const std::vector<std::string>& theZoneNames)
{
  std::size_t nbCells = 1;
  for (int a = 0; a < 3; ++a)
  {
    if (theCoords[a].size() < 2)
      throw std::invalid_argument("A Cartesian grid needs at least 2 coordinates per axis");
    nbCells *= theCoords[a].size() - 1;
  }
  if (theZoneOfCell.size() != nbCells)
    throw std::invalid_argument("Zone count does not match the number of cells");
  const int nbZones = (int)theZoneNames.size();
  if (std::any_of(theZoneOfCell.begin(), theZoneOfCell.end(),
                  [nbZones](int z) { return z < 0 || z > nbZones; }))
    throw std::invalid_argument("Cell zone out of range");
  std::set<std::string> names;
  for (const std::string& name : theZoneNames)
  {
    if (name.empty() || 4 + name.size() > theMaxName)
      throw std::invalid_argument("Zone group name empty or longer than 60 characters: " + name);
    if (!names.insert(name).second)
      throw std::invalid_argument("Duplicate zone group name: " + name);
  }

  // written to a temporary file renamed at the end: a failure leaves an existing file untouched
  const std::string tmpFile = theFile + ".tmp";
  try
  {
    MCAuto<MEDCouplingCMesh> cmesh = MEDCouplingCMesh::New(theMeshName.substr(0, theMaxName));
    MCAuto<DataArrayDouble> x[3];
    for (int a = 0; a < 3; ++a)
    {
      x[a] = DataArrayDouble::New();
      x[a]->alloc((mcIdType)theCoords[a].size(), 1);
      std::copy(theCoords[a].begin(), theCoords[a].end(), x[a]->getPointer());
      x[a]->setInfoOnComponent(0, std::string(1, "XYZ"[a]));
    }
    cmesh->setCoords(x[0], x[1], x[2]);

    MCAuto<MEDFileCMesh> fmesh = MEDFileCMesh::New();
    fmesh->setMesh(cmesh);
    MCAuto<DataArrayIdType> fam = DataArrayIdType::New();
    fam->alloc((mcIdType)nbCells, 1);
    std::transform(theZoneOfCell.begin(), theZoneOfCell.end(), fam->getPointer(),
                   [](int z) { return -(mcIdType)z; });
    fmesh->setFamilyFieldArr(0, fam);
    fmesh->addFamily("FAMILLE_ZERO", 0);
    for (int z = 0; z < nbZones; ++z)
    {
      const std::string family = "FAM_" + theZoneNames[z];
      fmesh->addFamily(family, -(z + 1));
      fmesh->setGroupsOnFamily(family, { theZoneNames[z] });
    }
    fmesh->write33(tmpFile, 2);
  }
  catch (const INTERP_KERNEL::Exception& ex)
  {
    std::remove(tmpFile.c_str());
    throw std::runtime_error(std::string("MED export failed: ") + ex.what());
  }
  if (std::rename(tmpFile.c_str(), theFile.c_str()) != 0)
  {
    std::remove(tmpFile.c_str());
    throw std::runtime_error("MED export failed: cannot write " + theFile);
  }
}
