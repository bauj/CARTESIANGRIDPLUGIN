// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#include "VoxelPlugin_Hypothesis_i.hxx"

#include "SMESH_Gen.hxx"
#include "SMESH_PythonDump.hxx"

#include <Utils_CorbaException.hxx>
#include <utilities.h>

VoxelPlugin_Hypothesis_i::VoxelPlugin_Hypothesis_i(PortableServer::POA_ptr thePOA,
                                                   ::SMESH_Gen*            theGenImpl)
  : SALOME::GenericObj_i(thePOA),
    SMESH_Hypothesis_i(thePOA)
{
  myBaseImpl = new ::VoxelPlugin_Hypothesis(theGenImpl->GetANewId(), theGenImpl);
}

VoxelPlugin_Hypothesis_i::~VoxelPlugin_Hypothesis_i()
{
}

::VoxelPlugin_Hypothesis* VoxelPlugin_Hypothesis_i::GetImpl()
{
  return (::VoxelPlugin_Hypothesis*)myBaseImpl;
}

void VoxelPlugin_Hypothesis_i::SetNbCells(CORBA::Short axis, CORBA::Long nb)
{
  try {
    GetImpl()->SetNbCells(axis, nb);
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  SMESH::TPythonDump() << _this() << ".SetNbCells( " << axis << ", " << nb << " )";
}

void VoxelPlugin_Hypothesis_i::SetStep(CORBA::Short axis, CORBA::Double step)
{
  try {
    GetImpl()->SetStep(axis, step);
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  SMESH::TPythonDump() << _this() << ".SetStep( " << axis << ", " << step << " )";
}

CORBA::Short VoxelPlugin_Hypothesis_i::GetMode(CORBA::Short axis)
{
  try {
    return (CORBA::Short)GetImpl()->GetMode(axis);
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  return 0;
}

CORBA::Long VoxelPlugin_Hypothesis_i::GetNbCells(CORBA::Short axis)
{
  try {
    return GetImpl()->GetNbCells(axis);
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  return 0;
}

CORBA::Double VoxelPlugin_Hypothesis_i::GetStep(CORBA::Short axis)
{
  try {
    return GetImpl()->GetStep(axis);
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  return 0.;
}

void VoxelPlugin_Hypothesis_i::SetKeepOutsideCells(CORBA::Boolean keep)
{
  GetImpl()->SetKeepOutsideCells(keep);
  SMESH::TPythonDump() << _this() << ".SetKeepOutsideCells( " << bool(keep) << " )";
}

CORBA::Boolean VoxelPlugin_Hypothesis_i::GetKeepOutsideCells()
{
  return GetImpl()->GetKeepOutsideCells();
}

CORBA::Boolean VoxelPlugin_Hypothesis_i::IsDimSupported(SMESH::Dimension type)
{
  return type == SMESH::DIM_3D;
}
