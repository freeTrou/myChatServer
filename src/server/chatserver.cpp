#include "chatserver.h"
#include <functional>
using namespace placeholders;

//初始化聊天服务器对象 构造
ChatServer::ChatServer(EventLoop* loop,
            const InetAddress& listenAddr,
            const string& nameArg):
            _server(loop,listenAddr,nameArg),_loop(loop)
{
    // 注册连接回调
    _server.setConnectionCallback(std::bind(&ChatServer::onConnection,this,_1));

    // 注册消息回调
    _server.setMessageCallback(std::bind(&ChatServer::onMessage,this,_1,_2,_3));

    //设置线程 1个i/o线程 3个工作线程
    _server.setThreadNum(4);
}

// 析构
ChatServer::~ChatServer()
{
    delete this->_loop;
}

//启动服务
void ChatServer::start()
{
    this->_server.start();
}

// 上报连接和断开相关信息的回调函数
void ChatServer::onConnection(const TcpConnectionPtr& conn)
{
    // 客户端断开连接
    if(!conn->connected())
    {
        //处理客户端异常关闭
        ChatService::instance()->clientCloseException(conn);
        conn->shutdown(); // 释放fd资源
    }
    else//客户端连接
    {

    }

}

// 上报读写事件相关的回调函数
void ChatServer::onMessage(const TcpConnectionPtr& conn, Buffer* buffer, Timestamp time)
{
    //拿到缓冲区的数据
    string buf = buffer->retrieveAllAsString();//将缓冲区的数据转成string返回

    //数据的反序列化 字符串转为json对象
    json js = json::parse(buf);

    //通过js[msgid]获取 handler业务处理方法
    auto msgHandler = ChatService::instance()->getHandler(js["msgid"].get<int>());//利用get方法转换为需要的类型
    msgHandler(conn, js, time);//调用方法
}
