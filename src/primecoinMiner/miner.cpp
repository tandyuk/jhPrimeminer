#include"global.h"
#include <ctime>
#include "ticker.h"


bool MineProbablePrimeChain(CSieveOfEratosthenes*& psieve, primecoinBlock_t* block, mpz_class& bnFixedMultiplier, bool& fNewBlock, unsigned int& nTriedMultiplier, unsigned int& nProbableChainLength, 
							unsigned int& , unsigned int& nPrimesHit, sint32 threadIndex, mpz_class& mpzHash, unsigned int nPrimorialMultiplier);

std::set<mpz_class> multiplierSet;

void BitcoinMiner(primecoinBlock_t* primecoinBlock, sint32 threadIndex)
{
	//printf("PrimecoinMiner started\n");
	//SetThreadPriority(THREAD_PRIORITY_LOWEST);
	//RenameThread("primecoin-miner");
	if( pctx == NULL )
		pctx = BN_CTX_new();
	// Each thread has its own key and counter
	//CReserveKey reservekey(pwallet);
	unsigned int nExtraNonce = 0;

	static const unsigned int nPrimorialHashFactor = 7;
	const unsigned int nPrimorialMultiplierStart = 61;  
	const unsigned int nPrimorialMultiplierMax = 79;

	unsigned int nPrimorialMultiplier = primeStats.nPrimorialMultiplier;
	uint64_t nTimeExpected = 0;   // time expected to prime chain (micro-second)   unused?
	uint64_t nTimeExpectedPrev = 0; // time expected to prime chain last time   unused?
	bool fIncrementPrimorial = true; // increase or decrease primorial factor   unused?
	uint64_t nSieveGenTime = 0;
	

	CSieveOfEratosthenes* psieve = NULL;

	//primecoinBlock->nonce = 0;
	//TODO: check this if it makes sense
	primecoinBlock->nonce = 0x01000000 * threadIndex;
   const unsigned long maxNonce = (0x01000000 * threadIndex) | 0x00FF0000;
	//primecoinBlock->nonce = 0;

	uint64 nTime = getTimeMilliseconds() + 1000*600;
	uint64 nStatTime = getTimeMilliseconds() + 2000;
	
	// note: originally a wanted to loop as long as (primecoinBlock->workDataHash != jhMiner_getCurrentWorkHash()) did not happen
	//		 but I noticed it might be smarter to just check if the blockHeight has changed, since that is what is really important
	uint32 loopCount = 0;

	//mpz_class mpzHashFactor;
	//Primorial(nPrimorialHashFactor, mpzHashFactor);
	//unsigned int nHashFactor = PrimorialFast(nPrimorialHashFactor);
	unsigned int nHashFactor = PrimorialFast(7);

	time_t unixTimeStart;
	time(&unixTimeStart);
	uint32 nTimeRollStart = primecoinBlock->timestamp - 5;
   uint32 nLastRollTime = getTimeMilliseconds();
	uint32 nCurrentTick = nLastRollTime;
	while( nCurrentTick < nTime && primecoinBlock->serverData.blockHeight == jhMiner_getCurrentWorkBlockHeight(primecoinBlock->threadIndex) )
			{
		nCurrentTick = getTimeMilliseconds();
		//if( primecoinBlock->xptMode )
		//{
		//	// when using x.pushthrough, roll time
		//	time_t unixTimeCurrent;
		//	time(&unixTimeCurrent);
		//	uint32 timeDif = unixTimeCurrent - unixTimeStart;
		//	uint32 newTimestamp = nTimeRollStart + timeDif;
		//	if( newTimestamp != primecoinBlock->timestamp )
		//	{
		//		primecoinBlock->timestamp = newTimestamp;
		//		primecoinBlock->nonce = 0;
		//		//nPrimorialMultiplierStart = startFactorList[(threadIndex&3)];
		//		nPrimorialMultiplier = nPrimorialMultiplierStart;
		//	}
		//}

		primecoinBlock_generateHeaderHash(primecoinBlock, primecoinBlock->blockHeaderHash.begin());
		//
		// Search
		//
		bool fNewBlock = true;
		unsigned int nTriedMultiplier = 0;
		// Primecoin: try to find hash divisible by primorial
        uint256 phash = primecoinBlock->blockHeaderHash;
        mpz_class mpzHash;
        mpz_set_uint256(mpzHash.get_mpz_t(), phash);
        
		while ((phash < hashBlockHeaderLimit || !mpz_divisible_ui_p(mpzHash.get_mpz_t(), nHashFactor)) && primecoinBlock->nonce < 0xffff0000) {
			primecoinBlock->nonce++;
			primecoinBlock_generateHeaderHash(primecoinBlock, primecoinBlock->blockHeaderHash.begin());
            phash = primecoinBlock->blockHeaderHash;
            mpz_set_uint256(mpzHash.get_mpz_t(), phash);
		}
		//printf("Use nonce %d\n", primecoinBlock->nonce);
		if (primecoinBlock->nonce >= 0xffff0000)
		{
			printf("Nonce overflow\n");
			break;
		}
		// Primecoin: primorial fixed multiplier
		mpz_class mpzPrimorial;
		unsigned int nRoundTests = 0;
		unsigned int nRoundPrimesHit = 0;
		uint64 nPrimeTimerStart = getTimeMilliseconds();
		
		//if( loopCount > 0 )
		//{
		//	//primecoinBlock->nonce++;
		//	if (!PrimeTableGetNextPrime(nPrimorialMultiplier))
		//		error("PrimecoinMiner() : primorial increment overflow");
		//}

		Primorial(nPrimorialMultiplier, mpzPrimorial);

		unsigned int nTests = 0;
		unsigned int nPrimesHit = 0;
		
		mpz_class mpzMultiplierMin = mpzPrimeMin * nHashFactor / mpzHash + 1;
		while (mpzPrimorial < mpzMultiplierMin )
		{
			if (!PrimeTableGetNextPrime(nPrimorialMultiplier))
				error("PrimecoinMiner() : primorial minimum overflow");
			Primorial(nPrimorialMultiplier, mpzPrimorial);
		}
        mpz_class mpzFixedMultiplier;
        if (mpzPrimorial > nHashFactor) {
            mpzFixedMultiplier = mpzPrimorial / nHashFactor;
        } else {
            mpzFixedMultiplier = 1;
        }		
		//printf("fixedMultiplier: %d nPrimorialMultiplier: %d\n", BN_get_word(&bnFixedMultiplier), nPrimorialMultiplier);
		// Primecoin: mine for prime chain
		unsigned int nProbableChainLength;
		MineProbablePrimeChain(psieve, primecoinBlock, mpzFixedMultiplier, fNewBlock, nTriedMultiplier, nProbableChainLength, nTests, nPrimesHit, threadIndex, mpzHash, nPrimorialMultiplier);
#ifdef _WIN32
		threadHearthBeat[threadIndex] = GetTickCount();
		if (appQuitSignal)
		{
			printf( "Shutting down mining thread %d.\n", threadIndex);
			return false;
		}
#endif
		//{
		//	// do nothing here, share is already submitted in MineProbablePrimeChain()
		//	//primecoinBlock->nonce += 0x00010000;
		//	primecoinBlock->nonce++;
		//	nPrimorialMultiplier = primeStats.nPrimorialMultiplier;
		//	//break;
		//}
		//psieve = NULL;
		nRoundTests += nTests;
		nRoundPrimesHit += nPrimesHit;
		nPrimorialMultiplier = primeStats.nPrimorialMultiplier;
		// added this
		if (fNewBlock)
		{
		}


		primecoinBlock->nonce ++;
		primecoinBlock->timestamp = std::max(primecoinBlock->timestamp, (unsigned int) time(NULL));
		loopCount++;
	}
	if( psieve )
	{
		delete psieve;
		psieve = NULL;
	}


	
}
