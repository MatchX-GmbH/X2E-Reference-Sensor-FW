set(src_dirs "src" "src/radio" "src/platform" "src/st25r3911")
set(inc_dirs "src" "src/radio" "src/platform")

# Add selected target
if(CONFIG_MATCHX_TARGET_NEXUS)
  list(APPEND src_dirs "mx_target/nexus")
  list(APPEND inc_dirs "mx_target")
endif()

if(CONFIG_MATCHX_TARGET_X2E_REF)
  list(APPEND src_dirs "mx_target/x2e_ref")
  list(APPEND inc_dirs "mx_target")
endif()

#
idf_component_register(SRC_DIRS "${src_dirs}"
                    INCLUDE_DIRS "${inc_dirs}")
