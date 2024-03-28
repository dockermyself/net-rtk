//
// Created by dockermyself on 2022/4/28.
//

#ifndef MSG_CORS_H
#define MSG_CORS_H

enum CORS_MSG_STATUS{
    CORS_EMPTY = 0x00,
    CORS_FULL = 0x01,

};
#pragma pack(1)
struct CORS_MSG{
    char* buffer;
    int buffer_size;
    int data_size;
    int status;
};

#pragma pack()
void msg_cors_init();
void msg_cors_update(const char *msg,int size);
int msg_cors_get(char *msg);
void msg_cors_destroy();






#endif //MSG_CORS_H


