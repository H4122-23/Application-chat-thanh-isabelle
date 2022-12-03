#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"
enum COMMANDS{UNKNOWN,DIRECT_MESSAGE,GROUP_CHAT};
typedef struct
{
   SOCKET sock;
   char name[BUF_SIZE];
}Client;
typedef struct
{
   char* name;
   int size;
   Client members[MAX_NAME];
}Groupchat;


#endif /* guard */