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

    def Grid(self, nbCells=None, step=None, keepOutsideCells=None):
        """Return the VoxelGrid hypothesis (created on first call).
        nbCells or step: one number for all axes or a list of 3."""
        if not self.params:
            self.params = self.Hypothesis("VoxelGrid", [], "libVoxelEngine.so", UseExisting=0)
        if nbCells is not None:
            values = nbCells if isinstance(nbCells, (list, tuple)) else [nbCells] * 3
            for axis, n in enumerate(values):
                self.params.SetNbCells(axis, n)
        if step is not None:
            values = step if isinstance(step, (list, tuple)) else [step] * 3
            for axis, s in enumerate(values):
                self.params.SetStep(axis, s)
        if keepOutsideCells is not None:
            self.params.SetKeepOutsideCells(keepOutsideCells)
        return self.params
