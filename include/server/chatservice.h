#ifndef CHATSERVICE_H
#define CHATSERVICE_H

#include <iostream>
#include <muduo/net/TcpConnection.h>
#include <muduo/base/Logging.h>
#include <unordered_map>
#include <functional>
#include <mutex>
#include <vector>

#include "usermodel.hpp"
#include "offlinemessagemodel.hpp"
#include "friendmodel.hpp"
#include "groupmodel.hpp"
#include "redis.hpp"
#include "json.hpp"
using namespace std;
using namespace muduo;
using namespace muduo::net;
using json = nlohmann::json;

//定义函数对象的别名  参数：服务器连接的实例相当于一个封装的fd，已经反序列化的json数据 
using MsgHandler = std::function<void(const TcpConnectionPtr& conn, json& js,Timestamp time)>;//<返回值(参数)>


//处理服务器的业务类 ==》给msg_id映射事件回调
class ChatService //采用单例模式
{

public:
    //返回单例模式唯一的实例
    static ChatService* instance();
    //处理登录业务
    void login(const TcpConnectionPtr& conn, json& js,Timestamp time);
    //处理注册业务
    void reg(const TcpConnectionPtr& conn, json& js,Timestamp time);
    //处理一对一聊天业务
    void oneChat(const TcpConnectionPtr& conn, json& js,Timestamp time);
    //处理添加好友业务
    void addFriend(const TcpConnectionPtr& conn, json& js,Timestamp time);
    // 创建群组业务
    void createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 加入群组业务
    void addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 群组聊天业务
    void groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time);
    // 处理注销业务
    void loginout(const TcpConnectionPtr &conn, json &js, Timestamp time);

    //处理客户端异常退出
    void clientCloseException(const TcpConnectionPtr& conn);
    //服务器异常的重置业务
    void reset();
    //获取消息对应的处理器
    MsgHandler getHandler(int msgid);

    //从redis消息队列中获取订阅的消息
    void handleRedisSubscribeMessage(int, string);
    

private:
    ChatService();//将构造函数私有化

    UserModel _userModel;//user表操作
    OfflineMsgModel _offlineMsgModel;//离线消息表操作
    FriendModel _friendModel;//好友表操作
    GroupModel _groupModel;

    unordered_map<int,MsgHandler> _msgHandlerMap;//存储消息id对应的业务处理方法
    mutex _connMutex;//定义互斥锁，保证_userConnMap的线程安全
    unordered_map<int,TcpConnectionPtr> _userConnMap;//存储在线用户的通信连接

    // redis操作对象
    Redis _redis;
};


#endif