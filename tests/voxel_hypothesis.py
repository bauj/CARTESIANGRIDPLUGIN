# VoxelGrid hypothesis through CORBA and the Python builder
import salome
salome.salome_init()
from salome.geom import geomBuilder
from salome.smesh import smeshBuilder
import SALOME
geompy = geomBuilder.New()
smesh = smeshBuilder.New()

hyp = smesh.CreateHypothesis("VoxelGrid", "libVoxelEngine.so")
assert hyp.GetNbCells(0) == 10 and hyp.GetMode(0) == 0
hyp.SetStep(1, 2.5)
assert hyp.GetMode(1) == 1 and hyp.GetStep(1) == 2.5
hyp.SetKeepOutsideCells(True)
assert hyp.GetKeepOutsideCells()
try:
    hyp.SetNbCells(3, 4)
    raise AssertionError("axis 3 accepted")
except SALOME.SALOME_Exception:
    pass

# ---- grid modes Coordinates and Spacing through CORBA and the builder
hm = smesh.Mesh(geompy.MakeBoxDXDYDZ(10, 10, 10), "GridModes")
algo_m = hm.Voxel()
hyp_m = algo_m.Grid(nbCells=5, coordinates=[[10, 0, 5, 2], None, None],
                    spacing=[None, None, (["0.5", "2"], [0.5])])
assert [hyp_m.GetMode(a) for a in range(3)] == [2, 0, 3], [hyp_m.GetMode(a) for a in range(3)]
assert list(hyp_m.GetCoordinates(0)) == [0, 2, 5, 10]
funcs, points = hyp_m.GetSpacing(2)
assert list(funcs) == ["0.5", "2"] and list(points) == [0.5], (funcs, points)
algo_m.Grid(spacing=[None, "1", None], step=[None, None, None])
assert hyp_m.GetMode(1) == 3 and list(hyp_m.GetSpacing(1)[0]) == ["1"]
for bad in (lambda: hyp_m.SetCoordinates(0, [1.]),
            lambda: hyp_m.SetCoordinates(0, [1., 1.]),
            lambda: hyp_m.SetCoordinates(0, [0., float("nan")]),
            lambda: hyp_m.SetSpacing(2, ["0.5-t"], []),
            lambda: hyp_m.SetSpacing(2, ["t+"], []),
            lambda: hyp_m.SetSpacing(2, ["1", "1"], [])):
    try:
        bad()
        raise AssertionError("no exception")
    except SALOME.SALOME_Exception:
        pass
assert list(hyp_m.GetCoordinates(0)) == [0, 2, 5, 10] and hyp_m.GetMode(2) == 3

# one spacing string for all axes, like nbCells/step; wrong shapes raise TypeError
algo_m.Grid(spacing="0.5")
assert [hyp_m.GetMode(a) for a in range(3)] == [3, 3, 3]
assert all(list(hyp_m.GetSpacing(a)[0]) == ["0.5"] for a in range(3))
for bad in (lambda: algo_m.Grid(coordinates=[0, 5, 10]),
            lambda: algo_m.Grid(coordinates=[[0, 5], None]),
            lambda: algo_m.Grid(spacing={"x": 1}),
            lambda: algo_m.Grid(spacing=[None, None, ("0.5", [])]),
            lambda: algo_m.Grid(spacing=[None, None, 0.5])):
    try:
        bad()
        raise AssertionError("no TypeError")
    except TypeError:
        pass
print("OK")
