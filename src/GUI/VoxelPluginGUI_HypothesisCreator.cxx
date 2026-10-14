// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#include "VoxelPluginGUI_HypothesisCreator.h"

#include "VoxelPlugin_Classifier.hxx"
#include "VoxelPlugin_Grid.hxx"
#include "VoxelPlugin_GridText.hxx"
#include "VoxelPlugin_Hypothesis.hxx"

#include <Utils_SALOME_Exception.hxx>

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

#include <algorithm>

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
    myMode[a]->addItems(QStringList() << tr("VOXEL_NB_CELLS") << tr("VOXEL_STEP")
                                      << tr("VOXEL_COORDINATES") << tr("VOXEL_SPACING"));
    g->addWidget(myMode[a], row, 1);

    myValueStack[a] = new QStackedWidget(gb);
    myNbCells[a] = new SalomeApp_IntSpinBox(gb);
    myNbCells[a]->setRange(1, 100000);
    myStep[a] = new SMESHGUI_SpinBox(gb);
    myStep[a]->RangeStepAndValidator(1e-12, 1e12, 1., "length_precision");
    myCoords[a] = new QLineEdit(gb);
    myCoords[a]->setToolTip(tr("VOXEL_COORDINATES_TIP"));
    mySpacing[a] = new QLineEdit(gb);
    mySpacing[a]->setToolTip(tr("VOXEL_SPACING_TIP"));
    myValueStack[a]->addWidget(myNbCells[a]);
    myValueStack[a]->addWidget(myStep[a]);
    myValueStack[a]->addWidget(myCoords[a]);
    myValueStack[a]->addWidget(mySpacing[a]);
    g->addWidget(myValueStack[a], row, 2);

    myActualStep[a] = new QLabel(gb);
    g->addWidget(myActualStep[a], row, 3);

    connect(myMode[a], SIGNAL(currentIndexChanged(int)), myValueStack[a], SLOT(setCurrentIndex(int)));
    connect(myMode[a], SIGNAL(currentIndexChanged(int)), this, SLOT(updateActualSteps()));
    connect(myNbCells[a], SIGNAL(valueChanged(int)), this, SLOT(updateActualSteps()));
    connect(myStep[a], SIGNAL(valueChanged(double)), this, SLOT(updateActualSteps()));
    connect(myCoords[a], SIGNAL(textChanged(const QString&)), this, SLOT(updateActualSteps()));
    connect(mySpacing[a], SIGNAL(textChanged(const QString&)), this, SLOT(updateActualSteps()));
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

bool VoxelPluginGUI_HypothesisCreator::textCoordinates(int a, std::vector<double>& theCoords,
                                                       std::string& theError) const
{
  if (myMode[a]->currentIndex() == VoxelPlugin_Hypothesis::COORDINATES)
    return VoxelPlugin_GridText::ParseCoordinates(myCoords[a]->text().toStdString(), theCoords, theError) &&
           VoxelPlugin_Grid::SortAndCheckCoordinates(theCoords, theError);
  std::vector<std::string> functions;
  std::vector<double> points;
  if (!VoxelPlugin_GridText::ParseSpacing(mySpacing[a]->text().toStdString(), functions, points, theError))
    return false;
  try
  {
    theCoords = VoxelPlugin_Hypothesis::SpacingCoordinates(0., myHasExtent ? myExtent[a] : 1.,
                                                           functions, points, a);
  }
  catch (const SALOME_Exception& ex)
  {
    theError = ex.what();
    return false;
  }
  return true;
}

void VoxelPluginGUI_HypothesisCreator::updateActualSteps()
{
  for (int a = 0; a < 3; ++a)
  {
    const int mode = myMode[a]->currentIndex();
    if (mode >= VoxelPlugin_Hypothesis::COORDINATES)
    {
      std::vector<double> c;
      std::string error;
      if ((mode == VoxelPlugin_Hypothesis::SPACING && !myHasExtent) || !textCoordinates(a, c, error))
      {
        myActualStep[a]->setText("-");
        continue;
      }
      double dmin = c[1] - c[0], dmax = dmin;
      for (std::size_t i = 2; i < c.size(); ++i)
      {
        dmin = std::min(dmin, c[i] - c[i - 1]);
        dmax = std::max(dmax, c[i] - c[i - 1]);
      }
      myActualStep[a]->setText(QString("%1..%2 (%3)").arg(dmin, 0, 'g', 6).arg(dmax, 0, 'g', 6)
                                                     .arg(c.size() - 1));
      continue;
    }
    if (!myHasExtent)
    {
      myActualStep[a]->setText("-");
      continue;
    }
    const long long n = mode == VoxelPlugin_Hypothesis::STEP
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
    VoxelPlugin::DoubleArray_var coords = h->GetCoordinates(a);
    std::vector<double> c(coords->length());
    for (CORBA::ULong i = 0; i < coords->length(); ++i)
      c[i] = coords[i];
    myCoords[a]->setText(QString::fromStdString(VoxelPlugin_GridText::FormatCoordinates(c)));
    VoxelPlugin::StringArray_var funs;
    VoxelPlugin::DoubleArray_var points;
    h->GetSpacing(a, funs.out(), points.out());
    std::vector<std::string> f;
    for (CORBA::ULong i = 0; i < funs->length(); ++i)
      f.push_back(funs[i].in());
    std::vector<double> p(points->length());
    for (CORBA::ULong i = 0; i < points->length(); ++i)
      p[i] = points[i];
    mySpacing[a]->setText(QString::fromStdString(VoxelPlugin_GridText::FormatSpacing(f, p)));
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
    const int mode = myMode[a]->currentIndex();
    std::string error;
    if (mode == VoxelPlugin_Hypothesis::COORDINATES)
    {
      std::vector<double> c;
      VoxelPlugin_GridText::ParseCoordinates(myCoords[a]->text().toStdString(), c, error); // checked
      VoxelPlugin::DoubleArray_var coords = new VoxelPlugin::DoubleArray;
      coords->length((CORBA::ULong)c.size());
      for (std::size_t i = 0; i < c.size(); ++i)
        coords[(CORBA::ULong)i] = c[i];
      h->SetCoordinates(a, coords);
      text += QString("%1: %2 coordinates; ").arg(axisNames[a]).arg(c.size());
    }
    else if (mode == VoxelPlugin_Hypothesis::SPACING)
    {
      std::vector<std::string> f;
      std::vector<double> p;
      VoxelPlugin_GridText::ParseSpacing(mySpacing[a]->text().toStdString(), f, p, error); // checked
      VoxelPlugin::StringArray_var funs = new VoxelPlugin::StringArray;
      funs->length((CORBA::ULong)f.size());
      for (std::size_t i = 0; i < f.size(); ++i)
        funs[(CORBA::ULong)i] = CORBA::string_dup(f[i].c_str());
      VoxelPlugin::DoubleArray_var points = new VoxelPlugin::DoubleArray;
      points->length((CORBA::ULong)p.size());
      for (std::size_t i = 0; i < p.size(); ++i)
        points[(CORBA::ULong)i] = p[i];
      h->SetSpacing(a, funs, points);
      text += QString("%1: spacing %2; ").arg(axisNames[a]).arg(mySpacing[a]->text());
    }
    else if (mode == VoxelPlugin_Hypothesis::STEP)
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
  {
    const int mode = myMode[a]->currentIndex();
    if (mode == VoxelPlugin_Hypothesis::STEP && !(myStep[a]->GetValue() > 0.))
    {
      msg = tr("VOXEL_BAD_STEP");
      return false;
    }
    std::vector<double> c;
    std::string error;
    if (mode >= VoxelPlugin_Hypothesis::COORDINATES && !textCoordinates(a, c, error))
    {
      msg = QString("%1: %2").arg(axisNames[a]).arg(QString::fromStdString(error));
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
