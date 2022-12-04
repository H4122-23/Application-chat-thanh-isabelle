#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"
enum COMMANDS{UNKNOWN,DIRECT_MESSAGE,ONLINE_USERS,CREATE_GROUP_CHAT,GROUP_CHAT, QUIT_GROUP_CHAT,ADD_TO_GROUP_CHAT,REMOVE_FROM_GROUP_CHAT,CHANGE_USERNAME};
typedef struct
{
   SOCKET sock;
   char name[BUF_SIZE];
}Client;


#endif /* guard */