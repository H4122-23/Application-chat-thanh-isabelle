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
   int nbCurrentMessage = 0;
   int max = sock;
   /* an array for all clients */
   Client clients[MAX_CLIENTS];
   Message messages[MAX_MESSAGES];
   Groupchat *groupchats[MAX_CLIENTS];
   int gc_index = 0; // no of groupchats

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
               write_client(c.sock, "This username has been taken. Please choose a new one! ");
               duplicated = 1;
               break;
            }
         }
         if (duplicated==0){
            clients[actual] = c;
            actual++;
            /*Load chat history once the client is connected*/
            load_history(c);
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
                  strncat(buffer, " disconnected !", BUF_SIZE - strlen(buffer) - 1);
                  printf("%s\n", buffer);
                  send_message_to_all_clients(clients, client, actual, buffer, 1);
               }else{
                  /*Get command entered by clients*/
                  enum COMMANDS c = get_command(buffer);
                  switch (c)
                  {
                  case DIRECT_MESSAGE:;
                     /* Find client from recipient's name entered by users*/
                     char* recipient_name= get_name(buffer);
                     int index_recipient = search_user_by_name(recipient_name,clients,actual);
                     /* Remove command and name from the buffer */
                     memmove(buffer, buffer + 4 + strlen(recipient_name), strlen(buffer));
                     /*Save message*/
                     Message* newMessage = create_message(buffer,client.name,recipient_name,&nbCurrentMessage,messages);
                     save_history(newMessage);
                     if (index_recipient>-1){
                        /*If the recipient is online, send the message */
                        Client* dest = &clients[index_recipient];
                         /* Send the private message */
                        send_message_to_specified_client(*dest,client,buffer);
                        printf("Message sent\n");
                     } else{
                        write_client(client.sock, "This person is currently offline.");
                     }
                     break;
                  case ONLINE_USERS:;
                     /*List all online users*/
                     char online[BUF_SIZE] = "Online - ";
                     char nbOnline[2]; 
                     sprintf(nbOnline,"%d",actual);
                     strcat(online,nbOnline);
                     strcat(online, ": ");
                     for(int i=0;i<actual;i++){
                        strcat(online,clients[i].name);
                        strcat(online," | ");
                     }
                     write_client(client.sock,online);
                     break;
                  case CHANGE_USERNAME:;
                     char* new_username= get_name(buffer);
                     char oldFilename[MAX_FILENAME];
                     char newFilename[MAX_FILENAME];
                     strcpy(oldFilename,client.name);
                     strcpy(newFilename,new_username);
                     for(int i=0;i<actual;i++){
                        if(strcmp(clients[i].name,client.name)==0){
                           strcpy(clients[i].name,new_username);
                           break;
                        }
                     }
                     write_client(client.sock, "Your username has been modified.");
                     strcat(oldFilename,".txt");
                     strcat(newFilename,".txt");
                     rename(oldFilename,newFilename);
                     break;
                  case GROUP_CHAT:;
                     /*Send message to group chat*/
                     char* gc_name= get_name(buffer);
                     Groupchat* current_group = (Groupchat*)malloc(sizeof(Groupchat));
                     bool found_gc = false;
                     /* Remove command and group chat from the buffer */
                     memmove(buffer, buffer + 4 + strlen(gc_name), strlen(buffer));
                     for( int i = 0; i < gc_index; i++){
                        /*Search for the groupchat of name gc_name*/
                        if(strcmp(gc_name, groupchats[i]->name)==0){
                           current_group = groupchats[i];
                           int nbMembers = groupchats[i]->size;
                           for(int j =0;j<nbMembers;j++){
                              if(strcmp(groupchats[i]->members[j].name,client.name)==0){
                                 /*If the client is currently in the groupchat, send message*/
                                 found_gc=true;
                                 send_message_to_groupchat(current_group, client, clients, buffer);
                                 /*Save message*/
                                 Message* messageToGroup = create_message(buffer,client.name,gc_name,&nbCurrentMessage,messages);
                                 save_history_groupchat(messageToGroup,current_group);
                                 break;
                              }
                           }
                           break;
                        }
                     }
                     if(!found_gc){
                        write_client(client.sock, "You are not a member of this groupchat.");
                     } 
                     memset(buffer,0,strlen(buffer));
                     break;  
                  case CREATE_GROUP_CHAT:;
                     /*to make a groupchat input -g <name_of_gc> <grouchat members> */
                     char* name_of_gc= get_name(buffer);
                     /* Remove command and group chat from the buffer */
                     memmove(buffer, buffer + 4 + strlen(name_of_gc), strlen(buffer));
                     Groupchat* new_gc = create_groupchat(buffer, client, actual, clients,name_of_gc);
                     if(new_gc!=NULL){
                        groupchats[gc_index] = new_gc;
                        gc_index++;
                        // send confirmation message to creator of gc
                        send_confirmation_message(new_gc);
                        printf("Groupchat created\nGroupchat name: %s\nGroupchat creator: %s\nGroupchat members: %s\n", new_gc->name, new_gc->members[0].name, new_gc->members[0].name);
                        for(int i = 1; i< new_gc->size; i++){
                           printf("\t\t   %s\n", new_gc->members[i].name);
                        }
                     } else{
                        write_client(client.sock,"Unable to create a groupchat with these users.");
                     }
                     memset(buffer,0,strlen(buffer));
                     break;
                  case ADD_TO_GROUP_CHAT:;
                     /*Add an user to a groupchat*/
                     char* name_gc= get_name(buffer);
                     /* Remove command and group chat from the buffer */
                     memmove(buffer, buffer + 4 + strlen(name_gc), strlen(buffer));
                     int id_client = search_user_by_name(buffer,clients,actual);
                     int i,j;
                     bool group_found = false;
                     bool duplicate = false;
                     if(id_client==-1)write_client(client.sock, "Unable to add this user.");
                     else{
                        Client to_be_added = clients[id_client];
                        for( i = 0; i < gc_index; i++){
                           if(strcmp(name_gc, groupchats[i]->name)==0){
                              group_found=true;
                              for( j=0;j<groupchats[i]->size;j++){
                                 if(strcmp(groupchats[i]->members[j].name,to_be_added.name)==0){
                                    write_client(client.sock,"This user is already in the groupchat.");
                                    duplicate= true;
                                    break;
                                 }
                              }if(!duplicate){
                                 /*The user is not in the groupchat, he can be added*/
                                 add_member(groupchats[i],to_be_added);
                                 char notif[MAX_NOTIFICATION]="You have been added to the groupchat ";
                                 strcat(notif,groupchats[i]->name);
                                 strcat(notif, " by ");
                                 strcat(notif,client.name);
                                 write_client(to_be_added.sock,notif);
                              }
                              break;
                           }
                        }if(!group_found) write_client(client.sock, "You are not a member of this groupchat."); 
                     }
                     break;
                  case QUIT_GROUP_CHAT:;
                     /*Quit a groupchat*/
                     char* name_ofgc= get_name(buffer);
                     bool foundgc = false;
                     for( int i = 0; i < gc_index; i++){
                        if(strcmp(name_ofgc, groupchats[i]->name)==0){
                           foundgc=true;
                           remove_member(groupchats[i],client);
                           char noti[MAX_NOTIFICATION]="You left the groupchat ";
                           strcat(noti,groupchats[i]->name);
                           write_client(client.sock,noti);
                        }
                     } 
                     if(!foundgc){
                        write_client(client.sock, "You are not a member of this groupchat.");
                     }
                     break;
                  case REMOVE_FROM_GROUP_CHAT:;
                     /*Remove an user from a groupchat*/
                     char* group_name= get_name(buffer);
                     /* Remove command and group chat from the buffer */
                     memmove(buffer, buffer + 4 + strlen(group_name), strlen(buffer));
                     int index_client = search_user_by_name(buffer,clients,actual);
                     bool gc_found = false;
                     if(index_client>-1){
                        Client to_be_removed = clients[index_client];
                        for( int i = 0; i < gc_index; i++){
                           if(strcmp(group_name, groupchats[i]->name)==0){
                              gc_found=true;
                              for(int j=0;j<groupchats[i]->size;j++){
                                 if(strcmp(groupchats[i]->members[j].name,to_be_removed.name)==0){
                                    /*check if the user to be removed belongs to the groupchat*/
                                    remove_member(groupchats[i],to_be_removed);
                                    char message[MAX_NOTIFICATION]="You have been removed from the groupchat ";
                                    strcat(message,groupchats[i]->name);
                                    write_client(to_be_removed.sock,message);
                                 }
                              }
                           }
                        } 
                     }
                     if(!gc_found){
                        write_client(client.sock, "You are not a member of this groupchat.");
                     }
                     break;
                  case UNKNOWN:
                     write_client(client.sock, "Unknown command");
                     break;
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

/*Initialize the connection*/
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

/*Extract commands entered by users*/
static enum COMMANDS get_command(const char* buffer){
   if (buffer[0]=='-'){
      if (buffer[1]=='m' && buffer[2]==' ')return DIRECT_MESSAGE;
      else if (buffer[1]=='u' && buffer[2]==' ')return CHANGE_USERNAME;
      else if (buffer[1]=='o'&&strlen(buffer)==2)return ONLINE_USERS;
      else if (buffer[1]=='g'&& buffer[2]==' ')return GROUP_CHAT;
      else if (buffer[1]=='c'&& buffer[2]==' ')return CREATE_GROUP_CHAT;
      else if(buffer[1]=='a'&& buffer[2]==' ')return ADD_TO_GROUP_CHAT;
      else if (buffer[1]=='q'&& buffer[2]==' ')return QUIT_GROUP_CHAT;
      else if (buffer[1]=='r'&& buffer[2]==' ')return REMOVE_FROM_GROUP_CHAT;
      else return UNKNOWN;
   }
   
}

/*Extract username from the buffer*/
static char* get_name(const char* buffer){
   char* name = (char*)malloc(MAX_FILENAME);
   int i = 3;
   while (buffer[i] != ' ' && buffer[i] != '\0' && i < BUF_SIZE)
   {
      name[i - 3] = buffer[i];
      i++;
   }
   name[i - 3] = '\0';
   return name;
}

static Groupchat* create_groupchat(char* members, Client creator, int actual,Client*clients,const char* name){
   Groupchat* new_group = (Groupchat*)malloc(sizeof(Groupchat));
   strcpy(new_group->name,name);
   char space[] = " ";
   char *ptr = strtok(members, space);
   bool found = false;
   //set group name
   new_group->members[0] = creator;
   int mem=1; // index / number of members in groupchat
	while(ptr != NULL)
	{
		//printf("'%s'\n", ptr);
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
         return NULL;
      }
      found = false;
		ptr = strtok(NULL, space);
	}
   new_group->size = mem;
   return new_group;
}

/*Add a member to group chat*/
static void add_member(Groupchat* gc, Client member){
   gc->members[gc->size]=member;
   gc->size++;
   printf("Member succesfully added\n");
}

/*Remove a member of group chat*/
static void remove_member(Groupchat* gc, Client member){
   for(int i = 0; i<gc->size; i++){
      if(strcmp(member.name, gc->members[i].name)==0){
         gc->size = gc->size-1;
         for(int j = i; j<gc->size; j++){
            gc->members[j]= gc->members[j+1];
         }
      }
   }
   printf("Member succesfully removed\n");
}

/*Send the message to one specified recipient - direct message*/
static void send_message_to_specified_client(Client recipient, Client sender, const char *buffer){
   char message[BUF_SIZE];
   strcpy(message, "(");
   strcat(message, sender.name);
   strcat(message, ") : ");
   strcat(message, buffer);
   write_client(recipient.sock, message);
}
/*Send message to all clients in the list*/
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

/*Send message to all members of groupchat*/
static void send_message_to_groupchat(Groupchat* groupchat, Client sender, Client *clients, char *buffer){
   char message[BUF_SIZE];
   message[0] = 0;
   for(int i = 0; i < groupchat->size; i++)
   {
      /* we don't send message to the sender */
      if(sender.sock != groupchat->members[i].sock)
      {  
         strncpy(message, "( ", sizeof message - strlen(message) - 1);
         strncat(message, groupchat->name, BUF_SIZE - 1);
         strncat(message, " ) ", sizeof message - strlen(message) - 1);
         strncat(message, sender.name, BUF_SIZE - 1);
         strncat(message, " : ", sizeof message - strlen(message) - 1);
         strncat(message, buffer, sizeof message - strlen(message) - 1);
         write_client(groupchat->members[i].sock, message);
      }
   }
}

/*Send confirmation message after creating groupchat*/
static void send_confirmation_message(Groupchat* gc){
   char confirmation[BUF_SIZE] = "You just made a groupchat called ";
   strcat(confirmation, gc->name);
   strcat(confirmation, " with users ");
   for(int i = 0; i < gc->size; i++){
      strcat(confirmation,gc->members[i].name);
      strcat(confirmation, " "); 
   }
   //printf(" confirmation message: %s\n", confirmation);
   write_client(gc->members[0].sock, confirmation);
   char to_members[BUF_SIZE] = "Added you to groupchat called ";
   strcat(to_members, gc->name);
   strcat(to_members, " with users ");
   for(int i = 0; i < gc->size; i++){
      strcat(to_members,gc->members[i].name);
      strcat(to_members, " "); 
   }
   //printf("message to members: %s\n", to_members);
   //send message to each member of gc saying that they are in the groupchat
   
   for(int i = 1; i < gc->size; i++){
      send_message_to_specified_client(gc->members[i],gc->members[0],to_members);
   }
}

/*Search for recipient in the list of clients*/
static int search_user_by_name(const char* name,Client*clients, int actual){
   /* Find client from name */
   for (int i = 0; i < actual; i++)
   {
      if (strcmp(clients[i].name, name) == 0)
      {
         return i;
      }
   }
   return -1;
}

/*Create message*/
static Message* create_message(const char* buffer, const char* sender, const char* recipient, int* nbCurrentMessage,Message* messages){
   time_t t;
   time(&t);
   Message* newMessage = (Message*)malloc(sizeof(Message));
	newMessage->timestamp = localtime(&t);
   /*Save message history in the array messages*/
   strcpy(newMessage->content,buffer);
   strcpy(newMessage->sender,sender);
   strcpy(newMessage->recipient,recipient);
   int nbMessages = *nbCurrentMessage;
   messages[nbMessages] = *newMessage;
   nbMessages++;
   *nbCurrentMessage=nbMessages;
   return newMessage;
}

/*Save message history in text files*/
static void save_history(Message* m){
   char filenameIn[MAX_FILENAME];
   char filenameOut[MAX_FILENAME];
   strcpy(filenameIn,m->recipient);
   strcat(filenameIn,".txt");
   strcpy(filenameOut,m->sender);
   strcat(filenameOut,".txt");
   FILE* fptrIn;
   FILE* fptrOut;
   fptrIn = fopen(filenameIn,"a");
   fptrOut = fopen(filenameOut,"a");
   if(fptrIn == NULL ||fptrOut ==NULL)
   {
      perror("Error when opening files.");   
      return;             
   }
   char timestamp[30];
   strftime(timestamp, 30, "%x - %I:%M%p", m->timestamp);
   fprintf(fptrIn,"received from (%s) at %s: %s \n",m->sender,timestamp,m->content);
   fprintf(fptrOut,"sent to (%s) at %s: %s \n",m->recipient,timestamp,m->content);
   fclose(fptrIn);
   fclose(fptrOut);
   free(m);
}
/*Save message history in text files*/
static void save_history_groupchat(Message* m,Groupchat* gc){
   char filenameIn[MAX_FILENAME];
   char filenameOut[MAX_FILENAME];
   strcpy(filenameOut,m->sender);
   strcat(filenameOut,".txt");
   FILE* fptrIn;
   FILE* fptrOut;
   fptrOut = fopen(filenameOut,"a");
   char timestamp[30];
   strftime(timestamp, 30, "%x - %I:%M%p", m->timestamp);
   if(fptrOut ==NULL)
   {
      perror("Error when opening files.");   
      return;             
   }
   int nbMembers = gc->size;
   for (int i=0;i<nbMembers;i++){
      if(strcmp(gc->members[i].name,m->sender)!=0){
         strcpy(filenameIn,gc->members[i].name);
         strcat(filenameIn,".txt");
         fptrIn = fopen(filenameIn,"a");
         if(fptrIn ==NULL)
         {
            perror("Error when opening files.");   
            return;             
         }
         fprintf(fptrIn,"received from groupchat (%s) - %s at %s: %s \n",m->recipient,m->sender,timestamp,m->content);
         fclose(fptrIn);
      }
   }
   fprintf(fptrOut,"sent to (%s) at %s: %s \n",m->recipient,timestamp,m->content);
   fclose(fptrOut);
   free(m);
}
   
/*Load message history in text file*/
static void load_history(Client client){
   FILE* fptr;
   char *line_buf = NULL;
   size_t line_buf_size = 0;
   int line_count = 0;
   ssize_t line_size;
   int i =0;
   char filename[MAX_FILENAME];
   strcpy(filename,client.name);
   strcat(filename,".txt");
   fptr = fopen(filename,"r");
   if(fptr == NULL)
   {
      perror("Error when opening files.");   
      return;             
   } 
   /* Get the first line of the history file. */
   line_size = getline(&line_buf, &line_buf_size, fptr);
   /* Loop through until we are done with the file. */
   while (line_size >= 0)
   {
      /* Increment our line count */
      line_count++;
      /* Send the line buffer to the client */
      write_client(client.sock, line_buf);
      /* Get the next line */
      line_size = getline(&line_buf, &line_buf_size, fptr);
  }
  /* Free the allocated line buffer */
  free(line_buf);

  /* Close the file now that we are done with it */
   fclose (fptr);
   write_client(client.sock,"All history has been loaded.");
}


int main(int argc, char **argv)
{
   init();

   app();

   end();

   return EXIT_SUCCESS;
}
