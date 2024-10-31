cmake_minimum_required(VERSION 3.14)

project(trftp
    VERSION 0.0.1
    LANGUAGES CXX
    DESCRIPTION "Trivial but Reliable File Transfer Protocol (TRFTP)"
)

# Set default build type to Release if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

# Option to build shared or static library (default is static)
if (BUILD_SHARED_LIBS)
    set(TRFTP_BUILD_SHARED_LIBS_DEFAULT ON)
else()
    set(TRFTP_BUILD_SHARED_LIBS_DEFAULT OFF)
endif()

option(TRFTP_BUILD_SHARED_LIBS "Build Shared Libraries" ${TRFTP_BUILD_SHARED_LIBS_DEFAULT})
if (TRFTP_BUILD_SHARED_LIBS)
    set(TRFTP_SHARED_OR_STATIC "SHARED")
else()
    set(TRFTP_SHARED_OR_STATIC "STATIC")
endif()

find_package(Threads REQUIRED)


# trftp::trftp-server
add_library(trftp-server ${TRFTP_SHARED_OR_STATIC})
target_sources(trftp-server
    PRIVATE src/server/server.cpp
            src/util.cpp
)
target_include_directories(trftp-server
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(trftp-server
    PRIVATE Threads::Threads
)
target_compile_features(trftp-server
    PUBLIC  cxx_std_17
)
set_target_properties(trftp-server PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME trftp-server
)
add_library(trftp::trftp-server ALIAS trftp-server)


# trftp::trftp-client
add_library(trftp-client ${TRFTP_SHARED_OR_STATIC})
target_sources(trftp-client
    PRIVATE src/client/client.cpp
            src/util.cpp
)
target_include_directories(trftp-client
    PUBLIC  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(trftp-client
    PRIVATE Threads::Threads
)
target_compile_features(trftp-client
    PUBLIC  cxx_std_17
)
set_target_properties(trftp-client PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME trftp-client
)
add_library(trftp::trftp-client ALIAS trftp-client)


# trftp::trftp
add_library(trftp ${TRFTP_SHARED_OR_STATIC})
target_sources(trftp
    PRIVATE src/server/server.cpp
            src/client/client.cpp
            src/util.cpp
)
target_link_libraries(trftp
    PUBLIC  trftp::trftp-server
            trftp::trftp-client
)
set_target_properties(trftp PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    EXPORT_NAME trftp
)
add_library(trftp::trftp ALIAS trftp)


# Installation options
include(CMakeDependentOption)
option(TRFTP_INSTALL "Generate the install target" ON)
cmake_dependent_option(TRFTP_INSTALL_EXPORT "Generate the export target for installation" ON
    "TRFTP_INSTALL" OFF
)
if (NOT TRFTP_INSTALL)
    return()
endif()


# Install targets and headers
include(GNUInstallDirs)

install(TARGETS trftp trftp-server trftp-client
    EXPORT trftp-targets
    ARCHIVE
        DESTINATION ${CMAKE_INSTALL_LIBDIR}    # Install static library
        COMPONENT lib
    LIBRARY
        DESTINATION ${CMAKE_INSTALL_LIBDIR}    # If you use shared in the future
        COMPONENT lib
        NAMELINK_COMPONENT dev
    RUNTIME
        DESTINATION ${CMAKE_INSTALL_BINDIR}    # For executables (not used here)
        COMPONENT bin
    INCLUDES
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY include/
    DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    COMPONENT Development
)

if (NOT TRFTP_INSTALL_EXPORT)
    return()
endif()


# Generate the version, config and target files
include(CMakePackageConfigHelpers)
set(config_install_dir ${CMAKE_INSTALL_LIBDIR}/cmake/trftp)
set(version_config ${CMAKE_CURRENT_BINARY_DIR}/generated/trftp-config-version.cmake)
set(project_config ${CMAKE_CURRENT_BINARY_DIR}/generated/trftp-config.cmake)
set(targets_export_name trftp-targets)

write_basic_package_version_file(
    ${version_config}
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion
)

configure_package_config_file(
    cmake/trftp-config.cmake.in
    ${project_config}
    INSTALL_DESTINATION ${config_install_dir}
    PATH_VARS CMAKE_INSTALL_INCLUDEDIR
)

# Install config files
install(
    FILES ${project_config} ${version_config}
    DESTINATION ${config_install_dir}
    COMPONENT dev
)

# Install the targets file
install(
    EXPORT ${targets_export_name}
    NAMESPACE trftp::
    DESTINATION ${config_install_dir}
    COMPONENT dev
)

# Export package for use from the build tree
export(EXPORT ${targets_export_name}
    FILE ${CMAKE_CURRENT_BINARY_DIR}/${targets_export_name}.cmake
    NAMESPACE trftp::
)
export(PACKAGE trftp)
