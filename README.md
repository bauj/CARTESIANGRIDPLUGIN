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
mesh.Compute()
```

Hypothesis **Voxel Grid**:

- per axis, a number of cells n (spacing = extent / n), or a step s
  (n = max(1, round(extent / s)), spacing adjusted so the grid covers the bounding box
  of the solids exactly);
- cells outside all solids: removed (default) or kept. Kept cells are bound to the main
  shape, which must then be a compound, and a warning gives their number.

In the GUI: *Create Mesh*, 3D algorithm *Voxel 3D*, hypothesis *Voxel Grid*.

## Limits

- The algorithm must be assigned to the main shape; no sub-meshes.
- Every solid must be closed. A solid smaller than a cell may own no cell (warning).
- About 760 bytes per hexahedron; 10^6 cells compute in about 6.5 s on 12 threads.
  `mesh.Evaluate()` gives the exact node and hexahedron counts before computing.
- A grid of more than 10^8 cells is refused with a compute error.
- A cell center the geometry kernel cannot locate in a solid is taken as outside it (warning).

## Layout

| Path | Content |
|---|---|
| `src/VoxelPlugin/VoxelPlugin_Grid.hxx`, `VoxelPlugin_Classifier.*` | pure OCCT core: grid and multithreaded cell owner classifier (`libVoxelCore`) |
| `src/VoxelPlugin/VoxelPlugin_Voxel_3D*`, `VoxelPlugin_Hypothesis*`, `VoxelPlugin_i.cxx` | SMESH algorithm, hypothesis, CORBA servants, factory (`libVoxelEngine`) |
| `src/VoxelPlugin/VoxelPluginBuilder.py` | Python API (`mesh.Voxel()`) |
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
