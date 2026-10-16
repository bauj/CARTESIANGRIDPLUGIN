// Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
// LGPL 2.1 or later
#ifndef CARTESIANGRIDPLUGINGUI_AxisTab_HeaderFile
#define CARTESIANGRIDPLUGINGUI_AxisTab_HeaderFile

#include <QFrame>

#include <string>
#include <vector>

class QButtonGroup;
class QLabel;
class QListWidget;
class QListWidgetItem;
class QPushButton;
class QStackedWidget;
class QTreeWidget;
class QTreeWidgetItem;
class SMESHGUI_SpinBox;
class SalomeApp_IntSpinBox;

/*!
 * \brief Grid definition along one axis: mode (Nb cells / Step / Coordinates / Spacing), its
 *        editor (coordinate list, or ranges of t with their spacing function, as Body Fitting)
 *        and the actual cells along the shape extent
 */
class CartesianGridPluginGUI_AxisTab : public QFrame
{
  Q_OBJECT

public:
  CartesianGridPluginGUI_AxisTab(QWidget* theParent, int theAxis);

  //! bounds of the mesh shape along the axis, for the actual cells and the default coordinates
  void SetExtent(bool theHasExtent, double theMin, double theMax);

  int  Mode() const;
  void SetMode(int theMode);

  int    NbCells() const;
  double Step() const;
  void   SetNbCells(int theNb);
  void   SetStep(double theStep);

  const std::vector<double>& Coordinates() const { return myCoords; }
  //! theIsDefault: the list of a new hypothesis, shown as the bounds of the shape until edited
  void SetCoordinates(const std::vector<double>& theCoords, bool theIsDefault = false);

  const std::vector<std::string>& SpacingFunctions() const { return myFunctions; }
  const std::vector<double>&      SpacingPoints() const { return myPoints; }
  void SetSpacing(const std::vector<std::string>& theFunctions, const std::vector<double>& thePoints);

  //! true if the definition of the current mode is valid, else false with theError
  bool Check(std::string& theError) const;

private slots:
  void onMode(int theMode);
  void onInsertCoordinate();
  void onDeleteCoordinate();
  void onPasteCoordinates();
  void onCopyCoordinates();
  void onCoordinateEdited(QListWidgetItem* theItem);
  void onSplitRange();
  void onMergeRange();
  void onRangeEdited(QTreeWidgetItem* theItem, int theColumn);
  void updateButtons();
  void updateActual();

private:
  void fillCoordinates(int theSelected);
  void fillRanges(int theSelected);
  //! a default coordinate list becomes the bounds of the shape
  void applyShapeBounds();
  //! the coordinates changed by the user: no longer the default list
  void coordinatesEdited();
  //! node coordinates of the current mode over [0, extent]; false with theError if invalid
  bool nodes(std::vector<double>& theNodes, std::string& theError) const;

  int                      myAxis;
  QButtonGroup*            myModeGroup;
  QStackedWidget*          myStack;
  SalomeApp_IntSpinBox*    myNbCells;
  SMESHGUI_SpinBox*        myStep;
  QListWidget*             myCoordList;
  QPushButton*             myInsertCoordBtn;
  QPushButton*             myDeleteCoordBtn;
  QPushButton*             myPasteBtn;
  QPushButton*             myCopyBtn;
  SMESHGUI_SpinBox*        myInsertStep;
  QTreeWidget*             myRangeTree;
  QPushButton*             mySplitBtn;
  QPushButton*             myMergeBtn;
  QLabel*                  myActual;
  QLabel*                  myMessage;
  bool                     myHasExtent;
  double                   myExtent;
  double                   myMin, myMax; //!< bounds of the shape when myHasExtent
  std::vector<double>      myCoords;
  bool                     myCoordsDefault; //!< myCoords not chosen by the user: bounds of the shape
  std::vector<std::string> myFunctions;
  std::vector<double>      myPoints;
};

#endif
