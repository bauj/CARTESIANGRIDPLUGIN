// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#include "CartesianGridPlugin_Hypothesis_i.hxx"
#include "CartesianGridPlugin_CartesianGrid_3D_i.hxx"

#include <cstring>

template <class T> class CartesianGridPlugin_Creator_i : public HypothesisCreator_i<T>
{
  // as we have 'module CartesianGridPlugin' in CartesianGridPlugin_Algorithm.idl
  virtual std::string GetModuleName() { return "CartesianGridPlugin"; }
};

extern "C"
{
  CARTESIANGRIDPLUGIN_EXPORT
  GenericHypothesisCreator_i* GetHypothesisCreator(const char* aHypName)
  {
    GenericHypothesisCreator_i* aCreator = 0;
    if (strcmp(aHypName, "CartesianGrid_3D") == 0)
      aCreator = new CartesianGridPlugin_Creator_i<CartesianGridPlugin_CartesianGrid_3D_i>;
    else if (strcmp(aHypName, "CartesianGridParameters") == 0)
      aCreator = new CartesianGridPlugin_Creator_i<CartesianGridPlugin_Hypothesis_i>;
    return aCreator;
  }
}
