
cmake_minimum_required(VERSION 3.22)

cmake_policy(SET CMP0091 NEW)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>DLL")
if(DEFINED VCPKG_HOST_TRIPLET AND NOT VCPKG_HOST_TRIPLET STREQUAL "")
  set(gRPC_BUILD_CODEGEN OFF)
endif()

project(snfm)

set ( Protobuf_USE_STATIC_LIBS ON )
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)
find_package(yaml-cpp REQUIRED)
# Force wxwidgets to turn off debug build
set(wxWidgets_USE_DEBUG OFF)
# Workaround issue where wxwidgets can't find a release only build with the above flag
message(STATUS "WXWIDGETS DIR " ${wxWidgets_LIB_DIR})
if(CMAKE_BUILD_TYPE MATCHES "Release")
  set(wxWidgets_LIB_DIR ${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/lib)
endif()
message(STATUS "new WXWIDGETS DIR " ${wxWidgets_LIB_DIR})
find_package(wxWidgets REQUIRED COMPONENTS core base)

set(CMAKE_CXX_STANDARD 20)
get_property(GRPC_CPP_PLUGIN TARGET gRPC::grpc_cpp_plugin PROPERTY IMPORTED_LOCATION_RELEASE )

function(PROTOBUF_GENERATE_GRPC_CPP SRCS HDRS)
  if(NOT ARGN)
    message(SEND_ERROR "Error: PROTOBUF_GENERATE_GRPC_CPP() called without any proto files")
    return()
  endif()

  if(PROTOBUF_GENERATE_CPP_APPEND_PATH) # This variable is common for all types of output.
    # Create an include path for each file specified
    foreach(FIL ${ARGN})
      get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
      get_filename_component(ABS_PATH ${ABS_FIL} PATH)
      list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
      if(${_contains_already} EQUAL -1)
          list(APPEND _protobuf_include_path -I ${ABS_PATH})
      endif()
    endforeach()
  else()
    set(_protobuf_include_path -I ${CMAKE_CURRENT_SOURCE_DIR})
  endif()

  if(DEFINED PROTOBUF_IMPORT_DIRS)
    foreach(DIR ${Protobuf_IMPORT_DIRS})
      get_filename_component(ABS_PATH ${DIR} ABSOLUTE)
      list(FIND _protobuf_include_path ${ABS_PATH} _contains_already)
      if(${_contains_already} EQUAL -1)
          list(APPEND _protobuf_include_path -I ${ABS_PATH})
      endif()
    endforeach()
  endif()

  set(${SRCS})
  set(${HDRS})
  foreach(FIL ${ARGN})
    get_filename_component(ABS_FIL ${FIL} ABSOLUTE)
    get_filename_component(FIL_WE ${FIL} NAME_WE)

    list(APPEND ${SRCS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.cc")
    list(APPEND ${HDRS} "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.h")

    add_custom_command(
      OUTPUT "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.cc"
             "${CMAKE_CURRENT_BINARY_DIR}/${FIL_WE}.grpc.pb.h"
      COMMAND  ${Protobuf_PROTOC_EXECUTABLE}
      ARGS --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
           --plugin=protoc-gen-grpc=${GRPC_CPP_PLUGIN}
           ${_protobuf_include_path} ${ABS_FIL}
      DEPENDS ${ABS_FIL} ${Protobuf_PROTOC_EXECUTABLE}
      COMMENT "Running gRPC C++ protocol buffer compiler on ${FIL}"
      VERBATIM)
  endforeach()

  set_source_files_properties(${${SRCS}} ${${HDRS}} PROPERTIES GENERATED TRUE)
  set(${SRCS} ${${SRCS}} PARENT_SCOPE)
  set(${HDRS} ${${HDRS}} PARENT_SCOPE)

endfunction()

protobuf_generate_cpp(PROTO_SRCS PROTO_HDRS sni/protos/sni/sni.proto)
PROTOBUF_GENERATE_GRPC_CPP(PROTO_GRPC_SRCS PROTO_GRPC_HDRS sni/protos/sni/sni.proto)

INCLUDE_DIRECTORIES(${CMAKE_CURRENT_BINARY_DIR} include icons %)

add_library(snfm_common ${PROTO_SRCS} ${PROTO_HDRS} ${PROTO_GRPC_SRCS} ${PROTO_GRPC_HDRS})
target_link_libraries(snfm_common protobuf::libprotoc protobuf::libprotobuf gRPC::grpc++_unsecure)
if(WIN32)
  target_compile_definitions(snfm_common PUBLIC NOMINMAX)
endif()

add_library(config "src/config.cpp")
target_link_libraries(config PRIVATE yaml-cpp)
if(WIN32)
    target_link_libraries(config PRIVATE Shlwapi.lib)
endif(WIN32)
add_executable(send_file 
    "src/send_file.cpp" 
    "src/snfm.cpp"
    "src/config.cpp")
target_link_libraries(send_file PRIVATE snfm_common config)
if(WIN32)
  target_sources(send_file PRIVATE "windows/send_file.rc")  
endif()

add_executable(manage_files WIN32 "src/sn_file_tree.cpp" "src/sn_file_tree_ui_events.cpp" "src/manage_files.cpp" "src/snfm.cpp")
if(WIN32)
    target_sources(manage_files PRIVATE "src/win_dnd.cpp" "windows/manage_files.rc")
endif(WIN32)
target_link_libraries(manage_files PRIVATE snfm_common)
target_compile_definitions(manage_files PRIVATE ${wxWidgets_DEFINITIONS} "$<$<CONFIG:DEBUG>:${wxWidgets_DEFINITIONS_DEBUG}>" wxDEBUG_LEVEL=0 )
target_include_directories(manage_files PRIVATE ${wxWidgets_INCLUDE_DIRS})
target_link_libraries(manage_files PRIVATE ${wxWidgets_LIBRARIES}) 

if(WIN32)
    # Magic bits to enable MFC
    add_definitions(-D_AFXDLL)
    set(CMAKE_MFC_FLAG 2)
    set_target_properties(manage_files PROPERTIES WIN32_EXECUTABLE TRUE)
endif()

add_custom_command(
        TARGET send_file POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy
                ${CMAKE_SOURCE_DIR}/snfm_config_example.yaml
                ${CMAKE_CURRENT_BINARY_DIR}/snfm_config_example.yaml)


add_custom_command(
TARGET send_file POST_BUILD
COMMAND ${CMAKE_COMMAND} -E copy
        ${CMAKE_SOURCE_DIR}/snfm_user_manual.md

        ${CMAKE_CURRENT_BINARY_DIR}/snfm_user_manual.md)


# https://github.com/microsoft/vcpkg/issues/25300
# Remove -ignore:4221 from all INTERFACE_LINK_OPTIONS
macro(get_all_targets_recursive targets dir)
message("dir ${dir}")
    get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir ${subdirectories})
        get_all_targets_recursive(${targets} ${subdir})
    endforeach()

    get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    foreach(t ${current_targets})
        # get_target_property(target_type ${t} TYPE)
        # if (NOT ${target_type} STREQUAL "INTERFACE_LIBRARY" AND NOT ${target_type} STREQUAL "UTILITY")
            list(APPEND ${targets} ${t})
            message("target ${t}")
        # endif()
    endforeach()
endmacro()
function(get_all_targets var)
    set(targets)
    get_all_targets_recursive(targets ${CMAKE_CURRENT_SOURCE_DIR})
    set(${var} ${targets} PARENT_SCOPE)
endfunction()
get_all_targets(all_targets)

# foreach(t ${_targets})
#     get_target_property(_opts ${t} INTERFACE_LINK_OPTIONS)
#     if(_opts)
#         list(REMOVE_ITEM _opts "-ignore:4221")
#         set_property(TARGET ${t} PROPERTY INTERFACE_LINK_OPTIONS "${_opts}")
#     endif()
# endforeach()


foreach(t ${all_targets})
    get_target_property(_type ${t} TYPE)
    if(NOT _type MATCHES "IMPORTED") 
        continue()
    endif()

    get_target_property(_lo  ${t} LINK_OPTIONS)
    get_target_property(_ilo ${t} INTERFACE_LINK_OPTIONS)

    message(STATUS "=== Imported: ${t} ===")
    message(STATUS "  LINK_OPTIONS:            ${_lo}")
    message(STATUS "  INTERFACE_LINK_OPTIONS:  ${_ilo}")
endforeach()
