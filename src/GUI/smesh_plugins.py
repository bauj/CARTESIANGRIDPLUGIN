# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later
# Entries of CARTESIANGRIDPLUGIN in Mesh > SMESH plugins (found through CARTESIANGRIDPLUGIN_ROOT_DIR)

import salome_pluginsmanager

def cartesianGridWizard(context):
    from salome.CartesianGridPlugin import CartesianGridPluginWizard
    CartesianGridPluginWizard.run(context)

salome_pluginsmanager.AddFunction("Cartesian Grid Wizard",
                                  "Geometry to Cartesian MED file in one dialog: grid, export, and "
                                  "optionally the SMESH mesh",
                                  cartesianGridWizard)
