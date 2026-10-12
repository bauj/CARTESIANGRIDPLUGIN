// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#include "VoxelPluginGUI_HypothesisCreator.h"

extern "C"
{
  VOXELPLUGIN_GUI_EXPORT
  SMESHGUI_GenericHypothesisCreator* GetHypothesisCreator(const QString& aHypType)
  {
    if (aHypType == "VoxelGrid")
      return new VoxelPluginGUI_HypothesisCreator(aHypType);
    return 0;
  }
}
