# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later
# Entries of CARTESIANGRIDPLUGIN in Mesh > SMESH plugins (found through CARTESIANGRIDPLUGIN_ROOT_DIR)

import salome_pluginsmanager

def cartesianGridMEDExport(context):
    from salome.CartesianGridPlugin import CartesianGridPluginExportDialog
    CartesianGridPluginExportDialog.run(context)

salome_pluginsmanager.AddFunction("Cartesian Grid MED Export...",
                                  "Write the Cartesian grid of the selected Cartesian Grid mesh to a MED "
                                  "file, one cell group per material",
                                  cartesianGridMEDExport)
