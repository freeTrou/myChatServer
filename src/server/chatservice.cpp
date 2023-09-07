#include "chatservice.h"
#include "public.h"


//返回单例模式唯一的实例
ChatService* ChatService::instance()
{
    static ChatService service;

    return &service;
}

ChatService::ChatService()
{
    //将消息id和业务方法做绑定
    _msgHandlerMap.insert({LOGIN_MSG, std::bind(&ChatService::login,this,_1,_2,_3)});//使用绑定器将函数对象
    _msgHandlerMap.insert({REG_MSG, std::bind(&ChatService::reg,this,_1,_2,_3)});
    _msgHandlerMap.insert({ONE_CHAT_MSG, std::bind(&ChatService::oneChat,this,_1,_2,_3)});
    _msgHandlerMap.insert({ADD_FRIEND_MSG,std::bind(&ChatService::addFriend,this,_1,_2,_3)});
    _msgHandlerMap.insert({LOGINOUT_MSG,std::bind(&ChatService::loginout,this,_1,_2,_3)});

     // 群组业务管理相关事件处理回调注册
    _msgHandlerMap.insert({CREATE_GROUP_MSG, std::bind(&ChatService::createGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({ADD_GROUP_MSG, std::bind(&ChatService::addGroup, this, _1, _2, _3)});
    _msgHandlerMap.insert({GROUP_CHAT_MSG, std::bind(&ChatService::groupChat, this, _1, _2, _3)});

     // 连接redis服务器
    if (_redis.connect())
    {
        // 设置上报消息的回调
        _redis.init_notify_handler(std::bind(&ChatService::handleRedisSubscribeMessage, this, _1, _2));
    }
}

 //服务器异常的重置业务
void ChatService::reset()
{
    //把所有用户的状态设置为离线
    _userModel.resetState();
}

//获取消息对应的处理器
MsgHandler ChatService::getHandler(int msgid)
{
    //当在map中未查到对应 错误记录 日志输出
    auto it = _msgHandlerMap.find(msgid);
    if(it == _msgHandlerMap.end())//未查到
    {
        //返回一个默认的业务处理（空操作）
        return [=](const TcpConnectionPtr& conn, json& js,Timestamp time)
        {
            LOG_ERROR << "msgid = " << msgid << " can not find handler!!!";
        };
    }
    else
    {
        //返回msgid对应的业务处理方法
        return _msgHandlerMap[msgid]; 
    }
}

// 处理登录业务
void ChatService::login(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int id = js["id"];
    string pwd = js["password"];

    User user = _userModel.query(id);//按照id去数据库里索引
    if(user.getId() == id || user.getPwd() == pwd)//(数据库查出来返回的id 和 pwd都相同)
    {
        //数据库查出来字段又分为两种情况(1、未登录，现在登录，改变状态 2、已经登录了，不可以重复登录)
        if(user.getState() == "online")
        {
            //该用户已经登录，不可以重复登录
            // 打包数据
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 2; // 错误：账号重复登录错误
            response["errmsg"] = "该账号已经登录，请勿重复登录";

            // 发送
            conn->send(response.dump()); // 转为字符串发送
        }
        else
        {
            //登录成功，记录用户连接信息,通过用户id关联连接信息
            {
                lock_guard<mutex> lock(_connMutex);//会自动加锁解锁(在当前大括号内)
                _userConnMap.insert({id,conn});//可能会被不同的工作线程操作，需要考虑多线程问题
            }

            // id用户登录成功后，向redis订阅channel(id)
            _redis.subscribe(id); 
            
            //登录成功，更新数据库状态字段
            user.setState("online");
            _userModel.updateState(user);

            // 打包数据
            json response;
            response["msgid"] = LOGIN_MSG_ACK;
            response["errno"] = 0; // 表示成功
            response["id"] = user.getId();
            response["name"] = user.getName();
            //查询用户是否有离线消息，有的话带回
            vector<string> vec = _offlineMsgModel.query(id);
            if(!vec.empty())//不为空
            {
                response["offlinemsg"] = vec;
                //读取离线消息后，删除离线消息
                _offlineMsgModel.remove(id);
            }

             // 查询该用户的好友信息并返回
            vector<User> userVec = _friendModel.query(id);
            if (!userVec.empty())
            {
                vector<string> vec2;
                for (User &user : userVec)
                {
                    json js;
                    js["id"] = user.getId();
                    js["name"] = user.getName();
                    js["state"] = user.getState();
                    vec2.push_back(js.dump());
                }
                response["friends"] = vec2;
            }

             // 查询用户的群组信息
            vector<Group> groupuserVec = _groupModel.queryGroups(id);
            if (!groupuserVec.empty())
            {
                // group:[{groupid:[xxx, xxx, xxx, xxx]}]
                vector<string> groupV;
                for (Group &group : groupuserVec)
                {
                    json grpjson;
                    grpjson["id"] = group.getId();
                    grpjson["groupname"] = group.getName();
                    grpjson["groupdesc"] = group.getDesc();
                    vector<string> userV;
                    for (GroupUser &user : group.getUsers())
                    {
                        json js;
                        js["id"] = user.getId();
                        js["name"] = user.getName();
                        js["state"] = user.getState();
                        js["role"] = user.getRole();
                        userV.push_back(js.dump());
                    }
                    grpjson["users"] = userV;
                    groupV.push_back(grpjson.dump());
                }

                response["groups"] = groupV;
            }

            // 发送
            conn->send(response.dump()); // 转为字符串发送
        }
        
    }
    else//登录失败(1、用户不存在 2、密码错误)
    {
        // 打包数据
        json response;
        response["msgid"] = LOGIN_MSG_ACK;
        response["errno"] = 1; // 表示失败
        response["errmsg"] = "用户名或者密码错误";

        // 发送
        conn->send(response.dump()); // 转为字符串发送
    }
}

// 处理注册业务
void ChatService::reg(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    //解析客户端发来的js字段拿到数据
    string name = js["name"];
    string pwd = js["password"];

    User user;
    user.setName(name);
    user.setPwd(pwd);
    bool state = _userModel.insert(user);//增加字段
    if(state)
    {
        //注册成功
        //给客户端发送数据 ==>成功

        //打包数据
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 0;//表示成功
        response["id"] = user.getId();

        //发送
        conn->send(response.dump());//转为字符串发送
    }
    else
    {
        //注册失败
        //打包数据
        json response;
        response["msgid"] = REG_MSG_ACK;
        response["errno"] = 1;//表示失败

        //发送
        conn->send(response.dump());//转为字符串发送
    }
}

//处理客户端异常退出
void ChatService::clientCloseException(const TcpConnectionPtr &conn)
{
    User user;
    {
        lock_guard<mutex> lock(_connMutex);
        // 在map表中查找这个链接
        for (auto it = _userConnMap.begin(); it != _userConnMap.end(); ++it)
        {
           
            if (it->second == conn) // 找到了
            {
                //记录用户id
                user.setId(it->first);
                //将这个连接在map表中删除
                _userConnMap.erase(it);

                break;
            }
        }
    }
    
    // 用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(user.getId()); 

    //2、更新数据库该用户的状态为离线
    if(user.getId() != -1)//判断用户id是否有效
    {
        user.setState("offline");
        _userModel.updateState(user);
    }
    
}

//处理一对一聊天业务
void ChatService::oneChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int toid = js["to"].get<int>();

    //查找用户是否有对应连接(表示在线)
    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(toid);
        if(it != _userConnMap.end())
        {
            //用户在线，做消息转发
            it->second->send(js.dump());

            return;
        }
    }

    //查询toid是否在线
    User user = _userModel.query(toid);
    if(user.getState() == "online")
    {
        _redis.publish(toid, js.dump());//集群环境用Redis订阅
        return;
    }

    //用户不在线，做离线存储
    _offlineMsgModel.insert(toid,js.dump());
}

// 处理添加好友业务 msgid id friendid
void ChatService::addFriend(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int friendid = js["friendid"].get<int>();

     // 存储好友信息
    _friendModel.insert(userid, friendid);
    //反向也加一条记录，这样好友列表双方都可以显示好友
    _friendModel.insert(friendid, userid);
}

// 创建群组业务
void ChatService::createGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    string name = js["groupname"];
    string desc = js["groupdesc"];

    // 存储新创建的群组信息
    Group group(-1, name, desc);
    if (_groupModel.createGroup(group))
    {
        // 存储群组创建人信息
        _groupModel.addGroup(userid, group.getId(), "creator");
    }
}

// 加入群组业务
void ChatService::addGroup(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    _groupModel.addGroup(userid, groupid, "normal");
}

// 群组聊天业务
void ChatService::groupChat(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
    int userid = js["id"].get<int>();
    int groupid = js["groupid"].get<int>();
    vector<int> useridVec = _groupModel.queryGroupUsers(userid, groupid);

    lock_guard<mutex> lock(_connMutex);
    for (int id : useridVec)
    {
        auto it = _userConnMap.find(id);
        if (it != _userConnMap.end())
        {
            // 转发群消息
            it->second->send(js.dump());
        }
        else
        {
            // 查询toid是否在线 
            User user = _userModel.query(id);
            if (user.getState() == "online")
            {
                _redis.publish(id, js.dump());
            }
            else
            {
                // 存储离线群消息
                _offlineMsgModel.insert(id, js.dump());
            }
        }
    }
}

// 处理注销业务
void ChatService::loginout(const TcpConnectionPtr &conn, json &js, Timestamp time)
{
     int userid = js["id"].get<int>();

    {
        lock_guard<mutex> lock(_connMutex);
        auto it = _userConnMap.find(userid);
        if (it != _userConnMap.end())
        {
            _userConnMap.erase(it);
        }
    }

    // 用户注销，相当于就是下线，在redis中取消订阅通道
    _redis.unsubscribe(userid); 
   
    // 更新用户的状态信息
    User user(userid, "", "", "offline");
    _userModel.updateState(user);
}

// 从redis消息队列中获取订阅的消息
void ChatService::handleRedisSubscribeMessage(int userid, string msg)
{
    lock_guard<mutex> lock(_connMutex);
    auto it = _userConnMap.find(userid);
    if (it != _userConnMap.end())
    {
        it->second->send(msg);
        return;
    }

    // 存储该用户的离线消息
    _offlineMsgModel.insert(userid, msg);
}