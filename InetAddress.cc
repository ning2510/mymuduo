#include "InetAddress.h"
#include <strings.h>
#include <string.h>

InetAddress::InetAddress(uint16_t port, std::string ip) {
    bzero(&addr_, sizeof(addr_));
    // 存储到 addr_ 中的都是网络字节序
    // 取出来时，需要把网络字节序转化为本地字节序
    addr_.sin_family = AF_INET;
    addr_.sin_port = htons(port);
    addr_.sin_addr.s_addr = inet_addr(ip.c_str());  // ip 从字符串转成网络字节序
}

std::string InetAddress::toIp() const {
    // ip
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    return buf;
}

std::string InetAddress::toIpPort() const {
    // ip:port
    char buf[64] = {0};
    ::inet_ntop(AF_INET, &addr_.sin_addr, buf, sizeof(buf));
    size_t end = strlen(buf);
    uint16_t port = ntohs(addr_.sin_port);
    sprintf(buf + end, ":%u", port);
    return buf;
}

uint16_t InetAddress::toPort() const {
    return ntohs(addr_.sin_port);
}

// #include <iostream>

// int main() {
//     InetAddress addr(8080);
//     std::cout << addr.toIpPort() << std::endl;
//     std::cout << addr.toPort() << std::endl;
//     std::cout << addr.toIp() << std::endl;

//     return 0;
// }