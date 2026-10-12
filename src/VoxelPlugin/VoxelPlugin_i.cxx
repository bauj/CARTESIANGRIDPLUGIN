// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#include "VoxelPlugin_Hypothesis_i.hxx"

#include <cstring>

template <class T> class VoxelPlugin_Creator_i : public HypothesisCreator_i<T>
{
  // as we have 'module VoxelPlugin' in VoxelPlugin_Algorithm.idl
  virtual std::string GetModuleName() { return "VoxelPlugin"; }
};

extern "C"
{
  VOXELPLUGIN_EXPORT
  GenericHypothesisCreator_i* GetHypothesisCreator(const char* aHypName)
  {
    GenericHypothesisCreator_i* aCreator = 0;
    if (strcmp(aHypName, "VoxelGrid") == 0)
      aCreator = new VoxelPlugin_Creator_i<VoxelPlugin_Hypothesis_i>;
    return aCreator;
  }
}
