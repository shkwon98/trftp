# TRFTP: Trivial but Reliable File Transfer Protocol C++ Implementation

[![Deploy doxygen to Pages](https://github.com/shkwon98/trftp/actions/workflows/deploy_docs.yml/badge.svg)](https://github.com/shkwon98/trftp/actions/workflows/deploy_docs.yml)

## Table of Contents

- [TRFTP: Trivial but Reliable File Transfer Protocol C++ Implementation](#trftp-trivial-but-reliable-file-transfer-protocol-c-implementation)
  - [Table of Contents](#table-of-contents)
  - [About ](#about-)
  - [Getting Started ](#getting-started-)
    - [find\_package](#find_package)
    - [FetchContent](#fetchcontent)
    - [git submodule](#git-submodule)

## About <a name = "about"></a>

**trftp** is a C++ library that transfers files between a client and a server like ftp or tftp. It is meant to be a simple and easy-to-use library that can be used in any C++ project that requires file transfer using only UDP. 

* [Documentation][documentation]

## Getting Started <a name = "getting_started"></a>

This section describes how to add **trftp** as a dependency to your C++ project.
While **trftp** requires C++17, it has no other dependencies and is easy to integrate into your project.
**trftp** supports CMake, so you can easily add it to your project by following the instructions below.

### find_package

The canonical way to discover dependencies in CMake is the [`find_package` command](https://cmake.org/cmake/help/latest/command/find_package.html).

```cmake
find_package(trftp CONFIG REQUIRED)
add_executable(my_exe my_exe.cpp)
target_link_libraries(my_exe trftp::trftp)
```

`find_package` can only find software that has already been installed on your system. In practice that means you'll need to install **trftp** using cmake first.

### FetchContent
If you are using CMake v3.11 or newer you'd better use CMake's [FetchContent module](https://cmake.org/cmake/help/latest/module/FetchContent.html).
The first time you run CMake in a given build directory, FetchContent will clone the **trftp** repository. `FetchContent_MakeAvailable()` also sets up an `add_subdirectory()` rule for you. This causes **trftp** to be built as part of your project.

```cmake
cmake_minimum_required(VERSION 3.11)
project(my_project)

include(FetchContent)
FetchContent_Declare(trftp
    GIT_REPOSITORY https://github.com/shkwon98/trftp.git
    GIT_TAG main
    GIT_SHALLOW TRUE
    EXCLUDE_FROM_ALL
)
set(FETCHCONTENT_QUIET OFF)
FetchContent_MakeAvailable(trftp)

add_executable(my_exe my_exe.cpp)
target_link_libraries(my_exe trftp::trftp)
```

### git submodule
If you cannot use FetchContent, another approach is to add the **trftp** source tree to your project as a [git submodule](https://git-scm.com/book/en/v2/Git-Tools-Submodules).
You can then add it to your CMake project with `add_subdirectory()`.

```cmake
add_subdirectory(trftp)
add_executable(my_exe my_exe.cpp)
target_link_libraries(my_exe trftp::trftp)
```


[documentation]:
    https://shkwon98.github.io/trftp/
    "trftp documentation"

