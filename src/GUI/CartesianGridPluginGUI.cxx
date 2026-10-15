// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#include "CartesianGridPluginGUI_HypothesisCreator.h"

extern "C"
{
  CARTESIANGRIDPLUGIN_GUI_EXPORT
  SMESHGUI_GenericHypothesisCreator* GetHypothesisCreator(const QString& aHypType)
  {
    if (aHypType == "CartesianGridParameters")
      return new CartesianGridPluginGUI_HypothesisCreator(aHypType);
    return 0;
  }
}
