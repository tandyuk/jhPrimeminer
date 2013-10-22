#include"global.h"
#include <iostream>
/*
 * Called when a packet with the opcode XPT_OPC_S_AUTH_ACK is received
 */
bool xptClient_processPacket_authResponse(xptClient_t* xptClient)
{
	xptPacketbuffer_t* cpb = xptClient->recvBuffer;
	// read data from the packet
	xptPacketbuffer_beginReadPacket(cpb);
	// start parsing
	bool readError = false;
	// read error code field
	uint32 authErrorCode = xptPacketbuffer_readU32(cpb, &readError);
	if( readError )
		return false;
	// read reject reason / motd
	char rejectReason[512];
	xptPacketbuffer_readString(cpb, rejectReason, 512, &readError);
	rejectReason[511] = '\0';
	if( readError )
		return false;
	if( authErrorCode == 0 )
	{
		xptClient->clientState = XPT_CLIENT_STATE_LOGGED_IN;
		if(!commandlineInput.silent){
			std::cout << "xpt: Logged in" << std::endl;
			if( rejectReason[0] != '\0' )
				std::cout << "Message from server: " << rejectReason << std::endl;
	}
	}
	else
	{
		// error logging in -> disconnect
		if(!commandlineInput.silent){
			std::cout << "xpt: Failed to log in" << std::endl;
		if( rejectReason[0] != '\0' )
			std::cout << "Reason: " << rejectReason << std::endl;
		}
		return false;
	}
	return true;
}

/*
 * Called when a packet with the opcode XPT_OPC_S_WORKDATA1 is received
 * This is the first version of xpt 'getwork'
 */
bool xptClient_processPacket_blockData1(xptClient_t* xptClient)
{
	// parse block data
	bool recvError = false;
	xptPacketbuffer_beginReadPacket(xptClient->recvBuffer);
	xptClient->workDataValid = false;
	// add general block info
	xptClient->blockWorkInfo.version = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);			// version
	xptClient->blockWorkInfo.height = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);			// block height
	xptClient->blockWorkInfo.nBits = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);			// nBits
	xptClient->blockWorkInfo.nBitsShare = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);		// nBitsRecommended / nBitsShare
	xptClient->blockWorkInfo.nTime = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);			// nTimestamp
	xptPacketbuffer_readData(xptClient->recvBuffer, xptClient->blockWorkInfo.prevBlock, 32, &recvError);	// prevBlockHash
	uint32 payloadNum = xptPacketbuffer_readU32(xptClient->recvBuffer, &recvError);							// payload num
	if( recvError )
	{
		if(!commandlineInput.silent && !commandlineInput.quiet)
			std::cout << "xptClient_processPacket_blockData1(): Parse error" << std::endl;
		return false;
	}
	if( xptClient->payloadNum != payloadNum )
	{
		if(!commandlineInput.silent && !commandlineInput.quiet)
			std::cout << "xptClient_processPacket_blockData1(): Invalid payloadNum" << std::endl;
		return false;
	}
	for(uint32 i=0; i<payloadNum; i++)
	{
		// read merkle root for each work data entry
		xptPacketbuffer_readData(xptClient->recvBuffer, xptClient->workData[i].merkleRoot, 32, &recvError);
	}
	if( recvError )
	{
		if(!commandlineInput.silent && !commandlineInput.quiet)
			std::cout << "xptClient_processPacket_blockData1(): Parse error 2" << std::endl;
		return false;
	}
	xptClient->workDataValid = true;
	xptClient->workDataCounter++;
	return true;
}

/*
 * Called when a packet with the opcode XPT_OPC_S_SHARE_ACK is received
 */
bool xptClient_processPacket_shareAck(xptClient_t* xptClient)
{
	xptPacketbuffer_t* cpb = xptClient->recvBuffer;
	// read data from the packet
	xptPacketbuffer_beginReadPacket(cpb);
	// start parsing
	bool readError = false;
	// read error code field
	uint32 shareErrorCode = xptPacketbuffer_readU32(cpb, &readError);
	if( readError )
		return false;
	// read reject reason
	char rejectReason[512];
	xptPacketbuffer_readString(cpb, rejectReason, 512, &readError);
	rejectReason[511] = '\0';
	float shareValue = xptPacketbuffer_readFloat(cpb, &readError);
	if( readError )
		return false;
	if( shareErrorCode == 0 )
	{
		total_shares++;
		valid_shares++;
		time_t now = time(0);
		char* dt = ctime(&now);
		if(!commandlineInput.silent)
			std::cout << "ACCEPTED [ " << valid_shares << " / " << total_shares << " val: " << shareValue << "] " << dt << std::endl;
		primeStats.fShareValue += shareValue;
		primeStats.fBlockShareValue += shareValue;
		primeStats.fTotalSubmittedShareValue += shareValue;
	}
	else
	{
		// error logging in -> disconnect
		total_shares++;
		if(!commandlineInput.silent){
			std::cout << "Invalid share" << std::endl;
			if( rejectReason[0] != '\0' )
				std::cout << "Reason: " << rejectReason << std::endl;
		}
	}

	return true;
}