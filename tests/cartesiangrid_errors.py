# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later (see COPYING)
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
m.CartesianGrid().Grid(nbCells=4, keepOutsideCells=True)
ok, msg = compute_error(m)
assert not ok and "requires a compound" in msg, msg

# shape without solid
face = geompy.MakeFaceHW(10, 10, 1)
geompy.addToStudy(face, "Face")
m = smesh.Mesh(face, "NoSolid")
m.CartesianGrid().Grid(nbCells=4)
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
m.CartesianGrid(geom=s1).Grid(nbCells=4)
ok, msg = compute_error(m)
assert not ok and "main shape" in msg, msg

# tiny step: error instead of overflow or memory exhaustion
m = smesh.Mesh(two, "TooManyCells")
m.CartesianGrid().Grid(step=1e-9)
ok, msg = compute_error(m)
assert not ok and "cells along" in msg, msg
assert m.NbElements() == 0

# per-axis counts allowed but a total no array can hold (2e9^3 cells): error before allocating
m = smesh.Mesh(two, "TooLarge")
m.CartesianGrid().Grid(nbCells=2000000000)
ok, msg = compute_error(m)
assert not ok and "is too large to be stored" in msg, msg
assert m.NbElements() == 0
# ---- Python API errors: ValueError / TypeError with a message, before any CORBA call
from salome.CartesianGridPlugin import CartesianGridPluginBuilder

def py_error(exc, func, *args, **kwargs):
    try:
        func(*args, **kwargs)
    except exc as e:
        return str(e)
    raise AssertionError("%s not raised" % exc.__name__)

# mesh with another algorithm: no Cartesian Grid 3D to export with
other = smesh.Mesh(sphere, "OtherAlgorithm")
other.Segment()
msg = py_error(ValueError, CartesianGridPluginBuilder.CartesianGridAlgorithm, other)
assert "has no CartesianGrid_3D algorithm" in msg, msg
msg = py_error(ValueError, CartesianGridPluginBuilder.ExportCartesianMED, other, "/nonexistent/x.med")
assert "has no CartesianGrid_3D algorithm" in msg, msg

# geometry groups of a shape not published in the study
unpublished = geompy.MakeBoxDXDYDZ(10, 10, 10)
corbaMesh = smesh.CreateMesh(unpublished)
corbaMesh.AddHypothesis(unpublished, smesh.CreateHypothesis("CartesianGrid_3D", "libCartesianGridEngine.so"))
msg = py_error(ValueError, CartesianGridPluginBuilder.ExportCartesianMED, corbaMesh, "/nonexistent/x.med")
assert "not published in the study" in msg, msg

# export of a mesh with the algorithm but no hypothesis: engine error, no file
import os, tempfile, SALOME
noHyp = smesh.Mesh(sphere, "NoHypothesis")
noHyp.CartesianGrid()
fNoHyp = os.path.join(tempfile.mkdtemp(), "nohyp.med")
try:
    CartesianGridPluginBuilder.ExportCartesianMED(noHyp, fNoHyp)
    raise AssertionError("export without hypothesis did not fail")
except SALOME.SALOME_Exception as e:
    assert "no Cartesian Grid Parameters hypothesis" in e.details.text, e.details.text
assert not os.path.exists(fNoHyp)

# Grid() arguments
algo = smesh.Mesh(sphere, "BadArguments").CartesianGrid()
for kwargs, text in ((dict(coordinates=[[0, 1], None]), "coordinates: list of 3"),
                     (dict(coordinates=[0, 1, 2]), "coordinates: list of 3"),
                     (dict(spacing=["1", "1"]), "spacing: one value for all axes or a list of 3"),
                     (dict(spacing=[5, None, None]), "spacing: a function string or (functions, internalPoints)"),
                     (dict(spacing=[(["1"], 0.5), None, None]), "spacing: a function string")):
    msg = py_error(TypeError, algo.Grid, **kwargs)
    assert text in msg, (kwargs, msg)
print("OK")
