#pragma once 
#define PROTOCOLID 1234567

#pragma pack(push, 1)
struct Header {
    Header();
    unsigned int proto;
    unsigned int id;
    unsigned long long ack;
    char rttData[16];
    unsigned int bodyLength;
};
#pragma pack(pop)