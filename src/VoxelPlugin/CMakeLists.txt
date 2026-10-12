INCLUDE_DIRECTORIES(
  ${OpenCASCADE_INCLUDE_DIR}
)

ADD_DEFINITIONS(
  ${OpenCASCADE_DEFINITIONS}
)

# Pure OCCT core: grid and cell owner classifier
SET(VoxelCore_HEADERS
  VoxelPlugin_Defs.hxx
  VoxelPlugin_Grid.hxx
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
