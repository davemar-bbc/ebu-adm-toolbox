vcpkg_from_github(
  OUT_SOURCE_PATH
  SOURCE_PATH
  REPO
  davemar-bbc/libadm
  REF
  a1cfc71d6522fcfe9f8ff916181253d7d4f400ec
  SHA512
  041e204de9714cba78c28ee9e65a39054390f792d19fe587ec9ecaf3bc638e5c3db66f58ecac2baa5099df0f71015ad75557124c8419d387ccb67c8fe1f946f6
  HEAD_REF
  draft)

vcpkg_cmake_configure(SOURCE_PATH ${SOURCE_PATH} OPTIONS -DADM_UNIT_TESTS=OFF
                      -DADM_EXAMPLES=OFF)
vcpkg_cmake_install()

if(WIN32 AND NOT CYGWIN)
    set(config_dir CMake)
else()
    set(config_dir share/cmake/adm)
endif()

vcpkg_cmake_config_fixup(PACKAGE_NAME adm
        CONFIG_PATH ${config_dir})
file(
  INSTALL "${SOURCE_PATH}/LICENSE"
  DESTINATION "${CURRENT_PACKAGES_DIR}/share/adm"
  RENAME copyright)

file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
