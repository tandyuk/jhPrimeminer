#include <algorithm>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/fcntl.h>
#include <unistd.h>
#include <signal.h>
#define Sleep(ms) usleep(1000*ms)
#include <pthread.h>

typedef uint8_t BYTE;
typedef uint32_t DWORD;
#include <cstdlib>
#include <cstdio>
#include <gmpxx.h>
#include <gmp.h>
#include <iostream>


#include"jhlib/JHLib.h"

#include<stdio.h>
#include<time.h>
#include<set>
#include<stdint.h>
#include <iomanip>
#include <fstream>

#include"sha256.h"
#include"ripemd160.h"
//#include"bignum_custom.h"
static const int PROTOCOL_VERSION = 70001;

#include<openssl/bn.h>

// our own improved versions of BN functions
BIGNUM *BN2_mod_inverse(BIGNUM *in,	const BIGNUM *a, const BIGNUM *n, BN_CTX *ctx);
int BN2_div(BIGNUM *dv, BIGNUM *rm, const BIGNUM *num, const BIGNUM *divisor);
int BN2_num_bits(const BIGNUM *a);
int BN2_rshift(BIGNUM *r, const BIGNUM *a, int n);
int BN2_lshift(BIGNUM *r, const BIGNUM *a, int n);
int BN2_uadd(BIGNUM *r, const BIGNUM *a, const BIGNUM *b);

#define fastInitBignum(bignumVar, bignumData) \
	bignumVar.d = (BN_ULONG*)bignumData; \
	bignumVar.dmax = 0x200/4; \
	bignumVar.flags = BN_FLG_STATIC_DATA; \
	bignumVar.neg = 0; \
	bignumVar.top = 1; 

// original primecoin BN stuff
#include"uint256.h"
#include"bignum2.h"
#include"prime.h"
#include"jsonrpc.h"

#include"xptServer.h"
#include"xptClient.h"

static const uint64_t COIN = 100000000;
static const uint64_t CENT = 1000000;


#define	bswap_16(value)  \
 	((((value) & 0xff) << 8) | ((value) >> 8))

#define	bswap_32(value)	\
 	(((uint32_t)bswap_16((uint16_t)((value) & 0xffff)) << 16) | \
 	(uint32_t)bswap_16((uint16_t)((value) >> 16)))


static inline uint32_t swab32(uint32_t v)
{
	return bswap_32(v);
}

static inline void MyLog(const char* szString)
{
	std::ofstream myfile;
	myfile.open ("logFile.txt",std::ios_base::out | std::ios_base::app);
	myfile << szString << std::endl;
	myfile.close();	
}

static inline void swap32yes(void*out, const void*in, size_t sz) {
	size_t swapcounter = 0;
	for (swapcounter = 0; swapcounter < sz; ++swapcounter)
		(((uint32_t*)out)[swapcounter]) = swab32(((uint32_t*)in)[swapcounter]);
}

#define BEGIN(a)            ((char*)&(a))
#define END(a)              ((char*)&((&(a))[1]))
#define swap32tobe(out, in, sz)  swap32yes(out, in, sz)


static inline float GetChainDifficulty(unsigned int nChainLength)
{
	return (float)nChainLength / 16777216.0;
}


template<typename T>
std::string HexStr(const T itbegin, const T itend, bool fSpaces=false)
{
    std::string rv;
    static const char hexmap[16] = { '0', '1', '2', '3', '4', '5', '6', '7',
                                     '8', '9', 'a', 'b', 'c', 'd', 'e', 'f' };
    rv.reserve((itend-itbegin)*3);
    for(T it = itbegin; it < itend; ++it)
    {
        unsigned char val = (unsigned char)(*it);
        if(fSpaces && it != itbegin)
            rv.push_back(' ');
        rv.push_back(hexmap[val>>4]);
        rv.push_back(hexmap[val&15]);
    }

    return rv;
}

typedef struct  
{
	/* +0x00 */ uint32 seed;
	/* +0x04 */ uint32 nBitsForShare;
	/* +0x08 */ uint32 blockHeight;
	/* +0x0C */ uint32 padding1;
	/* +0x10 */ uint32 padding2;
	/* +0x14 */ uint32 client_shareBits; // difficulty score of found share (the client is allowed to modify this value, but not the others)
	/* +0x18 */ uint32 serverStuff1;
	/* +0x1C */ uint32 serverStuff2;
}serverData_t;

typedef struct  
{
	volatile uint32_t primeChainsFound;
	volatile uint32_t foundShareCount;
	volatile float fShareValue;
	volatile float fBlockShareValue;
	volatile float fTotalSubmittedShareValue;
	volatile uint32_t chainCounter[4][13];
	volatile uint32_t nWaveTime;
	volatile unsigned int nWaveRound;
	volatile uint32_t nTestTime;
	volatile unsigned int nTestRound;

	volatile float nChainHit;
	volatile float nPrevChainHit;
	volatile unsigned int nPrimorialMultiplier;
	volatile float nSieveRounds;
	volatile float nCandidateCount;
	pthread_mutex_t cs;

	// since we can generate many (useless) primes ultra fast if we simply set sieve size low, 
	// its better if we only count primes with at least a given difficulty
	//volatile uint32 qualityPrimesFound;
	volatile uint32 bestPrimeChainDifficulty;
	volatile double bestPrimeChainDifficultySinceLaunch;
	uint64 primeLastUpdate;
  	uint64 startTime;
	uint64 blockStartTime;
	bool shareFound;
	bool shareRejected;
	volatile unsigned int nL1CacheElements;
	volatile unsigned int nSieveElements;

}primeStats_t;

extern primeStats_t primeStats;
extern bool bSoloMining;


typedef struct  
{
	uint32	version;
	uint8	prevBlockHash[32];
	uint8	merkleRoot[32];
	uint32	timestamp;
	uint32	nBits;
	uint32	nonce;
	// GetHeaderHash() goes up to this offset (4+32+32+4+4+4=80 bytes)
	uint256 blockHeaderHash;
	//CBigNum bnPrimeChainMultiplierBN; unused
	mpz_class mpzPrimeChainMultiplier;
	// other
	serverData_t serverData;
	uint32 threadIndex; // the index of the miner thread
	bool xptMode;
	// getblocktemplate data
	uint32 seed;
}primecoinBlock_t;


struct blockHeader_t {
  uint32	version;            //4(0)
  uint256	prevBlockHash;      //32(4)
  uint256	merkleRoot;			//32(36)
  uint32	timestamp;          //4(68)
  uint32	nBits;              //4(72)
  uint32	nonce;              //4(76)
  uint8		primeMultiplier[48];//48(80)
};                                   


extern jsonRequestTarget_t jsonRequestTarget; // rpc login data

// prototypes from main.cpp
bool error(const char *format, ...);
bool jhMiner_pushShare_primecoin(uint8 data[256], primecoinBlock_t* primecoinBlock);
bool SubmitBlock(primecoinBlock_t* pcBlock);
void primecoinBlock_generateHeaderHash(primecoinBlock_t* primecoinBlock, uint8 hashOutput[32]);
uint32 _swapEndianessU32(uint32 v);
uint32 jhMiner_getCurrentWorkBlockHeight(sint32 threadIndex);

bool BitcoinMiner(primecoinBlock_t* primecoinBlock, CSieveOfEratosthenes*& psieve, sint32 threadIndex);

typedef struct
{
	char* workername;
	char* workerpass;
	char* host;
	uint32 port;
	uint32 numThreads;
	uint32 sieveSize;
	uint32 sievePercentage;
	uint32 roundSievePercentage;
	uint32 sievePrimeLimit;	// how many primes should be sieved
	unsigned int L1CacheElements;
	unsigned int primorialMultiplier;
	unsigned int primorialMultiplier2;
	unsigned int primorialMultiplier3;
	unsigned int primorialMultiplier4;
	bool enableCacheTunning;
	uint32 targetOverride;
	uint32 targetBTOverride;
	uint32 initialPrimorial;
	uint32 sieveExtensions;
	unsigned int nullShareTimeout;
	bool disableInput;
	bool printDebug;
	bool quiet;
	bool silent;
	bool useXPT;

	bool startSieveTune;
	bool startPrimeTune;
	bool startPrimorialTune;
	bool startL1CacheTune;
	bool startSieveExtensionTune;
	// getblocktemplate stuff
	char* xpmAddress; // we will use this XPM address for block payout
}commandlineInput_t;

extern commandlineInput_t commandlineInput;

extern bool bEnablenPrimorialMultiplierTuning;

// direct access to share counters
extern volatile int total_shares;
extern volatile int valid_shares;
extern std::set<mpz_class> multiplierSet;
extern bool appQuitSignal;

extern BN_CTX* pctx;

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#if !HAVE_DECL_LE32DEC
static inline uint32_t le32dec(const void *pp)
{
	const uint8_t *p = (uint8_t const *)pp;
	return ((uint32_t)(p[0]) + ((uint32_t)(p[1]) << 8) +
	    ((uint32_t)(p[2]) << 16) + ((uint32_t)(p[3]) << 24));
}
#endif


#include"transaction.h"
uint64 jhMiner_primeCoin_targetGetMint(unsigned int nBits);
