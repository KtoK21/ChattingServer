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

#define PORT 5000

void respond (int sock);
char *ROOT;

struct message{
	int type;
	char receiver[10][32]={{0},};
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
void SendtoAll(message );
void EditUserInfo(message*, int);
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
	while(1){
		n = recv(sock, &msg, sizeof(message), 0);
		if (n < 0) {
			printf("recv() error\n");
			return;
		} else if (n == 0) {
			printf("Client disconnected unexpectedly\n");
			for(int i=0; i<10; i++)
				for(int j=0; j<100; j++)
					if(room[i][j].sock==sock){
						sprintf(msg.buffer, "%s is disconnected from room #%d",msg.Nickname, msg.RoomNum);
						SendtoAll(msg);
						room[i][j]=clientInfo();
						if(!room[i][j+1].IsLast)
							room[i][j].IsLast=false;
						shutdown(sock, SHUT_RDWR);
						close(sock);
						break;
					}
			return;
		}
		if(msg.type==1){
			if(!strcmp(msg.buffer, "/quit\n")){
				sprintf(msg.buffer, "Good bye %s",msg.Nickname);
				write(sock, &msg, sizeof(message));
				sprintf(msg.buffer, "%s is disconnected from room #%d",msg.Nickname, msg.RoomNum);
				SendtoAll(msg);
				room[msg.RoomNum-1][msg.UserNum]=clientInfo();
				shutdown(sock, SHUT_RDWR);
				close(sock);
				return;
			}
			
			EditUserInfo(&msg, sock);			
			sprintf(msg.buffer, "%s%s%s%d", "Hello ", msg.Nickname, "! This is room #",msg.RoomNum);
			write(sock, &msg, sizeof(message));
			sprintf(msg.buffer, "%s joined room %d", msg.Nickname, msg.RoomNum);
			SendtoAll(msg);
		}

		else{																							//send message to others
			if(msg.type==3)																				//send meesage to all
				SendtoAll(msg);				
			else{					
					//send meesage to selectivly
					if(!(strcmp(room[msg.RoomNum-1][msg.UserNum].Nickname, msg.Nickname))){					//Found sender's info
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
									printf("send to %s\n",msg.receiver[j]);
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
			}
		}
	}
}	


void SendtoAll(message msg){
	for(int i=0; i<100; i++)
		if(!room[msg.RoomNum-1][i].IsEmpty){
			if(strcmp(room[msg.RoomNum-1][i].Nickname, msg.Nickname))
				write(room[msg.RoomNum-1][i].sock, &msg, sizeof(message));
		}
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
