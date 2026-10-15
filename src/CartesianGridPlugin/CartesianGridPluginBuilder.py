# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later

##
# @package CartesianGridPluginBuilder
# Python API for the Cartesian Grid meshing plug-in: mesh.CartesianGrid().Grid()

from salome.smesh.smesh_algorithm import Mesh_Algorithm

noCartesianGridPlugin = 0
try:
    import CartesianGridPlugin
    import SMESH
except ImportError:
    noCartesianGridPlugin = 1

CARTESIAN_GRID_3D = "CartesianGrid_3D"

X, Y, Z = 0, 1, 2

def CartesianGridAlgorithm(mesh):
    """The CartesianGridPlugin_CartesianGrid_3D algorithm object assigned to the main shape of mesh
    (smeshBuilder.Mesh or SMESH.SMESH_Mesh); ValueError if there is none"""
    smeshMesh = mesh.GetMesh() if hasattr(mesh, "GetMesh") else mesh
    for h in smeshMesh.GetHypothesisList(smeshMesh.GetShapeToMesh()):
        if h.GetName() == CARTESIAN_GRID_3D:
            return h._narrow(CartesianGridPlugin.CartesianGridPlugin_CartesianGrid_3D)
    raise ValueError("The mesh has no %s algorithm" % CARTESIAN_GRID_3D)

GEOM_GROUP_TYPE = 37   # GetType() of a group, GEOM and SHAPERSTUDY

def _geometryGroupObjects(shape):
    """(study name of shape, [(group name, group object)]) of the groups published under shape
    in the study, GEOM or SHAPERSTUDY alike. Read from the study tree: no engine lookup, which can return another
    module's engine in a GUI session"""
    import salome
    import GEOM
    so = salome.ObjectToSObject(shape)
    if so is None:
        raise ValueError("The shape of the mesh is not published in the study")
    groups = []
    it = salome.myStudy.NewChildIterator(so)
    while it.More():
        child = it.Value()
        it.Next()
        obj = child.GetObject()
        obj = obj._narrow(GEOM.GEOM_Object) if obj is not None else None
        if obj is not None and obj.GetType() == GEOM_GROUP_TYPE:
            groups.append((child.GetName(), obj))
    return so.GetName(), groups

def _geometryGroups(shape):
    """(study name of shape, [(group name, sub-shape ids)]) of the groups published under shape"""
    shapeName, groups = _geometryGroupObjects(shape)
    return shapeName, [(name, [i for i in g.GetSubShapeIndices() if i > 0]) for name, g in groups]

def _entry(obj):
    import salome
    so = salome.ObjectToSObject(obj) if obj is not None else None
    return so.GetID() if so is not None else None

def CartesianGridMeshesOn(shape):
    """[(study name, SMESH.SMESH_Mesh)] of the study meshes built on shape (same study object)
    whose main shape carries the CartesianGrid_3D algorithm, in study order"""
    import salome
    shapeEntry = _entry(shape)
    comp = salome.myStudy.FindComponent("SMESH")
    if shapeEntry is None or comp is None:
        return []
    meshes = []
    it = salome.myStudy.NewChildIterator(comp)
    while it.More():
        so = it.Value()
        it.Next()
        obj = so.GetObject()
        mesh = obj._narrow(SMESH.SMESH_Mesh) if obj is not None else None
        if mesh is None or not mesh.HasShapeToMesh() or _entry(mesh.GetShapeToMesh()) != shapeEntry:
            continue
        try:
            CartesianGridAlgorithm(mesh)
        except ValueError:
            continue
        meshes.append((so.GetName(), mesh))
    return meshes

def CartesianGridParametersOf(mesh):
    """the CartesianGridParameters hypothesis of the main shape of mesh, or None"""
    smeshMesh = mesh.GetMesh() if hasattr(mesh, "GetMesh") else mesh
    for h in smeshMesh.GetHypothesisList(smeshMesh.GetShapeToMesh()):
        if h.GetName() == "CartesianGridParameters":
            return h._narrow(CartesianGridPlugin.CartesianGridPlugin_Hypothesis)
    return None

def CreateGroupsOnGeometry(mesh):
    """One volume group on geometry per group of the main shape of mesh (smeshBuilder.Mesh), named
    after it; names already used by a mesh group are skipped. Groups left empty (not a group of
    solids, or no cell center in its solids) are removed. Returns (created names,
    skipped [(name, reason)])"""
    _, groups = _geometryGroupObjects(mesh.GetShape())
    used = {g.GetName().strip() for g in mesh.GetGroups()}
    created, skipped = [], []
    for name, g in groups:
        if name in used:
            skipped.append((name, "a mesh group of that name exists"))
            continue
        group = mesh.GroupOnGeom(g, name, SMESH.VOLUME)
        if group.Size() == 0:
            mesh.RemoveGroup(group)
            skipped.append((name, "no cell: not a group of solids, or no cell center in its solids"))
        else:
            created.append(name)
            used.add(name)
    return created, skipped

GEOM_SUBSHAPE_TYPE = 28   # GetType() of a sub-shape published under its main shape
MAX_ZONE_NAME = 60        # MED family "FAM_<name>" must fit in 64 characters

def _shaperSolidNames(shape, nbSolids):
    """[name or None] per solid of a SHAPERSTUDY shape, in TopExp::MapShapes order, read from the
    SHAPER data model (sub-results, e.g. GDML volume names: SHAPERSTUDY does not publish them);
    None when the shape is not a SHAPER one or the SHAPER model is not loaded in this process"""
    try:
        import SHAPERSTUDY_ORB
        obj = shape._narrow(SHAPERSTUDY_ORB.SHAPER_Object)
        if obj is None:
            return None
        import ModelAPI
        from GeomAPI import GeomAPI_Shape, GeomAPI_ShapeExplorer
        # entry: "<part feature id>:<feature id>[:<result index>]", as model.featureStringId
        ids = [int(i) for i in obj.GetEntry().split(":")]
        root = ModelAPI.ModelAPI_Session.get().moduleDocument()
        doc = None
        for f in root.allFeatures():
            if f.data().featureId() == ids[0] and f.getKind() == "Part":
                doc = ModelAPI.modelAPI_ResultPart(f.firstResult()).partDoc()
        if doc is None:
            return None
        feature = next((f for f in doc.allFeatures() if f.data().featureId() == ids[1]), None)
        if feature is None:
            return None
        results = feature.results()
        body = ModelAPI.modelAPI_ResultBody(results[ids[2]] if len(ids) > 2 else feature.firstResult())
        if body is None or body.shape() is None:
            return None
    except Exception:
        return None
    # solids of the result shape in explorer order without repeats = TopExp::MapShapes order
    solids = []
    explorer = GeomAPI_ShapeExplorer(body.shape(), GeomAPI_Shape.SOLID)
    while explorer.more():
        solid = explorer.current()
        explorer.next()
        if not any(solid.isSame(s) for s in solids):
            solids.append(solid)
    if len(solids) != nbSolids:
        return None
    leaves = []
    def collect(result):
        if result.numberOfSubs() == 0:
            leaves.append(result)
        for i in range(result.numberOfSubs()):
            collect(result.subResult(i))
    collect(body)
    names = [None] * nbSolids
    for k, leaf in enumerate(leaves):
        leafShape = leaf.shape()
        if leafShape is None or not leafShape.isSolid():
            continue
        if k < nbSolids and leafShape.isSame(solids[k]):   # usual case: same order
            names[k] = leaf.data().name()
            continue
        for rank, s in enumerate(solids):
            if leafShape.isSame(s):
                names[rank] = leaf.data().name()
                break
    return names

def _listed(names, limit=10):
    return ", ".join(names[:limit]) + (", ..." if len(names) > limit else "")

def SolidNames(mesh):
    """[(zone name, sub-shape id)] of the solids of the main shape of mesh (smeshBuilder.Mesh or
    SMESH.SMESH_Mesh), in the export order. Name: the GEOM solid published under the main shape, the
    SHAPER sub-result (SHAPER data model of this process), the main shape name when it is a single
    solid, else SOLID_<id>; a name used by several solids gets _<id> on each; names cut to fit MED"""
    return _solidZones(mesh)[0]

def _solidZones(mesh):
    """(SolidNames list, report lines: "Warning: ..." for cut names and unreadable SHAPER names,
    "Note: ..." for unnamed solids and shared names)"""
    import salome
    import GEOM
    smeshMesh = mesh.GetMesh() if hasattr(mesh, "GetMesh") else mesh
    shape = smeshMesh.GetShapeToMesh()
    ids = list(CartesianGridAlgorithm(smeshMesh).GetSolidIds(smeshMesh))
    names = [None] * len(ids)
    rank = {i: k for k, i in enumerate(ids)}
    so = salome.ObjectToSObject(shape)
    if so is not None:
        it = salome.myStudy.NewChildIterator(so)
        while it.More():
            child = it.Value()
            it.Next()
            obj = child.GetObject()
            obj = obj._narrow(GEOM.GEOM_Object) if obj is not None else None
            if obj is None or obj.GetType() != GEOM_SUBSHAPE_TYPE or obj.GetShapeType() != GEOM.SOLID:
                continue
            indices = [i for i in obj.GetSubShapeIndices() if i > 0]
            if len(indices) == 1 and indices[0] in rank and child.GetName().strip():
                names[rank[indices[0]]] = child.GetName().strip()
    report = []
    shaperNames = _shaperSolidNames(shape, len(ids))
    if shaperNames:
        names = [n or s for n, s in zip(names, shaperNames)]
    elif _isShaperShape(shape):
        report.append("Warning: the SHAPER solid names cannot be read (SHAPER is not loaded in this "
                      "process, or the result changed since publication); unnamed solids get SOLID_<id>")
    if len(ids) == 1 and not names[0] and so is not None:
        names[0] = so.GetName()
    names = [n.strip() if n and n.strip() else None for n in names]
    count = {}
    for n in names:
        if n:
            count[n] = count.get(n, 0) + 1
    result, used, cut = [], set(), []
    shared = {}
    for n, i in zip(names, ids):
        suffix = "" if n and count[n] == 1 else "_%d" % i
        name = (n or "SOLID")[:MAX_ZONE_NAME - len(suffix)] + suffix
        if name in used:   # a cut name, or a real name equal to a suffixed one
            name = (n or "SOLID")[:MAX_ZONE_NAME - len("_%d" % i)] + "_%d" % i
        used.add(name)
        result.append((name, i))
        if n and len(n) + len(suffix) > MAX_ZONE_NAME:
            cut.append('"%s" -> "%s"' % (n, name))
        if n and count[n] > 1:
            shared.setdefault(n, []).append(name)
    for c in cut:
        report.append("Warning: zone name cut to %d characters (MED limit): %s" % (MAX_ZONE_NAME, c))
    unnamed = [name for (name, i), n in zip(result, names) if not n]
    if unnamed:
        report.append("Note: %d solid(s) without a name in the study or in SHAPER, named SOLID_<id>: %s"
                      % (len(unnamed), _listed(unnamed)))
    for n, zones in shared.items():
        report.append('Note: name "%s" used by %d solids, zones %s' % (n, len(zones), _listed(zones)))
    return result, report

def _isShaperShape(shape):
    try:
        import SHAPERSTUDY_ORB
        return shape._narrow(SHAPERSTUDY_ORB.SHAPER_Object) is not None
    except Exception:
        return False

def ExportCartesianMED(mesh, fileName, source="geometry", zonePrefix="MAT_"):
    """Write the Cartesian grid of mesh (smeshBuilder.Mesh or SMESH.SMESH_Mesh) to the MED file
    fileName as a Cartesian mesh, one cell group per zone; return the summary text.
    source "geometry": zones are the geometry groups of the main shape whose name starts with
    zonePrefix, each cell takes the zone of the solid containing its center, no mesh element is
    needed. source "mesh": zones are the volume groups of the computed mesh whose name starts
    with zonePrefix. A solid (geometry) or a cell (mesh) in two zone groups is an error. Without
    any zone group the file holds the grid only (every cell in family 0), with a warning.
    source "solids": one zone per solid of the main shape, named by SolidNames (zonePrefix is not
    used); no group is needed."""
    if source not in ("geometry", "mesh", "solids"):
        raise ValueError('source: "geometry", "mesh" or "solids", got %r' % (source,))
    smeshMesh = mesh.GetMesh() if hasattr(mesh, "GetMesh") else mesh
    algo = CartesianGridAlgorithm(smeshMesh)
    zones = []
    warning = ""
    if source == "solids":
        solids, report = _solidZones(smeshMesh)
        zones = [CartesianGridPlugin.ExportZone(name, [i]) for name, i in solids]
        warning = "".join(line + "\n" for line in report)
    elif source == "geometry":
        shapeName, groups = _geometryGroups(smeshMesh.GetShapeToMesh())
        for name, ids in groups:
            if name.startswith(zonePrefix):
                zones.append(CartesianGridPlugin.ExportZone(name, ids))
        if not zones:
            warning = ("Warning: no geometry group named %s* under %s (its groups: %s)\n"
                       % (zonePrefix, shapeName, ", ".join(n for n, _ in groups) or "none"))
    else:
        for g in smeshMesh.GetGroups():
            if g.GetType() == SMESH.VOLUME and g.GetName().startswith(zonePrefix):
                zones.append(CartesianGridPlugin.ExportZone(g.GetName(), []))
        if not zones:
            warning = "Warning: no mesh volume group named %s*\n" % zonePrefix
    return warning + algo.ExportCartesianMED(smeshMesh, fileName, source == "mesh", zones)

class CartesianGrid_3D_Algorithm(Mesh_Algorithm):
    """Cartesian staircase grid, one hexahedron per cell owned by a solid.
    Assign it to the main shape: mesh.CartesianGrid()"""

    meshMethod = "CartesianGrid"
    algoType   = CARTESIAN_GRID_3D

    def __init__(self, mesh, geom=0):
        Mesh_Algorithm.__init__(self)
        if noCartesianGridPlugin: print("Warning: CartesianGridPlugin module unavailable")
        self.Create(mesh, geom, self.algoType, "libCartesianGridEngine.so")
        self.params = None

    @staticmethod
    def _perAxis(values):
        """[(axis, value)] of one value for all axes or a list of 3, None entries skipped"""
        if values is None:
            return []
        if not isinstance(values, (list, tuple)):
            values = [values] * 3
        return [(axis, v) for axis, v in enumerate(values) if v is not None]

    @staticmethod
    def _spacingOf(value):
        """(functions, internalPoints) of a str or a (functions, internalPoints) pair"""
        if isinstance(value, str):
            return [value], []
        if (isinstance(value, (list, tuple)) and len(value) == 2 and
                isinstance(value[0], (list, tuple)) and isinstance(value[1], (list, tuple))):
            return list(value[0]), [float(t) for t in value[1]]
        raise TypeError("spacing: a function string or (functions, internalPoints), got %r" % (value,))

    def Grid(self, nbCells=None, step=None, coordinates=None, spacing=None, keepOutsideCells=None):
        """Return the CartesianGridParameters hypothesis (created on first call).
        Applied in this order: nbCells, step, coordinates, spacing.
        nbCells or step: one number for all axes or a list of 3 (None leaves an axis untouched).
        coordinates: list of 3, each None or the list of absolute node coordinates of the axis.
        spacing: one value for all axes or a list of 3 (None leaves an axis untouched); a value
        is a function of t (str) or (functions, internalPoints) as Body Fitting SetGridSpacing:
        internal points in ]0, 1[ split the bounding box into ranges, t runs from 0 to 1 within
        each range."""
        if not self.params:
            self.params = self.Hypothesis("CartesianGridParameters", [], "libCartesianGridEngine.so", UseExisting=0)
        for axis, n in self._perAxis(nbCells):
            self.params.SetNbCells(axis, n)
        for axis, s in self._perAxis(step):
            self.params.SetStep(axis, s)
        if coordinates is not None:
            if (not isinstance(coordinates, (list, tuple)) or len(coordinates) != 3 or
                    not all(c is None or isinstance(c, (list, tuple)) for c in coordinates)):
                raise TypeError("coordinates: list of 3, each None or a list of node coordinates")
            for axis, c in enumerate(coordinates):
                if c is not None:
                    self.params.SetCoordinates(axis, [float(x) for x in c])
        if spacing is not None:
            if isinstance(spacing, str):
                spacing = [spacing] * 3
            elif not isinstance(spacing, (list, tuple)) or len(spacing) != 3:
                raise TypeError("spacing: one value for all axes or a list of 3")
            values = [None if s is None else self._spacingOf(s) for s in spacing]
            for axis, value in enumerate(values):
                if value is not None:
                    self.params.SetSpacing(axis, value[0], value[1])
        if keepOutsideCells is not None:
            self.params.SetKeepOutsideCells(keepOutsideCells)
        return self.params

    def ExportCartesianMED(self, fileName, source="geometry", zonePrefix="MAT_"):
        """See CartesianGridPluginBuilder.ExportCartesianMED"""
        return ExportCartesianMED(self.mesh, fileName, source, zonePrefix)
