INCLUDE(UseQtExt)

INCLUDE_DIRECTORIES(
  ${QT_INCLUDES}
  ${OpenCASCADE_INCLUDE_DIR}
  ${SALOMEBOOTSTRAP_INCLUDE_DIRS}
  ${KERNEL_INCLUDE_DIRS}
  ${GUI_INCLUDE_DIRS}
  ${GEOM_INCLUDE_DIRS}
  ${SMESH_INCLUDE_DIRS}
  ${Boost_INCLUDE_DIRS}
  ${OMNIORB_INCLUDE_DIR}
  ${CMAKE_CURRENT_BINARY_DIR}
  ${PROJECT_BINARY_DIR}/idl
  ${PROJECT_SOURCE_DIR}/src/CartesianGridPlugin
)

ADD_DEFINITIONS(
  ${QT_DEFINITIONS}
  ${OMNIORB_DEFINITIONS}
  ${OpenCASCADE_DEFINITIONS}
  ${BOOST_DEFINITIONS}
)

SET(_link_LIBRARIES
  ${QT_LIBRARIES}
  ${SALOMEBOOTSTRAP_SALOMELocalTrace}
  ${GEOM_GEOM}
  ${GEOM_GEOMBase}
  ${SMESH_SMESH}
  ${SMESH_PluginUtils}
  ${GUI_suit}
  ${GUI_qtx}
  ${GUI_SalomeApp}
  ${OpenCASCADE_FoundationClasses_LIBRARIES}
  CartesianGridCore
  CartesianGridEngine
  SalomeIDLCARTESIANGRIDPLUGIN
)

SET(_moc_HEADERS
  CartesianGridPluginGUI_AxisTab.h
  CartesianGridPluginGUI_GridPanel.h
  CartesianGridPluginGUI_HypothesisCreator.h
)

QT_WRAP_MOC(_moc_SOURCES ${_moc_HEADERS})

SET(CartesianGridPluginGUI_SOURCES
  CartesianGridPluginGUI.cxx
  CartesianGridPluginGUI_AxisTab.cxx
  CartesianGridPluginGUI_GridPanel.cxx
  CartesianGridPluginGUI_HypothesisCreator.cxx
  CartesianGridPluginGUI_PanelGlue.cxx
  ${_moc_SOURCES}
)

SET(_ts_RESOURCES
  CartesianGridPlugin_images.ts
  CartesianGridPlugin_msg_en.ts
)

ADD_LIBRARY(CartesianGridPluginGUI ${CartesianGridPluginGUI_SOURCES})
TARGET_LINK_LIBRARIES(CartesianGridPluginGUI ${_link_LIBRARIES})
INSTALL(TARGETS CartesianGridPluginGUI EXPORT ${PROJECT_NAME}TargetGroup DESTINATION ${SALOME_INSTALL_LIBS})
INSTALL(FILES ${_moc_HEADERS} DESTINATION ${SALOME_INSTALL_HEADERS})
QT_INSTALL_TS_RESOURCES("${_ts_RESOURCES}" "${SALOME_CARTESIANGRIDPLUGIN_INSTALL_RES_DATA}")

# Mesh > SMESH plugins entries, found by the SALOME plugins manager through CARTESIANGRIDPLUGIN_ROOT_DIR
INSTALL(FILES smesh_plugins.py DESTINATION ${SALOME_CARTESIANGRIDPLUGIN_INSTALL_PLUGINS})
