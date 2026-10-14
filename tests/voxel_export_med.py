# Cartesian MED export: zone groups from geometry groups, read back with MEDLoader,
# re-import in SMESH, errors, progress and cancel
import os
import tempfile
import threading
import time
import salome
salome.salome_init()
import SALOME
import SMESH
import medcoupling as mc
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder
from salome.VoxelPlugin import VoxelPluginBuilder
geompy = geomBuilder.New()
smesh = smeshBuilder.New()

tmp = tempfile.mkdtemp()

def box(x0, y0, z0, x1, y1, z1):
    return geompy.MakeBoxTwoPnt(geompy.MakeVertex(x0, y0, z0), geompy.MakeVertex(x1, y1, z1))

# same world as voxel_four_materials: AIR box holding IRON (with LEAD inside) and WOOD
world = geompy.MakeCompound([box(-100, -100, -100, 100, 100, 100),
                             box(-90, -40, -40, -10, 40, 40),
                             box(-60, -10, -10, -40, 10, 10),
                             box(30, -60, -20, 70, -20, 20)])
geompy.addToStudy(world, "World")
solids = geompy.SubShapeAllSortedCentres(world, geompy.ShapeType["SOLID"])
by_volume = {round(geompy.BasicProperties(s)[2]): s for s in solids}
materials = {"MAT_AIR": 8000000, "MAT_IRON": 512000, "MAT_WOOD": 64000, "MAT_LEAD": 8000}
for name, vol in materials.items():
    g = geompy.CreateGroup(world, geompy.ShapeType["SOLID"])
    geompy.UnionList(g, [by_volume[vol]])
    geompy.addToStudyInFather(world, g, name)
other = geompy.CreateGroup(world, geompy.ShapeType["SOLID"])   # ignored by the MAT_ prefix
geompy.UnionList(other, [by_volume[512000]])
geompy.addToStudyInFather(world, other, "other")

expected = {"MAT_AIR": 7424, "MAT_IRON": 504, "MAT_LEAD": 8, "MAT_WOOD": 64}

def read(fileName):
    m = mc.MEDFileMesh.New(fileName)
    assert isinstance(m, mc.MEDFileCMesh), type(m)
    return m

def groupCounts(m):
    return {g: m.getGroupArr(0, g).getNumberOfTuples() for g in m.getGroupsNames()}

def raises(exc, func, *args, **kw):
    try:
        func(*args, **kw)
    except exc as e:
        return str(e.details.text if hasattr(e, "details") else e)
    raise AssertionError("no %s" % exc.__name__)

# ---- geometry source, mesh not computed: no element created
mesh = smesh.Mesh(world, "World")
algo = mesh.Voxel()
algo.Grid(nbCells=20)
fGeom = os.path.join(tmp, "geom.med")
summary = algo.ExportCartesianMED(fGeom)
print(summary)
assert "20 x 20 x 20 = 8000 cells" in summary, summary
assert "MAT_IRON: 504 cells" in summary and "no zone: 0 cells" in summary, summary
assert mesh.NbElements() == 0 and mesh.NbNodes() == 0
m = read(fGeom)
assert m.getName() == "World", m.getName()
assert groupCounts(m) == expected, groupCounts(m)
x = m.getMesh().getCoordsAt(0)
assert x.getNumberOfTuples() == 21 and x.getIJ(0, 0) == -100. and x.getIJ(20, 0) == 100.
assert m.getFamilyId("FAM_MAT_AIR") == -1 and m.getFamilyId("FAM_MAT_WOOD") == -4  # alphabetical

# same counts as Compute + groups on geometry
assert mesh.Compute(), mesh.GetComputeErrors()
for g in geompy.GetExistingSubObjects(world, True):
    if g.GetName() in expected:
        assert len(mesh.GroupOnGeom(g, g.GetName(), SMESH.VOLUME).GetListOfID()) == expected[g.GetName()]

# SMESH re-import of the Cartesian file: hexahedra with the zone groups
imported, status = smesh.CreateMeshesFromMED(fGeom)
assert status == SMESH.DRS_OK, status
assert imported[0].NbHexas() == 8000, imported[0].NbHexas()
# SMESH reads group names of a Cartesian MED grid padded with spaces to 80 characters
assert {g.GetName().strip(): g.Size() for g in imported[0].GetGroups()} == expected

# module-level function on the CORBA mesh, prefix narrowing: solids in no zone group
summary = VoxelPluginBuilder.ExportCartesianMED(mesh.GetMesh(), os.path.join(tmp, "iron.med"),
                                                zonePrefix="MAT_I")
assert "MAT_IRON: 504 cells" in summary and "no zone: 7496 cells" in summary, summary
assert "solids in no zone group: #" in summary, summary

# non-uniform grid: coordinates of the hypothesis written as is
mesh2 = smesh.Mesh(world, "NonUniform")
algo2 = mesh2.Voxel()
algo2.Grid(nbCells=4, coordinates=[[-100, -50, 0, 100], None, None], spacing=[None, "25", None])
f2 = os.path.join(tmp, "nonuniform.med")
algo2.ExportCartesianMED(f2)
m2 = read(f2)
assert list(m2.getMesh().getCoordsAt(0).getValues()) == [-100., -50., 0., 100.]
assert m2.getMesh().getCoordsAt(1).getNumberOfTuples() == 9     # 200 / 25 = 8 cells
assert m2.getMesh().getCoordsAt(2).getNumberOfTuples() == 5

# ---- errors
text = raises(SALOME.SALOME_Exception, algo.ExportCartesianMED, os.path.join(tmp, "x.med"),
              zonePrefix="")
assert "MAT_IRON" in text and "other" in text, text            # IRON solid in two zone groups
text = raises(ValueError, algo.ExportCartesianMED, os.path.join(tmp, "x.med"), zonePrefix="NOPE")
assert "NOPE" in text, text
raises(ValueError, algo.ExportCartesianMED, os.path.join(tmp, "x.med"), source="bad")
assert not os.path.exists(os.path.join(tmp, "x.med"))

# ---- progress and cancel of a long geometry export: no file, next export works
sphere = geompy.MakeSphereR(50.)
geompy.addToStudy(sphere, "Sphere")
gs = geompy.CreateGroup(sphere, geompy.ShapeType["SOLID"])
geompy.UnionList(gs, geompy.SubShapeAll(sphere, geompy.ShapeType["SOLID"]))
geompy.addToStudyInFather(sphere, gs, "MAT_BALL")
mesh3 = smesh.Mesh(sphere, "Sphere")
algo3 = mesh3.Voxel()
algo3.Grid(nbCells=400)
f3 = os.path.join(tmp, "sphere.med")
result = {}
def export():
    try:
        result["summary"] = algo3.ExportCartesianMED(f3)
    except SALOME.SALOME_Exception as e:
        result["error"] = e.details.text
worker = threading.Thread(target=export)
worker.start()
voxelAlgo = VoxelPluginBuilder.VoxelAlgorithm(mesh3)
progress = []
deadline = time.time() + 30.
while time.time() < deadline and len(progress) < 3:
    time.sleep(0.05)
    p = voxelAlgo.GetExportProgress()
    if 0. < p < 0.9 and (not progress or p > progress[-1]):
        progress.append(p)
assert len(progress) == 3, progress
voxelAlgo.CancelExport()
worker.join(60.)
assert not worker.is_alive(), "export not stopped by cancel"
assert "canceled" in result.get("error", ""), result
assert not os.path.exists(f3)
algo3.Grid(nbCells=10)
assert "1000 cells" in algo3.ExportCartesianMED(f3)
print("OK")
