# VOXELPLUGIN

Cartesian voxel meshing plug-in for the SALOME Mesh module (SMESH), SALOME 9.16.

The 3D algorithm **Voxel 3D** meshes a volume shape with an axis-aligned grid of
hexahedra ("staircase" voxels: no cut cells, no polyhedra). Each cell is bound to the
**smallest solid containing its center** (a center on a face counts as inside), so
nested or overlapping solids work: the deepest volume wins. This is what a GDML World
Volume built in SHAPER needs, where daughter volumes are not cut out of their mother.

Geometry groups of solids become mesh groups through SMESH's *Create all groups on
geometry*. With SHAPER in GDML mode, *Create material groups* makes one `MAT_<material>`
group per material, so the mesh gets one group per material.

## Usage

```python
from salome.smesh import smeshBuilder
smesh = smeshBuilder.New()

mesh = smesh.Mesh(shape)                  # algorithm on the main shape (all solids)
algo = mesh.Voxel()
hyp = algo.Grid(nbCells=[40, 40, 20])     # or step=2.5, keepOutsideCells=True
hyp.SetStep(2, 5.0)                       # axis 0=X, 1=Y, 2=Z
hyp.SetCoordinates(0, [0, 2, 5, 10])      # X node coordinates
hyp.SetSpacing(2, ["0.5", "2"], [0.5])    # Z: 0.5 up to the middle, then 2
# same two lines in one call:
# algo.Grid(coordinates=[[0, 2, 5, 10], None, None], spacing=[None, None, (["0.5", "2"], [0.5])])
mesh.Compute()
```

Hypothesis **Voxel Grid**:

- per axis, a number of cells n (spacing = extent / n), a step s
  (n = max(1, round(extent / s)), spacing adjusted so the grid covers the bounding box
  of the solids exactly), absolute node coordinates (the grid then spans first..last value
  along that axis; solids beyond are cut, with a warning), or spacing functions f(t) with
  internal points as in Body Fitting (internal points in ]0, 1[ split the bounding box into
  ranges, t runs from 0 to 1 within each range, at most about 1000 cells per range);
- cells outside all solids: removed (default) or kept. Kept cells are bound to the main
  shape, which must then be a compound, and a warning gives their number.

In the GUI: *Create Mesh*, 3D algorithm *Voxel 3D*, hypothesis *Voxel Grid*.

## Cartesian MED export

Writes the grid of a Voxel mesh to a MED file as a **Cartesian mesh** (three coordinate arrays,
one zone per cell): far smaller than an unstructured MED export.

- GUI: select the mesh, *Mesh > SMESH plugins > Cartesian Grid MED Export...*: file, source,
  zone prefix; progress bar and Cancel; a summary gives the cells per zone.
- Python:

```python
algo.ExportCartesianMED("grid.med", source="geometry", zonePrefix="MAT_")
# or on any mesh with the Voxel 3D algorithm (smeshBuilder or CORBA mesh):
from salome.VoxelPlugin import VoxelPluginBuilder
summary = VoxelPluginBuilder.ExportCartesianMED(mesh, "grid.med", source="mesh")
```

- Sources:
  - `geometry` (default): no mesh element is created. Zones are the geometry groups (GEOM or
    SHAPERSTUDY, e.g. the SHAPER `MAT_*` material groups) of the main shape whose name starts
    with the prefix; each cell takes the zone of the solid containing its center (same rule
    as Compute). A solid in two zone groups is an error. Solids in no zone group are listed in
    the summary; their cells get no zone.
  - `mesh`: zones are the volume groups of the computed mesh whose name starts with the prefix.
    A cell in two zone groups is an error, so is a mesh not computed.
- File layout: one `MEDFileCMesh` named after the mesh, coordinate components `X`, `Y`, `Z`;
  cells numbered i fastest, then j, then k; cell family `0` = no zone (outside cells, solids
  in no zone group); zone groups sorted by name get families `-1, -2, ...` named
  `FAM_<group>`, each holding the group `<group>`. Group names are at most 60 characters.
- Reading it in a solver with the MED C library: `MEDmeshInfo` (`MED_STRUCTURED_MESH`,
  `MED_CARTESIAN_GRID`), `MEDmeshGridIndexCoordinateRd` per axis,
  `MEDmeshEntityFamilyNumberRd` on `MED_CELL` / `MED_HEXA8`, then `MEDnFamily` and
  `MEDfamilyInfo` for the group names of each family. Read zones by group name, not by family
  number, and accept families holding several groups (planned for mixed cells).
- SMESH (*File > Import > MED*) and ParaVis open the file; SMESH shows the group names padded
  with spaces.

## Limits

- The algorithm must be assigned to the main shape; no sub-meshes.
- Every solid must be closed. A solid smaller than a cell may own no cell (warning).
- About 760 bytes per hexahedron; 10^6 cells compute in about 1.6 s on 12 threads.
  `mesh.Evaluate()` gives the exact node and hexahedron counts before computing.
- Cells are classified in parallel on all cores (one line intersection per row of cells and per
  solid, exact point classification near the boundary only), then nodes and hexahedra are created on one
  thread. The compute reports its progress and can be canceled (no partial mesh left).
- A grid of more than 10^8 cells is refused with a compute error.
- A cell center the geometry kernel cannot locate in a solid is taken as outside it (warning).

## Layout

| Path | Content |
|---|---|
| `src/VoxelPlugin/VoxelPlugin_Grid.hxx`, `VoxelPlugin_Classifier.*` | pure OCCT core: grid and multithreaded cell owner classifier (`libVoxelCore`) |
| `src/VoxelPlugin/VoxelPlugin_Voxel_3D*`, `VoxelPlugin_Hypothesis*`, `VoxelPlugin_i.cxx` | SMESH algorithm, hypothesis, CORBA servants, factory (`libVoxelEngine`) |
| `src/VoxelPlugin/VoxelPlugin_CartesianMED.*` | MED Cartesian file writer (MEDLoader) |
| `src/VoxelPlugin/VoxelPluginBuilder.py` | Python API (`mesh.Voxel()`, `ExportCartesianMED`) |
| `src/VoxelPlugin/VoxelPluginExportDialog.py`, `src/GUI/smesh_plugins.py` | *Cartesian Grid MED Export* dialog and its SMESH plugins menu entry |
| `src/GUI` | hypothesis dialog (`libVoxelPluginGUI`) |
| `idl/VoxelPlugin_Algorithm.idl` | CORBA interfaces |
| `resources/VoxelPlugin.xml` | meshers declaration for SMESH |
| `tests` | C++ core tests and Python smesh tests, `data/four_materials.gdml` |
| `misc` | GUI check list, memory measure, GDML test file generator |
| `doc/index.html` | user page |

## Build

With SAT, the product is `VOXELPLUGIN` (`products/VOXELPLUGIN.pyconf`). Its environment
adds `VoxelPlugin` to `SMESH_MeshersList` and the resource directory to `SalomeAppConfig`.

By hand, from the application directory:

```bash
source env_build.sh
cmake -S SOURCES/VOXELPLUGIN -B BUILD/VOXELPLUGIN -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$PWD/INSTALL/VOXELPLUGIN -DSALOME_BUILD_DOC=OFF
make -C BUILD/VOXELPLUGIN -j12 install
```

## Tests

```bash
# C++ core and hypothesis tests (build tree)
source env_build.sh
ctest --test-dir BUILD/VOXELPLUGIN -R TestVoxel

# Python tests (installed, SALOME environment)
source env_launch.sh
cd INSTALL/VOXELPLUGIN/bin/salome/test && ctest
```

The SHAPER tests (`voxel_shaper_gdml`, `voxel_gdml_file`) need a SHAPER with the GDML
*Create material groups* feature; without it they print `SKIP` and pass.

## License

LGPL 2.1 or later.
