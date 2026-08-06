vcpkg_from_github(
  OUT_SOURCE_PATH
  SOURCE_PATH
  REPO
  ebu/libadm
  REF
  88312603e5567167328f450b43de1a78ed73c02b
  SHA512
  1f0b9ac7946ec85ab3bdc32733f0786de63c25834de08c0384dd9fc4183c3cc963fb53aabfac2b957b32e8a9306dccdcfed192e8be24527bf55c340e3467a737
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
