/*
 * TLSTransBlock.cpp
 *
 *  Created on: 3 Apr 2023
 *      Author: jondurrant
 */

#define LIBRARY_LOG_LEVEL LOG_DEBUG

#include "TLSTransBlock.h"
#include <stdlib.h>
#include "pico/stdlib.h"
#include <errno.h>

#include <stdio.h>

TLSTransBlock::TLSTransBlock() {
	xHostDNSFound = xSemaphoreCreateBinary(  );

	//wolfSSL_Init();/* Initialize wolfSSL */
	//wolfSSL_Debugging_ON();

}

TLSTransBlock::~TLSTransBlock() {
	//wolfSSL_Cleanup();
	vSemaphoreDelete(xHostDNSFound);
}

/***
 * Send bytes through socket
 * @param pBuffer - Buffer to send from
 * @param bytesToSend - number of bytes to send
 * @return number of bytes sent
 */
int32_t TLSTransBlock::transSend(const void * pBuffer, size_t bytesToSend){
	uint32_t dataOut;

	//debugPrintBuffer("TLSTransBlock::transSend", pBuffer, bytesToSend);

	//dataOut = write(xSock,(uint8_t *)pBuffer, bytesToSend);
	dataOut = wolfSSL_write(pSSL, (uint8_t *)pBuffer, bytesToSend);
	if (dataOut != bytesToSend){
		LogError(("Send failed %d\n", dataOut));
	}
	return dataOut;
}


/***
 * Send
 * @param pBuffer
 * @param bytesToRecv
 * @return
 */
int32_t TLSTransBlock::transRead(void * pBuffer, size_t bytesToRecv){
	int32_t dataIn=0;
	uint32_t bytesAv=0;
	int ret;
	int buffered;


	//Do non blocking check on a 1 byte read
	//if (bytesToRecv == 1){
	if (true){
		//Only check if socket is OK
		if (status() >= 0){

			ret = ioctl (xSock,FIONREAD,&bytesAv);
			buffered = wolfSSL_pending(pSSL);

			if ((bytesAv ==0) && (buffered == 0)){
				return 0;
			}
		}
	}


	dataIn = wolfSSL_read(pSSL, (uint8_t *)pBuffer, bytesToRecv);

	//printf("TLSTransBlock::transRead(%d) = %d\n", bytesToRecv, dataIn);
	//debugPrintBuffer("TLSTransBlock::transRead", pBuffer, dataIn);

	return dataIn;
}





/***
 * Connect to remote TLS Socket
 * @param host - Host address
 * @param port - Port number
 * @return true on success
 */
bool TLSTransBlock::transConnect(const char * host, uint16_t port){
	err_t res = dns_gethostbyname(host, &xHost, TLSTransBlock::dnsCB, this);

	strcpy(xHostName, host);
	xPort = port;

	if (res == ERR_OK){
		//Address in table already no lookup required
		//printf("DNS Found %s\n", ipaddr_ntoa(&xHost));
	} else {
		//Lookup is being performed
		uint32_t now = to_ms_since_boot (get_absolute_time ());
		if (xSemaphoreTake(xHostDNSFound, TLS_TRANSPORT_WAIT) != pdTRUE){
			printf("DNS Timeout on Connect: %s, %d After %u ms",
					host, res,
					to_ms_since_boot (get_absolute_time ()) - now
					);
			return false;
		}
	}

	return transConnect();
}


/***
 * Connect to socket previously stored ip address and port number
 * @return true if socket openned
 */
bool TLSTransBlock::transConnect(){
	struct sockaddr_in serv_addr;
	int                ret, err;
	char buffer[80];


	xSock = socket(AF_INET, SOCK_STREAM, 0);
	if (xSock < 0){
		LogError(("ERROR opening socket\n"));
		return false;
	}


	/* Create the WOLFSSL_CTX */
#ifdef WOLFSSL_TLS13
	if (!xTLS12){
		pCtx = wolfSSL_CTX_new(wolfTLSv1_3_client_method());
		wolfSSL_CTX_SetMinVersion(pCtx, WOLFSSL_TLSV1_2);
	} else {
		pCtx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
	}
	if ( pCtx == NULL){
		LogError(("wolfSSL_CTX_new error.\n"));
	}
#else
	pCtx = wolfSSL_CTX_new(wolfTLSv1_2_client_method());
	if ( pCtx == NULL){
		LogError(("wolfSSL_CTX_new error.\n"));
	}
#endif

#ifdef HAVE_SNI
	if ( (wolfSSL_CTX_UseSNI(pCtx, WOLFSSL_SNI_HOST_NAME, xHostName, strlen(xHostName))) != WOLFSSL_SUCCESS) {
	    printf("Setting host name failed with error condition: %d and reason %s\n", ret, wolfSSL_ERR_error_string(ret, buffer));
	}
#endif

	wolfSSL_SetIORecv(pCtx, TLSTransBlock::IORecv);
	wolfSSL_SetIOSend(pCtx, TLSTransBlock::IOSend);
	wolfSSL_CTX_set_verify(pCtx, SSL_VERIFY_NONE, NULL);

	memset(&serv_addr,0,sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(xPort);
	memcpy(&serv_addr.sin_addr.s_addr, &xHost, sizeof(xHost));

	int res = connect(xSock,(struct sockaddr *)&serv_addr,sizeof(serv_addr));
	if (res < 0){
		char *s = ipaddr_ntoa(&xHost);
		LogError(("ERROR connecting %d to %s port %d\n",res, s, xPort));
		return false;
	}

	/* Create WOLFSSL object */
	if( (pSSL = wolfSSL_new(pCtx)) == NULL) {
	    LogError(("wolfSSL_new error.\n"));
	    return false;
	}

	if ((ret = wolfSSL_set_fd(pSSL, xSock)) != WOLFSSL_SUCCESS){
		LogError(("Failed to set the FD"));
		return false;
	}



	ret = wolfSSL_connect(pSSL);
	err = wolfSSL_get_error(pSSL, ret);

    if (ret != WOLFSSL_SUCCESS){
        LogError(("err %d: failed to connect to wolfSSL %d\n", err, ret));

        wolfSSL_ERR_error_string(err, buffer);
        printf("err = %d, %s\n", err, buffer);
       // printf("err %d: failed to connect to wolfSSL %d\n", err, ret);
        return false;
    }

	//LogInfo(("Connect success\n"));
	return true;
}


/***
 * Get status of the socket
 * @return int <0 is error
 */
int TLSTransBlock::status(){
	int error = 0;
	socklen_t len = sizeof (error);
	int retval = getsockopt (xSock, SOL_SOCKET, SO_ERROR, &error, &len);
	return error;
}

/***
 * Close the socket
 * @return true on success
 */
bool TLSTransBlock::transClose(){
	wolfSSL_free(pSSL);
	wolfSSL_CTX_free(pCtx);
	wolfSSL_Cleanup();
	closesocket(xSock);
	return true;
}

/***
 * Call back function for the DNS lookup
 * @param name - server name
 * @param ipaddr - resulting IP address
 * @param callback_arg - poiter to TLSTransBlock object
 */
void TLSTransBlock::dnsCB(const char *name, const ip_addr_t *ipaddr, void *callback_arg){
	TLSTransBlock *self = (TLSTransBlock *) callback_arg;
	self->dnsFound(name, ipaddr, callback_arg);
}


/***
 * Called when DNS is returned
 * @param name - server name
 * @param ipaddr - ip address of server
 * @param callback_arg - this TLSTransBlock object
 */
void TLSTransBlock::dnsFound(const char *name, const ip_addr_t *ipaddr, void *callback_arg){
	memcpy(&xHost, ipaddr, sizeof(xHost));

	LogInfo(("DNS Found %s copied to xHost %s\n", ipaddr_ntoa(ipaddr), ipaddr_ntoa(&xHost)));
	xSemaphoreGiveFromISR(xHostDNSFound, NULL );
}





int TLSTransBlock::IOSend(WOLFSSL* ssl, char* buff, int sz, void* ctx){
    /* By default, ctx will be a pointer to the file descriptor to write to.
     * This can be changed by calling wolfSSL_SetIOWriteCtx(). */
    int sockfd = *(int*)ctx;
    int sent;

    //LogInfo(("Sending %d", sz));

    /* Receive message from socket */
    if ((sent = send(sockfd, buff, sz, 0)) == -1) {
        /* error encountered. Be responsible and report it in wolfSSL terms */

    	LogError(("Send Error %d %d", sent, errno));

        int err = wolfSSL_get_error(ssl, errno);
        char errorString[80];
        wolfSSL_ERR_error_string(err, errorString);
    	LogError((errorString));

        switch (errno) {
        #if EAGAIN != EWOULDBLOCK
        case EAGAIN: /* EAGAIN == EWOULDBLOCK on some systems, but not others */
        #endif
        case EWOULDBLOCK:
            LogError(( "would block\n"));
            return WOLFSSL_CBIO_ERR_WANT_WRITE;
        case ECONNRESET:
            LogError(("connection reset\n"));
            return WOLFSSL_CBIO_ERR_CONN_RST;
        case EINTR:
            LogError(("socket interrupted\n"));
            return WOLFSSL_CBIO_ERR_ISR;
        case EPIPE:
            LogError(("socket EPIPE\n"));
            return WOLFSSL_CBIO_ERR_CONN_CLOSE;
        case EINPROGRESS:
        	LogInfo(("socket EINPROGRESS\n"));
			return 0;
        default:
            LogError(("general error\n"));
            return WOLFSSL_CBIO_ERR_GENERAL;
        }

    }


    /* successful send */
    //LogInfo(("my_IOSend: sent %d bytes to %d\n", sz, sockfd));
    return sent;
}

int TLSTransBlock::IORecv(WOLFSSL* ssl, char* buff, int sz, void* ctx){
    /* By default, ctx will be a pointer to the file descriptor to read from.
     * This can be changed by calling wolfSSL_SetIOReadCtx(). */
    int sockfd = *(int*)ctx;
    int recvd;



    uint32_t bytesAv=0;
	int ret;
	uint32_t start = to_ms_since_boot(get_absolute_time());

	while (bytesAv == 0){
		ret = ioctl (sockfd,FIONREAD,&bytesAv);
		if (bytesAv == 0){
			uint32_t now = to_ms_since_boot(get_absolute_time());
			if ((now - start) > 250){
				printf("TIMEOUT\n");
				return 0;
			}
			vTaskDelay(1);
		}
	}





    /* Receive message from socket */
    if ((recvd = recv(sockfd, buff, sz, 0)) == -1) {
        /* error encountered. Be responsible and report it in wolfSSL terms */

        int err = wolfSSL_get_error(ssl, errno);
        LogError(("IO RECEIVE ERROR: errno=%d sslErr=%d", errno, err));
        switch (errno) {
        #if EAGAIN != EWOULDBLOCK
        case EAGAIN: /* EAGAIN == EWOULDBLOCK on some systems, but not others */
        #endif
        case EWOULDBLOCK:
            if (!wolfSSL_dtls(ssl) || wolfSSL_get_using_nonblock(ssl)) {
                LogError(("would block\n"));
                return WOLFSSL_CBIO_ERR_WANT_READ;
            }
            else {
                LogError(("socket timeout\n"));
                return WOLFSSL_CBIO_ERR_TIMEOUT;
            }
        case ECONNRESET:
            LogError(("connection reset\n"));
            return WOLFSSL_CBIO_ERR_CONN_RST;
        case EINTR:
            LogError(("socket interrupted\n"));
            return WOLFSSL_CBIO_ERR_ISR;
        case ECONNREFUSED:
            LogError(("connection refused\n"));
            return WOLFSSL_CBIO_ERR_WANT_READ;
        case ECONNABORTED:
            LogError(("connection aborted\n"));
            return WOLFSSL_CBIO_ERR_CONN_CLOSE;
        case EINPROGRESS:
        	LogInfo(("socket EINPROGRESS\n"));
			return 0;

        default:
            LogError(("general error\n"));
            return WOLFSSL_CBIO_ERR_GENERAL;
        }


    }
    else if (recvd == 0) {
    	int error = 0;
		socklen_t len = sizeof (error);
		int retval = getsockopt (sockfd, SOL_SOCKET, SO_ERROR, &error, &len);
        LogInfo(("Connection closed. Status %d\n", error));
        return WOLFSSL_CBIO_ERR_CONN_CLOSE;
    }


    /* successful receive */
    //LogInfo(("Received %d bytes from %d\n", sz, sockfd));
    return recvd;
}


void TLSTransBlock::setForceTLS12(bool state){
	xTLS12 = state;
}

