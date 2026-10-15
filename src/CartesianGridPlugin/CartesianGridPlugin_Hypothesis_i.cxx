// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#include "CartesianGridPlugin_Hypothesis_i.hxx"

#include "CartesianGridPlugin_GridText.hxx"

#include "SMESH_Gen.hxx"
#include "SMESH_PythonDump.hxx"

#include <Utils_CorbaException.hxx>
#include <utilities.h>

CartesianGridPlugin_Hypothesis_i::CartesianGridPlugin_Hypothesis_i(PortableServer::POA_ptr thePOA,
                                                   ::SMESH_Gen*            theGenImpl)
  : SALOME::GenericObj_i(thePOA),
    SMESH_Hypothesis_i(thePOA)
{
  myBaseImpl = new ::CartesianGridPlugin_Hypothesis(theGenImpl->GetANewId(), theGenImpl);
}

CartesianGridPlugin_Hypothesis_i::~CartesianGridPlugin_Hypothesis_i()
{
}

::CartesianGridPlugin_Hypothesis* CartesianGridPlugin_Hypothesis_i::GetImpl()
{
  return (::CartesianGridPlugin_Hypothesis*)myBaseImpl;
}

void CartesianGridPlugin_Hypothesis_i::SetNbCells(CORBA::Short axis, CORBA::Long nb)
{
  try {
    GetImpl()->SetNbCells(axis, nb);
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  SMESH::TPythonDump() << _this() << ".SetNbCells( " << axis << ", " << nb << " )";
}

void CartesianGridPlugin_Hypothesis_i::SetStep(CORBA::Short axis, CORBA::Double step)
{
  try {
    GetImpl()->SetStep(axis, step);
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  SMESH::TPythonDump() << _this() << ".SetStep( " << axis << ", "
                       << CartesianGridPlugin_GridText::FormatNumber(step).c_str() << " )"; // full precision
}

CORBA::Short CartesianGridPlugin_Hypothesis_i::GetMode(CORBA::Short axis)
{
  try {
    return (CORBA::Short)GetImpl()->GetMode(axis);
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  return 0;
}

CORBA::Long CartesianGridPlugin_Hypothesis_i::GetNbCells(CORBA::Short axis)
{
  try {
    return GetImpl()->GetNbCells(axis);
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  return 0;
}

CORBA::Double CartesianGridPlugin_Hypothesis_i::GetStep(CORBA::Short axis)
{
  try {
    return GetImpl()->GetStep(axis);
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  return 0.;
}

void CartesianGridPlugin_Hypothesis_i::SetCoordinates(CORBA::Short axis, const CartesianGridPlugin::DoubleArray& coords)
{
  std::vector<double> c(coords.length());
  for (CORBA::ULong i = 0; i < coords.length(); ++i)
    c[i] = coords[i];
  try {
    GetImpl()->SetCoordinates(axis, c);
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  SMESH::TPythonDump dump;
  dump << _this() << ".SetCoordinates( " << axis << ", [ ";
  for (CORBA::ULong i = 0; i < coords.length(); ++i)
    dump << (i ? ", " : "") << CartesianGridPlugin_GridText::FormatNumber(coords[i]).c_str();
  dump << " ] )";
}

CartesianGridPlugin::DoubleArray* CartesianGridPlugin_Hypothesis_i::GetCoordinates(CORBA::Short axis)
{
  CartesianGridPlugin::DoubleArray_var coords = new CartesianGridPlugin::DoubleArray;
  try {
    const std::vector<double>& c = GetImpl()->GetCoordinates(axis);
    coords->length((CORBA::ULong)c.size());
    for (std::size_t i = 0; i < c.size(); ++i)
      coords[(CORBA::ULong)i] = c[i];
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  return coords._retn();
}

void CartesianGridPlugin_Hypothesis_i::SetSpacing(CORBA::Short axis, const CartesianGridPlugin::StringArray& functions,
                                          const CartesianGridPlugin::DoubleArray& internalPoints)
{
  std::vector<std::string> f;
  for (CORBA::ULong i = 0; i < functions.length(); ++i)
    f.push_back(functions[i].in());
  std::vector<double> p(internalPoints.length());
  for (CORBA::ULong i = 0; i < internalPoints.length(); ++i)
    p[i] = internalPoints[i];
  try {
    GetImpl()->SetSpacing(axis, f, p);
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  SMESH::TPythonDump dump;
  dump << _this() << ".SetSpacing( " << axis << ", [ ";
  for (CORBA::ULong i = 0; i < functions.length(); ++i)
    dump << (i ? ", " : "") << "'" << functions[i].in() << "'";
  dump << " ], [ ";
  for (CORBA::ULong i = 0; i < internalPoints.length(); ++i)
    dump << (i ? ", " : "") << CartesianGridPlugin_GridText::FormatNumber(internalPoints[i]).c_str();
  dump << " ] )";
}

void CartesianGridPlugin_Hypothesis_i::GetSpacing(CORBA::Short axis, CartesianGridPlugin::StringArray_out functions,
                                          CartesianGridPlugin::DoubleArray_out internalPoints)
{
  CartesianGridPlugin::StringArray_var f = new CartesianGridPlugin::StringArray;
  CartesianGridPlugin::DoubleArray_var p = new CartesianGridPlugin::DoubleArray;
  try {
    const std::vector<std::string>& funs = GetImpl()->GetSpacingFunctions(axis);
    const std::vector<double>& points = GetImpl()->GetSpacingPoints(axis);
    f->length((CORBA::ULong)funs.size());
    for (std::size_t i = 0; i < funs.size(); ++i)
      f[(CORBA::ULong)i] = CORBA::string_dup(funs[i].c_str());
    p->length((CORBA::ULong)points.size());
    for (std::size_t i = 0; i < points.size(); ++i)
      p[(CORBA::ULong)i] = points[i];
  }
  catch (SALOME_Exception& ex) {
    THROW_SALOME_CORBA_EXCEPTION(ex.what(), SALOME::BAD_PARAM);
  }
  functions = f._retn();
  internalPoints = p._retn();
}

void CartesianGridPlugin_Hypothesis_i::SetKeepOutsideCells(CORBA::Boolean keep)
{
  GetImpl()->SetKeepOutsideCells(keep);
  SMESH::TPythonDump() << _this() << ".SetKeepOutsideCells( " << bool(keep) << " )";
}

CORBA::Boolean CartesianGridPlugin_Hypothesis_i::GetKeepOutsideCells()
{
  return GetImpl()->GetKeepOutsideCells();
}

CORBA::Boolean CartesianGridPlugin_Hypothesis_i::IsDimSupported(SMESH::Dimension type)
{
  return type == SMESH::DIM_3D;
}
