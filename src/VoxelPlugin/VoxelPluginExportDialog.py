# Copyright (C) 2026  VOXELPLUGIN authors
# LGPL 2.1 or later
# Dialog of Mesh > SMESH plugins > Cartesian Grid MED Export...

import os
import threading

import salome
import SALOME
import SMESH
from qtsalome import (QDialog, QDialogButtonBox, QFileDialog, QFormLayout, QHBoxLayout, QLabel,
                      QLineEdit, QMessageBox, QProgressBar, QPushButton, QRadioButton, QTimer,
                      QVBoxLayout, QWidget)
from salome.VoxelPlugin import VoxelPluginBuilder

def selectedMesh():
    """(SMESH.SMESH_Mesh, name) of the single selected object, or (None, message)"""
    if salome.sg.SelectedCount() != 1:
        return None, "Select one mesh"
    obj = salome.IDToObject(salome.sg.getSelected(0))
    mesh = obj._narrow(SMESH.SMESH_Mesh) if obj is not None else None
    if mesh is None:
        return None, "The selected object is not a mesh"
    try:
        VoxelPluginBuilder.VoxelAlgorithm(mesh)
    except ValueError as e:
        return None, str(e)
    return mesh, salome.IDToSObject(salome.sg.getSelected(0)).GetName()

class ExportDialog(QDialog):
    def __init__(self, parent, mesh, name):
        QDialog.__init__(self, parent)
        self.setWindowTitle("Cartesian Grid MED Export")
        self.mesh = mesh
        self.algo = VoxelPluginBuilder.VoxelAlgorithm(mesh)
        self.result = None
        form = QFormLayout()
        form.addRow("Mesh", QLabel(name))
        self.file = QLineEdit(os.path.join(os.path.expanduser("~"), name + ".med"))
        browse = QPushButton("...")
        browse.clicked.connect(self.browse)
        fileRow = QHBoxLayout()
        fileRow.addWidget(self.file)
        fileRow.addWidget(browse)
        fileWidget = QWidget()
        fileWidget.setLayout(fileRow)
        form.addRow("File", fileWidget)
        self.fromGeometry = QRadioButton("Geometry (no mesh needed)")
        self.fromMesh = QRadioButton("Computed mesh")
        self.fromGeometry.setChecked(True)
        self.fromMesh.setEnabled(mesh.NbVolumes() > 0)
        sourceRow = QVBoxLayout()
        sourceRow.addWidget(self.fromGeometry)
        sourceRow.addWidget(self.fromMesh)
        sourceWidget = QWidget()
        sourceWidget.setLayout(sourceRow)
        form.addRow("From", sourceWidget)
        self.prefix = QLineEdit("MAT_")
        self.prefix.setToolTip("Groups whose name starts with this prefix are the zones; empty: all groups")
        form.addRow("Zone groups", self.prefix)
        self.progress = QProgressBar()
        self.progress.setRange(0, 100)
        self.progress.setVisible(False)
        self.buttons = QDialogButtonBox()
        self.exportBtn = self.buttons.addButton("Export", QDialogButtonBox.AcceptRole)
        self.closeBtn = self.buttons.addButton(QDialogButtonBox.Close)
        self.exportBtn.clicked.connect(self.export)
        self.closeBtn.clicked.connect(self.closeOrCancel)
        layout = QVBoxLayout(self)
        layout.addLayout(form)
        layout.addWidget(self.progress)
        layout.addWidget(self.buttons)
        self.timer = QTimer(self)
        self.timer.timeout.connect(self.poll)
        self.worker = None

    def browse(self):
        f, _ = QFileDialog.getSaveFileName(self, "MED file", self.file.text(), "MED files (*.med)")
        if f:
            self.file.setText(f if f.endswith(".med") else f + ".med")

    def export(self):
        fileName = self.file.text().strip()
        if fileName:   # the engine runs in the SMESH container, with its own working directory
            fileName = os.path.abspath(os.path.expanduser(fileName))
        else:
            QMessageBox.warning(self, "Cartesian Grid MED Export", "Give a file name")
            return
        source = "mesh" if self.fromMesh.isChecked() else "geometry"
        prefix = self.prefix.text()
        def work():
            try:
                self.result = ("ok", VoxelPluginBuilder.ExportCartesianMED(self.mesh, fileName, source, prefix))
            except SALOME.SALOME_Exception as e:
                self.result = ("error", e.details.text)
            except Exception as e:
                self.result = ("error", str(e))
        self.result = None
        self.exportBtn.setEnabled(False)
        self.closeBtn.setText("Cancel")
        self.progress.setValue(0)
        self.progress.setVisible(True)
        self.worker = threading.Thread(target=work)
        self.worker.start()
        self.timer.start(100)

    def poll(self):
        if self.worker.is_alive():
            self.progress.setValue(int(100 * self.algo.GetExportProgress()))
            return
        self.timer.stop()
        self.worker = None
        self.progress.setVisible(False)
        self.exportBtn.setEnabled(True)
        self.closeBtn.setText("Close")
        kind, text = self.result
        if kind == "ok":
            QMessageBox.information(self, "Cartesian Grid MED Export", text)
        else:
            QMessageBox.warning(self, "Cartesian Grid MED Export", text)

    def closeOrCancel(self):
        if self.worker is not None:
            self.algo.CancelExport()
        else:
            self.reject()

    def reject(self):
        if self.worker is not None:      # Escape or window close while exporting: cancel first
            self.algo.CancelExport()
            return
        QDialog.reject(self)

def run(context):
    mesh, name = selectedMesh()
    parent = context.sg.getDesktop()
    if mesh is None:
        QMessageBox.warning(parent, "Cartesian Grid MED Export", name)
        return
    dialog = ExportDialog(parent, mesh, name)
    dialog.exec_()
