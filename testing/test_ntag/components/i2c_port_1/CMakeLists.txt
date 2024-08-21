set(src_dirs "${CMAKE_CURRENT_SOURCE_DIR}")
set(inc_dirs "${CMAKE_CURRENT_SOURCE_DIR}" "${PROJECT_DIR}/main/mx_target" "${PROJECT_DIR}/../../common")
set(defines)

#
idf_component_register(SRC_DIRS "${src_dirs}"
                    INCLUDE_DIRS "${inc_dirs}")
