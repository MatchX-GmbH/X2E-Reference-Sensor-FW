set(src_dirs "${CMAKE_CURRENT_SOURCE_DIR}" "zmod4410" "bme280" "lis2de12")
set(inc_dirs "${CMAKE_CURRENT_SOURCE_DIR}" "zmod4410" "bme280" "lis2de12" "${PROJECT_DIR}/main/mx_target" "${PROJECT_DIR}/../../common")
set(defines)

#
idf_component_register(SRC_DIRS "${src_dirs}"
                    INCLUDE_DIRS "${inc_dirs}")


# ZOD4410 prebuilt lib
target_link_libraries(${COMPONENT_LIB} "${CMAKE_CURRENT_SOURCE_DIR}/zmod4410/lib_iaq_2nd_gen_ulp.a")
target_link_libraries(${COMPONENT_LIB} "${CMAKE_CURRENT_SOURCE_DIR}/zmod4410/lib_zmod4xxx_cleaning.a")