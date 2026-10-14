# Copyright (C) 2026  VOXELPLUGIN authors
# LGPL 2.1 or later

##
# @package VoxelPluginBuilder
# Python API for the Voxel meshing plug-in: mesh.Voxel().Grid()

from salome.smesh.smesh_algorithm import Mesh_Algorithm

noVoxelPlugin = 0
try:
    import VoxelPlugin
except ImportError:
    noVoxelPlugin = 1

VOXEL_3D = "Voxel_3D"

X, Y, Z = 0, 1, 2

class Voxel_3D_Algorithm(Mesh_Algorithm):
    """Cartesian staircase voxels, one hexahedron per cell owned by a solid.
    Assign it to the main shape: mesh.Voxel()"""

    meshMethod = "Voxel"
    algoType   = VOXEL_3D

    def __init__(self, mesh, geom=0):
        Mesh_Algorithm.__init__(self)
        if noVoxelPlugin: print("Warning: VoxelPlugin module unavailable")
        self.Create(mesh, geom, self.algoType, "libVoxelEngine.so")
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
        """Return the VoxelGrid hypothesis (created on first call).
        Applied in this order: nbCells, step, coordinates, spacing.
        nbCells or step: one number for all axes or a list of 3 (None leaves an axis untouched).
        coordinates: list of 3, each None or the list of absolute node coordinates of the axis.
        spacing: one value for all axes or a list of 3 (None leaves an axis untouched); a value
        is a function of t (str) or (functions, internalPoints) as Body Fitting SetGridSpacing:
        internal points in ]0, 1[ split the bounding box into ranges, t runs from 0 to 1 within
        each range."""
        if not self.params:
            self.params = self.Hypothesis("VoxelGrid", [], "libVoxelEngine.so", UseExisting=0)
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
