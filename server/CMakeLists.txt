ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/meta.pb.cc	${CMAKE_CURRENT_BINARY_DIR}/meta.pb.h 
	COMMAND protoc --proto_path=${CMAKE_CURRENT_SOURCE_DIR} --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/meta.proto
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/meta.proto
	)

set(server_srcs
	meta.pb.cc
	client_connection.cpp
	connection.cpp
	io_service_pool.cpp
	posix_client.cpp
	posix_main.cpp
	protobuf_connection.cpp
	raw_connection.cpp
	raw_protobuf_connection.cpp
	rpc_main.cpp
	server.cpp
	timer_master.cpp
	)
add_library(server1_server ${server_srcs})
add_dependencies(server1_server hellopb)
