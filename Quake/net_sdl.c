/*
Copyright (C) 1996-2001 Id Software, Inc.
Copyright (C) 2002-2005 John Fitzgibbons and others
Copyright (C) 2007-2008 Kristian Duske

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/
#include "quakedef.h"

#include "net_loop.h"
#include "net_dgrm.h"
#include "net_sdlnet.h"

net_driver_t net_drivers[MAX_NET_DRIVERS] =
{
	{
	"Loopback",
	false,
	Loop_Init,
	Loop_Listen,
	Loop_SearchForHosts,
	Loop_Connect,
	Loop_CheckNewConnections,
	Loop_GetMessage,
	Loop_SendMessage,
	Loop_SendUnreliableMessage,
	Loop_CanSendMessage,
	Loop_CanSendUnreliableMessage,
	Loop_Close,
	Loop_Shutdown
	}
	,
	{
	"Datagram",
	false,
	Datagram_Init,
	Datagram_Listen,
	Datagram_SearchForHosts,
	Datagram_Connect,
	Datagram_CheckNewConnections,
	Datagram_GetMessage,
	Datagram_SendMessage,
	Datagram_SendUnreliableMessage,
	Datagram_CanSendMessage,
	Datagram_CanSendUnreliableMessage,
	Datagram_Close,
	Datagram_Shutdown
	}
};

int net_numdrivers = 2;


net_landriver_t	net_landrivers[MAX_NET_DRIVERS] =
{
	{
	"UDP",
	false,
	0,
	SDLN_Init,
	SDLN_Shutdown,
	SDLN_Listen,
	SDLN_OpenSocket,
	SDLN_CloseSocket,
	SDLN_Connect,
	SDLN_CheckNewConnections,
	SDLN_Read,
	SDLN_Write,
	SDLN_Broadcast,
	SDLN_AddrToString,
	SDLN_StringToAddr,
	SDLN_GetSocketAddr,
	SDLN_GetNameFromAddr,
	SDLN_GetAddrFromName,
	SDLN_AddrCompare,
	SDLN_GetSocketPort,
	SDLN_SetSocketPort
	}
};

int net_numlandrivers = 1;

