# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later (see COPYING)
# Cartesian grid of a block holding a lead sphere, one mesh group and one MED zone per material

import os
import tempfile

import salome
salome.salome_init()

from salome.geom import geomBuilder
geompy = geomBuilder.New()

from salome.smesh import smeshBuilder
smesh = smeshBuilder.New()

import SMESH

# geometry: a block and a sphere inside it, in one compound (nested solids: the smallest one owns its cells)
block = geompy.MakeBoxDXDYDZ(100, 100, 50)
sphere = geompy.MakeSpherePntR(geompy.MakeVertex(50, 50, 25), 20)
world = geompy.MakeCompound([block, sphere])
geompy.addToStudy(world, "World")

# one group of solids per material, named with the zone prefix MAT_
solids = geompy.SubShapeAllSortedCentres(world, geompy.ShapeType["SOLID"])
byVolume = sorted(solids, key=lambda s: geompy.BasicProperties(s)[2])
for name, solid in (("MAT_LEAD", byVolume[0]), ("MAT_STEEL", byVolume[1])):
    group = geompy.CreateGroup(world, geompy.ShapeType["SOLID"])
    geompy.UnionList(group, [solid])
    geompy.addToStudyInFather(world, group, name)

# mesh: Cartesian Grid 3D algorithm and its Cartesian Grid Parameters hypothesis
mesh = smesh.Mesh(world, "World")
algo = mesh.CartesianGrid()
hyp = algo.Grid(nbCells=[40, 40, 20])   # X and Y: 40 cells, Z: 20 cells
hyp.SetStep(2, 2.5)                     # Z (axis 2) by step instead: 20 cells of 2.5

counts = mesh.Evaluate()                # exact counts before computing, indexed by SMESH.EntityType
print("expected nodes:", counts[smesh.EnumToLong(SMESH.Entity_Node)],
      "hexahedra:", counts[smesh.EnumToLong(SMESH.Entity_Hexa)])
assert mesh.Compute(), mesh.GetComputeErrors()
print("hexahedra:", mesh.NbHexas())

# one mesh group per geometry group (Create all groups on geometry in the GUI)
for group in geompy.GetGroups(world):
    mesh.GroupOnGeom(group, group.GetName(), SMESH.VOLUME)

# Cartesian MED file: three coordinate arrays and one cell group per zone, no mesh element needed
with tempfile.TemporaryDirectory() as folder:
    fileName = os.path.join(folder, "world.med")
    print(algo.ExportCartesianMED(fileName, source="geometry", zonePrefix="MAT_"))
    assert os.path.getsize(fileName) > 0

if salome.sg.hasDesktop():
    salome.sg.updateObjBrowser()
