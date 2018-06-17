#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include <string.h>

#include <fcntl.h>

#include <sys/sendfile.h>
#include <sys/types.h>
#include <pthread.h>
#include "./ThreadPool.h"

#define PORT 5001

void respond (int sock);
char *ROOT;

struct message{
	int type;
	char receiver[10][32]={{0},};
	int time[10]={0,};
	char buffer[1000];
	char Nickname[32];
	int RoomNum;
	int UserNum;
	int sock;
		
	message(){
		type=0;
		memset(buffer, 0, 1000);
		memset(Nickname, 0, 32);
		RoomNum=-1;
		UserNum=-1;
		sock=0;
	}
};

struct clientInfo{
	bool IsEmpty;
	char Nickname[32];
	int sock;
	bool IsLast;
	clientInfo(){
		IsEmpty=true;
		memset(Nickname, 0, 32);
		sock=0;
		IsLast=true;
	}
};

struct Reserve{
	message msg;
	int time;
	int slot;
};
void ReservedMsg(Reserve );
void clientInfoReset(clientInfo *);
void SendtoAll(message* );
void EditUserInfo(message*, int);
void JoinRoom(message*, int );
void ExitRoom(message* , bool , int );
clientInfo room[10][100];		//[i][j] ith room,jth client with name

int main( int argc, char *argv[] ) {
	int sockfd, newsockfd, portno = PORT;
	socklen_t clilen;
	struct sockaddr_in serv_addr, cli_addr;
	clilen = sizeof(cli_addr);
	ROOT = getenv("PWD");

	/* First call to socket() function */
	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd < 0) {
		perror("ERROR opening socket");
		exit(1);
	}

	// port reusable
	int tr = 1;
	if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &tr, sizeof(int)) == -1) {
		perror("setsockopt");
		exit(1);
	}

	/* Initialize socket structure */
	memset((char *) &serv_addr, 0, sizeof(serv_addr));

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = INADDR_ANY;
	serv_addr.sin_port = htons(portno);

	if (bind(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		perror("ERROR binding");
		return -1;
	}

	listen(sockfd, 5);

	printf("Server is running on port %d\n", portno);

	ThreadPool pool(100);

	while (1) {
		newsockfd = accept(sockfd, (struct sockaddr*)&cli_addr, &clilen);
		if (newsockfd < 0){
			perror("ERROR accept connection");
			return -1;
		}
		
		pool.enqueue([newsockfd]{respond(newsockfd);});
	}
	close(sockfd);
	return 0;
}

void respond(int sock) {
	int n, file;
	message msg;
	ThreadPool rsrv(10);
	while(1){
		n = recv(sock, &msg, sizeof(message), 0);
		if (n < 0) {
			printf("error\n");
			printf("recv() error\n");
			return;
		} 
		else if (n == 0) {
			printf("Client disconnected unexpectedly\n");
			ExitRoom(&msg, true, sock);
			shutdown(sock, SHUT_RDWR);
			close(sock);
			return;
		}
		if(msg.type==1){
			if(!strcmp(msg.buffer, "/quit\n")){
				sprintf(msg.buffer, "Good bye %s",msg.Nickname);
				write(sock, &msg, sizeof(message));
				ExitRoom(&msg, false, 0);
				shutdown(sock, SHUT_RDWR);
				close(sock);
				return;
			}
			
			else if(!strcmp(msg.buffer, "/list\n")){
				int count=1;
				sprintf(msg.buffer, "This is list of users in room #%d\n", msg.RoomNum);
				for(int i=0; i<100; i++){
					if(room[msg.RoomNum-1][i].IsLast)
						break;

					else if(!(room[msg.RoomNum-1][i].IsEmpty)){
						char next[1000];
						sprintf(next, "%d. %s\n", count, room[msg.RoomNum-1][i].Nickname);
						if(!strcmp(room[msg.RoomNum-1][i].Nickname, msg.Nickname))
							sprintf(next, "%d. %s*\n", count, room[msg.RoomNum-1][i].Nickname);
						sprintf(msg.buffer, "%s%s", msg.buffer, next);
						count++;
					}
				}
				msg.buffer[strlen(msg.buffer)-1]='\0';
				write(sock, &msg, sizeof(message));
				continue;
			}
			else{
			int NumEqualName=1;
			for(int i=0; i<10; i++)
				for(int j=0; j<100; j++)
					if(room[i][j].IsLast)
						break;
					else if(!strcmp(msg.Nickname, room[i][j].Nickname)){
						NumEqualName++;
						char* tmpNick=strtok(msg.Nickname,"-");
						sprintf(msg.Nickname,"%s-%d",tmpNick,NumEqualName);
					}
			JoinRoom(&msg, sock);
			}	
		}

		else if(msg.type==2){					
				char buffer[200]="There is no such user : ";
				bool IsMissing=false;
				for(int j=0; j<10; j++){
					if(!strlen(msg.receiver[j]))
						break;
					bool IsExist=false;
					for(int k=0; k<100; k++){													//Is receiver exist?
						if(room[msg.RoomNum-1][k].IsLast)
							break;
						if(!strcmp(msg.receiver[j],room[msg.RoomNum-1][k].Nickname)){
							IsExist=true;
							if(msg.time[j]!=0){
								Reserve R;
								R.time=msg.time[j];
								R.msg=msg;
								R.slot=k;
								rsrv.enqueue([R]{ReservedMsg(R);});
							}
							
							else
								write(room[msg.RoomNum-1][k].sock, &msg, sizeof(message));
							break;
						}
					}
					if(!IsExist){
						IsMissing=true;
						sprintf(buffer, "%s%s, ",buffer, msg.receiver[j]);
					}
				}
				if(IsMissing){
					buffer[strlen(buffer)-2]='\0';
					sprintf(msg.buffer,"%s", buffer);
					msg.type=1;
					write(sock, &msg, sizeof(message));
				}
		}
		
		else if(msg.type==3){
			if(msg.time[0]!=0){
				Reserve R;
				R.time=msg.time[0];
				R.msg=msg;
				rsrv.enqueue([R]{ReservedMsg(R);});
			}
			else
				SendtoAll(&msg);
		}
		
		else if(msg.type==4){
			int newRoomNum=atoi(msg.buffer);
			ExitRoom(&msg, false, 0);
			msg.RoomNum=newRoomNum;
			JoinRoom(&msg, sock);
		}	
	}
}

void ReservedMsg(Reserve R){
	sleep(R.time);
	if(R.msg.type==3)
		SendtoAll(&R.msg);
	else
		write(room[R.msg.RoomNum-1][R.slot].sock, &R.msg, sizeof(message));
}

void SendtoAll(message* msg){
	for(int i=0; i<100; i++)
		if(!room[msg->RoomNum-1][i].IsEmpty){
			if(strcmp(room[msg->RoomNum-1][i].Nickname, msg->Nickname))
				write(room[msg->RoomNum-1][i].sock, msg, sizeof(message));
		}
		else
			if(room[msg->RoomNum-1][i].IsLast)
				break;
	return;
}	

void EditUserInfo(message* msg, int sock){
	for(int i=0; i<100; i++)
		if(room[msg->RoomNum-1][i].IsEmpty){
			strcpy(room[msg->RoomNum-1][i].Nickname, msg->Nickname);
			room[msg->RoomNum-1][i].sock=sock;
			room[msg->RoomNum-1][i].IsEmpty = false;
			room[msg->RoomNum-1][i].IsLast = false;
			msg->UserNum=i;
			break;
		}
}

void ExitRoom(message* msg, bool isException, int sock){
	if(isException){
		for(int i=0; i<10; i++){
			bool IsDone=false;
			for(int j=0; j<100; j++)
				if(room[i][j].sock==sock){
					msg->RoomNum=i+1;
					msg->UserNum=j;
					IsDone=true;
					break;
				}
			if(IsDone)
				break;
		}
	}
	sprintf(msg->buffer, "%s is disconnected from room #%d",msg->Nickname, msg->RoomNum);
	SendtoAll(msg);
	clientInfoReset(&room[msg->RoomNum-1][msg->UserNum]);
	printf("info in %d,%d : %s, %d\n",msg->RoomNum-1, msg->UserNum, room[msg->RoomNum-1][msg->UserNum].Nickname, room[msg->RoomNum-1][msg->UserNum].sock);
	if(!room[msg->RoomNum-1][msg->UserNum+1].IsLast)
		room[msg->RoomNum-1][msg->UserNum].IsLast=false;
}

void JoinRoom(message* msg, int sock){
	EditUserInfo(msg, sock);			
	sprintf(msg->buffer, "%s%s%s%d", "Hello ", msg->Nickname, "! This is room #",msg->RoomNum);
	write(sock, msg, sizeof(message));
	printf("info in %d,%d : %s, %d\n",msg->RoomNum-1, msg->UserNum, room[msg->RoomNum-1][msg->UserNum].Nickname, room[msg->RoomNum-1][msg->UserNum].sock);
	sprintf(msg->buffer, "%s joined room %d", msg->Nickname, msg->RoomNum);
	SendtoAll(msg);
}

void clientInfoReset(clientInfo* client){
	client->sock=0;
	memset(client->Nickname, 0, 32);
	client->IsEmpty=true;
	client->IsLast=true;
}
