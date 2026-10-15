// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#ifndef _CartesianGridPlugin_CartesianGrid_3D_i_HXX_
#define _CartesianGridPlugin_CartesianGrid_3D_i_HXX_

#include "CartesianGridPlugin_Defs.hxx"

#include <SALOMEconfig.h>
#include CORBA_SERVER_HEADER(CartesianGridPlugin_Algorithm)

#include "SMESH_3D_Algo_i.hxx"
#include "CartesianGridPlugin_CartesianGrid_3D.hxx"

class CARTESIANGRIDPLUGIN_EXPORT CartesianGridPlugin_CartesianGrid_3D_i:
  public virtual POA_CartesianGridPlugin::CartesianGridPlugin_CartesianGrid_3D,
  public virtual SMESH_3D_Algo_i
{
public:
  CartesianGridPlugin_CartesianGrid_3D_i(PortableServer::POA_ptr thePOA, ::SMESH_Gen* theGenImpl);
  virtual ~CartesianGridPlugin_CartesianGrid_3D_i();

  ::CartesianGridPlugin_CartesianGrid_3D* GetImpl();

  char* ExportCartesianMED(SMESH::SMESH_Mesh_ptr mesh, const char* file, CORBA::Boolean fromMesh,
                           const CartesianGridPlugin::ExportZones& zones);
  CORBA::Double GetExportProgress();
  void CancelExport();
  CartesianGridPlugin::LongArray* GetSolidIds(SMESH::SMESH_Mesh_ptr mesh);
};

#endif
