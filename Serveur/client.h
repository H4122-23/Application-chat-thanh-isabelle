#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"
enum COMMANDS{UNKNOWN,DIRECT_MESSAGE,CREATE_GROUP_CHAT,GROUP_CHAT};
typedef struct
{
   SOCKET sock;
   char name[BUF_SIZE];
}Client;
typedef struct
{
   char name[BUF_SIZE];
   int size;
   Client members[MAX_NAME];
}Groupchat;


#endif /* guard */