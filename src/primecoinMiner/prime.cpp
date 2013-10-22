// Copyright (c) 2013 Primecoin developers
// Distributed under conditional MIT/X11 software license,
// see the accompanying file COPYING

#include "global.h"
#include <bitset>
#include <time.h>
#include <set>
#include "ticker.h"

#include <algorithm>

// Prime Table
//std::vector<unsigned int> vPrimes;
//uint32* vPrimes;
uint32* vPrimesTwoInverse;
uint32 vPrimesSize = 0;

  BN_CTX* pctx = NULL;

/* not used and gives errors because of LARGE_INTEGER, so disable for now
// changed to return the ticks since reboot
// doesnt need to be the correct time, just a more or less random input value
uint64 GetTimeMicros()
{
   LARGE_INTEGER t;
   QueryPerformanceCounter(&t);
   return (uint64)t.QuadPart;
}*/


std::vector<unsigned int> vPrimes;
unsigned int nSieveExtensions = nDefaultSieveExtensions;

static unsigned int int_invert(unsigned int a, unsigned int nPrime);



void GeneratePrimeTable(unsigned int nSieveSize)
{
   const unsigned int nPrimeTableLimit = nSieveSize ;
   vPrimes.clear();
   // Generate prime table using sieve of Eratosthenes
   std::vector<bool> vfComposite (nPrimeTableLimit, false);
   for (unsigned int nFactor = 2; nFactor * nFactor < nPrimeTableLimit; nFactor++)
   {
      if (vfComposite[nFactor])
         continue;
      for (unsigned int nComposite = nFactor * nFactor; nComposite < nPrimeTableLimit; nComposite += nFactor)
         vfComposite[nComposite] = true;
   }
   for (unsigned int n = 2; n < nPrimeTableLimit; n++)
      if (!vfComposite[n])
         vPrimes.push_back(n);
	if(!commandlineInput.silent && !commandlineInput.quiet)
	   std::cout << "GeneratePrimeTable() : prime table [1, " << nPrimeTableLimit << "] generated with " << vPrimes.size() << " primes" << std::endl;
   vPrimesSize = vPrimes.size();  
}

// Get next prime number of p
bool PrimeTableGetNextPrime(unsigned int& p)
{
   for(uint32 i=0; i<vPrimesSize; i++)
   {
      unsigned int nPrime = vPrimes[i];

      if ( nPrime > p)
      {
         p = nPrime;
         return true;
      }
   }
   return false;
}

// Get previous prime number of p
bool PrimeTableGetPreviousPrime(unsigned int& p)
{
   uint32 nPrevPrime = 0;
   for(uint32 i=0; i<vPrimesSize; i++)
   {
      if (vPrimes[i] >= p)
         break;
      nPrevPrime = vPrimes[i];
   }
   if (nPrevPrime)
   {
      p = nPrevPrime;
      return true;
   }
   return false;
}

// Compute Primorial number p#
void Primorial(unsigned int p, CBigNum& bnPrimorial)
{
   bnPrimorial = 1;
   for(uint32 i=0; i<vPrimesSize; i++)
   {
      unsigned int nPrime = vPrimes[i];
      if (nPrime > p) break;
      bnPrimorial *= nPrime;
   }
}


// Compute Primorial number p#
void Primorial(unsigned int p, mpz_class& mpzPrimorial)
{
   mpzPrimorial = 1;
   //BOOST_FOREACH(unsigned int nPrime, vPrimes)
   //for(uint32 i=0; i<vPrimes.size(); i++)
   for(uint32 i=0; i<vPrimesSize; i++)
   {
      unsigned int nPrime = vPrimes[i];
      if (nPrime > p) break;
      mpzPrimorial *= nPrime;
   }
}

// Compute Primorial number p#
// Fast 32-bit version assuming that p <= 23
unsigned int PrimorialFast(unsigned int p)
{
   unsigned int nPrimorial = 1;
   for(uint32 i=0; i<vPrimesSize; i++)
   {
      unsigned int nPrime = vPrimes[i];
      if (nPrime > p) break;
      nPrimorial *= nPrime;
   }
   return nPrimorial;
}

// Compute first primorial number greater than or equal to pn
void PrimorialAt(mpz_class& bn, mpz_class& mpzPrimorial)
{
   mpzPrimorial = 1;
   //BOOST_FOREACH(unsigned int nPrime, vPrimes)
   //for(uint32 i=0; i<vPrimes.size(); i++)
   for(uint32 i=0; i<vPrimesSize; i++)
   {
      unsigned int nPrime = vPrimes[i];
      mpzPrimorial *= nPrime;
      if (mpzPrimorial >= bn)
         return;
   }
}


// Check Fermat probable primality test (2-PRP): 2 ** (n-1) = 1 (mod n)
// true: n is probable prime
// false: n is composite; set fractional length in the nLength output
static bool FermatProbablePrimalityTest(const CBigNum& n, unsigned int& nLength)
{
   //CBigNum a = 2; // base; Fermat witness
   CBigNum e = n - 1;
   CBigNum r;
   BN_mod_exp(&r, &bnTwo, &e, &n, pctx);
   if (r == 1)
      return true;
   // Failed Fermat test, calculate fractional length
   unsigned int nFractionalLength = (((n-r) << nFractionalBits) / n).getuint();
   if (nFractionalLength >= (1 << nFractionalBits))
      return error("FermatProbablePrimalityTest() : fractional assert");
   nLength = (nLength & TARGET_LENGTH_MASK) | nFractionalLength;
   return false;
}


// Check Fermat probable primality test (2-PRP): 2 ** (n-1) = 1 (mod n)
// true: n is probable prime
// false: n is composite; set fractional length in the nLength output
static bool FermatProbablePrimalityTest(const mpz_class& n, unsigned int& nLength)
{
   // Faster GMP version

   mpz_t mpzN;
   mpz_t mpzE;
   mpz_t mpzR;

   mpz_init_set(mpzN, n.get_mpz_t());
   mpz_init(mpzE);
   mpz_sub_ui(mpzE, mpzN, 1);
   mpz_init(mpzR);
   mpz_powm(mpzR, mpzTwo.get_mpz_t(), mpzE, mpzN);
   if (mpz_cmp_ui(mpzR, 1) == 0) {
      mpz_clear(mpzN);
      mpz_clear(mpzE);
      mpz_clear(mpzR);
      return true;
   }
   // Failed Fermat test, calculate fractional length
   mpz_sub(mpzE, mpzN, mpzR);
   mpz_mul_2exp(mpzR, mpzE, nFractionalBits);
   mpz_tdiv_q(mpzE, mpzR, mpzN);
   unsigned int nFractionalLength = mpz_get_ui(mpzE);
   mpz_clear(mpzN);
   mpz_clear(mpzE);
   mpz_clear(mpzR);
   if (nFractionalLength >= (1 << nFractionalBits))
      return error("FermatProbablePrimalityTest() : fractional assert");
   nLength = (nLength & TARGET_LENGTH_MASK) | nFractionalLength;
   return false;
}

// Test probable primality of n = 2p +/- 1 based on Euler, Lagrange and Lifchitz
// fSophieGermain:
//   true:  n = 2p+1, p prime, aka Cunningham Chain of first kind
//   false: n = 2p-1, p prime, aka Cunningham Chain of second kind
// Return values
//   true: n is probable prime
//   false: n is composite; set fractional length in the nLength output

static bool EulerLagrangeLifchitzPrimalityTest(const CBigNum& n, bool fSophieGermain, unsigned int& nLength)
{
   //CBigNum a = 2;
   CBigNum e = (n - 1) >> 1;
   CBigNum r;
   BN_mod_exp(&r, &bnTwo, &e, &n, pctx);
   uint32 nMod8U32 = 0;
   if( n.top > 0 )
      nMod8U32 = n.d[0]&7;

   // validate the optimization above:
   //CBigNum nMod8 = n % bnConst8;
   //if( CBigNum(nMod8U32) != nMod8 )
   //	__debugbreak();

   bool fPassedTest = false;
   if (fSophieGermain && (nMod8U32 == 7)) // Euler & Lagrange
      fPassedTest = (r == 1);
   else if (fSophieGermain && (nMod8U32 == 3)) // Lifchitz
      fPassedTest = ((r+1) == n);
   else if ((!fSophieGermain) && (nMod8U32 == 5)) // Lifchitz
      fPassedTest = ((r+1) == n);
   else if ((!fSophieGermain) && (nMod8U32 == 1)) // LifChitz
      fPassedTest = (r == 1);
   else
      return error("EulerLagrangeLifchitzPrimalityTest() : invalid n %% 8 = %d, %s", nMod8U32, (fSophieGermain? "first kind" : "second kind"));

   if (fPassedTest)
      return true;
   // Failed test, calculate fractional length
   r = (r * r) % n; // derive Fermat test remainder
   unsigned int nFractionalLength = (((n-r) << nFractionalBits) / n).getuint();
   if (nFractionalLength >= (1 << nFractionalBits))
      return error("EulerLagrangeLifchitzPrimalityTest() : fractional assert");
   nLength = (nLength & TARGET_LENGTH_MASK) | nFractionalLength;
   return false;
}


class CPrimalityTestParams
{
public:
   // GMP variables
   mpz_t mpzE;
   mpz_t mpzR;
   mpz_t mpzRplusOne;

   // GMP C++ variables
   mpz_class mpzOriginMinusOne;
   mpz_class mpzOriginPlusOne;
   mpz_class N;


   // Values specific to a round
   unsigned int nBits;
   unsigned int nPrimorialSeq;
   unsigned int nCandidateType;
   unsigned int nTargetLength;
   unsigned int nHalfTargetLength;

   // Results
   unsigned int nChainLength;

   CPrimalityTestParams(unsigned int nBits, unsigned int nPrimorialSeq)
   {
      this->nBits = nBits;
      this->nTargetLength = TargetGetLength(nBits);
      this->nHalfTargetLength = nTargetLength / 2;
      this->nPrimorialSeq = nPrimorialSeq;
      nChainLength = 0;
      mpz_init(mpzE);
      mpz_init(mpzR);
      mpz_init(mpzRplusOne);
   }

   ~CPrimalityTestParams()
   {
      mpz_clear(mpzE);
      mpz_clear(mpzR);
      mpz_clear(mpzRplusOne);
   }
};


// Check Fermat probable primality test (2-PRP): 2 ** (n-1) = 1 (mod n)
// true: n is probable prime
// false: n is composite; set fractional length in the nLength output
static bool FermatProbablePrimalityTestFast(const mpz_class& n, unsigned int& nLength, CPrimalityTestParams& testParams, bool fFastFail = false)
{
   // Faster GMP version
   mpz_t& mpzE = testParams.mpzE;
   mpz_t& mpzR = testParams.mpzR;

   mpz_sub_ui(mpzE, n.get_mpz_t(), 1);
   mpz_powm(mpzR, mpzTwo.get_mpz_t(), mpzE, n.get_mpz_t());
   if (mpz_cmp_ui(mpzR, 1) == 0)
      return true;
   if (fFastFail)
      return false;
   // Failed Fermat test, calculate fractional length
   mpz_sub(mpzE, n.get_mpz_t(), mpzR);
   mpz_mul_2exp(mpzR, mpzE, nFractionalBits);
   mpz_tdiv_q(mpzE, mpzR, n.get_mpz_t());
   unsigned int nFractionalLength = mpz_get_ui(mpzE);
   if (nFractionalLength >= (1 << nFractionalBits))
      return error("FermatProbablePrimalityTest() : fractional assert");
   nLength = (nLength & TARGET_LENGTH_MASK) | nFractionalLength;
   return false;
}



// Test probable primality of n = 2p +/- 1 based on Euler, Lagrange and Lifchitz
// fSophieGermain:
//   true:  n = 2p+1, p prime, aka Cunningham Chain of first kind
//   false: n = 2p-1, p prime, aka Cunningham Chain of second kind
// Return values
//   true: n is probable prime
//   false: n is composite; set fractional length in the nLength output
static bool EulerLagrangeLifchitzPrimalityTestFast(const mpz_class& n, bool fSophieGermain, unsigned int& nLength, CPrimalityTestParams& testParams/*, bool fFastDiv = false*/)
{
   // Faster GMP version
   mpz_t& mpzE = testParams.mpzE;
   mpz_t& mpzR = testParams.mpzR;
   mpz_t& mpzRplusOne = testParams.mpzRplusOne;
   /*
   if (fFastDiv)
   {
   // Fast divisibility tests
   // Divide n by a large divisor
   // Use the remainder to test divisibility by small primes
   const unsigned int nDivSize = testParams.nFastDivisorsSize;
   for (unsigned int i = 0; i < nDivSize; i++)
   {
   unsigned long lRemainder = mpz_tdiv_ui(n.get_mpz_t(), testParams.vFastDivisors[i]);
   unsigned int nPrimeSeq = testParams.vFastDivSeq[i];
   const unsigned int nPrimeSeqEnd = testParams.vFastDivSeq[i + 1];
   for (; nPrimeSeq < nPrimeSeqEnd; nPrimeSeq++)
   {
   if (lRemainder % vPrimes[nPrimeSeq] == 0)
   return false;
   }
   }
   }
   */
   mpz_sub_ui(mpzE, n.get_mpz_t(), 1);
   mpz_tdiv_q_2exp(mpzE, mpzE, 1);
   mpz_powm(mpzR, mpzTwo.get_mpz_t(), mpzE, n.get_mpz_t());
   unsigned int nMod8 = mpz_get_ui(n.get_mpz_t()) % 8;
   bool fPassedTest = false;
   if (fSophieGermain && (nMod8 == 7)) // Euler & Lagrange
      fPassedTest = !mpz_cmp_ui(mpzR, 1);
   else if (fSophieGermain && (nMod8 == 3)) // Lifchitz
   {
      mpz_add_ui(mpzRplusOne, mpzR, 1);
      fPassedTest = !mpz_cmp(mpzRplusOne, n.get_mpz_t());
   }
   else if ((!fSophieGermain) && (nMod8 == 5)) // Lifchitz
   {
      mpz_add_ui(mpzRplusOne, mpzR, 1);
      fPassedTest = !mpz_cmp(mpzRplusOne, n.get_mpz_t());
   }
   else if ((!fSophieGermain) && (nMod8 == 1)) // LifChitz
      fPassedTest = !mpz_cmp_ui(mpzR, 1);
   else
      return error("EulerLagrangeLifchitzPrimalityTest() : invalid n %% 8 = %d, %s", nMod8, (fSophieGermain? "first kind" : "second kind"));

   if (fPassedTest)
   {
      return true;
   }

   // Failed test, calculate fractional length
   mpz_mul(mpzE, mpzR, mpzR);
   mpz_tdiv_r(mpzR, mpzE, n.get_mpz_t()); // derive Fermat test remainder

   mpz_sub(mpzE, n.get_mpz_t(), mpzR);
   mpz_mul_2exp(mpzR, mpzE, nFractionalBits);
   mpz_tdiv_q(mpzE, mpzR, n.get_mpz_t());
   unsigned int nFractionalLength = mpz_get_ui(mpzE);

   if (nFractionalLength >= (1 << nFractionalBits))
      return error("EulerLagrangeLifchitzPrimalityTest() : fractional assert");
   nLength = (nLength & TARGET_LENGTH_MASK) | nFractionalLength;
   return false;
}


// Test probable primality of n = 2p +/- 1 based on Euler, Lagrange and Lifchitz
// fSophieGermain:
//   true:  n = 2p+1, p prime, aka Cunningham Chain of first kind
//   false: n = 2p-1, p prime, aka Cunningham Chain of second kind
// Return values
//   true: n is probable prime
//   false: n is composite; set fractional length in the nLength output
static bool EulerLagrangeLifchitzPrimalityTest(const mpz_class& n, bool fSophieGermain, unsigned int& nLength)
{
   // Faster GMP version
   mpz_t mpzN;
   mpz_t mpzE;
   mpz_t mpzR;

   mpz_init_set(mpzN, n.get_mpz_t());
   mpz_init(mpzE);
   mpz_sub_ui(mpzE, mpzN, 1);
   mpz_tdiv_q_2exp(mpzE, mpzE, 1);
   mpz_init(mpzR);
   mpz_powm(mpzR, mpzTwo.get_mpz_t(), mpzE, mpzN);
   unsigned int nMod8 = mpz_tdiv_ui(mpzN, 8);
   bool fPassedTest = false;
   if (fSophieGermain && (nMod8 == 7)) // Euler & Lagrange
      fPassedTest = !mpz_cmp_ui(mpzR, 1);
   else if (fSophieGermain && (nMod8 == 3)) // Lifchitz
   {
      mpz_t mpzRplusOne;
      mpz_init(mpzRplusOne);
      mpz_add_ui(mpzRplusOne, mpzR, 1);
      fPassedTest = !mpz_cmp(mpzRplusOne, mpzN);
      mpz_clear(mpzRplusOne);
   }
   else if ((!fSophieGermain) && (nMod8 == 5)) // Lifchitz
   {
      mpz_t mpzRplusOne;
      mpz_init(mpzRplusOne);
      mpz_add_ui(mpzRplusOne, mpzR, 1);
      fPassedTest = !mpz_cmp(mpzRplusOne, mpzN);
      mpz_clear(mpzRplusOne);
   }
   else if ((!fSophieGermain) && (nMod8 == 1)) // LifChitz
   {
      fPassedTest = !mpz_cmp_ui(mpzR, 1);
   }
   else
   {
      mpz_clear(mpzN);
      mpz_clear(mpzE);
      mpz_clear(mpzR);
      return error("EulerLagrangeLifchitzPrimalityTest() : invalid n %% 8 = %d, %s", nMod8, (fSophieGermain? "first kind" : "second kind"));
   }

   if (fPassedTest) {
      mpz_clear(mpzN);
      mpz_clear(mpzE);
      mpz_clear(mpzR);
      return true;
   }

   // Failed test, calculate fractional length
   mpz_mul(mpzE, mpzR, mpzR);
   mpz_tdiv_r(mpzR, mpzE, mpzN); // derive Fermat test remainder

   mpz_sub(mpzE, mpzN, mpzR);
   mpz_mul_2exp(mpzR, mpzE, nFractionalBits);
   mpz_tdiv_q(mpzE, mpzR, mpzN);
   unsigned int nFractionalLength = mpz_get_ui(mpzE);
   mpz_clear(mpzN);
   mpz_clear(mpzE);
   mpz_clear(mpzR);

   if (nFractionalLength >= (1 << nFractionalBits)) {
      return error("EulerLagrangeLifchitzPrimalityTest() : fractional assert");
   }
   nLength = (nLength & TARGET_LENGTH_MASK) | nFractionalLength;
   return false;
}

// Proof-of-work Target (prime chain target):
//   format - 32 bit, 8 length bits, 24 fractional length bits

unsigned int nTargetInitialLength = 7; // initial chain length target
unsigned int nTargetMinLength = 6;     // minimum chain length target

unsigned int TargetGetLimit()
{
   return (nTargetMinLength << nFractionalBits);
}

unsigned int TargetGetInitial()
{
   return (nTargetInitialLength << nFractionalBits);
}

unsigned int TargetGetLength(unsigned int nBits)
{
   return ((nBits & TARGET_LENGTH_MASK) >> nFractionalBits);
}

bool TargetSetLength(unsigned int nLength, unsigned int& nBits)
{
   if (nLength >= 0xff)
      return error("TargetSetLength() : invalid length=%u", nLength);
   nBits &= TARGET_FRACTIONAL_MASK;
   nBits |= (nLength << nFractionalBits);
   return true;
}

void TargetIncrementLength(unsigned int& nBits)
{
   nBits += (1 << nFractionalBits);
}

void TargetDecrementLength(unsigned int& nBits)
{
   if (TargetGetLength(nBits) > nTargetMinLength)
      nBits -= (1 << nFractionalBits);
}

unsigned int TargetGetFractional(unsigned int nBits)
{
   return (nBits & TARGET_FRACTIONAL_MASK);
}

uint64 TargetGetFractionalDifficulty(unsigned int nBits)
{
   return (nFractionalDifficultyMax / (uint64) ((1ull<<nFractionalBits) - TargetGetFractional(nBits)));
}

bool TargetSetFractionalDifficulty(uint64 nFractionalDifficulty, unsigned int& nBits)
{
   if (nFractionalDifficulty < nFractionalDifficultyMin)
      return error("TargetSetFractionalDifficulty() : difficulty below min");
   uint64 nFractional = nFractionalDifficultyMax / nFractionalDifficulty;
   if (nFractional > (1u<<nFractionalBits))
      return error("TargetSetFractionalDifficulty() : fractional overflow: nFractionalDifficulty=%016I64d", nFractionalDifficulty);
   nFractional = (1u<<nFractionalBits) - nFractional;
   nBits &= TARGET_LENGTH_MASK;
   nBits |= (unsigned int)nFractional;
   return true;
}

#ifdef _WIN32
std::string TargetToString(unsigned int nBits)
{
   __debugbreak(); // return strprintf("%02x.%06x", TargetGetLength(nBits), TargetGetFractional(nBits));
   return NULL; // todo
}
#endif

unsigned int TargetFromInt(unsigned int nLength)
{
   return (nLength << nFractionalBits);
}

// Get mint value from target
// Primecoin mint rate is determined by target
//   mint = 999 / (target length ** 2)
// Inflation is controlled via Moore's Law
bool TargetGetMint(unsigned int nBits, uint64& nMint)
{
   nMint = 0;
   static uint64 nMintLimit = 999ull * COIN;
   CBigNum bnMint = nMintLimit;
   if (TargetGetLength(nBits) < nTargetMinLength)
      return error("TargetGetMint() : length below minimum required, nBits=%08x", nBits);
   bnMint = (bnMint << nFractionalBits) / nBits;
   bnMint = (bnMint << nFractionalBits) / nBits;
   bnMint = (bnMint / CENT) * CENT;  // mint value rounded to cent
   nMint = bnMint.getuint256().Get64();
   if (nMint > nMintLimit)
   {
      nMint = 0;
      return error("TargetGetMint() : mint value over limit, nBits=%08x", nBits);
   }
   return true;
}

// Get next target value
bool TargetGetNext(unsigned int nBits, uint64_t nInterval, uint64_t nTargetSpacing, uint64 nActualSpacing, unsigned int& nBitsNext)
{
   nBitsNext = nBits;
   // Convert length into fractional difficulty
   uint64 nFractionalDifficulty = TargetGetFractionalDifficulty(nBits);
   // Compute new difficulty via exponential moving toward target spacing
   CBigNum bnFractionalDifficulty = nFractionalDifficulty;
   bnFractionalDifficulty *= ((nInterval + 1) * nTargetSpacing);
   bnFractionalDifficulty /= ((nInterval - 1) * nTargetSpacing + nActualSpacing + nActualSpacing);
   if (bnFractionalDifficulty > nFractionalDifficultyMax)
      bnFractionalDifficulty = nFractionalDifficultyMax;
   if (bnFractionalDifficulty < nFractionalDifficultyMin)
      bnFractionalDifficulty = nFractionalDifficultyMin;
   uint64 nFractionalDifficultyNew = bnFractionalDifficulty.getuint256().Get64();
   //if (fDebug && GetBoolArg("-printtarget"))
   //	printf("TargetGetNext() : nActualSpacing=%d nFractionDiff=%016"PRI64x" nFractionDiffNew=%016"PRI64x"\n", (int)nActualSpacing, nFractionalDifficulty, nFractionalDifficultyNew);
   // Step up length if fractional past threshold
   if (nFractionalDifficultyNew > nFractionalDifficultyThreshold)
   {
      nFractionalDifficultyNew = nFractionalDifficultyMin;
      TargetIncrementLength(nBitsNext);
   }
   // Step down length if fractional at minimum
   else if (nFractionalDifficultyNew == nFractionalDifficultyMin && TargetGetLength(nBitsNext) > nTargetMinLength)
   {
      nFractionalDifficultyNew = nFractionalDifficultyThreshold;
      TargetDecrementLength(nBitsNext);
   }
   // Convert fractional difficulty back to length
   if (!TargetSetFractionalDifficulty(nFractionalDifficultyNew, nBitsNext))
      return error("TargetGetNext() : unable to set fractional difficulty prev=0x%016I64d new=0x%016I64d", nFractionalDifficulty, nFractionalDifficultyNew);
   return true;
}



// Test Probable Cunningham Chain for: n
// fSophieGermain:
//   true - Test for Cunningham Chain of first kind (n, 2n+1, 4n+3, ...)
//   false - Test for Cunningham Chain of second kind (n, 2n-1, 4n-3, ...)
// Return value:
//   true - Probable Cunningham Chain found (length at least 2)
//   false - Not Cunningham Chain
static bool ProbableCunninghamChainTestFast(const mpz_class& n, const bool fSophieGermain, const bool fFermatTest, unsigned int& nProbableChainLength, CPrimalityTestParams& testParams, bool fBiTwinTest)
{
   nProbableChainLength = 0;

   // Fermat test for n first
     if (!FermatProbablePrimalityTestFast(n, nProbableChainLength, testParams, true))
     return false;

   const int chainIncremental = (fSophieGermain? 1 : (-1));

   mpz_class &N = testParams.N;
   N = n;
   unsigned int quickTargetCheck = 0;
   if (bSoloMining)
   {
      // Get prime origin.
      mpz_class M = n;

   // Get target depth to check.
   unsigned int quickTargetCheck = (!fBiTwinTest) ? testParams.nTargetLength : testParams.nHalfTargetLength;
   M = (M + chainIncremental) * (1 << (quickTargetCheck - 2)) - chainIncremental;
   // If this target fails we don't have a valid candidate, go no further.
   if (!FermatProbablePrimalityTestFast(M, nProbableChainLength, testParams, true))
   {
      return false;
   }
   else
   {
      M <<= 1;
      M += chainIncremental;
      if (!FermatProbablePrimalityTestFast(M, nProbableChainLength, testParams, true))
      {
         return false;
      }
   }
   }

   // Euler-Lagrange-Lifchitz test for the following numbers in chain
   unsigned int currentLength = 0;
   while (true)
   {
      TargetIncrementLength(nProbableChainLength);
      N <<= 1;
      N += chainIncremental;
      currentLength++;

      if (bSoloMining)
      {
         if (currentLength == quickTargetCheck - 2) continue; // We already proved this length is valid.
         if (currentLength == quickTargetCheck - 1) continue; // We already proved this length is valid.
      }
      if (fFermatTest)
      {
         if (!FermatProbablePrimalityTestFast(N, nProbableChainLength, testParams))
            break;
      }
      else
      {
         if (!EulerLagrangeLifchitzPrimalityTestFast(N, fSophieGermain, nProbableChainLength, testParams))
            break;
      }
   }

   return (currentLength >= 2);
}

// Test Probable Cunningham Chain for: n
// fSophieGermain:
//   true - Test for Cunningham Chain of first kind (n, 2n+1, 4n+3, ...)
//   false - Test for Cunningham Chain of second kind (n, 2n-1, 4n-3, ...)
// Return value:
//   true - Probable Cunningham Chain found (length at least 2)
//   false - Not Cunningham Chain
static bool ProbableCunninghamChainTest(const mpz_class& n, bool fSophieGermain, bool fFermatTest, unsigned int& nProbableChainLength)
{
   nProbableChainLength = 0;
   mpz_class N = n;

   // Fermat test for n first
   if (!FermatProbablePrimalityTest(N, nProbableChainLength))
      return false;

   // Euler-Lagrange-Lifchitz test for the following numbers in chain
   while (true)
   {
      TargetIncrementLength(nProbableChainLength);
      N = N + N + (fSophieGermain? 1 : (-1));
      if (fFermatTest)
      {
         if (!FermatProbablePrimalityTest(N, nProbableChainLength))
            break;
      }
      else
      {
         if (!EulerLagrangeLifchitzPrimalityTest(N, fSophieGermain, nProbableChainLength))
            break;
      }
   }

   return (TargetGetLength(nProbableChainLength) >= 2);
}

static bool ProbableCunninghamChainTestBN(const CBigNum& n, bool fSophieGermain, bool fFermatTest, unsigned int& nProbableChainLength)
{
   nProbableChainLength = 0;
   CBigNum N = n;

   // Fermat test for n first
   if (!FermatProbablePrimalityTest(N, nProbableChainLength))
      return false;

   // Euler-Lagrange-Lifchitz test for the following numbers in chain
   while (true)
   {
      TargetIncrementLength(nProbableChainLength);
      N = N + N + (fSophieGermain? 1 : (-1));
      if (fFermatTest)
      {
         if (!FermatProbablePrimalityTest(N, nProbableChainLength))
            break;
      }
      else
      {
         if (!EulerLagrangeLifchitzPrimalityTest(N, fSophieGermain, nProbableChainLength))
            break;
      }
   }

   return (TargetGetLength(nProbableChainLength) >= 2);
}

// Test probable prime chain for: nOrigin
// Return value:
//   true - Probable prime chain found (one of nChainLength meeting target)
//   false - prime chain too short (none of nChainLength meeting target)
bool ProbablePrimeChainTest(const mpz_class& bnPrimeChainOrigin, unsigned int nBits, bool fFermatTest, unsigned int& nChainLengthCunningham1, unsigned int& nChainLengthCunningham2, unsigned int& nChainLengthBiTwin, bool fullTest)
{
   nChainLengthCunningham1 = 0;
   nChainLengthCunningham2 = 0;
   nChainLengthBiTwin = 0;

   // Test for Cunningham Chain of second kind
   ProbableCunninghamChainTest(bnPrimeChainOrigin+1, false, fFermatTest, nChainLengthCunningham2);
   if (nChainLengthCunningham2 >= nBits && !fullTest)
      return true;
   // Test for Cunningham Chain of first kind
   ProbableCunninghamChainTest(bnPrimeChainOrigin-1, true, fFermatTest, nChainLengthCunningham1);
   if (nChainLengthCunningham1 >= nBits && !fullTest)
      return true;
   // Figure out BiTwin Chain length
   // BiTwin Chain allows a single prime at the end for odd length chain
   nChainLengthBiTwin =
      (TargetGetLength(nChainLengthCunningham1) > TargetGetLength(nChainLengthCunningham2))?
      (nChainLengthCunningham2 + TargetFromInt(TargetGetLength(nChainLengthCunningham2)+1)) :
   (nChainLengthCunningham1 + TargetFromInt(TargetGetLength(nChainLengthCunningham1)));
   if (fullTest)
      return (nChainLengthCunningham1 >= nBits || nChainLengthCunningham2 >= nBits || nChainLengthBiTwin >= nBits);
   else
      return nChainLengthBiTwin >= nBits;
}


// Test probable prime chain for: nOrigin
// Return value:
//   true - Probable prime chain found (one of nChainLength meeting target)
//   false - prime chain too short (none of nChainLength meeting target)
static bool ProbablePrimeChainTestFast(const mpz_class& mpzPrimeChainOrigin, CPrimalityTestParams& testParams)
{
   const unsigned int nBits = testParams.nBits;
   const unsigned int nCandidateType = testParams.nCandidateType;
   unsigned int& nChainLength = testParams.nChainLength;
   mpz_class& mpzOriginMinusOne = testParams.mpzOriginMinusOne;
   mpz_class& mpzOriginPlusOne = testParams.mpzOriginPlusOne;
   nChainLength = 0;

   // Test for Cunningham Chain of first kind
   if (nCandidateType == PRIME_CHAIN_CUNNINGHAM1)
   {
      mpzOriginMinusOne = mpzPrimeChainOrigin - 1;
      ProbableCunninghamChainTestFast(mpzOriginMinusOne, true, false, nChainLength, testParams, false);
   }
   else if (nCandidateType == PRIME_CHAIN_CUNNINGHAM2)
   {
      // Test for Cunningham Chain of second kind
      mpzOriginPlusOne = mpzPrimeChainOrigin + 1;
      ProbableCunninghamChainTestFast(mpzOriginPlusOne, false, false, nChainLength, testParams, false);
   }
   else
   {
      unsigned int nChainLengthCunningham1 = 0;
      unsigned int nChainLengthCunningham2 = 0;
      mpzOriginMinusOne = mpzPrimeChainOrigin - 1;
      if (ProbableCunninghamChainTestFast(mpzOriginMinusOne, true, false, nChainLengthCunningham1, testParams, true))
      {
         mpzOriginPlusOne = mpzPrimeChainOrigin + 1;
         ProbableCunninghamChainTestFast(mpzOriginPlusOne, false, false, nChainLengthCunningham2, testParams, true);

         // Figure out BiTwin Chain length
         // BiTwin Chain allows a single prime at the end for odd length chain
         nChainLength =
            (TargetGetLength(nChainLengthCunningham1) > TargetGetLength(nChainLengthCunningham2))?
            (nChainLengthCunningham2 + TargetFromInt(TargetGetLength(nChainLengthCunningham2)+1)) :
         (nChainLengthCunningham1 + TargetFromInt(TargetGetLength(nChainLengthCunningham1)));
      }
   }

   primeStats.nTestRound ++;

   return (nChainLength >= nBits);
}

//// Sieve for mining
//boost::thread_specific_ptr<CSieveOfEratosthenes> psieve;

// Mine probable prime chain of form: n = h * p# +/- 1
bool MineProbablePrimeChain(CSieveOfEratosthenes*& psieve, primecoinBlock_t* block, mpz_class& mpzFixedMultiplier, bool& fNewBlock, unsigned int& nTriedMultiplier, unsigned int& nProbableChainLength, 
	unsigned int& nTests, unsigned int& nPrimesHit, sint32 threadIndex, mpz_class& mpzHash, unsigned int nPrimorialMultiplier)
{
	nProbableChainLength = 0;
	nTests = 0;
	nPrimesHit = 0;
	fNewBlock = false;
	unsigned int lSieveTarget, lSieveBTTarget;
	if (nOverrideTargetValue > 0)
		lSieveTarget = nOverrideTargetValue;
	else
	{
		lSieveTarget = TargetGetLength(block->nBits);
		// If Difficulty gets within 1/36th of next target length, its actually more efficent to
		// increase the target length.. While technically worse for share val/hr - this should
		// help block rate.
		// Discussions with jh00 revealed this is non-linear, and graphs show that 0.1 diff is enough
		// to warrant a switch
		if (GetChainDifficulty(block->nBits) >= ((lSieveTarget + 1) - 0.1f))
			lSieveTarget++;
	}
	if (nOverrideBTTargetValue > 0)
		lSieveBTTarget = nOverrideBTTargetValue;
	else
		lSieveBTTarget = lSieveTarget; // Set to same as target
	//int64 nStart, nCurrent; // microsecond timer
	if (psieve == NULL || (psieve->getSieveExtensions() != nSieveExtensions))
	{  
		// Build sieve
		psieve = new CSieveOfEratosthenes(nMaxSieveSize, nSievePercentage, nSieveExtensions, lSieveTarget, lSieveBTTarget, mpzHash, mpzFixedMultiplier);
		psieve->Weave();
	}
	else
	{
		psieve->Init(nMaxSieveSize, nSievePercentage, nSieveExtensions, lSieveTarget, lSieveBTTarget, mpzHash, mpzFixedMultiplier);
		psieve->Weave();
	}
	primeStats.nSieveRounds++;
	primeStats.nCandidateCount += psieve->GetCandidateCount();
	mpz_class mpzHashMultiplier = mpzHash * mpzFixedMultiplier;
	mpz_class mpzChainOrigin;
	// Determine the sequence number of the round primorial
	unsigned int nPrimorialSeq = 0;
	while (vPrimes[nPrimorialSeq + 1] <= nPrimorialMultiplier)
		nPrimorialSeq++;
	// Allocate GMP variables for primality tests
	CPrimalityTestParams testParams(block->serverData.nBitsForShare, nPrimorialSeq);
	// References to test parameters
	unsigned int& nChainLength = testParams.nChainLength;
	unsigned int& nCandidateType = testParams.nCandidateType;
	//nStart = GetTickCount();
	//nCurrent = nStart;
	//uint32 timeStop = GetTickCount() + 25000;
	//int nTries = 0;
	bool multipleShare = false;
	mpz_class mpzPrevPrimeChainMultiplier = 0;
	bool rtnValue = false;
	uint64 start = getTimeMilliseconds();
	while (block->serverData.blockHeight == jhMiner_getCurrentWorkBlockHeight(block->threadIndex))
	{
		if (!psieve->GetNextCandidateMultiplier(nTriedMultiplier, nCandidateType))
		{			
			// power tests completed for the sieve
			fNewBlock = true; // notify caller to change nonce
			rtnValue = false;
			break;
		}
		nTests++;
		mpzChainOrigin = mpzHash * mpzFixedMultiplier * nTriedMultiplier;		
		nChainLength = 0;		
		ProbablePrimeChainTestFast(mpzChainOrigin, testParams);
		nProbableChainLength = nChainLength;
		sint32 shareDifficultyMajor = 0;
		primeStats.primeChainsFound++;
		if( nProbableChainLength >= 0x06000000 ){
			shareDifficultyMajor = (sint32)(nChainLength>>24);
		}else{
			continue;
		}

		if( nProbableChainLength > primeStats.bestPrimeChainDifficulty )
			primeStats.bestPrimeChainDifficulty = nProbableChainLength;

		if(nProbableChainLength >= block->serverData.nBitsForShare)
		{
			// Update Stats
			primeStats.chainCounter[0][std::min(shareDifficultyMajor,12)]++;
			primeStats.chainCounter[nCandidateType][std::min(shareDifficultyMajor,12)]++;
			primeStats.nChainHit++;

			block->mpzPrimeChainMultiplier = mpzFixedMultiplier * nTriedMultiplier;

			time_t now = time(0);
			struct tm * timeinfo;
			timeinfo = localtime (&now);
			char sNow [80];
			strftime (sNow, 80, "%x-%X",timeinfo);
			float shareDiff = GetChainDifficulty(nProbableChainLength);

			threadHearthBeat[threadIndex] = getTimeMilliseconds();


			if (bSoloMining)
			{
				printf("%s - BLOCK FOUND !!!  ---  DIFF: %f\n", sNow, shareDiff);
				return SubmitBlock( block);
			}
			else
			{
				// attempt to prevent duplicate share submissions.
				if (multipleShare && multiplierSet.find(block->mpzPrimeChainMultiplier) != multiplierSet.end())
					continue;

				// update server data
				block->serverData.client_shareBits = nProbableChainLength;
				// generate block raw data
				uint8 blockRawData[256] = {0};
				memcpy(blockRawData, block, 80);
				uint32 writeIndex = 80;
				sint32 lengthBN = 0;
				CBigNum bnPrimeChainMultiplier;
				bnPrimeChainMultiplier.SetHex(block->mpzPrimeChainMultiplier.get_str(16));
				std::vector<unsigned char> bnSerializeData = bnPrimeChainMultiplier.getvch();
				lengthBN = bnSerializeData.size();
				*(uint8*)(blockRawData+writeIndex) = (uint8)lengthBN; // varInt (we assume it always has a size low enough for 1 byte)
				writeIndex += 1;
				memcpy(blockRawData+writeIndex, &bnSerializeData[0], lengthBN);
				writeIndex += lengthBN;	
				// switch endianness
				for(uint32 f=0; f<256/4; f++)
				{
					*(uint32*)(blockRawData+f*4) = _swapEndianessU32(*(uint32*)(blockRawData+f*4));
				}

				if(!commandlineInput.silent)
					std::cout << sNow << " - SHARE FOUND! - (Th#:" << threadIndex << ") - DIFF:" << shareDiff << " - TYPE:" << nCandidateType << std::endl;
				if (commandlineInput.printDebug)
				{
					printf("HashNum        : %s\n", mpzHash.get_str(16).c_str());
					printf("FixedMultiplier: %s\n", mpzFixedMultiplier.get_str(16).c_str());
					printf("HashMultiplier : %u\n", nTriedMultiplier);
				}

				// submit this share
				multiplierSet.insert(block->mpzPrimeChainMultiplier);
				multipleShare = true;
				jhMiner_pushShare_primecoin(blockRawData, block);
				primeStats.foundShareCount ++;
				memset(blockRawData, 0, 256);
			}
			}
			//if(TargetGetLength(nProbableChainLength) >= 1)
			//	nPrimesHit++;
			//nCurrent = GetTickCount();
		}
		//if( *psieve )
		//{
		//	delete *psieve;
		//	*psieve = NULL;
		//}
		uint64 end = getTimeMilliseconds(); 
		primeStats.nTestTime += end-start;
		return rtnValue; // stop as timed out
	}


// prime target difficulty value for visualization
double GetPrimeDifficulty(unsigned int nBits){
   return ((double) nBits / (double) (1 << nFractionalBits));
}

// Estimate work transition target to longer prime chain
unsigned int EstimateWorkTransition(unsigned int nPrevWorkTransition, unsigned int nBits, unsigned int nChainLength)
{
	uint64_t nInterval = 500;
	uint64_t nWorkTransition = nPrevWorkTransition;
   unsigned int nBitsCeiling = 0;
   TargetSetLength(TargetGetLength(nBits)+1, nBitsCeiling);
   unsigned int nBitsFloor = 0;
   TargetSetLength(TargetGetLength(nBits), nBitsFloor);
   uint64 nFractionalDifficulty = TargetGetFractionalDifficulty(nBits);
   bool fLonger = (TargetGetLength(nChainLength) > TargetGetLength(nBits));
   if (fLonger)
      nWorkTransition = (nWorkTransition * (((nInterval - 1) * nFractionalDifficulty) >> 32) + 2 * ((uint64) nBitsFloor)) / ((((nInterval - 1) * nFractionalDifficulty) >> 32) + 2);
   else
      nWorkTransition = ((nInterval - 1) * nWorkTransition + 2 * ((uint64) nBitsCeiling)) / (nInterval + 1);
   return nWorkTransition;
}

//// prime chain type and length value
//std::string GetPrimeChainName(unsigned int nChainType, unsigned int nChainLength)
//{
//	return strprintf("%s%s", (nChainType==PRIME_CHAIN_CUNNINGHAM1)? "1CC" : ((nChainType==PRIME_CHAIN_CUNNINGHAM2)? "2CC" : "TWN"), TargetToString(nChainLength).c_str());
//}

/*
// Weave sieve for the next prime in table
// Return values:
//   True  - weaved another prime; nComposite - number of composites removed
//   False - sieve already completed
bool CSieveOfEratosthenes::WeaveOriginal()
{
if (nPrimeSeq >= vPrimesSize || vPrimes[nPrimeSeq] >= nSieveSize)
return false;  // sieve has been completed
CBigNum p = vPrimes[nPrimeSeq];
if (bnFixedFactor % p == 0)
{
// Nothing in the sieve is divisible by this prime
nPrimeSeq++;
return true;
}
// Find the modulo inverse of fixed factor
CBigNum bnFixedInverse;
if (!BN_mod_inverse(&bnFixedInverse, &bnFixedFactor, &p, pctx))
return error("CSieveOfEratosthenes::Weave(): BN_mod_inverse of fixed factor failed for prime #%u=%u", nPrimeSeq, vPrimes[nPrimeSeq]);
CBigNum bnTwo = 2;
CBigNum bnTwoInverse;
if (!BN_mod_inverse(&bnTwoInverse, &bnTwo, &p, pctx))
return error("CSieveOfEratosthenes::Weave(): BN_mod_inverse of 2 failed for prime #%u=%u", nPrimeSeq, vPrimes[nPrimeSeq]);

// Weave the sieve for the prime
unsigned int nChainLength = TargetGetLength(nBits);
for (unsigned int nBiTwinSeq = 0; nBiTwinSeq < 2 * nChainLength; nBiTwinSeq++)
{
// Find the first number that's divisible by this prime
int nDelta = ((nBiTwinSeq % 2 == 0)? (-1) : 1);
unsigned int nSolvedMultiplier = ((bnFixedInverse * (p - nDelta)) % p).getuint();
if (nBiTwinSeq % 2 == 1)
bnFixedInverse *= bnTwoInverse; // for next number in chain

unsigned int nPrime = vPrimes[nPrimeSeq];
if (nBiTwinSeq < nChainLength)
for (unsigned int nVariableMultiplier = nSolvedMultiplier; nVariableMultiplier < nSieveSize; nVariableMultiplier += nPrime)
vfCompositeBiTwin[nVariableMultiplier>>3] |= 1<<(nVariableMultiplier&7);
if (((nBiTwinSeq & 1u) == 0))
for (unsigned int nVariableMultiplier = nSolvedMultiplier; nVariableMultiplier < nSieveSize; nVariableMultiplier += nPrime)
vfCompositeCunningham1[nVariableMultiplier>>3] |= 1<<(nVariableMultiplier&7);
if (((nBiTwinSeq & 1u) == 1u))
for (unsigned int nVariableMultiplier = nSolvedMultiplier; nVariableMultiplier < nSieveSize; nVariableMultiplier += nPrime)
vfCompositeCunningham2[nVariableMultiplier>>3] |= 1<<(nVariableMultiplier&7);
}
nPrimeSeq++;
//delete[] p;
return true;
}
*/



/* akruppa's code */
int single_modinv (int a, int modulus)
{ /* start of single_modinv */
   int ps1, ps2, parity, dividend, divisor, rem, q, t;
   q = 1;
   rem = a;
   dividend = modulus;
   divisor = a;
   ps1 = 1;
   ps2 = 0;
   parity = 0;
   while (divisor > 1)
   {
      rem = dividend - divisor;
      t = rem - divisor;
      if (t >= 0) {
         q += ps1;
         rem = t;
         t -= divisor;
         if (t >= 0) {
            q += ps1;
            rem = t;
            t -= divisor;
            if (t >= 0) {
               q += ps1;
               rem = t;
               t -= divisor;
               if (t >= 0) {
                  q += ps1;
                  rem = t;
                  t -= divisor;
                  if (t >= 0) {
                     q += ps1;
                     rem = t;
                     t -= divisor;
                     if (t >= 0) {
                        q += ps1;
                        rem = t;
                        t -= divisor;
                        if (t >= 0) {
                           q += ps1;
                           rem = t;
                           t -= divisor;
                           if (t >= 0) {
                              q += ps1;
                              rem = t;
                              if (rem >= divisor) {
                                 q = dividend/divisor;
                                 rem = dividend - q * divisor;
                                 q *= ps1;
                              }}}}}}}}}
      q += ps2;
      parity = ~parity;
      dividend = divisor;
      divisor = rem;
      ps2 = ps1;
      ps1 = q;
   }

   if (parity == 0)
      return (ps1);
   else
      return (modulus - ps1);
} /* end of single_modinv */

/*
// Weave sieve for the next prime in table
// Return values:
//   True  - weaved another prime; nComposite - number of composites removed
//   False - sieve already completed
bool CSieveOfEratosthenes::WeaveFastAllBN()
{
//CBigNum p = vPrimes[nPrimeSeq];
// init some required bignums on the stack (no dynamic allocation at all)
BIGNUM bn_p;
BIGNUM bn_tmp;
BIGNUM bn_fixedInverse;
BIGNUM bn_twoInverse;
BIGNUM bn_fixedMod;
uint32 bignumData_p[0x200/4];
uint32 bignumData_tmp[0x200/4];
uint32 bignumData_fixedInverse[0x200/4];
uint32 bignumData_twoInverse[0x200/4];
uint32 bignumData_fixedMod[0x200/4];
fastInitBignum(bn_p, bignumData_p);
fastInitBignum(bn_tmp, bignumData_tmp);
fastInitBignum(bn_fixedInverse, bignumData_fixedInverse);
fastInitBignum(bn_twoInverse, bignumData_twoInverse);
fastInitBignum(bn_fixedMod, bignumData_fixedMod);

unsigned int nChainLength = TargetGetLength(nBits);
unsigned int nChainLengthX2 = nChainLength*2;

while( true )
{
if (nPrimeSeq >= vPrimesSize || vPrimes[nPrimeSeq] >= nSieveSize)
return false;  // sieve has been completed


BN_set_word(&bn_p, vPrimes[nPrimeSeq]);
BN2_div(NULL, &bn_tmp, &bnFixedFactor, &bn_p);


//if (bnFixedFactor % p == 0)
if( BN_is_zero(&bn_tmp) )
{
// Nothing in the sieve is divisible by this prime
nPrimeSeq++;
continue;
}

//CBigNum p = CBigNum(vPrimes[nPrimeSeq]);
// debug: Code is correct until here

// Find the modulo inverse of fixed factor
//if (!BN2_mod_inverse(&bn_fixedInverse, &bnFixedFactor, &bn_p, pctx))
//	return error("CSieveOfEratosthenes::Weave(): BN_mod_inverse of fixed factor failed for prime #%u=%u", nPrimeSeq, vPrimes[nPrimeSeq]);
//

//sint32 djf1 = single_modinv_ak(bn_tmp.d[0], vPrimes[nPrimeSeq]);
//sint32 djf2 = int_invert(bn_tmp.d[0], vPrimes[nPrimeSeq]);
//if( djf1 != djf2 )
//	__debugbreak();

BN_set_word(&bn_fixedInverse, single_modinv(bn_tmp.d[0], vPrimes[nPrimeSeq]));

//CBigNum bnTwo = 2;
//if (!BN_mod_inverse(&bn_twoInverse, &bn_constTwo, &bn_p, pctx))
//	return error("CSieveOfEratosthenes::Weave(): BN_mod_inverse of 2 failed for prime #%u=%u", nPrimeSeq, vPrimes[nPrimeSeq]);
//if( BN_cmp(&bn_twoInverse, &CBigNum(vPrimesTwoInverse[nPrimeSeq])) )
//	__debugbreak();

////BN_set_word(&bn_twoInverse, vPrimesTwoInverse[nPrimeSeq]);

uint64 pU64 = (uint64)vPrimes[nPrimeSeq];
uint64 fixedInverseU64 = BN_get_word(&bn_fixedInverse);
uint64 twoInverseU64 = vPrimesTwoInverse[nPrimeSeq];//BN_get_word(&bn_twoInverse);
// Weave the sieve for the prime
for (unsigned int nBiTwinSeq = 0; nBiTwinSeq < nChainLengthX2; nBiTwinSeq++)
{
// Find the first number that's divisible by this prime
//BN_copy(&bn_tmp, &bn_p);
//if( (nBiTwinSeq&1) == 0 )
//	BN_add_word(&bn_tmp, 1);
//else
//	BN_sub_word(&bn_tmp, 1);		
//BN_mul(&bn_tmp, &bn_fixedInverse, &bn_tmp, pctx);
//BN_mod(&bn_tmp, &bn_tmp, &bn_p, pctx);
//unsigned int nSolvedMultiplier = BN_get_word(&bn_tmp);

uint64 nSolvedMultiplier;
if( (nBiTwinSeq&1) == 0 )
nSolvedMultiplier = ((fixedInverseU64) * (pU64 + 1ULL)) % pU64;
else
nSolvedMultiplier = ((fixedInverseU64) * (pU64 - 1ULL)) % pU64;

//if( nSolvedMultiplier != nSolvedMultiplier2 )
//	__debugbreak();



if (nBiTwinSeq % 2 == 1)
{
fixedInverseU64 = (fixedInverseU64*twoInverseU64)%pU64;
}

unsigned int nPrime = vPrimes[nPrimeSeq];
if (nBiTwinSeq < nChainLength)
for (unsigned int nVariableMultiplier = nSolvedMultiplier; nVariableMultiplier < nSieveSize; nVariableMultiplier += nPrime)
vfCompositeBiTwin[nVariableMultiplier>>3] |= 1<<(nVariableMultiplier&7);
if (((nBiTwinSeq & 1u) == 0))
for (unsigned int nVariableMultiplier = nSolvedMultiplier; nVariableMultiplier < nSieveSize; nVariableMultiplier += nPrime)
vfCompositeCunningham1[nVariableMultiplier>>3] |= 1<<(nVariableMultiplier&7);
if (((nBiTwinSeq & 1u) == 1u))
for (unsigned int nVariableMultiplier = nSolvedMultiplier; nVariableMultiplier < nSieveSize; nVariableMultiplier += nPrime)
vfCompositeCunningham2[nVariableMultiplier>>3] |= 1<<(nVariableMultiplier&7);
}

nPrimeSeq++;
}
return false; // never reached
}
*/

/*
// Weave sieve for the next prime in table
// Return values:
//   True  - weaved another prime; nComposite - number of composites removed
//   False - sieve already completed
bool CSieveOfEratosthenes::WeaveFastAll()
{

// Faster GMP version
const unsigned int nChainLength = TargetGetLength(nBits);
unsigned int nPrimeSeq = 0;
unsigned int vPrimesSize2 = vPrimesSize;

// Keep all variables local for max performance
unsigned int nSieveSize = this->nSieveSize;

// Process only 10% of the primes
// Most composites are still found
vPrimesSize2 = (uint64)vPrimesSize2 *nSievePercentage / 100;


mpz_t mpzFixedFactor; // fixed factor to derive the chain
mpz_t mpzFixedFactorMod;
mpz_t p;
mpz_t mpzFixedInverse;
//mpz_t mpzTwo;
mpz_t mpzTwoInverse;


unsigned long nFixedFactorMod;
unsigned long nFixedInverse;
unsigned long nTwoInverse;

mpz_init_set(mpzFixedFactor, this->mpzFixedFactor.get_mpz_t());
mpz_init(mpzFixedFactorMod);
mpz_init(p);
mpz_init(mpzFixedInverse);
//mpz_init_set_ui(mpzTwo, 2);
mpz_init(mpzTwoInverse);

while( true )
{
unsigned int nPrime = vPrimes[nPrimeSeq];
if (nPrimeSeq >= vPrimesSize2 || nPrime >= nSieveSize)
{
break;
//return false;  // sieve has been completed

}
//BN_set_word(&bn_p, nPrime);
//mpz_set_ui(p, nPrime);

//BN_mod(&bn_tmp, &bnFixedFactor, &bn_p, pctx);
nFixedFactorMod = mpz_tdiv_r_ui(mpzFixedFactorMod, mpzFixedFactor, nPrime);

if (nFixedFactorMod == 0)
{
// Nothing in the sieve is divisible by this prime
nPrimeSeq++;
continue;
}
mpz_set_ui(p, nPrime);

// Find the modulo inverse of fixed factor
if (!mpz_invert(mpzFixedInverse, mpzFixedFactorMod, p))
return error("CSieveOfEratosthenes::Weave(): mpz_invert of fixed factor failed for prime #%u=%u", nPrimeSeq, vPrimes[nPrimeSeq]);
//nFixedInverse = mpz_get_ui(mpzFixedInverse);
if (!mpz_invert(mpzTwoInverse, mpzTwo, p))
return error("CSieveOfEratosthenes::Weave(): mpz_invert of 2 failed for prime #%u=%u", nPrimeSeq, vPrimes[nPrimeSeq]);
//nTwoInverse = mpz_get_ui(mpzTwoInverse);

uint64 pU64 = (uint64)vPrimes[nPrimeSeq];
//uint64 fixedInverseU64 = BN_get_word(&bn_fixedInverse);
uint64 fixedInverseU64 = mpz_get_ui(mpzFixedInverse);
//uint64 twoInverseU64 = BN_get_word(&bn_twoInverse);
uint64 twoInverseU64 = mpz_get_ui(mpzTwoInverse);
// Weave the sieve for the prime		
for (unsigned int nBiTwinSeq = 0; nBiTwinSeq < 2 * nChainLength; nBiTwinSeq++)
{

uint64 nSolvedMultiplier;

if( (nBiTwinSeq&1) == 0 )
nSolvedMultiplier = ((fixedInverseU64) * (pU64 + 1ULL)) % pU64;
else
nSolvedMultiplier = ((fixedInverseU64) * (pU64 - 1ULL)) % pU64;

//if( nSolvedMultiplier != nSolvedMultiplier2 )
//	__debugbreak();

if (nBiTwinSeq % 2 == 1)
{
fixedInverseU64 = (fixedInverseU64*twoInverseU64)%pU64;
}
unsigned int nPrime = vPrimes[nPrimeSeq];
if (nBiTwinSeq < nChainLength)
for (unsigned int nVariableMultiplier = nSolvedMultiplier; nVariableMultiplier < nSieveSize; nVariableMultiplier += nPrime)
vfCompositeBiTwin[nVariableMultiplier>>3] |= 1<<(nVariableMultiplier&7);
if (((nBiTwinSeq & 1u) == 0))
for (unsigned int nVariableMultiplier = nSolvedMultiplier; nVariableMultiplier < nSieveSize; nVariableMultiplier += nPrime)
vfCompositeCunningham1[nVariableMultiplier>>3] |= 1<<(nVariableMultiplier&7);
if (((nBiTwinSeq & 1u) == 1u))
for (unsigned int nVariableMultiplier = nSolvedMultiplier; nVariableMultiplier < nSieveSize; nVariableMultiplier += nPrime)
vfCompositeCunningham2[nVariableMultiplier>>3] |= 1<<(nVariableMultiplier&7);
}
nPrimeSeq++;
}
this->nPrimeSeq = nPrimeSeq;

mpz_clear(mpzFixedFactor);
mpz_clear(mpzFixedFactorMod);
mpz_clear(p);
mpz_clear(mpzFixedInverse);
//mpz_clear(mpzTwo);
mpz_clear(mpzTwoInverse);

return false; // never reached
}
*/

static unsigned int int_invert(unsigned int a, unsigned int nPrime)
{
   // Extended Euclidean algorithm to calculate the inverse of a in finite field defined by nPrime
   int rem0 = nPrime, rem1 = a % nPrime, rem2;
   int aux0 = 0, aux1 = 1, aux2;
   int quotient, inverse;

   while (1)
   {
      if (rem1 <= 1)
      {
         inverse = aux1;
         break;
      }

      rem2 = rem0 % rem1;
      quotient = rem0 / rem1;
      aux2 = -quotient * aux1 + aux0;

      if (rem2 <= 1)
      {
         inverse = aux2;
         break;
      }

      rem0 = rem1 % rem2;
      quotient = rem1 / rem2;
      aux0 = -quotient * aux2 + aux1;

      if (rem0 <= 1)
      {
         inverse = aux0;
         break;
      }

      rem1 = rem2 % rem0;
      quotient = rem2 / rem0;
      aux1 = -quotient * aux0 + aux2;
   }

   return (inverse + nPrime) % nPrime;
}



void CSieveOfEratosthenes::ProcessMultiplier(sieve_word_t *vfComposites, const unsigned int nMinMultiplier, const unsigned int nMaxMultiplier, const std::vector<unsigned int>& vPrimes, unsigned int *vMultipliers, unsigned int nLayerSeq)
{
   // Wipe the part of the array first
   if (nMinMultiplier < nMaxMultiplier)
      memset(vfComposites + GetWordNum(nMinMultiplier), 0, (nMaxMultiplier - nMinMultiplier + nWordBits - 1) / nWordBits * sizeof(sieve_word_t));

   for (unsigned int nPrimeSeqLocal = nMinPrimeSeq; nPrimeSeqLocal < nPrimes; nPrimeSeqLocal++)
{
      const unsigned int nPrime = vPrimes[nPrimeSeqLocal];
      const unsigned int nMultiplierPos = nPrimeSeqLocal * nSieveLayers + nLayerSeq;
      unsigned int nVariableMultiplier = vMultipliers[nMultiplierPos];
      if (nVariableMultiplier < nMinMultiplier)
         nVariableMultiplier += (nMinMultiplier - nVariableMultiplier + nPrime - 1) / nPrime * nPrime;
#ifdef USE_ROTATE
      const unsigned int nRotateBits = nPrime % nWordBits;
      sieve_word_t lBitMask = GetBitMask(nVariableMultiplier);
      for (; nVariableMultiplier < nMaxMultiplier; nVariableMultiplier += nPrime)
      {
         vfComposites[GetWordNum(nVariableMultiplier)] |= lBitMask;
         lBitMask = (lBitMask << nRotateBits) | (lBitMask >> (nWordBits - nRotateBits));
      }
      vMultipliers[nMultiplierPos] = nVariableMultiplier;
#else
      for (; nVariableMultiplier < nMaxMultiplier; nVariableMultiplier += nPrime)
      {
         vfComposites[GetWordNum(nVariableMultiplier)] |= GetBitMask(nVariableMultiplier);
      }
      vMultipliers[nMultiplierPos] = nVariableMultiplier;
#endif
   }
}


// Weave sieve for the next prime in table
// Return values:
//   True  - weaved another prime; nComposite - number of composites removed
//   False - sieve already completed
bool CSieveOfEratosthenes::Weave()
{
	// Faster GMP version
   uint64 start;
	if(bEnablenPrimorialMultiplierTuning){
   start = getTimeMilliseconds(); 
	}
	// Keep all variables local for max performance
   const unsigned int nChainLength = this->nChainLength;
   const unsigned int nBTChainLength = this->nBTChainLength;
   const unsigned int nSieveSize = this->nSieveSize;
   mpz_class mpzHash = this->mpzHash;
   mpz_class mpzFixedMultiplier = this->mpzFixedMultiplier;

   // Process only a set percentage of the primes
   // Most composites are still found
   const unsigned int nPrimes = this->nPrimes;
   sieve_word_t *vfCandidates = this->vfCandidates;

   // Check whether fixed multiplier fits in an unsigned long
   //bool fUseLongForFixedMultiplier = mpzFixedMultiplier < ULONG_MAX;
   bool fUseLongForFixedMultiplier = false;
   unsigned long nFixedMultiplier;
   mpz_class mpzFixedFactor;
   if (fUseLongForFixedMultiplier)
      nFixedMultiplier = mpzFixedMultiplier.get_ui();
   else
      mpzFixedFactor = mpzHash * mpzFixedMultiplier;

   unsigned int nCombinedEndSeq = this->nPrimeSeq;
   unsigned int nFixedFactorCombinedMod = 0;

   for (unsigned int nPrimeSeqLocal = this->nPrimeSeq; nPrimeSeqLocal < nPrimes; nPrimeSeqLocal++)
   {
      // TODO: on new block stop
      //if (pindexPrev != pindexBest)
      //    break;  // new block
      unsigned int nPrime = vPrimes[nPrimeSeqLocal];
      if (nPrimeSeqLocal >= nCombinedEndSeq)
      {
         // Combine multiple primes to produce a big divisor
         unsigned int nPrimeCombined = 1;
         while (nPrimeCombined < UINT_MAX / vPrimes[nCombinedEndSeq]  )
         {
            nPrimeCombined *= vPrimes[nCombinedEndSeq];
            nCombinedEndSeq++;
         }

         if (fUseLongForFixedMultiplier)
         {
            nFixedFactorCombinedMod = mpz_tdiv_ui(mpzHash.get_mpz_t(), nPrimeCombined);
            nFixedFactorCombinedMod = (uint64)nFixedFactorCombinedMod * (nFixedMultiplier % nPrimeCombined) % nPrimeCombined;
         }
         else
            nFixedFactorCombinedMod = mpz_tdiv_ui(mpzFixedFactor.get_mpz_t(), nPrimeCombined);
      }

      unsigned int nFixedFactorMod = nFixedFactorCombinedMod % nPrime;
      if (nFixedFactorMod == 0)
      {
         // Nothing in the sieve is divisible by this prime
         continue;
      }
      // Find the modulo inverse of fixed factor
      unsigned int nFixedInverse = int_invert(nFixedFactorMod, nPrime);
      if (!nFixedInverse)
         return error("CSieveOfEratosthenes::Weave(): int_invert of fixed factor failed for prime #%u=%u", nPrimeSeqLocal, vPrimes[nPrimeSeqLocal]);
      unsigned int nTwoInverse = (nPrime + 1) / 2;

      // Check whether 32-bit arithmetic can be used for nFixedInverse
      const bool fUse32BArithmetic = (UINT_MAX / nTwoInverse) >= nPrime;
      unsigned int multiplierPos = nPrimeSeqLocal * nSieveLayers;
      if (fUse32BArithmetic)
      {
      // Weave the sieve for the prime
         for (unsigned int nChainSeq = 0; nChainSeq < nSieveLayers; nChainSeq++)
      {
         // Find the first number that's divisible by this prime
            vCunningham1Multipliers[multiplierPos] = nFixedInverse;
            vCunningham2Multipliers[multiplierPos] = nPrime - nFixedInverse;

            // For next number in chain
            nFixedInverse = nFixedInverse * nTwoInverse % nPrime;

            // Increment Multiplier Positon.
            multiplierPos++;
         }
         }
         else
         {
         // Weave the sieve for the prime
         for (unsigned int nChainSeq = 0; nChainSeq < nSieveLayers; nChainSeq++)
      {
         // Find the first number that's divisible by this prime
            vCunningham1Multipliers[multiplierPos] = nFixedInverse;
            vCunningham2Multipliers[multiplierPos] = nPrime - nFixedInverse;

            // For next number in chain
            nFixedInverse = (uint64)nFixedInverse * nTwoInverse % nPrime;

            // Increment Multiplier Positon.
            multiplierPos++;
         }
      }
   }

   // Number of elements that are likely to fit in L1 cache
   // NOTE: This needs to be a multiple of nWordBits
   const unsigned int nL1CacheElements = primeStats.nL1CacheElements;
   const unsigned int nArrayRounds = (nSieveSize + nL1CacheElements - 1) / nL1CacheElements;

   // Calculate the number of CC1 and CC2 layers needed for BiTwin candidates
   const unsigned int nBiTwinCC1Layers = (nBTChainLength + 1) / 2;
   const unsigned int nBiTwinCC2Layers = nBTChainLength / 2;

   // Only 50% of the array is used in extensions
   const unsigned int nExtensionsMinMultiplier = nSieveSize / 2;
   const unsigned int nExtensionsMinWord = nExtensionsMinMultiplier / nWordBits;

   // Loop over each array one at a time for optimal L1 cache performance
   for (unsigned int j = 0; j < nArrayRounds; j++)
   {
      const unsigned int nMinMultiplier = nL1CacheElements * j;
      const unsigned int nMaxMultiplier = std::min(nL1CacheElements * (j + 1), nSieveSize);
      const unsigned int nExtMinMultiplier = std::max(nMinMultiplier, nExtensionsMinMultiplier);
      const unsigned int nMinWord = nMinMultiplier / nWordBits;
      const unsigned int nMaxWord = (nMaxMultiplier + nWordBits - 1) / nWordBits;
      const unsigned int nExtMinWord = std::max(nMinWord, nExtensionsMinWord);

      //if (pindexPrev != pindexBest)
      //    break;  // new block

      // Loop over the layers
      for (unsigned int nLayerSeq = 0; nLayerSeq < nSieveLayers; nLayerSeq++) {
         //if (pindexPrev != pindexBest)
         //    break;  // new block
         if (nLayerSeq < nChainLength)
         {
            ProcessMultiplier(vfCompositeLayerCC1, nMinMultiplier, nMaxMultiplier, vPrimes, vCunningham1Multipliers, nLayerSeq);
            ProcessMultiplier(vfCompositeLayerCC2, nMinMultiplier, nMaxMultiplier, vPrimes, vCunningham2Multipliers, nLayerSeq);
         }
         else
         {
            // Optimize: First halves of the arrays are not needed in the extensions
            ProcessMultiplier(vfCompositeLayerCC1, nExtMinMultiplier, nMaxMultiplier, vPrimes, vCunningham1Multipliers, nLayerSeq);
            ProcessMultiplier(vfCompositeLayerCC2, nExtMinMultiplier, nMaxMultiplier, vPrimes, vCunningham2Multipliers, nLayerSeq);
      }

         // Apply the layer to the primary sieve arrays
         if (nLayerSeq < nChainLength)
         {
            if (nLayerSeq < nBiTwinCC2Layers)
            {
               for (unsigned int nWord = nMinWord; nWord < nMaxWord; nWord++)
               {
                  vfCompositeCunningham1[nWord] |= vfCompositeLayerCC1[nWord];
                  vfCompositeCunningham2[nWord] |= vfCompositeLayerCC2[nWord];
                  vfCompositeBiTwin[nWord] |= vfCompositeLayerCC1[nWord] | vfCompositeLayerCC2[nWord];
               }
            }
            else if (nLayerSeq < nBiTwinCC1Layers)
            {
               for (unsigned int nWord = nMinWord; nWord < nMaxWord; nWord++)
               {
                  vfCompositeCunningham1[nWord] |= vfCompositeLayerCC1[nWord];
                  vfCompositeCunningham2[nWord] |= vfCompositeLayerCC2[nWord];
                  vfCompositeBiTwin[nWord] |= vfCompositeLayerCC1[nWord];
               }
            }
            else
            {
               for (unsigned int nWord = nMinWord; nWord < nMaxWord; nWord++)
               {
                  vfCompositeCunningham1[nWord] |= vfCompositeLayerCC1[nWord];
                  vfCompositeCunningham2[nWord] |= vfCompositeLayerCC2[nWord];
               }
            }
         }

         // Apply the layer to extensions
         for (unsigned int nExtensionSeq = 0; nExtensionSeq < nSieveExtensions; nExtensionSeq++)
      {
            const unsigned int nLayerOffset = nExtensionSeq + 1;
            if (nLayerSeq >= nLayerOffset && nLayerSeq < nChainLength + nLayerOffset)
         {
               const unsigned int nLayerExtendedSeq = nLayerSeq - nLayerOffset;
               sieve_word_t *vfExtCC1 = vfExtendedCompositeCunningham1 + nExtensionSeq * nCandidatesWords;
               sieve_word_t *vfExtCC2 = vfExtendedCompositeCunningham2 + nExtensionSeq * nCandidatesWords;
               sieve_word_t *vfExtTWN = vfExtendedCompositeBiTwin + nExtensionSeq * nCandidatesWords;
               if (nLayerExtendedSeq < nBiTwinCC2Layers)
               {
                  for (unsigned int nWord = nExtMinWord; nWord < nMaxWord; nWord++)
                  {
                     vfExtCC1[nWord] |= vfCompositeLayerCC1[nWord];
                     vfExtCC2[nWord] |= vfCompositeLayerCC2[nWord];
                     vfExtTWN[nWord] |= vfCompositeLayerCC1[nWord] | vfCompositeLayerCC2[nWord];
                  }
               }
               else if (nLayerExtendedSeq < nBiTwinCC1Layers)
               {
                  for (unsigned int nWord = nExtMinWord; nWord < nMaxWord; nWord++)
                  {
                     vfExtCC1[nWord] |= vfCompositeLayerCC1[nWord];
                     vfExtCC2[nWord] |= vfCompositeLayerCC2[nWord];
                     vfExtTWN[nWord] |= vfCompositeLayerCC1[nWord];
                  }
               }
               else
               {
                  for (unsigned int nWord = nExtMinWord; nWord < nMaxWord; nWord++)
                  {
                     vfExtCC1[nWord] |= vfCompositeLayerCC1[nWord];
                     vfExtCC2[nWord] |= vfCompositeLayerCC2[nWord];
                  }
               }
            }
         }
      }

      // Combine the bitsets
      // vfCandidates = ~(vfCompositeCunningham1 & vfCompositeCunningham2 & vfCompositeBiTwin)
      for (unsigned int i = nMinWord; i < nMaxWord; i++)
         vfCandidates[i] = ~(vfCompositeCunningham1[i] & vfCompositeCunningham2[i] & vfCompositeBiTwin[i]);

      // Combine the extended bitsets
      for (unsigned int j = 0; j < nSieveExtensions; j++)
         for (unsigned int i = nExtMinWord; i < nMaxWord; i++)
            vfExtendedCandidates[j * nCandidatesWords + i] = ~(
            vfExtendedCompositeCunningham1[j * nCandidatesWords + i] &
            vfExtendedCompositeCunningham2[j * nCandidatesWords + i] &
            vfExtendedCompositeBiTwin[j * nCandidatesWords + i]);
   }

   // The sieve has been partially weaved
   this->nPrimeSeq = nPrimes - 1;

   if(bEnablenPrimorialMultiplierTuning){
   uint64 end = getTimeMilliseconds(); 
   primeStats.nWaveTime += end-start;
   primeStats.nWaveRound ++;
   }
   return false;
}

