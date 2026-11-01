# A DLL, so the dr_flac and zstd it bundles stay out of the game's own symbols.
vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)
vcpkg_from_github(
    OUT_SOURCE_PATH SOURCE_PATH
    REPO rtissera/libchdr
    REF 47d794151b2b64f6aba60304e6480f4f5a2bc24b
    SHA512 da7af9bd1d0c7f63f2e0c65a4b0c3c036138b1d887a13e990730b1a8a1f9cec9511d717309106b4027c56a5b5b5443ea132c1d0038ebd911689f296bf02921eb
    HEAD_REF master
)
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DCHDR_WANT_TESTS=OFF
)
vcpkg_cmake_install()
# Upstream's install rules name no RUNTIME destination, so they leave out the DLL.
foreach(CONFIG IN ITEMS rel dbg)
    if(CONFIG STREQUAL "dbg")
        set(DEST "${CURRENT_PACKAGES_DIR}/debug")
    else()
        set(DEST "${CURRENT_PACKAGES_DIR}")
    endif()
    if(EXISTS "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-${CONFIG}/chdr.dll")
        file(INSTALL "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-${CONFIG}/chdr.dll"
            DESTINATION "${DEST}/bin")
    endif()
endforeach()
vcpkg_fixup_pkgconfig()
vcpkg_copy_pdbs()
file(REMOVE_RECURSE "${CURRENT_PACKAGES_DIR}/debug/include")
# miniz and zstd keep their notices in their source headers, not in license files.
vcpkg_install_copyright(FILE_LIST
    "${SOURCE_PATH}/LICENSE.txt"
    "${SOURCE_PATH}/deps/lzma-26.02/LICENSE"
    "${CMAKE_CURRENT_LIST_DIR}/bundled-notices.txt")
