# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later
# Mesh > SMESH plugins > Cartesian Grid Wizard: geometry -> grid -> Cartesian MED file, optionally
# the SMESH mesh too. Builds an ordinary mesh (CartesianGrid_3D + CartesianGridParameters).

import os
import threading

import salome
import SALOME
import GEOM
from salome.smesh import smeshBuilder
from salome.CartesianGridPlugin import CartesianGridPluginBuilder as Builder

TITLE = "Cartesian Grid Wizard"
NEW_MESH = "New mesh"

def newMesh(geom, name):
    """smeshBuilder.Mesh on geom with the CartesianGrid_3D algorithm and a new
    CartesianGridParameters hypothesis"""
    mesh = smeshBuilder.New().Mesh(geom, name)
    mesh.CartesianGrid().Grid()
    return mesh

class WizardRun:
    """Compute (optional) and export of one mesh; GUI-free. run() returns the summary text or
    raises RuntimeError; progress() and cancel() may be called from another thread"""
    def __init__(self, mesh, fileName, compute, zonePrefix):
        self.mesh = mesh                 # smeshBuilder.Mesh
        self.fileName = fileName
        self.compute = compute
        self.zonePrefix = zonePrefix
        self.phase = None                # "compute", "export"
        self.canceled = False
        self.algo = Builder.CartesianGridAlgorithm(mesh)

    def progress(self):
        if self.phase == "compute":
            return self.mesh.GetMesh().GetComputeProgress()
        if self.phase == "export":
            return self.algo.GetExportProgress()
        return 0.

    def cancel(self):
        self.canceled = True
        if self.phase == "compute":
            smeshBuilder.New().CancelCompute(self.mesh.GetMesh(), self.mesh.GetShape())
        elif self.phase == "export":
            self.algo.CancelExport()

    def _computeErrors(self):
        lines = []
        for err in self.mesh.GetComputeErrors():
            if err.code != 0:
                lines.append("%s: %s" % (err.algoName, err.comment or "error code %d" % err.code))
        return "\n".join(lines)

    def run(self):
        parts = []
        if self.compute:
            self.phase = "compute"
            ok = self.mesh.Compute()
            if self.canceled:
                raise RuntimeError("Computation canceled")
            errors = self._computeErrors()
            if not ok or self.mesh.NbVolumes() == 0:
                raise RuntimeError("The mesh is not computed" + (":\n" + errors if errors else ""))
            parts.append("SMESH mesh computed: %d hexahedra" % self.mesh.NbVolumes())
            if errors:
                parts.append("Warnings:\n" + errors)
            created, skipped = Builder.CreateGroupsOnGeometry(self.mesh)
            if created:
                parts.append("Mesh groups created: " + ", ".join(created))
            for name, reason in skipped:
                parts.append("Group %s skipped: %s" % (name, reason))
        if self.canceled:
            raise RuntimeError("Canceled")
        self.phase = "export"
        try:
            parts.append(Builder.ExportCartesianMED(self.mesh, self.fileName, "geometry", self.zonePrefix))
        except SALOME.SALOME_Exception as e:
            raise RuntimeError(e.details.text)
        except ValueError as e:
            raise RuntimeError(str(e))
        finally:
            self.phase = None
        return "\n\n".join(parts)

# ---------------------------------------------------------------------------- dialog

from qtsalome import (QCheckBox, QComboBox, QDialog, QDialogButtonBox, QFileDialog, QFormLayout,
                      QGroupBox, QHBoxLayout, QIcon, QLabel, QLineEdit, QMessageBox, QProgressBar,
                      QPushButton, QTimer, QVBoxLayout, QWidget)

COMPUTE_TIP = ("Creates the hexahedra in SMESH, to view the cells and the groups (one mesh group per "
               "group of solids of the geometry).\nA SMESH mesh is unstructured: every node and "
               "hexahedron is stored explicitly (about 760 bytes per cell), so it is much bigger and "
               "slower than the Cartesian MED file,\nwhich stores only the grid coordinates and one "
               "family per cell. Not needed for the export.")
PREFIX_TIP = ("Geometry groups whose name starts with this prefix are the zones (cell groups) of the "
              "MED file; empty: all groups.\nThe export works on the geometry: no SMESH cells are "
              "needed.")
MESH_TIP = ("Meshes of this geometry using the Cartesian Grid 3D algorithm. The wizard updates the "
            "chosen one;\n\"New mesh\" creates another. Changing the grid of a computed mesh "
            "clears its cells (SMESH keeps meshes consistent with their hypotheses).")
FILE_TIP = "Cartesian MED file: grid coordinates and one family per cell (structured, compact)"

def _selectedGeometry():
    """(entry, GEOM_Object, name) of the single selected geometry (GEOM or SHAPER), or
    (None, None, reason)"""
    if salome.sg.SelectedCount() != 1:
        return None, None, "Select one geometry in the object browser"
    entry = salome.sg.getSelected(0)
    obj = salome.IDToObject(entry)
    geom = obj._narrow(GEOM.GEOM_Object) if obj is not None else None
    if geom is None:
        return None, None, "The selected object is not a geometry"
    if str(geom.GetShapeType()) not in ("SOLID", "COMPSOLID", "COMPOUND"):
        return None, None, "The selected geometry holds no solid"
    return entry, geom, salome.IDToSObject(entry).GetName()

class WizardDialog(QDialog):
    def __init__(self, parent):
        QDialog.__init__(self, parent)
        self.setWindowTitle(TITLE)
        self.geom = None
        self.geomName = ""
        self.meshes = []                 # [(name, SMESH.SMESH_Mesh)] of the combo
        self.run = None
        self.worker = None
        self.result = None

        layout = QVBoxLayout(self)

        # geometry
        geomBox = QGroupBox("Geometry")
        geomLayout = QVBoxLayout(geomBox)
        row = QHBoxLayout()
        self.geomField = QLineEdit()
        self.geomField.setReadOnly(True)
        self.geomField.setPlaceholderText("Select a geometry (GEOM or SHAPER) in the object browser")
        # SMESH style selection button: while pressed, the field follows the object browser
        self.selectBtn = QPushButton()
        self.selectBtn.setIcon(QIcon(os.path.join(os.environ.get("SMESH_ROOT_DIR", ""), "share", "salome",
                                                  "resources", "smesh", "select1.png")))
        self.selectBtn.setCheckable(True)
        self.selectBtn.setChecked(True)
        self.selectBtn.setToolTip("While pressed, the geometry follows the selection in the object browser")
        self.selectBtn.toggled.connect(self.selectionToggled)
        row.addWidget(self.selectBtn)
        row.addWidget(self.geomField)
        geomLayout.addLayout(row)
        self.geomMessage = QLabel()
        self.geomMessage.setStyleSheet("color: red")
        geomLayout.addWidget(self.geomMessage)
        layout.addWidget(geomBox)

        # mesh
        meshBox = QGroupBox("Mesh")
        meshForm = QFormLayout(meshBox)
        self.meshCombo = QComboBox()
        self.meshCombo.setToolTip(MESH_TIP)
        self.meshCombo.currentIndexChanged.connect(self.meshChanged)
        meshForm.addRow("Mesh", self.meshCombo)
        self.meshName = QLineEdit()
        self.meshNameLabel = QLabel("Name")
        meshForm.addRow(self.meshNameLabel, self.meshName)
        layout.addWidget(meshBox)

        # grid: the C++ panel of the hypothesis dialog
        gridBox = QGroupBox("Grid")
        gridLayout = QVBoxLayout(gridBox)
        from salome.CartesianGridPlugin.CartesianGridPluginPanel import GridPanel
        self.panel = GridPanel(gridBox)
        gridLayout.addWidget(self.panel.widget)
        layout.addWidget(gridBox)

        # export
        exportBox = QGroupBox("Export")
        exportForm = QFormLayout(exportBox)
        self.file = QLineEdit()
        self.file.setToolTip(FILE_TIP)
        browse = QPushButton("...")
        browse.clicked.connect(self.browse)
        fileRow = QHBoxLayout()
        fileRow.addWidget(self.file)
        fileRow.addWidget(browse)
        fileWidget = QWidget()
        fileWidget.setLayout(fileRow)
        exportForm.addRow("MED file", fileWidget)
        self.prefix = QLineEdit("MAT_")
        self.prefix.setToolTip(PREFIX_TIP)
        exportForm.addRow("Zone groups", self.prefix)
        self.computeBox = QCheckBox("Also compute the SMESH mesh")
        self.computeBox.setToolTip(COMPUTE_TIP)
        exportForm.addRow(self.computeBox)
        layout.addWidget(exportBox)

        self.progress = QProgressBar()
        self.progress.setRange(0, 100)
        self.progress.setVisible(False)
        layout.addWidget(self.progress)
        self.buttons = QDialogButtonBox()
        self.runBtn = self.buttons.addButton("Run", QDialogButtonBox.AcceptRole)
        self.closeBtn = self.buttons.addButton(QDialogButtonBox.Close)
        self.runBtn.clicked.connect(self.start)
        self.closeBtn.clicked.connect(self.closeOrCancel)
        layout.addWidget(self.buttons)

        self.timer = QTimer(self)
        self.timer.timeout.connect(self.poll)
        self.geomEntry = None
        import SalomePyQt   # GUI only
        self.selection = SalomePyQt.SalomePyQt.getSelection()
        self.selection.currentSelectionChanged.connect(self.selectionChanged)
        self.selectGeometry()

    def _disconnect(self, *args):
        try:
            self.selection.currentSelectionChanged.disconnect(self.selectionChanged)
        except (RuntimeError, TypeError):
            pass

    # ---- geometry and mesh
    def selectionToggled(self, on):
        if on:
            self.selectGeometry()

    def selectionChanged(self):
        if self.selectBtn.isChecked() and self.worker is None:
            self.selectGeometry()

    def selectGeometry(self):
        entry, geom, text = _selectedGeometry()
        if geom is None:
            # another kind of object selected: the geometry is kept; a hint while there is none
            if self.geom is None:
                self.geomMessage.setText("" if salome.sg.SelectedCount() == 0 else text)
            self.updateRunButton()
            return
        if entry == self.geomEntry:
            return
        self.geomEntry = entry
        self.geom, self.geomName = geom, text
        self.geomField.setText(text)
        self.geomMessage.setText("")
        try:
            self.panel.setShape(entry)
        except RuntimeError as e:
            self.geomMessage.setText(str(e))
        self.fillMeshes()

    def fillMeshes(self, current=None):
        self.meshes = Builder.CartesianGridMeshesOn(self.geom) if self.geom is not None else []
        self.meshCombo.blockSignals(True)
        self.meshCombo.clear()
        for name, _ in self.meshes:
            self.meshCombo.addItem(name)
        self.meshCombo.addItem(NEW_MESH)
        index = 0
        for i, (_, m) in enumerate(self.meshes):
            if current is not None and m._is_equivalent(current):
                index = i
        self.meshCombo.setCurrentIndex(index)
        self.meshCombo.blockSignals(False)
        self.meshChanged()

    def selectedMesh(self):
        """SMESH.SMESH_Mesh of the combo, None for New mesh"""
        i = self.meshCombo.currentIndex()
        return self.meshes[i][1] if 0 <= i < len(self.meshes) else None

    def meshChanged(self, *args):
        mesh = self.selectedMesh()
        isNew = mesh is None
        self.meshName.setVisible(isNew)
        self.meshNameLabel.setVisible(isNew)
        if isNew:
            # the grid shown is kept: a new mesh starts from it
            self.meshName.setText("CartesianGrid_" + self.geomName if self.geomName else "CartesianGrid")
            name = self.meshName.text()
        else:
            name = self.meshCombo.currentText()
            hyp = Builder.CartesianGridParametersOf(mesh)
            if hyp is not None:
                try:
                    self.panel.load(hyp)
                except RuntimeError as e:
                    QMessageBox.warning(self, TITLE, str(e))
        self.file.setText(os.path.join(os.path.expanduser("~"), name + ".med"))
        self.updateRunButton()

    def updateRunButton(self):
        self.runBtn.setEnabled(self.geom is not None and self.worker is None)
        self.selectBtn.setEnabled(self.worker is None)

    def browse(self):
        f, _ = QFileDialog.getSaveFileName(self, "MED file", self.file.text(), "MED files (*.med)")
        if f:
            self.file.setText(f if f.endswith(".med") else f + ".med")

    # ---- run
    def start(self):
        if self.geom is None:
            return
        try:
            self.panel.check()
        except RuntimeError as e:
            QMessageBox.warning(self, TITLE, str(e))
            return
        fileName = self.file.text().strip()
        if not fileName:
            QMessageBox.warning(self, TITLE, "Give a file name")
            return
        fileName = os.path.abspath(os.path.expanduser(fileName))  # the engine runs in the SMESH container
        smesh = smeshBuilder.New()
        try:
            corba = self.selectedMesh()
            if corba is None:
                name = self.meshName.text().strip() or "CartesianGrid"
                mesh = newMesh(self.geom, name)
            else:
                mesh = smesh.Mesh(corba)
            self.panel.store(Builder.CartesianGridParametersOf(mesh))
            self.run = WizardRun(mesh, fileName, self.computeBox.isChecked(), self.prefix.text())
        except (RuntimeError, ValueError) as e:
            QMessageBox.warning(self, TITLE, str(e))
            return
        except SALOME.SALOME_Exception as e:
            QMessageBox.warning(self, TITLE, e.details.text)
            return
        if corba is None:   # the new mesh becomes the current one
            self.fillMeshes(mesh.GetMesh())
        salome.sg.updateObjBrowser()

        def work():
            try:
                self.result = ("ok", self.run.run())
            except RuntimeError as e:
                self.result = ("error", str(e))
            except SALOME.SALOME_Exception as e:
                self.result = ("error", e.details.text)
            except Exception as e:
                self.result = ("error", "%s: %s" % (type(e).__name__, e))
        self.result = None
        self.worker = threading.Thread(target=work)
        self.updateRunButton()
        self.closeBtn.setText("Cancel")
        self.progress.setValue(0)
        self.progress.setVisible(True)
        self.worker.start()
        self.timer.start(100)

    def poll(self):
        if self.worker.is_alive():
            phase = {"compute": "Computing the SMESH mesh %p%", "export": "Exporting %p%"}
            self.progress.setFormat(phase.get(self.run.phase, "%p%"))
            self.progress.setValue(int(100 * self.run.progress()))
            return
        self.timer.stop()
        self.worker = None
        self.progress.setVisible(False)
        self.closeBtn.setText("Close")
        self.updateRunButton()
        salome.sg.updateObjBrowser()
        kind, text = self.result
        if kind == "ok":
            QMessageBox.information(self, TITLE, text)
        else:
            QMessageBox.warning(self, TITLE, text)

    def closeOrCancel(self):
        if self.worker is not None:
            self.run.cancel()
        else:
            self.reject()

    def reject(self):
        if self.worker is not None:      # Escape or window close while running: cancel first
            self.run.cancel()
            return
        QDialog.reject(self)
        self._disconnect()
        global _dialog
        if _dialog is self:
            _dialog = None
        self.deleteLater()     # not modal: free it (and the C++ panel) when closed

_dialog = None   # the open wizard: not modal, one at a time

def run(context):
    global _dialog
    parent = context.sg.getDesktop()
    if _dialog is not None:
        _dialog.show()
        _dialog.raise_()
        _dialog.activateWindow()
        return
    try:
        dialog = WizardDialog(parent)
    except OSError as e:
        from salome.CartesianGridPlugin.CartesianGridPluginPanel import libraryPath
        QMessageBox.warning(parent, TITLE, "Cannot load the grid panel library %s:\n%s" % (libraryPath(), e))
        return
    _dialog = dialog
    dialog.show()
