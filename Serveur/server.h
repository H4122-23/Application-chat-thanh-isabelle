#ifndef SERVER_H
#define SERVER_H

#ifdef WIN32

#include <winsock2.h>

#elif defined (linux)



#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h> /* close */
#include <netdb.h> /* gethostbyname */
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define closesocket(s) close(s)
typedef int SOCKET;
typedef struct sockaddr_in SOCKADDR_IN;
typedef struct sockaddr SOCKADDR;
typedef struct in_addr IN_ADDR;

#else

#error not defined for this platform

#endif

#define CRLF        "\r\n"
#define PORT         1977
#define MAX_CLIENTS     100
#define MAX_FILENAME 30
#define MAX_MESSAGES 1024
#define BUF_SIZE    1024
#define MAX_NAME 10
#define MAX_NOTIFICATION 50

#include "client.h"
#include <time.h>

typedef struct
{
   char content[BUF_SIZE];
   char sender[MAX_FILENAME];
   char recipient[MAX_FILENAME];
   struct tm* timestamp;
} Message;

typedef struct
{
   char name[BUF_SIZE];
   int size;
   Client members[MAX_NAME];
}Groupchat;

static void init(void);
static void end(void);
static void app(void);
static int init_connection(void);
static void end_connection(int sock);
static int read_client(SOCKET sock, char *buffer);
static void write_client(SOCKET sock, const char *buffer);
static void remove_client(Client *clients, int to_remove, int *actual);
static void clear_clients(Client *clients, int actual);
static enum COMMANDS get_command(const char* buffer);
static char* get_name(const char* buffer);
static Message* create_message(const char* buffer, const char* sender, const char* recipient, int* nbCurrentMessage,Message* messages);
static Groupchat* create_groupchat( char* members, Client creator, int actual,Client*clients,const char* name);
static void send_message_to_all_clients(Client *clients, Client client, int actual, const char *buffer, char from_server);
static int search_user_by_name(const char* buffer,Client * clients, int actual);
static void send_message_to_specified_client(Client recipient,Client sender, const char* buffer);
static void send_confirmation_message(Groupchat* gc);
static void send_message_to_groupchat(Groupchat* groupchat, Client sender, Client *clients, char *buffer);
static void save_history(Message* message);
static void save_history_groupchat(Message* message,Groupchat* gc);
static void load_history(Client client);
static void add_member(Groupchat* gc, Client member);
static void remove_member(Groupchat* gc, Client member);

#endif /* guard */
