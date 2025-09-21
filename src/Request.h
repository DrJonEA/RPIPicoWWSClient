/*
 * Request.h
 *
 *  Created on: 14 Sept 2023
 *      Author: jondurrant
 */

#ifndef HTTPWS_GETTIMELWIPBM_SRC_REQUEST_H_
#define HTTPWS_GETTIMELWIPBM_SRC_REQUEST_H_

#include "pico/stdlib.h"
#include "RequestObserver.h"
#include <map>
#include <cstring>
#include <uri.hh>
#include "core_http_client.h"
#include "TCPTransport.h"
#include "TLSTransBlock.h"
#include "JSONSerialisable.h"


#ifndef REQUEST_BUFFER_SIZE
#define REQUEST_BUFFER_SIZE 256
#endif

#ifndef REQUEST_MAX_PATH
#define REQUEST_MAX_PATH 256
#endif

#define REQUEST_BUF_OVERFLOW -200

//#define REQUEST_DEBUG
#ifdef REQUEST_DEBUG
#endif

class Request {
public:
	/***
	 * Constructor will allocate own buffer.
	 */
	Request();

	/***
	 * Constructor with buffer for working space
	 * @param buffer - Buffer area
	 * @param bufLen - Length of buffer
	 */
	Request(char * buffer, uint bufLen);

	virtual ~Request();

	/***
	 * Free up any memory allocated during process
	 */
	void freeMemory();

	/***
	 * Set Observer to get notified of response
	 * @param obs
	 */
	void setObserver(RequestObserver *obs);

	/***
	 * Undertake an HTTP Get
	 * @param url - URL to connect to
	 * @param query - Query to send as name/value pairs
	 * @return true if successful
	 */
	bool get(const char * url, std::map<std::string, std::string> *query = NULL);

	/***
	 * Undertake an HTTP Post
	 * @param url - URL to connect to
	 * @param query - Query to send as name/value pairs
	 * @return true if successful
	 */
	bool post(const char * url,  std::map<std::string, std::string> *query = NULL);

	/***
	 * Undertake an HTTP Post
	 * @param url - URL to connect to
	 * @param json - JSON data to send
	 * @return true if successful
	 */
	bool postJSON(const char * url,  JSONSerialisable *json);

	/**
	 * Get the HTTP result code
	 * @return 200 for sucess
	 */
	int getStatusCode();

	/***
	 * Get a pointer to the start of the headers.
	 * Though char * they may not be null terminated.
	 * @return
	 */
	const char * getHeader();

	/***
	 * Get the length of the HTTP Headers
	 * @return
	 */
	int getHeaderLen();

	/***
	 * Get the payload start location. It may not be null terminatqed
	 * Use getPayloadLen to identify length
	 * @return
	 */
	const char * getPayload();

	/***
	 * Returns the length of the payload returned from the request
	 * @return
	 */
	int getPayloadLen();

	/***
	 * Get the URI that will be connected to as a char * null terminated
	 * @return - URL
	 */
	const char * getUriChar();


	/***
	 * If set to true this forces TLS1.2 even if TLS1.3 is compiled in
	 * I've needed to do this for some sites.
	 * @param state
	 */
	void setForceTLS12(bool state);

protected:
	/***
	 * Issue an HTTP Request
	 * @param method - Request type GET or POST
	 * @param url - URL to conect to
	 * @param payload - Payload to send if a POSt
	 * @param payloadLen - Payload length
	 * @return true if sucessful
	 */
	bool doRequest(const char * method, const char * url, const char * payload, uint payloadLen);

    /***
     * Encode string using URL encoding rules
     * Note does not null terminate the target
     * @param target - target location to write to
     * @param source - source string (null terminated)
     * @return number of characters written to target
     */
    int urlEncode(char *target, const char * source);


	void *pAllocBuffer = NULL;
	char * pBuffer = NULL;
	uint xBufferLen = 0;
	RequestObserver *pObserver = NULL;

	uri *pUri = NULL;

	TransportInterface_t xTransportInterface;
	NetworkContext_t xNetworkContext;
	HTTPRequestInfo_t xRequestInfo;
	HTTPResponse_t xResponse;
	HTTPRequestHeaders_t xRequestHeaders;
	HTTPStatus_t xHTTPStatus = HTTPSuccess;

	TCPTransport xSockTrans;
	TLSTransBlock xTLSTrans;
	Transport *pTrans;
	bool xTLS12 = false;

};

#endif /* HTTPWS_GETTIMELWIPBM_SRC_REQUEST_H_ */
