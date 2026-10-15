# Evaluate predicts the node and hexahedron counts of Compute
import salome
salome.salome_init()
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder
import SMESH
geompy = geomBuilder.New()
smesh = smeshBuilder.New()

NODE = smesh.EnumToLong(SMESH.Entity_Node)
HEXA = smesh.EnumToLong(SMESH.Entity_Hexa)

# world sphere with a nested box and a box sticking out of it
world = geompy.MakeSphereR(50.)
inner = geompy.MakeBoxTwoPnt(geompy.MakeVertex(-10, -10, -10), geompy.MakeVertex(10, 10, 10))
out = geompy.MakeBoxTwoPnt(geompy.MakeVertex(30, 30, 30), geompy.MakeVertex(60, 60, 60))
shape = geompy.MakeCompound([world, inner, out])
geompy.addToStudy(shape, "Nested")

mesh = smesh.Mesh(shape, "Nested")
algo = mesh.CartesianGrid()
for keep in (False, True):
    algo.Grid(nbCells=[22, 23, 24], keepOutsideCells=keep)
    nbs = mesh.Evaluate()
    assert mesh.Compute(), mesh.GetComputeErrors()
    assert nbs[HEXA] == mesh.NbHexas(), (keep, nbs[HEXA], mesh.NbHexas())
    assert nbs[NODE] == mesh.NbNodes(), (keep, nbs[NODE], mesh.NbNodes())
    assert sum(nbs) == nbs[NODE] + nbs[HEXA], nbs
assert mesh.NbHexas() == 22 * 23 * 24

# a failing Evaluate predicts nothing
algo.Grid(step=1e-9)
mesh.Clear()
nbs = mesh.Evaluate()
assert nbs[HEXA] == 0 and nbs[NODE] == 0, nbs
# SMESH replaces the algorithm message by a generic one
assert any("Failed to evaluate" in e.comment for e in mesh.GetComputeErrors()), mesh.GetComputeErrors()

# a failed Evaluate does not spoil the next Compute
algo.Grid(nbCells=10)
assert mesh.Compute(), mesh.GetComputeErrors()
assert mesh.NbHexas() > 0
print("OK")
