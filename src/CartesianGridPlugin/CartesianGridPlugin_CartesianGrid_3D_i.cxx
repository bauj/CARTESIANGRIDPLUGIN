// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#include "CartesianGridPlugin_CartesianGrid_3D_i.hxx"

#include "SMESH_Gen.hxx"
#include "SMESH_Gen_i.hxx"
#include "SMESH_Mesh_i.hxx"

#include <SALOMEDS_wrap.hxx>
#include <Utils_CorbaException.hxx>

#include "SMESH_TryCatch.hxx"   // after the OCC headers (OCC_CATCH_SIGNALS)

CartesianGridPlugin_CartesianGrid_3D_i::CartesianGridPlugin_CartesianGrid_3D_i(PortableServer::POA_ptr thePOA,
                                               ::SMESH_Gen*            theGenImpl)
  : SALOME::GenericObj_i(thePOA),
    SMESH_Hypothesis_i(thePOA),
    SMESH_Algo_i(thePOA),
    SMESH_3D_Algo_i(thePOA)
{
  myBaseImpl = new ::CartesianGridPlugin_CartesianGrid_3D(theGenImpl->GetANewId(), theGenImpl);
}

CartesianGridPlugin_CartesianGrid_3D_i::~CartesianGridPlugin_CartesianGrid_3D_i()
{
}

::CartesianGridPlugin_CartesianGrid_3D* CartesianGridPlugin_CartesianGrid_3D_i::GetImpl()
{
  return (::CartesianGridPlugin_CartesianGrid_3D*)myBaseImpl;
}

char* CartesianGridPlugin_CartesianGrid_3D_i::ExportCartesianMED(SMESH::SMESH_Mesh_ptr     mesh,
                                                 const char*               file,
                                                 const CartesianGridPlugin::ExportZones& zones)
{
  SMESH_Mesh_i* meshI = SMESH::DownCast<SMESH_Mesh_i*>(mesh);
  if (!meshI)
    THROW_SALOME_CORBA_EXCEPTION("Not a mesh of this session", SALOME::BAD_PARAM);
  meshI->Load();   // data of a mesh of a reopened study are loaded on demand
  std::string meshName = "Mesh";
  SALOMEDS::SObject_wrap so = SMESH_Gen_i::GetSMESHGen()->ObjectToSObject(mesh);
  if (!so->_is_nil())
  {
    CORBA::String_var name = so->GetName();
    meshName = name.in();
  }
  std::vector<CartesianGridPlugin_ExportZone> z(zones.length());
  for (CORBA::ULong i = 0; i < zones.length(); ++i)
  {
    z[i].name = zones[i].name.in();
    z[i].ids.assign(zones[i].ids.get_buffer(), zones[i].ids.get_buffer() + zones[i].ids.length());
  }
  SMESH_TRY;
  return CORBA::string_dup(GetImpl()->ExportCartesianMED(meshI->GetImpl(), file, meshName, z).c_str());
  SMESH_CATCH( SMESH::throwCorbaException );
  return nullptr;
}

CORBA::Double CartesianGridPlugin_CartesianGrid_3D_i::GetExportProgress() { return GetImpl()->ExportProgress(); }
void CartesianGridPlugin_CartesianGrid_3D_i::CancelExport() { GetImpl()->CancelExport(); }

CartesianGridPlugin::LongArray* CartesianGridPlugin_CartesianGrid_3D_i::GetSolidIds(SMESH::SMESH_Mesh_ptr mesh)
{
  SMESH_Mesh_i* meshI = SMESH::DownCast<SMESH_Mesh_i*>(mesh);
  if (!meshI)
    THROW_SALOME_CORBA_EXCEPTION("Not a mesh of this session", SALOME::BAD_PARAM);
  CartesianGridPlugin::LongArray_var result = new CartesianGridPlugin::LongArray();
  SMESH_TRY;
  meshI->Load();
  const std::vector<int> ids = ::CartesianGridPlugin_CartesianGrid_3D::SolidIds(meshI->GetImpl());
  result->length((CORBA::ULong)ids.size());
  for (std::size_t i = 0; i < ids.size(); ++i)
    result[(CORBA::ULong)i] = ids[i];
  SMESH_CATCH( SMESH::throwCorbaException );
  return result._retn();
}
