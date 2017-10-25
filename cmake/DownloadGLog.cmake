if (CMAKE_VERSION VERSION_LESS 3.2)
  set(UPDATE_DISCONNECTED_IF_AVAILABLE "")
else()
  set(UPDATE_DISCONNECTED_IF_AVAILABLE "UPDATE_DISCONNECTED 1")
endif()

download_project(PROJ                glog
             GIT_REPOSITORY      https://github.com/google/glog.git
             GIT_TAG             master
             ${UPDATE_DISCONNECTED_IF_AVAILABLE}
)

# Prevent GTest from overriding our compiler/linker options
# when building with Visual Studio
set(glog_force_shared_crt ON CACHE BOOL "" FORCE)

add_subdirectory(${glog_SOURCE_DIR} ${glog_BINARY_DIR})

# When using CMake 2.8.11 or later, header path dependencies
# are automatically added to the gtest and gmock targets.
# For earlier CMake versions, we have to explicitly add the
# required directories to the header search path ourselves.
if (CMAKE_VERSION VERSION_LESS 2.8.11)
  include_directories("${glog_SOURCE_DIR}/include")
endif()
