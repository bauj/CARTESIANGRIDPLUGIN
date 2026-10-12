# Invalid inputs give compute errors, never crashes
import salome
salome.salome_init()
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder
import SMESH
geompy = geomBuilder.New()
smesh = smeshBuilder.New()

def compute_error(mesh):
    ok = mesh.Compute()
    errors = [e.comment for e in mesh.GetComputeErrors() if e.code != SMESH.COMPERR_WARNING._v]
    return ok, " ".join(errors)

# Keep on a single solid (not a compound)
sphere = geompy.MakeSphereR(10.)
geompy.addToStudy(sphere, "Sphere")
m = smesh.Mesh(sphere, "KeepOnSolid")
m.Voxel().Grid(nbCells=4, keepOutsideCells=True)
ok, msg = compute_error(m)
assert not ok and "requires a compound" in msg, msg

# shape without solid
face = geompy.MakeFaceHW(10, 10, 1)
geompy.addToStudy(face, "Face")
m = smesh.Mesh(face, "NoSolid")
m.Voxel().Grid(nbCells=4)
# SMESH never calls a 3D algorithm without solid: it reports the missing algorithms instead
m.Compute()
assert m.NbElements() == 0
assert m.GetAlgoState(face), "missing algorithm not reported"

# test_algo_on_subshape: algorithm only on one solid of a compound
b1 = geompy.MakeBoxDXDYDZ(10, 10, 10)
b2 = geompy.MakeTranslation(geompy.MakeBoxDXDYDZ(10, 10, 10), 20, 0, 0)
two = geompy.MakeCompound([b1, b2])
geompy.addToStudy(two, "TwoBoxes")
s1 = geompy.SubShapeAllSortedCentres(two, geompy.ShapeType["SOLID"])[0]
geompy.addToStudyInFather(two, s1, "s1")
m = smesh.Mesh(two, "OnSubShape")
m.Voxel(geom=s1).Grid(nbCells=4)
ok, msg = compute_error(m)
assert not ok and "main shape" in msg, msg
print("OK")
