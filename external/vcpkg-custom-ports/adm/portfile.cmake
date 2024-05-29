vcpkg_from_github(
  OUT_SOURCE_PATH
  SOURCE_PATH
  REPO
  ebu/libadm
  REF
  258059d04cdfb9058601c995fd35ad5a67267389
  SHA512
  d2c132aee14c1341a3c6321f303dcd95a4f009978546f93eb82b4d09d417ba820d327ee8ba0e901c37c9ba4598efff48a4ca8ac2e756c33f9891ff25da1848ea
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
