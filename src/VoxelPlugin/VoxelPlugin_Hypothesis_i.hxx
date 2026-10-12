// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#ifndef _VoxelPlugin_Hypothesis_i_HXX_
#define _VoxelPlugin_Hypothesis_i_HXX_

#include "VoxelPlugin_Defs.hxx"

#include <SALOMEconfig.h>
#include CORBA_SERVER_HEADER(VoxelPlugin_Algorithm)

#include "SMESH_Hypothesis_i.hxx"
#include "VoxelPlugin_Hypothesis.hxx"

class SMESH_Gen;

class VOXELPLUGIN_EXPORT VoxelPlugin_Hypothesis_i:
  public virtual POA_VoxelPlugin::VoxelPlugin_Hypothesis,
  public virtual SMESH_Hypothesis_i
{
public:
  VoxelPlugin_Hypothesis_i(PortableServer::POA_ptr thePOA, ::SMESH_Gen* theGenImpl);
  virtual ~VoxelPlugin_Hypothesis_i();

  void          SetNbCells(CORBA::Short axis, CORBA::Long nb);
  void          SetStep(CORBA::Short axis, CORBA::Double step);
  CORBA::Short  GetMode(CORBA::Short axis);
  CORBA::Long   GetNbCells(CORBA::Short axis);
  CORBA::Double GetStep(CORBA::Short axis);

  void           SetKeepOutsideCells(CORBA::Boolean keep);
  CORBA::Boolean GetKeepOutsideCells();

  ::VoxelPlugin_Hypothesis* GetImpl();

  CORBA::Boolean IsDimSupported(SMESH::Dimension type);

  // no geometry parameter
  virtual bool getObjectsDependOn(std::vector<std::string>&, std::vector<int>&) const { return false; }
  virtual bool setObjectsDependOn(std::vector<std::string>&, std::vector<int>&) { return true; }
};

#endif
