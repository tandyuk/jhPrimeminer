
typedef struct  
{
	struct  
	{
		uint8 hash[32];
		uint32 index;
	}previous_output;
	uint32 scriptSignatureLength;
	uint8* scriptSignatureData;
	uint32 sequence;
	uint32 coinbaseMinerExtraNonceSign; // used only in combination with coinbase transaction (points to offset where the miner is allowed to insert his custom exta nonce)
}bitclientVIn_t;

typedef struct  
{
	uint64 coinValue;
	uint32 scriptPkLength;
	uint8* scriptPkData;
}bitclientVOut_t;

typedef struct  
{
	uint32 version;
	//uint32 tx_in_count; // 
	simpleList_t* tx_in;
	simpleList_t* tx_out;
	uint32 lock_time;
}bitclientTransaction_t;

bitclientTransaction_t* bitclient_createTransaction();
bitclientTransaction_t* bitclient_createCoinbaseTransactionFromSeed(uint32 seed, uint32 seed_userId, uint32 blockHeight, uint8* pubKeyRipeHash, uint64 inputValue);
void bitclient_destroyTransaction(bitclientTransaction_t* tx);
void bitclient_writeTransactionToStream(stream_t* dataStream, bitclientTransaction_t* tx);
void bitclient_generateTxHash(bitclientTransaction_t* tx, uint8* txHash);
void bitclient_calculateMerkleRoot(uint8* txHashes, uint32 numberOfTxHashes, uint8 merkleRoot[32]);
// misc
void bitclient_addVarIntFromStream(stream_t* msgStream, uint64 varInt);