# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later (see COPYING)
# Remove vs Keep outside cells, solid owning no cell, recompute after Keep
import math
import salome
salome.salome_init()
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder
import SMESH
geompy = geomBuilder.New()
smesh = smeshBuilder.New()

sphere = geompy.MakeSphereR(50.)
compound = geompy.MakeCompound([sphere])
geompy.addToStudy(compound, "SphereCompound")

mesh = smesh.Mesh(compound, "Sphere")
algo = mesh.CartesianGrid()
algo.Grid(nbCells=20)            # step 5
assert mesh.Compute(), mesh.GetComputeErrors()
nb_in = mesh.NbHexas()
exact = 4. / 3. * math.pi * 50 ** 3 / 125.
assert abs(nb_in - exact) < 0.03 * exact, nb_in
assert all(e.code != SMESH.COMPERR_WARNING._v for e in mesh.GetComputeErrors())

# Keep: full grid, outside cells bound to the main compound, warning
algo.Grid(keepOutsideCells=True)
assert mesh.Compute()
assert mesh.NbHexas() == 8000, mesh.NbHexas()
errors = mesh.GetComputeErrors()
assert any(e.code == SMESH.COMPERR_WARNING._v and str(8000 - nb_in) in e.comment for e in errors), errors
sphere_group = mesh.GroupOnGeom(geompy.SubShapeAll(compound, geompy.ShapeType["SOLID"])[0], "in", SMESH.VOLUME)
assert sphere_group.Size() == nb_in

# back to Remove: kept outside cells must be gone (no stale elements)
algo.Grid(keepOutsideCells=False)
assert mesh.Compute()
assert mesh.NbHexas() == nb_in, mesh.NbHexas()

# small solid owning no cell -> warning, compute OK
tiny = geompy.MakeTranslation(geompy.MakeBoxDXDYDZ(0.1, 0.1, 0.1), 60, 60, 60)
two = geompy.MakeCompound([sphere, tiny])
geompy.addToStudy(two, "SphereAndTiny")
mesh2 = smesh.Mesh(two, "SphereAndTiny")
mesh2.CartesianGrid().Grid(nbCells=20)
assert mesh2.Compute(), mesh2.GetComputeErrors()
assert any(e.code == SMESH.COMPERR_WARNING._v and "own no cell" in e.comment
           for e in mesh2.GetComputeErrors()), mesh2.GetComputeErrors()
# a construction vertex in the compound does not enlarge the grid
with_vertex = geompy.MakeCompound([geompy.MakeBoxDXDYDZ(10, 10, 10), geompy.MakeVertex(100, 0, 0)])
geompy.addToStudy(with_vertex, "BoxAndVertex")
mesh3 = smesh.Mesh(with_vertex, "BoxAndVertex")
mesh3.CartesianGrid().Grid(nbCells=10)
assert mesh3.Compute(), mesh3.GetComputeErrors()
assert mesh3.NbHexas() == 1000, mesh3.NbHexas()
print("OK")
