/* fpont 12/99 */
/* pont.net    */
/* udpClient.c */

/* Converted to echo client/server with select() (timeout option) */
/* 3/30/05 John Schultz */

#include <stdlib.h> /* for exit() */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h> /* memset() */
#include <sys/time.h> /* select() */ 
#include <time.h>


/* BEGIN jcs 3/30/05 */

#define SOCKET_ERROR -1
#define PKT_SIZE 1000
#define ACK_BUFFER_SIZE 64

/* function prototypes */
void receiveDataFromServer(int sd, int flags, int timeOut, char *fileName, float probability);
int indexOfCharInString(char *str, int length, char ch);
int parseHeader(char *packet, int headerLength, int *seqNum, int *pktMaxNum);
int findHeaderLength(char *packet, int length);
void sendAck(int sd, int seqNum, int flags, const struct sockaddr *echoServAddr, int echoLen);
int lostAck(float pro);

void error(char *msg)
{
    perror(msg);
    exit(1);
}

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

/* END jcs 3/30/05 */

int main(int argc, char *argv[]) {
  
  int sd, rc, flags, timeOut, portNum;
  float probability;
  struct sockaddr_in cliAddr, remoteServAddr;
  struct hostent *h;


  /* check command line args */
  if(argc<5) {
    printf("usage : %s <server> <port_number> <filename> <lost ACK probability>\n", argv[0]);
    exit(1);
  }

  /*init srand*/
  srand(time(NULL));

  portNum = atoi(argv[2]);
  probability = atof(argv[4]);

  /* get server IP address (no check if input is IP address or DNS name */
  h = gethostbyname(argv[1]);
  if(h==NULL) {
    printf("%s: unknown host '%s' \n", argv[0], argv[1]);
    exit(1);
  }

  printf("%s: sending data to '%s' (IP : %s) \n", argv[0], h->h_name,
	 inet_ntoa(*(struct in_addr *)h->h_addr_list[0]));

  remoteServAddr.sin_family = h->h_addrtype;
  memcpy((char *) &remoteServAddr.sin_addr.s_addr, 
	 h->h_addr_list[0], h->h_length);
  remoteServAddr.sin_port = htons(portNum);

  /* socket creation */
  sd = socket(AF_INET,SOCK_DGRAM,0);
  if(sd<0) {
    printf("%s: cannot open socket \n",argv[0]);
    exit(1);
  }
  
  /* bind any port */
  cliAddr.sin_family = AF_INET;
  cliAddr.sin_addr.s_addr = htonl(INADDR_ANY);
  cliAddr.sin_port = htons(0);
  
  rc = bind(sd, (struct sockaddr *) &cliAddr, sizeof(cliAddr));
  if(rc<0) {
    printf("%s: cannot bind port\n", argv[0]);
    exit(1);
  }

/* BEGIN jcs 3/30/05 */

  flags = 0;

  timeOut = 100; // ms

/* END jcs 3/30/05 */

  /* send filename */
  rc = sendto(sd, argv[3], strlen(argv[3])+1, flags, (struct sockaddr *) &remoteServAddr, sizeof(remoteServAddr));

  if(rc<0) {
      printf("%s: cannot send filename %s\n",argv[0], argv[3]);
      close(sd);
      exit(1);}

/* BEGIN jcs 3/30/05 */

  receiveDataFromServer(sd, flags, timeOut, argv[3], probability);

/* END jcs 3/30/05 */
  return 1;
}

void receiveDataFromServer(int sd, int flags, int timeOut, char *fileName, float probability){
	struct sockaddr echoServAddr;
	int echoLen, errorL, receivedCount, headerLength, seqNum, pktMaxNum, n;
	char buffer[PKT_SIZE];
	FILE *fileId;
	int preSeqNum = -1;
	//open file to store reveived data
	fileId = fopen(fileName,"w"); // write mode
	if(fileId == NULL){error("Cannot open file on client.\n");}

	//wait for data from server
	printf("Waiting for data from server...");
	while (!isReadable(sd,&errorL,timeOut)) printf(".");
	printf("\n");
	//reveive data from server; store them into buffer
	receivedCount = recvfrom(sd, buffer, PKT_SIZE, flags, (struct sockaddr *) &echoServAddr , &echoLen);
	if(receivedCount<0){error("Client cannot receive data.\n");}
	if(receivedCount>PKT_SIZE){error("Packet size from server is over the limit on client.\n");}
	//parse header
	headerLength = findHeaderLength(buffer, PKT_SIZE);if(headerLength<0){error("Invalid header.\n");}
	errorL = parseHeader(buffer, headerLength, &seqNum, &pktMaxNum);if(errorL < 0){error("Error parsing header.\n");}
	printf("Received packet %d.\n", seqNum);
	//send ACK, simulate ACK lost
	if(!lostAck(probability)){
		sendAck(sd, seqNum, flags, &echoServAddr, echoLen);
		printf("ACK%d sent.\n",seqNum);
	}else{
		printf("ACK%d sent.(will lost)\n",seqNum);
	}
	while(seqNum<pktMaxNum){
		//check the received packet is duplicated or not
		if(preSeqNum==seqNum){//duplicated
			printf("Duplicated packet %d. Drop it.\n", seqNum);
		}else{
			//write data into file
			n = fwrite(buffer+headerLength, sizeof(char), receivedCount-headerLength, fileId);
			if(n<0){error("Error writing data into file.\n");}
			preSeqNum = seqNum;
		}
		//wait for data from server
		while (!isReadable(sd,&errorL,timeOut)) printf(".");
		printf("\n");
		//reveive data from server; store them into buffer
		receivedCount = recvfrom(sd, buffer, PKT_SIZE, flags, (struct sockaddr *) &echoServAddr , &echoLen);
		if(receivedCount<0){error("Client cannot receive data.\n");}
		if(receivedCount>PKT_SIZE){error("Packet size from server is over the limit on client.\n");}
		//parse header
		headerLength = findHeaderLength(buffer, PKT_SIZE);if(headerLength<0){error("Invalid header.\n");}
		errorL = parseHeader(buffer, headerLength, &seqNum, &pktMaxNum);if(errorL < 0){error("Error parsing header.\n");}
		printf("Received packet %d.\n", seqNum);
		//send ACK, simulate ACK lost
		if(!lostAck(probability)){
			sendAck(sd, seqNum, flags, &echoServAddr, echoLen);
			printf("ACK%d sent.\n",seqNum);
		}else{
			printf("ACK%d sent.(will lost)\n",seqNum);
		}
	}
	//write data into file
	n = fwrite(buffer+headerLength, sizeof(char), receivedCount-headerLength, fileId);
	if(n<0){error("Error writing data into file.\n");}
	//close file
	fclose(fileId);
	printf("All data successfully received.\n");
}

void sendAck(int sd, int seqNum, int flags, const struct sockaddr *echoServAddr, int echoLen){
	int n;
	char ackBuffer[ACK_BUFFER_SIZE];
	n = sprintf(ackBuffer, "ACK %d", seqNum);if(n>ACK_BUFFER_SIZE){error("ACK buffer overflow.\n");}
	n = sendto(sd,ackBuffer,n+1,flags,echoServAddr,echoLen);if(n<0){error("Cannot send ACK to server.\n");}
}

int findHeaderLength(char *packet, int length){
	int i;	
	for(i=0; i<length-3; i++){
		if(packet[i]=='\r'&&packet[i+1]=='\n'&&packet[i+2]=='\r'&&packet[i+3]=='\n'){
			return i+4;
		}
	}
	return -1;
}

int parseHeader(char *packet, int headerLength, int *seqNum, int *pktMaxNum){
	int spaceIndex, i, lineBreakIndex, n;
	char buffer[32];
	spaceIndex = indexOfCharInString(packet, headerLength, ' ');
	if(spaceIndex<0){return -1;}
	//find sequence number
	for(i=0; i<spaceIndex; i++){
		buffer[i] = packet[i];
	}
	buffer[spaceIndex] = '\0';
	*seqNum = atoi(buffer);
	//find packet max number
	lineBreakIndex = indexOfCharInString(packet, headerLength, '\r');
	if(lineBreakIndex<0){return -1;}
	n = 0;
	for(i=spaceIndex+1; i<lineBreakIndex; i++){
		buffer[n] = packet[i];
		n++;
	}
	buffer[n] = '\0';
	*pktMaxNum = atoi(buffer);
	return 0;
}

int indexOfCharInString(char *str, int length, char ch){
	int i;
	for(i = 0; i<length; i++){
		if(str[i] == ch){return i;}
	}
	return -1;
}

/*simulate ACK lost; return 1 if ack lost; return 0 if not*/
int lostAck(float pro){
	float rnd;
	rnd = (float)rand() / (float)RAND_MAX;
	return rnd < pro ? 1 : 0;
}
