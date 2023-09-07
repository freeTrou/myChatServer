#ifndef CHATSERVER_H
#define CHATSERVER_H

#include <iostream>
#include <muduo/net/TcpServer.h>
#include <muduo/net/EventLoop.h>
#include <string>
#include <json.hpp>
#include "chatservice.h"
using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;


// 聊天服务器类
class ChatServer
{
public: 
    //初始化聊天服务器对象
    ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg);
    ~ChatServer();// 析构

    //启动服务
    void start();

private:
    // 上报连接和断开相关信息的回调函数
    void onConnection(const TcpConnectionPtr&);
    // 上报读写事件相关的回调函数
    void onMessage(const TcpConnectionPtr&, Buffer*, Timestamp);

    TcpServer _server; // 实现服务器功能的类对象
    EventLoop* _loop; //指向事件循环对象的指针
};

#endif