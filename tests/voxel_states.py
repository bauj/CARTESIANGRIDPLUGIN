# Sub-mesh states across hypothesis and algorithm changes
import salome
salome.salome_init()
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder
import SMESH
geompy = geomBuilder.New()
smesh = smeshBuilder.New()

def warnings(mesh):
    return " ".join(e.comment for e in mesh.GetComputeErrors() if e.code == SMESH.COMPERR_WARNING._v)

def errors(mesh):
    return " ".join(e.comment for e in mesh.GetComputeErrors() if e.code != SMESH.COMPERR_WARNING._v)

big = geompy.MakeBoxDXDYDZ(10, 10, 10)
tiny = geompy.MakeTranslation(geompy.MakeBoxDXDYDZ(1, 1, 1), 12, 0, 0)
shape = geompy.MakeCompound([big, tiny])
geompy.addToStudy(shape, "BigAndTiny")
tiny_solid = geompy.SubShapeAllSortedCentres(shape, geompy.ShapeType["SOLID"])[1]
geompy.addToStudyInFather(shape, tiny_solid, "tiny")

def refine(grid):   # step 1: tiny owns one cell
    for axis, n in enumerate((13, 10, 10)):
        grid.SetNbCells(axis, n)

def coarsen(grid):  # one cell, center in big
    for axis in range(3):
        grid.SetNbCells(axis, 1)

mesh = smesh.Mesh(shape, "BigAndTiny")
algo = mesh.Voxel()
grid = algo.Grid(nbCells=[1, 1, 1])
tiny_group = mesh.GroupOnGeom(tiny_solid, "tiny", SMESH.VOLUME)

# coarse grid: tiny owns no cell, warning only
assert mesh.Compute(), errors(mesh)
assert "own no cell" in warnings(mesh), mesh.GetComputeErrors()
assert tiny_group.Size() == 0

# refined grid: the solid owning no cell before gets cells
refine(grid)
assert mesh.Compute(), errors(mesh)
assert tiny_group.Size() > 0 and "own no cell" not in warnings(mesh)

# and back to coarse
coarsen(grid)
assert mesh.Compute(), errors(mesh)
assert tiny_group.Size() == 0

# hypothesis removed: nothing left, missing hypothesis in the algorithm state (like Body
# Fitting, SMESH Compute() still returns True); back: compute OK
mesh.RemoveHypothesis(grid)
mesh.Compute()
assert mesh.NbElements() == 0, mesh.NbElements()
assert any(e.state == SMESH.HYP_MISSING for e in mesh.GetAlgoState(shape)), mesh.GetAlgoState(shape)
mesh.AddHypothesis(grid)
refine(grid)
assert mesh.Compute(), errors(mesh)
assert tiny_group.Size() > 0

# failed compute after Keep: no stale outside cells
grid.SetKeepOutsideCells(True)
assert mesh.Compute(), errors(mesh)
nb_keep = mesh.NbHexas()
grid.SetStep(0, 1e-9)
assert not mesh.Compute()
assert mesh.NbElements() == 0, mesh.NbElements()
refine(grid)
assert mesh.Compute(), errors(mesh)
assert mesh.NbHexas() == nb_keep
grid.SetKeepOutsideCells(False)

# Voxel_3D replaced on a mesh with a solid owning no cell: the solid is meshed again
coarsen(grid)
assert mesh.Compute(), errors(mesh)
assert tiny_group.Size() == 0
mesh.RemoveHypothesis(algo.GetAlgorithm())
mesh.RemoveHypothesis(grid)
mesh.Segment().NumberOfSegments(2)
mesh.Quadrangle()
mesh.Hexahedron()
assert mesh.Compute(), errors(mesh)
assert mesh.NbHexas() == 16, mesh.NbHexas()
assert tiny_group.Size() == 8, tiny_group.Size()
print("OK")
