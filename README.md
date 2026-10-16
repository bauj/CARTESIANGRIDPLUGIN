# CARTESIANGRIDPLUGIN

Cartesian grid meshing plug-in for the SALOME Mesh module (SMESH), SALOME 9.16.

The 3D algorithm **Cartesian Grid 3D** meshes a volume shape with an axis-aligned grid of
hexahedra ("staircase" cells: no cut cells, no polyhedra). Each cell is bound to the
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
algo = mesh.CartesianGrid()
hyp = algo.Grid(nbCells=[40, 40, 20])     # or step=2.5, keepOutsideCells=True
hyp.SetStep(2, 5.0)                       # axis 0=X, 1=Y, 2=Z
hyp.SetCoordinates(0, [0, 2, 5, 10])      # X node coordinates
hyp.SetSpacing(2, ["0.5", "2"], [0.5])    # Z: 0.5 up to the middle, then 2
# same two lines in one call:
# algo.Grid(coordinates=[[0, 2, 5, 10], None, None], spacing=[None, None, (["0.5", "2"], [0.5])])
mesh.Compute()
```

Hypothesis **Cartesian Grid Parameters**:

- per axis, a number of cells n (spacing = extent / n), a step s
  (n = max(1, round(extent / s)), spacing adjusted so the grid covers the bounding box
  of the solids exactly), absolute node coordinates (the grid then spans first..last value
  along that axis; solids beyond are cut, with a warning), or spacing functions f(t) with
  internal points as in Body Fitting (internal points in ]0, 1[ split the bounding box into
  ranges, t runs from 0 to 1 within each range, at most about 1000 cells per range);
- cells outside all solids: removed (default) or kept. Kept cells are bound to the main
  shape, which must then be a compound, and a warning gives their number.

In the GUI: *Create Mesh*, 3D algorithm *Cartesian Grid 3D*, hypothesis *Cartesian Grid Parameters*.

## Cartesian MED export

Writes the grid of a Cartesian Grid mesh to a MED file as a **Cartesian mesh** (three coordinate arrays,
one zone per cell): far smaller than an unstructured MED export.

- GUI: *Mesh > SMESH plugins > Cartesian Grid Wizard* (see below).
- Python:

```python
algo.ExportCartesianMED("grid.med", source="geometry", zonePrefix="MAT_")
# or on any mesh with the Cartesian Grid 3D algorithm (smeshBuilder or CORBA mesh):
from salome.CartesianGridPlugin import CartesianGridPluginBuilder
summary = CartesianGridPluginBuilder.ExportCartesianMED(mesh, "grid.med", source="mesh")
# one zone per solid, no group needed:
summary = CartesianGridPluginBuilder.ExportCartesianMED(mesh, "grid.med", source="solids")
```

- Sources:
  - `geometry` (default): no mesh element is created. Zones are the geometry groups (GEOM or
    SHAPERSTUDY, e.g. the SHAPER `MAT_*` material groups) of the main shape whose name starts
    with the prefix; each cell takes the zone of the solid containing its center (same rule
    as Compute). A solid in two zone groups is an error. Solids in no zone group are listed in
    the summary; their cells get no zone.
  - `mesh`: zones are the volume groups of the computed mesh whose name starts with the prefix.
    A cell in two zone groups is an error, so is a mesh not computed.
  - `solids`: one zone per solid of the main shape, no group needed (the prefix is not used), same
    classification as `geometry`. Zone name: the solid's name in the study (GEOM solid published under
    the main shape), else its SHAPER sub-result name (e.g. a GDML volume; read from the SHAPER data
    model, since SHAPERSTUDY does not publish sub-solids), else the main shape name for a single solid,
    else `SOLID_<id>` (sub-shape id); a name used by several solids gets `_<id>` on each.
    `CartesianGridPluginBuilder.SolidNames(mesh)` lists them. The summary starts with a `Warning:`
    line per name cut to 60 characters and when SHAPER names cannot be read, and a `Note:` line for
    unnamed solids and for each name shared by several solids.
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
- The plugin does not limit the number of cells, the memory does: about 16 bytes per cell for the Cartesian MED export alone, about 760 bytes per hexahedron for an SMESH mesh. A step giving more than 2^31 cells along an axis is refused.
- A cell center the geometry kernel cannot locate in a solid is taken as outside it (warning).

## Layout

## Cartesian Grid Wizard

*Mesh > SMESH plugins > Cartesian Grid Wizard*: geometry to Cartesian MED file in one dialog, without
the Create Mesh steps.

- **Geometry**: the dialog is not modal; while its selection button (SMESH arrow icon) is pressed, the
  field follows the GEOM or SHAPER geometry selected in the object browser.
- **Mesh**: the Cartesian Grid meshes of that geometry, plus *New mesh* (with a name). The chosen mesh
  is updated; a new one is created with the Cartesian Grid 3D algorithm and its hypothesis and stays in
  the study (ordinary mesh, normal Python dump).
- **Grid**: the per-axis tabs of the *Cartesian Grid Parameters* dialog (same C++ widget), filled from
  the chosen mesh.
- **Export**: MED file and zones: *Geometry groups named* a prefix (`MAT_`), or *Solids* (one zone per
  solid, named as above). With groups and no matching group, the file holds the grid only (every cell in
  family 0), with a warning.
- **Also compute the SMESH mesh** (optional): computes the hexahedra and creates one mesh group per
  geometry group of solids (names already used are skipped). A SMESH mesh is unstructured (every node
  and hexahedron stored), much bigger than the Cartesian MED file; it is only for viewing.
- Run shows a progress bar (export, then compute) and Cancel, then a summary; the dialog stays open. The MED file is written first: a failed or canceled compute leaves it, with a warning in the summary.

The grid panel is the C++ `CartesianGridPluginGUI_GridPanel`, created through C entry points of
`libCartesianGridPluginGUI` (ctypes) and wrapped as a PyQt widget with `sip.wrapinstance`.

| Path | Content |
|---|---|
| `src/CartesianGridPlugin/CartesianGridPlugin_Grid.hxx`, `CartesianGridPlugin_Classifier.*` | pure OCCT core: grid and multithreaded cell owner classifier (`libCartesianGridCore`) |
| `src/CartesianGridPlugin/CartesianGridPlugin_CartesianGrid_3D*`, `CartesianGridPlugin_Hypothesis*`, `CartesianGridPlugin_i.cxx` | SMESH algorithm, hypothesis, CORBA servants, factory (`libCartesianGridEngine`) |
| `src/CartesianGridPlugin/CartesianGridPlugin_CartesianMED.*` | MED Cartesian file writer (MEDLoader) |
| `src/CartesianGridPlugin/CartesianGridPluginBuilder.py` | Python API (`mesh.CartesianGrid()`, `ExportCartesianMED`) |
| `src/CartesianGridPlugin/CartesianGridPluginWizard.py`, `CartesianGridPluginPanel.py`, `src/GUI/smesh_plugins.py` | *Cartesian Grid Wizard*, its embedding of the C++ grid panel, SMESH plugins menu entry |
| `src/GUI` | hypothesis dialog and grid panel with its C entry points (`libCartesianGridPluginGUI`) |
| `idl/CartesianGridPlugin_Algorithm.idl` | CORBA interfaces |
| `resources/CartesianGridPlugin.xml` | meshers declaration for SMESH |
| `tests` | C++ core tests and Python smesh tests, `data/four_materials.gdml` |
| `misc` | GUI check list, memory measure, GDML test file generator |
| `doc/index.html` | user page |

## Build

With SAT, the product is `CARTESIANGRIDPLUGIN` (`products/CARTESIANGRIDPLUGIN.pyconf`). Its environment
adds `CartesianGridPlugin` to `SMESH_MeshersList` and the resource directory to `SalomeAppConfig`.

By hand, from the application directory:

```bash
source env_build.sh
cmake -S SOURCES/CARTESIANGRIDPLUGIN -B BUILD/CARTESIANGRIDPLUGIN -DCMAKE_BUILD_TYPE=Release \
      -DCMAKE_INSTALL_PREFIX=$PWD/INSTALL/CARTESIANGRIDPLUGIN -DSALOME_BUILD_DOC=OFF
make -C BUILD/CARTESIANGRIDPLUGIN -j12 install
```

## Tests

```bash
# C++ core and hypothesis tests (build tree)
source env_build.sh
ctest --test-dir BUILD/CARTESIANGRIDPLUGIN -R "Test(Classifier|Hypothesis|GridText|CartesianMED)"

# Python tests (installed, SALOME environment)
source env_launch.sh
cd INSTALL/CARTESIANGRIDPLUGIN/bin/salome/test && ctest
```

The SHAPER tests (`cartesiangrid_shaper_gdml`, `cartesiangrid_gdml_file`) need a SHAPER with the GDML
*Create material groups* feature; without it they print `SKIP` and pass.

## License

GNU Lesser General Public License, version 2.1 or later: see `COPYING`.
