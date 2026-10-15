// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
// C entry points of the grid panel for the Cartesian Grid Wizard (Python, ctypes + sip.wrapinstance).
// Return 1 on success, 0 with a message in theErr; no exception crosses them.

#include "CartesianGridPluginGUI_GridPanel.h"

#include <SALOME_Exception.hh>
#include <SMESHGUI_Utils.h>
#include <Standard_Failure.hxx>

#include <cstring>
#include <exception>
#include <stdexcept>
#include <string>

namespace
{
  void copyText(char* theBuf, int theLen, const std::string& theText)
  {
    if (theBuf && theLen > 0)
    {
      std::strncpy(theBuf, theText.c_str(), theLen - 1);
      theBuf[theLen - 1] = '\0';
    }
  }

  CartesianGridPlugin::CartesianGridPlugin_Hypothesis_var hypothesis(const char* theIOR)
  {
    CartesianGridPlugin::CartesianGridPlugin_Hypothesis_var h =
      SMESH::IORToInterface<CartesianGridPlugin::CartesianGridPlugin_Hypothesis>(QString(theIOR));
    if (h->_is_nil())
      throw std::runtime_error("not a CartesianGridParameters hypothesis");
    return h;
  }

  //! runs theWork, converting any exception to an error message
  template <class Work>
  int guarded(char* theErr, int theLen, Work theWork)
  {
    try
    {
      return theWork() ? 1 : 0;
    }
    catch (const SALOME::SALOME_Exception& e)
    {
      copyText(theErr, theLen, e.details.text.in());
    }
    catch (const Standard_Failure& e)
    {
      copyText(theErr, theLen, std::string("OCC error: ") + e.GetMessageString());
    }
    catch (const CORBA::Exception& e)
    {
      copyText(theErr, theLen, std::string("CORBA error: ") + e._name());
    }
    catch (const std::exception& e)
    {
      copyText(theErr, theLen, e.what());
    }
    catch (...)
    {
      copyText(theErr, theLen, "unknown error");
    }
    return 0;
  }
}

extern "C"
{
  void* CartesianGridPanel_New(void* theParent)
  {
    try
    {
      return new CartesianGridPluginGUI_GridPanel(static_cast<QWidget*>(theParent));
    }
    catch (...)
    {
      return 0;
    }
  }

  int CartesianGridPanel_SetShape(void* thePanel, const char* theEntry, char* theErr, int theLen)
  {
    return guarded(theErr, theLen, [&]() {
      if (static_cast<CartesianGridPluginGUI_GridPanel*>(thePanel)->SetShape(QString(theEntry)))
        return true;
      copyText(theErr, theLen, std::string("cannot read the shape of ") + theEntry);
      return false;
    });
  }

  int CartesianGridPanel_Load(void* thePanel, const char* theIOR, char* theErr, int theLen)
  {
    return guarded(theErr, theLen, [&]() {
      static_cast<CartesianGridPluginGUI_GridPanel*>(thePanel)->Load(hypothesis(theIOR));
      return true;
    });
  }

  int CartesianGridPanel_Check(void* thePanel, char* theErr, int theLen)
  {
    return guarded(theErr, theLen, [&]() {
      QString msg;
      if (static_cast<CartesianGridPluginGUI_GridPanel*>(thePanel)->Check(msg))
        return true;
      copyText(theErr, theLen, msg.toStdString());
      return false;
    });
  }

  //! theSummary receives the grid summary on success
  int CartesianGridPanel_Store(void* thePanel, const char* theIOR, char* theSummary, int theSummaryLen,
                               char* theErr, int theLen)
  {
    return guarded(theErr, theLen, [&]() {
      QString text = static_cast<CartesianGridPluginGUI_GridPanel*>(thePanel)->Store(hypothesis(theIOR));
      copyText(theSummary, theSummaryLen, text.toStdString());
      return true;
    });
  }
}
