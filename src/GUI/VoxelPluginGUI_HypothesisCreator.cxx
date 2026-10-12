// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#include "VoxelPluginGUI_HypothesisCreator.h"

#include "VoxelPlugin_Classifier.hxx"
#include "VoxelPlugin_Grid.hxx"

#include <GEOMBase.h>
#include <SMESHGUI_SpinBox.h>
#include <SMESHGUI_Utils.h>
#include <SUIT_ResourceMgr.h>
#include <SUIT_Session.h>
#include <SalomeApp_IntSpinBox.h>

#include <SALOMEconfig.h>
#include CORBA_SERVER_HEADER(VoxelPlugin_Algorithm)
#include CORBA_SERVER_HEADER(GEOM_Gen)

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QStackedWidget>
#include <QVBoxLayout>

namespace
{
  const char* axisNames[3] = { "X", "Y", "Z" };
}

VoxelPluginGUI_HypothesisCreator::VoxelPluginGUI_HypothesisCreator(const QString& theHypType)
  : SMESHGUI_GenericHypothesisCreator(theHypType), myName(0), myOutside(0), myHasExtent(false)
{
  for (int a = 0; a < 3; ++a)
    myExtent[a] = 0.;
}

QFrame* VoxelPluginGUI_HypothesisCreator::buildFrame()
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
    g->addWidget(myName, row++, 1, 1, 3);
  }

  g->addWidget(new QLabel(tr("VOXEL_MODE"), gb), row, 1);
  g->addWidget(new QLabel(tr("VOXEL_VALUE"), gb), row, 2);
  g->addWidget(new QLabel(tr("VOXEL_ACTUAL_STEP"), gb), row++, 3);

  for (int a = 0; a < 3; ++a, ++row)
  {
    g->addWidget(new QLabel(axisNames[a], gb), row, 0);
    myMode[a] = new QComboBox(gb);
    myMode[a]->addItems(QStringList() << tr("VOXEL_NB_CELLS") << tr("VOXEL_STEP"));
    g->addWidget(myMode[a], row, 1);

    myValueStack[a] = new QStackedWidget(gb);
    myNbCells[a] = new SalomeApp_IntSpinBox(gb);
    myNbCells[a]->setRange(1, 100000);
    myStep[a] = new SMESHGUI_SpinBox(gb);
    myStep[a]->RangeStepAndValidator(1e-12, 1e12, 1., "length_precision");
    myValueStack[a]->addWidget(myNbCells[a]);
    myValueStack[a]->addWidget(myStep[a]);
    g->addWidget(myValueStack[a], row, 2);

    myActualStep[a] = new QLabel(gb);
    g->addWidget(myActualStep[a], row, 3);

    connect(myMode[a], SIGNAL(currentIndexChanged(int)), myValueStack[a], SLOT(setCurrentIndex(int)));
    connect(myMode[a], SIGNAL(currentIndexChanged(int)), this, SLOT(updateActualSteps()));
    connect(myNbCells[a], SIGNAL(valueChanged(int)), this, SLOT(updateActualSteps()));
    connect(myStep[a], SIGNAL(valueChanged(double)), this, SLOT(updateActualSteps()));
  }

  g->addWidget(new QLabel(tr("VOXEL_OUTSIDE_CELLS"), gb), row, 0, 1, 2);
  myOutside = new QComboBox(gb);
  myOutside->addItems(QStringList() << tr("VOXEL_REMOVE") << tr("VOXEL_KEEP"));
  g->addWidget(myOutside, row++, 2, 1, 2);

  // extent of the mesh shape, to show the actual step
  QString entry = getMainShapeEntry();
  if (entry.isEmpty())
    entry = getShapeEntry();
  if (!entry.isEmpty())
  {
    GEOM::GEOM_Object_var geomObj = SMESH::EntryToInterface<GEOM::GEOM_Object>(entry);
    TopoDS_Shape shape;
    double mn[3], mx[3];
    if (GEOMBase::GetShape(geomObj, shape) && !shape.IsNull() &&
        VoxelPlugin_Classifier::BoundingBox(shape, mn, mx))
    {
      myHasExtent = true;
      for (int a = 0; a < 3; ++a)
        myExtent[a] = mx[a] - mn[a];
    }
  }
  return fr;
}

void VoxelPluginGUI_HypothesisCreator::updateActualSteps()
{
  for (int a = 0; a < 3; ++a)
  {
    if (!myHasExtent)
    {
      myActualStep[a]->setText("-");
      continue;
    }
    const int n = myMode[a]->currentIndex() == 1
      ? VoxelPlugin_Grid::NbCellsForStep(myExtent[a], myStep[a]->GetValue())
      : myNbCells[a]->value();
    myActualStep[a]->setText(QString("%1 (%2)").arg(myExtent[a] / n, 0, 'g', 6).arg(n));
  }
}

void VoxelPluginGUI_HypothesisCreator::retrieveParams() const
{
  VoxelPlugin::VoxelPlugin_Hypothesis_var h =
    VoxelPlugin::VoxelPlugin_Hypothesis::_narrow(initParamsHypothesis());
  if (myName)
    myName->setText(hypName());
  for (int a = 0; a < 3; ++a)
  {
    const int mode = h->GetMode(a);
    myNbCells[a]->setValue(h->GetNbCells(a));
    myStep[a]->SetValue(h->GetStep(a));
    myMode[a]->setCurrentIndex(mode);
    myValueStack[a]->setCurrentIndex(mode);
  }
  myOutside->setCurrentIndex(h->GetKeepOutsideCells() ? 1 : 0);
  const_cast<VoxelPluginGUI_HypothesisCreator*>(this)->updateActualSteps();
}

QString VoxelPluginGUI_HypothesisCreator::storeParams() const
{
  VoxelPlugin::VoxelPlugin_Hypothesis_var h =
    VoxelPlugin::VoxelPlugin_Hypothesis::_narrow(hypothesis());
  if (isCreation() && myName)
    SMESH::SetName(SMESH::FindSObject(h), myName->text().toUtf8().constData());
  QString text;
  for (int a = 0; a < 3; ++a)
  {
    if (myMode[a]->currentIndex() == 1)
    {
      h->SetStep(a, myStep[a]->GetValue());
      text += QString("%1: step %2; ").arg(axisNames[a]).arg(myStep[a]->GetValue());
    }
    else
    {
      h->SetNbCells(a, myNbCells[a]->value());
      text += QString("%1: %2 cells; ").arg(axisNames[a]).arg(myNbCells[a]->value());
    }
  }
  h->SetKeepOutsideCells(myOutside->currentIndex() == 1);
  return text;
}

bool VoxelPluginGUI_HypothesisCreator::checkParams(QString& msg) const
{
  for (int a = 0; a < 3; ++a)
    if (myMode[a]->currentIndex() == 1 && !(myStep[a]->GetValue() > 0.))
    {
      msg = tr("VOXEL_BAD_STEP");
      return false;
    }
  return true;
}

QString VoxelPluginGUI_HypothesisCreator::caption() const
{
  return tr("VOXEL_GRID_TITLE");
}

QPixmap VoxelPluginGUI_HypothesisCreator::icon() const
{
  return SUIT_Session::session()->resourceMgr()->loadPixmap("VoxelPlugin", tr("ICON_DLG_VOXEL_GRID"));
}

QString VoxelPluginGUI_HypothesisCreator::type() const
{
  return tr("VOXEL_GRID_HYPOTHESIS");
}

QString VoxelPluginGUI_HypothesisCreator::helpPage() const
{
  return "voxelplugin_hypo_page.html";
}
