// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#include "VoxelPlugin_Voxel_3D_i.hxx"

#include "SMESH_Gen.hxx"

VoxelPlugin_Voxel_3D_i::VoxelPlugin_Voxel_3D_i(PortableServer::POA_ptr thePOA,
                                               ::SMESH_Gen*            theGenImpl)
  : SALOME::GenericObj_i(thePOA),
    SMESH_Hypothesis_i(thePOA),
    SMESH_Algo_i(thePOA),
    SMESH_3D_Algo_i(thePOA)
{
  myBaseImpl = new ::VoxelPlugin_Voxel_3D(theGenImpl->GetANewId(), theGenImpl);
}

VoxelPlugin_Voxel_3D_i::~VoxelPlugin_Voxel_3D_i()
{
}

::VoxelPlugin_Voxel_3D* VoxelPlugin_Voxel_3D_i::GetImpl()
{
  return (::VoxelPlugin_Voxel_3D*)myBaseImpl;
}
