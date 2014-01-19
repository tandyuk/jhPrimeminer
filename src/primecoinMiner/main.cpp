#include"global.h"
#include "ticker.h"
#include<ctime>
#include<map>
#include <termios.h>            //termios, TCSANOW, ECHO, ICANON
#include <unistd.h>     //STDIN_FILENO

//used for get_num_cpu
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
#include <sys/types.h>
#include <sys/sysctl.h>
#endif


primeStats_t primeStats = {};
commandlineInput_t commandlineInput = {};
volatile int total_shares = 0;
volatile int valid_shares = 0;
unsigned int nMaxSieveSize;
unsigned int nSievePercentage;
bool nPrintDebugMessages;
unsigned long nOverrideTargetValue;
unsigned int nOverrideBTTargetValue;
unsigned int nRoundSievePercentage;
bool bEnablenPrimorialMultiplierTuning;

char* dt;
bool useGetBlockTemplate = true;
uint8 decodedWalletAddress[32];
int decodedWalletAddressLen;

uint64 * threadHearthBeat;
	std::map<unsigned int, bool> thmap;
	std::map <unsigned int, bool>::iterator thmapIter;
	typedef std::pair <unsigned int, bool> thmapKeyVal;


typedef struct  
{
	bool isValidData;
	// block data
	uint32 version;
	uint32 height;
	uint32 nTime;
	uint32 nBits;
	uint8 previousBlockHash[32];
	uint8 target[32]; // sha256 & scrypt
	// coinbase aux
	uint8 coinbaseauxFlags[128];
	uint32 coinbaseauxFlagsLength; // in bytes
	// todo: mempool transactions
}getBlockTemplateData_t;

getBlockTemplateData_t getBlockTemplateData = {0};

uint64 lastShareSubmit = getTimeMilliseconds(); // Lets pretend something was submitted at start - to not reset too soon!

char* minerVersionString = "jhPrimeminer - TandyUK 3.3a";


bool error(const char *format, ...)
{
	puts(format);
	return false;
}


bool hex2bin(unsigned char *p, const char *hexstr, size_t len)
{
	bool ret = false;

	while (*hexstr && len) {
		char hex_byte[4];
		unsigned int v;

		if (!hexstr[1]) {
			std::cout << "hex2bin str truncated" << std::endl;
			return ret;
		}

		memset(hex_byte, 0, 4);
		hex_byte[0] = hexstr[0];
		hex_byte[1] = hexstr[1];

		if (sscanf(hex_byte, "%x", &v) != 1) {
			std::cout << "hex2bin sscanf '" << hex_byte << "' failed" << std::endl;
			return ret;
		}

		*p = (unsigned char) v;

		p++;
		hexstr += 2;
		len--;
	}

	if (len == 0 && *hexstr == 0)
		ret = true;
	return ret;
}



uint32 _swapEndianessU32(uint32 v)
{
	return ((v>>24)&0xFF)|((v>>8)&0xFF00)|((v<<8)&0xFF0000)|((v<<24)&0xFF000000);
}

uint32 _getHexDigitValue(uint8 c)
{
	if( c >= '0' && c <= '9' )
		return c-'0';
	else if( c >= 'a' && c <= 'f' )
		return c-'a'+10;
	else if( c >= 'A' && c <= 'F' )
		return c-'A'+10;
	return 0;
}


static const char* pszBase58 = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

inline bool DecodeBase58(const char* psz, uint8* vchRet, int* retLength)
{
	CAutoBN_CTX pctx;
	CBigNum bn58 = 58;
	CBigNum bn = 0;
	CBigNum bnChar;
	while (isspace(*psz))
		psz++;
	// Convert big endian string to bignum
	for (const char* p = psz; *p; p++)
	{
		const char* p1 = strchr(pszBase58, *p);
		if (p1 == NULL)
		{
			while (isspace(*p))
				p++;
			if (*p != '\0')
				return false;
			break;
		}
		bnChar.setulong(p1 - pszBase58);
		if (!BN_mul(&bn, &bn, &bn58, pctx))
			throw bignum_error("DecodeBase58 : BN_mul failed");
		bn += bnChar;
	}

	// Get bignum as little endian data
	std::vector<unsigned char> vchTmp = bn.getvch();

	// Trim off sign byte if present
	if (vchTmp.size() >= 2 && vchTmp.end()[-1] == 0 && vchTmp.end()[-2] >= 0x80)
		vchTmp.erase(vchTmp.end()-1);

	// Restore leading zeros
	int nLeadingZeros = 0;
	for (const char* p = psz; *p == pszBase58[0]; p++)
		nLeadingZeros++;
	// Convert little endian data to big endian
	int rLen = nLeadingZeros + vchTmp.size();
	for(int i=0; i<rLen; i++)
	{
		vchRet[rLen-i-1] = vchTmp[i];
	}
	*retLength = rLen;
	return true;
}

/*
* Parses a hex string
* Length should be a multiple of 2
*/
void jhMiner_parseHexString(char* hexString, uint32 length, uint8* output)
{
	uint32 lengthBytes = length / 2;
	for(uint32 i=0; i<lengthBytes; i++)
	{
		// high digit
		uint32 d1 = _getHexDigitValue(hexString[i*2+0]);
		// low digit
		uint32 d2 = _getHexDigitValue(hexString[i*2+1]);
		// build byte
		output[i] = (uint8)((d1<<4)|(d2));	
	}
}

/*
* Parses a hex string and converts it to LittleEndian (or just opposite endianness)
* Length should be a multiple of 2
*/
void jhMiner_parseHexStringLE(char* hexString, uint32 length, uint8* output)
{
	uint32 lengthBytes = length / 2;
	for(uint32 i=0; i<lengthBytes; i++)
	{
		// high digit
		uint32 d1 = _getHexDigitValue(hexString[i*2+0]);
		// low digit
		uint32 d2 = _getHexDigitValue(hexString[i*2+1]);
		// build byte
		output[lengthBytes-i-1] = (uint8)((d1<<4)|(d2));	
	}
}


void primecoinBlock_generateHeaderHash(primecoinBlock_t* primecoinBlock, uint8 hashOutput[32])
{
	uint8 blockHashDataInput[512];
	memcpy(blockHashDataInput, primecoinBlock, 80);
	sha256_context ctx;
	sha256_starts(&ctx);
	sha256_update(&ctx, (uint8*)blockHashDataInput, 80);
	sha256_finish(&ctx, hashOutput);
	sha256_starts(&ctx); // is this line needed?
	sha256_update(&ctx, hashOutput, 32);
	sha256_finish(&ctx, hashOutput);
}

void primecoinBlock_generateBlockHash(primecoinBlock_t* primecoinBlock, uint8 hashOutput[32])
{
	uint8 blockHashDataInput[512];
	memcpy(blockHashDataInput, primecoinBlock, 80);
	uint32 writeIndex = 80;
	sint32 lengthBN = 0;
	CBigNum bnPrimeChainMultiplier;
	bnPrimeChainMultiplier.SetHex(primecoinBlock->mpzPrimeChainMultiplier.get_str(16));
	std::vector<unsigned char> bnSerializeData = bnPrimeChainMultiplier.getvch();
	lengthBN = bnSerializeData.size();
	*(uint8*)(blockHashDataInput+writeIndex) = (uint8)lengthBN;
	writeIndex += 1;
	memcpy(blockHashDataInput+writeIndex, &bnSerializeData[0], lengthBN);
	writeIndex += lengthBN;
	sha256_context ctx;
	sha256_starts(&ctx);
	sha256_update(&ctx, (uint8*)blockHashDataInput, writeIndex);
	sha256_finish(&ctx, hashOutput);
	sha256_starts(&ctx); // is this line needed?
	sha256_update(&ctx, hashOutput, 32);
	sha256_finish(&ctx, hashOutput);
}

typedef struct  
{
	bool dataIsValid;
	uint8 data[128];
	uint32 dataHash; // used to detect work data changes
	uint8 serverData[32]; // contains data from the server 
}workDataEntry_t;

typedef struct  
{
	pthread_mutex_t cs;
	uint8 protocolMode;
	// xpm
	workDataEntry_t workEntry[128]; // work data for each thread (up to 128)
	// x.pushthrough
	xptClient_t* xptClient;
}workData_t;

#define MINER_PROTOCOL_GETWORK		(1)
#define MINER_PROTOCOL_STRATUM		(2)
#define MINER_PROTOCOL_XPUSHTHROUGH	(3)
#define MINER_PROTOCOL_GBT		(4)

bool bSoloMining = false;
workData_t workData;
int lastBlockCount = 0;

jsonRequestTarget_t jsonRequestTarget = {}; // rpc login data
bool useLocalPrimecoindForLongpoll;

/*
* Pushes the found block data to the server for giving us the $$$
* Uses getwork to push the block
* Returns true on success
* Note that the primecoin data can be larger due to the multiplier at the end, so we use 512 bytes per default
* 29.sep: switched to 512 bytes per block as default, since Primecoin can use up to 2000 bits (250 bytes) for the multiplier chain + length prefix of 2 bytes
*/
bool jhMiner_pushShare_primecoin(uint8 data[512], primecoinBlock_t* primecoinBlock){
	if( workData.protocolMode == MINER_PROTOCOL_GETWORK ){
		// prepare buffer to send
		fStr_buffer4kb_t fStrBuffer_parameter;
		fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
		fStr_append(fStr_parameter, "[\"");
		fStr_addHexString(fStr_parameter, data, 512);
		fStr_appendFormatted(fStr_parameter, "\",\"");
		fStr_addHexString(fStr_parameter, (uint8*)&primecoinBlock->serverData, 32);
		fStr_append(fStr_parameter, "\"]");
		// send request
		sint32 rpcErrorCode = 0;
		jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getwork", fStr_parameter, &rpcErrorCode);
		if( jsonReturnValue == NULL ){
			printf("PushWorkResult failed :(\n");
			return false;
		}else{
			// rpc call worked, sooooo.. is the server happy with the result?
			jsonObject_t* jsonReturnValueBool = jsonObject_getParameter(jsonReturnValue, "result");
			if( jsonObject_isTrue(jsonReturnValueBool) ){
				total_shares++;
				valid_shares++;
				time_t now = time(0);
				dt = ctime(&now);
				//printf("Valid share found!");
				//printf("[ %d / %d ] %s",valid_shares, total_shares,dt);
				jsonObject_freeObject(jsonReturnValue);
				return true;
			}else{
				total_shares++;
				// the server says no to this share :(
				printf("Server rejected share (BlockHeight: %d/%d nBits: 0x%08X)\n", primecoinBlock->serverData.blockHeight, jhMiner_getCurrentWorkBlockHeight(primecoinBlock->threadIndex), primecoinBlock->serverData.client_shareBits);
				jsonObject_freeObject(jsonReturnValue);
				return false;
			}
		}
		jsonObject_freeObject(jsonReturnValue);
		return false;
	}else if( workData.protocolMode == MINER_PROTOCOL_GBT ){
		// use submitblock
		char* methodName = "submitblock";
		// get multiplier
		CBigNum bnPrimeChainMultiplier;
		bnPrimeChainMultiplier.SetHex(primecoinBlock->mpzPrimeChainMultiplier.get_str(16));
		std::vector<unsigned char> bnSerializeData = bnPrimeChainMultiplier.getvch();
		sint32 lengthBN = bnSerializeData.size();
		//memcpy(xptShareToSubmit->chainMultiplier, &bnSerializeData[0], lengthBN);
		//xptShareToSubmit->chainMultiplierSize = lengthBN;
		// prepare raw data of block
		uint8 dataRaw[512] = {0};
		uint8 proofOfWorkHash[32];
		bool shareAccepted = false;
		memset(dataRaw, 0x00, sizeof(dataRaw));
		*(uint32*)(dataRaw+0) = primecoinBlock->version;
		memcpy((dataRaw+4), primecoinBlock->prevBlockHash, 32);
		memcpy((dataRaw+36), primecoinBlock->merkleRoot, 32);
		*(uint32*)(dataRaw+68) = primecoinBlock->timestamp;
		*(uint32*)(dataRaw+72) = primecoinBlock->nBits;
		*(uint32*)(dataRaw+76) = primecoinBlock->nonce;
		*(uint8*)(dataRaw+80) = lengthBN;
		if( lengthBN > 0x7F )
			printf("Warning: chainMultiplierSize exceeds 0x7F in jhMiner_pushShare_primecoin()\n");
		memcpy(dataRaw+81, &bnSerializeData[0], lengthBN);
		// create stream to write block data to
		stream_t* blockStream = streamEx_fromDynamicMemoryRange(1024*64);
		// write block data
		stream_writeData(blockStream, dataRaw, 80+1+lengthBN);
		// generate coinbase transaction
		bitclientTransaction_t* txCoinbase = bitclient_createCoinbaseTransactionFromSeed(primecoinBlock->seed, primecoinBlock->threadIndex, getBlockTemplateData.height, decodedWalletAddress+1, jhMiner_primeCoin_targetGetMint(primecoinBlock->nBits));
		// write amount of transactions (varInt)
		bitclient_addVarIntFromStream(blockStream, 1);
		bitclient_writeTransactionToStream(blockStream, txCoinbase);
		// map buffer
		sint32 blockDataLength = 0;
		uint8* blockData = (uint8*)streamEx_map(blockStream, &blockDataLength);
		// clean up
		bitclient_destroyTransaction(txCoinbase);
		// prepare buffer to send
		fStr_buffer4kb_t fStrBuffer_parameter;
		fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
		fStr_append(fStr_parameter, "[\""); // \"]
		fStr_addHexString(fStr_parameter, blockData, blockDataLength);
		fStr_append(fStr_parameter, "\"]");
		// send request
		sint32 rpcErrorCode = 0;
		jsonObject_t* jsonReturnValue = NULL;
		jsonReturnValue = jsonClient_request(&jsonRequestTarget, methodName, fStr_parameter, &rpcErrorCode);		
		// clean up rest
		stream_destroy(blockStream);
		free(blockData);
		// process result
		if( jsonReturnValue == NULL ){
			printf("SubmitBlock failed :(\n");
			return false;
		}else{
			// is the bitcoin client happy with the result?
			jsonObject_t* jsonReturnValueRejectReason = jsonObject_getParameter(jsonReturnValue, "result");
			if( jsonObject_getType(jsonReturnValueRejectReason) == JSON_TYPE_NULL ){
				printf("Valid block found!\n");
				jsonObject_freeObject(jsonReturnValue);
				return true;
			}else{
				// :( the client says no
				printf("Coin daemon rejected block :(\n");
				jsonObject_freeObject(jsonReturnValue);
				return false;
			}
		}
	}else if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH ){
		// printf("Queue share\n");
		xptShareToSubmit_t* xptShareToSubmit = (xptShareToSubmit_t*)malloc(sizeof(xptShareToSubmit_t));
		memset(xptShareToSubmit, 0x00, sizeof(xptShareToSubmit_t));
		memcpy(xptShareToSubmit->merkleRoot, primecoinBlock->merkleRoot, 32);
		memcpy(xptShareToSubmit->prevBlockHash, primecoinBlock->prevBlockHash, 32);
		xptShareToSubmit->version = primecoinBlock->version;
		xptShareToSubmit->nBits = primecoinBlock->nBits;
		xptShareToSubmit->nonce = primecoinBlock->nonce;
		xptShareToSubmit->nTime = primecoinBlock->timestamp;
		// set multiplier
		CBigNum bnPrimeChainMultiplier;
		bnPrimeChainMultiplier.SetHex(primecoinBlock->mpzPrimeChainMultiplier.get_str(16));
		std::vector<unsigned char> bnSerializeData = bnPrimeChainMultiplier.getvch();
		sint32 lengthBN = bnSerializeData.size();
		memcpy(xptShareToSubmit->chainMultiplier, &bnSerializeData[0], lengthBN);
		xptShareToSubmit->chainMultiplierSize = lengthBN;
		// todo: Set stuff like sieve size
		if( workData.xptClient && !workData.xptClient->disconnected){
			xptClient_foundShare(workData.xptClient, xptShareToSubmit);
			lastShareSubmit = getTimeMilliseconds();
			return true;
		}else{
			std::cout << "Share submission failed. The client is not connected to the pool." << std::endl;
			return false;
		}
	}
	return false;
}

int queryLocalPrimecoindBlockCount(bool useLocal)
{
	sint32 rpcErrorCode = 0;
	jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getblockcount", NULL, &rpcErrorCode);
	if( jsonReturnValue == NULL )
	{
		printf("getblockcount() failed with %serror code %d\n", (rpcErrorCode>1000)?"http ":"", rpcErrorCode>1000?rpcErrorCode-1000:rpcErrorCode);
		return 0;
	}
	else
	{
		jsonObject_t* jsonResult = jsonObject_getParameter(jsonReturnValue, "result");
		return (int) jsonObject_getNumberValueAsS32(jsonResult);
		jsonObject_freeObject(jsonReturnValue);
	}

	return 0;
}

static double DIFFEXACTONE = 26959946667150639794667015087019630673637144422540572481103610249215.0;
static const uint64_t diffone = 0xFFFF000000000000ull;

double target_diff(const uint32_t  *target)
{
	double targ = 0;
	signed int i;

	for (i = 0; i < 8; i++)
		targ = (targ * 0x100) + target[7 - i];

	return DIFFEXACTONE / ((double)targ ?  targ : 1);
}


std::string HexBits(unsigned int nBits)
{
	union {
		int32_t nBits;
		char cBits[4];
	} uBits;
	uBits.nBits = htonl((int32_t)nBits);
	return HexStr(BEGIN(uBits.cBits), END(uBits.cBits));
}

int getNumThreads(void) {
	// based on code from ceretullis on SO
	uint32_t numcpu = 1; // in case we fall through;
#if defined(__FreeBSD__) || defined(__NetBSD__) || defined(__OpenBSD__)
	int mib[4];
	size_t len = sizeof(numcpu); 

	/* set the mib for hw.ncpu */
	mib[0] = CTL_HW;
#ifdef HW_AVAILCPU
	mib[1] = HW_AVAILCPU;  // alternatively, try HW_NCPU;
#else
	mib[1] = HW_NCPU;
#endif
	/* get the number of CPUs from the system */
	sysctl(mib, 2, &numcpu, &len, NULL, 0);

	if( numcpu < 1 )
	{
		numcpu = 1;
	}

#elif defined(__linux__) || defined(sun) || defined(__APPLE__)
	numcpu = static_cast<uint32_t>(sysconf(_SC_NPROCESSORS_ONLN));
#elif defined(_SYSTYPE_SVR4)
	numcpu = sysconf( _SC_NPROC_ONLN );
#elif defined(hpux)
	numcpu = mpctl(MPC_GETNUMSPUS, NULL, NULL);
#endif

	return numcpu;
}

/*
* Queries the work data from the coin client
* Uses "getblocktemplate"
* Should be called periodically (5-15 seconds) to keep the current block data up-to-date
*/
void jhMiner_queryWork_primecoin_getblocktemplate()
{
	sint32 rpcErrorCode = 0;
	fStr_buffer4kb_t fStrBuffer_parameter;
	fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
	fStr_append(fStr_parameter, "[{\"capabilities\": [\"coinbasetxn\", \"workid\", \"coinbase/append\"]}]");
	jsonObject_t* jsonReturnValue = NULL;
	jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getblocktemplate", fStr_parameter, &rpcErrorCode);
	if( jsonReturnValue == NULL )
	{
		printf("UpdateWork(GetBlockTemplate) failed.\n");
		getBlockTemplateData.isValidData = false;
		return;
	}
	else
	{
		jsonObject_t* jsonResult = jsonObject_getParameter(jsonReturnValue, "result");
		// data
		jsonObject_t* jsonResult_version = jsonObject_getParameter(jsonResult, "version");
		jsonObject_t* jsonResult_previousblockhash = jsonObject_getParameter(jsonResult, "previousblockhash");
		jsonObject_t* jsonResult_target = jsonObject_getParameter(jsonResult, "target");
		//jsonObject_t* jsonResult_mintime = jsonObject_getParameter(jsonResult, "mintime");
		jsonObject_t* jsonResult_curtime = jsonObject_getParameter(jsonResult, "curtime");
		jsonObject_t* jsonResult_bits = jsonObject_getParameter(jsonResult, "bits");
		jsonObject_t* jsonResult_height = jsonObject_getParameter(jsonResult, "height");
		jsonObject_t* jsonResult_coinbaseaux = jsonObject_getParameter(jsonResult, "coinbaseaux");
		jsonObject_t* jsonResult_coinbaseaux_flags = NULL;
		if( jsonResult_coinbaseaux )
			jsonResult_coinbaseaux_flags = jsonObject_getParameter(jsonResult_coinbaseaux, "flags");
		// are all fields present?
		if( jsonResult_version == NULL || jsonResult_previousblockhash == NULL || jsonResult_curtime == NULL || jsonResult_bits == NULL || jsonResult_height == NULL || jsonResult_coinbaseaux_flags == NULL )
		{
			printf("UpdateWork(GetBlockTemplate) failed due to missing fields in the response.\n");
			jsonObject_freeObject(jsonReturnValue);
		}
		// prepare field lengths
		uint32 stringLength_previousblockhash = 0;
		uint32 stringLength_target = 0;
		uint32 stringLength_bits = 0;
		uint32 stringLength_height = 0;
		// get version
		uint32 gbtVersion = jsonObject_getNumberValueAsS32(jsonResult_version);
		// get previous block hash
		uint8* stringData_previousBlockHash = jsonObject_getStringData(jsonResult_previousblockhash, &stringLength_previousblockhash);
		memset(getBlockTemplateData.previousBlockHash, 0, 32);
		jhMiner_parseHexStringLE((char*)stringData_previousBlockHash, stringLength_previousblockhash, getBlockTemplateData.previousBlockHash);
		// get target hash (optional)
		uint8* stringData_target = jsonObject_getStringData(jsonResult_target, &stringLength_target);
		memset(getBlockTemplateData.target, 0, 32);
		if( stringData_target )
			jhMiner_parseHexStringLE((char*)stringData_target, stringLength_target, getBlockTemplateData.target);
		// get timestamp (mintime)
		uint32 gbtTime = jsonObject_getNumberValueAsU32(jsonResult_curtime);
		// get bits
		char bitsTmpText[32]; // temporary buffer so we can add NT
		uint8* stringData_bits = jsonObject_getStringData(jsonResult_bits, &stringLength_bits);
		memcpy(bitsTmpText, stringData_bits, stringLength_bits);
		bitsTmpText[stringLength_bits] = '\0'; 
		uint32 gbtBits = 0;
		sscanf((const char*)bitsTmpText, "%x", &gbtBits);
		// get height
		uint32 gbtHeight = jsonObject_getNumberValueAsS32(jsonResult_height);
		// get coinbase aux flags
		uint32 stringLength_coinbaseauxFlags = 0;
		uint8* stringData_coinbaseauxFlags = jsonObject_getStringData(jsonResult_coinbaseaux_flags, &stringLength_coinbaseauxFlags);
		jhMiner_parseHexString((char*)stringData_coinbaseauxFlags, stringLength_coinbaseauxFlags, getBlockTemplateData.coinbaseauxFlags);
		getBlockTemplateData.coinbaseauxFlagsLength = stringLength_coinbaseauxFlags/2;
		// set remaining number parameters
		getBlockTemplateData.version = gbtVersion;
		getBlockTemplateData.nBits = gbtBits;
		getBlockTemplateData.nTime = gbtTime;
		getBlockTemplateData.height = gbtHeight;
		// done
		jsonObject_freeObject(jsonReturnValue);
		getBlockTemplateData.isValidData = true;
	}
}

void jhMiner_queryWork_primecoin_getwork()
{
	sint32 rpcErrorCode = 0;
	uint32 time1 = getTimeMilliseconds();
	jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getwork", NULL, &rpcErrorCode);
	uint32 time2 = getTimeMilliseconds() - time1;
	// printf("request time: %dms\n", time2);
	if( jsonReturnValue == NULL )
	{
		printf("Getwork() failed with %serror code %d\n", (rpcErrorCode>1000)?"http ":"", rpcErrorCode>1000?rpcErrorCode-1000:rpcErrorCode);
		workData.workEntry[0].dataIsValid = false;
		return;
	}
	else
	{
		jsonObject_t* jsonResult = jsonObject_getParameter(jsonReturnValue, "result");
		jsonObject_t* jsonResult_data = jsonObject_getParameter(jsonResult, "data");
		//jsonObject_t* jsonResult_hash1 = jsonObject_getParameter(jsonResult, "hash1");
		jsonObject_t* jsonResult_target = jsonObject_getParameter(jsonResult, "target");
		jsonObject_t* jsonResult_serverData = jsonObject_getParameter(jsonResult, "serverData");
		//jsonObject_t* jsonResult_algorithm = jsonObject_getParameter(jsonResult, "algorithm");
		if( jsonResult_data == NULL )
		{
			printf("Error :(\n");
			workData.workEntry[0].dataIsValid = false;
			jsonObject_freeObject(jsonReturnValue);
			return;
		}
		// data
		uint32 stringData_length = 0;
		uint8* stringData_data = jsonObject_getStringData(jsonResult_data, &stringData_length);
		//printf("data: %.*s...\n", (sint32)min(48, stringData_length), stringData_data);

		pthread_mutex_lock(&workData.cs);
		jhMiner_parseHexString((char*)stringData_data, std::min<unsigned long>(128*2, stringData_length), workData.workEntry[0].data);
		workData.workEntry[0].dataIsValid = true;
		if (jsonResult_serverData == NULL)
		{
			unsigned char binDataReverse[128];
			for (unsigned int i = 0; i < 128 / 4;++i) 
				((unsigned int *)binDataReverse)[i] = _swapEndianessU32(((unsigned int *)workData.workEntry[0].data)[i]);
			blockHeader_t * blockHeader = (blockHeader_t *)&binDataReverse[0];

			memset(workData.workEntry[0].serverData, 0, 32);
			((serverData_t*)workData.workEntry[0].serverData)->nBitsForShare = blockHeader->nBits;
			((serverData_t*)workData.workEntry[0].serverData)->blockHeight = lastBlockCount;
			useLocalPrimecoindForLongpoll = false;
			bSoloMining = true;

		}
		else
		{
			// get server data
			uint32 stringServerData_length = 0;
			uint8* stringServerData_data = jsonObject_getStringData(jsonResult_serverData, &stringServerData_length);
			memset(workData.workEntry[0].serverData, 0, 32);
			if( jsonResult_serverData )
				jhMiner_parseHexString((char*)stringServerData_data, std::min(128*2, 32*2), workData.workEntry[0].serverData);
		}
		// generate work hash
		uint32 workDataHash = 0x5B7C8AF4;
		for(uint32 i=0; i<stringData_length/2; i++)
		{
			workDataHash = (workDataHash>>29)|(workDataHash<<3);
			workDataHash += (uint32)workData.workEntry[0].data[i];
		}
		workData.workEntry[0].dataHash = workDataHash;
		pthread_mutex_unlock(&workData.cs);
		jsonObject_freeObject(jsonReturnValue);
	}
}


bool SubmitBlock(primecoinBlock_t* pcBlock)
{
	blockHeader_t block = {0};
	memcpy(&block, pcBlock, 80);
	CBigNum bnPrimeChainMultiplier;
	bnPrimeChainMultiplier.SetHex(pcBlock->mpzPrimeChainMultiplier.get_str(16));
	std::vector<unsigned char> primemultiplier = bnPrimeChainMultiplier.getvch();

	//printf("nBits: %d\n", block.nBits);
	//printf("nNonce: %d\n", block.nonce);
	//printf("hashPrevBlock: %s\n", block.prevBlockHash.GetHex().c_str());
	//printf("block   - hashMerkleRoot: %s\n", block.merkleRoot.GetHex().c_str());
	//printf("pcBlock - hashMerkleRoot: %s\n",  HexStr(BEGIN(pcBlock->merkleRoot), END(pcBlock->merkleRoot)).c_str());
	//printf("Multip: %s\n", bnPrimeChainMultiplier.GetHex().c_str());

	if (primemultiplier.size() > 47) {
		error("primemultiplier is too big");
		return false;
	}

	block.primeMultiplier[0] = primemultiplier.size();

	for (size_t i = 0; i < primemultiplier.size(); ++i) 
		block.primeMultiplier[1 + i] = primemultiplier[i];

	for (unsigned int i = 0; i < 128 / 4; ++i) ((unsigned int *)&block)[i] =
		_swapEndianessU32(((unsigned int *)&block)[i]);

	unsigned char pdata[128] = {0};
	memcpy(pdata, &block, 128);
	std::string data_hex = HexStr(BEGIN(pdata), END(pdata));

	fStr_buffer4kb_t fStrBuffer_parameter;
	fStr_t* fStr_parameter = fStr_alloc(&fStrBuffer_parameter, FSTR_FORMAT_UTF8);
	fStr_append(fStr_parameter, "[\""); 
	fStr_append(fStr_parameter, (char *) data_hex.c_str());
	fStr_append(fStr_parameter, "\"]");

	// send request
	sint32 rpcErrorCode = 0;
	//jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "submitblock", fStr_parameter, &rpcErrorCode);
	jsonObject_t* jsonReturnValue = jsonClient_request(&jsonRequestTarget, "getwork", fStr_parameter, &rpcErrorCode);
	if( jsonReturnValue == NULL )
	{
		printf("PushWorkResult failed :(\n");
		return false;
	}
	else
	{
		// rpc call worked, sooooo.. is the server happy with the result?
		jsonObject_t* jsonReturnValueBool = jsonObject_getParameter(jsonReturnValue, "result");
		if( jsonObject_isTrue(jsonReturnValueBool) )
		{
			total_shares++;
			valid_shares++;
			printf("Submit block succeeded! :)\n");
			jsonObject_freeObject(jsonReturnValue);
			return true;
		}
		else
		{
			total_shares++;
			// the server says no to this share :(
			printf("Server rejected the Block. :(\n");
			jsonObject_freeObject(jsonReturnValue);
			return false;
		}
	}
	jsonObject_freeObject(jsonReturnValue);
	return false;
}


/*
* Returns the block height of the most recently received workload
*/
uint32 jhMiner_getCurrentWorkBlockHeight(sint32 threadIndex)
{
	if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
		return ((serverData_t*)workData.workEntry[0].serverData)->blockHeight;	
	else if( workData.protocolMode == MINER_PROTOCOL_GBT )
		return getBlockTemplateData.height;	
	else
		return ((serverData_t*)workData.workEntry[threadIndex].serverData)->blockHeight;
}

/*
* Worker thread mainloop for getwork() mode
*/
void *jhMiner_workerThread_getwork(void *arg){
	uint32_t threadIndex = static_cast<uint32_t>((uintptr_t)arg);
	CSieveOfEratosthenes* psieve = NULL;
	while( true )
	{
		uint8 localBlockData[128];
		// copy block data from global workData
		uint32 workDataHash = 0;
		uint8 serverData[32];
		while( workData.workEntry[0].dataIsValid == false ) Sleep(200);
		pthread_mutex_lock(&workData.cs);
		memcpy(localBlockData, workData.workEntry[0].data, 128);
		//seed = workData.seed;
		memcpy(serverData, workData.workEntry[0].serverData, 32);
		pthread_mutex_unlock(&workData.cs);
		// swap endianess
		for(uint32 i=0; i<128/4; i++)
		{
			*(uint32*)(localBlockData+i*4) = _swapEndianessU32(*(uint32*)(localBlockData+i*4));
		}
		// convert raw data into primecoin block
		primecoinBlock_t primecoinBlock = {0};
		memcpy(&primecoinBlock, localBlockData, 80);
		// we abuse the timestamp field to generate an unique hash for each worker thread...
		primecoinBlock.timestamp += threadIndex;
		primecoinBlock.threadIndex = threadIndex;
		primecoinBlock.xptMode = (workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH);
		// ypool uses a special encrypted serverData value to speedup identification of merkleroot and share data
		memcpy(&primecoinBlock.serverData, serverData, 32);
		// start mining
		if( !BitcoinMiner(&primecoinBlock, psieve, threadIndex))
			break;
		primecoinBlock.mpzPrimeChainMultiplier = 0;
	}
	if( psieve )
	{
		delete psieve;
		psieve = NULL;
	}
	return 0;
}

static const sint64 PRIMECOIN_COIN = 100000000;
static const sint64 PRIMECOIN_CENT = 1000000;
static const unsigned int PRIMECOIN_nFractionalBits = 24;

/*
* Returns value of block
*/
uint64 jhMiner_primeCoin_targetGetMint(unsigned int nBits)
{
	if( nBits == 0 )
		return 0;
	uint64 nMint = 0;
	static uint64 nMintLimit = 999ull * PRIMECOIN_COIN;
	uint64 bnMint = nMintLimit;
	bnMint = (bnMint << PRIMECOIN_nFractionalBits) / nBits;
	bnMint = (bnMint << PRIMECOIN_nFractionalBits) / nBits;
	bnMint = (bnMint / PRIMECOIN_CENT) * PRIMECOIN_CENT;  // mint value rounded to cent
	nMint = bnMint;
	return nMint;
}

/*
* Worker thread mainloop for getblocktemplate mode
*/
void *jhMiner_workerThread_gbt(void *arg)
{
	uint32_t threadIndex = static_cast<uint32_t>((uintptr_t)arg);

	CSieveOfEratosthenes* psieve = NULL;
	while( true )
	{
		//uint8 localBlockData[128];
		primecoinBlock_t primecoinBlock = {0};
		// copy block data from global workData
		//uint32 workDataHash = 0;
		//uint8 serverData[32];
		while( getBlockTemplateData.isValidData == false ) Sleep(200);
		pthread_mutex_lock(&workData.cs);
		// generate work from getBlockTemplate data
		primecoinBlock.threadIndex = threadIndex;
		primecoinBlock.version = getBlockTemplateData.version;
		primecoinBlock.timestamp = getBlockTemplateData.nTime;
		primecoinBlock.nonce = 0;
		primecoinBlock.seed = rand();
		primecoinBlock.nBits = getBlockTemplateData.nBits;
		memcpy(primecoinBlock.prevBlockHash, getBlockTemplateData.previousBlockHash, 32);
		// setup serverData struct
		primecoinBlock.serverData.blockHeight = getBlockTemplateData.height;
		primecoinBlock.serverData.nBitsForShare = getBlockTemplateData.nBits;
		// generate coinbase transaction and merkleroot
		bitclientTransaction_t* txCoinbase = bitclient_createCoinbaseTransactionFromSeed(primecoinBlock.seed, threadIndex, getBlockTemplateData.height, decodedWalletAddress+1, jhMiner_primeCoin_targetGetMint(primecoinBlock.nBits));
		bitclientTransaction_t* txList[64];
		txList[0] = txCoinbase;
		uint32 numberOfTx = 1;
		// generate tx hashes (currently we only support coinbase transaction)
		uint8 txHashList[64*32];
		for(uint32 t=0; t<numberOfTx; t++)
			bitclient_generateTxHash(txList[t], (txHashList+t*32));
		bitclient_calculateMerkleRoot(txHashList, numberOfTx, primecoinBlock.merkleRoot);
		bitclient_destroyTransaction(txCoinbase);
		pthread_mutex_unlock(&workData.cs);
		primecoinBlock.xptMode = false;
		// start mining
		if (!BitcoinMiner(&primecoinBlock, psieve, threadIndex))
			break;
		primecoinBlock.mpzPrimeChainMultiplier = 0;
	}
	if( psieve )
	{
		delete psieve;
		psieve = NULL;
	}
	return 0;
}

/*
* Worker thread mainloop for xpt() mode
*/
void *jhMiner_workerThread_xpt(void *arg){
	uint32_t threadIndex = static_cast<uint32_t>((uintptr_t)arg);

	if(commandlineInput.printDebug){
		std::cout << "#" << threadIndex << ": Launched new worker thread" << std::endl;
	}

	CSieveOfEratosthenes* psieve = NULL;
	while( true )
	{
		if(commandlineInput.printDebug){
			std::cout << "#" << threadIndex << ": Start Loop" << std::endl;
		}
		uint8 localBlockData[128];
		// copy block data from global workData
		uint32 workDataHash = 0;
		uint8 serverData[32];
		int sleepcount =0;
		while( workData.workEntry[threadIndex].dataIsValid == false ){
			sleepcount++;
			if(sleepcount>20){
				if(commandlineInput.printDebug){
					std::cout << "#" << threadIndex << ": Data invalid for 20 ticks" << std::endl;
				}
				break;
			}
			Sleep(50);
		}

		if(commandlineInput.printDebug){
			std::cout << "#" << threadIndex << ": Got new data" << std::endl;
		}
//			std::map<unsigned int, bool> thmap;
//	std::map <unsigned int, bool>::iterator thmapIter;
//	typedef std::pair <unsigned int, bool> thmapKeyVal;
if(thmap.count(threadIndex)){
		if(!thmap.find(threadIndex)->second){
			if(commandlineInput.printDebug){
				std::cout << "#" << threadIndex << ": Signalled to quit" << std::endl;
			}
			//we have been signalled to die
			thmap.erase(threadIndex);
			break;
		}
}

			pthread_mutex_lock(&workData.cs);
			memcpy(localBlockData, workData.workEntry[threadIndex].data, 128);
			memcpy(serverData, workData.workEntry[threadIndex].serverData, 32);
			workDataHash = workData.workEntry[threadIndex].dataHash;
			pthread_mutex_unlock(&workData.cs);
			// convert raw data into primecoin block
			primecoinBlock_t primecoinBlock = {0};
			memcpy(&primecoinBlock, localBlockData, 80);
			// we abuse the timestamp field to generate an unique hash for each worker thread...
			primecoinBlock.timestamp += threadIndex;
			primecoinBlock.threadIndex = threadIndex;
			primecoinBlock.xptMode = (workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH);
			// ypool uses a special encrypted serverData value to speedup identification of merkleroot and share data
			memcpy(&primecoinBlock.serverData, serverData, 32);
			// start mining
			uint32 time1 = getTimeMilliseconds();
			if(commandlineInput.printDebug){
				std::cout << "#" << threadIndex << ": Started Mining" << std::endl;
			}
			if( !BitcoinMiner(&primecoinBlock, psieve, threadIndex))
				break;
			if(commandlineInput.printDebug){
				std::cout << "#" << threadIndex << ": Stopped Mining after " << getTimeMilliseconds()-time1 << "ms" << std::endl;
			}
			primecoinBlock.mpzPrimeChainMultiplier = 0;
	}
	if( psieve )
	{
		delete psieve;
		psieve = NULL;
	}
	return 0;
}


void jhMiner_printHelp()
{
	using namespace std;
	cout << "Usage: jhPrimeminer.exe [options]" << endl;
	cout << "Options:" << endl;
	cout << "  -o, -O <url>                   The miner will connect to this url" << endl;
	cout << "                                 You can specifiy an port after the url using -o url:port" << endl;
	cout << "  -xpt                           Use x.pushthrough protocol" << endl;
	cout << "  -u <string>                    The username (workername) used for login" << endl;
	cout << "  -p <string>                    The password used for login" << endl;
	cout << "  -xpm <wallet address>          When doing solo mining this is the address your mined XPM will be transfered to." << endl;
	cout << "Performance Options:" << endl;
	cout << "  -t <num>                       The number of threads for mining (default = detected cpu cores)" << endl;
	cout << "                                 For most efficient mining, set to number of CPU cores" << endl;
	cout << "  -s <num>                       Set MaxSieveSize range from 200000 - 10000000" << endl;
	cout << "                                 Default is 1500000." << endl;
	cout << "  -d <num>                       Set SievePercentage - range from 1 - 100" << endl;
	cout << "                                 Default is 15 and it's not recommended to use lower values than 8." << endl;
	cout << "                                 It limits how many base primes are used to filter out candidate multipliers in the sieve." << endl;
	cout << "  -r <num>                       Set RoundSievePercentage - range from 3 - 97" << endl;
	cout << "                                 The parameter determines how much time is spent running the sieve." << endl;
	cout << "                                 By default 80% of time is spent in the sieve and 20% is spent on checking the candidates produced by the sieve" << endl;
	cout << "  -primes <num>                  Sets how many prime factors are used to filter the sieve" << endl;
	cout << "                                 Default is MaxSieveSize. Valid range: 300 - 200000000" << endl;
	cout << "  -tune [true|false|1|0]         Enable Auto Tuning" << endl;
	cout << "  -ns <num>                      Null Share Timeout (Default: 0)" << endl;
	cout << "                                 After this many minutes with 0 shares, miner will exit. 0 to disable" << endl;
	cout << "Display Options" << endl;
	cout << "  -quiet                         Enable Quiet mode. Client will only print 1 line per share found" << endl;
	cout << "  -silent                        Enable Silent mode. No output to console." << endl;
	cout << "  -debug                         Enable Debug mode. Verbose output to console." << endl;
	cout << "Example usage:" << endl;
	cout << "  ./jhprimeminer -o http://poolurl.com:10034 -u workername -p workerpass" << endl;
}

void jhMiner_parseCommandline(int argc, char **argv)
{
	using namespace std;
	sint32 cIdx = 1;
	while( cIdx < argc )
	{
		char* argument = argv[cIdx];
		cIdx++;
		if( memcmp(argument, "-o", 3)==0 || memcmp(argument, "-O", 3)==0 )
		{
			// -o
			if( cIdx >= argc )
			{
				cout << "Missing URL after -o option" << endl;
				exit(0);
			}
			if( strstr(argv[cIdx], "http://") )
				commandlineInput.host = fStrDup(strstr(argv[cIdx], "http://")+7);
			else
				commandlineInput.host = fStrDup(argv[cIdx]);
			char* portStr = strstr(commandlineInput.host, ":");
			if( portStr )
			{
				*portStr = '\0';
				commandlineInput.port = atoi(portStr+1);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-u", 3)==0 )
		{
			// -u
			if( cIdx >= argc )
			{
				cout << "Missing username/workername after -u option" << endl;
				exit(0);
			}
			commandlineInput.workername = fStrDup(argv[cIdx], 64);
			cIdx++;
		}
		else if( memcmp(argument, "-p", 3)==0 )
		{
			// -p
			if( cIdx >= argc )
			{
				cout << "Missing password after -p option" << endl;
				exit(0);
			}
			commandlineInput.workerpass = fStrDup(argv[cIdx], 64);
			cIdx++;
		}
		else if( memcmp(argument, "-xpm", 5)==0 )
		{
			// -xpm
			if( cIdx >= argc )
			{
				cout << "Missing wallet address after -xpm option" << endl;
				exit(0);
			}
			commandlineInput.xpmAddress = fStrDup(argv[cIdx], 64);
			cIdx++;
		}
		else if( memcmp(argument, "-t", 3)==0 )
		{
			// -t
			if( cIdx >= argc )
			{
				cout << "Missing thread number after -t option" << endl;
				exit(0);
			}
			commandlineInput.numThreads = atoi(argv[cIdx]);
			if( commandlineInput.numThreads < 1 || commandlineInput.numThreads > 128 )
			{
				cout << "-t parameter out of range" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-s", 3)==0 )
		{
			// -s
			if( cIdx >= argc )
			{
				cout << "Missing number after -s option" << endl;
				exit(0);
			}
			commandlineInput.sieveSize = atoi(argv[cIdx]);
			if( commandlineInput.sieveSize < 200000 || commandlineInput.sieveSize > 40000000 )
			{
				printf("-s parameter out of range, must be between 200000 - 10000000");
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-d", 3)==0 )
		{
			// -s
			if( cIdx >= argc )
			{
				cout << "Missing number after -d option" << endl;
				exit(0);
			}
			commandlineInput.sievePercentage = atoi(argv[cIdx]);
			if( commandlineInput.sievePercentage < 1 || commandlineInput.sievePercentage > 100 )
			{
				cout << "-d parameter out of range, must be between 1 - 100" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-r", 3)==0 )
		{
			// -s
			if( cIdx >= argc )
			{
				cout << "Missing number after -r option" << endl;
				exit(0);
			}
			commandlineInput.roundSievePercentage = atoi(argv[cIdx]);
			if( commandlineInput.roundSievePercentage < 3 || commandlineInput.roundSievePercentage > 97 )
			{
				cout << "-r parameter out of range, must be between 3 - 97" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-primes", 8)==0 )
		{
			// -primes
			if( cIdx >= argc )
			{
				cout << "Missing number after -primes option" << endl;
				exit(0);
			}
			commandlineInput.sievePrimeLimit = atoi(argv[cIdx]);
			if( commandlineInput.sievePrimeLimit < 300 || commandlineInput.sievePrimeLimit > 200000000 )
			{
				cout << "-primes parameter out of range, must be between 300 - 200000000" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-c", 3)==0 )
		{
			// -c
			if( cIdx >= argc )
			{
				cout << "Missing number after -c option" << endl;
				exit(0);
			}
			commandlineInput.L1CacheElements = atoi(argv[cIdx]);
			if( commandlineInput.L1CacheElements < 300 || commandlineInput.L1CacheElements > 200000000  || commandlineInput.L1CacheElements % 32 != 0) 
			{
				cout << "-c parameter out of range, must be between 64000 - 2000000 and multiply of 32" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-m", 3)==0 )
		{
			// -primes
			if( cIdx >= argc )
			{
				cout << "Missing number after -m option" << endl;
				exit(0);
			}
			commandlineInput.primorialMultiplier = atoi(argv[cIdx]);
			if( commandlineInput.primorialMultiplier < 5 || commandlineInput.primorialMultiplier > 1009) 
			{
				cout << "-m parameter out of range, must be between 5 - 1009 and should be a prime number" << endl;
				exit(0);
			}
			cIdx++;
		}
else if (memcmp(argument, "-m2", 3) == 0) {
			// -primes
			if (cIdx >= argc) {
				cout << "Missing number after -m2 option" << endl;
				exit(0);
			}
			commandlineInput.primorialMultiplier2 = atoi(argv[cIdx]);
			if (commandlineInput.primorialMultiplier2 < 5
					|| commandlineInput.primorialMultiplier2 > 1009) {
				cout
						<< "-m2 parameter out of range, must be between 5 - 1009 and should be a prime number"
						<< endl;
				exit(0);
			}
			cIdx++;
		} else if (memcmp(argument, "-m3", 3) == 0) {
			// -primes
			if (cIdx >= argc) {
				cout << "Missing number after -m3 option" << endl;
				exit(0);
			}
			commandlineInput.primorialMultiplier3 = atoi(argv[cIdx]);
			if (commandlineInput.primorialMultiplier3 < 5
					|| commandlineInput.primorialMultiplier3 > 1009) {
				cout
						<< "-m3 parameter out of range, must be between 5 - 1009 and should be a prime number"
						<< endl;
				exit(0);
			}
			cIdx++;
		} else if (memcmp(argument, "-m4", 3) == 0) {
			// -primes
			if (cIdx >= argc) {
				cout << "Missing number after -m4 option" << endl;
				exit(0);
			}
			commandlineInput.primorialMultiplier4 = atoi(argv[cIdx]);
			if (commandlineInput.primorialMultiplier4 < 5
					|| commandlineInput.primorialMultiplier4 > 1009) {
				cout
						<< "-m4 parameter out of range, must be between 5 - 1009 and should be a prime number"
						<< endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-tune", 6)==0 )
		{
			// -tune
			commandlineInput.enableCacheTunning = true;
		}
		else if( memcmp(argument, "-target", 8)==0 )
		{
			// -target
			if( cIdx >= argc )
			{
				cout << "Missing number after -target option" << endl;
				exit(0);
			}
			commandlineInput.targetOverride = atoi(argv[cIdx]);
			if( commandlineInput.targetOverride < 9 || commandlineInput.targetOverride > 100 )
			{
				printf("-target parameter out of range, must be between 9 - 100");
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-bttarget", 10)==0 )
		{
			// -bttarget
			if( cIdx >= argc )
			{
				printf("Missing number after -bttarget option\n");
				exit(0);
			}
			commandlineInput.targetBTOverride = atoi(argv[cIdx]);
			if( commandlineInput.targetBTOverride < 9 || commandlineInput.targetBTOverride > 100 )
			{
				printf("-bttarget parameter out of range, must be between 9 - 100");
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-primorial", 11)==0 )
		{
			// -primorial
			if( cIdx >= argc )
			{
				cout << "Missing number after -primorial option" << endl;
				exit(0);
			}
			commandlineInput.initialPrimorial = atoi(argv[cIdx]);
			if( commandlineInput.initialPrimorial < 11 || commandlineInput.initialPrimorial > 1000 )
			{
				cout << "-primorial parameter out of range, must be between 11 - 1000" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-se", 4)==0 )
		{
			// -target
			if( cIdx >= argc )
			{
				cout << "Missing number after -se option" << endl;
				exit(0);
			}
			commandlineInput.sieveExtensions = atoi(argv[cIdx]);
			if( commandlineInput.sieveExtensions < 0 || commandlineInput.sieveExtensions > 15 )
			{
				cout << "-se parameter out of range, must be between 0 - 15" << endl;
				exit(0);
			}
			cIdx++;
		}
		else if( memcmp(argument, "-ns", 4)==0 )
		{
			// -nullShareTimeout
			if( cIdx >= argc )
			{
				cout << "Missing number after -ns option" << endl;
				exit(0);
			}
			commandlineInput.nullShareTimeout = atoi(argv[cIdx]);
			cIdx++;
		}

		else if( memcmp(argument, "-debug", 6)==0 )
		{
			// -debug
			commandlineInput.printDebug = true;
			cout << "Enabled debug spam!" << endl;

		}

		else if( memcmp(argument, "-noinput", 8)==0 )
		{
			// -noinput
			commandlineInput.disableInput = true;
		}

		else if( memcmp(argument, "-quiet", 7)==0 )
		{
			bool arg = false;
			commandlineInput.quiet = true;
		}
		else if( memcmp(argument, "-silent", 8)==0 )
		{
			bool arg = false;
			commandlineInput.silent = true;
		}
		else if( memcmp(argument, "-xpt", 5)==0 )
		{
			commandlineInput.useXPT = true;
		}
		else if( memcmp(argument, "-help", 6)==0 || memcmp(argument, "--help", 7)==0 )
		{
			jhMiner_printHelp();
			exit(0);
		}
		else
		{
			cout << "'" << argument << "' is an unknown option." << endl;
			cout << "Invoke with -help for more info" << endl; 
			exit(-1);
		}
	}
	if( argc <= 1 )
	{
		jhMiner_printHelp();
		exit(0);
	}
}



/*
bool bOptimalL1SearchInProgress = false;
void *CacheAutoTuningWorkerThread(void * arg)
{
	bool bEnabled = (bool) arg;

	if (bOptimalL1SearchInProgress || !bEnabled)
		return 0;
	bOptimalL1SearchInProgress = true;

	DWORD startTime = getTimeMilliseconds();	
	unsigned int nL1CacheElementsStart = 64000;
	unsigned int nL1CacheElementsMax   = 2560000;
	unsigned int nL1CacheElementsIncrement = 32000;
	BYTE nSampleSeconds = 60;

	unsigned int nL1CacheElements = primeStats.nL1CacheElements;
	std::map <unsigned int, unsigned int> mL1Stat;
	std::map <unsigned int, unsigned int>::iterator mL1StatIter;
	typedef std::pair <unsigned int, unsigned int> KeyVal;

	primeStats.nL1CacheElements = nL1CacheElementsStart;

	long nCounter = 0;
	while (true && bEnabled && !appQuitSignal)
	{		
		primeStats.nWaveTime = 0;
		primeStats.nWaveRound = 0;
		primeStats.nTestTime = 0;
		primeStats.nTestRound = 0;
		Sleep(nSampleSeconds*1000);
		DWORD waveTime = primeStats.nWaveTime;
		if (bEnabled)
			nCounter ++;
		if (nCounter <=1) 
			continue;// wait a litle at the beginning

		nL1CacheElements = primeStats.nL1CacheElements;
		mL1Stat.insert( KeyVal(primeStats.nL1CacheElements, primeStats.nWaveRound == 0 ? 0xFFFF : primeStats.nWaveTime / primeStats.nWaveRound));
		if (nL1CacheElements < nL1CacheElementsMax)
			primeStats.nL1CacheElements += nL1CacheElementsIncrement;
		else
		{
			// set the best value
			DWORD minWeveTime = mL1Stat.begin()->second;
			unsigned int nOptimalSize = nL1CacheElementsStart;
			for (  mL1StatIter = mL1Stat.begin(); mL1StatIter != mL1Stat.end(); mL1StatIter++ )
			{
				if (mL1StatIter->second < minWeveTime)
				{
					minWeveTime = mL1StatIter->second;
					nOptimalSize = mL1StatIter->first;
				}
			}
			printf("The optimal L1CacheElement size is: %u\n", nOptimalSize);
			primeStats.nL1CacheElements = nOptimalSize;
			nL1CacheElements = nOptimalSize;
			bOptimalL1SearchInProgress = false;
			break;
		}			
		printf("Auto Tuning in progress: %u %%\n", ((primeStats.nL1CacheElements  - nL1CacheElementsStart)*100) / (nL1CacheElementsMax - nL1CacheElementsStart));

		float ratio = primeStats.nWaveTime == 0 ? 0 : ((float)primeStats.nWaveTime / (float)(primeStats.nWaveTime + primeStats.nTestTime)) * 100.0;
		printf("WaveTime %u - Wave Round %u - L1CacheSize %u - TotalWaveTime: %u - TotalTestTime: %u - Ratio: %.01f / %.01f %%\n", 
			primeStats.nWaveRound == 0 ? 0 : primeStats.nWaveTime / primeStats.nWaveRound, primeStats.nWaveRound, nL1CacheElements,
			primeStats.nWaveTime, primeStats.nTestTime, ratio, 100.0 - ratio);

	}
}

*/










void PrintCurrentSettings()
{
	using namespace std;
	unsigned long uptime = (getTimeMilliseconds() - primeStats.startTime);

	unsigned int days = uptime / (24 * 60 * 60 * 1000);
	uptime %= (24 * 60 * 60 * 1000);
	unsigned int hours = uptime / (60 * 60 * 1000);
	uptime %= (60 * 60 * 1000);
	unsigned int minutes = uptime / (60 * 1000);
	uptime %= (60 * 1000);
	unsigned int seconds = uptime / (1000);

	cout << endl << "--------------------------------------------------------------------------------"<< endl;
	cout << "Worker name (-u): " << commandlineInput.workername << endl;
	cout << "Number of mining threads (-t): " << commandlineInput.numThreads << endl;
	cout << "Sieve Size (-s): " << nMaxSieveSize << endl;
	cout << "Sieve Percentage (-d): " << nSievePercentage << endl;
	cout << "Round Sieve Percentage (-r): " << nRoundSievePercentage << endl;
	cout << "Prime Limit (-primes): " << commandlineInput.sievePrimeLimit << endl;
	cout << "Primorial Multiplier (-m): " << primeStats.nPrimorialMultiplier << endl;
	cout << "L1CacheElements (-c): " << primeStats.nL1CacheElements << endl;
	cout << "Chain Length Target (-target): " << nOverrideTargetValue << endl;
	cout << "BiTwin Length Target (-bttarget): " << nOverrideBTTargetValue << endl;
	cout << "Sieve Extensions (-se): " << nSieveExtensions << endl;
	cout << "Total Runtime: " << days << " Days, " << hours << " Hours, " << minutes << " minutes, " << seconds << " seconds" << endl;
	if (!bSoloMining)
		cout << "Total Share Value submitted to the Pool: " << primeStats.fTotalSubmittedShareValue << endl;	
	cout << "--------------------------------------------------------------------------------" << endl;
}

bool appQuitSignal = false;

void *input_thread(void *){
	static struct termios oldt, newt;
	/*tcgetattr gets the parameters of the current terminal
	STDIN_FILENO will tell tcgetattr that it should write the settings
	of stdin to oldt*/
	tcgetattr( STDIN_FILENO, &oldt);
	/*now the settings will be copied*/
	newt = oldt;
	/*ICANON normally takes care that one line at a time will be processed
	that means it will return if it sees a "\n" or an EOF or an EOL*/
	newt.c_lflag &= ~(ICANON);          
	/*Those new settings will be set to STDIN
	TCSANOW tells tcsetattr to change attributes immediately. */
	tcsetattr( STDIN_FILENO, TCSANOW, &newt);
	while (!commandlineInput.disableInput) {
		int input = getchar();
		switch (input) {
		case 'q': case 'Q': case 3: //case 27:
			appQuitSignal = true;
			Sleep(3200);
			std::exit(0);
			/*restore the old settings*/
			tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
			return 0;
			break;
		case 'c': case 'C':
			commandlineInput.startL1CacheTune = true;
			break;
		case 'x': case 'X':
			commandlineInput.startSieveTune = true;
			break;
		case 'z': case 'Z':
			commandlineInput.startPrimeTune = true;
			break;
		case 'v': case 'V':
			commandlineInput.startPrimorialTune = true;
			break;
		case 'b': case 'B':
			commandlineInput.startSieveExtensionTune = true;
			break;
		case 'h': case 'H':
			// explicit cast to ref removes g++ warning but might be dumb, dunno
			if (!PrimeTableGetPreviousPrime((unsigned int &) primeStats.nPrimorialMultiplier))
				error("PrimecoinMiner() : primorial decrement overflow");	
			std::cout << "Primorial Multiplier: " << primeStats.nPrimorialMultiplier << std::endl;
			break;
		case 'y': case 'Y':
			// explicit cast to ref removes g++ warning but might be dumb, dunno
			if (!PrimeTableGetNextPrime((unsigned int &)  primeStats.nPrimorialMultiplier))
				error("PrimecoinMiner() : primorial increment overflow");
			std::cout << "Primorial Multiplier: " << primeStats.nPrimorialMultiplier << std::endl;
			break;
		case 'p': case 'P':
			bEnablenPrimorialMultiplierTuning = !bEnablenPrimorialMultiplierTuning;
			std::cout << "Primorial Multiplier Auto Tuning was " << (bEnablenPrimorialMultiplierTuning ? "Enabled": "Disabled") << std::endl;
			break;
		case 's': case 'S':			
			PrintCurrentSettings();
			break;
		case 'u': case 'U':
			if (nMaxSieveSize < 10000000)
				nMaxSieveSize += 32000;
			std::cout << "Sieve size: " << nMaxSieveSize << std::endl;
			break;
		case 'j': case 'J':
			if (nMaxSieveSize > 100000)
				nMaxSieveSize -= 32000;
			std::cout << "Sieve size: " << nMaxSieveSize << std::endl;
			break;
		case 't': case 'T':
			if( nRoundSievePercentage < 98)
				nRoundSievePercentage++;
			std::cout << "Round Sieve Percentage: " << nRoundSievePercentage << "%" << std::endl;
			break;
		case 'g': case 'G':
			if( nRoundSievePercentage > 2)
				nRoundSievePercentage--;
			std::cout << "Round Sieve Percentage: " << nRoundSievePercentage << "%" << std::endl;
			break;
		case 'o': case 'O':
			nSieveExtensions++;
			break;
		case 'l': case 'L':
			nSieveExtensions--;
			break;
		}
		Sleep(20);
	}
	/*restore the old settings*/
	tcsetattr( STDIN_FILENO, TCSANOW, &oldt);
	return 0;
}

void OnNewBlock(double nBitsShare, double nBits, unsigned long blockHeight)
{
	double totalRunTime = (double)(getTimeMilliseconds() - primeStats.startTime);
	double poolDiff = GetPrimeDifficulty( nBitsShare );
	double blockDiff = GetPrimeDifficulty( nBits );
	if(!commandlineInput.silent && commandlineInput.quiet){
		std::cout << "New Block: " << blockHeight << " - Diff: " << blockDiff << " / " << poolDiff << std::endl;
	}
	if(!commandlineInput.silent && !commandlineInput.quiet){
		std::cout << std::endl << "--------------------------------------------------------------------------------" << std::endl;
		std::cout << "New Block: " << blockHeight << " - Diff: " << blockDiff << " / " << poolDiff << std::endl;
		std::cout << "Valid/Total shares: [ " << valid_shares << " / " << total_shares << " ]  -  Max diff: " << primeStats.bestPrimeChainDifficultySinceLaunch << std::endl;
		for (int i = 6; i <= std::max(6,(int)primeStats.bestPrimeChainDifficultySinceLaunch); i++){
			double sharePerHour = ((double)primeStats.chainCounter[0][i] / totalRunTime) * 3600000.0;
			std::cout << i << "ch/h: " << sharePerHour << " - " << primeStats.chainCounter[0][i] << " [ " << primeStats.chainCounter[1][i] << " / " << primeStats.chainCounter[2][i] << " / " << primeStats.chainCounter[3][i] << " ]" << std::endl;
		}
		if (!bSoloMining)
			std::cout << "Share Value submitted - Last Block/Total: " << primeStats.fBlockShareValue << " / " << primeStats.fTotalSubmittedShareValue << std::endl;
		std::cout << "Current Primorial Value: " << primeStats.nPrimorialMultiplier << std::endl;
		std::cout << "--------------------------------------------------------------------------------" << std::endl;
	}
	primeStats.fBlockShareValue = 0;
	multiplierSet.clear();
}



void PrintStat()
{
	if( workData.workEntry[0].dataIsValid )
	{
		double totalRunTime = (double)(getTimeMilliseconds() - primeStats.startTime);
		double statsPassedTime = (double)(getTimeMilliseconds() - primeStats.primeLastUpdate);
		if( statsPassedTime < 1.0 ) statsPassedTime = 1.0; // avoid division by zero
		if( totalRunTime < 1.0 ) totalRunTime = 1.0; // avoid division by zero
		double primesPerSecond = (double)primeStats.primeChainsFound / (statsPassedTime / 1000.0);
		float avgCandidatesPerRound = (double)primeStats.nCandidateCount / primeStats.nSieveRounds;
		float sievesPerSecond = (double)primeStats.nSieveRounds / (statsPassedTime / 1000.0);
		float bestChainSinceLaunch = GetChainDifficulty(primeStats.bestPrimeChainDifficultySinceLaunch);
		float shareValuePerHour = primeStats.fShareValue / totalRunTime * 3600000.0;
		printf("\nVal/h:%8f - PPS:%d - SPS:%.03f - ACC:%d\n", shareValuePerHour, (sint32)primesPerSecond, sievesPerSecond, (sint32)avgCandidatesPerRound);
		printf(" Chain/Hr: ");

		for(int i=6; i<=9; i++)
		{
			printf("%2d: %8.02f ", i, ((double)primeStats.chainCounter[0][i] / totalRunTime) * 3600000.0);
		}
		if (bestChainSinceLaunch >= 10)
		{
			printf("\n           ");
			for(int i=10; i<=13; i++)
			{
				printf("%2d: %8.02f ", i, ((double)primeStats.chainCounter[0][i] / statsPassedTime) * 3600000.0);
			}
		}
		printf("\n\n");
	}
}

/*
* Mainloop when using getblocktemplate mode
*/
int jhMiner_main_gbtMode()
{
	// main thread, query work every x seconds
	sint32 gbtloopCounter = 0;
	while( true )
	{
		// query new work
		jhMiner_queryWork_primecoin_getblocktemplate();
		// calculate stats every second tick
		if( gbtloopCounter&1 )
		{
			PrintStat();
			primeStats.primeLastUpdate = getTimeMilliseconds();
			primeStats.primeChainsFound = 0;
			primeStats.nCandidateCount = 0;
			primeStats.nSieveRounds = 0;
		}		
		// wait and check some stats
		if (appQuitSignal)
			return 0;
		int currentBlockCount = getBlockTemplateData.height;
		if (currentBlockCount != lastBlockCount && lastBlockCount > 0)
		{	
			serverData_t* serverData = (serverData_t*)workData.workEntry[0].serverData; 				
			// update serverData
			serverData->nBitsForShare = getBlockTemplateData.nBits;
			serverData->blockHeight = getBlockTemplateData.height;
			OnNewBlock(serverData->nBitsForShare, serverData->nBitsForShare, serverData->blockHeight);
			lastBlockCount = currentBlockCount;
			break;
		}
		lastBlockCount = currentBlockCount;
		gbtloopCounter++;
	}
	return 0;
}


/*
* Mainloop when using getwork() mode
*/
int jhMiner_main_getworkMode()
{
	// main thread, query work every 8 seconds
	sint32 loopCounter = 0;
	while( true )
	{
		// query new work
		jhMiner_queryWork_primecoin_getwork();
		// calculate stats every second tick
		if( loopCounter&1 )
		{
			PrintStat();
			primeStats.primeLastUpdate = getTimeMilliseconds();
			primeStats.primeChainsFound = 0;
			primeStats.nCandidateCount = 0;
			primeStats.nSieveRounds = 0;
		}		
		// wait and check some stats
		uint32 time_updateWork = getTimeMilliseconds();
		while( true )
		{
			if (appQuitSignal)
				return 0;
			uint32 passedTime = getTimeMilliseconds() - time_updateWork;
			if( passedTime >= 4000 )
				break;

			int currentBlockCount = queryLocalPrimecoindBlockCount(useLocalPrimecoindForLongpoll);

			if (currentBlockCount != lastBlockCount && lastBlockCount > 0)
			{	
				serverData_t * serverData = (serverData_t*)workData.workEntry[0].serverData; 				
				OnNewBlock( serverData->nBitsForShare, serverData->nBitsForShare, serverData->blockHeight);
				lastBlockCount = currentBlockCount;
				break;
			}
			lastBlockCount = currentBlockCount;
			Sleep(200);
		}
		loopCounter++;
	}
	return 0;
}


void getIpAddr(char* ipText, char* addr) {
	struct addrinfo hints, *res;
	memset(&hints, 0, sizeof(hints));
	hints.ai_family = AF_INET;
	getaddrinfo(addr, 0, &hints, &res);
	inet_ntop(AF_INET, &((struct sockaddr_in *)res->ai_addr)->sin_addr, ipText, INET_ADDRSTRLEN);
}

/*
* Mainloop when using xpt mode
*/
int jhMiner_main_xptMode(){
	// main thread, don't query work, just wait and process
	sint32 loopCounter = 0;
	uint32 xptWorkIdentifier = 0xFFFFFFFF;
	//unsigned long lastFiveChainCount = 0;
	//unsigned long lastFourChainCount = 0;
	bool bOptimalSieveSearchInProgress = false;
	bool bOptimalSieveExtnSearchInProgress = false;

	unsigned int nSieveElementsStart = 128000;
	unsigned int nSieveElementsMax   = 5120000;
	unsigned int nSieveElementsIncrement = 64000;
	unsigned int nSieveRounds = 0;
	unsigned int nSieveExtnStart = 4;
	unsigned int nSieveExtnMax   = 12;
	unsigned int nSieveExtnRounds   = 0;
	unsigned int nSieveExtnStartBlock = 0;

	std::map <unsigned int, unsigned int> mSieveStat;
	std::map <unsigned int, float> mSieveExtnStat;
	std::map <unsigned int, unsigned int>::iterator mSieveStatIter;
	std::map <unsigned int, float>::iterator mSieveExtnStatIter;
	typedef std::pair <unsigned int, unsigned int> SieveKeyVal;
	typedef std::pair <unsigned int, float> SieveExtnKeyVal;




	while( true ){
		if (appQuitSignal) return 0;

		if(commandlineInput.startSieveTune && !bOptimalSieveSearchInProgress){
			bOptimalSieveSearchInProgress = true;
			nMaxSieveSize = nSieveElementsStart;
			mSieveStat.clear();
			nSieveRounds = 0;
			commandlineInput.startSieveTune = false;
			std::cout << "Sieve Auto tune started" << std::endl;
		}


		if(commandlineInput.startSieveExtensionTune && !bOptimalSieveExtnSearchInProgress){
			bOptimalSieveExtnSearchInProgress = true;
			nSieveExtensions = nSieveExtnStart;
			mSieveExtnStat.clear();
			nSieveExtnRounds = 0;
			nSieveExtnStartBlock = workData.xptClient->blockWorkInfo.height;
			commandlineInput.startSieveExtensionTune = false;
			std::cout << "Sieve Extension Auto tune started" << std::endl;
		}


		if(commandlineInput.startPrimorialTune){
			bEnablenPrimorialMultiplierTuning = !bEnablenPrimorialMultiplierTuning;
			std::cout << "Primorial Multiplier Auto Tuning was " << (bEnablenPrimorialMultiplierTuning ? "Enabled": "Disabled") << std::endl;
			commandlineInput.startPrimorialTune = false;
		}


		// calculate stats every 10 seconds or so
		if( loopCounter>200  && workData.workEntry[0].dataIsValid){
			//collect general stats
			double totalRunTime = (double)(getTimeMilliseconds() - primeStats.startTime);
			double statsPassedTime = (double)(getTimeMilliseconds() - primeStats.primeLastUpdate);
			double primesPerSecond = (double)primeStats.primeChainsFound / (statsPassedTime / 1000.0);
			float avgCandidatesPerRound = (double)primeStats.nCandidateCount / primeStats.nSieveRounds;
			float sievesPerSecond = (double)primeStats.nSieveRounds / (statsPassedTime / 1000.0);
			uint32 bestDifficulty = primeStats.bestPrimeChainDifficulty;
			float primeDifficulty = GetChainDifficulty(bestDifficulty);
			unsigned int nps = floor(sievesPerSecond * (nMaxSieveSize + (nSieveExtensions * (nMaxSieveSize/2))));
			primeStats.bestPrimeChainDifficultySinceLaunch = std::max<double>((double)primeStats.bestPrimeChainDifficultySinceLaunch, primeDifficulty);
			float shareValuePerHour = primeStats.fShareValue / totalRunTime * 3600000.0;

			if(bOptimalSieveSearchInProgress){
				if(commandlineInput.printDebug)
					printf("Auto Sieve Tuning in progress: %u %% - SieveSize %u - NPS %u\n", ((nMaxSieveSize  - nSieveElementsStart)*100) / (nSieveElementsMax - nSieveElementsStart), nMaxSieveSize, nps);
				std::pair<std::map<unsigned int, unsigned int>::iterator,bool> ret;
				ret = mSieveStat.insert( SieveKeyVal(nMaxSieveSize, nps));
				if(ret.second==false){
					ret.first->second = (ret.first->second + nps)/2; 
				}
				if (nMaxSieveSize < nSieveElementsMax && nSieveRounds >= 2){
					nMaxSieveSize += nSieveElementsIncrement;
					nSieveRounds = 0;
					primeStats.nL1CacheElements = nMaxSieveSize;
				}else if(nMaxSieveSize < nSieveElementsMax && nSieveRounds < 2){
					nSieveRounds++;
				}else{
					// set the best value
					unsigned int maxnps = mSieveStat.begin()->second;
					unsigned int nOptimalSize = nSieveElementsStart;
					for (  mSieveStatIter = mSieveStat.begin(); mSieveStatIter != mSieveStat.end(); mSieveStatIter++ ){
						if(commandlineInput.printDebug){
							std::cout << "SieveSize: " << mSieveStatIter->first << " - NPS: " << mSieveStatIter->second << std::endl;
						}

						if (mSieveStatIter->second > maxnps){
							maxnps = mSieveStatIter->second;
							nOptimalSize = mSieveStatIter->first;
						}
					}
					printf("The optimal Sieve size is: %u\n", nOptimalSize);
					primeStats.nSieveElements = nOptimalSize;
					nMaxSieveSize = nOptimalSize;
					primeStats.nL1CacheElements = nMaxSieveSize;
					bOptimalSieveSearchInProgress = false;
				}			
			}


			if(bOptimalSieveExtnSearchInProgress){
				if(nSieveExtnStartBlock < workData.xptClient->blockWorkInfo.height){
					if(commandlineInput.printDebug)
						printf("Auto Sieve Extension Tuning in progress: %u %% - SieveSize %u - Sieve Extensions %u - SPS %.01f\n", ((nSieveExtensions  - nSieveExtnStart)*100) / (nSieveExtnMax - nSieveExtnStart), nMaxSieveSize, nSieveExtensions, sievesPerSecond);
					nSieveExtnRounds++;
					std::pair<std::map<unsigned int, float>::iterator,bool> ret;
					ret = mSieveExtnStat.insert( SieveExtnKeyVal(nSieveExtensions, sievesPerSecond));
					if(ret.second==false){
						ret.first->second = (ret.first->second + sievesPerSecond)/2; 
					}
					if (nSieveExtensions == nSieveExtnMax){
						// set the best value
						float maxsps = mSieveExtnStat.begin()->second;
						unsigned int nOptimalSize = nSieveExtnStart;
						for (  mSieveExtnStatIter = mSieveExtnStat.begin(); mSieveExtnStatIter != mSieveExtnStat.end(); mSieveExtnStatIter++ ){
							if(commandlineInput.printDebug){
								std::cout << "Sieve Extensions: " << mSieveExtnStatIter->first << " - SPS: " << mSieveExtnStatIter->second << std::endl;
							}
							if (mSieveExtnStatIter->second > maxsps){
								maxsps = mSieveExtnStatIter->second;
								nOptimalSize = mSieveExtnStatIter->first;
							}
						}
						printf("The optimal Sieve Extensions is: %u\n", nOptimalSize);
						nSieveExtensions = nOptimalSize;
						bOptimalSieveExtnSearchInProgress = false;
					}			
				}
			}



/*			if(bEnablenPrimorialMultiplierTuning){
				float ratio = primeStats.nWaveTime == 0 ? 0 : ((float)primeStats.nWaveTime / (float)(primeStats.nWaveTime + primeStats.nTestTime)) * 100.0;
				if(commandlineInput.printDebug){
					std::cout << "Sieve/Test Ratio: " << ratio << std::endl;
				}
				if (ratio > 0 && ratio > nRoundSievePercentage + 5){
					if (!PrimeTableGetNextPrime((unsigned int &)  primeStats.nPrimorialMultiplier))
						error("PrimecoinMiner() : primorial increment overflow");
					printf( "Sieve/Test ratio: %.01f / %.01f %%  - New PrimorialMultiplier: %u\n", ratio, 100.0 - ratio,  primeStats.nPrimorialMultiplier);
				}
				else if (ratio < nRoundSievePercentage - 5){
					if ( primeStats.nPrimorialMultiplier > 2){
						if (!PrimeTableGetPreviousPrime((unsigned int &) primeStats.nPrimorialMultiplier))
							error("PrimecoinMiner() : primorial decrement overflow");
						printf( "Sieve/Test ratio: %.01f / %.01f %%  - New PrimorialMultiplier: %u\n", ratio, 100.0 - ratio,  primeStats.nPrimorialMultiplier);
					}
				}
			}
*/





			if(!commandlineInput.silent && !commandlineInput.quiet){
				std::cout << "Val/h: " << shareValuePerHour << " - PPS: " << (sint32)primesPerSecond << " - SPS: " << sievesPerSecond << " - ACC: " << (sint32)avgCandidatesPerRound  << " - Primorial: " << primeStats.nPrimorialMultiplier << std::endl;
				std::cout << std::fixed << std::showpoint << std::setprecision(2);
				std::cout << "MNPS: " << nps/1000000 << std::endl;
				std::cout << std::fixed << std::showpoint << std::setprecision(8);
				std::cout << " Chain/Hr:  ";
				for(int i=6; i<=std::max(6,(int)primeStats.bestPrimeChainDifficultySinceLaunch); i++){
					std::cout << i << ": " <<  std::setprecision(2) << (((double)primeStats.chainCounter[0][i] / totalRunTime) * 3600000.0) << " ";
				}
				std::cout << std::setprecision(8);
				std::cout << std::endl;
			}





			//reset counters
			loopCounter = 0;
			primeStats.primeLastUpdate = getTimeMilliseconds();
			primeStats.primeChainsFound = 0;
			primeStats.nCandidateCount = 0;
			primeStats.nSieveRounds = 0;
			primeStats.primeChainsFound = 0;
			primeStats.bestPrimeChainDifficulty = 0;
			primeStats.nWaveTime = 0;
			primeStats.nWaveRound = 0;
			primeStats.nTestTime = 0;
			primeStats.nTestRound = 0;
		}





		// wait and check some stats
		//		uint64 time_updateWork = getTimeMilliseconds();
		//		while( true ){
		//			uint64 tickCount = getTimeMilliseconds();
		//			uint64 passedTime = tickCount - time_updateWork;
		//			if( passedTime >= 4000 ) break;



		xptClient_process(workData.xptClient);
		char* disconnectReason = false;
		if( workData.xptClient == NULL || xptClient_isDisconnected(workData.xptClient, &disconnectReason) ){
			// disconnected, mark all data entries as invalid
			for(uint32 i=0; i<128; i++) workData.workEntry[i].dataIsValid = false;
			if(!commandlineInput.silent && !commandlineInput.quiet){
				std::cout << "xpt: Disconnected, auto reconnect in 30 seconds"<<std::endl;
			}
			if( workData.xptClient && disconnectReason ){
				if(!commandlineInput.silent && !commandlineInput.quiet){
					std::cout << "xpt: Disconnect reason: " << disconnectReason << std::endl;
				}
			}
			Sleep(30*1000);
			if( workData.xptClient ) xptClient_free(workData.xptClient);
			xptWorkIdentifier = 0xFFFFFFFF;
			while( true ){
				// resolve hostname before reconnecting
				getIpAddr(jsonRequestTarget.ip, commandlineInput.host);
				workData.xptClient = xptClient_connect(&jsonRequestTarget, commandlineInput.numThreads);
				if( workData.xptClient ) 
					break;
				std::cout << "Failed to reconnect, retry in 30 seconds" << std::endl;
				Sleep(1000*30);
			}
		}
		// has the block data changed?
		if( workData.xptClient && xptWorkIdentifier != workData.xptClient->workDataCounter ){
			printf("New work\n");
			xptWorkIdentifier = workData.xptClient->workDataCounter;
			for(uint32 i=0; i<workData.xptClient->payloadNum; i++){
				uint8 blockData[256];
				memset(blockData, 0x00, sizeof(blockData));
				*(uint32*)(blockData+0) = workData.xptClient->blockWorkInfo.version;
				memcpy(blockData+4, workData.xptClient->blockWorkInfo.prevBlock, 32);
				memcpy(blockData+36, workData.xptClient->workData[i].merkleRoot, 32);
				*(uint32*)(blockData+68) = workData.xptClient->blockWorkInfo.nTime;
				*(uint32*)(blockData+72) = workData.xptClient->blockWorkInfo.nBits;
				*(uint32*)(blockData+76) = 0; // nonce
				memcpy(workData.workEntry[i].data, blockData, 80);
				((serverData_t*)workData.workEntry[i].serverData)->blockHeight = workData.xptClient->blockWorkInfo.height;
				((serverData_t*)workData.workEntry[i].serverData)->nBitsForShare = workData.xptClient->blockWorkInfo.nBitsShare;
				// is the data really valid?
				if( workData.xptClient->blockWorkInfo.nTime > 0 ){
					workData.workEntry[i].dataIsValid = true;
				}else{
					workData.workEntry[i].dataIsValid = false;
				}
			}
			if (workData.xptClient->blockWorkInfo.height > 0){
				OnNewBlock(workData.xptClient->blockWorkInfo.nBitsShare,workData.xptClient->blockWorkInfo.nBits,workData.xptClient->blockWorkInfo.height);


				//needs moving
				if(bOptimalSieveExtnSearchInProgress && nSieveExtnRounds>3){
					nSieveExtnRounds = 0;
					if (nSieveExtensions < nSieveExtnMax){
						nSieveExtensions += 1;
					}
				}


			}
		}
		Sleep(50);
		//		}
		loopCounter++;
	}
	return 0;
}




static void *watchdog_thread(void *){
	uint32_t totalThreads = commandlineInput.numThreads+1;
	pthread_t threads[totalThreads];
	//	using namespace boost;
	threadHearthBeat = (uint64_t *)malloc(commandlineInput.numThreads * sizeof(uint64_t));
	//	threadIDs = (thread::id *)malloc(commandlineInput.numThreads * sizeof(thread::id));
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);
	//Set the stack size of the thread
	pthread_attr_setstacksize(&threadAttr, 120*1024);
	// free resources of thread upon return
	pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);
	// start threads
	uint32 maxTimeBetweenShareSubmit = commandlineInput.nullShareTimeout * 60 * 1000;		// Nice if it was a cmd line option, so it can be ajusted!
	uint32 loopcount = 0;
	uint32 looptime = 240;  //every minute

	if(commandlineInput.printDebug){
		looptime = 40; //every 10 seconds
	}

	while(true){
		loopcount++;
		for(uint32 threadIdx=0; threadIdx<commandlineInput.numThreads; threadIdx++){

			if(thmap.count(threadIdx)){
				if(loopcount>looptime){
					if(commandlineInput.printDebug){
						std::cout << "Heartbeat Thread #" << threadIdx << " - Last share: " << (getTimeMilliseconds()-threadHearthBeat[threadIdx])/1000 << "s ago" << std::endl;
					}
			
					if( threadHearthBeat[threadIdx]+maxTimeBetweenShareSubmit < getTimeMilliseconds()){
						if(!commandlineInput.silent){
							std::cout << "Error - Watchdog - Thread #" << threadIdx << " - No accepted shares for too long - set restart flag" << std::endl;
						}
						//kill unresponsive thread	
						//pthread_kill(&threads[threadIdx],15);
						std::pair<std::map<unsigned int, bool>::iterator,bool> ret;
						ret = thmap.insert(thmapKeyVal(threadIdx, false));
						if(ret.second==false){
							ret.first->second = false; 
						}
					}
				}

			}
			if(!thmap.count(threadIdx)){
				if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
					pthread_create(&threads[threadIdx], &threadAttr, jhMiner_workerThread_getwork, (void *)threadIdx);
				else if( workData.protocolMode == MINER_PROTOCOL_GBT )
					pthread_create(&threads[threadIdx], &threadAttr, jhMiner_workerThread_gbt, (void *)threadIdx);
				else if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH )
					pthread_create(&threads[threadIdx], &threadAttr, jhMiner_workerThread_xpt, (void *)threadIdx);

				if(commandlineInput.printDebug){
					std::cout << "Watchdog - Started Thread #" << threadIdx << std::endl;
				}


				threadHearthBeat[threadIdx] = getTimeMilliseconds();
				thmap.insert(thmapKeyVal(threadIdx, true));
			}
		}
		if(loopcount>looptime){
			loopcount=0;
		}
		//pthread_attr_destroy(&threadAttr);
		Sleep(250);
	}
	std::cout << "Watchdog - Error something caused us to abort" << std::endl;
	return false;
}





int main(int argc, char **argv)
{
	// setup some default values
	commandlineInput.port = 10034;
	commandlineInput.numThreads = std::max(getNumThreads(), 1);
	commandlineInput.sieveSize = 1000000; // default maxSieveSize
	commandlineInput.sievePercentage = 10; // default 
	commandlineInput.roundSievePercentage = 70; // default 
	commandlineInput.enableCacheTunning = false;
	commandlineInput.L1CacheElements = 256000;
	commandlineInput.primorialMultiplier = 67; // for default 0 we will switch auto tune on
	commandlineInput.targetOverride = 9;
	commandlineInput.targetBTOverride = 9;
	commandlineInput.initialPrimorial = 67;
	commandlineInput.printDebug = false;
	commandlineInput.sieveExtensions = 9;
	commandlineInput.disableInput = false;
	commandlineInput.quiet = false;
	commandlineInput.silent = false;
	commandlineInput.nullShareTimeout = 1440; //"disabled" by default - 24h with no shares seems fair
	commandlineInput.sievePrimeLimit = 0;
	commandlineInput.startL1CacheTune = false;
	commandlineInput.startPrimeTune = false;
	commandlineInput.startPrimorialTune = false;
	commandlineInput.startSieveTune = false;
	commandlineInput.startSieveExtensionTune = false;
	std::cout << std::fixed << std::showpoint << std::setprecision(8);
	// parse command lines
	jhMiner_parseCommandline(argc, argv);
	// Sets max sieve size
	nMaxSieveSize = commandlineInput.sieveSize;
	nSievePercentage = commandlineInput.sievePercentage;
	nRoundSievePercentage = commandlineInput.roundSievePercentage;
	nOverrideTargetValue = commandlineInput.targetOverride;
	nOverrideBTTargetValue = commandlineInput.targetBTOverride;
	nSieveExtensions = commandlineInput.sieveExtensions;
	primeStats.nL1CacheElements = commandlineInput.L1CacheElements;

	if(commandlineInput.primorialMultiplier == 0)
	{
		primeStats.nPrimorialMultiplier = commandlineInput.initialPrimorial;
		bEnablenPrimorialMultiplierTuning = true;
	} else {
		primeStats.nPrimorialMultiplier = commandlineInput.primorialMultiplier;
	}

	if( commandlineInput.host == NULL){
		std::cout << "Missing required -o option" << std::endl;
		exit(-1);	
	}
	// if set, validate xpm address
	if( commandlineInput.xpmAddress )
	{
		DecodeBase58(commandlineInput.xpmAddress, decodedWalletAddress, &decodedWalletAddressLen);
		sha256_context ctx;
		uint8 addressValidationHash[32];
		sha256_starts(&ctx);
		sha256_update(&ctx, (uint8*)decodedWalletAddress, 20+1);
		sha256_finish(&ctx, addressValidationHash);
		sha256_starts(&ctx); // is this line needed?
		sha256_update(&ctx, addressValidationHash, 32);
		sha256_finish(&ctx, addressValidationHash);
		if( *(uint32*)addressValidationHash != *(uint32*)(decodedWalletAddress+21) )
		{
			printf("Address '%s' is not a valid wallet address.\n", decodedWalletAddress);
			exit(-2);
		}
	}

	//CRYPTO_set_mem_ex_functions(mallocEx, reallocEx, freeEx);
	if(!commandlineInput.silent && !commandlineInput.quiet){	
		std::cout << 
			" ============================================================================ " << std::endl <<
			"|  jhPrimeMiner - mod by rdebourbon -v3.3beta                     |" << std::endl <<
			"|     optimised from hg5fm (mumus) v7.1 build + HP10 updates      |" << std::endl <<
			"|     jsonrpc stats and remote config added by tandyuk            |" << std::endl <<
			"|  author: JH (http://ypool.net)                                  |" << std::endl <<
			"|  contributors: x3maniac                                         |" << std::endl <<
			"|  Credits: Sunny King for the original Primecoin client&miner    |" << std::endl <<
			"|  Credits: mikaelh for the performance optimizations             |" << std::endl <<
			"|  Credits: erkmos for the original linux port                    |" << std::endl <<
			"|  Credits: tandyuk for the linux build of rdebourbons mod        |" << std::endl <<
			"|                                                                 |" << std::endl <<
			"|  Donations (XPM):                                               |" << std::endl <<
			"|    JH00: AQjz9cAUZfjFgHXd8aTiWaKKbb3LoCVm2J                     |" << std::endl <<
			"|    rdebourbon: AUwKMCYCacE6Jq1rsLcSEHSNiohHVVSiWv               |" << std::endl <<
			"|    tandyuk: AYwmNUt6tjZJ1nPPUxNiLCgy1D591RoFn4                  |" << std::endl <<
			" ============================================================================ " << std::endl <<
			"Launching miner..." << std::endl;
	}
	// set priority lower so the user still can do other things
	// init memory speedup (if not already done in preMain)
	//mallocSpeedupInit();
	if( pctx == NULL )
		pctx = BN_CTX_new();
	// init prime table
	GeneratePrimeTable(commandlineInput.sievePrimeLimit);
	if(!commandlineInput.silent && !commandlineInput.quiet)
		printf("Sieve Percentage: %u %%\n", nSievePercentage);
	// init winsock
	pthread_mutex_init(&workData.cs, NULL);
	// connect to host
	char ipText[32];
	getIpAddr(ipText, commandlineInput.host);
	// setup RPC connection data (todo: Read from command line)
	jsonRequestTarget.ip = ipText;
	jsonRequestTarget.port = commandlineInput.port;
	jsonRequestTarget.authUser = (char *)commandlineInput.workername;
	jsonRequestTarget.authPass = (char *)commandlineInput.workerpass;

	if(!commandlineInput.silent){	
		std::cout << "Connecting to '" << commandlineInput.host << "'" << std::endl;
	}

	// init stats
	primeStats.primeLastUpdate = primeStats.blockStartTime = primeStats.startTime = getTimeMilliseconds();
	primeStats.shareFound = false;
	primeStats.shareRejected = false;
	primeStats.primeChainsFound = 0;
	primeStats.foundShareCount = 0;
	for(uint32 i = 0; i < sizeof(primeStats.chainCounter[0])/sizeof(uint32);  i++)
	{
		primeStats.chainCounter[0][i] = 0;
		primeStats.chainCounter[1][i] = 0;
		primeStats.chainCounter[2][i] = 0;
		primeStats.chainCounter[3][i] = 0;
	}
	primeStats.fShareValue = 0;
	primeStats.fBlockShareValue = 0;
	primeStats.fTotalSubmittedShareValue = 0;
	primeStats.nWaveTime = 0;
	primeStats.nWaveRound = 0;

	// setup thread count and print info
	if(!commandlineInput.silent && !commandlineInput.quiet){

		std::cout << "Using " << commandlineInput.numThreads << " threads" << std::endl;
		std::cout << "Username: " << jsonRequestTarget.authUser << std::endl;
		std::cout << "Password: " << jsonRequestTarget.authPass << std::endl;
	}
	// decide protocol
	if( commandlineInput.port == 10034 || commandlineInput.useXPT )
	{
		// port 10034 indicates xpt protocol (in future we will also add a -o URL prefix)
		workData.protocolMode = MINER_PROTOCOL_XPUSHTHROUGH;
		if(!commandlineInput.silent && !commandlineInput.quiet)
			std::cout << "Using x.pushthrough protocol" << std::endl;
	}
	else
	{
		if( useGetBlockTemplate )
		{
			workData.protocolMode = MINER_PROTOCOL_GBT;
			// getblocktemplate requires a valid xpm address to be set
			if( commandlineInput.xpmAddress == NULL )
			{
				printf("GetBlockTemplate mode requires -xpm parameter\n");
				exit(-3);
			}
		}
		else
		{
			workData.protocolMode = MINER_PROTOCOL_GETWORK;
			printf("Using GetWork() protocol\n");
			printf("Warning: \n");
			printf("   GetWork() is outdated and inefficient. You are losing mining performance\n");
			printf("   by using it. If the pool supports it, consider switching to x.pushthrough.\n");
			printf("   Just add the port :10034 to the -o parameter.\n");
			printf("   Example: jhPrimeminer.exe -o http://poolurl.net:10034 ...\n");
		}
	}
	// initial query new work / create new connection
	if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
	{
		jhMiner_queryWork_primecoin_getwork();
	}
	else if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH )
	{
		workData.xptClient = NULL;
		// x.pushthrough initial connect & login sequence
		while( true )
		{
			// repeat connect & login until it is successful (with 30 seconds delay)
			while ( true )
			{
				workData.xptClient = xptClient_connect(&jsonRequestTarget, commandlineInput.numThreads);
				if( workData.xptClient != NULL )
					break;
				if(!commandlineInput.silent){	
					std::cout << "Failed to connect, retry in 30 seconds" << std::endl;
				}
				Sleep(1000*30);
				// resolve address before reconnecting
				getIpAddr(jsonRequestTarget.ip, commandlineInput.host);
			}
			// make sure we are successfully authenticated
			while( xptClient_isDisconnected(workData.xptClient, NULL) == false && xptClient_isAuthenticated(workData.xptClient) == false )
			{
				xptClient_process(workData.xptClient);
				Sleep(1);
			}
			char* disconnectReason = NULL;
			// everything went alright?
			if( xptClient_isDisconnected(workData.xptClient, &disconnectReason) == true )
			{
				xptClient_free(workData.xptClient);
				workData.xptClient = NULL;
				break;
			}
			if( xptClient_isAuthenticated(workData.xptClient) == true )
			{
				break;
			}
			if( disconnectReason ){
				if(!commandlineInput.silent){	
					std::cout << "xpt error: " << disconnectReason << std::endl;
				}
			}
			// delete client
			xptClient_free(workData.xptClient);
			// try again in 30 seconds
			if(!commandlineInput.silent && !commandlineInput.quiet){	
				std::cout << "x.pushthrough authentication sequence failed, retry in 30 seconds" << std::endl;
			}
			Sleep(30*1000);
		}
	}


	if(!commandlineInput.silent && !commandlineInput.quiet){
		std::cout << "\nVal/h = 'Share Value per Hour', PPS = 'Primes per Second'," <<std::endl <<
			"SPS = 'Sieves per Second', ACC = 'Avg. Candidate Count / Sieve' " << std::endl <<
			"===============================================================" << std::endl <<
			"Keyboard shortcuts:" << std::endl <<
			"   <Ctrl-C>, <Q>     - Quit" << std::endl <<
			"   <Y> - Increment Primorial Multiplier" << std::endl <<
			"   <H> - Decrement Primorial Multiplier" << std::endl <<
			"   <U> - Increment Sieve size" << std::endl <<
			"   <J> - Decrement Sive size" << std::endl <<
			"   <T> - Increment Round Sieve Percentage" << std::endl <<
			"   <G> - Decrement Round Sieve Percentage" << std::endl <<
			"   <S> - Print current settings" << std::endl;
		if( commandlineInput.enableCacheTunning ){
			std::cout << "Note: While the initial auto tuning is in progress several values cannot be changed." << std::endl;
		}
	}	


	uint32_t totalThreads = commandlineInput.numThreads+1;
	pthread_t threads[totalThreads];
	// start the Auto Tuning thread
	pthread_attr_t threadAttr;
	pthread_attr_init(&threadAttr);
	//Set the stack size of the thread
	pthread_attr_setstacksize(&threadAttr, 120*1024);
	// free resources of thread upon return
	pthread_attr_setdetachstate(&threadAttr, PTHREAD_CREATE_DETACHED);

	pthread_create(&threads[commandlineInput.numThreads], &threadAttr, input_thread, NULL);
	pthread_create(&threads[commandlineInput.numThreads+1], &threadAttr, watchdog_thread, NULL);

	// enter different mainloops depending on protocol mode
	if( workData.protocolMode == MINER_PROTOCOL_GBT )
		return jhMiner_main_gbtMode();
	else if( workData.protocolMode == MINER_PROTOCOL_GETWORK )
		return jhMiner_main_getworkMode();
	else if( workData.protocolMode == MINER_PROTOCOL_XPUSHTHROUGH )
		return jhMiner_main_xptMode();

	return 0;
}



