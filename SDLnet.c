/*
    NETLIB:  An example cross-platform network library for use with SDL
    Copyright (C) 1997-1999  Sam Lantinga

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public
    License along with this library; if not, write to the Free
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

    Sam Lantinga
    5635-34 Springhouse Dr.
    Pleasanton, CA 94588 (USA)
    slouken@devolution.com
    
    
    History:
    
    2000/03/29 - rrwood - Return MacOS OpenTransport results if errors occur
    2000/03/29 - rrwood - SDLNet_ResolveHost() return 0 on success
*/

#include <string.h>

#include "SDL_byteorder.h"

#include "SDLnetsys.h"
#include "SDL_net.h"


/* Since the UNIX/Win32/BeOS code is so different from MacOS,
   we'll just have two completely different sections here.
*/

#ifdef MACOS_OPENTRANSPORT

static Boolean OTstarted = false;
static InetSvcRef dns = 0;
Uint32 OTlocalhost = 0;

/* Local functions for initializing and cleaning up the DNS resolver */
static int OpenDNS(void)
{
	int retval;
	OSStatus status;

	retval = 0;
#if ! TARGET_API_MAC_CARBON
	dns = OTOpenInternetServices(kDefaultInternetServicesPath, 0, &status);
#else
	dns = OTOpenInternetServicesInContext( kDefaultInternetServicesPath, 0, &status, NULL );
#endif
	if ( status == noErr ) {
		InetInterfaceInfo	info;
		
		/* Get the address of the local system -
		   What should it be if ethernet is off?
		 */
		OTInetGetInterfaceInfo(&info, kDefaultInetInterface);
		OTlocalhost = info.fAddress;
	} else {
		SDLNet_SetError("Unable to open DNS handle");
		retval = status;
	}
	
	return(retval);
}

static void CloseDNS(void)
{
	if ( dns ) {
		OTCloseProvider(dns);
		dns = 0;
	}
	
	OTlocalhost = 0;
}

/* Initialize/Cleanup the network API */
int  SDLNet_Init(void)
{
	OSStatus status;
	int retval;
#if TARGET_API_MAC_CARBON
	OTClientContextPtr	context;
#endif
	
	retval = 0;
	if ( ! OTstarted ) {
#if ! TARGET_API_MAC_CARBON
		status = InitOpenTransport();
#else
		status = InitOpenTransportInContext( kInitOTForApplicationMask, &context );
#endif
		if ( status == noErr ) {
			OTstarted = true;
			retval = OpenDNS();
			if ( retval < 0 ) {
				SDLNet_Quit();
			}
		} else {
			SDLNet_SetError("Unable to initialize Open Transport");
			retval = status;
		}
	}
	
	return(retval);
}

void SDLNet_Quit(void)
{
	if ( OTstarted ) {
		CloseDNS();
#if ! TARGET_API_MAC_CARBON
		CloseOpenTransport();
#else
		CloseOpenTransportInContext( NULL );
#endif
		OTstarted = false;
	}
}

/* Resolve a host name and port to an IP address in network form */
int SDLNet_ResolveHost(IPaddress *address, char *host, Uint16 port)
{
	int retval = 0;

	/* Perform the actual host resolution */
	if ( host == NULL ) {
		address->host = INADDR_ANY;
	} else {
		int a[4];

		address->host = INADDR_NONE;
		
		if ( sscanf(host, "%d.%d.%d.%d", a, a+1, a+2, a+3) == 4 ) {
			if ( !(a[0] & 0xFFFFFF00) && !(a[1] & 0xFFFFFF00) &&
			     !(a[2] & 0xFFFFFF00) && !(a[3] & 0xFFFFFF00) ) {
				address->host = ((a[0] << 24) |
				                 (a[1] << 16) |
				                 (a[2] <<  8) | a[3]);
				if ( address->host == 0x7F000001 ) { /* localhost */
					address->host = OTlocalhost;
				}
			}
		}
		
		if ( address->host == INADDR_NONE ) {
			InetHostInfo hinfo;
			
			/* Check for special case - localhost */
			if ( strcmp(host, "localhost") == 0 )
				return(SDLNet_ResolveHost(address, "127.0.0.1", port));

			/* Have OpenTransport resolve the hostname for us */
			retval = OTInetStringToAddress(dns, host, &hinfo);
			if (retval == noErr) {
				address->host = hinfo.addrs[0];
			}
		}
	}
	
	address->port = SDL_SwapBE16(port);

	/* Return the status */
	return(retval);
}

/* Resolve an ip address to a host name in canonical form.
   If the ip couldn't be resolved, this function returns NULL,
   otherwise a pointer to a static buffer containing the hostname
   is returned.  Note that this function is not thread-safe.
*/
/* MacOS implementation by Roy Wood
 */
char *SDLNet_ResolveIP(IPaddress *ip)
{
	if (ip != nil)
	{
	InetHost				theIP;
	static InetDomainName	theInetDomainName;
	OSStatus				theOSStatus;
	
		
		/*	Default result will be null string */
		
		theInetDomainName[0] = '\0';	
		
		
		/*	Do a reverse DNS lookup */
		
		theIP = ip->host;
		
		theOSStatus = OTInetAddressToName(dns,theIP,theInetDomainName);
		
		/*	If successful, return the result */
			
		if (theOSStatus == kOTNoError)
		{
			return(theInetDomainName);
		}
	}
	
	SDLNet_SetError("Can't perform reverse DNS lookup");
	
	return(NULL);
}

#else /* !MACOS_OPENTRANSPORT */

/* Initialize/Cleanup the network API */
int  SDLNet_Init(void)
{
#ifdef Win32_Winsock
	/* Start up the windows networking */
	WORD version_wanted = MAKEWORD(1,1);
	WSADATA wsaData;

	if ( WSAStartup(version_wanted, &wsaData) != 0 ) {
		SDLNet_SetError("Couldn't initialize Winsock 1.1\n");
		return(-1);
	}
#endif
	return(0);
}
void SDLNet_Quit(void)
{
#ifdef Win32_Winsock
	/* Clean up windows networking */
	if ( WSACleanup() == SOCKET_ERROR ) {
		if ( WSAGetLastError() == WSAEINPROGRESS ) {
			WSACancelBlockingCall();
			WSACleanup();
		}
	}
#endif
}

/* Resolve a host name and port to an IP address in network form */
int SDLNet_ResolveHost(IPaddress *address, char *host, Uint16 port)
{
	int retval = 0;

	/* Perform the actual host resolution */
	if ( host == NULL ) {
		address->host = INADDR_ANY;
	} else {
		address->host = inet_addr(host);
		if ( address->host == INADDR_NONE ) {
			struct hostent *hp;

			hp = gethostbyname(host);
			if ( hp ) {
				memcpy(&address->host,hp->h_addr,hp->h_length);
			} else {
				retval = -1;
			}
		}
	}
	address->port = SDL_SwapBE16(port);

	/* Return the status */
	return(retval);
}

/* Resolve an ip address to a host name in canonical form.
   If the ip couldn't be resolved, this function returns NULL,
   otherwise a pointer to a static buffer containing the hostname
   is returned.  Note that this function is not thread-safe.
*/
/* Written by Miguel Angel Blanch.
 * Main Programmer of Arianne RPG.
 * http://come.to/arianne_rpg
 */
char *SDLNet_ResolveIP(IPaddress *ip)
{
	struct hostent *hp;

	hp = gethostbyaddr((char *)&ip->host, 4, AF_INET);
	if ( hp != NULL ) {
		return hp->h_name;
	}
  	return NULL;
}

#endif /* MACOS_OPENTRANSPORT */

#ifdef USE_GUSI_SOCKETS

/* Configure Socket Factories */

void GUSISetupFactories()
{
	GUSIwithInetSockets();
}

/* Configure File Devices */

void GUSISetupDevices()
{
	return;
}

#endif /* USE_GUSI_SOCKETS */
