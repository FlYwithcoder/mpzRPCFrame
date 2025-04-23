#include <iostream>
#include <thread>
#include <vector>
#include <chrono>
#include <atomic>
#include <iomanip>
#include "MprpcApplication.h"
#include "MprpcChannel.h"
#include "MprpcController.h"
#include "user.pb.h"

using namespace fixbug;

// 压测指标统计
struct Metrics {
    std::atomic<int> success_count{0};
    std::atomic<int> fail_count{0};
    std::atomic<long long> total_latency{0};
};

void send_request(Metrics& metrics, int thread_id) {
    try {
        // 创建 stub 对象
        std::shared_ptr<MprpcChannel> channel = std::make_shared<MprpcChannel>();
        if (!channel) {
            throw std::runtime_error("Failed to create MprpcChannel");
        }
        
        UserServiceRpc_Stub stub(channel.get());

        // 准备请求参数
        LoginRequest request;
        request.set_name("test");
        request.set_pwd("123456");
        
        LoginResponse response;
        MprpcController controller;

        // 记录开始时间
        auto start = std::chrono::high_resolution_clock::now();
        
        // 发起RPC调用
        stub.Login(&controller, &request, &response, nullptr);
        
        // 记录结束时间
        auto end = std::chrono::high_resolution_clock::now();
        auto latency = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();
        metrics.total_latency += latency;

        // 统计结果
        if (!controller.Failed() && response.result().errcode() == 0) {
            metrics.success_count++;
        } else {
            metrics.fail_count++;
            std::cerr << "Thread " << thread_id << " RPC failed: " 
                      << controller.ErrorText() << std::endl;
        }
    } catch (const std::exception& e) {
        metrics.fail_count++;
        std::cerr << "Thread " << thread_id << " exception: " << e.what() << std::endl;
    }
}

int main(int argc, char **argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " -i config.conf [thread_num] [requests_per_thread]\n";
        return 1;
    }

    // 初始化RPC框架
    MprpcApplication::Init(argc, argv);


    std::cout << "RPC framework initialized successfully\n";

    // 压测参数
    int thread_num = 6;
    int requests_per_thread = 10;  // 减少初始请求数，便于测试
    
    if (argc > 3) {
        thread_num = std::atoi(argv[2]);
        requests_per_thread = std::atoi(argv[3]);
    }

    std::cout << "Starting pressure test with " << thread_num << " threads, " 
              << requests_per_thread << " requests per thread\n";

    Metrics metrics;
    std::vector<std::thread> threads;
    auto start_time = std::chrono::high_resolution_clock::now();

    try {
        threads.reserve(thread_num);
        for (int i = 0; i < thread_num; i++) {
            threads.emplace_back([&metrics, requests_per_thread, i]() {
                for (int j = 0; j < requests_per_thread; j++) {
                    send_request(metrics, i);
                    std::this_thread::sleep_for(std::chrono::milliseconds(10));
                }
            });
            std::cout << "Thread " << i << " started\n";
        }

        // 等待所有线程完成
        for (auto& t : threads) {
            t.join();
        }

        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
            end_time - start_time);
        
        // 输出压测报告
        std::cout << "\n=== Pressure Test Report ===\n";
        std::cout << std::fixed << std::setprecision(2);
        std::cout << "Duration: " << duration.count() << "ms\n";
        std::cout << "Total Requests: " << thread_num * requests_per_thread << "\n";
        std::cout << "Success: " << metrics.success_count << "\n";
        std::cout << "Failed: " << metrics.fail_count << "\n";
        
        if (metrics.success_count > 0) {
            double qps = (metrics.success_count * 1000.0) / duration.count();
            double avg_latency = metrics.total_latency / (double)metrics.success_count;
            
            std::cout << "QPS: " << qps << "\n";
            std::cout << "Average Latency: " << avg_latency << "ms\n";
        }
        std::cout << "=========================\n";

    } catch (const std::exception& e) {
        std::cerr << "Main thread exception: " << e.what() << std::endl;
        return 1;
    }

    return 0;
}

// #include <iostream>
// #include "MprpcApplication.h"
// #include "user.pb.h"
// #include "MprpcChannel.h"

// #ifdef JSON
// #include <google/protobuf/util/json_util.h>
// #include <google/protobuf/message.h>
// #endif

// int main(int argc, char **argv)
// {
//     // 整个程序启动以后，想使用mprpc框架来享受rpc服务调用，一定需要先调用框架的初始化函数（只初始化一次）
//     MprpcApplication::Init(argc, argv);

//     // 演示调用远程发布的rpc方法Login
//     fixbug::UserServiceRpc_Stub stub(new MprpcChannel());   //需要传入一个RpcChannel对象，最终他调用的所有rpc方法，底层都是channel的CallMethod方法
    
//     // rpc方法的请求参数
//     fixbug::LoginRequest request;
//     request.set_name("zhang san");
//     request.set_pwd("123456");

//     // rpc方法的响应
//     fixbug::LoginResponse response;

//     // 发起rpc方法的调用  同步的rpc调用过程  MprpcChannel::callmethod
//     stub.Login(nullptr, &request, &response, nullptr); // RpcChannel->RpcChannel::callMethod 集中来做所有rpc方法调用的参数序列化和网络发送

//     // 处理响应结果
// #ifdef JSON
//     std::string json_response;
//     google::protobuf::util::MessageToJsonString(response, &json_response);
//     std::cout << "JSON Response: " << json_response << std::endl;
// #else
//     if (0 == response.result().errcode()) {
//         std::cout << "rpc login response success:" << response.sucess() << std::endl;
//     } else {
//         std::cout << "rpc login response error : " << response.result().errmsg() << std::endl;
//     }
// #endif
    
// /*   
//     // 一次rpc调用完成，读调用的结果
//     if (0 == response.result().errcode())
//     {
//         std::cout << "rpc login response success:" << response.sucess() << std::endl;
//     }
//     else
//     {
//         std::cout << "rpc login response error : " << response.result().errmsg() << std::endl;
//     }
// */ 

//     // 演示调用远程发布的rpc方法Register
//     fixbug::RegisterRequest req;
//     req.set_id(2000);
//     req.set_name("mprpc");
//     req.set_pwd("666666");
//     fixbug::RegisterResponse rsp;

//     // 以同步的方式发起rpc调用请求，等待返回结果
//     stub.Register(nullptr, &req, &rsp, nullptr); 

// // 处理响应结果
// #ifdef JSON
//     std::string json_rsp;
//     google::protobuf::util::MessageToJsonString(rsp, &json_rsp);
//     std::cout << "JSON Response: " << json_rsp << std::endl;
// #else
//     if (0 == rsp.result().errcode()) {
//         std::cout << "rpc register response success:" << rsp.sucess() << std::endl;
//     } else {
//         std::cout << "rpc register response error : " << rsp.result().errmsg() << std::endl;
//     }
// #endif

// /*
//     // 一次rpc调用完成，读调用的结果
//     if (0 == rsp.result().errcode())
//     {
//         std::cout << "rpc register response success:" << rsp.sucess() << std::endl;
//     }
//     else
//     {
//         std::cout << "rpc register response error : " << rsp.result().errmsg() << std::endl;
//     }
// */    
//     return 0;
// }
