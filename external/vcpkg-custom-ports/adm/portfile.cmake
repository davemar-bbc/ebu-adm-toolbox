vcpkg_from_github(
  OUT_SOURCE_PATH
  SOURCE_PATH
  REPO
  davemar-bbc/libadm
  REF
  4b9b43e341323f1e3a64a5696475ff749a0c9ca0
  SHA512
  d10af72f132d6e98b05ed83bc72963f09f33de468112647cf4b2b05ce8b8e0b7fbec023017b02bcf1859ce5e5dd1ebeb5287a988a9d5d01161dbc6609da603ce
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
