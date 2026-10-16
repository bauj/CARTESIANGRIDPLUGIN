# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later (see COPYING)
# Four overlapping boxes (GDML-like nesting): owner = smallest containing solid,
# groups on geometry become mesh groups, recompute leaves no stale element
import salome
salome.salome_init()
import GEOM
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder
import SMESH
geompy = geomBuilder.New()
smesh = smeshBuilder.New()

def box(x0, y0, z0, x1, y1, z1):
    return geompy.MakeBoxTwoPnt(geompy.MakeVertex(x0, y0, z0), geompy.MakeVertex(x1, y1, z1))

world = geompy.MakeCompound([box(-100, -100, -100, 100, 100, 100),   # AIR
                             box(-90, -40, -40, -10, 40, 40),        # IRON
                             box(-60, -10, -10, -40, 10, 10),        # LEAD
                             box(30, -60, -20, 70, -20, 20)])        # WOOD
geompy.addToStudy(world, "World")
solids = geompy.SubShapeAllSortedCentres(world, geompy.ShapeType["SOLID"])
assert len(solids) == 4
# identify solids by volume: AIR 8e6, IRON 512000, WOOD 64000, LEAD 8000
by_volume = {round(geompy.BasicProperties(s)[2]): s for s in solids}
materials = {"MAT_AIR": 8000000, "MAT_IRON": 512000, "MAT_WOOD": 64000, "MAT_LEAD": 8000}
groups = {}
for name, vol in materials.items():
    g = geompy.CreateGroup(world, geompy.ShapeType["SOLID"])
    geompy.UnionList(g, [by_volume[vol]])
    geompy.addToStudyInFather(world, g, name)
    groups[name] = g

mesh = smesh.Mesh(world, "World")
algo = mesh.CartesianGrid()
algo.Grid(nbCells=20)
assert mesh.Compute(), mesh.GetComputeErrors()

expected = {"MAT_AIR": 7424, "MAT_IRON": 504, "MAT_LEAD": 8, "MAT_WOOD": 64}
def check_groups():
    seen = set()
    for name, g in groups.items():
        mg = mesh.GroupOnGeom(g, name, SMESH.VOLUME)
        ids = mg.GetListOfID()
        assert len(ids) == expected[name], (name, len(ids))
        assert not seen.intersection(ids), name
        seen.update(ids)
        mesh.RemoveGroup(mg)
    assert mesh.NbHexas() == 8000 == mesh.NbVolumes(), mesh.NbHexas()
    assert mesh.NbNodes() == 21 ** 3, mesh.NbNodes()
    assert abs(smesh.GetVolume(mesh) - 200.0 ** 3) < 1e-6 * 200.0 ** 3
    bad = smesh.GetFilter(SMESH.VOLUME, SMESH.FT_BadOrientedVolume)
    assert len(mesh.GetIdsFromFilter(bad)) == 0
check_groups()

# test_recompute: change the hypothesis, compute again, then restore
algo.Grid(nbCells=10)
assert mesh.Compute()
assert mesh.NbHexas() == 1000 and mesh.NbNodes() == 11 ** 3, (mesh.NbHexas(), mesh.NbNodes())
algo.Grid(step=10.)          # same grid as 20 cells, through Step mode
assert mesh.Compute()
check_groups()
print("OK")
