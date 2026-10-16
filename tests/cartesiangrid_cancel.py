# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later (see COPYING)
# Progress and cancel of a long compute, during classification then during element creation
import threading
import time
import salome
salome.salome_init()
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder
import SMESH
geompy = geomBuilder.New()
smesh = smeshBuilder.New()

sphere = geompy.MakeSphereR(50.)
geompy.addToStudy(sphere, "Sphere")
mesh = smesh.Mesh(sphere, "Sphere")
grid = mesh.CartesianGrid().Grid(nbCells=400)   # 6.4e7 cells: seconds of classification

result = {}
def compute():
    result["ok"] = mesh.Compute()
worker = threading.Thread(target=compute)
worker.start()

# progress goes up while classifying (below 0.2); cancel there, before any element exists
progress = []
deadline = time.time() + 30.
while time.time() < deadline and len(progress) < 3:
    time.sleep(0.05)
    p = mesh.mesh.GetComputeProgress()
    if 0. < p < 0.15 and (not progress or p > progress[-1]):
        progress.append(p)
assert len(progress) == 3, progress

smesh.CancelCompute(mesh.mesh, sphere)
worker.join(60.)
assert not worker.is_alive(), "compute not stopped by cancel"
assert result["ok"] is False, result
assert mesh.NbElements() == 0, mesh.NbElements()

# cancel while creating nodes and hexahedra (progress above 0.2): no partial mesh left
for axis in range(3):
    grid.SetNbCells(axis, 120)    # 1.7e6 cells, about 1 s of creation
result.clear()
worker = threading.Thread(target=compute)
worker.start()
deadline = time.time() + 60.
p = 0.
while time.time() < deadline and not 0.3 < p < 0.9:
    time.sleep(0.02)
    p = mesh.mesh.GetComputeProgress()
assert 0.3 < p < 0.9, p
smesh.CancelCompute(mesh.mesh, sphere)
worker.join(60.)
assert not worker.is_alive(), "compute not stopped by cancel"
assert result["ok"] is False, result
assert mesh.NbElements() == 0 and mesh.NbNodes() == 0, (mesh.NbElements(), mesh.NbNodes())

# a canceled compute does not prevent the next one
for axis in range(3):
    grid.SetNbCells(axis, 10)
assert mesh.Compute(), mesh.GetComputeErrors()
assert mesh.NbHexas() > 0
print("OK")
