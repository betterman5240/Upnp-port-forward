/*
 * UPNPNAT.cpp
 *
 *  Created on: May 18, 2015
 *      Author: ryan
 */
#include <iostream>
#include <string.h>
#include "UPNPNAT.h"
#include "xmlParser.h"
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>

 #include <sys/ioctl.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SOCKET_ERROR -1
using namespace std;
UPNPNAT::UPNPNAT() {
	// TODO Auto-generated constructor stub

}

UPNPNAT::~UPNPNAT() {
	// TODO Auto-generated destructor stub
}





#define MAX_BUFF_SIZE 102400

static bool parseUrl(const char* url,std::string& host,unsigned short* port,std::string& path)
{
	std::string str_url=url;

	std::string::size_type pos1,pos2,pos3;
	pos1=str_url.find("://");
	if(pos1==std::string::npos)
	{
		return false;
	}
	pos1=pos1+3;

	pos2=str_url.find(":",pos1);
	if(pos2==std::string::npos)
	{
		*port=80;
		pos3=str_url.find("/",pos1);
		if(pos3==std::string::npos)
		{
			return false;
		}

		host=str_url.substr(pos1,pos3-pos1);
	}
	else
	{
		host=str_url.substr(pos1,pos2-pos1);
		pos3=str_url.find("/",pos1);
		if(pos3==std::string::npos)
		{
			return false;
		}

		std::string str_port=str_url.substr(pos2+1,pos3-pos2-1);
		*port=(unsigned short)atoi(str_port.c_str());
	}

	if(pos3+1>=str_url.size())
	{
		path="/";
	}
	else
	{
		path=str_url.substr(pos3,str_url.size());
	}

	return true;
}


/******************************************************************
** Discovery Defines                                                 *
*******************************************************************/
#define HTTPMU_HOST_ADDRESS "239.255.255.250"
#define HTTPMU_HOST_PORT 1900
#define SEARCH_REQUEST_STRING "M-SEARCH * HTTP/1.1\r\n" \
"Host: 239.255.255.250:1900\r\n"\
"Man: \"ssdp:discover\"\r\n"\
"ST:upnp:rootdevice\r\n"\
"MX:3\r\n"\
"\r\n"
#define HTTP_OK "200 OK"
#define DEFAULT_HTTP_PORT 80


/******************************************************************
** Device and Service  Defines                                                 *
*******************************************************************/

#define DEVICE_TYPE_1	"urn:schemas-upnp-org:device:InternetGatewayDevice:1"
#define DEVICE_TYPE_2	"urn:schemas-upnp-org:device:WANDevice:1"
#define DEVICE_TYPE_3	"urn:schemas-upnp-org:device:WANConnectionDevice:1"

#define SERVICE_WANIP	"urn:schemas-upnp-org:service:WANIPConnection:1"
#define SERVICE_WANPPP	"urn:schemas-upnp-org:service:WANPPPConnection:1"


/******************************************************************
** Action Defines                                                 *
*******************************************************************/
#define HTTP_HEADER_ACTION "POST %s HTTP/1.1\r\n"                         \
                           "HOST: %s:%u\r\n"                                  \
                           "SOAPACTION:\"%s#%s\"\r\n"                           \
                           "CONTENT-TYPE: text/xml ; charset=\"utf-8\"\r\n"\
                           "Content-Length: %d \r\n\r\n"

#define SOAP_ACTION  "<?xml version=\"1.0\" encoding=\"utf-8\"?>\r\n"     \
                     "<s:Envelope xmlns:s="                               \
                     "\"http://schemas.xmlsoap.org/soap/envelope/\" "     \
                     "s:encodingStyle="                                   \
                     "\"http://schemas.xmlsoap.org/soap/encoding/\">\r\n" \
                     "<s:Body>\r\n"                                       \
                     "<u:%s xmlns:u=\"%s\">\r\n%s"         \
                     "</u:%s>\r\n"                                        \
                     "</s:Body>\r\n"                                      \
                     "</s:Envelope>\r\n"

#define PORT_MAPPING_LEASE_TIME "63072000"                                //two year

#define ADD_PORT_MAPPING_PARAMS "<NewRemoteHost></NewRemoteHost>\r\n"      \
                                "<NewExternalPort>%u</NewExternalPort>\r\n"\
                                "<NewProtocol>%s</NewProtocol>\r\n"        \
                                "<NewInternalPort>%u</NewInternalPort>\r\n"\
                                "<NewInternalClient>%s</NewInternalClient>\r\n"  \
                                "<NewEnabled>1</NewEnabled>\r\n"           \
                                "<NewPortMappingDescription>%s</NewPortMappingDescription>\r\n"  \
                                "<NewLeaseDuration>"                       \
                                PORT_MAPPING_LEASE_TIME                    \
                                "</NewLeaseDuration>\r\n"

#define ACTION_ADD	 "AddPortMapping"
//*********************************************************************************


bool UPNPNAT::init(int time,int inter)
{
	time_out=time;
	interval=inter;
	status=NAT_INIT;

    /*WORD wVersionRequested;
    WSADATA wsaData;
    int err;
    wVersionRequested = MAKEWORD (2, 2);
    err = WSAStartup (wVersionRequested, &wsaData);
	if(err != 0)
		return false;*/
	return true;
}

bool UPNPNAT::tcp_connect(const char * _host,unsigned short int _port)
{
	int ret,i;
	tcp_socket_fd=socket(AF_INET,SOCK_STREAM,0);
	struct sockaddr_in r_address;

    r_address.sin_family = AF_INET;
	r_address.sin_port=htons(_port);
    r_address.sin_addr.s_addr=inet_addr(_host);

	for(i=1;i<=time_out;i++)
	{
		if(i>1)
			sleep(1000);

		ret=connect(tcp_socket_fd,(const struct sockaddr *)&r_address,sizeof(struct sockaddr_in) );
		if(ret==0)
		{
			status=NAT_TCP_CONNECTED;
			return true;
		}
	}

	status=NAT_ERROR;
	char temp[100];
	sprintf(temp,"Fail to connect to %s:%i (using TCP)\n",_host,_port);
	last_error=temp;

	return false;
}

bool UPNPNAT::discovery()
{
	char data[] =
				"M-SEARCH * HTTP/1.1\r\n"
				"Host: 239.255.255.250:1900\r\n"
				"Man: \"ssdp:discover\"\r\n"
				"ST:upnp:rootdevice\r\n"
				"MX:3\r\n"
				"\r\n";
		//==================================================SSDP DATA
		/*string strRetXML;//=strInXML;
		int nsockfd;
		struct sockaddr_in st_addr;
		struct sockaddr_in st_senderinfo;
		int nSendSu,iRecvdata;
		nsockfd=socket(AF_INET,SOCK_DGRAM,0);
		st_addr.sin_family=AF_INET;
		st_addr.sin_port=htons(1900);
		inet_pton(AF_INET,"239.255.255.250",&st_addr.sin_addr.s_addr);
		struct timeval timeout;
		timeout.tv_sec=0;
		timeout.tv_usec=10000;
		if(setsockopt(nsockfd,SOL_SOCKET,SO_RCVTIMEO,(char*)&timeout,sizeof(timeout))!=0)
		{
			perror("setsocketopt timeout");
			close(nsockfd);
			return false;
		}
		for(int i=0;i<20;i++)
		{
			nSendSu=sendto(nsockfd,data,sizeof(data),0,(struct sockaddr *)&st_addr,sizeof(st_addr));
			if(nSendSu<1)
			{
				perror("sendto");
				close(nsockfd);
				return false;
			}
		}
		//=========================================================================================================================================

		socklen_t senderinfolen;
		senderinfolen=sizeof(st_senderinfo);
		string strCheckBuf="";
		int ncount=0;
		int nRetry=0;
		while(1)
		{
			char iRecvbuffer[10*1024];
			memset(iRecvbuffer,0,10*1024);
			iRecvdata=recvfrom(nsockfd,iRecvbuffer,sizeof(iRecvbuffer),0,(struct sockaddr *)&st_senderinfo,&senderinfolen);
			if(iRecvdata<=0)
			{
				//cout<<iRecvdata<<endl;
				//cout<<nRetry<<endl;
				nRetry++;
				if(nRetry>5)
					break;
				continue;
			}
			nRetry=0;
			string strBuff=iRecvbuffer;
			string strURL;
			cout<<iRecvbuffer<<endl;
			if(strBuff.find("10.128.20.142")!=string::npos)
				cout<<iRecvbuffer<<endl;
			cout<<ncount++<<endl;

		}
		close(nsockfd);*/

   udp_socket_fd=socket(AF_INET,SOCK_DGRAM,0);
    int i,ret;
    std::string send_buff=SEARCH_REQUEST_STRING;
    std::string recv_buff;
    char buff[MAX_BUFF_SIZE+1]; //buff should be enough big

    struct sockaddr_in r_address;
    r_address.sin_family=AF_INET;
    r_address.sin_port=htons(1900);
    inet_pton(AF_INET,"239.255.255.250",&r_address.sin_addr.s_addr);

    bool bOptVal = true;
    int bOptLen = sizeof(bool);
    int iOptLen = sizeof(int);

    ret=setsockopt(udp_socket_fd, SOL_SOCKET, SO_BROADCAST, (char*)&bOptVal, bOptLen);

    ret=sendto(udp_socket_fd,send_buff.c_str(),send_buff.size(),0,(struct sockaddr*)&r_address,sizeof(struct sockaddr_in));

	for(i=1;i<=time_out;i++)
    {
		u_long val = 1;
		ioctl(udp_socket_fd,FIONBIO,&val);//none block

		memset(buff, 0, sizeof(buff));
        ret=recvfrom(udp_socket_fd,buff,MAX_BUFF_SIZE,0,NULL,NULL);
		if(ret==SOCKET_ERROR){
			sleep(1);
            continue;
		}
		printf("%s\n",buff);
		recv_buff=buff;
        ret=recv_buff.find(HTTP_OK);
        if(ret==std::string::npos)
			continue;                       //invalid response

        std::string::size_type begin=recv_buff.find("http://");
        if(begin==std::string::npos)
            continue;                       //invalid response
        std::string::size_type end=recv_buff.find("\r",begin);
        if(end==std::string::npos)
			continue;    //invalid response

		describe_url=describe_url.assign(recv_buff,begin,end-begin);

		if(!get_description()){
			sleep(1);//_sleep(1000)=sleep(1)
			continue;
		}

		if(!parser_description()){
			sleep(1);
			continue;
		}

        close(udp_socket_fd);
		status=NAT_FOUND;					//find a router
		return true ;
    }

	status=NAT_ERROR;
	last_error="Fail to find an UPNP NAT.\n";

    return false;                               //no router finded
}

bool UPNPNAT::get_description()
{
	std::string host,path;
	unsigned short int port;
	int ret=parseUrl(describe_url.c_str(),host,&port,path);
	if(!ret)
	{
		status=NAT_ERROR;
		last_error="Failed to parseURl: "+describe_url+"\n";
		return false;
	}

	//connect
	ret=tcp_connect(host.c_str(),port);
	if(!ret){
		return false;
	}

	char request[200];
	sprintf (request,"GET %s HTTP/1.1\r\nHost: %s:%d\r\n\r\n",path.c_str(),host.c_str(),port);
	std::string http_request=request;

	//send request
	ret=send(tcp_socket_fd,http_request.c_str(),http_request.size(),0);
	//get description xml file
	char buff[MAX_BUFF_SIZE+1];
	memset(buff, 0, sizeof(buff));
	std::string response;
	while ( ret=recv(tcp_socket_fd,buff,MAX_BUFF_SIZE,0) >0)
	{
		response+=buff;
		memset(buff, 0, sizeof(buff));
	}

	description_info=response;

	return true;
}

bool UPNPNAT::parser_description()
{
	XMLNode node=XMLNode::parseString(description_info.c_str(),"root");
	if(node.isEmpty())
	{
		status=NAT_ERROR;
		last_error="The device descripe XML file is not a valid XML file. Cann't find root element.\n";
		return false;
	}

	XMLNode baseURL_node=node.getChildNode("URLBase",0);
	if(!baseURL_node.getText())
	{
		std::string::size_type index=describe_url.find("/",7);
		if(index==std::string::npos )
		{
			status=NAT_ERROR;
			last_error="Fail to get base_URL from XMLNode \"URLBase\" or describe_url.\n";
			return false;
		}
		base_url=base_url.assign(describe_url,0,index);
	}
	else
		base_url=baseURL_node.getText();

	int num,i;
	XMLNode device_node,deviceList_node,deviceType_node;
	num=node.nChildNode("device");
	for(i=0;i<num;i++)
	{
		device_node=node.getChildNode("device",i);
		if(device_node.isEmpty())
			break;
		deviceType_node=device_node.getChildNode("deviceType",0);
		if(strcmp(deviceType_node.getText(),DEVICE_TYPE_1)==0)
			break;
	}

	if(device_node.isEmpty())
	{
		status=NAT_ERROR;
		last_error="Fail to find device \"urn:schemas-upnp-org:device:InternetGatewayDevice:1 \"\n";
		return false;
	}

	deviceList_node=device_node.getChildNode("deviceList",0);
	if(deviceList_node.isEmpty())
	{
		status=NAT_ERROR;
		last_error=" Fail to find deviceList of device \"urn:schemas-upnp-org:device:InternetGatewayDevice:1 \"\n";
		return false;
	}

	// get urn:schemas-upnp-org:device:WANDevice:1 and it's devicelist
	num=deviceList_node.nChildNode("device");
	for(i=0;i<num;i++)
	{
		device_node=deviceList_node.getChildNode("device",i);
		if(device_node.isEmpty())
			break;
		deviceType_node=device_node.getChildNode("deviceType",0);
		if(strcmp(deviceType_node.getText(),DEVICE_TYPE_2)==0)
			break;
	}

	if(device_node.isEmpty())
	{
		status=NAT_ERROR;
		last_error="Fail to find device \"urn:schemas-upnp-org:device:WANDevice:1 \"\n";
		return false;
	}

	deviceList_node=device_node.getChildNode("deviceList",0);
	if(deviceList_node.isEmpty())
	{
		status=NAT_ERROR;
		last_error=" Fail to find deviceList of device \"urn:schemas-upnp-org:device:WANDevice:1 \"\n";
		return false;
	}

	// get urn:schemas-upnp-org:device:WANConnectionDevice:1 and it's servicelist
	num=deviceList_node.nChildNode("device");
	for(i=0;i<num;i++)
	{
		device_node=deviceList_node.getChildNode("device",i);
		if(device_node.isEmpty())
			break;
		deviceType_node=device_node.getChildNode("deviceType",0);
		if(strcmp(deviceType_node.getText(),DEVICE_TYPE_3)==0)
			break;
	}
	if(device_node.isEmpty())
	{
		status=NAT_ERROR;
		last_error="Fail to find device \"urn:schemas-upnp-org:device:WANConnectionDevice:1\"\n";
		return false;
	}

	XMLNode serviceList_node,service_node,serviceType_node;
	serviceList_node=device_node.getChildNode("serviceList",0);
	if(serviceList_node.isEmpty())
	{
		status=NAT_ERROR;
		last_error=" Fail to find serviceList of device \"urn:schemas-upnp-org:device:WANDevice:1 \"\n";
		return false;
	}

	num=serviceList_node.nChildNode("service");
	const char * serviceType;
	bool is_found=false;
	for(i=0;i<num;i++)
	{
		service_node=serviceList_node.getChildNode("service",i);
		if(service_node.isEmpty())
			break;
		serviceType_node=service_node.getChildNode("serviceType");
		if(serviceType_node.isEmpty())
			continue;
		serviceType=serviceType_node.getText();
		if(strcmp(serviceType,SERVICE_WANIP)==0||strcmp(serviceType,SERVICE_WANPPP)==0)
		{
			is_found=true;
			break;
		}
	}

	if(!is_found)
	{
		status=NAT_ERROR;
		last_error="can't find  \" SERVICE_WANIP \" or \" SERVICE_WANPPP \" service.\n";
		return false;
	}

	this->service_type=serviceType;

	XMLNode controlURL_node=service_node.getChildNode("controlURL");
	control_url=controlURL_node.getText();

	//make the complete control_url;
	if(control_url.find("http://")==std::string::npos&&control_url.find("HTTP://")==std::string::npos)
		control_url=base_url+control_url;
	if(service_describe_url.find("http://")==std::string::npos&&service_describe_url.find("HTTP://")==std::string::npos)
		service_describe_url=base_url+service_describe_url;

	close(tcp_socket_fd);
	status=NAT_GETCONTROL;
	return true;
}


bool UPNPNAT::add_port_mapping(char * _description, char * _destination_ip, unsigned short int _port_ex, unsigned short int _port_in, char * _protocal)
{
	int ret;

	std::string host,path;
	unsigned short int port;
	ret=parseUrl(control_url.c_str(),host,&port,path);
	if(!ret)
	{
		status=NAT_ERROR;
		last_error="Fail to parseURl: "+describe_url+"\n";
		return false;
	}

	//connect
	ret=tcp_connect(host.c_str(),port);
	if(!ret)
		return false;

	char buff[MAX_BUFF_SIZE+1];
	sprintf(buff,ADD_PORT_MAPPING_PARAMS,_port_ex,_protocal,_port_in,_destination_ip,_description);
	std::string action_params=buff;

	sprintf(buff,SOAP_ACTION,ACTION_ADD,service_type.c_str(),action_params.c_str(),ACTION_ADD);
	std::string soap_message=buff;

	sprintf(buff,HTTP_HEADER_ACTION,path.c_str(),host.c_str(),port,service_type.c_str(),ACTION_ADD,soap_message.size());
	std::string action_message=buff;

	std::string http_request=action_message+soap_message;

	//send request
	ret=send(tcp_socket_fd,http_request.c_str(),http_request.size(),0);

	//wait for response
	std::string response;
	memset(buff, 0, sizeof(buff));
	while (ret=recv(tcp_socket_fd,buff,MAX_BUFF_SIZE,0) >0)
	{
		response+=buff;
		memset(buff, 0, sizeof(buff));
	}

	if(response.find(HTTP_OK)==std::string::npos)
	{
		status=NAT_ERROR;
		char temp[100];
		sprintf(temp,"Fail to add port mapping (%s/%s)\n",_description,_protocal);
		last_error=temp;
		return false;
	}

	close(tcp_socket_fd);
	status=NAT_ADD;
	return true;
}
