# Cartesian MED export with one zone per solid (source "solids"): names from GEOM solids
# published under the main shape, from SHAPER sub-results (SHAPERSTUDY does not publish them),
# SOLID_<id> for unnamed solids, _<id> on duplicated names; no group and no mesh element needed
import os
import tempfile
import salome
salome.standalone()
salome.salome_init()
import medcoupling as mc
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder
from salome.CartesianGridPlugin import CartesianGridPluginBuilder as Builder
geompy = geomBuilder.New()
smesh = smeshBuilder.New()

tmp = tempfile.mkdtemp()

def counts(fileName):
    m = mc.MEDFileMesh.New(fileName)
    assert isinstance(m, mc.MEDFileCMesh), type(m)
    return {g: m.getGroupArr(0, g).getNumberOfTuples() for g in m.getGroupsNames()}

def box(x0, y0, z0, x1, y1, z1):
    return geompy.MakeBoxTwoPnt(geompy.MakeVertex(x0, y0, z0), geompy.MakeVertex(x1, y1, z1))

# ---- GEOM: same world as cartesiangrid_four_materials, no group at all
world = geompy.MakeCompound([box(-100, -100, -100, 100, 100, 100),   # AIR
                             box(-90, -40, -40, -10, 40, 40),        # IRON
                             box(-60, -10, -10, -40, 10, 10),        # LEAD
                             box(30, -60, -20, 70, -20, 20)])        # WOOD
geompy.addToStudy(world, "World")
by_volume = {round(geompy.BasicProperties(s)[2]): s
             for s in geompy.SubShapeAll(world, geompy.ShapeType["SOLID"])}
air, iron, lead, wood = (by_volume[v] for v in (8000000, 512000, 8000, 64000))
ident = {name: geompy.GetSubShapeID(world, s) for name, s in
         (("air", air), ("iron", iron), ("lead", lead), ("wood", wood))}
geompy.addToStudyInFather(world, iron, "IRON")
geompy.addToStudyInFather(world, lead, "DUP")    # same name twice: both get _<id>
geompy.addToStudyInFather(world, wood, "DUP")    # AIR stays unpublished: SOLID_<id>

mesh = smesh.Mesh(world, "World")
algo = mesh.CartesianGrid()
algo.Grid(nbCells=20)

ids = list(Builder.CartesianGridAlgorithm(mesh).GetSolidIds(mesh.GetMesh()))
assert sorted(ids) == sorted(ident.values()), (ids, ident)
names = Builder.SolidNames(mesh)
assert [i for _, i in names] == ids, names
expectedNames = {"SOLID_%d" % ident["air"]: 7424, "IRON": 504,
                 "DUP_%d" % ident["lead"]: 8, "DUP_%d" % ident["wood"]: 64}
assert sorted(n for n, _ in names) == sorted(expectedNames), names

f = os.path.join(tmp, "solids.med")
summary = Builder.ExportCartesianMED(mesh, f, source="solids")
assert summary.startswith("Note: 1 solid(s) without a name in the study or in SHAPER, named SOLID_<id>: SOLID_%d\n"
                          % ident["air"]), summary
assert 'Note: name "DUP" used by 2 solids, zones ' in summary, summary
assert "Warning" not in summary, summary
assert counts(f) == expectedNames, counts(f)
assert mesh.NbVolumes() == 0                     # nothing computed
assert "no zone: 0 cells" in summary, summary   # every cell is in some solid here

# the same through the smeshBuilder algorithm, with an unused prefix
f2 = os.path.join(tmp, "solids2.med")
algo.ExportCartesianMED(f2, source="solids", zonePrefix="IGNORED_")
assert counts(f2) == expectedNames

# a bad source names all three
try:
    Builder.ExportCartesianMED(mesh, f, source="bogus")
    raise AssertionError("no ValueError")
except ValueError as e:
    assert '"solids"' in str(e), e

# ---- single solid: the main shape name
single = box(0, 0, 0, 10, 10, 10)
geompy.addToStudy(single, "Single")
m1 = smesh.Mesh(single, "Single")
m1.CartesianGrid().Grid(nbCells=4)
assert [n for n, _ in Builder.SolidNames(m1)] == ["Single"]
f3 = os.path.join(tmp, "single.med")
s3 = Builder.ExportCartesianMED(m1, f3, source="solids")
assert counts(f3) == {"Single": 64}, counts(f3)
assert not s3.startswith("Note"), s3

# ---- long names are cut to fit MED (60 characters with the suffix)
longName = "L" * 70
geompy.addToStudyInFather(world, iron, longName)   # IRON now also published as longName
longs = [n for n, _ in Builder.SolidNames(mesh) if n.startswith("L")]
assert len(longs) == 1 and len(longs[0]) <= 60, longs
f5 = os.path.join(tmp, "long.med")
s5 = Builder.ExportCartesianMED(mesh, f5, source="solids")
assert s5.startswith('Warning: zone name cut to 60 characters (MED limit): "%s" -> "%s"' % (longName, "L" * 60)), s5
assert ("L" * 60) in counts(f5)

# ---- SHAPER: nested compounds, renamed sub-results (as GDML volume names), not in the study
from salome.shaper import model
import ModelAPI
model.begin()
partSet = model.moduleDocument()
doc = model.addPart(partSet).document()
b = [model.addBox(doc, 0, 0, 0, 100, 100, 100),      # AIR
     model.addBox(doc, -50, 0, 0, 40, 40, 40),       # IRON
     model.addBox(doc, -50, 0, 0, 10, 10, 10),       # LEAD
     model.addBox(doc, 50, -40, 0, 20, 20, 20)]      # WOOD
inner = model.addCompound(doc, [model.selection("SOLID", b[1].result().name()),
                                model.selection("SOLID", b[2].result().name())])
sworld = model.addCompound(doc, [model.selection("SOLID", b[0].result().name()),
                                 model.selection("COMPOUND", inner.result().name()),
                                 model.selection("SOLID", b[3].result().name())])
sworld.result().setName("ShaperWorld")
model.do()
body = ModelAPI.modelAPI_ResultBody(sworld.feature().firstResult())
body.subResult(0).data().setName("AIR_box")
body.subResult(1).subResult(0).data().setName("IRON_box")
body.subResult(1).subResult(1).data().setName("PART")   # LEAD and WOOD share a name
body.subResult(2).data().setName("PART")
model.end()
model.publishToShaperStudy()
import SHAPERSTUDY
published = SHAPERSTUDY.shape(model.featureStringId(sworld))
shaperWorld = published[0] if isinstance(published, (list, tuple)) else published

sm = smesh.Mesh(shaperWorld, "ShaperWorld")
sm.CartesianGrid().Grid(nbCells=20)
sids = list(Builder.CartesianGridAlgorithm(sm).GetSolidIds(sm.GetMesh()))
snames = Builder.SolidNames(sm)
assert [i for _, i in snames] == sids, snames
f4 = os.path.join(tmp, "shaper.med")
s4 = Builder.ExportCartesianMED(sm, f4, source="solids")
assert s4.startswith('Note: name "PART" used by 2 solids'), s4
c4 = counts(f4)
assert c4["AIR_box"] == 7424 and c4["IRON_box"] == 504, c4
parts = sorted((n, c) for n, c in c4.items() if n.startswith("PART_"))
assert len(parts) == 2 and sorted(c for _, c in parts) == [8, 64], c4
assert all(int(n[5:]) in sids for n, _ in parts), (parts, sids)
assert len(c4) == 4, c4

print("cartesiangrid_export_solids OK")
