set(src_dirs 
    "platform/DAL" "platform/phOsal/src/Freertos" "platform/board"
    "comps/phhalHw/src" "comps/phhalHw/src/Rc663"
    "comps/phNfcLib/src"
    "comps/phKeyStore/src" "comps/phKeyStore/src/Sw"
    "comps/phCryptoRng/src" "comps/phCryptoRng/src/Sw"
    "comps/phCryptoSym/src" "comps/phCryptoSym/src/Sw"
    "comps/phTMIUtils/src"
    "comps/phTools/src"

    "comps/phpalI14443p3a/src" "comps/phpalI14443p3a/src/Sw"
    "comps/phpalI14443p3b/src" "comps/phpalI14443p3b/src/Sw"
    "comps/phpalI14443p4/src" "comps/phpalI14443p4/src/Sw"
    "comps/phpalI14443p4a/src" "comps/phpalI14443p4a/src/Sw"
    "comps/phpalMifare/src" "comps/phpalMifare/src/Sw"
    "comps/phpalFelica/src" "comps/phpalFelica/src/Sw"
    "comps/phpalSli15693/src" "comps/phpalSli15693/src/Sw"
    "comps/phpalI18000p3m3/src" "comps/phpalI18000p3m3/src/Sw"
    "comps/phpalEpcUid/src" "comps/phpalEpcUid/src/Sw"
    "comps/phpalI18092mPI/src" "comps/phpalI18092mPI/src/Sw"

    "comps/phalFelica/src" "comps/phalFelica/src/Sw"
    "comps/phalMfc/src" "comps/phalMfc/src/Sw"
    "comps/phalMful/src" "comps/phalMful/src/Sw"
    "comps/phalMfdf/src" "comps/phalMfdf/src/Sw"
    "comps/phalVca/src" "comps/phalVca/src/Sw"
    "comps/phalMfpEVx/src" "comps/phalMfpEVx/src/Sw"
    "comps/phalMfNtag42XDna/src" "comps/phalMfNtag42XDna/src/Sw"
    "comps/phalT1T/src" "comps/phalT1T/src/Sw"
    "comps/phalICode/src" "comps/phalICode/src/Sw"
    "comps/phalTop/src" "comps/phalTop/src/Sw"
    "comps/phalI18000p3m3/src" "comps/phalI18000p3m3/src/Sw"
    "comps/phalMfdfEVx/src" "comps/phalMfdfEVx/src/Sw"

    "comps/phacDiscLoop/src" "comps/phacDiscLoop/src/Sw"
    )

set(inc_dirs "${src_dirs}" "intfs" "types" "platform/phOsal/inc" "${PROJECT_DIR}/components/i2c_port_1")
set(defines "PH_OSAL_FREERTOS" "NXPBUILD__PHHAL_HW_RC663")


#
idf_component_register(SRC_DIRS "${src_dirs}"
                    INCLUDE_DIRS "${inc_dirs}")


target_compile_definitions(${COMPONENT_LIB} PUBLIC ${defines})
