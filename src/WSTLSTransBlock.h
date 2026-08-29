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

private:
    bool transConnect();
    static void dnsCB(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
    void dnsFound(const char *name, const ip_addr_t *ipaddr, void *callback_arg);
    static int IOSend(WOLFSSL* ssl, char* buff, int sz, void* ctx);
    static int IORecv(WOLFSSL* ssl, char* buff, int sz, void* ctx);

    int xSock = 0;
    uint16_t xPort=80;
    ip_addr_t xHost;
    char xHostName[80];
    SemaphoreHandle_t xHostDNSFound;

    WOLFSSL_CTX* pCtx;
    WOLFSSL* pSSL;
    bool xTLS12 = false;
};

#endif /* _WSTLSTRANSBLOCK_H_ */
