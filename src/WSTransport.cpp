/* WSTransport.cpp */

#include "WSTransport.h"
#include <stdlib.h>
#include "pico/stdlib.h"
#include <errno.h>

#include <stdio.h>

WSTransport::WSTransport() {
}

WSTransport::~WSTransport() {
}


/***
 * Print the buffer in hex and plain text for debugging
 */
void WSTransport::debugPrintBuffer(const char *title, const void * pBuffer, size_t bytes){
    size_t count =0;
    size_t lineEnd=0;
    const uint8_t *pBuf = (uint8_t *)pBuffer;

    printf("DEBUG: %s of size %d\n", title, bytes);

    while (count < bytes){
        lineEnd = count + 25;
        if (lineEnd > bytes){
            lineEnd = bytes;
        }

        //Print HEX DUMP
        for (size_t i=count; i < lineEnd; i++){
            if (pBuf[i] <= 0x0F){
                printf("0%X ", pBuf[i]);
            } else {
                printf("%X ", pBuf[i]);
            }
        }

        //Pad for short lines
        size_t pad = (25 - (lineEnd - count)) * 3;
        for (size_t i=0; i < pad; i++){
            printf(" ");
        }

        //Print Plain Text
        for (size_t i=count; i < lineEnd; i++){
            if ((pBuf[i] >= 0x20) && (pBuf[i] <= 0x7e)){
                printf("%c", pBuf[i]);
            } else {
                printf(".");
            }
        }

        printf("\n");

        count = lineEnd;

    }
}

/***
 * Callback C function to support the NetworkContext on coreHTTP
 * @param pNetworkContext
 * @param pBuffer
 * @param bytesToRecv
 * @return
 */
int32_t WSTransport:: staticTransRead(
            NetworkContext_t * pNetworkContext,
         void * pBuffer,
         size_t bytesToRecv ){
    WSTransport *t = (WSTransport *) pNetworkContext->pTransport;
    return t->transRead(pBuffer,  bytesToRecv);
}

/***
* Callback C function to support the NetworkContext on coreHTTP
* @param pNetworkContext
* @param pBuffer
* @param bytesToSend
* @return
*/
int32_t WSTransport::staticTransSend(
        NetworkContext_t * pNetworkContext,
    const void * pBuffer,
    size_t bytesToSend ){
    WSTransport *t = (WSTransport *) pNetworkContext->pTransport;
    return t->transSend(pBuffer,  bytesToSend);
}


/***
 * Application provided function to query the current time in milliseconds.
 * @return time in MS
 */
uint32_t WSTransport::getTime(){

    return  to_ms_since_boot(get_absolute_time ());

}

