#ifndef USER_H
#define USER_H

#include <string>
using namespace std;

//匹配user表的ORM类
class User
{
public:
    User(int id = -1, string name = "", string pwd = "", string state = "offline") : 
    id(id),name(name),
    password(pwd),state(state)
    {}
    ~User()
    {}

    //设置成员变量
    void setId(int id){ this->id = id; }
    void setName(string name){ this->name = name; }
    void setPwd(string pwd){ this->password = pwd; }
    void setState(string state){ this->state = state; }

     //获取成员变量
    int getId(){ return this->id; }
    string getName(){ return this->name; }
    string getPwd(){ return this->password; }
    string getState(){ return this->state; }

private:
   int id;
   string name;
   string password;
   string state; // 状态
};




#endif