#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h> 
#include <stdbool.h>
#include "server.h"
#include "client.h"
int num_names =0;
static void init(void)
{
#ifdef WIN32
   WSADATA wsa;
   int err = WSAStartup(MAKEWORD(2, 2), &wsa);
   if(err < 0)
   {
      puts("WSAStartup failed !");
      exit(EXIT_FAILURE);
   }
#endif
}

static void end(void)
{
#ifdef WIN32
   WSACleanup();
#endif
}

static void app(void)
{
   SOCKET sock = init_connection();
   char buffer[BUF_SIZE];
   /* the index for the array */
   int actual = 0;
   int max = sock;
   /* an array for all clients */
   Client clients[MAX_CLIENTS];

   fd_set rdfs;

   while(1)
   {
      int i = 0;
      FD_ZERO(&rdfs);

      /* add STDIN_FILENO */
      FD_SET(STDIN_FILENO, &rdfs);

      /* add the connection socket */
      FD_SET(sock, &rdfs);

      /* add socket of each client */
      for(i = 0; i < actual; i++)
      {
         FD_SET(clients[i].sock, &rdfs);
      }

      if(select(max + 1, &rdfs, NULL, NULL, NULL) == -1)
      {
         perror("select()");
         exit(errno);
      }

      /* something from standard input : i.e keyboard */
      if(FD_ISSET(STDIN_FILENO, &rdfs))
      {
         /* stop process when type on keyboard */
         break;
      }
      else if(FD_ISSET(sock, &rdfs))
      {
         /* new client */
         SOCKADDR_IN csin = { 0 };
         size_t sinsize = sizeof csin;
         int csock = accept(sock, (SOCKADDR *)&csin, &sinsize);
         if(csock == SOCKET_ERROR)
         {
            perror("accept()");
            continue;
         }

         /* after connecting the client sends its name */
         if(read_client(csock, buffer) == -1)
         {
            /* disconnected */
            continue;
         }

         /* what is the new maximum fd ? */
         max = csock > max ? csock : max;

         FD_SET(csock, &rdfs);
         int duplicated = 0;
         Client c = { csock };
         strncpy(c.name, buffer, BUF_SIZE - 1);
         /*Check if the client exists*/
         for(int i = 0; i < actual; i++)
         {
            if(strcmp(c.name,clients[i].name)==0){
               write_client(c.sock, "This pseudoname has been taken. Please choose a new one! ");
               duplicated = 1;
               break;
            }
         }
         if (duplicated==0){
           
            clients[actual] = c;
            actual++;
         }else
         {
            closesocket(c.sock);
         }
      }
      else
      {
         int i=0;
         for(i = 0; i < actual; i++)
         {
            /* a client is talking */
            if(FD_ISSET(clients[i].sock, &rdfs))
            {
               Client client = clients[i];
               int c = read_client(clients[i].sock,buffer);
               //printf("Message from %s : %s \n", client.name,buffer);
               /* client disconnected */
               if(c == 0)
               {
                  closesocket(clients[i].sock);
                  remove_client(clients, i, &actual);
                  strncpy(buffer, client.name, BUF_SIZE - 1);
                  printf("%s left the server ! \n", client.name);
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);
                  send_message_to_all_clients(clients, client, actual, buffer, 1);
               }else{
                  /*Get command entered by clients*/
                  enum COMMANDS c = get_command(buffer);
                  switch (c)
                  {
                  case DIRECT_MESSAGE:;
                     /* Find client from recipient's name entered by users*/
                     int index_recipient = search_recipient(buffer,clients,actual);
                     if (index_recipient==-1)
                     {
                        write_client(client.sock, "Destination user not found");
                     }
                     else
                     {
                        /* Remove command and name from the buffer */
                        Client* dest = &clients[index_recipient];
                        memmove(buffer, buffer + 4 + strlen(dest->name), strlen(buffer));
                         /* Send the private message */
                        send_message_to_specified_client(*dest,client,buffer);
                        printf("Message sent\n");
                     }
                     break;
                  case GROUP_CHAT:
                     // to make a groupchat input -g <name_of_gc> <grouchat members> -
                     printf("groupchat\n");
                     //char ** names = gc_names(buffer);
                     
                     // get info from input buffer
                     char *name_of_gc = get_group_name(buffer);
                     char namegc[BUF_SIZE];
                     printf("%s\n", buffer);
                     // change type of name of gc to array of characters instead of pointer
                     int j=0;
                     while (*name_of_gc){
                        namegc[j] = *name_of_gc;
                        name_of_gc++;
                        j++;
                     }
                     char* members = get_group_members(buffer);
                     printf("Groupchat members without creator: %s\n", members);
                     // make new groupchat
                     Groupchat* new_gc = create_groupchat(members, client, actual, clients);
                     strcpy(new_gc->name, namegc);
                     // send confirmation message to creator of gc
                     send_confirmation_message(new_gc);
                     printf("done\n");
                     break;
                  case UNKNOWN:
                     write_client(client.sock, "Unknown command");
                  default:
                     break;
                  }

               }break;
            }
         }
      }
   }

   clear_clients(clients, actual);
   end_connection(sock);
}

static void send_confirmation_message(Groupchat* gc){
   char confirmation[] = "You just made a groupchat called ";
   strcat(confirmation, gc->name);
   strcat(confirmation, " with users ");
   for(int i = 0; i < gc->size; i++){
      strcat(confirmation,gc->members[i].name);
      strcat(confirmation, " "); 
   }
   printf(" confirmation message: %s\n", confirmation);
   write_client(gc->members[0].sock, confirmation);
   


   char to_members[] = "Added you to groupchat called ";
   strcat(to_members, gc->name);
   strcat(to_members, " with users ");
   for(int i = 0; i < gc->size; i++){
      strcat(to_members,gc->members[i].name);
      strcat(to_members, " "); 
   }
   printf("message to members: %s\n", to_members);
   //send message to each member of gc saying that they are in the groupchat
   
   for(int i = 1; i < gc->size; i++){
      send_message_to_specified_client(gc->members[i],gc->members[0],to_members);
   }
   return;
}

static Groupchat* create_groupchat(char* members, Client creator, int actual,Client*clients){
   Groupchat* new_group = (Groupchat*)malloc(sizeof(Groupchat));
   char space[] = " ";
   char *ptr = strtok(members, space);
   bool found = false;
   //set group name
   //new_group->name = gc_name;
   new_group->members[0] = creator;
   int mem=1; // index / number of members in groupchat
	while(ptr != NULL)
	{
		printf("'%s'\n", ptr);
      /* Find client from name */
      for(int i = 0; i < actual; i++)
      {
         if(strcmp(ptr,clients[i].name)==0){
            found = true;
            
           // new_group->members[mem] = malloc(sizeof(Client*));
            new_group->members[mem] = clients[i];
            mem++;
            break;
         }
      }
      if (!found){
         printf("Client attempting to make groupchat with user not found\n");
         write_client(creator.sock, "User not found");
         return NULL;
      }
      found = false;
		ptr = strtok(NULL, space);
	}
   new_group->size = mem;
   return new_group;
}



static char* get_group_name(const char* buffer){
   int i = 3;
   char* name;
   while (buffer[i] != ' ' && i < BUF_SIZE)
   {
      name[i - 3] = buffer[i];
      i++;
   }
   return name;
}

//function to get members of gc from input 

static char* get_group_members(char* buffer){
   
   int i = 3;
   char* names;
   
   while (buffer[i] != ' ' && i < BUF_SIZE)
   {
     // name[i - 3] = buffer[i];
      i++;
   }
   
   i++;
   int j=0; //index of names
   while(buffer[i] != '-' && j < 100){
      names[j] = buffer[i];
      j++;
      i++;
   }

   return names;
}

static void clear_clients(Client *clients, int actual)
{
   int i = 0;
   for(i = 0; i < actual; i++)
   {
      closesocket(clients[i].sock);
   }
}

static void remove_client(Client *clients, int to_remove, int *actual)
{
   /* we remove the client in the array */
   memmove(clients + to_remove, clients + to_remove + 1, (*actual - to_remove - 1) * sizeof(Client));
   /* number client - 1 */
   (*actual)--;
}

static void send_message_to_all_clients(Client *clients, Client sender, int actual, const char *buffer, char from_server)
{
   int i = 0;
   char message[BUF_SIZE];
   message[0] = 0;
   for(i = 0; i < actual; i++)
   {
      /* we don't send message to the sender */
      if(sender.sock != clients[i].sock)
      {
         if(from_server == 0)
         {
            strncpy(message, sender.name, BUF_SIZE - 1);
            strncat(message, " : ", sizeof message - strlen(message) - 1);
         }
         strncat(message, buffer, sizeof message - strlen(message) - 1);
         write_client(clients[i].sock, message);
      }
   }
}

static int init_connection(void)
{
   SOCKET sock = socket(AF_INET, SOCK_STREAM, 0);
   SOCKADDR_IN sin = { 0 };

   if(sock == INVALID_SOCKET)
   {
      perror("socket()");
      exit(errno);
   }

   sin.sin_addr.s_addr = htonl(INADDR_ANY);
   sin.sin_port = htons(PORT);
   sin.sin_family = AF_INET;

   if(bind(sock,(SOCKADDR *) &sin, sizeof sin) == SOCKET_ERROR)
   {
      perror("bind()");
      exit(errno);
   }

   if(listen(sock, MAX_CLIENTS) == SOCKET_ERROR)
   {
      perror("listen()");
      exit(errno);
   }

   return sock;
}

static void end_connection(int sock)
{
   closesocket(sock);
}

static int read_client(SOCKET sock, char *buffer)
{
   int n = 0;
   if((n = recv(sock, buffer, BUF_SIZE - 1, 0)) < 0)
   {
      perror("recv()");
      /* if recv error we disconnect the client */
      n = 0;
   }

   buffer[n] = 0;

   return n;
}

static void write_client(SOCKET sock, const char *buffer)
{
   if(send(sock, buffer, strlen(buffer), 0) < 0)
   {
      perror("send()");
      exit(errno);
   }
}

/*Extract commands entered by users*/
static enum COMMANDS get_command(const char* buffer){
   if (buffer[0]=='-'){
      if (buffer[1]=='m')return DIRECT_MESSAGE;
      else if (buffer[1]=='g')return GROUP_CHAT;
      else return UNKNOWN;
   }
   
}

/*Search for recipient in the list of clients*/
static int search_recipient(const char* buffer,Client*clients, int actual){
   char name[BUF_SIZE];
   int i = 3;
   while (buffer[i] != ' ' && i < BUF_SIZE)
   {
      name[i - 3] = buffer[i];
      i++;
   }
   name[i - 3] = '\0';
   /* Find client from name */
   for (i = 0; i < actual; i++)
   {
      if (strcmp(clients[i].name, name) == 0)
      {
         return i;
      }
   }
   return -1;
}
static int search_recipient1(const char* buffer,Client*clients, int actual){
   char name[BUF_SIZE];
   int i = 0;
   while (buffer[i] && i < BUF_SIZE)
   {
      name[i] = buffer[i];
      i++;
   }
   name[i] = '\0';
   /* Find client from name */
   for (i = 0; i < actual; i++)
   {
      if (strcmp(clients[i].name, name) == 0)
      {
         return i;
      }
   }
   return -1;
}

static void send_message_to_specified_client(Client recipient, Client sender, const char *buffer){
   int i = 0;
   char message[BUF_SIZE];
   strcpy(message, "(");
   strcat(message, sender.name);
   strcat(message, ") : ");
   strcat(message, buffer);
   write_client(recipient.sock, message);
}



int main(int argc, char **argv)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
