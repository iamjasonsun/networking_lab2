/* fpont 12/99 */
/* pont.net    */
/* udpServer.c */

/* Converted to echo client/server with select() (timeout option). See udpClient.c */
/* 3/30/05 John Schultz */

#include <stdlib.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h> /* close() */
#include <string.h> /* memset() */
#include <sys/stat.h>
#include <time.h>

#define SOCKET_ERROR -1
#define MAX_MSG 100
#define PKT_SIZE 1000
#define PAYLOAD_SIZE 800
#define ACK_BUFFER_SIZE 64
#define MAX_RESEND_TIME 16

/* function prototypes */
void sendFileToClient(char *fileName, int sd, int flags, struct sockaddr *cliAddr, int cliLen, float probability);
int getFileSize(char *filePath);
int lostPacket(float pro);
int checkResendPacket(int sd, int timeout, char *ackBuffer, int flags, struct sockaddr *cliAddr, int *cliLenAdd, int seqNum);
int isExpectedACK(char *ackBuffer, int seqNum);
int startTimer(int sd, int timeout);

int isReadable(int sd,int * error,int timeOut) { // milliseconds
  fd_set socketReadSet;
  FD_ZERO(&socketReadSet);
  FD_SET(sd,&socketReadSet);
  struct timeval tv;
  if (timeOut) {
    tv.tv_sec  = timeOut / 1000;
    tv.tv_usec = (timeOut % 1000) * 1000;
  } else {
    tv.tv_sec  = 0;
    tv.tv_usec = 0;
  } // if
  if (select(sd+1,&socketReadSet,0,0,&tv) == SOCKET_ERROR) {
    *error = 1;
    return 0;
  } // if
  *error = 0;
  return FD_ISSET(sd,&socketReadSet) != 0;
} /* isReadable */

int main(int argc, char *argv[]) {
  
  int sd, rc, n, cliLen, flags, portNum;
  struct sockaddr_in cliAddr, servAddr;
  char msg[MAX_MSG];
  float probability;

  if(argc!=3){
    printf("Usage: %s <portNumber> <pl>\n", argv[0]);
    exit(1);
  }

  portNum = atoi(argv[1]);
  probability = atof(argv[2]);

  /* socket creation */
  sd=socket(AF_INET, SOCK_DGRAM, 0);
  if(sd<0) {
    printf("%s: cannot open socket \n",argv[0]);
    exit(1);
  }

  /* bind local server port */
  servAddr.sin_family = AF_INET;
  servAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  servAddr.sin_port = htons(portNum);
  rc = bind (sd, (struct sockaddr *) &servAddr,sizeof(servAddr));
  if(rc<0) {
    printf("%s: cannot bind port number %d \n", 
	   argv[0], portNum);
    exit(1);
  }

  printf("%s: waiting for data on port UDP %u\n", 
	   argv[0],portNum);

/* BEGIN jcs 3/30/05 */

  flags = 0;

/* END jcs 3/30/05 */

  /* server infinite loop */
  while(1) {
    /*init srand*/
    srand(time(NULL));
    
    /* init buffer */
    memset(msg,0x0,MAX_MSG);

    /* receive message */
    cliLen = sizeof(cliAddr);
    n = recvfrom(sd, msg, MAX_MSG, flags,
		 (struct sockaddr *) &cliAddr, &cliLen);

    if(n<0) {
      printf("%s: cannot receive data \n",argv[0]);
      continue;
    }
      
    /* print received message */
    printf("%s: from %s:UDP%u : %s \n", 
	   argv[0],inet_ntoa(cliAddr.sin_addr),
	   ntohs(cliAddr.sin_port),msg);

/* BEGIN jcs 3/30/05 */

    sleep(1);

    sendFileToClient(msg, sd, flags, (struct sockaddr *)&cliAddr, cliLen, probability);

/* END jcs 3/30/05 */
    
  }/* end of server infinite loop */

return 0;
}
/*
 *
 *
 */
void sendFileToClient(char *fileName, int sd, int flags, struct sockaddr *cliAddr, int cliLen, float probability){
	FILE *fileId;
	char packet[PKT_SIZE];
	char ackBuffer[ACK_BUFFER_SIZE];
	int fileLength, pktMaxNum, seqNum, headerLength, n, i, error, resentCount, resentTotalCount;
	char *header = "%d %d\r\n\r\n";
	int timeout = 1000;
	//open file
	fileId = fopen(fileName,"r"); // read mode
	if(fileId == NULL){
		printf("Cannot open file \"%s\" on server.\n", fileName);
		return;
	}
	// get file size
	fileLength = getFileSize(fileName);
	if(fileLength<0){printf("Error occurs while getting file size on server.\n");return;}
	// calculate the total number of packets
	pktMaxNum = fileLength % PAYLOAD_SIZE == 0 ? (fileLength/PAYLOAD_SIZE):(fileLength/PAYLOAD_SIZE+1);
	seqNum = 1;
	resentTotalCount = 0;
	//start sending data
	printf("Sending data to client...\n");
	for(i=0; i<pktMaxNum; i++){
		//make packet: make header, write file content to packet
		headerLength = sprintf(packet, header, seqNum, pktMaxNum);
		if(headerLength > PKT_SIZE - PAYLOAD_SIZE){printf("Server: Header size cannot over %d.\n", PKT_SIZE - PAYLOAD_SIZE);return;}
		n = fread(packet+headerLength, sizeof(char), PAYLOAD_SIZE, fileId);
		//send packet
		if(!lostPacket(probability)){//simulate packet lost
			error = sendto(sd,packet,n+headerLength,flags,cliAddr,cliLen);
			if(error<0){printf("Cannot send packet %d to client.\n", seqNum);return;}
			printf("Packet %d is sent.\n", seqNum);
		}else{
			printf("Packet %d is sent. (will lost)\n", seqNum);
		}
		resentCount = 0;
		//check if we need to resend packet; if it is not an expected ACK or timeout, resend the packet
		while(checkResendPacket(sd, timeout, ackBuffer, flags, cliAddr, &cliLen, seqNum)){//resend packet
			if(!lostPacket(probability)){//simulate packet lost
				error = sendto(sd,packet,n+headerLength,flags,cliAddr,cliLen);
				if(error<0){printf("Cannot send packet %d to client.\n", seqNum);return;}
				printf("Packet %d is resent.\n", seqNum);
			}else{
				printf("Packet %d is resent. (will lost)\n", seqNum);
			}
			resentTotalCount++;
			//if resent more than 16 times did not get back from client; client is dead, return
			resentCount++;
			if(resentCount>MAX_RESEND_TIME){printf("Client is not alive any more. Stop...\n");return;}
		}
		seqNum++;
	}
	fclose(fileId);
	printf("All data successfully sent. %d packets are retransmitted.\n", resentTotalCount);
}

/*
Waiting for ACK from client; if timeout or received ACK is not expected, return 1 to indicate we need to resend the packet.
Return 1 if we need to resent packet; return 0, if we do not need to resent
*/
int checkResendPacket(int sd, int timeout, char *ackBuffer, int flags, struct sockaddr *cliAddr, int *cliLenAdd, int seqNum){
	int timerStatus, error;
	//waiting for ACK from
	printf("Waiting for ACK from client...");
	timerStatus = startTimer(sd, timeout);
	printf("\n");
	if(!timerStatus){//timeout
		printf("Timeout waiting for ACK%d.\n", seqNum);
		return 1;
	}
	//receive packet within timeout
	error = recvfrom(sd, ackBuffer, ACK_BUFFER_SIZE, flags, cliAddr, cliLenAdd);
	if(error<0){printf("Cannot receive ACK from client.\n");return 1;}
	//check the received ACK is expected or not
	if(!isExpectedACK(ackBuffer, seqNum)){//not expected, resend packet
		printf("Not expected ACK.\n");
		return 1;
	}else{
		printf("Received ACK%d\n", seqNum);
		return 0;
	}
}

/*Return 1 if it is expected ACK; return 0 if it is not expected ACK*/
int isExpectedACK(char *ackBuffer, int seqNum){
	int receivedSeqNum;
	if(!(ackBuffer[0]=='A'&&ackBuffer[1]=='C'&&ackBuffer[2]=='K'&&ackBuffer[3]==' '&&strlen(ackBuffer)>4)){
		return 0;
	}
	receivedSeqNum = atoi(ackBuffer+4);
	return receivedSeqNum == seqNum ? 1 : 0;
}

/*Return 1 if timer stops before timeout; return 0 if timeout*/
int startTimer(int sd, int timeout){
	int error;
	int timestep = 100;
	int counter = 0;
	while (!isReadable(sd,&error,timestep)){
		printf(".");
		counter++;
		if(counter==timeout/timestep){return 0;}
	}
	return 1;
}

int getFileSize(char *filePath){
	struct stat fst;
	bzero(&fst,sizeof(fst));
        if (stat(filePath,&fst) != 0) {return -1;}
	return fst.st_size;
}

/*Simulate packet lost; return 1 if loss packet; return 0 if not*/
int lostPacket(float pro){
	float rnd;
	rnd = (float)rand() / (float)RAND_MAX;
	return rnd < pro ? 1 : 0;
}
