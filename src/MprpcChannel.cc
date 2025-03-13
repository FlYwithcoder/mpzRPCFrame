#include "MprpcChannel.h"
#include <string>
#include "rpcheader.pb.h"
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <unistd.h>
#include <errno.h>
#include "MprpcApplication.h"
#include "MprpcController.h"
#include "zookeeperutil.h"

#include <google/protobuf/util/json_util.h>
#include <google/protobuf/message.h>

std::mutex g_data_mutx;

/*
header_size + service_name method_name args_size + args
*/
// 所有通过stub代理对象调用的rpc方法，都走到这里了，统一做rpc方法调用的数据数据序列化和网络发送 
void MprpcChannel::CallMethod(const google::protobuf::MethodDescriptor* method,
                                google::protobuf::RpcController* controller, 
                                const google::protobuf::Message* request,
                                google::protobuf::Message* response,
                                google::protobuf:: Closure* done)
{
    const google::protobuf::ServiceDescriptor* sd = method->service();
    std::string service_name = sd->name(); // service_name
    std::string method_name = method->name(); // method_name

    // 获取参数的序列化字符串长度 args_size
    uint32_t args_size = 0;
    std::string args_str;

#ifdef JSON
    // 将请求对象序列化为 JSON 字符串
    google::protobuf::util::JsonPrintOptions options;
    options.add_whitespace = true; // 可选：添加空格以提高可读性
    options.always_print_primitive_fields = true; // 确保所有字段都被打印
    options.preserve_proto_field_names = true; // 使用原始字段名称
    // 使用 JSON 进行序列化
    google::protobuf::util::MessageToJsonString(*request, &args_str);

#else
    if (request->SerializeToString(&args_str))
    {
        args_size = args_str.size();
    }
    else
    {
        controller->SetFailed("serialize request error!");
        return;
    }

#endif
    // 定义rpc的请求header
    mprpc::RpcHeader rpcHeader;
    rpcHeader.set_service_name(service_name);
    rpcHeader.set_method_name(method_name);
    rpcHeader.set_args_size(args_size);

    uint32_t header_size = 0;
    std::string rpc_header_str;
    if (rpcHeader.SerializeToString(&rpc_header_str))
    {
        header_size = rpc_header_str.size();
    }
    else
    {
        controller->SetFailed("serialize rpc header error!");
        return;
    }

    // 组织待发送的rpc请求的字符串
    std::string send_rpc_str;
/*
    uint32_t net_header_size = htonl(header_size);
    send_rpc_str.insert(0, std::string((char*)&net_header_size, 4));
*/
    send_rpc_str.insert(0, std::string((char*)&header_size, 4)); // header_size
/*
    1、(char*)&header_size：将 header_size 这个整数类型的变量转换为指向其内存地址的指针，
    然后强制类型转换为 char* 类型。这样做的目的是为了将整数转换为4字节的字符数组。
    2、std::string((char*)&header_size, 4)：使用 std::string 的构造函数，将上述的 char* 类型的指针和长度（4）作为参数，
    创建一个新的字符串，这个字符串包含了 header_size 的4个字节。
    3、send_rpc_str.insert(0, ...)：使用 std::string 的 insert 方法，将新创建的字符串插入到 end_rpc_str 的开头。
*/
    send_rpc_str += rpc_header_str; // rpcheader
    send_rpc_str += args_str; // args

    // 打印调试信息
    std::cout << "============================================" << std::endl;
    std::cout << "header_size: " << header_size << std::endl; 
    std::cout << "rpc_header_str: " << rpc_header_str << std::endl; 
    std::cout << "service_name: " << service_name << std::endl; 
    std::cout << "method_name: " << method_name << std::endl; 
    std::cout << "args_str: " << args_str << std::endl; 
    std::cout << "============================================" << std::endl;

    // 使用tcp编程，完成rpc方法的远程调用
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if (-1 == clientfd)
    {
        char errtxt[512] = {0};
        sprintf(errtxt, "create socket error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 读取配置文件rpcserver的信息
    // std::string ip = MprpcApplication::GetInstance().GetConfig().Load("rpcserverip");
    // uint16_t port = atoi(MprpcApplication::GetInstance().GetConfig().Load("rpcserverport").c_str());
    // rpc调用方想调用service_name的method_name服务，需要查询zk上该服务所在的host信息
    ZkClient zkCli;
    zkCli.Start();
    //  /UserServiceRpc/Login
    std::string method_path = "/" + service_name + "/" + method_name;
    // 127.0.0.1:8000
    std::string host_data = zkCli.GetData(method_path.c_str());
    if (host_data == "")
    {
        controller->SetFailed(method_path + " is not exist!");
        return;
    }
    int idx = host_data.find(":");
    if (idx == -1)
    {
        controller->SetFailed(method_path + " address is invalid!");
        return;
    }
    std::string ip = host_data.substr(0, idx);
    uint16_t port = atoi(host_data.substr(idx+1, host_data.size()-idx).c_str()); 

    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip.c_str());

    // 连接rpc服务节点
    if (-1 == connect(clientfd, (struct sockaddr*)&server_addr, sizeof(server_addr)))
    {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "connect error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 发送rpc请求
    if (-1 == send(clientfd, send_rpc_str.c_str(), send_rpc_str.size(), 0))
    {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "send error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 接收rpc请求的响应值  
    char recv_buf[1024] = {0};
    int recv_size = 0;
    if (-1 == (recv_size = recv(clientfd, recv_buf, 1024, 0)))
    {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "recv error! errno:%d", errno);
        controller->SetFailed(errtxt);
        return;
    }

    // 反序列化rpc调用的响应数据
    // std::string response_str(recv_buf, 0, recv_size); // bug出现问题，recv_buf中遇到\0后面的数据就存不下来了，导致反序列化失败
    // if (!response->ParseFromString(response_str))

#ifdef JSON
    // 使用 JSON 解析响应
    google::protobuf::util::JsonStringToMessage(recv_buf, response);
#else
    // 使用 protobuf 解析响应
    if (!response->ParseFromArray(recv_buf, recv_size))
    {
        close(clientfd);
        char errtxt[512] = {0};
        sprintf(errtxt, "parse error! response_str:%s", recv_buf);
        controller->SetFailed(errtxt);
        return;
    }
#endif

    close(clientfd);

/*
    // 接收rpc请求的响应值
    uint32_t net_expected_size = 0;
    if (recv(clientfd, (char*)&net_expected_size, 4, 0) <= 0) {
        close(clientfd);
        controller->SetFailed("recv header size failed");
        return;
    }
    uint32_t expected_size = ntohl(net_expected_size);

    // 使用循环确保读取完整数据
    char* recv_buf = new char[expected_size];
    int total_received = 0;
    while (total_received < expected_size) {
        int received = recv(clientfd, recv_buf + total_received, expected_size - total_received, 0);
        if (received <= 0) {
            close(clientfd);
            delete[] recv_buf;
            controller->SetFailed("recv error! Connection closed or failed.");
            return;
        }
        total_received += received;
    }

    if (!response->ParseFromArray(recv_buf, expected_size)) {
        close(clientfd);
        delete[] recv_buf;
        controller->SetFailed("parse error! response data corrupted.");
        return;
    }

    delete[] recv_buf;
    close(clientfd);
*/
}

bool MprpcChannel::newConnect(const char* ip, uint16_t port)
{
    // 使用socket 网络编程
    int clientfd = socket(AF_INET, SOCK_STREAM, 0);
    if(-1 == clientfd)
    {
        char errtxt[512] = {0};
        // std::cout << "socket error" << strerror_r(error, errtxt, sizeof(errtxt)) << std::endl;
        // LOG(ERROR) << "socket error:" << errtxt;
        return false;
    }
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    server_addr.sin_addr.s_addr = inet_addr(ip);

    if(-1 == connect(clientfd,(struct sockaddr*)&server_addr, sizeof(server_addr)))
    {
        close(clientfd);
        char errtxt[512] = {0};
        // std::cout << "connect error" << strerror_r(error, errtxt, sizeof(errtxt)) << std::endl;
        // LOG(ERROR) << "connect error:" << errtxt;
        return false;
    }
    m_clientfd = clientfd;
    return true;
}

std::string QueryServiceHost(ZkClient* zkclient,std::string service_name,std::string method_name,int idx)
{
    std::string method_path = "/" + service_name + "/" + method_name;
    std::cout << "method_path: " << method_path << std::endl;
    std::unique_lock<std::mutex> lock(g_data_mutx);
    std::string host_data_1 = zkclient->GetData(method_path.c_str());
    lock.unlock();
    if(host_data_1 == "")
    {
        // LOG(ERROR) << method_path + "is not exist!";
        return " ";
    }
    idx = host_data_1.find(":"); //127.0.0.1:8000 获取到的ip和port端口
    if(idx == -1)
    {
        // LOG(ERROR) << method_path + "is not invalid!";
        return " ";
    }
    return host_data_1;
}

MprpcChannel::MprpcChannel(bool connectNow) : m_clientfd(-1),m_idx(0)
{
    if(!connectNow)
    {
        return;
    }
    auto rt = newConnect(m_ip.c_str(), m_port); //判断是否连接成功
    int count = 3;  //重试次数
    while(!rt && count--)
    {
        rt = newConnect(m_ip.c_str(),m_port);
    }
}