// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#ifndef _VoxelPlugin_Voxel_3D_i_HXX_
#define _VoxelPlugin_Voxel_3D_i_HXX_

#include "VoxelPlugin_Defs.hxx"

#include <SALOMEconfig.h>
#include CORBA_SERVER_HEADER(VoxelPlugin_Algorithm)

#include "SMESH_3D_Algo_i.hxx"
#include "VoxelPlugin_Voxel_3D.hxx"

class VOXELPLUGIN_EXPORT VoxelPlugin_Voxel_3D_i:
  public virtual POA_VoxelPlugin::VoxelPlugin_Voxel_3D,
  public virtual SMESH_3D_Algo_i
{
public:
  VoxelPlugin_Voxel_3D_i(PortableServer::POA_ptr thePOA, ::SMESH_Gen* theGenImpl);
  virtual ~VoxelPlugin_Voxel_3D_i();

  ::VoxelPlugin_Voxel_3D* GetImpl();

  char* ExportCartesianMED(SMESH::SMESH_Mesh_ptr mesh, const char* file, CORBA::Boolean fromMesh,
                           const VoxelPlugin::ExportZones& zones);
  CORBA::Double GetExportProgress();
  void CancelExport();
};

#endif
