# SHAPER GDML World -> MAT_* groups -> SHAPERSTUDY -> CartesianGrid_3D -> mesh groups
import salome
salome.standalone()
salome.salome_init()

from salome.shaper import model
if not hasattr(model, "createMaterialGroups"):
    print("SKIP: SHAPER without GDML material groups")
    raise SystemExit(0)

def compoundOf(theVolume):
    return model.selection("COMPOUND", theVolume.feature().firstResult().data().name())

# World box 200 (AIR) holding iron box 80 at x=-50 (IRON) with lead box 20 inside (LEAD),
# and the same wood box 40 (WOOD) placed twice; GDML boxes are centered on their origin.
# All faces lie on multiples of 10, cell centers (20 cells per axis) on 5 mod 10.
model.begin()
partSet = model.moduleDocument()
Part_1_doc = model.addPart(partSet).document()
aWorldBox = model.addGDMLBox(Part_1_doc, 200, 200, 200)
aIronBox = model.addGDMLBox(Part_1_doc, 80, 80, 80)
aLeadBox = model.addGDMLBox(Part_1_doc, 20, 20, 20)
aWoodBox = model.addGDMLBox(Part_1_doc, 40, 40, 40)
aLead = model.addLogicalVolume(Part_1_doc, model.selection("SOLID", aLeadBox.result().name()), "LEAD")
aIron = model.addLogicalVolume(Part_1_doc, model.selection("SOLID", aIronBox.result().name()), "IRON")
aIron.addPhysVol(compoundOf(aLead), 0, 0, 0, 0, 0, 0)
aWood = model.addLogicalVolume(Part_1_doc, model.selection("SOLID", aWoodBox.result().name()), "WOOD")
aWorld = model.addWorldVolume(Part_1_doc, model.selection("SOLID", aWorldBox.result().name()), "AIR")
aWorld.addPhysVol(compoundOf(aIron), -50, 0, 0, 0, 0, 0)
aWorld.addPhysVol(compoundOf(aWood), 50, -40, 0, 0, 0, 0)
aWorld.addPhysVol(compoundOf(aWood), 50, 40, 0, 0, 0, 0)
model.end()
model.createMaterialGroups(Part_1_doc, aWorld)   # runs its own operation
model.publishToShaperStudy()

import SHAPERSTUDY
import SMESH
from salome.smesh import smeshBuilder
smesh = smeshBuilder.New()

published = SHAPERSTUDY.shape(model.featureStringId(aWorld))
world = published[0] if isinstance(published, (list, tuple)) else published

mesh = smesh.Mesh(world, "World")
mesh.CartesianGrid().Grid(nbCells=20)
assert mesh.Compute(), mesh.GetComputeErrors()

# "Create all groups on geometry": every SHAPERSTUDY group of the shape -> mesh group
shaperBuilder = salome.lcc.FindOrLoadComponent("FactoryServer", "SHAPERSTUDY")
geomGroups = shaperBuilder.GetIShapesOperations().GetExistingSubObjects(world, True)
counts = {}
seen = set()
for g in geomGroups:
    name = g.GetName()
    mg = mesh.GroupOnGeom(g, name, SMESH.VOLUME)
    ids = mg.GetListOfID()
    assert not seen.intersection(ids), name
    seen.update(ids)
    counts[name] = len(ids)
expected = {"MAT_AIR": 7360, "MAT_IRON": 504, "MAT_LEAD": 8, "MAT_WOOD": 128}
assert counts == expected, counts
assert sum(counts.values()) == mesh.NbHexas() == 8000, mesh.NbHexas()

# Cartesian MED export from the SHAPERSTUDY geometry groups: same counts
import os, tempfile
import medcoupling as mc
from salome.CartesianGridPlugin import CartesianGridPluginBuilder
f = os.path.join(tempfile.mkdtemp(), "world.med")
summary = CartesianGridPluginBuilder.ExportCartesianMED(mesh, f)
m = mc.MEDFileMesh.New(f)
assert {g: m.getGroupArr(0, g).getNumberOfTuples() for g in m.getGroupsNames()} == expected, summary
print("OK")
