# Copyright (C) 2026  VOXELPLUGIN authors
# LGPL 2.1 or later
# Entries of VOXELPLUGIN in Mesh > SMESH plugins (found through VOXELPLUGIN_ROOT_DIR)

import salome_pluginsmanager

def cartesianGridMEDExport(context):
    from salome.VoxelPlugin import VoxelPluginExportDialog
    VoxelPluginExportDialog.run(context)

salome_pluginsmanager.AddFunction("Cartesian Grid MED Export...",
                                  "Write the Cartesian grid of the selected Voxel mesh to a MED "
                                  "file, one cell group per material",
                                  cartesianGridMEDExport)
