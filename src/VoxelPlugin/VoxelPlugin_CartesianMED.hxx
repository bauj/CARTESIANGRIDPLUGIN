// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#ifndef _VoxelPlugin_CartesianMED_HXX_
#define _VoxelPlugin_CartesianMED_HXX_

#include "VoxelPlugin_Defs.hxx"

#include <string>
#include <vector>

/*!
 * \brief MED file of a Cartesian grid whose cells carry named zones (MEDFileCMesh).
 */
namespace VoxelPlugin_CartesianMED
{
  //! Writes (overwrites) theFile: Cartesian mesh theMeshName with node coordinates theCoords
  //! (components X, Y, Z); cell c (VoxelPlugin_Grid::CellIndex order, i fastest) gets family
  //! -theZoneOfCell[c], family -z being "FAM_<name>" holding the group theZoneNames[z-1]; 0 = no
  //! zone. Throws std::invalid_argument on bad input (nothing written), std::runtime_error when
  //! MEDLoader fails.
  VOXELPLUGIN_EXPORT void Write(const std::string& theFile, const std::string& theMeshName,
                                const std::vector<double> theCoords[3],
                                const std::vector<int>& theZoneOfCell,
                                const std::vector<std::string>& theZoneNames);
}

#endif
