#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h> 
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
                     
                     char *gc_name = get_group_name(buffer);
                     printf("%s\n", buffer);
                     printf("Group name: %s\n", gc_name);
                     //printf("Size of g name is %ld", sizeof(gc_name));
                     char** members = get_group_members(buffer);
                    // printf("out of function\n");
                     printf("member 1 : %s", members[0]);
                     /*int m=0;
                     for(int m =0; m<num_names;m++){
                        int index_recipient = search_recipient1(names[m],clients,actual);
                        if (index_recipient==-1)
                        {
                           write_client(client.sock, "Destination user not found\n");
                        }
                        else{

                           Client* dest = &clients[index_recipient];
                          // memmove(buffer, buffer + 4 + strlen(dest->name), strlen(buffer));
                           send_message_to_specified_client(*dest,client,buffer);
                        }
                        
                     }*/
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

static char** get_group_members(char* buffer){
   
   int i = 3;
   char** names;
   char* temp;

   while (buffer[i] != ' ' && i < BUF_SIZE)
   {
     // name[i - 3] = buffer[i];
      i++;
   }
   printf("now we are at index of the space after group name in array\n");
   
   i++;
   int j=0; //index of temp 
   int k=0; //index of names
   printf("buffer is : %s\n", buffer);
   while(buffer[i] != '-' && k < 10){
      //printf("%c\n", buffer[i]);
     // printf("in while loop at index %d\n", i);

      if(buffer[i]==' '){
         //next name
         printf("new name\n");
         printf("%s\n", temp);
         
        // printf("%s\n", names[k]);
         names[k] = (char*)malloc(j * sizeof(char*));
         names[k]= temp;
         printf("names[k] is : %s\n", names[k]);
         
         k++;
         j=0;

      }
      else{
         //printf("adding character to temp\n");
         temp[j]=buffer[i];
         j++;
      }
      i++;
   }
   printf("out of loop\n");
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
/*
static char** gc_names(char* buffer){
   char * token = strtok(buffer, " ");
   char **names;
   char **names_final;
   int i=0; int j=0;
   while( token != NULL ) {
      //if(token[0]!='-'){
         names[j] = token;
         printf("%s\n", names[j]);
         token = strtok(NULL, " ");
         j++;
      //}
      i++;
   }
   num_names = j-1;
   int k;
   
   printf("%d is number of names in gc\n", num_names);
   for(k = 0; k < j; k++){
      char* temp = names[k+1];
      //if(temp[0] == "-"){
      //   break;
      //}
      names[k]=names[k+1];
      printf("%s, %d\n", names[k],k);
   }
   names[k+1]="\0";
   return names;
}*/

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
