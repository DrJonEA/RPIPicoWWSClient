/* WSTLSTransBlock.h */

#ifndef _WSTLSTRANSBLOCK_H_
#define _WSTLSTRANSBLOCK_H_

#define TLS_TRANSPORT_WAIT 10000

#include "WSTransport.h"

extern "C" {
#include <FreeRTOS.h>
#include <task.h>
#include <semphr.h>

#include "lwip/ip4_addr.h"
#include "lwip/ip_addr.h"
#include "lwip/sockets.h"
#include "lwip/dns.h"

#include "wolfssl/ssl.h"

}


class WSTLSTransBlock :  public WSTransport{
public:
    WSTLSTransBlock();
    virtual ~WSTLSTransBlock();

    bool transConnect(const char * host, uint16_t port);
    int status();
    bool transClose();
    int32_t transSend(const void * pBuffer, size_t bytesToSend);
    int32_t transRead(void * pBuffer, size_t bytesToRecv);
    void setForceTLS12(bool state);

    /***
     * Set the root CA certificate(s) used to verify the server.
     * PEM format, null terminated. Several PEM blocks can be joined
     * in one string. Pass NULL to turn verification off (not safe).
     * The string must stay in memory while the transport is used.
     * @param pem - root CA certificate(s) in PEM format
     */
    void setRootCA(const char *pem);

    /***
     * Allow TLS without checking the server's certificate.
     * Encrypted, but the server could be anyone. For quick demos only.
     * Without a root CA, connections are unverified anyway.
     * @param state - true to allow unverified connections
     */
    void setInsecure(bool state);

    /***
     * TLS version negotiated on the last connection, e.g. "TLSv1.3"
     */
    const char * getTLSVersion();

    /***
     * Cipher suite negotiated on the last connection
     */
    const char * getTLSCipher();

private:
    bool transConnect();
    static void dnsCB(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
    void dnsFound(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
    static int IOSend(WOLFSSL* ssl, char* buff, int sz, void* ctx);
    static int IORecv(WOLFSSL* ssl, char* buff, int sz, void* ctx);

    int xSock = -1;
    uint16_t xPort=80;
    ip_addr_t xHost;
    char xHostName[80];
    SemaphoreHandle_t xHostDNSFound;

    WOLFSSL_CTX* pCtx = NULL;
    WOLFSSL* pSSL = NULL;
    bool xTLS12 = false;
    const char *pRootCA = NULL;
    bool xInsecure = false;
    char xTLSVersion[16] = "";
    char xTLSCipher[48] = "";
};

#endif /* _WSTLSTRANSBLOCK_H_ */
