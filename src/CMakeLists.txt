# 当前文件夹下所有文件为 SRC_LIST
#aux_source_directory(. SRC_LIST)
set(SRC_LIST 
    MprpcApplication.cc 
    MprpcConfig.cc 
    rpcheader.pb.cc 
    MprpcProvider.cc 
    MprpcChannel.cc
    MprpcController.cc
    logger.cc
    zookeeperutil.cc)
add_library(mprpc ${SRC_LIST})
target_link_libraries(mprpc muduo_net muduo_base pthread zookeeper_mt)