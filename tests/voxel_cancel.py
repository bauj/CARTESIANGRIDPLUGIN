# Progress and cancel of a long compute
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
grid = mesh.Voxel().Grid(nbCells=300)   # 2.7e7 cells: far longer than the test waits

result = {}
def compute():
    result["ok"] = mesh.Compute()
worker = threading.Thread(target=compute)
worker.start()

# progress goes up while computing
progress = []
deadline = time.time() + 30.
while time.time() < deadline and len(progress) < 3:
    time.sleep(0.5)
    p = mesh.mesh.GetComputeProgress()
    if 0. < p < 1.:
        progress.append(p)
assert len(progress) == 3, progress
assert progress == sorted(progress), progress

smesh.CancelCompute(mesh.mesh, sphere)
worker.join(60.)
assert not worker.is_alive(), "compute not stopped by cancel"
assert result["ok"] is False, result
assert mesh.NbElements() == 0, mesh.NbElements()

# cancel while creating nodes and hexahedra (progress above 0.8): no partial mesh left
for axis in range(3):
    grid.SetNbCells(axis, 120)    # 1.7e6 cells, about 1 s of creation
result.clear()
worker = threading.Thread(target=compute)
worker.start()
deadline = time.time() + 60.
p = 0.
while time.time() < deadline and not 0.82 < p < 0.97:
    time.sleep(0.02)
    p = mesh.mesh.GetComputeProgress()
assert 0.82 < p < 0.97, p
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
