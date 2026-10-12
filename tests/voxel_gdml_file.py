# GDML file -> SHAPER ImportCSG -> MAT_* groups -> SHAPERSTUDY -> Voxel_3D -> mesh groups
# data/four_materials.gdml is written by misc/make_four_materials_gdml.py (same world as
# voxel_shaper_gdml.py: wood LV placed twice, faces on multiples of 10)
import os
import salome
salome.standalone()
salome.salome_init()

from salome.shaper import model
if not hasattr(model, "createMaterialGroups"):
    print("SKIP: SHAPER without GDML material groups")
    raise SystemExit(0)
from ModelAPI import ModelAPI_Session

GDML_FILE = os.path.join(os.path.dirname(os.path.abspath(__file__)), "data", "four_materials.gdml")

class Volume:
    """minimal wrapper giving a ModelAPI feature the .feature() of model API objects"""
    def __init__(self, theFeature):
        self._feature = theFeature
    def feature(self):
        return self._feature

model.begin()
Part_1_doc = model.addPart(model.moduleDocument()).document()
model.end()
aSession = ModelAPI_Session.get()
aSession.startOperation("Import GDML")
anImport = Part_1_doc.addFeature("ImportCSG")
anImport.string("csg_import_format").setValue("GDML")
anImport.string("gdml_import_file_path").setValue(GDML_FILE)
aSession.finishOperation()

worlds = [f for f in Part_1_doc.allFeatures() if f.getKind() == "GDML_WorldVolume"]
assert len(worlds) == 1, len(worlds)
world = Volume(worlds[0])
model.createMaterialGroups(Part_1_doc, world)
model.publishToShaperStudy()

import SHAPERSTUDY
import SMESH
from salome.smesh import smeshBuilder
smesh = smeshBuilder.New()

published = SHAPERSTUDY.shape(model.featureStringId(world))
shape = published[0] if isinstance(published, (list, tuple)) else published

mesh = smesh.Mesh(shape, "World")
mesh.Voxel().Grid(nbCells=20)
assert mesh.Compute(), mesh.GetComputeErrors()

shaperBuilder = salome.lcc.FindOrLoadComponent("FactoryServer", "SHAPERSTUDY")
counts = {}
for g in shaperBuilder.GetIShapesOperations().GetExistingSubObjects(shape, True):
    counts[g.GetName()] = mesh.GroupOnGeom(g, g.GetName(), SMESH.VOLUME).Size()
expected = {"MAT_AIR": 7360, "MAT_IRON": 504, "MAT_LEAD": 8, "MAT_WOOD": 128}
assert counts == expected, counts
assert mesh.NbHexas() == 8000, mesh.NbHexas()
print("OK")
