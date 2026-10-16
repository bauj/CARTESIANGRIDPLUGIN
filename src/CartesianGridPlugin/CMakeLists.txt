# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later (see COPYING)

INCLUDE_DIRECTORIES(
  ${SALOMEBOOTSTRAP_INCLUDE_DIRS}
  ${KERNEL_INCLUDE_DIRS}
  ${OpenCASCADE_INCLUDE_DIR}
  ${GEOM_INCLUDE_DIRS}
  ${SMESH_INCLUDE_DIRS}
  ${MEDCOUPLING_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${OMNIORB_INCLUDE_DIR}
  ${PROJECT_BINARY_DIR}/idl
)

ADD_DEFINITIONS(
  ${OMNIORB_DEFINITIONS}
  ${OpenCASCADE_DEFINITIONS}
  ${BOOST_DEFINITIONS}
)

# Pure OCCT core: grid and cell owner classifier
SET(CartesianGridCore_HEADERS
  CartesianGridPlugin_Defs.hxx
  CartesianGridPlugin_Grid.hxx
  CartesianGridPlugin_GridEdit.hxx
  CartesianGridPlugin_GridText.hxx
  CartesianGridPlugin_Classifier.hxx
)

SET(CartesianGridCore_SOURCES
  CartesianGridPlugin_Classifier.cxx
)

ADD_LIBRARY(CartesianGridCore ${CartesianGridCore_SOURCES})
TARGET_INCLUDE_DIRECTORIES(CartesianGridCore PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
TARGET_LINK_LIBRARIES(CartesianGridCore
  ${OpenCASCADE_FoundationClasses_LIBRARIES}
  ${OpenCASCADE_ModelingData_LIBRARIES}
  ${OpenCASCADE_ModelingAlgorithms_LIBRARIES}
  Threads::Threads
)
INSTALL(TARGETS CartesianGridCore EXPORT ${PROJECT_NAME}TargetGroup DESTINATION ${SALOME_INSTALL_LIBS})
INSTALL(FILES ${CartesianGridCore_HEADERS} DESTINATION ${SALOME_INSTALL_HEADERS})

# SMESH engine: hypothesis, algorithm, servants, factory
SET(CartesianGridEngine_HEADERS
  CartesianGridPlugin_CartesianMED.hxx
  CartesianGridPlugin_Hypothesis.hxx
  CartesianGridPlugin_Hypothesis_i.hxx
  CartesianGridPlugin_CartesianGrid_3D.hxx
  CartesianGridPlugin_CartesianGrid_3D_i.hxx
)

SET(CartesianGridEngine_SOURCES
  CartesianGridPlugin_CartesianMED.cxx
  CartesianGridPlugin_Hypothesis.cxx
  CartesianGridPlugin_Hypothesis_i.cxx
  CartesianGridPlugin_i.cxx
  CartesianGridPlugin_CartesianGrid_3D.cxx
  CartesianGridPlugin_CartesianGrid_3D_i.cxx
)

ADD_LIBRARY(CartesianGridEngine ${CartesianGridEngine_SOURCES})
TARGET_LINK_LIBRARIES(CartesianGridEngine
  CartesianGridCore
  ${SALOMEBOOTSTRAP_SALOMELocalTrace}
  ${SALOMEBOOTSTRAP_SALOMEBasics}
  ${KERNEL_SalomeGenericObj}
  ${KERNEL_SalomeNS}
  ${KERNEL_OpUtil}
  ${SMESH_SMDS}
  ${SMESH_SMESHDS}
  ${SMESH_SMESHimpl}
  ${SMESH_SMESHEngine}
  ${SMESH_StdMeshers}
  ${MEDCoupling_medloader}
  SalomeIDLCARTESIANGRIDPLUGIN
)
INSTALL(TARGETS CartesianGridEngine EXPORT ${PROJECT_NAME}TargetGroup DESTINATION ${SALOME_INSTALL_LIBS})
INSTALL(FILES ${CartesianGridEngine_HEADERS} DESTINATION ${SALOME_INSTALL_HEADERS})

SET(_bin_SCRIPTS
  __init__.py
  CartesianGridPluginBuilder.py
  CartesianGridPluginPanel.py
  CartesianGridPluginWizard.py
)
SALOME_INSTALL_SCRIPTS("${_bin_SCRIPTS}" ${SALOME_INSTALL_PYTHON}/salome/CartesianGridPlugin)
