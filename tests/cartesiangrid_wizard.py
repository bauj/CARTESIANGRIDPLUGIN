# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later (see COPYING)
# Cartesian Grid Wizard run logic (no GUI): meshes found on a geometry, export without and with
# the SMESH mesh, groups on geometry (solid groups only, no duplicates), compute failure; GEOM and
# SHAPER geometries
import os
import tempfile
import salome
salome.standalone()
salome.salome_init()
import SMESH
import medcoupling as mc
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder
from salome.CartesianGridPlugin import CartesianGridPluginBuilder as Builder
from salome.CartesianGridPlugin.CartesianGridPluginWizard import WizardRun, newMesh
geompy = geomBuilder.New()
smesh = smeshBuilder.New()

tmp = tempfile.mkdtemp()
expected = {"MAT_AIR": 7424, "MAT_IRON": 504, "MAT_LEAD": 8, "MAT_WOOD": 64}

def box(x0, y0, z0, x1, y1, z1):
    return geompy.MakeBoxTwoPnt(geompy.MakeVertex(x0, y0, z0), geompy.MakeVertex(x1, y1, z1))

def medGroups(fileName):
    m = mc.MEDFileMesh.New(fileName)
    assert isinstance(m, mc.MEDFileCMesh), type(m)
    return {g: m.getGroupArr(0, g).getNumberOfTuples() for g in m.getGroupsNames()}

def meshGroups(mesh):
    return {g.GetName().strip(): g.Size() for g in mesh.GetGroups()}

def raises(func):
    try:
        func()
    except RuntimeError as e:
        return str(e)
    raise AssertionError("no RuntimeError")

# ---- GEOM world of cartesiangrid_four_materials, plus a face group and a second solid group
world = geompy.MakeCompound([box(-100, -100, -100, 100, 100, 100),
                             box(-90, -40, -40, -10, 40, 40),
                             box(-60, -10, -10, -40, 10, 10),
                             box(30, -60, -20, 70, -20, 20)])
geompy.addToStudy(world, "World")
solids = geompy.SubShapeAllSortedCentres(world, geompy.ShapeType["SOLID"])
by_volume = {round(geompy.BasicProperties(s)[2]): s for s in solids}
for name, vol in {"MAT_AIR": 8000000, "MAT_IRON": 512000, "MAT_WOOD": 64000, "MAT_LEAD": 8000}.items():
    g = geompy.CreateGroup(world, geompy.ShapeType["SOLID"])
    geompy.UnionList(g, [by_volume[vol]])
    geompy.addToStudyInFather(world, g, name)
other = geompy.CreateGroup(world, geompy.ShapeType["SOLID"])
geompy.UnionList(other, [by_volume[512000]])
geompy.addToStudyInFather(world, other, "other")
faces = geompy.CreateGroup(world, geompy.ShapeType["FACE"])
geompy.UnionList(faces, geompy.SubShapeAll(by_volume[8000000], geompy.ShapeType["FACE"])[:1])
geompy.addToStudyInFather(world, faces, "faces")

# ---- meshes found on the geometry
assert Builder.CartesianGridMeshesOn(world) == []
hexa = smesh.Mesh(world, "Hexa")
hexa.Segment().NumberOfSegments(2)
hexa.Hexahedron()
mesh = newMesh(world, "W1")
found = Builder.CartesianGridMeshesOn(world)
assert [n for n, _ in found] == ["W1"], found
assert found[0][1]._is_equivalent(mesh.GetMesh())
hyp = Builder.CartesianGridParametersOf(mesh)
assert hyp is not None and Builder.CartesianGridParametersOf(hexa) is None
for axis in range(3):
    hyp.SetNbCells(axis, 20)

# ---- export only: no SMESH cells
f1 = os.path.join(tmp, "w1.med")
summary = WizardRun(mesh, f1, False, "MAT_").run()
assert medGroups(f1) == expected, medGroups(f1)
assert mesh.NbVolumes() == 0
assert summary.startswith("MED file: "), summary

# ---- export with the SMESH mesh: groups of solids created, face group skipped
f2 = os.path.join(tmp, "w2.med")
summary = WizardRun(mesh, f2, True, "MAT_").run()
assert medGroups(f2) == expected
assert mesh.NbVolumes() == 8000, mesh.NbVolumes()
groups = meshGroups(mesh)
assert groups == dict(expected, other=504), groups
assert "  mesh groups from the geometry groups: " in summary and "  group faces skipped: no cell" in summary, summary
assert summary.startswith("SMESH mesh: 8000 hexahedra\n"), summary
assert "\n\nMED file: %s\n  zones: geometry groups named MAT_*\n  Cartesian grid 20 x 20 x 20 = 8000 cells\n" % f2 in summary, summary

# ---- run again: no duplicate group
again = WizardRun(mesh, f2, True, "MAT_")
summary = again.run()
assert meshGroups(mesh) == groups, meshGroups(mesh)
assert "  group MAT_AIR skipped: a mesh group of that name exists" in summary, summary

# ---- a second mesh on the same geometry
mesh2 = newMesh(world, "W2")
assert [n for n, _ in Builder.CartesianGridMeshesOn(world)] == ["W1", "W2"]

# ---- invalid hypothesis (Keep outside cells on a non-compound shape): export error, no compute
single = box(0, 0, 0, 10, 10, 10)
geompy.addToStudy(single, "Single")
g = geompy.CreateGroup(single, geompy.ShapeType["SOLID"])
geompy.UnionList(g, [geompy.SubShapeAll(single, geompy.ShapeType["SOLID"])[0]])
geompy.addToStudyInFather(single, g, "MAT_X")
bad = newMesh(single, "Bad")
Builder.CartesianGridParametersOf(bad).SetKeepOutsideCells(True)
f3 = os.path.join(tmp, "bad.med")
msg = raises(WizardRun(bad, f3, True, "MAT_").run)
assert "Keep outside cells requires a compound" in msg, msg
assert not os.path.exists(f3)
assert bad.NbVolumes() == 0

# ---- compute failure after the export: the MED file stays, the report warns
Builder.CartesianGridParametersOf(bad).SetKeepOutsideCells(False)
bad.Compute = lambda: False   # stands for a compute error (e.g. memory)
badRun = WizardRun(bad, f3, True, "MAT_")
summary = badRun.run()
assert summary.startswith("SMESH mesh: not computed"), summary
assert badRun.parts[0][0][0] in ("NOT_COMPUTED", "NOT_COMPUTED_DETAILS"), badRun.parts
assert "\n\nMED file: " + f3 in summary, summary
assert medGroups(f3) == {"MAT_X": 1000}, medGroups(f3)

# ---- no zone group with the prefix: grid only, with a warning
f5 = os.path.join(tmp, "none.med")
gridOnly = WizardRun(mesh2, f5, False, "NOPE_")
summary = gridOnly.run()
assert "no geometry group named NOPE_*" in summary and "grid only" in summary, summary
assert medGroups(f5) == {}, medGroups(f5)

# ---- zones from the solids (no group needed): unpublished solids are SOLID_<id>
f6 = os.path.join(tmp, "solids.med")
summary = WizardRun(mesh, f6, False, "IGNORED_", "solids").run()   # 20 cells per axis
ids = {vol: geompy.GetSubShapeID(world, s) for vol, s in by_volume.items()}
assert medGroups(f6) == {"SOLID_%d" % ids[8000000]: 7424, "SOLID_%d" % ids[512000]: 504,
                         "SOLID_%d" % ids[8000]: 8, "SOLID_%d" % ids[64000]: 64}, medGroups(f6)
assert "  zones: one per solid, named as in the study or SHAPER\n" in summary, summary
assert "\n  Note: 4 solid(s) without a name" in summary, summary

# ---- report messages: level of the message box, French text from CartesianGridPlugin_msg_fr.qm
from salome.CartesianGridPlugin.CartesianGridPluginWizard import reportLevel, reportText
from qtsalome import QCoreApplication, QTranslator
assert reportLevel(again.parts) == "info", again.parts           # groups created / skipped
assert reportLevel(gridOnly.parts) == "warning", gridOnly.parts
assert reportLevel(badRun.parts) == "warning", badRun.parts           # compute failed, file written
app = QCoreApplication.instance() or QCoreApplication([])
fr = QTranslator()
assert fr.load(os.path.join(os.environ["CARTESIANGRIDPLUGIN_ROOT_DIR"], "share", "salome", "resources",
                            "cartesiangridplugin", "CartesianGridPlugin_msg_fr.qm"))
french = reportText(again.parts,
                    lambda key: fr.translate("CartesianGridPluginWizard", "CARTESIANGRID_REPORT_" + key) or None)
assert french.startswith("Maillage SMESH : 8000 hexaèdres\n"), french
assert "\nFichier MED : " in french and "\n  Grille cartésienne 20 x 20 x 20 = 8000 mailles\n" in french, french
assert "  groupe MAT_AIR ignoré : un groupe de maillage de ce nom existe" in french, french
assert "  MAT_IRON : 504 mailles" in french, french

# ---- SHAPER geometry, as cartesiangrid_four_materials_shaper
from salome.shaper import model
import SHAPERSTUDY
model.begin()
partSet = model.moduleDocument()
doc = model.addPart(partSet).document()
boxes = [model.addBox(doc, 0, 0, 0, 100, 100, 100), model.addBox(doc, -50, 0, 0, 40, 40, 40),
         model.addBox(doc, -50, 0, 0, 10, 10, 10), model.addBox(doc, 50, -40, 0, 20, 20, 20)]
shaperWorld = model.addCompound(doc, [model.selection("SOLID", b.result().name()) for b in boxes])
shaperWorld.result().setName("World")
model.do()
for i, name in enumerate(["MAT_AIR", "MAT_IRON", "MAT_LEAD", "MAT_WOOD"]):
    grp = model.addGroup(doc, [model.selection("SOLID", "Compound_1_1_%d" % (i + 1))])
    grp.setName(name)
    grp.result().setName(name)
model.end()
model.publishToShaperStudy()
published = SHAPERSTUDY.shape(model.featureStringId(shaperWorld))
sworld = published[0] if isinstance(published, (list, tuple)) else published

smesh3 = newMesh(sworld, "S1")
assert [n for n, _ in Builder.CartesianGridMeshesOn(sworld)] == ["S1"]
Builder.CartesianGridMeshesOn(world)       # GEOM meshes not mixed in
assert [n for n, _ in Builder.CartesianGridMeshesOn(world)] == ["W1", "W2"]
h3 = Builder.CartesianGridParametersOf(smesh3)
for axis in range(3):
    h3.SetNbCells(axis, 20)
f4 = os.path.join(tmp, "shaper.med")
WizardRun(smesh3, f4, True, "MAT_").run()
assert medGroups(f4) == expected, medGroups(f4)
assert meshGroups(smesh3) == expected, meshGroups(smesh3)
print("OK")
