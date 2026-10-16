# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later
# Mesh > SMESH plugins > Cartesian Grid Wizard: geometry -> grid -> Cartesian MED file, optionally
# the SMESH mesh too. Builds an ordinary mesh (CartesianGrid_3D + CartesianGridParameters).

import html
import os
import threading

import salome
import SALOME
import GEOM
from salome.smesh import smeshBuilder
from salome.CartesianGridPlugin import CartesianGridPluginBuilder as Builder


def newMesh(geom, name):
    """smeshBuilder.Mesh on geom with the CartesianGrid_3D algorithm and a new
    CartesianGridParameters hypothesis"""
    mesh = smeshBuilder.New().Mesh(geom, name)
    mesh.CartesianGrid().Grid()
    return mesh

# report messages of a run (Builder.REPORT_FORMATS + the compute ones), translated by the dialog
# report of a run: a paragraph for the SMESH mesh (its groups always come from the geometry groups,
# whatever the zones of the file), a paragraph for the MED file. The MED file is written first: it
# does not need the SMESH mesh, so a failed or canceled compute still leaves it (warning in the report)
REPORT_FORMATS = dict(Builder.REPORT_FORMATS, **{
    "COMPUTED": ("info", "SMESH mesh: {0} hexahedra"),
    "COMPUTE_CANCELED": ("warning", "SMESH mesh: computation canceled"),
    "NOT_COMPUTED": ("warning", "SMESH mesh: not computed"),
    "NOT_COMPUTED_DETAILS": ("warning", "SMESH mesh: not computed:\n{0}"),
    "COMPUTE_WARNINGS": ("warning", "  warnings:\n{0}"),
    "GROUPS_CREATED": ("info", "  mesh groups from the geometry groups: {0}"),
    "GROUP_EXISTS": ("info", "  group {0} skipped: " + Builder.SKIP_EXISTS),
    "GROUP_EMPTY": ("info", "  group {0} skipped: " + Builder.SKIP_EMPTY),
    "MED_FILE": ("info", "MED file: {0}"),
    "MED_ZONES_SOLIDS": ("info", "  zones: one per solid, named as in the study or SHAPER"),
    "MED_ZONES_GROUPS": ("info", "  zones: geometry groups named {0}*"),
    "MED_ZONES_ALL": ("info", "  zones: all geometry groups"),
    "MED_GRID": ("info", "  Cartesian grid {0} x {1} x {2} = {3} cells"),
})

class WizardRun:
    """Export and compute (optional) of one mesh; GUI-free. run() returns the summary text or
    raises RuntimeError when the export fails (then no compute); parts holds the report as lists of messages (key, args) or
    (key, args, indent) of REPORT_FORMATS, one list per paragraph. progress() and cancel() may be called from another thread"""
    def __init__(self, mesh, fileName, compute, zonePrefix, zoneSource="geometry"):
        self.mesh = mesh                 # smeshBuilder.Mesh
        self.fileName = fileName
        self.compute = compute
        self.zonePrefix = zonePrefix
        self.zoneSource = zoneSource     # "geometry": groups named zonePrefix*, "solids": one zone per solid
        self.phase = None                # "compute", "export"
        self.canceled = False
        self.parts = []
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
        self.parts = []
        self.phase = "export"
        try:
            medPart = self._medPart(Builder.ExportCartesianMEDMessages(self.mesh, self.fileName,
                                                                       self.zoneSource, self.zonePrefix))
        except SALOME.SALOME_Exception as e:
            raise RuntimeError(e.details.text)
        except ValueError as e:
            raise RuntimeError(str(e))
        finally:
            self.phase = None
        if self.compute:
            self.parts.append(self._meshPart())
        self.parts.append(medPart)
        return reportText(self.parts)

    def _meshPart(self):
        """compute the SMESH mesh and create its groups; the report paragraph, a warning when the
        compute is canceled or fails"""
        if self.canceled:   # canceled at the end of the export
            return [("COMPUTE_CANCELED", ())]
        self.phase = "compute"
        try:
            ok = self.mesh.Compute()
        finally:
            self.phase = None
        if self.canceled:
            return [("COMPUTE_CANCELED", ())]
        errors = self._computeErrors()
        if not ok or self.mesh.NbVolumes() == 0:
            return [("NOT_COMPUTED_DETAILS", (errors,))] if errors else [("NOT_COMPUTED", ())]
        part = [("COMPUTED", (self.mesh.NbVolumes(),))]
        if errors:
            part.append(("COMPUTE_WARNINGS", (errors,)))
        created, skipped = Builder.CreateGroupsOnGeometry(self.mesh)
        if created:
            part.append(("GROUPS_CREATED", (", ".join(created),)))
        for name, reason in skipped:
            part.append(("GROUP_EXISTS" if reason == Builder.SKIP_EXISTS else "GROUP_EMPTY", (name,)))
        return part

    def _medPart(self, messages):
        """export messages as a paragraph: file, zone choice, grid, cell counts, then the notes and
        warnings (indented under the file)"""
        grid = [args for key, args in messages if key == "GRID"][0]
        if self.zoneSource == "solids":
            zones = ("MED_ZONES_SOLIDS", ())
        elif self.zonePrefix:
            zones = ("MED_ZONES_GROUPS", (self.zonePrefix,))
        else:
            zones = ("MED_ZONES_ALL", ())
        counts = [m for m in messages if m[0] in ("ZONE_CELLS", "NO_ZONE_CELLS", "SOLIDS_NO_ZONE")]
        others = [(key, args, 0 if REPORT_FORMATS[key][1].startswith(" ") else 2) for key, args in messages
                  if key != "GRID" and (key, args) not in counts]
        return [("MED_FILE", (grid[4],)), zones, ("MED_GRID", grid[:4])] + counts + others

def reportText(parts, translate=None):
    """text of the report paragraphs of WizardRun.parts, a blank line between paragraphs;
    translate(key) gives the text of a key, None for English"""
    lines = []
    for part in parts:
        if lines:
            lines.append("")
        for message in part:
            key, args = message[:2]
            indent = message[2] if len(message) > 2 else 0
            text = translate(key) if translate else None
            lines.append(" " * indent + (text or REPORT_FORMATS[key][1]).format(*args))
    return "\n".join(lines)

def reportLevel(parts):
    """warning if a message of the report is a warning, else info"""
    levels = {REPORT_FORMATS[m[0]][0] for part in parts for m in part}
    return "warning" if "warning" in levels else "info"

# ---------------------------------------------------------------------------- dialog

from qtsalome import (QButtonGroup, QCheckBox, QComboBox, QCoreApplication, QDialog, QDialogButtonBox,
                      QFileDialog, QGridLayout, QGroupBox, QHBoxLayout, QIcon, QLabel, QLineEdit, QMessageBox,
                      QProgressBar, QPushButton, QRadioButton, QTimer, QVBoxLayout, QWidget)

def tr(key):
    """text of key in CartesianGridPlugin_msg_<language>.ts (SALOME loads it at start)"""
    return QCoreApplication.translate("CartesianGridPluginWizard", key)


def _translatedReport(key):
    """text of a report message key in the .ts files, None if missing"""
    text = tr("CARTESIANGRID_REPORT_" + key)
    return None if text == "CARTESIANGRID_REPORT_" + key else text

def _guiHelper():
    """salome.gui.helper (GUI only): selection through its own SALOMEGUI_Swig, as the other Python
    plugins; salome.sg is a SalomeOutsideGUI without selection when salome_init() first ran
    non-embedded in the GUI process"""
    from salome.gui import helper
    return helper

def _selectionCount():
    return _guiHelper().SalomeGUI.SelectedCount()

def _updateObjBrowser():
    _guiHelper().sgPyQt.updateObjBrowser()

def _selectedGeometry():
    """(entry, GEOM_Object, name) of the single selected geometry (GEOM or SHAPER), or
    (None, None, reason)"""
    sobj, entry = _guiHelper().getSObjectSelected()
    if sobj is None:
        return None, None, tr("CARTESIANGRID_WIZ_SELECT_ONE")
    obj = sobj.GetObject()
    geom = obj._narrow(GEOM.GEOM_Object) if obj is not None else None
    if geom is None:
        return None, None, tr("CARTESIANGRID_WIZ_NOT_GEOMETRY")
    if str(geom.GetShapeType()) not in ("SOLID", "COMPSOLID", "COMPOUND"):
        return None, None, tr("CARTESIANGRID_WIZ_NO_SOLID")
    return entry, geom, sobj.GetName()

class WizardDialog(QDialog):
    def __init__(self, parent):
        QDialog.__init__(self, parent)
        self.setWindowTitle(tr("CARTESIANGRID_WIZ_TITLE"))
        self.setMinimumWidth(520)
        self.geom = None
        self.geomName = ""
        self.meshes = []                 # [(name, SMESH.SMESH_Mesh)] of the combo
        self.run = None
        self.worker = None
        self.result = None

        layout = QVBoxLayout(self)
        labels = []                      # first column of every group, same width for alignment

        def group(title):
            box = QGroupBox(title)
            grid = QGridLayout(box)
            grid.setColumnStretch(1, 1)
            layout.addWidget(box)
            return grid

        def addRow(grid, text, widget):
            row = grid.rowCount()
            label = QLabel(text)
            labels.append(label)
            grid.addWidget(label, row, 0)
            grid.addWidget(widget, row, 1)
            return label

        def hbox(*widgets):
            w = QWidget()
            lay = QHBoxLayout(w)
            lay.setContentsMargins(0, 0, 0, 0)
            for x in widgets:
                lay.addWidget(x)
            return w

        # geometry
        geomGrid = group(tr("CARTESIANGRID_WIZ_GEOMETRY"))
        self.geomField = QLineEdit()
        self.geomField.setReadOnly(True)
        self.geomField.setPlaceholderText(tr("CARTESIANGRID_WIZ_GEOMETRY_HINT"))
        # SMESH style selection button: while pressed, the field follows the object browser
        self.selectBtn = QPushButton()
        self.selectBtn.setIcon(QIcon(os.path.join(os.environ.get("SMESH_ROOT_DIR", ""), "share", "salome",
                                                  "resources", "smesh", "select1.png")))
        self.selectBtn.setCheckable(True)
        self.selectBtn.setChecked(True)
        self.selectBtn.setToolTip(tr("CARTESIANGRID_WIZ_SELECT_TIP"))
        self.selectBtn.toggled.connect(self.selectionToggled)
        addRow(geomGrid, tr("CARTESIANGRID_WIZ_SHAPE"), hbox(self.selectBtn, self.geomField))
        self.geomMessage = QLabel()
        self.geomMessage.setStyleSheet("color: red")
        self.geomMessage.setVisible(False)
        geomGrid.addWidget(self.geomMessage, geomGrid.rowCount(), 1)

        # mesh
        meshGrid = group(tr("CARTESIANGRID_WIZ_MESH"))
        self.meshCombo = QComboBox()
        self.meshCombo.setToolTip(tr("CARTESIANGRID_WIZ_MESH_TIP"))
        self.meshCombo.currentIndexChanged.connect(self.meshChanged)
        addRow(meshGrid, tr("CARTESIANGRID_WIZ_MESH"), self.meshCombo)
        self.meshName = QLineEdit()
        self.meshName.textChanged.connect(self.updateRunTip)
        self.meshNameLabel = addRow(meshGrid, tr("CARTESIANGRID_WIZ_NAME"), self.meshName)

        # grid: the C++ panel of the hypothesis dialog
        gridBox = QGroupBox(tr("CARTESIANGRID_WIZ_GRID"))
        gridLayout = QVBoxLayout(gridBox)
        from salome.CartesianGridPlugin.CartesianGridPluginPanel import GridPanel
        self.panel = GridPanel(gridBox)
        gridLayout.addWidget(self.panel.widget)
        layout.addWidget(gridBox, 1)

        # export
        exportGrid = group(tr("CARTESIANGRID_WIZ_EXPORT"))
        self.file = QLineEdit()
        self.file.setToolTip(tr("CARTESIANGRID_WIZ_FILE_TIP"))
        self.file.textChanged.connect(self.updateRunTip)
        self.fileChosen = False          # edited or browsed: no longer follows the mesh name
        self.file.textEdited.connect(self.fileEdited)
        browse = QPushButton("...")
        browse.setFixedWidth(browse.fontMetrics().horizontalAdvance("...") + 24)
        browse.setToolTip(tr("CARTESIANGRID_WIZ_BROWSE_TIP"))
        browse.clicked.connect(self.browse)
        addRow(exportGrid, tr("CARTESIANGRID_WIZ_MED_FILE"), hbox(self.file, browse))
        self.groupsRadio = QRadioButton(tr("CARTESIANGRID_WIZ_GROUPS"))
        self.groupsRadio.setToolTip(tr("CARTESIANGRID_WIZ_GROUPS_TIP"))
        self.groupsRadio.setChecked(True)
        self.prefix = QLineEdit("MAT_")
        self.prefix.setToolTip(tr("CARTESIANGRID_WIZ_PREFIX_TIP"))
        self.prefix.textChanged.connect(self.updateRunTip)
        addRow(exportGrid, tr("CARTESIANGRID_WIZ_ZONES"), hbox(self.groupsRadio, self.prefix))
        self.solidsRadio = QRadioButton(tr("CARTESIANGRID_WIZ_SOLIDS"))
        self.solidsRadio.setToolTip(tr("CARTESIANGRID_WIZ_SOLIDS_TIP"))
        exportGrid.addWidget(self.solidsRadio, exportGrid.rowCount(), 1)
        self.zoneButtons = QButtonGroup(self)
        self.zoneButtons.addButton(self.groupsRadio)
        self.zoneButtons.addButton(self.solidsRadio)
        self.groupsRadio.toggled.connect(self.zoneSourceChanged)
        self.computeBox = QCheckBox(tr("CARTESIANGRID_WIZ_COMPUTE"))
        self.computeBox.setToolTip(tr("CARTESIANGRID_WIZ_COMPUTE_TIP"))
        self.computeBox.toggled.connect(self.updateRunTip)
        exportGrid.addWidget(self.computeBox, exportGrid.rowCount(), 1)

        width = max(l.sizeHint().width() for l in labels)
        for l in labels:
            l.setMinimumWidth(width)

        self.progress = QProgressBar()
        self.progress.setRange(0, 100)
        self.progress.setVisible(False)
        layout.addWidget(self.progress)
        self.buttons = QDialogButtonBox()
        self.runBtn = self.buttons.addButton(tr("CARTESIANGRID_WIZ_RUN"), QDialogButtonBox.AcceptRole)
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
    def setGeomMessage(self, text):
        self.geomMessage.setText(text)
        self.geomMessage.setVisible(bool(text))

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
                self.setGeomMessage("" if _selectionCount() == 0 else text)
            self.updateRunButton()
            return
        if entry == self.geomEntry:
            return
        self.geomEntry = entry
        self.geom, self.geomName = geom, text
        self.geomField.setText(text)
        self.setGeomMessage("")
        try:
            self.panel.setShape(entry)
        except RuntimeError as e:
            self.setGeomMessage(str(e))
        self.fillMeshes()

    def fillMeshes(self, current=None):
        self.meshes = Builder.CartesianGridMeshesOn(self.geom) if self.geom is not None else []
        self.meshCombo.blockSignals(True)
        self.meshCombo.clear()
        for name, _ in self.meshes:
            self.meshCombo.addItem(name)
        self.meshCombo.addItem(tr("CARTESIANGRID_WIZ_NEW_MESH"))
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
                    QMessageBox.warning(self, tr("CARTESIANGRID_WIZ_TITLE"), str(e))
        if not self.fileChosen:
            self.file.setText(os.path.join(os.path.expanduser("~"), name + ".med"))
        self.updateRunButton()

    def updateRunButton(self):
        self.runBtn.setEnabled(self.geom is not None and self.worker is None)
        self.selectBtn.setEnabled(self.worker is None)
        self.updateRunTip()

    def updateRunTip(self, *args):
        """tooltip of Run: what a run does with the current options"""
        if self.geom is None:
            self.runBtn.setToolTip(tr("CARTESIANGRID_WIZ_SELECT_FIRST"))
            return
        mesh = self.selectedMesh()
        steps = []
        if mesh is None:
            steps.append(tr("CARTESIANGRID_WIZ_STEP_NEW_MESH")
                         % (self.meshName.text().strip() or "CartesianGrid", self.geomName))
        else:
            steps.append(tr("CARTESIANGRID_WIZ_STEP_MESH") % self.meshCombo.currentText())
        prefix = self.prefix.text()
        if self.zoneSource() == "solids":
            zones = tr("CARTESIANGRID_WIZ_ZONES_SOLIDS")
        else:
            zones = (tr("CARTESIANGRID_WIZ_ZONES_PREFIX") % prefix if prefix else tr("CARTESIANGRID_WIZ_ZONES_ALL"))
        steps.append(tr("CARTESIANGRID_WIZ_STEP_EXPORT")
                     % (self.file.text().strip() or tr("CARTESIANGRID_WIZ_NO_FILE_NAME"), zones))
        if self.computeBox.isChecked():
            steps.append(tr("CARTESIANGRID_WIZ_STEP_COMPUTE"))
        else:
            steps.append(tr("CARTESIANGRID_WIZ_STEP_NO_COMPUTE"))
        # rich text: Qt wraps it
        self.runBtn.setToolTip("<b>%s</b><ol>%s</ol>" % (html.escape(tr("CARTESIANGRID_WIZ_RUN").replace("&", "")),
                                                         "".join("<li>%s</li>" % html.escape(t) for t in steps)))

    def zoneSource(self):
        return "solids" if self.solidsRadio.isChecked() else "geometry"

    def zoneSourceChanged(self, *args):
        self.prefix.setEnabled(self.groupsRadio.isChecked())
        self.updateRunTip()

    def fileEdited(self, *args):
        self.fileChosen = bool(self.file.text().strip())

    def browse(self):
        f, _ = QFileDialog.getSaveFileName(self, tr("CARTESIANGRID_WIZ_MED_FILE"), self.file.text(),
                                       tr("CARTESIANGRID_WIZ_MED_FILTER"))
        if f:
            self.file.setText(f if f.endswith(".med") else f + ".med")
            self.fileChosen = True

    # ---- run
    def start(self):
        if self.geom is None:
            return
        try:
            self.panel.check()
        except RuntimeError as e:
            QMessageBox.warning(self, tr("CARTESIANGRID_WIZ_TITLE"), str(e))
            return
        fileName = self.file.text().strip()
        if not fileName:
            QMessageBox.warning(self, tr("CARTESIANGRID_WIZ_TITLE"), tr("CARTESIANGRID_WIZ_NO_FILE"))
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
            self.run = WizardRun(mesh, fileName, self.computeBox.isChecked(), self.prefix.text(),
                                 self.zoneSource())
        except (RuntimeError, ValueError) as e:
            QMessageBox.warning(self, tr("CARTESIANGRID_WIZ_TITLE"), str(e))
            return
        except SALOME.SALOME_Exception as e:
            QMessageBox.warning(self, tr("CARTESIANGRID_WIZ_TITLE"), e.details.text)
            return
        if corba is None:   # the new mesh becomes the current one
            self.fillMeshes(mesh.GetMesh())
        _updateObjBrowser()

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
        self.closeBtn.setText(tr("CARTESIANGRID_WIZ_CANCEL"))
        self.progress.setValue(0)
        self.progress.setVisible(True)
        self.worker.start()
        self.timer.start(100)

    def poll(self):
        if self.worker.is_alive():
            phase = {"compute": tr("CARTESIANGRID_WIZ_COMPUTING"), "export": tr("CARTESIANGRID_WIZ_EXPORTING")}
            self.progress.setFormat(phase.get(self.run.phase, "%p%"))
            self.progress.setValue(int(100 * self.run.progress()))
            return
        self.timer.stop()
        self.worker = None
        self.progress.setVisible(False)
        self.closeBtn.setText(tr("CARTESIANGRID_WIZ_CLOSE"))
        self.updateRunButton()
        _updateObjBrowser()
        kind, text = self.result
        if kind == "ok":
            text = reportText(self.run.parts, _translatedReport)
        if kind == "ok" and reportLevel(self.run.parts) == "info":
            QMessageBox.information(self, tr("CARTESIANGRID_WIZ_TITLE"), text)
        else:
            QMessageBox.warning(self, tr("CARTESIANGRID_WIZ_TITLE"), text)

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
        QMessageBox.warning(parent, tr("CARTESIANGRID_WIZ_TITLE"), tr("CARTESIANGRID_WIZ_NO_LIBRARY") % (libraryPath(), e))
        return
    _dialog = dialog
    dialog.show()
