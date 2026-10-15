// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#ifndef CARTESIANGRIDPLUGINGUI_HypothesisCreator_HeaderFile
#define CARTESIANGRIDPLUGINGUI_HypothesisCreator_HeaderFile

#ifdef WIN32
  #define CARTESIANGRIDPLUGIN_GUI_EXPORT __declspec(dllexport)
#else
  #define CARTESIANGRIDPLUGIN_GUI_EXPORT
#endif

#include <SMESHGUI_Hypotheses.h>

class QComboBox;
class QLineEdit;
class QTabWidget;
class CartesianGridPluginGUI_AxisTab;

/*!
 * \brief Dialog of the CartesianGridParameters hypothesis: one tab per axis (mode, values, actual cells
 *        for the mesh shape), outside cells policy
 */
class CARTESIANGRIDPLUGIN_GUI_EXPORT CartesianGridPluginGUI_HypothesisCreator : public SMESHGUI_GenericHypothesisCreator
{
  Q_OBJECT

public:
  CartesianGridPluginGUI_HypothesisCreator(const QString& theHypType);

  virtual bool    checkParams(QString& msg) const;
  virtual QString helpPage() const;

protected:
  virtual QFrame*  buildFrame();
  virtual void     retrieveParams() const;
  virtual QString  storeParams() const;
  virtual QString  caption() const;
  virtual QPixmap  icon() const;
  virtual QString  type() const;

private:
  QLineEdit*              myName;
  QTabWidget*             myTabWidget;
  CartesianGridPluginGUI_AxisTab* myTabs[3];
  QComboBox*              myOutside;
};

#endif
