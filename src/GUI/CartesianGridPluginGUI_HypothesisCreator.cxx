// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#include "CartesianGridPluginGUI_HypothesisCreator.h"

#include "CartesianGridPluginGUI_GridPanel.h"

#include <SMESHGUI_Utils.h>
#include <SUIT_ResourceMgr.h>
#include <SUIT_Session.h>

#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>

CartesianGridPluginGUI_HypothesisCreator::CartesianGridPluginGUI_HypothesisCreator(const QString& theHypType)
  : SMESHGUI_GenericHypothesisCreator(theHypType), myName(0), myPanel(0)
{
}

QFrame* CartesianGridPluginGUI_HypothesisCreator::buildFrame()
{
  QFrame* fr = new QFrame(0);
  QVBoxLayout* lay = new QVBoxLayout(fr);
  lay->setContentsMargins(0, 0, 0, 0);

  QGroupBox* gb = new QGroupBox(tr("SMESH_ARGUMENTS"), fr);
  QGridLayout* g = new QGridLayout(gb);
  lay->addWidget(gb);

  int row = 0;
  if (isCreation())
  {
    g->addWidget(new QLabel(tr("SMESH_NAME"), gb), row, 0);
    myName = new QLineEdit(gb);
    g->addWidget(myName, row++, 1);
  }

  myPanel = new CartesianGridPluginGUI_GridPanel(gb);
  g->addWidget(myPanel, row++, 0, 1, 2);

  // extent of the mesh shape, to show the actual step
  QString entry = getMainShapeEntry();
  if (entry.isEmpty())
    entry = getShapeEntry();
  myPanel->SetShape(entry);
  return fr;
}

void CartesianGridPluginGUI_HypothesisCreator::retrieveParams() const
{
  CartesianGridPlugin::CartesianGridPlugin_Hypothesis_var h =
    CartesianGridPlugin::CartesianGridPlugin_Hypothesis::_narrow(initParamsHypothesis());
  if (myName)
    myName->setText(hypName());
  myPanel->Load(h);
}

QString CartesianGridPluginGUI_HypothesisCreator::storeParams() const
{
  CartesianGridPlugin::CartesianGridPlugin_Hypothesis_var h =
    CartesianGridPlugin::CartesianGridPlugin_Hypothesis::_narrow(hypothesis());
  if (isCreation() && myName)
    SMESH::SetName(SMESH::FindSObject(h), myName->text().toUtf8().constData());
  return myPanel->Store(h);
}

bool CartesianGridPluginGUI_HypothesisCreator::checkParams(QString& msg) const
{
  return myPanel->Check(msg);
}

QString CartesianGridPluginGUI_HypothesisCreator::caption() const
{
  return tr("CARTESIANGRID_GRID_TITLE");
}

QPixmap CartesianGridPluginGUI_HypothesisCreator::icon() const
{
  return SUIT_Session::session()->resourceMgr()->loadPixmap("CartesianGridPlugin", tr("ICON_DLG_CARTESIAN_GRID"));
}

QString CartesianGridPluginGUI_HypothesisCreator::type() const
{
  return tr("CARTESIANGRID_GRID_HYPOTHESIS");
}

QString CartesianGridPluginGUI_HypothesisCreator::helpPage() const
{
  return "cartesiangridplugin_hypo_page.html";
}
