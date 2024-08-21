set(src_dirs "${CMAKE_CURRENT_SOURCE_DIR}")
set(inc_dirs "${CMAKE_CURRENT_SOURCE_DIR}" "${CMAKE_CURRENT_SOURCE_DIR}/LCD-fonts"
   "${PROJECT_DIR}/../../common" "${PROJECT_DIR}/components/i2c_port_1")
set(defines)

#
idf_component_register(SRC_DIRS "${src_dirs}"
                    INCLUDE_DIRS "${inc_dirs}")
