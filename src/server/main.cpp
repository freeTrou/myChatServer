#include "chatserver.h"
#include <iostream>
#include <signal.h>
using namespace std;

//处理服务器ctrl+C结束后，重置user表的状态
void resetHandler(int)
{
    ChatService::instance()->reset();
    exit(0);
}

//启动时传入ip和端口号，不写死
int main(int argc, char** argv)
{
    if (argc < 3)
    {
        cerr << "command invalid! example: ./ChatServer 127.0.0.1 6000" << endl;
        exit(-1);
    }

    // 解析通过命令行参数传递的ip和port
    char *ip = argv[1];
    uint16_t port = atoi(argv[2]);

    signal(SIGINT, resetHandler);

    EventLoop loop;
    InetAddress addr(ip, port); 
    ChatServer server(&loop, addr, "chatserver");

    server.start();
    loop.loop();

    return 0;
}