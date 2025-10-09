/*
 * Request.cpp
 *
 *  Created on: 14 Sept 2023
 *      Author: jondurrant
 */

#include "Request.h"
#include "json-maker/json-maker.h"
#include "FreeRTOS.h"
#include "task.h"


Request::Request() {
	pAllocBuffer = pvPortMalloc(REQUEST_BUFFER_SIZE);
	pBuffer = (char *) pAllocBuffer;
	xBufferLen = REQUEST_BUFFER_SIZE;
	pTrans = &xSockTrans;
}

Request::Request(char * buffer, uint bufLen) {
	pAllocBuffer = NULL;
	pBuffer = (char *) buffer;
	xBufferLen = bufLen;
	pTrans = &xSockTrans;
}

Request::~Request() {
	if (pAllocBuffer != NULL){
		vPortFree(pAllocBuffer);
	}
	if (pUri != NULL){
		delete(pUri);
	}
}

void Request::freeMemory(){
	if (pUri != NULL){
		delete(pUri);
		pUri = NULL;
	}
}

void Request::setObserver(RequestObserver *obs){
	pObserver = obs;
}

bool Request::get(const char * url, std::map<std::string, std::string> *query){
	char path[REQUEST_MAX_PATH];
	const char payload[]="";
	bool res;

	createURL(path, REQUEST_MAX_PATH,  url, query);


	res = doRequest("GET", path, payload, 0);
	return res;

}

bool Request::createURL(
		char * target,
		size_t len,
		const char * url,
		std::map<std::string, std::string> *query){


	if (strlen(url) >= len)
		return false;

	strcpy(target,  url);
	if (query != NULL){
		int length=strlen(target);
		target[length++] = '?';

		std::map<std::string, std::string>::iterator it = query->begin();
		while (it != query->end()){
			int newLen = length + it->first.length() + it->second.length() + 2;
			if (newLen > len){
#ifdef REQUEST_DEBUG
				printf("HTTP Request path exceeded building query\n");
#endif
				return false;
			}
			strcpy(&target[length],  it->first.c_str());
			length +=  it->first.length();
			target[length++] = '=';
			length += urlEncode(&target[length],  it->second.c_str());

			it++;
			if ( it != query->end()){
				target[length++] = '&';
			} else {
				target[length++] = 0;
			}
		}
	}

	int l = strlen(target);
	if (l >= REQUEST_MAX_PATH){
		printf("URL is too long (%d): %s\n", l, target);
		return false;
	}
	return true;
}



bool Request::doRequest(const char * method, const char * url, const char * payload, uint payloadLen){
	char path[REQUEST_MAX_PATH];
	char authPlain[REQUEST_MAX_PATH];
	char auth64[REQUEST_MAX_PATH];

	freeMemory();
	pUri = new uri (url);

	int serverPort = pUri->get_port();

	if (pUri->get_scheme().compare("http") == 0 ){
		pTrans =&xSockTrans;
		if (serverPort == 0){
			serverPort = 80;
		}
#ifdef REQUEST_DEBUG
		printf("HTTP on Port %d\n", serverPort);
#endif
	} else if (pUri->get_scheme().compare("https") == 0 ){
		pTrans =&xTLSTrans;
		xTLSTrans.setForceTLS12(xTLS12);
		if (serverPort == 0){
			serverPort = 443;
		}
#ifdef REQUEST_DEBUG
		printf("HTTPS on Port %d\n", serverPort);
#endif
	}  else {
		printf("Schema not implemented %s\n",
				pUri->get_scheme().c_str()
				);
		return false;
	}


	if (!pTrans->transConnect(pUri->get_host().c_str(), serverPort)){
		printf("Socket Connect Failed\r\n");
		pTrans->transClose();
		return false;
	}

	xNetworkContext.pTransport = pTrans;
	xTransportInterface.pNetworkContext = &xNetworkContext;
	xTransportInterface.send = Transport::staticTransSend;
	xTransportInterface.recv = Transport::staticTransRead;

	  /* Initialize all HTTP Client library API structs to 0. */
	( void ) memset( &xRequestInfo, 0, sizeof( xRequestInfo ) );
	( void ) memset( &xResponse, 0, sizeof( xResponse ) );
	( void ) memset( &xRequestHeaders, 0, sizeof( xRequestHeaders ) );


	if (pUri->get_query().length() == 0){
		sprintf(path, "/%s", pUri->get_path().c_str() );
	} else {
		sprintf(path, "/%s?%s", pUri->get_path().c_str(), pUri->get_query().c_str() );
	}

	/* Initialize the request object. */
	xRequestInfo.pHost = pUri->get_host().c_str();
	xRequestInfo.hostLen = pUri->get_host().length();
	xRequestInfo.pMethod =  method;
	xRequestInfo.methodLen = strlen(method);
	xRequestInfo.pPath = path;
	xRequestInfo.pathLen = strlen(path);
	xRequestInfo.reqFlags = false;

	/* Set the buffer used for storing request headers. */
	xRequestHeaders.pBuffer = (unsigned char *)pBuffer;
	xRequestHeaders.bufferLen = xBufferLen;


	xHTTPStatus = HTTPClient_InitializeRequestHeaders( &xRequestHeaders,
													   &xRequestInfo );

	if (xHTTPStatus == HTTPSuccess){
		if (payloadLen > 0){
			xHTTPStatus = HTTPClient_AddHeader(
					&xRequestHeaders,
					"Content-Type",
					strlen("Content-Type") ,
					"application/json",
					strlen("application/json")
				   );
		}
	}

	if( xHTTPStatus == HTTPSuccess ) {
			/* Initialize the response object. The same buffer used for storing
			 * request headers is reused here. */
			xResponse.pBuffer = (unsigned char *)pBuffer;
			xResponse.bufferLen = xBufferLen;
			xResponse.getTime = Transport::getTime;

#ifdef REQUEST_DEBUG
			if (payloadLen != 0){
				printf("Method %.*s Host %.*s:%d Path %.*s Payload %.*s\n",
						xRequestInfo.methodLen,
						xRequestInfo.pMethod,
						xRequestInfo.hostLen,
						xRequestInfo.pHost,
						serverPort,
						xRequestInfo.pathLen ,
						xRequestInfo.pPath,
						payloadLen,
						payload
						);
				 printf( "Request Headers:\n%.*s\n",
				                   ( int32_t ) xRequestHeaders.headersLen,
				                   ( char * ) xRequestHeaders.pBuffer  );
			} else {
				printf("Method %.*s Host %.*s:%d Path %.*s \n",
						xRequestInfo.methodLen,
						xRequestInfo.pMethod,
						xRequestInfo.hostLen,
						xRequestInfo.pHost,
						serverPort,
						xRequestInfo.pathLen ,
						xRequestInfo.pPath
						);
			}
#endif

			//printf("Request Buffer %d\n",  xBufferLen);
			/* Send the request and receive the response. */
			xHTTPStatus = HTTPClient_Send( &xTransportInterface,
										   &xRequestHeaders,
										   ( uint8_t * ) payload,
										  payloadLen,
										   &xResponse,
										   0 );

		} else {
			printf ( "Failed to initialize HTTP request headers: Error=%s.",
						HTTPClient_strerror( xHTTPStatus ) );
		}


	if (pObserver != NULL){
		pObserver->requestComplete(this);
	}


	pTrans->transClose();

	if (xHTTPStatus == HTTPSuccess){
		return true;
	}
	return false;

}



bool Request::post(
		const char * url,
		std::map<std::string, std::string> *param,
		std::map<std::string, std::string> *query){
	char payload[REQUEST_MAX_PATH] = "";
	int length=0;
	bool res;
	char path[REQUEST_MAX_PATH];

	createURL(path, REQUEST_MAX_PATH,  url, query);

	if (param != NULL){

			size_t remLen = REQUEST_MAX_PATH;
		   char* s = json_objOpen( payload,  NULL,  &remLen );

			std::map<std::string, std::string>::iterator it = param->begin();
			while (it != param->end()){

				s = json_str( s,  it->first.c_str(), it->second.c_str(), &remLen );

				it++;
			}
			s = json_objClose( s, &remLen );
			s = json_end( s, &remLen );

#ifdef REQUEST_DEBUG
			printf("Payload %s\n", payload);
#endif
			length = strlen(payload);
	}

	res = doRequest("POST", path, payload, length);
	return res;
}

bool Request::postJSON(const char * url,  JSONSerialisable *json){
	char *payload;
	int length=0;
	bool res;


	if (json != NULL){

			payload = json->json();
			length = strlen(payload);
	}

	res = doRequest("POST", url, payload, length);
	return res;
}

int Request::getStatusCode(){
	return xResponse.statusCode;
}

const char * Request::getHeader(){
	return (const char *)xResponse.pHeaders;
}

int Request::getHeaderLen(){
	return xResponse.headersLen;
}


//const char * Request::getPayload(){
//	return (const char *)xResponse.pBody;
char * Request::getPayload(){
	return (char *)xResponse.pBody;
}

int  Request::getPayloadLen(){
	return xResponse.bodyLen;
}


const char * Request::getUriChar(){
	return pUri->to_string().c_str();
}



int Request::urlEncode(char *target, const char * source){
	int length = strlen(source);
	int targetInd = 0;

	for (int i=0; i < length; i++){
		char c = source[i];
		if (isalnum(c) || c == '-' || c == '_' || c == '.' || c == '~') {
			target[targetInd++] = c;
		} else {
			if (c < 16){
				sprintf(&target[targetInd], "%%0%X", c);
			} else {
				sprintf(&target[targetInd], "%%%X", c);
			}
			targetInd+=3;
		}
	}
	return targetInd;
}


void Request::setForceTLS12(bool state){
	xTLS12 = state;
}
