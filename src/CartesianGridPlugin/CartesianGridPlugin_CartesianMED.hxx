// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#ifndef _CartesianGridPlugin_CartesianMED_HXX_
#define _CartesianGridPlugin_CartesianMED_HXX_

#include "CartesianGridPlugin_Defs.hxx"

#include <string>
#include <vector>

/*!
 * \brief MED file of a Cartesian grid whose cells carry named zones (MEDFileCMesh).
 */
namespace CartesianGridPlugin_CartesianMED
{
  //! Writes (overwrites) theFile: Cartesian mesh theMeshName with node coordinates theCoords
  //! (components X, Y, Z); cell c (CartesianGridPlugin_Grid::CellIndex order, i fastest) gets family
  //! -theZoneOfCell[c], family -z being "FAM_<name>" holding the group theZoneNames[z-1]; 0 = no
  //! zone. theMeshName is cut to the 64 characters MED allows. Throws std::invalid_argument on
  //! bad input, std::runtime_error when writing fails; an existing theFile is then left untouched.
  CARTESIANGRIDPLUGIN_EXPORT void Write(const std::string& theFile, const std::string& theMeshName,
                                const std::vector<double> theCoords[3],
                                const std::vector<int>& theZoneOfCell,
                                const std::vector<std::string>& theZoneNames);
}

#endif
