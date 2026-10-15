# voxel_four_materials with SHAPER: four overlapping boxes in a compound, MAT_* groups,
# published to SHAPERSTUDY; owner = smallest containing solid, groups on geometry become
# mesh groups, recompute leaves no stale element
import salome
salome.standalone()
salome.salome_init()

from salome.shaper import model

model.begin()
partSet = model.moduleDocument()
Part_1_doc = model.addPart(partSet).document()
# center + half lengths, same boxes as voxel_four_materials
boxes = [model.addBox(Part_1_doc, 0, 0, 0, 100, 100, 100),     # AIR
         model.addBox(Part_1_doc, -50, 0, 0, 40, 40, 40),      # IRON
         model.addBox(Part_1_doc, -50, 0, 0, 10, 10, 10),      # LEAD
         model.addBox(Part_1_doc, 50, -40, 0, 20, 20, 20)]     # WOOD
aWorld = model.addCompound(Part_1_doc, [model.selection("SOLID", b.result().name()) for b in boxes])
aWorld.result().setName("World")
model.do()
names = ["MAT_AIR", "MAT_IRON", "MAT_LEAD", "MAT_WOOD"]
for i, name in enumerate(names):
    # sub-results keep the feature name (Compound_1_1_<n>), not the renamed result
    g = model.addGroup(Part_1_doc, [model.selection("SOLID", "Compound_1_1_%d" % (i + 1))])
    g.setName(name)
    g.result().setName(name)
model.end()
model.publishToShaperStudy()

import SHAPERSTUDY
import SMESH
from salome.smesh import smeshBuilder
smesh = smeshBuilder.New()

published = SHAPERSTUDY.shape(model.featureStringId(aWorld))
world = published[0] if isinstance(published, (list, tuple)) else published
shaperBuilder = salome.lcc.FindOrLoadComponent("FactoryServer", "SHAPERSTUDY")
groups = {g.GetName(): g for g in shaperBuilder.GetIShapesOperations().GetExistingSubObjects(world, True)}
assert sorted(groups) == names, sorted(groups)

mesh = smesh.Mesh(world, "World")
algo = mesh.Voxel()
algo.Grid(nbCells=20)
assert mesh.Compute(), mesh.GetComputeErrors()

expected = {"MAT_AIR": 7424, "MAT_IRON": 504, "MAT_LEAD": 8, "MAT_WOOD": 64}
def check_groups():
    seen = set()
    for name, g in groups.items():
        mg = mesh.GroupOnGeom(g, name, SMESH.VOLUME)
        ids = mg.GetListOfID()
        assert len(ids) == expected[name], (name, len(ids))
        assert not seen.intersection(ids), name
        seen.update(ids)
        mesh.RemoveGroup(mg)
    assert mesh.NbHexas() == 8000 == mesh.NbVolumes(), mesh.NbHexas()
    assert mesh.NbNodes() == 21 ** 3, mesh.NbNodes()
    assert abs(smesh.GetVolume(mesh) - 200.0 ** 3) < 1e-6 * 200.0 ** 3
    bad = smesh.GetFilter(SMESH.VOLUME, SMESH.FT_BadOrientedVolume)
    assert len(mesh.GetIdsFromFilter(bad)) == 0
check_groups()

# test_recompute: change the hypothesis, compute again, then restore
algo.Grid(nbCells=10)
assert mesh.Compute()
assert mesh.NbHexas() == 1000 and mesh.NbNodes() == 11 ** 3, (mesh.NbHexas(), mesh.NbNodes())
algo.Grid(step=10.)          # same grid as 20 cells, through Step mode
assert mesh.Compute()
check_groups()
print("OK")
