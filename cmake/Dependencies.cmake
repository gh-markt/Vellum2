# Use explicit installations when supplied. Otherwise link the prepared prefix.
set(_dependency_lock "${PROJECT_SOURCE_DIR}/docs/dependencies.lock.json")
set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${_dependency_lock}")
file(READ "${_dependency_lock}" _dependency_config)
string(JSON _mupdf_archive GET "${_dependency_config}" mupdf_artifacts 0)
string(JSON _mupdf_third_archive GET "${_dependency_config}" mupdf_artifacts 1)
string(JSON _mupdf_fitz_header GET "${_dependency_config}" mupdf_artifacts 2)
string(JSON _mupdf_pdf_header GET "${_dependency_config}" mupdf_artifacts 3)
string(JSON _v8_archive GET "${_dependency_config}" v8_artifacts 0)
string(JSON _v8_header GET "${_dependency_config}" v8_artifacts 1)
set(_mupdf_root "${VELLUM_MUPDF_ROOT}")
set(_v8_root "${VELLUM_V8_ROOT}")

if(VELLUM_MUPDF_ROOT)
    if(NOT EXISTS "${_mupdf_root}/${_mupdf_fitz_header}" OR
       NOT EXISTS "${_mupdf_root}/${_mupdf_pdf_header}")
        message(FATAL_ERROR "VELLUM_MUPDF_ROOT has no MuPDF headers")
    endif()
else()
    set(_mupdf_root "${VELLUM_DEPS_PREFIX}")
    file(MAKE_DIRECTORY "${_mupdf_root}/include")
endif()
set(_mupdf_library "${_mupdf_root}/${_mupdf_archive}")
set(_mupdf_third_library "${_mupdf_root}/${_mupdf_third_archive}")

if(VELLUM_V8_ROOT)
    if(NOT EXISTS "${_v8_root}/${_v8_header}")
        message(FATAL_ERROR "VELLUM_V8_ROOT has no V8 headers")
    endif()
else()
    set(_v8_root "${VELLUM_DEPS_PREFIX}")
    file(MAKE_DIRECTORY "${_v8_root}/include/v8")
endif()
set(_v8_library "${_v8_root}/${_v8_archive}")

add_library(vellum_mupdf STATIC IMPORTED GLOBAL)
set_target_properties(vellum_mupdf PROPERTIES
        IMPORTED_LOCATION "${_mupdf_library}"
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_mupdf_root}/include"
        INTERFACE_INCLUDE_DIRECTORIES "${_mupdf_root}/include"
        INTERFACE_LINK_LIBRARIES
        "${_mupdf_third_library};Threads::Threads;${CMAKE_DL_LIBS};m"
)

find_package(PkgConfig REQUIRED)
pkg_check_modules(VELLUM_SYSTEM_ICU REQUIRED IMPORTED_TARGET icu-uc>=73 icu-i18n>=73)
add_library(vellum_v8 STATIC IMPORTED GLOBAL)
set_target_properties(vellum_v8 PROPERTIES
        IMPORTED_LOCATION "${_v8_library}"
        INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_v8_root}/include/v8"
        INTERFACE_INCLUDE_DIRECTORIES "${_v8_root}/include/v8"
        INTERFACE_LINK_LIBRARIES "PkgConfig::VELLUM_SYSTEM_ICU;Threads::Threads;${CMAKE_DL_LIBS};m;atomic"
)

# Explicit external installations are inputs, not generated build outputs.
set(_dependency_byproducts)
if(NOT VELLUM_MUPDF_ROOT)
    list(APPEND _dependency_byproducts
            "${_mupdf_library}"
            "${_mupdf_third_library}"
            "${_mupdf_root}/${_mupdf_fitz_header}"
            "${_mupdf_root}/${_mupdf_pdf_header}"
    )
endif()
if(NOT VELLUM_V8_ROOT)
    list(APPEND _dependency_byproducts "${_v8_library}" "${_v8_root}/${_v8_header}")
    if("${VELLUM_DEPS_WORK}" MATCHES ";")
        message(FATAL_ERROR "VELLUM_DEPS_WORK cannot contain semicolons")
    endif()
    # V8's GN output is outside the CMake build tree. A user-requested clean
    # must remove this managed configuration too, so the next build recompiles it.
    set_property(DIRECTORY APPEND PROPERTY ADDITIONAL_CLEAN_FILES
            "${VELLUM_DEPS_WORK}/v8-tree/v8/out/vellum")
endif()

add_custom_target(vellum_dependencies
        COMMAND ${Python3_EXECUTABLE}
        "${PROJECT_SOURCE_DIR}/scripts/prepare_dependencies.py"
        --prefix "${VELLUM_DEPS_PREFIX}"
        --work "${VELLUM_DEPS_WORK}"
        --jobs "${VELLUM_DEPENDENCY_JOBS}"
        "--mupdf-root=${VELLUM_MUPDF_ROOT}"
        "--v8-root=${VELLUM_V8_ROOT}"
        BYPRODUCTS ${_dependency_byproducts}
        USES_TERMINAL
        VERBATIM
)

add_dependencies(vellum_mupdf vellum_dependencies)
add_dependencies(vellum_v8 vellum_dependencies)
