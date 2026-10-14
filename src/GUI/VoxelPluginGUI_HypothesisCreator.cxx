// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#include "VoxelPluginGUI_HypothesisCreator.h"

#include "VoxelPluginGUI_AxisTab.h"
#include "VoxelPlugin_Classifier.hxx"
#include "VoxelPlugin_Hypothesis.hxx"

#include <GEOMBase.h>
#include <SMESHGUI_Utils.h>
#include <SUIT_ResourceMgr.h>
#include <SUIT_Session.h>

#include <SALOMEconfig.h>
#include CORBA_SERVER_HEADER(VoxelPlugin_Algorithm)
#include CORBA_SERVER_HEADER(GEOM_Gen)

#include <QComboBox>
#include <QFrame>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QLineEdit>
#include <QTabWidget>
#include <QVBoxLayout>

#include <string>
#include <vector>

namespace
{
  const char* axisNames[3] = { "X", "Y", "Z" };
}

VoxelPluginGUI_HypothesisCreator::VoxelPluginGUI_HypothesisCreator(const QString& theHypType)
  : SMESHGUI_GenericHypothesisCreator(theHypType), myName(0), myTabWidget(0), myOutside(0)
{
  for (int a = 0; a < 3; ++a)
    myTabs[a] = 0;
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
    g->addWidget(myName, row++, 1);
  }

  myTabWidget = new QTabWidget(gb);
  for (int a = 0; a < 3; ++a)
  {
    myTabs[a] = new VoxelPluginGUI_AxisTab(myTabWidget, a);
    myTabWidget->addTab(myTabs[a], axisNames[a]);
  }
  g->addWidget(myTabWidget, row++, 0, 1, 2);

  g->addWidget(new QLabel(tr("VOXEL_OUTSIDE_CELLS"), gb), row, 0);
  myOutside = new QComboBox(gb);
  myOutside->addItems(QStringList() << tr("VOXEL_REMOVE") << tr("VOXEL_KEEP"));
  g->addWidget(myOutside, row++, 1);

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
      for (int a = 0; a < 3; ++a)
        myTabs[a]->SetExtent(true, mx[a] - mn[a]);
    }
  }
  return fr;
}

void VoxelPluginGUI_HypothesisCreator::retrieveParams() const
{
  VoxelPlugin::VoxelPlugin_Hypothesis_var h =
    VoxelPlugin::VoxelPlugin_Hypothesis::_narrow(initParamsHypothesis());
  if (myName)
    myName->setText(hypName());
  for (int a = 0; a < 3; ++a)
  {
    myTabs[a]->SetNbCells(h->GetNbCells(a));
    myTabs[a]->SetStep(h->GetStep(a));
    VoxelPlugin::DoubleArray_var coords = h->GetCoordinates(a);
    std::vector<double> c(coords->length());
    for (CORBA::ULong i = 0; i < coords->length(); ++i)
      c[i] = coords[i];
    myTabs[a]->SetCoordinates(c);
    VoxelPlugin::StringArray_var funs;
    VoxelPlugin::DoubleArray_var points;
    h->GetSpacing(a, funs.out(), points.out());
    std::vector<std::string> f;
    for (CORBA::ULong i = 0; i < funs->length(); ++i)
      f.push_back(funs[i].in());
    std::vector<double> p(points->length());
    for (CORBA::ULong i = 0; i < points->length(); ++i)
      p[i] = points[i];
    myTabs[a]->SetSpacing(f, p);
    myTabs[a]->SetMode(h->GetMode(a));
  }
  myOutside->setCurrentIndex(h->GetKeepOutsideCells() ? 1 : 0);
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
    const VoxelPluginGUI_AxisTab* tab = myTabs[a];
    switch (tab->Mode())
    {
    case VoxelPlugin_Hypothesis::COORDINATES:
    {
      const std::vector<double>& c = tab->Coordinates();
      VoxelPlugin::DoubleArray_var coords = new VoxelPlugin::DoubleArray;
      coords->length((CORBA::ULong)c.size());
      for (std::size_t i = 0; i < c.size(); ++i)
        coords[(CORBA::ULong)i] = c[i];
      h->SetCoordinates(a, coords);
      text += QString("%1: %2 coordinates; ").arg(axisNames[a]).arg(c.size());
      break;
    }
    case VoxelPlugin_Hypothesis::SPACING:
    {
      const std::vector<std::string>& f = tab->SpacingFunctions();
      const std::vector<double>& p = tab->SpacingPoints();
      VoxelPlugin::StringArray_var funs = new VoxelPlugin::StringArray;
      funs->length((CORBA::ULong)f.size());
      for (std::size_t i = 0; i < f.size(); ++i)
        funs[(CORBA::ULong)i] = CORBA::string_dup(f[i].c_str());
      VoxelPlugin::DoubleArray_var points = new VoxelPlugin::DoubleArray;
      points->length((CORBA::ULong)p.size());
      for (std::size_t i = 0; i < p.size(); ++i)
        points[(CORBA::ULong)i] = p[i];
      h->SetSpacing(a, funs, points);
      text += QString("%1: spacing, %2 range(s); ").arg(axisNames[a]).arg(f.size());
      break;
    }
    case VoxelPlugin_Hypothesis::STEP:
      h->SetStep(a, tab->Step());
      text += QString("%1: step %2; ").arg(axisNames[a]).arg(tab->Step());
      break;
    default:
      h->SetNbCells(a, tab->NbCells());
      text += QString("%1: %2 cells; ").arg(axisNames[a]).arg(tab->NbCells());
    }
  }
  h->SetKeepOutsideCells(myOutside->currentIndex() == 1);
  return text;
}

bool VoxelPluginGUI_HypothesisCreator::checkParams(QString& msg) const
{
  for (int a = 0; a < 3; ++a)
  {
    std::string error;
    if (!myTabs[a]->Check(error))
    {
      msg = QString("%1: %2").arg(axisNames[a]).arg(QString::fromStdString(error));
      myTabWidget->setCurrentIndex(a);
      return false;
    }
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
