# VoxelGrid hypothesis through CORBA and the Python builder
import salome
salome.salome_init()
from salome.smesh import smeshBuilder
import SALOME
smesh = smeshBuilder.New()

hyp = smesh.CreateHypothesis("VoxelGrid", "libVoxelEngine.so")
assert hyp.GetNbCells(0) == 10 and hyp.GetMode(0) == 0
hyp.SetStep(1, 2.5)
assert hyp.GetMode(1) == 1 and hyp.GetStep(1) == 2.5
hyp.SetKeepOutsideCells(True)
assert hyp.GetKeepOutsideCells()
try:
    hyp.SetNbCells(3, 4)
    raise AssertionError("axis 3 accepted")
except SALOME.SALOME_Exception:
    pass
print("OK")
