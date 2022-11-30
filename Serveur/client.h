#ifndef CLIENT_H
#define CLIENT_H

#include "server.h"
enum COMMANDS{UNKNOWN,DIRECT_MESSAGE,GROUP_CHAT,SHOW_HISTORY,SAVE_HISTORY};
typedef struct
{
   SOCKET sock;
   char name[BUF_SIZE];
}Client;

#endif /* guard */