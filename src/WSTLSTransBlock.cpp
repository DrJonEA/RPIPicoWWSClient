#define LIBRARY_LOG_LEVEL LOG_DEBUG

#include "WSTLSTransBlock.h"
#include <stdlib.h>
#include "pico/stdlib.h"
#include <errno.h>

#include <stdio.h>
#include "lwip/api.h"

WSTLSTransBlock::WSTLSTransBlock() {
    xHostDNSFound = xSemaphoreCreateBinary(  );

    //wolfSSL_Init();/* Initialize wolfSSL */
    //wolfSSL_Debugging_ON();

}

WSTLSTransBlock::~WSTLSTransBlock() {
    //wolfSSL_Cleanup();
    vSemaphoreDelete(xHostDNSFound);
}

int32_t WSTLSTransBlock::transSend(const void * pBuffer, size_t bytesToSend){
    uint32_t dataOut;

    dataOut = wolfSSL_write(pSSL, (uint8_t *)pBuffer, bytesToSend);
    if (dataOut != bytesToSend){
        LogError(("Send failed %d\n", dataOut));
    }
    return dataOut;
}

int32_t WSTLSTransBlock::transRead(void * pBuffer, size_t bytesToRecv){
    int32_t dataIn=0;
    uint32_t bytesAv=0;
    int ret;
    int buffered;

    if (true){
        if (status() >= 0){

            ret = ioctl (xSock,FIONREAD,&bytesAv);
            buffered = wolfSSL_pending(pSSL);

            if ((bytesAv ==0) && (buffered == 0)){
                return 0;
            }
        }
    }


    dataIn = wolfSSL_read(pSSL, (uint8_t *)pBuffer, bytesToRecv);

    return dataIn;
}


bool WSTLSTransBlock::transConnect(const char * host, uint16_t port){
    strncpy(xHostName, host, sizeof(xHostName) - 1);
    xHostName[sizeof(xHostName) - 1] = 0;
    xPort = port;

    // netconn_gethostbyname is the thread-safe, blocking lookup for use
    // from an application task. Calling dns_gethostbyname directly from
    // here skipped lwIP's core lock, and waited the full timeout even when
    // the lookup had already failed (seen as "DNS Timeout ... -6").
    uint32_t now = to_ms_since_boot (get_absolute_time ());
    err_t res = netconn_gethostbyname(xHostName, &xHost);
    if (res != ERR_OK){
        printf("DNS lookup failed for %s: err %d after %u ms\n",
                xHostName, res,
                to_ms_since_boot (get_absolute_time ()) - now
                );
        return false;
    }

    return transConnect();
}


bool WSTLSTransBlock::transConnect(){
    struct sockaddr_in serv_addr;
    int                ret = 0, err;
    char buffer[80];

    //Secure by default: refuse unless we know who to trust,
    //or have been told explicitly not to check
    if ((pRootCA == NULL) && (!xInsecure)){
        printf("TLS refused: no root CA set. Call setRootCA(), or setInsecure(true) for an unverified demo\n");
        return false;
    }

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

    wolfSSL_SetIORecv(pCtx, WSTLSTransBlock::IORecv);
    wolfSSL_SetIOSend(pCtx, WSTLSTransBlock::IOSend);

    xTLSVersion[0] = 0;
    xTLSCipher[0] = 0;

    if (pRootCA != NULL){
        //Trust only the root CA we were given, and check the server's
        //certificate chains up to it
        ret = wolfSSL_CTX_load_verify_buffer(pCtx,
                (const unsigned char *)pRootCA, strlen(pRootCA),
                WOLFSSL_FILETYPE_PEM);
        if (ret != WOLFSSL_SUCCESS){
            LogError(("Failed to load root CA %d\n", ret));
            return false;
        }
        wolfSSL_CTX_set_verify(pCtx, WOLFSSL_VERIFY_PEER, NULL);
    } else {
        //setInsecure(true): encrypted, but we have no idea who we are talking to
        printf("WARNING: TLS without server verification\n");
        wolfSSL_CTX_set_verify(pCtx, WOLFSSL_VERIFY_NONE, NULL);
    }

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

    if (pRootCA != NULL){
        //The certificate must be for the host we asked for
        if (wolfSSL_check_domain_name(pSSL, xHostName) != WOLFSSL_SUCCESS){
            LogError(("Failed to set domain name check\n"));
            return false;
        }
    }

    ret = wolfSSL_connect(pSSL);
    err = wolfSSL_get_error(pSSL, ret);

    if (ret != WOLFSSL_SUCCESS){
        LogError(("err %d: failed to connect to wolfSSL %d\n", err, ret));

        wolfSSL_ERR_error_string(err, buffer);
        printf("err = %d, %s\n", err, buffer);
        return false;
    }

    strncpy(xTLSVersion, wolfSSL_get_version(pSSL), sizeof(xTLSVersion) - 1);
    strncpy(xTLSCipher, wolfSSL_get_cipher_name(pSSL), sizeof(xTLSCipher) - 1);

    return true;
}

int WSTLSTransBlock::status(){
    int error = 0;
    socklen_t len = sizeof (error);
    int retval = getsockopt (xSock, SOL_SOCKET, SO_ERROR, &error, &len);
    return error;
}

bool WSTLSTransBlock::transClose(){
    //Null the pointers so a failed connect can't free them twice
    if (pSSL != NULL){
        wolfSSL_free(pSSL);
        pSSL = NULL;
    }
    if (pCtx != NULL){
        wolfSSL_CTX_free(pCtx);
        pCtx = NULL;
    }
    wolfSSL_Cleanup();
    if (xSock >= 0){
        closesocket(xSock);
        xSock = -1;
    }
    return true;
}

void WSTLSTransBlock::dnsCB(const char *name, const ip_addr_t *ipaddr, void *callback_arg){
    WSTLSTransBlock *self = (WSTLSTransBlock *) callback_arg;
    self->dnsFound(name, ipaddr, callback_arg);
}

void WSTLSTransBlock::dnsFound(const char *name, const ip_addr_t *ipaddr, void *callback_arg){
    memcpy(&xHost, ipaddr, sizeof(xHost));

    LogInfo(("DNS Found %s copied to xHost %s\n", ipaddr_ntoa(ipaddr), ipaddr_ntoa(&xHost)));
    xSemaphoreGiveFromISR(xHostDNSFound, NULL );
}


int WSTLSTransBlock::IOSend(WOLFSSL* ssl, char* buff, int sz, void* ctx){
    int sockfd = *(int*)ctx;
    int sent;

    if ((sent = send(sockfd, buff, sz, 0)) == -1) {
        int err = wolfSSL_get_error(ssl, errno);
        char errorString[80];
        wolfSSL_ERR_error_string(err, errorString);
        LogError((errorString));

        switch (errno) {
        #if EAGAIN != EWOULDBLOCK
        case EAGAIN:
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

    return sent;
}

int WSTLSTransBlock::IORecv(WOLFSSL* ssl, char* buff, int sz, void* ctx){
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

    if ((recvd = recv(sockfd, buff, sz, 0)) == -1) {
        int err = wolfSSL_get_error(ssl, errno);
        LogError(("IO RECEIVE ERROR: errno=%d sslErr=%d", errno, err));
        switch (errno) {
        #if EAGAIN != EWOULDBLOCK
        case EAGAIN:
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

    return recvd;
}

void WSTLSTransBlock::setForceTLS12(bool state){
    xTLS12 = state;
}

void WSTLSTransBlock::setRootCA(const char *pem){
    pRootCA = pem;
}

void WSTLSTransBlock::setInsecure(bool state){
    xInsecure = state;
}

const char * WSTLSTransBlock::getTLSVersion(){
    return xTLSVersion;
}

const char * WSTLSTransBlock::getTLSCipher(){
    return xTLSCipher;
}
