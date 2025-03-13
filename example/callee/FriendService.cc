#include <iostream>
#include <string>
#include "friend.pb.h"
#include "MprpcApplication.h"
#include "MprpcProvider.h"
#include <vector>
#include "logger.h"

class FriendService : public fixbug::FiendServiceRpc
{
public:
    std::vector<std::string> GetFriendsList(uint32_t userid)
    {
        std::cout << "do GetFriendsList service! userid:" << userid << std::endl;
        std::vector<std::string> vec;
        vec.push_back("gao yang");
        vec.push_back("liu hong");
        vec.push_back("wang shuo");
        return vec;
    }

    // 重写基类方法
    void GetFriendsList(::google::protobuf::RpcController* controller,
                       const ::fixbug::GetFriendsListRequest* request,
                       ::fixbug::GetFriendsListResponse* response,
                       ::google::protobuf::Closure* done)
    {
        // 获取数据
        uint32_t userid = request->userid();
         
        // 做本地业务
        std::vector<std::string> friendsList = GetFriendsList(userid);

        // 把响应写入
        response->mutable_result()->set_errcode(0);
        response->mutable_result()->set_errmsg("");
        for (std::string &name : friendsList)
        {
            std::string *p = response->add_friends();
            *p = name;
        }

        // 执行回调
        done->Run();
    }
};

int main(int argc, char **argv)
{
    LOG_ERR("%s:%s:%d",__FILE__,__FUNCTION__,__LINE__);
    LOG_INFO("first log message!");

    // 调用框架的初始化操作
    MprpcApplication::Init(argc, argv);

    // provider是一个rpc网络服务对象。把UserService对象发布到rpc节点上
    MprpcProvider provider;
    provider.NotifyService(new FriendService());

    // 启动一个rpc服务发布节点   Run以后，进程进入阻塞状态，等待远程的rpc调用请求
    provider.Run();

    return 0;
}