# Grid modes Coordinates and Spacing: counts, node positions, coverage warning, Evaluate, dump
import os
import tempfile
import salome
salome.salome_init()
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder
import SMESH
import VoxelPlugin
geompy = geomBuilder.New()
smesh = smeshBuilder.New()

NODE = smesh.EnumToLong(SMESH.Entity_Node)
HEXA = smesh.EnumToLong(SMESH.Entity_Hexa)

box = geompy.MakeBoxDXDYDZ(10, 10, 10)
geompy.addToStudy(box, "Box")

def node_coords(mesh, axis):
    return sorted({round(mesh.GetNodeXYZ(n)[axis], 9) for n in mesh.GetNodesId()})

def warnings(mesh):
    return [e.comment for e in mesh.GetComputeErrors() if e.code == SMESH.COMPERR_WARNING._v]

def compute_like_evaluate(mesh):
    nbs = mesh.Evaluate()
    assert mesh.Compute(), mesh.GetComputeErrors()
    assert nbs[HEXA] == mesh.NbHexas(), (nbs[HEXA], mesh.NbHexas())
    assert nbs[NODE] == mesh.NbNodes(), (nbs[NODE], mesh.NbNodes())

# coordinates along X (unsorted input), 5 cells along Y and Z
mesh = smesh.Mesh(box, "GridModes")
algo = mesh.Voxel()
hyp = algo.Grid(nbCells=5, coordinates=[[10, 0, 5, 2], None, None])
compute_like_evaluate(mesh)
assert mesh.NbHexas() == 3 * 5 * 5, mesh.NbHexas()
assert node_coords(mesh, 0) == [0, 2, 5, 10], node_coords(mesh, 0)
assert not warnings(mesh), warnings(mesh)

# mixed: X coordinates, Y step, Z spacing fine below 5 and coarse above
hyp.SetStep(1, 2.5)
hyp.SetSpacing(2, ["0.5", "2"], [0.5])
compute_like_evaluate(mesh)
ys, zs = node_coords(mesh, 1), node_coords(mesh, 2)
assert ys == [0, 2.5, 5, 7.5, 10], ys
assert 5 in zs and zs[0] == 0 and zs[-1] == 10, zs
assert all(b - a < 0.6 for a, b in zip(zs, zs[1:]) if b <= 5), zs
assert all(b - a > 1.5 for a, b in zip(zs, zs[1:]) if a >= 5), zs
assert mesh.NbHexas() == 3 * 4 * (len(zs) - 1), mesh.NbHexas()

# planes covering half of the box along X: warning, compute OK, half the cells
hyp.SetCoordinates(0, [0, 2.5, 5])
compute_like_evaluate(mesh)
assert mesh.NbHexas() == 2 * 4 * (len(zs) - 1), mesh.NbHexas()
assert any("X planes cover [0, 5], solids span [0, 10]" in w for w in warnings(mesh)), warnings(mesh)
assert max(node_coords(mesh, 0)) == 5

# back to the full mixed grid for the dump
hyp.SetCoordinates(0, [0, 2, 5, 10])
assert mesh.Compute()

# Dump Study replays the new setters
dump_dir = tempfile.mkdtemp()
assert salome.myStudy.DumpStudy(dump_dir, "grid_modes_dump", True, False)
script = open(os.path.join(dump_dir, "grid_modes_dump.py")).read()
assert ".SetCoordinates(" in script and ".SetSpacing(" in script, script
namespace = {"__name__": "__main__"}
exec(compile(script, "grid_modes_dump.py", "exec"), namespace)
replayed = [v for v in namespace.values() if isinstance(v, VoxelPlugin._objref_VoxelPlugin_Hypothesis)]
assert any(r.GetMode(0) == 2 and list(r.GetCoordinates(0)) == [0, 2, 5, 10] and
           r.GetMode(1) == 1 and r.GetStep(1) == 2.5 and
           r.GetMode(2) == 3 and list(r.GetSpacing(2)[0]) == ["0.5", "2"] and
           list(r.GetSpacing(2)[1]) == [0.5]
           for r in replayed), [(r.GetMode(0), r.GetMode(1), r.GetMode(2)) for r in replayed]

# planes entirely outside the solids: compute OK, no cell, both warnings
# (after the dump: SMESH IsComputedOK() is false for an empty mesh, so a dump replaying
# this compute would stop at CheckCompute())
hyp.SetCoordinates(0, [20, 30])
assert mesh.Compute(), mesh.GetComputeErrors()
assert mesh.NbHexas() == 0, mesh.NbHexas()
w = warnings(mesh)
assert any("X planes cover [20, 30]" in x for x in w), w
assert any("own no cell" in x for x in w), w

print("OK")
