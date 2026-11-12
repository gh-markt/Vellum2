# Prefer explicitly supplied compatible static installations, otherwise build local copies.
# The bootstrap's V8 ABI uses no pointer compression, no V8 sandbox, embedded ICU/startup data.
set(_mupdf_root "${VELLUM_MUPDF_ROOT}")
set(_v8_root "${VELLUM_V8_ROOT}")
if(NOT _mupdf_root)
    set(_mupdf_root "${VELLUM_DEPS_PREFIX}")
endif()
if(NOT _v8_root)
    set(_v8_root "${VELLUM_DEPS_PREFIX}")
endif()
if(NOT VELLUM_MUPDF_ROOT)
    file(MAKE_DIRECTORY "${_mupdf_root}/include")
elseif(NOT EXISTS "${_mupdf_root}/include/mupdf/fitz.h")
    message(FATAL_ERROR "VELLUM_MUPDF_ROOT has no MuPDF headers")
endif()
if(NOT VELLUM_V8_ROOT)
    file(MAKE_DIRECTORY "${_v8_root}/include/v8")
elseif(NOT EXISTS "${_v8_root}/include/v8/v8.h")
    message(FATAL_ERROR "VELLUM_V8_ROOT has no V8 headers")
endif()
add_library(vellum_mupdf STATIC IMPORTED GLOBAL)
set_target_properties(vellum_mupdf PROPERTIES
    IMPORTED_LOCATION "${_mupdf_root}/lib/libmupdf.a"
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_mupdf_root}/include"
    INTERFACE_INCLUDE_DIRECTORIES "${_mupdf_root}/include"
    INTERFACE_LINK_LIBRARIES "${_mupdf_root}/lib/libmupdf-third.a;Threads::Threads;${CMAKE_DL_LIBS};m")
add_library(vellum_v8 STATIC IMPORTED GLOBAL)
set_target_properties(vellum_v8 PROPERTIES
    IMPORTED_LOCATION "${_v8_root}/lib/libv8_monolith.a"
    INTERFACE_SYSTEM_INCLUDE_DIRECTORIES "${_v8_root}/include/v8"
    INTERFACE_INCLUDE_DIRECTORIES "${_v8_root}/include/v8"
    INTERFACE_LINK_LIBRARIES "Threads::Threads;${CMAKE_DL_LIBS};m")
add_custom_command(OUTPUT "${PROJECT_BINARY_DIR}/dependencies-ready.stamp"
    COMMAND ${Python3_EXECUTABLE} ${PROJECT_SOURCE_DIR}/scripts/prepare_dependencies.py
        --prefix ${VELLUM_DEPS_PREFIX} --work ${VELLUM_DEPS_WORK}
        --jobs ${VELLUM_DEPENDENCY_JOBS}
        --mupdf-root=${VELLUM_MUPDF_ROOT} --v8-root=${VELLUM_V8_ROOT}
    COMMAND ${CMAKE_COMMAND} -E touch "${PROJECT_BINARY_DIR}/dependencies-ready.stamp"
    DEPENDS ${PROJECT_SOURCE_DIR}/scripts/prepare_dependencies.py ${PROJECT_SOURCE_DIR}/docs/dependencies.lock.json
    BYPRODUCTS "${_mupdf_root}/lib/libmupdf.a" "${_mupdf_root}/lib/libmupdf-third.a"
               "${_v8_root}/lib/libv8_monolith.a"
    USES_TERMINAL VERBATIM)
add_custom_target(vellum_dependencies DEPENDS "${PROJECT_BINARY_DIR}/dependencies-ready.stamp")
add_dependencies(vellum_mupdf vellum_dependencies)
add_dependencies(vellum_v8 vellum_dependencies)
