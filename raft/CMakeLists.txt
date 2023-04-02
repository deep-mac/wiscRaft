# Copyright 2018 gRPC authors.
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.
#
# cmake build file for C++ helloworld example.
# Assumes protobuf and gRPC have been installed using cmake.
# See cmake_externalproject/CMakeLists.txt for all-in-one cmake build
# that automatically builds all the dependencies before building helloworld.

cmake_minimum_required(VERSION 3.5.1)

project(wiscRaft C CXX)

include(common.cmake)

# Proto file
get_filename_component(database_proto "../protos/database.proto" ABSOLUTE)
get_filename_component(database_proto_path "${database_proto}" PATH)
get_filename_component(raft_proto "../protos/raft.proto" ABSOLUTE)
get_filename_component(raft_proto_path "${raft_proto}" PATH)

get_filename_component(database_path "../database" ABSOLUTE)
get_filename_component(log_path "../log" ABSOLUTE)


# Generated sources
set(database_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/database.pb.cc")
set(database_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/database.pb.h")
set(database_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/database.grpc.pb.cc")
set(database_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/database.grpc.pb.h")

set(raft_proto_srcs "${CMAKE_CURRENT_BINARY_DIR}/raft.pb.cc")
set(raft_proto_hdrs "${CMAKE_CURRENT_BINARY_DIR}/raft.pb.h")
set(raft_grpc_srcs "${CMAKE_CURRENT_BINARY_DIR}/raft.grpc.pb.cc")
set(raft_grpc_hdrs "${CMAKE_CURRENT_BINARY_DIR}/raft.grpc.pb.h")

set(database_srcs "${CMAKE_CURRENT_BINARY_DIR}/../../../database/database.cc")
set(database_hdrs "${CMAKE_CURRENT_BINARY_DIR}/../../../database/database.h")
set(log_srcs "${CMAKE_CURRENT_BINARY_DIR}/../../../log/log.cc")
set(log_hdrs "${CMAKE_CURRENT_BINARY_DIR}/../../../log/log.h")

add_custom_command(
      OUTPUT "${database_proto_srcs}" "${database_proto_hdrs}" "${database_grpc_srcs}" "${database_grpc_hdrs}" "${raft_proto_srcs}" "${raft_proto_hdrs}" "${raft_grpc_srcs}" "${raft_grpc_hdrs}" "${database_srcs}" "${database_hdrs}" "${log_srcs}" "${log_hdrs}"
      COMMAND ${_PROTOBUF_PROTOC}
      ARGS --grpc_out "${CMAKE_CURRENT_BINARY_DIR}"
        --cpp_out "${CMAKE_CURRENT_BINARY_DIR}"
	-I "${database_proto_path}" 
        --plugin=protoc-gen-grpc="${_GRPC_CPP_PLUGIN_EXECUTABLE}"
        "${database_proto}" "${raft_proto}"
      DEPENDS "${database_proto}" "${raft_proto}")

# Include generated *.pb.h files
include_directories("${CMAKE_CURRENT_BINARY_DIR}")
include_directories("${log_path}")
include_directories("${database_path}")

# hw_grpc_proto
add_library(grpc_proto
  ${database_grpc_srcs}
  ${database_grpc_hdrs}
  ${database_proto_srcs}
  ${database_proto_hdrs}
  ${raft_grpc_srcs}
  ${raft_grpc_hdrs}
  ${raft_proto_srcs}
  ${raft_proto_hdrs}
  ${database_srcs}
  ${database_hdrs}
  ${log_srcs}
  ${log_hdrs}
  )
target_link_libraries(grpc_proto
  ${_REFLECTION}
  ${_GRPC_GRPCPP}
  ${_PROTOBUF_LIBPROTOBUF})

# Targets greeter_[async_](database|raft)
foreach(_target
  client server)
  add_executable(${_target} "${_target}.cc")
  target_link_libraries(${_target}
    grpc_proto
    ${_REFLECTION}
    ${_GRPC_GRPCPP}
    ${_PROTOBUF_LIBPROTOBUF})
endforeach()
