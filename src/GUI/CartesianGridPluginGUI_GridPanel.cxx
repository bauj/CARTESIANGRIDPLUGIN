// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#include "CartesianGridPluginGUI_GridPanel.h"

#include "CartesianGridPluginGUI_AxisTab.h"
#include "CartesianGridPlugin_Classifier.hxx"
#include "CartesianGridPlugin_Hypothesis.hxx"

#include <GEOMBase.h>
#include <SMESHGUI_Utils.h>

#include CORBA_SERVER_HEADER(GEOM_Gen)

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QTabWidget>

#include <string>
#include <vector>

namespace
{
  const char* axisNames[3] = { "X", "Y", "Z" };
}

CartesianGridPluginGUI_GridPanel::CartesianGridPluginGUI_GridPanel(QWidget* theParent)
  : QFrame(theParent)
{
  QGridLayout* g = new QGridLayout(this);
  g->setContentsMargins(0, 0, 0, 0);

  myTabWidget = new QTabWidget(this);
  for (int a = 0; a < 3; ++a)
  {
    myTabs[a] = new CartesianGridPluginGUI_AxisTab(myTabWidget, a);
    myTabWidget->addTab(myTabs[a], axisNames[a]);
  }
  g->addWidget(myTabWidget, 0, 0, 1, 2);

  g->addWidget(new QLabel(tr("CARTESIANGRID_OUTSIDE_CELLS"), this), 1, 0);
  myOutside = new QComboBox(this);
  myOutside->addItems(QStringList() << tr("CARTESIANGRID_REMOVE") << tr("CARTESIANGRID_KEEP"));
  g->addWidget(myOutside, 1, 1);
}

bool CartesianGridPluginGUI_GridPanel::SetShape(const QString& theEntry)
{
  if (!theEntry.isEmpty())
  {
    GEOM::GEOM_Object_var geomObj = SMESH::EntryToInterface<GEOM::GEOM_Object>(theEntry);
    TopoDS_Shape shape;
    double mn[3], mx[3];
    if (!geomObj->_is_nil() && GEOMBase::GetShape(geomObj, shape) && !shape.IsNull() &&
        CartesianGridPlugin_Classifier::BoundingBox(shape, mn, mx))
    {
      for (int a = 0; a < 3; ++a)
        myTabs[a]->SetExtent(true, mx[a] - mn[a]);
      return true;
    }
  }
  for (int a = 0; a < 3; ++a)
    myTabs[a]->SetExtent(false, 0.);
  return false;
}

void CartesianGridPluginGUI_GridPanel::Load(CartesianGridPlugin::CartesianGridPlugin_Hypothesis_ptr h)
{
  for (int a = 0; a < 3; ++a)
  {
    myTabs[a]->SetNbCells(h->GetNbCells(a));
    myTabs[a]->SetStep(h->GetStep(a));
    CartesianGridPlugin::DoubleArray_var coords = h->GetCoordinates(a);
    std::vector<double> c(coords->length());
    for (CORBA::ULong i = 0; i < coords->length(); ++i)
      c[i] = coords[i];
    myTabs[a]->SetCoordinates(c);
    CartesianGridPlugin::StringArray_var funs;
    CartesianGridPlugin::DoubleArray_var points;
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

bool CartesianGridPluginGUI_GridPanel::Check(QString& msg) const
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

QString CartesianGridPluginGUI_GridPanel::Store(CartesianGridPlugin::CartesianGridPlugin_Hypothesis_ptr h) const
{
  QString text;
  for (int a = 0; a < 3; ++a)
  {
    const CartesianGridPluginGUI_AxisTab* tab = myTabs[a];
    switch (tab->Mode())
    {
    case CartesianGridPlugin_Hypothesis::COORDINATES:
    {
      const std::vector<double>& c = tab->Coordinates();
      CartesianGridPlugin::DoubleArray_var coords = new CartesianGridPlugin::DoubleArray;
      coords->length((CORBA::ULong)c.size());
      for (std::size_t i = 0; i < c.size(); ++i)
        coords[(CORBA::ULong)i] = c[i];
      h->SetCoordinates(a, coords);
      text += QString("%1: %2 coordinates; ").arg(axisNames[a]).arg(c.size());
      break;
    }
    case CartesianGridPlugin_Hypothesis::SPACING:
    {
      const std::vector<std::string>& f = tab->SpacingFunctions();
      const std::vector<double>& p = tab->SpacingPoints();
      CartesianGridPlugin::StringArray_var funs = new CartesianGridPlugin::StringArray;
      funs->length((CORBA::ULong)f.size());
      for (std::size_t i = 0; i < f.size(); ++i)
        funs[(CORBA::ULong)i] = CORBA::string_dup(f[i].c_str());
      CartesianGridPlugin::DoubleArray_var points = new CartesianGridPlugin::DoubleArray;
      points->length((CORBA::ULong)p.size());
      for (std::size_t i = 0; i < p.size(); ++i)
        points[(CORBA::ULong)i] = p[i];
      h->SetSpacing(a, funs, points);
      text += QString("%1: spacing, %2 range(s); ").arg(axisNames[a]).arg(f.size());
      break;
    }
    case CartesianGridPlugin_Hypothesis::STEP:
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
