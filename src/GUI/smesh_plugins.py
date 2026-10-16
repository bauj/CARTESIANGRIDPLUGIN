# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later
# Entries of CARTESIANGRIDPLUGIN in Mesh > SMESH plugins (found through CARTESIANGRIDPLUGIN_ROOT_DIR)

import salome_pluginsmanager
from qtsalome import QCoreApplication

def cartesianGridWizard(context):
    from salome.CartesianGridPlugin import CartesianGridPluginWizard
    CartesianGridPluginWizard.run(context)

def tr(key):
    return QCoreApplication.translate("CartesianGridPluginWizard", key)

salome_pluginsmanager.AddFunction(tr("CARTESIANGRID_WIZ_TITLE"), tr("CARTESIANGRID_WIZ_MENU_TIP"),
                                  cartesianGridWizard)
