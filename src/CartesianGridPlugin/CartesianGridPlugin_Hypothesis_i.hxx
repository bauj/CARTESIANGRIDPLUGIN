// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#ifndef _CartesianGridPlugin_Hypothesis_i_HXX_
#define _CartesianGridPlugin_Hypothesis_i_HXX_

#include "CartesianGridPlugin_Defs.hxx"

#include <SALOMEconfig.h>
#include CORBA_SERVER_HEADER(CartesianGridPlugin_Algorithm)

#include "SMESH_Hypothesis_i.hxx"
#include "CartesianGridPlugin_Hypothesis.hxx"

class SMESH_Gen;

class CARTESIANGRIDPLUGIN_EXPORT CartesianGridPlugin_Hypothesis_i:
  public virtual POA_CartesianGridPlugin::CartesianGridPlugin_Hypothesis,
  public virtual SMESH_Hypothesis_i
{
public:
  CartesianGridPlugin_Hypothesis_i(PortableServer::POA_ptr thePOA, ::SMESH_Gen* theGenImpl);
  virtual ~CartesianGridPlugin_Hypothesis_i();

  void          SetNbCells(CORBA::Short axis, CORBA::Long nb);
  void          SetStep(CORBA::Short axis, CORBA::Double step);
  CORBA::Short  GetMode(CORBA::Short axis);
  CORBA::Long   GetNbCells(CORBA::Short axis);
  CORBA::Double GetStep(CORBA::Short axis);

  void                      SetCoordinates(CORBA::Short axis, const CartesianGridPlugin::DoubleArray& coords);
  CartesianGridPlugin::DoubleArray* GetCoordinates(CORBA::Short axis);
  void                      SetSpacing(CORBA::Short axis, const CartesianGridPlugin::StringArray& functions,
                                       const CartesianGridPlugin::DoubleArray& internalPoints);
  void                      GetSpacing(CORBA::Short axis, CartesianGridPlugin::StringArray_out functions,
                                       CartesianGridPlugin::DoubleArray_out internalPoints);

  void           SetKeepOutsideCells(CORBA::Boolean keep);
  CORBA::Boolean GetKeepOutsideCells();

  ::CartesianGridPlugin_Hypothesis* GetImpl();

  CORBA::Boolean IsDimSupported(SMESH::Dimension type);

  // no geometry parameter
  virtual bool getObjectsDependOn(std::vector<std::string>&, std::vector<int>&) const { return false; }
  virtual bool setObjectsDependOn(std::vector<std::string>&, std::vector<int>&) { return true; }
};

#endif
