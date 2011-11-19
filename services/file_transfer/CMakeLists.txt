ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/checkbook.pb.cc	${CMAKE_CURRENT_BINARY_DIR}/checkbook.pb.h 
	COMMAND protoc --proto_path=${CMAKE_CURRENT_SOURCE_DIR} --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/checkbook.proto
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/checkbook.proto
	)
ADD_CUSTOM_COMMAND(
	OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/file_transfer.pb.cc	${CMAKE_CURRENT_BINARY_DIR}/file_transfer.pb.h 
	COMMAND protoc --proto_path=${CMAKE_CURRENT_SOURCE_DIR} --cpp_out=${CMAKE_CURRENT_BINARY_DIR} ${CMAKE_CURRENT_SOURCE_DIR}/file_transfer.proto
	DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/file_transfer.proto
	)


INCLUDE_DIRECTORIES(
	${CMAKE_CURRENT_SOURCE_DIR}
	${CMAKE_CURRENT_BINARY_DIR}
	${CMAKE_CURRENT_SOURCE_DIR}/../../net
	)

SET(file_transfer_srcs
	checkbook.pb.cc
	checkbook.cpp
	file_transfer.pb.cc
	file_transfer_client.cpp
	file_transfer_service.cpp
	file_download_client.cpp
	file_download_service.cpp
	)

add_library(file_transfer ${file_transfer_srcs})

add_executable(file_download_server file_download_service_main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../../net/linux/mac_address.cpp)
target_link_libraries(file_download_server file_transfer server1_base server1_server libprotobuf
	${home_dir}/third_party/glog/lib/libglog.so
	${home_dir}/third_party/gflags/lib/libgflags.so 
	${OPENSSL_LIBRARIES}
	${boost_system_target}
	${boost_thread_target} ${boost_filesystem_target} ${boost_iostreams_target} ${ZLIB_LIBRARY})
add_executable(file_download_client file_download_client_main.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/../../net/linux/mac_address.cpp)
target_link_libraries(file_download_client file_transfer server1_base server1_server libprotobuf
	${home_dir}/third_party/glog/lib/libglog.so
	${home_dir}/third_party/gflags/lib/libgflags.so 
	${OPENSSL_LIBRARIES}
	${boost_system_target}
	${boost_thread_target} ${boost_filesystem_target} ${boost_iostreams_target} ${ZLIB_LIBRARY})
