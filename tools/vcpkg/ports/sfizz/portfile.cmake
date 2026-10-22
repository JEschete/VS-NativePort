vcpkg_check_linkage(ONLY_DYNAMIC_LIBRARY)
vcpkg_download_distfile(ARCHIVE
    URLS "https://github.com/sfztools/sfizz/releases/download/1.2.3/sfizz-1.2.3.tar.gz"
    FILENAME "sfizz-1.2.3.tar.gz"
    SHA512 753df985b0863baf3335cc386075a1f8053800a521725ad57102394883274aa617f1a554da8471444789c985891c604946f2e0627e3a5e2349586532ea0af3cc
)
vcpkg_extract_source_archive(SOURCE_PATH ARCHIVE "${ARCHIVE}")
vcpkg_replace_string(
    "${SOURCE_PATH}/external/st_audiofile/thirdparty/wavpack/CMakeLists.txt"
    "add_library(wavpack" "add_library(wavpack STATIC")
vcpkg_cmake_configure(
    SOURCE_PATH "${SOURCE_PATH}"
    OPTIONS
        -DSFIZZ_SHARED=ON
        -DSFIZZ_JACK=OFF
        -DSFIZZ_RENDER=OFF
        -DSFIZZ_TESTS=OFF
        -DSFIZZ_BENCHMARKS=OFF
        -DSFIZZ_DEMOS=OFF
        -DSFIZZ_DEVTOOLS=OFF
        -DSFIZZ_GIT_SUBMODULE_CHECK=OFF
        -DENABLE_LTO=OFF
)
vcpkg_cmake_build(TARGET sfizz_shared)
# Upstream 1.2.3 has no MSVC install rules. Keep its private dependencies
# inside the DLL and expose only the public C/C++ API and import library.
foreach(CONFIG IN ITEMS rel dbg)
    if(CONFIG STREQUAL "dbg")
        set(DEST "${CURRENT_PACKAGES_DIR}/debug")
    else()
        set(DEST "${CURRENT_PACKAGES_DIR}")
    endif()
    if(EXISTS "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-${CONFIG}/library/bin/sfizz.dll")
        file(INSTALL
            "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-${CONFIG}/library/bin/sfizz.dll"
            DESTINATION "${DEST}/bin")
        file(INSTALL
            "${CURRENT_BUILDTREES_DIR}/${TARGET_TRIPLET}-${CONFIG}/library/lib/sfizz.lib"
            DESTINATION "${DEST}/lib")
    endif()
endforeach()
file(INSTALL "${SOURCE_PATH}/src/sfizz.h" "${SOURCE_PATH}/src/sfizz.hpp"
    "${SOURCE_PATH}/src/sfizz_message.h" DESTINATION "${CURRENT_PACKAGES_DIR}/include")
file(GLOB_RECURSE NOTICES
    "${SOURCE_PATH}/external/LICENSE*" "${SOURCE_PATH}/external/COPYING*")
vcpkg_install_copyright(FILE_LIST "${SOURCE_PATH}/LICENSE" ${NOTICES})
vcpkg_copy_pdbs()
