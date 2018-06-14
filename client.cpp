#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <string.h>
#include <arpa/inet.h>
#include <pthread.h>

void* SendMsg(void*);

struct message{
	int type;							//1 for initialization, 2 for msg to individuals, 3 for msg to all
	char receiver[10][32] = {{0},};
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

int sockfd;
bool IsThreadDone=false;
int main( int argc, char *argv[] ) {
	char *ip_addr, *portno;
	message recv_msg;
	message send_msg;
	struct sockaddr_in serv_addr;

	if(argc < 4){
		printf("Usage: %s IP:port RoomNumber  Nickname\n",argv[0]);
		return -1;
	}

	if(sizeof(argv[3]) > 32){
		printf("Nickname should be shorter than 32 char\n");
		return -1;
	}

	else if(atoi(argv[2])>10){
		printf("There are only 10 rooms\n");
		return -1;
	}

	sockfd = socket(AF_INET, SOCK_STREAM, 0);

	if (sockfd <0) {
		perror("ERROR opening socket");
		exit(1);
	}
	ip_addr = strtok(argv[1], ":");
	portno = strtok(NULL, "");
	
	bzero((char *) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_addr.s_addr = inet_addr(ip_addr);
	serv_addr.sin_port = htons(atoi(portno));
	
	if (connect(sockfd, (struct sockaddr*)&serv_addr, sizeof(serv_addr)) < 0){
		perror("ERROR connect fail");
		return -1;
	}
	
	printf("Successfully connected\n");

//-------------------------Connect to Server Done---------------------------

	send_msg.type=1;
	strcpy(send_msg.Nickname, argv[3]);
	memset(send_msg.buffer,0,1000);
	send_msg.RoomNum=atoi(argv[2]);
	send_msg.sock=sockfd;		
	int test=send(sockfd, (char*)&send_msg, sizeof(message),0);

//-------------------------Send userInfo------------------------------------
	int length = recv(sockfd, &recv_msg, sizeof(message), 0);
	char tmp[300];
	sprintf(tmp, "%s%s%s%s","Hello ",argv[2],"! This is room #",(char*)argv[3]);
	
	if(recv_msg.type==1 && !strcmp(recv_msg.buffer, tmp)){
		perror("ERROR fail to get in room");
		return -1;
	}
	send_msg.UserNum=recv_msg.UserNum;
	printf("%s\n", recv_msg.buffer);
//------------------------Successfully exchange UserInfo--------------------
	pthread_t thread;
	pthread_create(&thread, NULL, SendMsg, (void*)&send_msg);
//------------------------MultiThread for keyboard input--------------------
	while (1) {
		int n=recv(sockfd, &recv_msg, sizeof(message), 0);
		if(n<0){
			printf("recv() error\n");
			break;
		}
		else if(n==0){
			printf("Server disconnected unexpectedly\n");
			break;
		}
		if(recv_msg.type==1 && strcmp(recv_msg.Nickname, argv[2])){
			printf("%s\n", recv_msg.buffer); 
		}
		else
			printf("%s : %s\n",recv_msg.Nickname, recv_msg.buffer);
		if(IsThreadDone){
			break;
		}
	}
//------------------------main Thread for receving from Server--------------
	pthread_join(thread, NULL);
	close(sockfd);
	return 0;
}	

void* SendMsg(void* msg) {
	char *result;
	message send_msg =*(message*)msg;
	while (1) {
	
		result = fgets(send_msg.buffer, 1000, stdin);
		if(!strcmp(send_msg.buffer, "/quit\n")){
			send_msg.type=1;
			send(sockfd, &send_msg, sizeof(send_msg),0);
			IsThreadDone=true;
			return msg;
		}
		char* token=strtok(send_msg.buffer, " ,"); 
		send_msg.type=2;
		int i=0;
		bool IsValid=true;
		while(token!=NULL){
			if(strlen(token)>32){
				IsValid=false;
				printf("Username shoud be shorter than 32 char\n");
				break;
			}
				strcpy(send_msg.receiver[i], token);
			i++;
			token=strtok(NULL, " ,");
			if(!strcmp(token, ":"))
				break;
		}										//parsing receiver list
		if(!IsValid)
			continue;
		strcpy(send_msg.buffer, strtok(NULL,""));
		send_msg.buffer[strlen(send_msg.buffer)-1]='\0';

		if(!strcmp(send_msg.receiver[0], "All"))
			send_msg.type=3;
		send(sockfd, &send_msg, sizeof(send_msg), 0);
	}
}
