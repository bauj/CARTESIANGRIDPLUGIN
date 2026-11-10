# Copyright (C) 2026  CARTESIANGRIDPLUGIN authors
# LGPL 2.1 or later (see COPYING)

# User's guide (Doxygen), built at install time, as in the other SMESH plugins:
# GUI pages from input/*.doc, Python reference from CartesianGridPluginBuilder.py.

SALOME_CONFIGURE_FILE(doxyfile.in doxyfile)
SALOME_CONFIGURE_FILE(doxyfile_py.in doxyfile_py)
SALOME_CONFIGURE_FILE(static/header.html.in ${CMAKE_CURRENT_BINARY_DIR}/static/header.html)
SALOME_CONFIGURE_FILE(static/header_py.html.in ${CMAKE_CURRENT_BINARY_DIR}/static/header_py.html)

SET(smesh_file "${SMESH_ROOT_DIR}/bin/salome/collect_mesh_methods.py")
SET(kernel_file "$ENV{KERNEL_ROOT_DIR}/bin/salome/prepare_generating_doc.py")

SET(ADD_VAR)
LIST(APPEND ADD_VAR SMESH_MeshersList=CartesianGridPlugin)

SET(_cmd_options ${smesh_file} -d -o tmp/smeshBuilder.py CartesianGridPlugin)
SALOME_GENERATE_ENVIRONMENT_SCRIPT(_cmd env_script "${PYTHON_EXECUTABLE}" "${_cmd_options}"
                                   CONTEXT "CARTESIANGRIDPLUGIN_DOC" ADDITIONAL_VARIABLES ${ADD_VAR})

ADD_CUSTOM_TARGET(usr_docs ${CMAKE_COMMAND} -E make_directory tmp
  COMMAND ${PYTHON_EXECUTABLE} ${kernel_file} -o tmp/CartesianGridPluginBuilder.py
          ${CMAKE_SOURCE_DIR}/src/CartesianGridPlugin/CartesianGridPluginBuilder.py
  COMMAND ${PYTHON_EXECUTABLE} ${kernel_file} -o tmp/smesh_algorithm.py
          ${SMESH_ROOT_DIR}/${SALOME_INSTALL_PYTHON}/salome/smesh/smesh_algorithm.py
  COMMAND ${_cmd}
  COMMAND ${DOXYGEN_EXECUTABLE} doxyfile_py
  COMMAND ${DOXYGEN_EXECUTABLE} doxyfile
  COMMAND ${CMAKE_COMMAND} -E remove_directory tmp
  VERBATIM
  WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
)

INSTALL(CODE "EXECUTE_PROCESS(COMMAND \"${CMAKE_COMMAND}\" --build ${PROJECT_BINARY_DIR} --target usr_docs)")
INSTALL(DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}/CARTESIANGRIDPLUGIN DESTINATION ${SALOME_INSTALL_DOC}/gui)

SET(make_clean_files CARTESIANGRIDPLUGIN cartesiangridpluginpy_doc.tag)
SET_DIRECTORY_PROPERTIES(PROPERTIES ADDITIONAL_MAKE_CLEAN_FILES "${make_clean_files}")
