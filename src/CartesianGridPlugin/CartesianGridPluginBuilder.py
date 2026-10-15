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

def _geometryGroups(shape):
    """(study name of shape, [(group name, sub-shape ids)]) of the groups published under shape
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
            groups.append((child.GetName(), [i for i in obj.GetSubShapeIndices() if i > 0]))
    return so.GetName(), groups

def ExportCartesianMED(mesh, fileName, source="geometry", zonePrefix="MAT_"):
    """Write the Cartesian grid of mesh (smeshBuilder.Mesh or SMESH.SMESH_Mesh) to the MED file
    fileName as a Cartesian mesh, one cell group per zone; return the summary text.
    source "geometry": zones are the geometry groups of the main shape whose name starts with
    zonePrefix, each cell takes the zone of the solid containing its center, no mesh element is
    needed. source "mesh": zones are the volume groups of the computed mesh whose name starts
    with zonePrefix. A solid (geometry) or a cell (mesh) in two zone groups is an error."""
    if source not in ("geometry", "mesh"):
        raise ValueError('source: "geometry" or "mesh", got %r' % (source,))
    smeshMesh = mesh.GetMesh() if hasattr(mesh, "GetMesh") else mesh
    algo = CartesianGridAlgorithm(smeshMesh)
    zones = []
    if source == "geometry":
        shapeName, groups = _geometryGroups(smeshMesh.GetShapeToMesh())
        for name, ids in groups:
            if name.startswith(zonePrefix):
                zones.append(CartesianGridPlugin.ExportZone(name, ids))
        if not zones:
            raise ValueError("No geometry group named %s* under %s; its groups: %s"
                             % (zonePrefix, shapeName, ", ".join(n for n, _ in groups) or "none"))
    else:
        for g in smeshMesh.GetGroups():
            if g.GetType() == SMESH.VOLUME and g.GetName().startswith(zonePrefix):
                zones.append(CartesianGridPlugin.ExportZone(g.GetName(), []))
        if not zones:
            raise ValueError("No mesh volume group named %s*" % zonePrefix)
    return algo.ExportCartesianMED(smeshMesh, fileName, source == "mesh", zones)

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
