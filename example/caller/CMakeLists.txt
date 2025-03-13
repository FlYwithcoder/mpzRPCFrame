set(SRC_LIST CallUserService.cc ../user.pb.cc)
# set(SRC_LIST CallFriendService.cc ../friend.pb.cc)

add_executable(consumer ${SRC_LIST})

# 查找 Protobuf
find_package(Protobuf REQUIRED)

# 添加 Protobuf 头文件路径
include_directories(${PROTOBUF_INCLUDE_DIRS})

# 链接库
target_link_libraries(consumer mprpc ${PROTOBUF_LIBRARIES} ${PROTOBUF_PROTOC_LIBRARIES})

# target_link_libraries(consumer mprpc protobuf)