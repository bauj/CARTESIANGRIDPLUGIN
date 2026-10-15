// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#ifndef CARTESIANGRIDPLUGINGUI_GridPanel_HeaderFile
#define CARTESIANGRIDPLUGINGUI_GridPanel_HeaderFile

#include <SALOMEconfig.h>
#include CORBA_CLIENT_HEADER(CartesianGridPlugin_Algorithm)

#include <QFrame>
#include <QString>

class QComboBox;
class QTabWidget;
class CartesianGridPluginGUI_AxisTab;

/*!
 * \brief Grid definition of the CartesianGridParameters hypothesis: one tab per axis, outside cells
 *        policy. Shared by the hypothesis dialog and the Cartesian Grid Wizard
 */
class CartesianGridPluginGUI_GridPanel : public QFrame
{
  Q_OBJECT

public:
  CartesianGridPluginGUI_GridPanel(QWidget* theParent);

  //! extents of the shape of the study object theEntry (GEOM or SHAPERSTUDY), for the actual cells;
  //! false if the shape cannot be read (extents cleared)
  bool SetShape(const QString& theEntry);

  void    Load(CartesianGridPlugin::CartesianGridPlugin_Hypothesis_ptr theHyp);
  //! true if every axis is valid, else false with theMsg and the faulty tab shown
  bool    Check(QString& theMsg) const;
  //! writes the panel to theHyp, returns a summary of the grid
  QString Store(CartesianGridPlugin::CartesianGridPlugin_Hypothesis_ptr theHyp) const;

private:
  QTabWidget*                     myTabWidget;
  CartesianGridPluginGUI_AxisTab* myTabs[3];
  QComboBox*                      myOutside;
};

#endif
