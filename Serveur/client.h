#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"
enum COMMANDS{UNKNOWN,DIRECT_MESSAGE,ONLINE_USERS,CHANGE_USERNAME};
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