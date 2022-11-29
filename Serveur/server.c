#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h> 
#include "server.h"
#include "client.h"

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
               printf("Another checkpoint segfaut");
               Client client = clients[i];
               int c = read_client(clients[i].sock,buffer);
               //printf("Message from %s : %s \n", client.name,buffer);
               printf("Checkpoint segfaut");
               /* client disconnected */
               if(c == 0)
               {
                  printf("Checkpoint0");
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
                     /* Get destination client name*/
                     char name[BUF_SIZE];
                     int i = 3;
                     while (buffer[i] != ' ' && i < BUF_SIZE)
                     {
                        name[i - 3] = buffer[i];
                        i++;
                     }
                     name[i - 3] = '\0';

                     /* Find client from name */
                     Client *dest = NULL;
                     for (i = 0; i < actual; i++)
                     {
                        if (strcmp(clients[i].name, name) == 0)
                        {
                           dest = &clients[i];
                        }
                     }

                     if (dest == NULL)
                     {
                        write_client(client.sock, "Destination user not found");
                     }
                     else
                     {
                        /* Create a new buffer without the command and the name */
                        memmove(buffer, buffer + 4 + strlen(name), strlen(buffer));

                        /* Send the private message */
                        printf("Message sent\n");
                     }
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

static enum COMMANDS get_command(const char* buffer){
   printf(buffer);
   if (buffer[0]=='/'){
      if (buffer[1]=='m')return DIRECT_MESSAGE;
      else if (buffer[1]=='g')return GROUP_CHAT;
      else return UNKNOWN;

   }
}

static void send_message_to_specified_client(Client* clients, const char* buffer){
   /*copy buffer to use strtok*/
   char* delimiter =" ";
   char* copy_buffer;
   strncpy(copy_buffer,buffer,BUF_SIZE-1);
   /*extract recipient's name and message*/
   char* recipient_pseudo = strtok(copy_buffer,delimiter);
   recipient_pseudo= strtok(NULL,delimiter);
   char* message = strtok(NULL,delimiter);
   printf("pseudo: %s, message: %s",recipient_pseudo,message);
   int messageSent = 0;
   /*search recipient in the array from his/her name*/
   while (clients!=NULL&&!messageSent)
   {
      if (strcmp(clients->name, recipient_pseudo) == 0)
      {
         write_client(clients->sock,message);
         messageSent=1;
      }
      clients++;
   }
}



int main(int argc, char **argv)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
