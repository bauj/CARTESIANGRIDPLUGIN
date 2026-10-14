// Copyright (C) 2026  VOXELPLUGIN authors
// LGPL 2.1 or later
#include "VoxelPluginGUI_AxisTab.h"

#include "VoxelPlugin_Grid.hxx"
#include "VoxelPlugin_GridEdit.hxx"
#include "VoxelPlugin_GridText.hxx"
#include "VoxelPlugin_Hypothesis.hxx"

#include <SMESHGUI_SpinBox.h>
#include <SalomeApp_IntSpinBox.h>
#include <Utils_SALOME_Exception.hxx>

#include <QApplication>
#include <QButtonGroup>
#include <QClipboard>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QListWidget>
#include <QPushButton>
#include <QRadioButton>
#include <QSignalBlocker>
#include <QStackedWidget>
#include <QStyledItemDelegate>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <algorithm>

namespace
{
  //! column without editor (range start in the range table)
  class ReadOnlyDelegate : public QStyledItemDelegate
  {
  public:
    using QStyledItemDelegate::QStyledItemDelegate;
    QWidget* createEditor(QWidget*, const QStyleOptionViewItem&, const QModelIndex&) const override
    {
      return nullptr;
    }
  };

  QString number(double v)
  {
    return QString::fromStdString(VoxelPlugin_GridText::FormatNumber(v));
  }

  enum { COL_FROM, COL_TO, COL_FUNCTION };
}

VoxelPluginGUI_AxisTab::VoxelPluginGUI_AxisTab(QWidget* theParent, int theAxis)
  : QFrame(theParent), myAxis(theAxis), myHasExtent(false), myExtent(0.),
    myCoords{ 0., 1. }, myFunctions{ "1" }
{
  // mode
  QGroupBox* modeBox = new QGroupBox(tr("VOXEL_MODE"), this);
  QGridLayout* modeLay = new QGridLayout(modeBox);
  myModeGroup = new QButtonGroup(this);
  const char* modeNames[4] = { "VOXEL_NB_CELLS", "VOXEL_STEP", "VOXEL_COORDINATES", "VOXEL_SPACING" };
  for (int m = 0; m < 4; ++m)
  {
    QRadioButton* btn = new QRadioButton(tr(modeNames[m]), modeBox);
    myModeGroup->addButton(btn, m);
    modeLay->addWidget(btn, m / 2, m % 2);
  }

  myStack = new QStackedWidget(this);

  // page NB_CELLS
  QWidget* nbPage = new QWidget(myStack);
  QHBoxLayout* nbLay = new QHBoxLayout(nbPage);
  nbLay->addWidget(new QLabel(tr("VOXEL_NB_CELLS"), nbPage));
  myNbCells = new SalomeApp_IntSpinBox(nbPage);
  myNbCells->setRange(1, 100000);
  nbLay->addWidget(myNbCells, 1);
  myStack->addWidget(nbPage);

  // page STEP
  QWidget* stepPage = new QWidget(myStack);
  QHBoxLayout* stepLay = new QHBoxLayout(stepPage);
  stepLay->addWidget(new QLabel(tr("VOXEL_STEP"), stepPage));
  myStep = new SMESHGUI_SpinBox(stepPage);
  myStep->RangeStepAndValidator(1e-12, 1e12, 1., "length_precision");
  stepLay->addWidget(myStep, 1);
  myStack->addWidget(stepPage);

  // page COORDINATES: list + Insert / Delete / Paste / insertion step
  QWidget* coordPage = new QWidget(myStack);
  QHBoxLayout* coordLay = new QHBoxLayout(coordPage);
  myCoordList = new QListWidget(coordPage);
  myCoordList->setToolTip(tr("VOXEL_COORDINATES_TIP"));
  coordLay->addWidget(myCoordList, 1);
  QVBoxLayout* coordBtnLay = new QVBoxLayout();
  myInsertCoordBtn = new QPushButton(tr("VOXEL_INSERT"), coordPage);
  myDeleteCoordBtn = new QPushButton(tr("VOXEL_DELETE"), coordPage);
  myPasteBtn = new QPushButton(tr("VOXEL_PASTE"), coordPage);
  myPasteBtn->setToolTip(tr("VOXEL_PASTE_TIP"));
  myInsertStep = new SMESHGUI_SpinBox(coordPage);
  myInsertStep->RangeStepAndValidator(-1e12, 1e12, 1., "length_precision");
  myInsertStep->SetValue(1.);
  coordBtnLay->addWidget(myInsertCoordBtn);
  coordBtnLay->addWidget(myDeleteCoordBtn);
  coordBtnLay->addWidget(myPasteBtn);
  coordBtnLay->addWidget(new QLabel(tr("VOXEL_INSERT_STEP"), coordPage));
  coordBtnLay->addWidget(myInsertStep);
  coordBtnLay->addStretch();
  coordLay->addLayout(coordBtnLay);
  myStack->addWidget(coordPage);

  // page SPACING: ranges of t and their function + Insert (split) / Delete (merge)
  QWidget* spacingPage = new QWidget(myStack);
  QHBoxLayout* spacingLay = new QHBoxLayout(spacingPage);
  myRangeTree = new QTreeWidget(spacingPage);
  myRangeTree->setColumnCount(3);
  myRangeTree->setHeaderLabels(QStringList() << tr("VOXEL_FROM_T") << tr("VOXEL_TO_T") << tr("VOXEL_FUNCTION"));
  myRangeTree->setRootIsDecorated(false);
  myRangeTree->setToolTip(tr("VOXEL_SPACING_TIP"));
  myRangeTree->setItemDelegateForColumn(COL_FROM, new ReadOnlyDelegate(myRangeTree));
  myRangeTree->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                               QAbstractItemView::SelectedClicked);
  myRangeTree->header()->setStretchLastSection(true);
  spacingLay->addWidget(myRangeTree, 1);
  QVBoxLayout* spacingBtnLay = new QVBoxLayout();
  mySplitBtn = new QPushButton(tr("VOXEL_INSERT"), spacingPage);
  mySplitBtn->setToolTip(tr("VOXEL_SPLIT_TIP"));
  myMergeBtn = new QPushButton(tr("VOXEL_DELETE"), spacingPage);
  myMergeBtn->setToolTip(tr("VOXEL_MERGE_TIP"));
  spacingBtnLay->addWidget(mySplitBtn);
  spacingBtnLay->addWidget(myMergeBtn);
  spacingBtnLay->addStretch();
  spacingLay->addLayout(spacingBtnLay);
  myStack->addWidget(spacingPage);

  myActual = new QLabel(this);
  myMessage = new QLabel(this);
  myMessage->setStyleSheet("color: red");
  myMessage->setWordWrap(true);

  QVBoxLayout* lay = new QVBoxLayout(this);
  lay->addWidget(modeBox);
  lay->addWidget(myStack, 1);
  lay->addWidget(myActual);
  lay->addWidget(myMessage);

  connect(myModeGroup, SIGNAL(buttonClicked(int)), this, SLOT(onMode(int)));
  connect(myNbCells, SIGNAL(valueChanged(int)), this, SLOT(updateActual()));
  connect(myStep, SIGNAL(valueChanged(double)), this, SLOT(updateActual()));
  connect(myInsertCoordBtn, SIGNAL(clicked()), this, SLOT(onInsertCoordinate()));
  connect(myDeleteCoordBtn, SIGNAL(clicked()), this, SLOT(onDeleteCoordinate()));
  connect(myPasteBtn, SIGNAL(clicked()), this, SLOT(onPasteCoordinates()));
  connect(myCoordList, SIGNAL(itemChanged(QListWidgetItem*)), this, SLOT(onCoordinateEdited(QListWidgetItem*)));
  connect(myCoordList, SIGNAL(itemSelectionChanged()), this, SLOT(updateButtons()));
  connect(mySplitBtn, SIGNAL(clicked()), this, SLOT(onSplitRange()));
  connect(myMergeBtn, SIGNAL(clicked()), this, SLOT(onMergeRange()));
  connect(myRangeTree, SIGNAL(itemChanged(QTreeWidgetItem*, int)), this, SLOT(onRangeEdited(QTreeWidgetItem*, int)));
  connect(myRangeTree, SIGNAL(itemSelectionChanged()), this, SLOT(updateButtons()));

  fillCoordinates(-1);
  fillRanges(-1);
  SetMode(VoxelPlugin_Hypothesis::NB_CELLS);
}

void VoxelPluginGUI_AxisTab::SetExtent(bool theHasExtent, double theExtent)
{
  myHasExtent = theHasExtent;
  myExtent = theExtent;
  updateActual();
}

int VoxelPluginGUI_AxisTab::Mode() const
{
  return myModeGroup->checkedId();
}

void VoxelPluginGUI_AxisTab::SetMode(int theMode)
{
  myModeGroup->button(theMode)->setChecked(true);
  onMode(theMode);
}

int VoxelPluginGUI_AxisTab::NbCells() const
{
  return myNbCells->value();
}

double VoxelPluginGUI_AxisTab::Step() const
{
  return myStep->GetValue();
}

void VoxelPluginGUI_AxisTab::SetNbCells(int theNb)
{
  myNbCells->setValue(theNb);
}

void VoxelPluginGUI_AxisTab::SetStep(double theStep)
{
  myStep->SetValue(theStep);
}

void VoxelPluginGUI_AxisTab::SetCoordinates(const std::vector<double>& theCoords)
{
  myCoords = theCoords;
  fillCoordinates(-1);
  updateActual();
}

void VoxelPluginGUI_AxisTab::SetSpacing(const std::vector<std::string>& theFunctions,
                                        const std::vector<double>&      thePoints)
{
  myFunctions = theFunctions;
  myPoints = thePoints;
  fillRanges(-1);
  updateActual();
}

bool VoxelPluginGUI_AxisTab::Check(std::string& theError) const
{
  switch (Mode())
  {
  case VoxelPlugin_Hypothesis::STEP:
    if (!(Step() > 0.))
    {
      theError = tr("VOXEL_BAD_STEP").toStdString();
      return false;
    }
    return true;
  case VoxelPlugin_Hypothesis::COORDINATES:
  {
    std::vector<double> c(myCoords);
    return VoxelPlugin_Grid::SortAndCheckCoordinates(c, theError);
  }
  case VoxelPlugin_Hypothesis::SPACING:
    try
    {
      VoxelPlugin_Hypothesis::SpacingCoordinates(0., 1., myFunctions, myPoints, myAxis);
    }
    catch (const SALOME_Exception& ex)
    {
      theError = ex.what();
      return false;
    }
    return true;
  default:
    return true;
  }
}

void VoxelPluginGUI_AxisTab::onMode(int theMode)
{
  myStack->setCurrentIndex(theMode);
  myMessage->clear();
  updateButtons();
  updateActual();
}

void VoxelPluginGUI_AxisTab::fillCoordinates(int theSelected)
{
  QSignalBlocker blocker(myCoordList);
  // same number of rows (value edited): items kept, as an edited item may still be in use
  if (myCoordList->count() != (int)myCoords.size())
  {
    myCoordList->clear();
    for (std::size_t i = 0; i < myCoords.size(); ++i)
      new QListWidgetItem(myCoordList);
  }
  for (int i = 0; i < myCoordList->count(); ++i)
  {
    QListWidgetItem* item = myCoordList->item(i);
    item->setText(number(myCoords[i]));
    item->setFlags(item->flags() | Qt::ItemIsEditable);
  }
  if (theSelected >= 0 && theSelected < myCoordList->count())
    myCoordList->setCurrentRow(theSelected);
  updateButtons();
}

void VoxelPluginGUI_AxisTab::fillRanges(int theSelected)
{
  QSignalBlocker blocker(myRangeTree);
  // same number of rows (value edited): items kept, as an edited item may still be in use
  if (myRangeTree->topLevelItemCount() != (int)myFunctions.size())
  {
    myRangeTree->clear();
    for (std::size_t i = 0; i < myFunctions.size(); ++i)
      new QTreeWidgetItem(myRangeTree);
  }
  for (int i = 0; i < (int)myFunctions.size(); ++i)
  {
    QTreeWidgetItem* item = myRangeTree->topLevelItem(i);
    item->setText(COL_FROM, number(VoxelPlugin_GridEdit::RangeStart(myPoints, i)));
    item->setText(COL_TO, number(VoxelPlugin_GridEdit::RangeEnd(myPoints, i)));
    item->setText(COL_FUNCTION, QString::fromStdString(myFunctions[i]));
    item->setFlags(item->flags() | Qt::ItemIsEditable);
  }
  if (theSelected >= 0 && theSelected < myRangeTree->topLevelItemCount())
    myRangeTree->setCurrentItem(myRangeTree->topLevelItem(theSelected));
  updateButtons();
}

void VoxelPluginGUI_AxisTab::onInsertCoordinate()
{
  const int index = VoxelPlugin_GridEdit::InsertCoordinate(myCoords, myCoordList->currentRow(),
                                                           myInsertStep->GetValue());
  if (index < 0)
    myMessage->setText(tr("VOXEL_COORDINATE_EXISTS"));
  else
    myMessage->clear();
  fillCoordinates(index);
  updateActual();
}

void VoxelPluginGUI_AxisTab::onDeleteCoordinate()
{
  const int row = myCoordList->currentRow();
  if (VoxelPlugin_GridEdit::DeleteCoordinate(myCoords, row))
  {
    myMessage->clear();
    fillCoordinates(std::min(row, (int)myCoords.size() - 1));
    updateActual();
  }
}

void VoxelPluginGUI_AxisTab::onPasteCoordinates()
{
  std::vector<double> pasted;
  std::string error;
  if (!VoxelPlugin_GridText::ParseNumbers(QApplication::clipboard()->text().toStdString(), pasted, error))
  {
    myMessage->setText(QString::fromStdString(error));
    return;
  }
  std::vector<double> c(myCoords);
  for (double v : pasted)
    if (std::find(c.begin(), c.end(), v) == c.end()) // values already in the list are skipped
      c.push_back(v);
  if (!VoxelPlugin_Grid::SortAndCheckCoordinates(c, error))
  {
    myMessage->setText(QString::fromStdString(error));
    return;
  }
  myMessage->clear();
  myCoords = c;
  fillCoordinates(-1);
  updateActual();
}

void VoxelPluginGUI_AxisTab::onCoordinateEdited(QListWidgetItem* theItem)
{
  const int row = myCoordList->row(theItem);
  double v;
  std::string error;
  std::vector<double> c(myCoords);
  if (!VoxelPlugin_GridText::ParseNumber(VoxelPlugin_GridText::Trim(theItem->text().toStdString()), v))
    error = tr("VOXEL_BAD_NUMBER").toStdString();
  else
  {
    c[row] = v;
    VoxelPlugin_Grid::SortAndCheckCoordinates(c, error);
  }
  if (!error.empty())
  {
    myMessage->setText(QString::fromStdString(error));
    fillCoordinates(row); // back to the previous value
    return;
  }
  myMessage->clear();
  myCoords = c;
  fillCoordinates((int)(std::find(myCoords.begin(), myCoords.end(), v) - myCoords.begin()));
  updateActual();
}

void VoxelPluginGUI_AxisTab::onSplitRange()
{
  QTreeWidgetItem* item = myRangeTree->currentItem();
  const int row = item ? myRangeTree->indexOfTopLevelItem(item) : 0;
  const int index = VoxelPlugin_GridEdit::SplitRange(myFunctions, myPoints, row);
  if (index < 0)
    myMessage->setText(tr("VOXEL_RANGE_TOO_SMALL"));
  else
    myMessage->clear();
  fillRanges(index < 0 ? row : index);
  updateActual();
}

void VoxelPluginGUI_AxisTab::onMergeRange()
{
  QTreeWidgetItem* item = myRangeTree->currentItem();
  if (!item)
    return;
  const int row = myRangeTree->indexOfTopLevelItem(item);
  if (VoxelPlugin_GridEdit::MergeRange(myFunctions, myPoints, row))
  {
    myMessage->clear();
    fillRanges(std::min(row, (int)myFunctions.size() - 1));
    updateActual();
  }
}

void VoxelPluginGUI_AxisTab::onRangeEdited(QTreeWidgetItem* theItem, int theColumn)
{
  const int row = myRangeTree->indexOfTopLevelItem(theItem);
  const std::string text = VoxelPlugin_GridText::Trim(theItem->text(theColumn).toStdString());
  std::string error;
  if (theColumn == COL_TO)
  {
    double t;
    if (!VoxelPlugin_GridText::ParseNumber(text, t))
      error = tr("VOXEL_BAD_NUMBER").toStdString();
    else
      VoxelPlugin_GridEdit::SetRangeEnd(myPoints, row, t, error);
  }
  else if (theColumn == COL_FUNCTION)
  {
    std::vector<std::string> functions(myFunctions);
    functions[row] = text;
    try
    {
      VoxelPlugin_Hypothesis::SpacingCoordinates(0., 1., functions, myPoints, myAxis);
      myFunctions = functions;
    }
    catch (const SALOME_Exception& ex)
    {
      error = ex.what();
    }
  }
  if (error.empty())
    myMessage->clear();
  else
    myMessage->setText(QString::fromStdString(error));
  fillRanges(row); // shows the stored values: the edit, or the previous ones if it was refused
  updateActual();
}

void VoxelPluginGUI_AxisTab::updateButtons()
{
  myDeleteCoordBtn->setEnabled(myCoordList->currentRow() >= 0 && myCoords.size() > 2);
  myMergeBtn->setEnabled(myRangeTree->currentItem() && myFunctions.size() > 1);
}

bool VoxelPluginGUI_AxisTab::nodes(std::vector<double>& theNodes, std::string& theError) const
{
  if (!Check(theError))
    return false;
  switch (Mode())
  {
  case VoxelPlugin_Hypothesis::COORDINATES:
    theNodes = myCoords;
    std::sort(theNodes.begin(), theNodes.end());
    return true;
  case VoxelPlugin_Hypothesis::SPACING:
    if (!myHasExtent)
      return false;
    theNodes = VoxelPlugin_Hypothesis::SpacingCoordinates(0., myExtent, myFunctions, myPoints, myAxis);
    return true;
  default:
    return false; // uniform modes: see updateActual()
  }
}

void VoxelPluginGUI_AxisTab::updateActual()
{
  const int mode = Mode();
  if (mode == VoxelPlugin_Hypothesis::NB_CELLS || mode == VoxelPlugin_Hypothesis::STEP)
  {
    if (!myHasExtent || (mode == VoxelPlugin_Hypothesis::STEP && !(Step() > 0.)))
    {
      myActual->setText(tr("VOXEL_ACTUAL_NONE"));
      return;
    }
    const long long n = mode == VoxelPlugin_Hypothesis::STEP
      ? VoxelPlugin_Grid::NbCellsForStep(myExtent, Step())
      : NbCells();
    myActual->setText(tr("VOXEL_ACTUAL").arg(n).arg(myExtent / n, 0, 'g', 6));
    return;
  }
  std::vector<double> c;
  std::string error;
  if (!nodes(c, error) || c.size() < 2)
  {
    myActual->setText(tr("VOXEL_ACTUAL_NONE"));
    return;
  }
  double dmin = c[1] - c[0], dmax = dmin;
  for (std::size_t i = 2; i < c.size(); ++i)
  {
    dmin = std::min(dmin, c[i] - c[i - 1]);
    dmax = std::max(dmax, c[i] - c[i - 1]);
  }
  const QString step = dmin == dmax ? QString::number(dmin, 'g', 6)
                                    : QString("%1..%2").arg(dmin, 0, 'g', 6).arg(dmax, 0, 'g', 6);
  myActual->setText(tr("VOXEL_ACTUAL").arg(c.size() - 1).arg(step));
}
