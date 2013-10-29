#include"global.h"

void bitclient_addVarIntFromStream(stream_t* msgStream, uint64 varInt)
{
	if( varInt <= 0xFC )
	{
		stream_writeU8(msgStream, (uint8)varInt);
		return;
	}
	else if( varInt <= 0xFFFF )
	{
		stream_writeU8(msgStream, 0xFD);
		stream_writeU16(msgStream, (uint16)varInt);
		return;
	}
	else if( varInt <= 0xFFFFFFFF )
	{
		stream_writeU8(msgStream, 0xFE);
		stream_writeU32(msgStream, (uint32)varInt);
		return;
	}
	else
	{
		stream_writeU8(msgStream, 0xFF);
		stream_writeData(msgStream, &varInt, 8);
		return;
	}
}

bitclientTransaction_t* bitclient_createTransaction()
{
	bitclientTransaction_t* tx = (bitclientTransaction_t*)malloc(sizeof(bitclientTransaction_t));
	memset(tx, 0, sizeof(bitclientTransaction_t));
	// init lists
	tx->tx_in = simpleList_create(1);
	tx->tx_out = simpleList_create(1);
	// init version and misc
	tx->version = 1;
	tx->lock_time = 0;
	// return tx object
	return tx;
}

void bitclient_destroyTransaction(bitclientTransaction_t* tx)
{
	for(uint32 i=0; i<tx->tx_in->objectCount; i++)
		free(tx->tx_in->objects[i]);
	for(uint32 i=0; i<tx->tx_out->objectCount; i++)
		free(tx->tx_out->objects[i]);
	simpleList_free(tx->tx_in);
	simpleList_free(tx->tx_out);
	free(tx);
}

void bitclient_writeTransactionToStream(stream_t* dataStream, bitclientTransaction_t* tx)
{
	stream_writeU32(dataStream, tx->version);
	bitclient_addVarIntFromStream(dataStream, tx->tx_in->objectCount);
	for(uint32 i=0; i<tx->tx_in->objectCount; i++)
	{
		bitclientVIn_t* vin = (bitclientVIn_t*)tx->tx_in->objects[i];
		stream_writeData(dataStream, vin->previous_output.hash, 32);
		stream_writeU32(dataStream, vin->previous_output.index);
		bitclient_addVarIntFromStream(dataStream, vin->scriptSignatureLength);
		stream_writeData(dataStream, vin->scriptSignatureData, vin->scriptSignatureLength);
		stream_writeU32(dataStream, vin->sequence);
	}
	bitclient_addVarIntFromStream(dataStream, tx->tx_out->objectCount);
	for(uint32 i=0; i<tx->tx_out->objectCount; i++)
	{
		bitclientVOut_t* vout = (bitclientVOut_t*)tx->tx_out->objects[i];
		stream_writeData(dataStream, &vout->coinValue, 8);
		bitclient_addVarIntFromStream(dataStream, vout->scriptPkLength);
		stream_writeData(dataStream, vout->scriptPkData, vout->scriptPkLength);
	}
	stream_writeU32(dataStream, 0); // lock time
}

bitclientVIn_t* bitclient_addTransactionInput(bitclientTransaction_t* tx)
{
	bitclientVIn_t* vin = (bitclientVIn_t*)malloc(sizeof(bitclientVIn_t)+2*1024);
	memset(vin, 0, sizeof(bitclientVIn_t)+2*1024);
	vin->scriptSignatureData = (uint8*)(vin+1);
	simpleList_add(tx->tx_in, vin);
	return vin;
}

bitclientVOut_t* bitclient_addTransactionOutput(bitclientTransaction_t* tx)
{
	bitclientVOut_t* vout = (bitclientVOut_t*)malloc(sizeof(bitclientVOut_t)+2*1024);
	memset(vout, 0, sizeof(bitclientVOut_t)+2*1024);
	vout->scriptPkData = (uint8*)(vout+1);
	simpleList_add(tx->tx_out,vout);
	return vout;
}

void bitclient_addScriptOpcode(bitclientVIn_t* vin, uint8 opcode)
{
	vin->scriptSignatureData[vin->scriptSignatureLength] = opcode;
	vin->scriptSignatureLength++;
}

void bitclient_addScriptU32(bitclientVIn_t* vin, uint32 v, bool hasLengthPrefix)
{
	if( v <= 0x7F )
	{
		if( hasLengthPrefix )
		{
			vin->scriptSignatureData[vin->scriptSignatureLength] = 1;
			vin->scriptSignatureLength++;
		}
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength] = v;
		vin->scriptSignatureLength += 1;
	}
	else if( v <= 0x7FFF )
	{
		if( hasLengthPrefix )
		{
			vin->scriptSignatureData[vin->scriptSignatureLength] = 2;
			vin->scriptSignatureLength++;
		}
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+0] = (v>>0)&0xFF;
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+1] = (v>>8)&0xFF;
		vin->scriptSignatureLength += 2;
	}
	else if( v <= 0x7FFFFF )
	{
		if( hasLengthPrefix )
		{
			vin->scriptSignatureData[vin->scriptSignatureLength] = 3;
			vin->scriptSignatureLength++;
		}
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+0] = (v>>0)&0xFF;
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+1] = (v>>8)&0xFF;
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+2] = (v>>16)&0xFF;
		vin->scriptSignatureLength += 3;
	}
	else if( v <= 0x7FFFFFFF )
	{
		if( hasLengthPrefix )
		{
			vin->scriptSignatureData[vin->scriptSignatureLength] = 4;
			vin->scriptSignatureLength++;
		}
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+0] = (v>>0)&0xFF;
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+1] = (v>>8)&0xFF;
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+2] = (v>>16)&0xFF;
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+3] = (v>>24)&0xFF;
		vin->scriptSignatureLength += 4;
	}
	else
	{
		if( hasLengthPrefix )
		{
			vin->scriptSignatureData[vin->scriptSignatureLength] = 5;
			vin->scriptSignatureLength++;
		}
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+0] = (v>>0)&0xFF;
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+1] = (v>>8)&0xFF;
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+2] = (v>>16)&0xFF;
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+3] = (v>>24)&0xFF;
		*(uint8*)&vin->scriptSignatureData[vin->scriptSignatureLength+4] = 0;
		vin->scriptSignatureLength += 5;
	}

}

void bitclient_addScriptData(bitclientVIn_t* vin, uint8* data, uint32 length, bool hasLengthPrefix)
{
	if( length <= 100 && hasLengthPrefix )
		bitclient_addScriptOpcode(vin, length);
	memcpy(vin->scriptSignatureData+vin->scriptSignatureLength, data, length);
	vin->scriptSignatureLength += length;
}

void bitclient_addScriptOpcode(bitclientVOut_t* vout, uint8 opcode)
{
	vout->scriptPkData[vout->scriptPkLength] = opcode;
	vout->scriptPkLength++;
}

void bitclient_addScriptData(bitclientVOut_t* vout, uint8* data, uint32 length)
{
	if( length <= 100 )
		bitclient_addScriptOpcode(vout, length);
	memcpy(vout->scriptPkData+vout->scriptPkLength, data, length);
	vout->scriptPkLength += length;
}

bitclientTransaction_t* bitclient_createCoinbaseTransactionFromSeed(uint32 seed, uint32 seed_userId, uint32 blockHeight, uint8* pubKeyRipeHash, uint64 inputValue)
{
	bitclientTransaction_t* tx = bitclient_createTransaction();
	// add coinbase input
	bitclientVIn_t* vInput = bitclient_addTransactionInput(tx);
	memset(vInput->previous_output.hash, 0, 32);
	vInput->previous_output.index = -1;
	vInput->sequence = -1;
	bitclient_addScriptU32(vInput, blockHeight, true);
	bitclient_addScriptData(vInput, (uint8*)&seed, 4, true);
	uint32 seed2 = (seed_userId ^ 0xf83220df + seed_userId * 29481 + seed_userId * 3);
	bitclient_addScriptData(vInput, (uint8*)&seed2, 4, true);
	bitclient_addScriptData(vInput, (uint8*)"jhPrimeminer", 12, true);
	bitclient_addScriptData(vInput, (uint8*)"/P2SH/", 6, false); // coinbase flags...
	// create output
	bitclientVOut_t* vOutput = bitclient_addTransactionOutput(tx);
	bitclient_addScriptOpcode(vOutput, 0x76); // OP_DUP
	bitclient_addScriptOpcode(vOutput, 0xA9); // OP_HASH160
	bitclient_addScriptData(vOutput, (uint8*)pubKeyRipeHash, 20);
	bitclient_addScriptOpcode(vOutput, 0x88); // OP_EQUALVERIFY
	bitclient_addScriptOpcode(vOutput, 0xAC); // OP_CHECKSIG
	vOutput->coinValue = inputValue;
	return tx;
}

void bitclient_generateTxHash(bitclientTransaction_t* tx, uint8* txHash)
{
	stream_t* streamTXData = streamEx_fromDynamicMemoryRange(1024*32);
	bitclient_writeTransactionToStream(streamTXData, tx);
	sint32 transactionDataLength = 0;
	uint8* transactionData = (uint8*)streamEx_map(streamTXData, &transactionDataLength);
	// special case, we can use the hash of the transaction
	uint8 hashOut[32];
	sha256_context sha256_ctx;
	sha256_starts(&sha256_ctx);
	sha256_update(&sha256_ctx, transactionData, transactionDataLength);
	sha256_finish(&sha256_ctx, hashOut);
	sha256_starts(&sha256_ctx);
	sha256_update(&sha256_ctx, hashOut, 32);
	sha256_finish(&sha256_ctx, txHash);
	free(transactionData);
	stream_destroy(streamTXData);
}

void bitclient_calculateMerkleRoot(uint8* txHashes, uint32 numberOfTxHashes, uint8 merkleRoot[32])
{
	if( numberOfTxHashes > 32 )
	{
		// we only support 32 transactions at a time
		printf("bitclient_calculateMerkleRoot: Too many transactions, numberOfTx set to 32\n");
		numberOfTxHashes = 32;
	}
	if(numberOfTxHashes <= 0 )
	{
		printf("bitclient_calculateMerkleRoot: Block has zero transactions (not even coinbase)\n");
		memset(merkleRoot, 0, 32);
		return;
	}
	else if( numberOfTxHashes == 1 )
	{
		// generate transaction data
		memcpy(merkleRoot, txHashes+0, 32);
		////uint32 transactionData[]
		//stream_t* streamTXData = streamEx_fromDynamicMemoryRange(1024*32);
		//bitclient_writeTransactionToStream(streamTXData, txList[0]);
		//sint32 transactionDataLength = 0;
		//uint8* transactionData = (uint8*)streamEx_map(streamTXData, &transactionDataLength);
		//// special case, we can use the hash of the transaction
		//uint8 hashOut[32];
		//sha256_context sha256_ctx;
		//sha256_starts(&sha256_ctx);
		//sha256_update(&sha256_ctx, transactionData, transactionDataLength);
		//sha256_finish(&sha256_ctx, hashOut);
		//sha256_starts(&sha256_ctx);
		//sha256_update(&sha256_ctx, hashOut, 32);
		//sha256_finish(&sha256_ctx, merkleRoot);
		//free(transactionData);
		//stream_destroy(streamTXData);
		return;
	}
	else
	{
		// build merkle root tree
		uint8 hashData[256*32]; // space for 256 hashes
		uint32 hashCount = 0; // number of hashes written to hashData
		uint32 hashReadIndex = 0; // index of currently processed hash
		uint32 layerSize[10] = {0}; // up to 16 layers (which means 2^10 hashes are possible)
		layerSize[0] = numberOfTxHashes;
		for(uint32 i=0; i<numberOfTxHashes; i++)
		{
			memcpy(hashData+(hashCount*32), txHashes+(i*32), 32);
			hashCount++;
		}
		if(numberOfTxHashes&1 && numberOfTxHashes > 1 )
		{
			// duplicate last hash
			memcpy(hashData+(hashCount*32), hashData+((hashCount-1)*32), 32);
			hashCount++;
			layerSize[0]++;
		}
		// process layers
		for(uint32 f=0; f<10; f++)
		{
			if( layerSize[f] == 0 )
			{
				printf("bitclient_calculateMerkleRoot: Error generating merkleRoot hash\n");
				return;
			}
			else if( layerSize[f] == 1 )
			{
				// result found
				memcpy(merkleRoot, hashData+(hashReadIndex*32), 32);
				hashReadIndex++;
				return;
			}
			for(uint32 i=0; i<layerSize[f]; i += 2)
			{
				uint8 hashOut[32];
				sha256_context sha256_ctx;
				sha256_starts(&sha256_ctx);
				sha256_update(&sha256_ctx, hashData+(hashReadIndex*32), 32*2);
				hashReadIndex += 2;
				sha256_finish(&sha256_ctx, hashOut);
				sha256_starts(&sha256_ctx);
				sha256_update(&sha256_ctx, hashOut, 32);
				sha256_finish(&sha256_ctx, hashData+(hashCount*32));
				hashCount++;
				layerSize[f+1]++;
			}
			// do we need to duplicate the last hash?
			if( layerSize[f+1]&1 && layerSize[f+1] > 1 )
			{
				// duplicate last hash
				memcpy(hashData+(hashCount*32), hashData+((hashCount-1)*32), 32);
				hashCount++;
			}
		}
	}
}