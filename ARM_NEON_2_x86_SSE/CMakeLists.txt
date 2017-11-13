# Boilerplate: Copyright (C) 2014-2017 Ruslan Baratov
# Modification: Copyright (C) 2017 David Hirvonen

cmake_minimum_required(VERSION 3.0)
project(NEON_2_SSE VERSION 1.0.0)

add_library(${PROJECT_NAME} INTERFACE)

target_include_directories(${PROJECT_NAME}
  INTERFACE
  "$<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>"
  )

### Install ###
set(generated_dir "${CMAKE_CURRENT_BINARY_DIR}/generated")

set(config_install_dir "lib/cmake/${PROJECT_NAME}")
set(include_install_dir "include")

set(version_config "${generated_dir}/${PROJECT_NAME}ConfigVersion.cmake")
set(project_config "${generated_dir}/${PROJECT_NAME}Config.cmake")
set(targets_export_name "${PROJECT_NAME}Targets")
set(namespace "${PROJECT_NAME}::")

include(CMakePackageConfigHelpers)
write_basic_package_version_file(
    "${version_config}" COMPATIBILITY SameMajorVersion
)

# Note: use 'targets_export_name'
configure_file("cmake/Config.cmake.in" "${project_config}" @ONLY)

install(
    TARGETS ${PROJECT_NAME}
    EXPORT "${targets_export_name}"
    INCLUDES DESTINATION "${include_install_dir}"
)

install(
    FILES NEON_2_SSE.h
    DESTINATION "${include_install_dir}"
)

install(
    FILES "${project_config}" "${version_config}"
    DESTINATION "${config_install_dir}"
)

install(
    EXPORT "${targets_export_name}"
    NAMESPACE "${namespace}"
    DESTINATION "${config_install_dir}"
)
