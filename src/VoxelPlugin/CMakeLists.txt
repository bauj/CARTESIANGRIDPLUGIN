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
SET(VoxelCore_HEADERS
  VoxelPlugin_Defs.hxx
  VoxelPlugin_Grid.hxx
  VoxelPlugin_GridText.hxx
  VoxelPlugin_Classifier.hxx
)

SET(VoxelCore_SOURCES
  VoxelPlugin_Classifier.cxx
)

ADD_LIBRARY(VoxelCore ${VoxelCore_SOURCES})
TARGET_INCLUDE_DIRECTORIES(VoxelCore PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>)
TARGET_LINK_LIBRARIES(VoxelCore
  ${OpenCASCADE_FoundationClasses_LIBRARIES}
  ${OpenCASCADE_ModelingData_LIBRARIES}
  ${OpenCASCADE_ModelingAlgorithms_LIBRARIES}
  Threads::Threads
)
INSTALL(TARGETS VoxelCore EXPORT ${PROJECT_NAME}TargetGroup DESTINATION ${SALOME_INSTALL_LIBS})
INSTALL(FILES ${VoxelCore_HEADERS} DESTINATION ${SALOME_INSTALL_HEADERS})

# SMESH engine: hypothesis, algorithm, servants, factory
SET(VoxelEngine_HEADERS
  VoxelPlugin_Hypothesis.hxx
  VoxelPlugin_Hypothesis_i.hxx
  VoxelPlugin_Voxel_3D.hxx
  VoxelPlugin_Voxel_3D_i.hxx
)

SET(VoxelEngine_SOURCES
  VoxelPlugin_Hypothesis.cxx
  VoxelPlugin_Hypothesis_i.cxx
  VoxelPlugin_i.cxx
  VoxelPlugin_Voxel_3D.cxx
  VoxelPlugin_Voxel_3D_i.cxx
)

ADD_LIBRARY(VoxelEngine ${VoxelEngine_SOURCES})
TARGET_LINK_LIBRARIES(VoxelEngine
  VoxelCore
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
  SalomeIDLVOXELPLUGIN
)
INSTALL(TARGETS VoxelEngine EXPORT ${PROJECT_NAME}TargetGroup DESTINATION ${SALOME_INSTALL_LIBS})
INSTALL(FILES ${VoxelEngine_HEADERS} DESTINATION ${SALOME_INSTALL_HEADERS})

SET(_bin_SCRIPTS
  __init__.py
  VoxelPluginBuilder.py
)
SALOME_INSTALL_SCRIPTS("${_bin_SCRIPTS}" ${SALOME_INSTALL_PYTHON}/salome/VoxelPlugin)
