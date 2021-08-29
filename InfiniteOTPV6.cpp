#include "SampleSubstitutionTable.hpp"

#include <assert.h>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <vector>

using namespace std;

bool CHECKASSERT = false;
#define DASSERT(cond) if (CHECKASSERT) { assert(cond); }

#define SWAP_SHORTS(...)	SWAP_SHORTS_(__VA_ARGS__)

#define SWAP_SHORTS_(a, b)				\
	do {						\
		uint16_t temp = a;			\
		a = b;					\
		b = temp;				\
	} while(0)

//vector<uint16_t> substitutionTable;
vector<vector<uint16_t>> OTPs;
vector<uint16_t> xoredEPUK_OTP;
vector<uint16_t> superRearrangementTable;
vector<uint16_t> reverseSuperRearrangementTable;
static vector<uint16_t> EPUK;

uint32_t test;

constexpr uint32_t k1 = 1024;
constexpr uint32_t k4 = 4*k1;
constexpr uint32_t k64 = 64*k1;
constexpr uint32_t elementsIn1024bits = k1 / (sizeof(uint16_t) * 8);

void printVector(vector<uint16_t>& input, uint32_t start, uint32_t end) {
	for(uint32_t i = start; i < end; ++i) {
		cout << hex << input[i] << dec << "  " << setw(3);
		if (i != start && i % 10 == 0) {
			cout << endl;
		}
	}
	cout << endl;
}

void printVectorAsNibbles(vector<uint16_t>& input, uint32_t start, uint32_t end) {
	cout << " ";
	for(uint32_t i = start; i < end; ++i) {
		for (uint32_t k = 0; k < 2; ++k) {
			uint8_t val = *reinterpret_cast<uint8_t*>(input.data()+i);
			cout << hex << (val & 0x0F) << dec << " " << setw(2);
			cout << hex << ((val >> 4) & 0x0F) << dec << " " << setw(2);
		}
		if (i && i % 4 == 0) cout << endl;
	}
	cout << endl;
}

void printDiff(vector<uint16_t>& vec1, vector<uint16_t>& vec2) {
	uint32_t size = (vec1.size() < vec2.size() ? vec1.size() : vec2.size());
	for (uint32_t i = 0; i < size; ++i) {
		if (vec1[i] != vec2[i]) {
			cout << hex << vec1[i] << " != " << vec2[i] << " at index " << i << dec << endl;
		}
	}
}

string convertDurationToString(chrono::duration<double, nano> nanoSeconds) {
	uint64_t timeDuration = nanoSeconds.count();
	if (timeDuration > 100000 && timeDuration < 100000*1000UL) {
		return to_string(chrono::duration_cast<chrono::microseconds>(nanoSeconds).count()) + " microSeconds";  
	}
	if (timeDuration > 100000*1000UL && timeDuration < uint64_t(100000*1000*1000UL)) {
		return to_string(chrono::duration_cast<chrono::milliseconds>(nanoSeconds).count()) + " milliSeconds";
	}
	if (timeDuration > uint64_t(100000UL*1000*1000) && timeDuration < uint64_t(100000UL*1000*1000*60)) {
		return to_string(chrono::duration_cast<chrono::seconds>(nanoSeconds).count()) + " seconds";
	}
	if (timeDuration > uint64_t(100000UL*1000*1000*60)) {
		return to_string(chrono::duration_cast<chrono::minutes>(nanoSeconds).count()) + " minutes";
	}
	return to_string(chrono::duration_cast<chrono::nanoseconds>(nanoSeconds).count()) + " nanoSeconds";  
}

class Timer {
	chrono::high_resolution_clock::time_point startTime;
	chrono::high_resolution_clock::time_point endTime;
	chrono::duration<double, nano> duration;
	chrono::duration<double, nano>& encryptionDuration;
	const string desc;
	public:
	Timer(const string& desc, chrono::duration<double, nano>& encryptionDuration) : desc(desc), encryptionDuration(encryptionDuration) {
		startTime = chrono::high_resolution_clock::now();
	}

	~Timer() {
		endTime = chrono::high_resolution_clock::now();
		duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
		encryptionDuration += duration;
		cout << desc << convertDurationToString(duration) << endl;
	}
};

/***
void initializeSubstitutionTable() {
	substitutionTable.resize(k64);
	for(uint16_t i = 0; i < k64 / 2; ++i) {
		substitutionTable[i] = (i + (k64/2));
		substitutionTable[i + (k64/2)] = i;
	}
}
***/

uint16_t getSuperIndex(uint16_t j, uint16_t i) { // Note: i and j should be 1-indexed
	uint16_t newIndex = i + (256 * (j - 1)) - 1;
	return newIndex;
}

void initializeSuperRearrangementTable() {
	superRearrangementTable.resize(256*32);
	reverseSuperRearrangementTable.resize(256*32);
	for(uint16_t i = 0; i < 256*32; ++i) {
		uint16_t newInd = getSuperIndex((i % 32) + 1, (i / 32) + 1);
		superRearrangementTable[i] = newInd;
		reverseSuperRearrangementTable[newInd] = i;
	}
}

void initializeOTPsections() {
	uint32_t sectionCount = k64;
	uint32_t sectionSize = 64 / sizeof(uint16_t); // each section is 64 bytes
	OTPs.resize(sectionCount);
	srand(0);
	for (uint32_t i = 0; i < sectionCount; ++i) {
		OTPs[i].resize(sectionSize);
		for (uint32_t j = 0; j < sectionSize; ++j) {
			OTPs[i][j] = rand();
		}
	}
}

void initializeXoredEPUK_OTP() {
	uint32_t numRounds = 4;
	uint32_t numShorts = k4 / sizeof(uint16_t); // each page is 4K bytes
	xoredEPUK_OTP.resize(numRounds * numShorts);
	uint16_t OTPsectionCount = 0;
	for (uint32_t i = 0; i < numRounds; ++i) {
		for (uint16_t m = 0; m < 32; ++m) {
			uint16_t cachedEPUK = EPUK[OTPsectionCount];
			for (uint16_t k = 0; k < 32; ++k) {
				xoredEPUK_OTP[(i*numShorts)+(m*64)+k] = EPUK[k] ^ OTPs[cachedEPUK][k];
			}
			cachedEPUK = EPUK[++OTPsectionCount];
			for (uint16_t k = 32; k < 64; ++k) {
				xoredEPUK_OTP[(i*numShorts)+(m*64)+k] = EPUK[k] ^ OTPs[cachedEPUK][k-32];
			}
		}
	}
}


#define SA(offset)	(bptr1+offset)
#define SB(offset)	(bptr1+8+offset)
#define SC(offset)	(bptr1+16+offset)
#define SD(offset)	(bptr1+24+offset)
#define SE(offset)	(bptr1+32+offset)
#define SF(offset)	(bptr1+40+offset)
#define SG(offset)	(bptr1+48+offset)
#define SH(offset)	(bptr1+56+offset)


#define cycles 3

// This function works in near AES compatibility mode where the encryption block size is 1024 bits
// and key strength is 1024 bits.
void replaceFoldCyclesCM(uint16_t* input, uint16_t* epuk_otpp, uint32_t start, uint32_t end) {

	{
		// Process 1st section of 128 bits
		register uint16_t* bptr1 = input+start;
		register uint16_t* epuk_otp = epuk_otpp+start;

		uint16_t va1 = substitutionTable[*SA(0) ^ epuk_otp[0]];
		uint16_t va2 = va1 ^ substitutionTable[*SA(1) ^ epuk_otp[1]];
		uint16_t va3 = va2 ^ substitutionTable[*SA(2) ^ epuk_otp[2]];
		uint16_t va4 = va3 ^ substitutionTable[*SA(3) ^ epuk_otp[3]];
		uint16_t va5 = va4 ^ substitutionTable[*SA(4) ^ epuk_otp[4]];
		uint16_t va6 = va5 ^ substitutionTable[*SA(5) ^ epuk_otp[5]];
		uint16_t va7 = va6 ^ substitutionTable[*SA(6) ^ epuk_otp[6]];
		uint16_t va8 = va7 ^ substitutionTable[*SA(7) ^ epuk_otp[7]];

		// Process 2nd section of 128 bits
		uint16_t vb1 = va8 ^ substitutionTable[*SB(0) ^ epuk_otp[8]];
		uint16_t vb2 = vb1 ^ substitutionTable[*SB(1) ^ epuk_otp[9]];
		uint16_t vb3 = vb2 ^ substitutionTable[*SB(2) ^ epuk_otp[10]];
		uint16_t vb4 = vb3 ^ substitutionTable[*SB(3) ^ epuk_otp[11]];
		uint16_t vb5 = vb4 ^ substitutionTable[*SB(4) ^ epuk_otp[12]];
		uint16_t vb6 = vb5 ^ substitutionTable[*SB(5) ^ epuk_otp[13]];
		uint16_t vb7 = vb6 ^ substitutionTable[*SB(6) ^ epuk_otp[14]];
		uint16_t vb8 = vb7 ^ substitutionTable[*SB(7) ^ epuk_otp[15]];

		// Process 3rd section of 128 bits
		uint16_t vc1 = vb8 ^ substitutionTable[*SC(0) ^ epuk_otp[16]];
		uint16_t vc2 = vc1 ^ substitutionTable[*SC(1) ^ epuk_otp[17]];
		uint16_t vc3 = vc2 ^ substitutionTable[*SC(2) ^ epuk_otp[18]];
		uint16_t vc4 = vc3 ^ substitutionTable[*SC(3) ^ epuk_otp[19]];
		uint16_t vc5 = vc4 ^ substitutionTable[*SC(4) ^ epuk_otp[20]];
		uint16_t vc6 = vc5 ^ substitutionTable[*SC(5) ^ epuk_otp[21]];
		uint16_t vc7 = vc6 ^ substitutionTable[*SC(6) ^ epuk_otp[22]];
		uint16_t vc8 = vc7 ^ substitutionTable[*SC(7) ^ epuk_otp[23]];


		// Process 4th section of 128 bits
		uint16_t vd1 = vc8 ^ substitutionTable[*SD(0) ^ epuk_otp[24]];
		uint16_t vd2 = vd1 ^ substitutionTable[*SD(1) ^ epuk_otp[25]];
		uint16_t vd3 = vd2 ^ substitutionTable[*SD(2) ^ epuk_otp[26]];
		uint16_t vd4 = vd3 ^ substitutionTable[*SD(3) ^ epuk_otp[27]];
		uint16_t vd5 = vd4 ^ substitutionTable[*SD(4) ^ epuk_otp[28]];
		uint16_t vd6 = vd5 ^ substitutionTable[*SD(5) ^ epuk_otp[29]];
		uint16_t vd7 = vd6 ^ substitutionTable[*SD(6) ^ epuk_otp[30]];
		uint16_t vd8 = vd7 ^ substitutionTable[*SD(7) ^ epuk_otp[31]];


		// Process 5th section of 128 bits
		uint16_t ve1 = vd8 ^ substitutionTable[*SE(0) ^ epuk_otp[32]];
		uint16_t ve2 = ve1 ^ substitutionTable[*SE(1) ^ epuk_otp[33]];
		uint16_t ve3 = ve2 ^ substitutionTable[*SE(2) ^ epuk_otp[34]];
		uint16_t ve4 = ve3 ^ substitutionTable[*SE(3) ^ epuk_otp[35]];
		uint16_t ve5 = ve4 ^ substitutionTable[*SE(4) ^ epuk_otp[36]];
		uint16_t ve6 = ve5 ^ substitutionTable[*SE(5) ^ epuk_otp[37]];
		uint16_t ve7 = ve6 ^ substitutionTable[*SE(6) ^ epuk_otp[38]];
		uint16_t ve8 = ve7 ^ substitutionTable[*SE(7) ^ epuk_otp[39]];


		// Process 6th section of 128 bits
		uint16_t vf1 = ve8 ^ substitutionTable[*SF(0) ^ epuk_otp[40]];
		uint16_t vf2 = vf1 ^ substitutionTable[*SF(1) ^ epuk_otp[41]];
		uint16_t vf3 = vf2 ^ substitutionTable[*SF(2) ^ epuk_otp[42]];
		uint16_t vf4 = vf3 ^ substitutionTable[*SF(3) ^ epuk_otp[43]];
		uint16_t vf5 = vf4 ^ substitutionTable[*SF(4) ^ epuk_otp[44]];
		uint16_t vf6 = vf5 ^ substitutionTable[*SF(5) ^ epuk_otp[45]];
		uint16_t vf7 = vf6 ^ substitutionTable[*SF(6) ^ epuk_otp[46]];
		uint16_t vf8 = vf7 ^ substitutionTable[*SF(7) ^ epuk_otp[47]];


		// Process 7th section of 128 bits
		uint16_t vg1 = vf8 ^ substitutionTable[*SG(0) ^ epuk_otp[48]];
		uint16_t vg2 = vg1 ^ substitutionTable[*SG(1) ^ epuk_otp[49]];
		uint16_t vg3 = vg2 ^ substitutionTable[*SG(2) ^ epuk_otp[50]];
		uint16_t vg4 = vg3 ^ substitutionTable[*SG(3) ^ epuk_otp[51]];
		uint16_t vg5 = vg4 ^ substitutionTable[*SG(4) ^ epuk_otp[52]];
		uint16_t vg6 = vg5 ^ substitutionTable[*SG(5) ^ epuk_otp[53]];
		uint16_t vg7 = vg6 ^ substitutionTable[*SG(6) ^ epuk_otp[54]];
		uint16_t vg8 = vg7 ^ substitutionTable[*SG(7) ^ epuk_otp[55]];


		// Process 8th section of 128 bits
		uint16_t vh1 = vg8 ^ substitutionTable[*SH(0) ^ epuk_otp[56]];
		uint16_t vh2 = vh1 ^ substitutionTable[*SH(1) ^ epuk_otp[57]];
		uint16_t vh3 = vh2 ^ substitutionTable[*SH(2) ^ epuk_otp[58]];
		uint16_t vh4 = vh3 ^ substitutionTable[*SH(3) ^ epuk_otp[59]];
		uint16_t vh5 = vh4 ^ substitutionTable[*SH(4) ^ epuk_otp[60]];
		uint16_t vh6 = vh5 ^ substitutionTable[*SH(5) ^ epuk_otp[61]];
		uint16_t vh7 = vh6 ^ substitutionTable[*SH(6) ^ epuk_otp[62]];
		uint16_t vh8 = vh7 ^ substitutionTable[*SH(7) ^ epuk_otp[63]];

		va1 ^= vh8;

		for (int i = 1; i < cycles; ++i) {
			epuk_otp += 2048; // epuk_otp - 64 = starting value previous round; epuk_otp - 64 + 2048 = starting value for upcoming round

			// Section 1
			va1 = substitutionTable[va1 ^ epuk_otp[0]];
			va2 = va1 ^ substitutionTable[va2 ^ epuk_otp[1]];
			va3 = va2 ^ substitutionTable[va3 ^ epuk_otp[2]];
			va4 = va3 ^ substitutionTable[va4 ^ epuk_otp[3]];
			va5 = va4 ^ substitutionTable[va5 ^ epuk_otp[4]];
			va6 = va5 ^ substitutionTable[va6 ^ epuk_otp[5]];
			va7 = va6 ^ substitutionTable[va7 ^ epuk_otp[6]];
			va8 = va7 ^ substitutionTable[va8 ^ epuk_otp[7]];

			// Section 2
			vb1 = va8 ^ substitutionTable[vb1 ^ epuk_otp[8]];
			vb2 = vb1 ^ substitutionTable[vb2 ^ epuk_otp[9]];
			vb3 = vb2 ^ substitutionTable[vb3 ^ epuk_otp[10]];
			vb4 = vb3 ^ substitutionTable[vb4 ^ epuk_otp[11]];
			vb5 = vb4 ^ substitutionTable[vb5 ^ epuk_otp[12]];
			vb6 = vb5 ^ substitutionTable[vb6 ^ epuk_otp[13]];
			vb7 = vb6 ^ substitutionTable[vb7 ^ epuk_otp[14]];
			vb8 = vb7 ^ substitutionTable[vb8 ^ epuk_otp[15]];

			// Section 3
			vc1 = vb8 ^ substitutionTable[vc1 ^ epuk_otp[16]];
			vc2 = vc1 ^ substitutionTable[vc2 ^ epuk_otp[17]];
			vc3 = vc2 ^ substitutionTable[vc3 ^ epuk_otp[18]];
			vc4 = vc3 ^ substitutionTable[vc4 ^ epuk_otp[19]];
			vc5 = vc4 ^ substitutionTable[vc5 ^ epuk_otp[20]];
			vc6 = vc5 ^ substitutionTable[vc6 ^ epuk_otp[21]];
			vc7 = vc6 ^ substitutionTable[vc7 ^ epuk_otp[22]];
			vc8 = vc7 ^ substitutionTable[vc8 ^ epuk_otp[23]];

			// Section 4
			vd1 = vc8 ^ substitutionTable[vd1 ^ epuk_otp[24]];
			vd2 = vd1 ^ substitutionTable[vd2 ^ epuk_otp[25]];
			vd3 = vd2 ^ substitutionTable[vd3 ^ epuk_otp[26]];
			vd4 = vd3 ^ substitutionTable[vd4 ^ epuk_otp[27]];
			vd5 = vd4 ^ substitutionTable[vd5 ^ epuk_otp[28]];
			vd6 = vd5 ^ substitutionTable[vd6 ^ epuk_otp[29]];
			vd7 = vd6 ^ substitutionTable[vd7 ^ epuk_otp[30]];
			vd8 = vd7 ^ substitutionTable[vd8 ^ epuk_otp[31]];

			// Section 5
			ve1 = vd8 ^ substitutionTable[ve1 ^ epuk_otp[32]];
			ve2 = ve1 ^ substitutionTable[ve2 ^ epuk_otp[33]];
			ve3 = ve2 ^ substitutionTable[ve3 ^ epuk_otp[34]];
			ve4 = ve3 ^ substitutionTable[ve4 ^ epuk_otp[35]];
			ve5 = ve4 ^ substitutionTable[ve5 ^ epuk_otp[36]];
			ve6 = ve5 ^ substitutionTable[ve6 ^ epuk_otp[37]];
			ve7 = ve6 ^ substitutionTable[ve7 ^ epuk_otp[38]];
			ve8 = ve7 ^ substitutionTable[ve8 ^ epuk_otp[39]];

			// Section 6
			vf1 = ve8 ^ substitutionTable[vf1 ^ epuk_otp[40]];
			vf2 = vf1 ^ substitutionTable[vf2 ^ epuk_otp[41]];
			vf3 = vf2 ^ substitutionTable[vf3 ^ epuk_otp[42]];
			vf4 = vf3 ^ substitutionTable[vf4 ^ epuk_otp[43]];
			vf5 = vf4 ^ substitutionTable[vf5 ^ epuk_otp[44]];
			vf6 = vf5 ^ substitutionTable[vf6 ^ epuk_otp[45]];
			vf7 = vf6 ^ substitutionTable[vf7 ^ epuk_otp[46]];
			vf8 = vf7 ^ substitutionTable[vf8 ^ epuk_otp[47]];

			// Section 7
			vg1 = vf8 ^ substitutionTable[vg1 ^ epuk_otp[48]];
			vg2 = vg1 ^ substitutionTable[vg2 ^ epuk_otp[49]];
			vg3 = vg2 ^ substitutionTable[vg3 ^ epuk_otp[50]];
			vg4 = vg3 ^ substitutionTable[vg4 ^ epuk_otp[51]];
			vg5 = vg4 ^ substitutionTable[vg5 ^ epuk_otp[52]];
			vg6 = vg5 ^ substitutionTable[vg6 ^ epuk_otp[53]];
			vg7 = vg6 ^ substitutionTable[vg7 ^ epuk_otp[54]];
			vg8 = vg7 ^ substitutionTable[vg8 ^ epuk_otp[55]];

			// Section 8
			vh1 = vg8 ^ substitutionTable[vh1 ^ epuk_otp[56]];
			vh2 = vh1 ^ substitutionTable[vh2 ^ epuk_otp[57]];
			vh3 = vh2 ^ substitutionTable[vh3 ^ epuk_otp[58]];
			vh4 = vh3 ^ substitutionTable[vh4 ^ epuk_otp[59]];
			vh5 = vh4 ^ substitutionTable[vh5 ^ epuk_otp[60]];
			vh6 = vh5 ^ substitutionTable[vh6 ^ epuk_otp[61]];
			vh7 = vh6 ^ substitutionTable[vh7 ^ epuk_otp[62]];
			vh8 = vh7 ^ substitutionTable[vh8 ^ epuk_otp[63]];

			va1 ^= vh8;
		}

		epuk_otp += 2048;
		*SA(0) = va1 ^ epuk_otp[0];
		*SA(1) = va2 ^ epuk_otp[1];
		*SA(2) = va3 ^ epuk_otp[2];
		*SA(3) = va4 ^ epuk_otp[3];
		*SA(4) = va5 ^ epuk_otp[4];
		*SA(5) = va6 ^ epuk_otp[5];
		*SA(6) = va7 ^ epuk_otp[6];
		*SA(7) = va8 ^ epuk_otp[7];

		*SB(0) = vb1 ^ epuk_otp[8];
		*SB(1) = vb2 ^ epuk_otp[9];
		*SB(2) = vb3 ^ epuk_otp[10];
		*SB(3) = vb4 ^ epuk_otp[11];
		*SB(4) = vb5 ^ epuk_otp[12];
		*SB(5) = vb6 ^ epuk_otp[13];
		*SB(6) = vb7 ^ epuk_otp[14];
		*SB(7) = vb8 ^ epuk_otp[15];

		*SC(0) = vc1 ^ epuk_otp[16];
		*SC(1) = vc2 ^ epuk_otp[17];
		*SC(2) = vc3 ^ epuk_otp[18];
		*SC(3) = vc4 ^ epuk_otp[19];
		*SC(4) = vc5 ^ epuk_otp[20];
		*SC(5) = vc6 ^ epuk_otp[21];
		*SC(6) = vc7 ^ epuk_otp[22];
		//cout << "E: Xored vc7 with " << hex << epuk_otp[22] << " to get " << *SC(6) << dec << endl;
		*SC(7) = vc8 ^ epuk_otp[23];

		*SD(0) = vd1 ^ epuk_otp[24];
		*SD(1) = vd2 ^ epuk_otp[25];
		*SD(2) = vd3 ^ epuk_otp[26];
		*SD(3) = vd4 ^ epuk_otp[27];
		*SD(4) = vd5 ^ epuk_otp[28];
		*SD(5) = vd6 ^ epuk_otp[29];
		*SD(6) = vd7 ^ epuk_otp[30];
		*SD(7) = vd8 ^ epuk_otp[31];

		*SE(0) = ve1 ^ epuk_otp[32];
		*SE(1) = ve2 ^ epuk_otp[33];
		*SE(2) = ve3 ^ epuk_otp[34];
		*SE(3) = ve4 ^ epuk_otp[35];
		*SE(4) = ve5 ^ epuk_otp[36];
		*SE(5) = ve6 ^ epuk_otp[37];
		*SE(6) = ve7 ^ epuk_otp[38];
		*SE(7) = ve8 ^ epuk_otp[39];

		*SF(0) = vf1 ^ epuk_otp[40];
		*SF(1) = vf2 ^ epuk_otp[41];
		*SF(2) = vf3 ^ epuk_otp[42];
		*SF(3) = vf4 ^ epuk_otp[43];
		*SF(4) = vf5 ^ epuk_otp[44];
		*SF(5) = vf6 ^ epuk_otp[45];
		*SF(6) = vf7 ^ epuk_otp[46];
		*SF(7) = vf8 ^ epuk_otp[47];

		*SG(0) = vg1 ^ epuk_otp[48];
		*SG(1) = vg2 ^ epuk_otp[49];
		*SG(2) = vg3 ^ epuk_otp[50];
		*SG(3) = vg4 ^ epuk_otp[51];
		*SG(4) = vg5 ^ epuk_otp[52];
		*SG(5) = vg6 ^ epuk_otp[53];
		*SG(6) = vg7 ^ epuk_otp[54];
		*SG(7) = vg8 ^ epuk_otp[55];

		*SH(0) = vh1 ^ epuk_otp[56];
		*SH(1) = vh2 ^ epuk_otp[57];
		*SH(2) = vh3 ^ epuk_otp[58];
		*SH(3) = vh4 ^ epuk_otp[59];
		*SH(4) = vh5 ^ epuk_otp[60];
		*SH(5) = vh6 ^ epuk_otp[61];
		*SH(6) = vh7 ^ epuk_otp[62];
		*SH(7) = vh8 ^ epuk_otp[63];

		//printVector(input, 56, 65);
	}
}

//void reverseReplaceFoldCyclesCM(vector<uint16_t>& input, uint32_t start, uint32_t end) {
void reverseReplaceFoldCyclesCM(uint16_t* input, uint16_t* epuk_otpp, uint32_t start, uint32_t end) {
//	if (end == 0) end = input.size();

	{
		register uint16_t* bptr1 = input+start;
		register uint16_t* epuk_otp4 = epuk_otpp + start + (2048*cycles);
		register uint16_t* epuk_otp = epuk_otpp + start + (2048*(cycles-1));

		// Process 8th section of 128 bits
		uint16_t vh8 = *SH(7) ^ epuk_otp4[63]; 
		uint16_t va1 = *SA(0) ^ epuk_otp4[0] ^ vh8; 
		//		cout << "D: Xored va1 with " << hex << epuk_otp4[0] << " and " << vh8 <<" to get " << va1 << dec << endl;
		uint16_t vh7 = *SH(6) ^ epuk_otp4[62]; 
		uint16_t vh6 = *SH(5) ^ epuk_otp4[61]; 
		uint16_t vh5 = *SH(4) ^ epuk_otp4[60]; 
		uint16_t vh4 = *SH(3) ^ epuk_otp4[59]; 
		uint16_t vh3 = *SH(2) ^ epuk_otp4[58]; 
		uint16_t vh2 = *SH(1) ^ epuk_otp4[57]; 
		uint16_t vh1 = *SH(0) ^ epuk_otp4[56]; 

		vh8 = reverseSubstitutionTable[vh8 ^ vh7] ^ epuk_otp[63];
		vh7 = reverseSubstitutionTable[vh7 ^ vh6] ^ epuk_otp[62];
		vh6 = reverseSubstitutionTable[vh6 ^ vh5] ^ epuk_otp[61];
		vh5 = reverseSubstitutionTable[vh5 ^ vh4] ^ epuk_otp[60];
		vh4 = reverseSubstitutionTable[vh4 ^ vh3] ^ epuk_otp[59];
		vh3 = reverseSubstitutionTable[vh3 ^ vh2] ^ epuk_otp[58];
		vh2 = reverseSubstitutionTable[vh2 ^ vh1] ^ epuk_otp[57];
		uint16_t vg8 = *SG(7) ^ epuk_otp4[55]; 
		vh1 = reverseSubstitutionTable[vh1 ^ vg8] ^ epuk_otp[56];

		// Process 7th section of 128 bits
		uint16_t vg7 = *SG(6) ^ epuk_otp4[54]; 
		uint16_t vg6 = *SG(5) ^ epuk_otp4[53]; 
		uint16_t vg5 = *SG(4) ^ epuk_otp4[52]; 
		uint16_t vg4 = *SG(3) ^ epuk_otp4[51]; 
		uint16_t vg3 = *SG(2) ^ epuk_otp4[50]; 
		uint16_t vg2 = *SG(1) ^ epuk_otp4[49]; 
		uint16_t vg1 = *SG(0) ^ epuk_otp4[48]; 

		vg8 = reverseSubstitutionTable[vg8 ^ vg7] ^ epuk_otp[55];
		vg7 = reverseSubstitutionTable[vg7 ^ vg6] ^ epuk_otp[54];
		vg6 = reverseSubstitutionTable[vg6 ^ vg5] ^ epuk_otp[53];
		vg5 = reverseSubstitutionTable[vg5 ^ vg4] ^ epuk_otp[52];
		vg4 = reverseSubstitutionTable[vg4 ^ vg3] ^ epuk_otp[51];
		vg3 = reverseSubstitutionTable[vg3 ^ vg2] ^ epuk_otp[50];
		vg2 = reverseSubstitutionTable[vg2 ^ vg1] ^ epuk_otp[49];
		uint16_t vf8 = *SF(7) ^ epuk_otp4[47]; 
		vg1 = reverseSubstitutionTable[vg1 ^ vf8] ^ epuk_otp[48];


		// Process 6th section of 128 bits
		uint16_t vf7 = *SF(6) ^ epuk_otp4[46]; 
		uint16_t vf6 = *SF(5) ^ epuk_otp4[45]; 
		uint16_t vf5 = *SF(4) ^ epuk_otp4[44]; 
		uint16_t vf4 = *SF(3) ^ epuk_otp4[43]; 
		uint16_t vf3 = *SF(2) ^ epuk_otp4[42]; 
		uint16_t vf2 = *SF(1) ^ epuk_otp4[41]; 
		uint16_t vf1 = *SF(0) ^ epuk_otp4[40]; 

		vf8 = reverseSubstitutionTable[vf8 ^ vf7] ^ epuk_otp[47];
		vf7 = reverseSubstitutionTable[vf7 ^ vf6] ^ epuk_otp[46];
		vf6 = reverseSubstitutionTable[vf6 ^ vf5] ^ epuk_otp[45];
		vf5 = reverseSubstitutionTable[vf5 ^ vf4] ^ epuk_otp[44];
		vf4 = reverseSubstitutionTable[vf4 ^ vf3] ^ epuk_otp[43];
		vf3 = reverseSubstitutionTable[vf3 ^ vf2] ^ epuk_otp[42];
		vf2 = reverseSubstitutionTable[vf2 ^ vf1] ^ epuk_otp[41];
		uint16_t ve8 = *SE(7) ^ epuk_otp4[39]; 
		vf1 = reverseSubstitutionTable[vf1 ^ ve8] ^ epuk_otp[40];

		// Process 5th section of 128 bits
		uint16_t ve7 = *SE(6) ^ epuk_otp4[38]; 
		uint16_t ve6 = *SE(5) ^ epuk_otp4[37]; 
		uint16_t ve5 = *SE(4) ^ epuk_otp4[36]; 
		uint16_t ve4 = *SE(3) ^ epuk_otp4[35]; 
		uint16_t ve3 = *SE(2) ^ epuk_otp4[34]; 
		uint16_t ve2 = *SE(1) ^ epuk_otp4[33]; 
		uint16_t ve1 = *SE(0) ^ epuk_otp4[32]; 

		ve8 = reverseSubstitutionTable[ve8 ^ ve7] ^ epuk_otp[39];
		ve7 = reverseSubstitutionTable[ve7 ^ ve6] ^ epuk_otp[38];
		ve6 = reverseSubstitutionTable[ve6 ^ ve5] ^ epuk_otp[37];
		ve5 = reverseSubstitutionTable[ve5 ^ ve4] ^ epuk_otp[36];
		ve4 = reverseSubstitutionTable[ve4 ^ ve3] ^ epuk_otp[35];
		ve3 = reverseSubstitutionTable[ve3 ^ ve2] ^ epuk_otp[34];
		ve2 = reverseSubstitutionTable[ve2 ^ ve1] ^ epuk_otp[33];
		uint16_t vd8 = *SD(7) ^ epuk_otp4[31]; 
		ve1 = reverseSubstitutionTable[ve1 ^ vd8] ^ epuk_otp[32];

		// Process 4th section of 128 bits
		uint16_t vd7 = *SD(6) ^ epuk_otp4[30]; 
		uint16_t vd6 = *SD(5) ^ epuk_otp4[29]; 
		uint16_t vd5 = *SD(4) ^ epuk_otp4[28]; 
		uint16_t vd4 = *SD(3) ^ epuk_otp4[27]; 
		uint16_t vd3 = *SD(2) ^ epuk_otp4[26]; 
		uint16_t vd2 = *SD(1) ^ epuk_otp4[25]; 
		uint16_t vd1 = *SD(0) ^ epuk_otp4[24]; 

		vd8 = reverseSubstitutionTable[vd8 ^ vd7] ^ epuk_otp[31];
		vd7 = reverseSubstitutionTable[vd7 ^ vd6] ^ epuk_otp[30];
		vd6 = reverseSubstitutionTable[vd6 ^ vd5] ^ epuk_otp[29];
		vd5 = reverseSubstitutionTable[vd5 ^ vd4] ^ epuk_otp[28];
		vd4 = reverseSubstitutionTable[vd4 ^ vd3] ^ epuk_otp[27];
		vd3 = reverseSubstitutionTable[vd3 ^ vd2] ^ epuk_otp[26];
		vd2 = reverseSubstitutionTable[vd2 ^ vd1] ^ epuk_otp[25];
		uint16_t vc8 = *SC(7) ^ epuk_otp4[23];
		vd1 = reverseSubstitutionTable[vd1 ^ vc8] ^ epuk_otp[24];

		// Process 3rd section of 128 bits
		uint16_t vc7 = *SC(6) ^ epuk_otp4[22]; 
		uint16_t vc6 = *SC(5) ^ epuk_otp4[21]; 
		uint16_t vc5 = *SC(4) ^ epuk_otp4[20]; 
		uint16_t vc4 = *SC(3) ^ epuk_otp4[19]; 
		uint16_t vc3 = *SC(2) ^ epuk_otp4[18]; 
		uint16_t vc2 = *SC(1) ^ epuk_otp4[17]; 
		uint16_t vc1 = *SC(0) ^ epuk_otp4[16]; 

		vc8 = reverseSubstitutionTable[vc8 ^ vc7] ^ epuk_otp[23];
		//cout << hex << "D: Xored vc7 (" << *SC(6) << ") with " << epuk_otp4[22] << " to get " << vc7 << dec << endl;
		//cout << "Xored vc7 with " << vc6 << " and reverseSubstituted to to " << reverseSubstitutionTable[vc7 ^ vc6] << " and xored with " << epuk_otp[22] << " to get " << dec;
		vc7 = reverseSubstitutionTable[vc7 ^ vc6] ^ epuk_otp[22];
		//cout << hex << vc7 << endl;
		vc6 = reverseSubstitutionTable[vc6 ^ vc5] ^ epuk_otp[21];
		vc5 = reverseSubstitutionTable[vc5 ^ vc4] ^ epuk_otp[20];
		vc4 = reverseSubstitutionTable[vc4 ^ vc3] ^ epuk_otp[19];
		vc3 = reverseSubstitutionTable[vc3 ^ vc2] ^ epuk_otp[18];
		vc2 = reverseSubstitutionTable[vc2 ^ vc1] ^ epuk_otp[17];
		uint16_t vb8 = *SB(7) ^ epuk_otp4[15];
		vc1 = reverseSubstitutionTable[vc1 ^ vb8] ^ epuk_otp[16];

		// Process 2nd section of 128 bits
		uint16_t vb7 = *SB(6) ^ epuk_otp4[14]; 
		uint16_t vb6 = *SB(5) ^ epuk_otp4[13]; 
		uint16_t vb5 = *SB(4) ^ epuk_otp4[12]; 
		uint16_t vb4 = *SB(3) ^ epuk_otp4[11]; 
		uint16_t vb3 = *SB(2) ^ epuk_otp4[10]; 
		uint16_t vb2 = *SB(1) ^ epuk_otp4[9]; 
		uint16_t vb1 = *SB(0) ^ epuk_otp4[8]; 

		vb8 = reverseSubstitutionTable[vb8 ^ vb7] ^ epuk_otp[15];
		vb7 = reverseSubstitutionTable[vb7 ^ vb6] ^ epuk_otp[14];
		vb6 = reverseSubstitutionTable[vb6 ^ vb5] ^ epuk_otp[13];
		vb5 = reverseSubstitutionTable[vb5 ^ vb4] ^ epuk_otp[12];
		vb4 = reverseSubstitutionTable[vb4 ^ vb3] ^ epuk_otp[11];
		vb3 = reverseSubstitutionTable[vb3 ^ vb2] ^ epuk_otp[10];
		vb2 = reverseSubstitutionTable[vb2 ^ vb1] ^ epuk_otp[9];
		uint16_t va8 = *SA(7) ^ epuk_otp4[7];
		vb1 = reverseSubstitutionTable[vb1 ^ va8] ^ epuk_otp[8];

		// Process 1st section of 128 bits
		uint16_t va7 = *SA(6) ^ epuk_otp4[6]; 
		uint16_t va6 = *SA(5) ^ epuk_otp4[5]; 
		uint16_t va5 = *SA(4) ^ epuk_otp4[4]; 
		uint16_t va4 = *SA(3) ^ epuk_otp4[3]; 
		uint16_t va3 = *SA(2) ^ epuk_otp4[2]; 
		uint16_t va2 = *SA(1) ^ epuk_otp4[1]; 

		va8 = reverseSubstitutionTable[va8 ^ va7] ^ epuk_otp[7];
		va7 = reverseSubstitutionTable[va7 ^ va6] ^ epuk_otp[6];
		va6 = reverseSubstitutionTable[va6 ^ va5] ^ epuk_otp[5];
		va5 = reverseSubstitutionTable[va5 ^ va4] ^ epuk_otp[4];
		va4 = reverseSubstitutionTable[va4 ^ va3] ^ epuk_otp[3];
		va3 = reverseSubstitutionTable[va3 ^ va2] ^ epuk_otp[2];
		va2 = reverseSubstitutionTable[va2 ^ va1] ^ epuk_otp[1];
		va1 = reverseSubstitutionTable[va1] ^ epuk_otp[0];
		//cout << hex << va1 << dec << endl;

		for (int i = 1; i < cycles; ++i) {
			epuk_otp -= 2048; // epuk_otp - 64 = starting value previous round; epuk_otp - 64 + 256 = starting value for upcoming round

			// Process 8th section of 128 bits
			va1 ^= vh8; 

			vh8 = reverseSubstitutionTable[vh8 ^ vh7] ^ epuk_otp[63];
			vh7 = reverseSubstitutionTable[vh7 ^ vh6] ^ epuk_otp[62];
			vh6 = reverseSubstitutionTable[vh6 ^ vh5] ^ epuk_otp[61];
			vh5 = reverseSubstitutionTable[vh5 ^ vh4] ^ epuk_otp[60];
			vh4 = reverseSubstitutionTable[vh4 ^ vh3] ^ epuk_otp[59];
			vh3 = reverseSubstitutionTable[vh3 ^ vh2] ^ epuk_otp[58];
			vh2 = reverseSubstitutionTable[vh2 ^ vh1] ^ epuk_otp[57];
			vh1 = reverseSubstitutionTable[vh1 ^ vg8] ^ epuk_otp[56];

			// Process 7th section of 128 bits
			vg8 = reverseSubstitutionTable[vg8 ^ vg7] ^ epuk_otp[55];
			vg7 = reverseSubstitutionTable[vg7 ^ vg6] ^ epuk_otp[54];
			vg6 = reverseSubstitutionTable[vg6 ^ vg5] ^ epuk_otp[53];
			vg5 = reverseSubstitutionTable[vg5 ^ vg4] ^ epuk_otp[52];
			vg4 = reverseSubstitutionTable[vg4 ^ vg3] ^ epuk_otp[51];
			vg3 = reverseSubstitutionTable[vg3 ^ vg2] ^ epuk_otp[50];
			vg2 = reverseSubstitutionTable[vg2 ^ vg1] ^ epuk_otp[49];
			vg1 = reverseSubstitutionTable[vg1 ^ vf8] ^ epuk_otp[48];


			// Process 6th section of 128 bits
			vf8 = reverseSubstitutionTable[vf8 ^ vf7] ^ epuk_otp[47];
			vf7 = reverseSubstitutionTable[vf7 ^ vf6] ^ epuk_otp[46];
			vf6 = reverseSubstitutionTable[vf6 ^ vf5] ^ epuk_otp[45];
			vf5 = reverseSubstitutionTable[vf5 ^ vf4] ^ epuk_otp[44];
			vf4 = reverseSubstitutionTable[vf4 ^ vf3] ^ epuk_otp[43];
			vf3 = reverseSubstitutionTable[vf3 ^ vf2] ^ epuk_otp[42];
			vf2 = reverseSubstitutionTable[vf2 ^ vf1] ^ epuk_otp[41];
			vf1 = reverseSubstitutionTable[vf1 ^ ve8] ^ epuk_otp[40];

			// Process 5th section of 128 bits
			ve8 = reverseSubstitutionTable[ve8 ^ ve7] ^ epuk_otp[39];
			ve7 = reverseSubstitutionTable[ve7 ^ ve6] ^ epuk_otp[38];
			ve6 = reverseSubstitutionTable[ve6 ^ ve5] ^ epuk_otp[37];
			ve5 = reverseSubstitutionTable[ve5 ^ ve4] ^ epuk_otp[36];
			ve4 = reverseSubstitutionTable[ve4 ^ ve3] ^ epuk_otp[35];
			ve3 = reverseSubstitutionTable[ve3 ^ ve2] ^ epuk_otp[34];
			ve2 = reverseSubstitutionTable[ve2 ^ ve1] ^ epuk_otp[33];
			ve1 = reverseSubstitutionTable[ve1 ^ vd8] ^ epuk_otp[32];

			// Process 4th section of 128 bits
			vd8 = reverseSubstitutionTable[vd8 ^ vd7] ^ epuk_otp[31];
			vd7 = reverseSubstitutionTable[vd7 ^ vd6] ^ epuk_otp[30];
			vd6 = reverseSubstitutionTable[vd6 ^ vd5] ^ epuk_otp[29];
			vd5 = reverseSubstitutionTable[vd5 ^ vd4] ^ epuk_otp[28];
			vd4 = reverseSubstitutionTable[vd4 ^ vd3] ^ epuk_otp[27];
			vd3 = reverseSubstitutionTable[vd3 ^ vd2] ^ epuk_otp[26];
			vd2 = reverseSubstitutionTable[vd2 ^ vd1] ^ epuk_otp[25];
			vd1 = reverseSubstitutionTable[vd1 ^ vc8] ^ epuk_otp[24];

			// Process 3rd section of 128 bits
			vc8 = reverseSubstitutionTable[vc8 ^ vc7] ^ epuk_otp[23];
			vc7 = reverseSubstitutionTable[vc7 ^ vc6] ^ epuk_otp[22];
			vc6 = reverseSubstitutionTable[vc6 ^ vc5] ^ epuk_otp[21];
			vc5 = reverseSubstitutionTable[vc5 ^ vc4] ^ epuk_otp[20];
			vc4 = reverseSubstitutionTable[vc4 ^ vc3] ^ epuk_otp[19];
			vc3 = reverseSubstitutionTable[vc3 ^ vc2] ^ epuk_otp[18];
			vc2 = reverseSubstitutionTable[vc2 ^ vc1] ^ epuk_otp[17];
			vc1 = reverseSubstitutionTable[vc1 ^ vb8] ^ epuk_otp[16];

			// Process 2nd section of 128 bits
			vb8 = reverseSubstitutionTable[vb8 ^ vb7] ^ epuk_otp[15];
			vb7 = reverseSubstitutionTable[vb7 ^ vb6] ^ epuk_otp[14];
			vb6 = reverseSubstitutionTable[vb6 ^ vb5] ^ epuk_otp[13];
			vb5 = reverseSubstitutionTable[vb5 ^ vb4] ^ epuk_otp[12];
			vb4 = reverseSubstitutionTable[vb4 ^ vb3] ^ epuk_otp[11];
			vb3 = reverseSubstitutionTable[vb3 ^ vb2] ^ epuk_otp[10];
			vb2 = reverseSubstitutionTable[vb2 ^ vb1] ^ epuk_otp[9];
			vb1 = reverseSubstitutionTable[vb1 ^ va8] ^ epuk_otp[8];

			// Process 1st section of 128 bits
			va8 = reverseSubstitutionTable[va8 ^ va7] ^ epuk_otp[7];
			va7 = reverseSubstitutionTable[va7 ^ va6] ^ epuk_otp[6];
			va6 = reverseSubstitutionTable[va6 ^ va5] ^ epuk_otp[5];
			va5 = reverseSubstitutionTable[va5 ^ va4] ^ epuk_otp[4];
			va4 = reverseSubstitutionTable[va4 ^ va3] ^ epuk_otp[3];
			va3 = reverseSubstitutionTable[va3 ^ va2] ^ epuk_otp[2];
			va2 = reverseSubstitutionTable[va2 ^ va1] ^ epuk_otp[1];
			va1 = reverseSubstitutionTable[va1] ^ epuk_otp[0];

		}

		*SA(0) = va1;
		*SA(1) = va2;
		*SA(2) = va3;
		*SA(3) = va4;
		*SA(4) = va5;
		*SA(5) = va6;
		*SA(6) = va7;
		*SA(7) = va8;

		*SB(0) = vb1;
		*SB(1) = vb2;
		*SB(2) = vb3;
		*SB(3) = vb4;
		*SB(4) = vb5;
		*SB(5) = vb6;
		*SB(6) = vb7;
		*SB(7) = vb8;

		*SC(0) = vc1;
		//cout << "D: " << hex << vc1 << dec << endl;
		*SC(1) = vc2;
		*SC(2) = vc3;
		*SC(3) = vc4;
		*SC(4) = vc5;
		*SC(5) = vc6;
		*SC(6) = vc7;
		*SC(7) = vc8;

		*SD(0) = vd1;
		*SD(1) = vd2;
		*SD(2) = vd3;
		*SD(3) = vd4;
		*SD(4) = vd5;
		*SD(5) = vd6;
		*SD(6) = vd7;
		*SD(7) = vd8;

		*SE(0) = ve1;
		*SE(1) = ve2;
		*SE(2) = ve3;
		*SE(3) = ve4;
		*SE(4) = ve5;
		*SE(5) = ve6;
		*SE(6) = ve7;
		*SE(7) = ve8;

		*SF(0) = vf1;
		*SF(1) = vf2;
		*SF(2) = vf3;
		*SF(3) = vf4;
		*SF(4) = vf5;
		*SF(5) = vf6;
		*SF(6) = vf7;
		*SF(7) = vf8;

		*SG(0) = vg1;
		*SG(1) = vg2;
		*SG(2) = vg3;
		*SG(3) = vg4;
		*SG(4) = vg5;
		*SG(5) = vg6;
		*SG(6) = vg7;
		*SG(7) = vg8;

		*SH(0) = vh1;
		*SH(1) = vh2;
		*SH(2) = vh3;
		*SH(3) = vh4;
		*SH(4) = vh5;
		*SH(5) = vh6;
		*SH(6) = vh7;
		*SH(7) = vh8;

		//cout << "Encrypted " << endl;
		//printVector(input, 56, 65);
	}
}


void initialize() {
	//initializeSubstitutionTable();
	initializeOTPsections();
	initializeSuperRearrangementTable();

	uint32_t size = (k4 / sizeof(uint16_t)); // 4K bytes
	EPUK.resize(size);

	for (uint32_t i = 0; i < size; ++i) {
		EPUK[i] = rand();
	}

	initializeXoredEPUK_OTP();
}

int compareDiffCiphers(vector<uint16_t>& c1, vector<uint16_t>& c2, __m128i& diffOrg, int diffCountOrg) {
	// Calculate the difference between c1 and c2
	__m128i diff;
	for (uint32_t i = 0; i < elmentsIn1024bits; ++i) {
		diff= _mm_mask_set1_epi16(diff, 0xFFFF - i - 1,  abs(c1 - c2));
	}
	// Find out how "less" diff is from diffOrg.
	int diffCount = _mm_cmp_epi16_mask(diff, diffOrg, 1);
	if (diffCount < diffCountOrg) {
		diffOrg = diff;
	}
	return diffCount;
}

int main() {
	initialize();

	vector<uint16_t> original;
	uint32_t size = k4 / (sizeof(uint16_t)); // 4k bytes
	original.resize(size);

	for (uint32_t i = 0; i < size; ++i) {
		original[i] = rand();
	}

	vector<uint16_t> input(original);

	uint32_t start = elementsIn1024bits*3;
	uint32_t OTPsectionCount = 0;
	chrono::duration<double, nano> duration = chrono::duration<double, nano>();

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	{
		uint16_t* dataptr = input.data();
		uint16_t* epukOtpp = xoredEPUK_OTP.data();
		Timer timer("Time taken for forward CM replace and fold : ", duration);
		for (int32_t i = 0; i < 32; i += 1) {
			replaceFoldCyclesCM(dataptr, epukOtpp, i*elementsIn1024bits, (i+1)*elementsIn1024bits);
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	{
		uint16_t* dataptr = input.data();
		uint16_t* epukOtpp = xoredEPUK_OTP.data();
		Timer timer("Time taken for reverse CM replace and fold : ", duration);
		for (int32_t i = 0; i < 32; i += 1) {
			reverseReplaceFoldCyclesCM(dataptr, epukOtpp, i*elementsIn1024bits, (i+1)*elementsIn1024bits);
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << endl << endl;

	uint32_t count = 10000;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	{
		uint16_t* dataptr = input.data();
		uint16_t* epukOtpp = xoredEPUK_OTP.data();
		Timer timer("Time taken to encrypt " + to_string(count) + " times using forward CM replace and fold : ", duration);
		for (int j = 0; j < count; ++j) {
			for (int32_t i = 0; i < 32; i += 1) {
				replaceFoldCyclesCM(dataptr, epukOtpp, i*elementsIn1024bits, (i+1)*elementsIn1024bits);
			}
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	{
		uint16_t* dataptr = input.data();
		uint16_t* epukOtpp = xoredEPUK_OTP.data();
		Timer timer("Time taken to decrypt " + to_string(count) + " times using reverse CM replace and fold : ", duration);
		for (int j = 0; j < count; ++j) {
			for (int32_t i = 0; i < 32; i += 1) {
				reverseReplaceFoldCyclesCM(dataptr, epukOtpp, i*elementsIn1024bits, (i+1)*elementsIn1024bits);
			}
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << endl << endl;


	// Store created cipher texts as well as related info on which cipher texts are the most similar.
	uint32_t compareCiphers = 100;
	vector<uint32_t> similarCipherNumbers;
	vector<vector<uint16_t>> ciphers;
	similarCipherNumbers.resize(compareCiphers);
	ciphers.resize(compareCiphers);

	// Info needed to encryption a portion
	int portionNumber = 0; // Notes which portion from input will be encrypted
	uint16_t* dataptr = input.data();
	uint16_t* epukOtpp = xoredEPUK_OTP.data();

	// Generate cipher texts
	for (uint32_t i = 0; i < compareCiphers; ++i) {
		ciphers[i].resize(elementsIn124bits);

		// Encryption a specific portion of input and store the cipher in ciphers[i]
		replaceFoldCyclesCM(dataptr, epukOtpp portionNumber*elementsIn1024bits, (portionNumber+1)*elementsIn1024bits);

		for (uint32_t j = 0; j < 128 / sizeof(uint16_t); ++j) {
			ciphers[i][j] = dataptr[(portionNumber*elementsIn1024bits)+j];
		}

		// Decrypt input so that it can be reused for the next round.
		reverseReplaceFoldCyclesCM(dataptr, epukOtpp, portionNumber*elementsIn1024bits, (portionNumber+1)*elementsIn1024bits);

		// Increment the specific portion in input by 1
		++(*(dataPtr + ((portionNumber+1)*elementsIn1024bits) - (compareCiphers / 16) - 1]);
	}

	// Traverse through each of the cipher texts and compute which other cipher text is most similar to it.
	for (uint32_t i = 0; i < compareCiphers; ++i) {
		// The following two variales store the difference between ciphers[i] and its current closet cipher
		__m128i diffCipher; 
		int minDiffCount = INT_MAX;
		for (uint32_t j = 0; j < compareCiphers; ++j) {
			if (j == i) {
				continue;
			}
			int newDiffCount = compareDiffCiphers(ciphers[i], ciphers[j], diffCipher, minDiffCount);
			if (newDiffCount < minDiffCount) {
				similarCipherNumbers[i] = j;
			}
		}
	}

	return 0;
}

