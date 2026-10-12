// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#ifndef VOXELPLUGINGUI_HypothesisCreator_HeaderFile
#define VOXELPLUGINGUI_HypothesisCreator_HeaderFile

#ifdef WIN32
  #define VOXELPLUGIN_GUI_EXPORT __declspec(dllexport)
#else
  #define VOXELPLUGIN_GUI_EXPORT
#endif

#include <SMESHGUI_Hypotheses.h>

class QComboBox;
class QLabel;
class QLineEdit;
class SMESHGUI_SpinBox;
class SalomeApp_IntSpinBox;
class QStackedWidget;

/*!
 * \brief Dialog of the VoxelGrid hypothesis: per axis mode (Nb cells / Step), value,
 *        actual step for the mesh shape; outside cells policy
 */
class VOXELPLUGIN_GUI_EXPORT VoxelPluginGUI_HypothesisCreator : public SMESHGUI_GenericHypothesisCreator
{
  Q_OBJECT

public:
  VoxelPluginGUI_HypothesisCreator(const QString& theHypType);

  virtual bool    checkParams(QString& msg) const;
  virtual QString helpPage() const;

protected:
  virtual QFrame*  buildFrame();
  virtual void     retrieveParams() const;
  virtual QString  storeParams() const;
  virtual QString  caption() const;
  virtual QPixmap  icon() const;
  virtual QString  type() const;

private slots:
  void updateActualSteps();

private:
  QLineEdit*            myName;
  QComboBox*            myMode[3];
  QStackedWidget*       myValueStack[3];
  SalomeApp_IntSpinBox* myNbCells[3];
  SMESHGUI_SpinBox*     myStep[3];
  QLabel*               myActualStep[3];
  QComboBox*            myOutside;
  bool                  myHasExtent;
  double                myExtent[3];
};

#endif
