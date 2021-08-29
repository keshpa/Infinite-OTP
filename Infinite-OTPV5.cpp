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
#include <boost/preprocessor/arithmetic/mul.hpp>

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
	for (uint32_t i = 0; i < numRounds; ++i) { // works on rounds of xor
		for (uint32_t j = 0; j < 32; ++j) { // words on 4K page
			for (uint32_t m = 0; m < 16; ++m) { // works on a portion
				uint16_t cachedEPUK = EPUK[OTPsectionCount];
				uint16_t cached8ByteSecInd = (EPUK[++OTPsectionCount] & 0x7000) * 4; // 4 shorts in 8 bytes
				for (int k = 0; k < 4; ++k) { // words on 8 bytes
					xoredEPUK_OTP[(i*numShorts) + (k*m) + (j*64)] = EPUK[(k*m) + (j*64)] ^ OTPs[cachedEPUK][cached8ByteSecInd+k];
				}
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

void replaceFoldCycles(vector<uint16_t>& input, uint32_t start, uint32_t end, int numRounds, vector<uint16_t>& EPUK, uint32_t& startOTPSectionCount) {
	if (end == 0) end = input.size();
	//	DASSERT((end - start) == (k1 / (sizeof(uint16_t) * 8))); // input must be 1024 bits long

	//	DASSERT(numRounds == 2 || numRounds == 4);
	//cout << "Encryption input " << endl;
	//printVector(input, 56, 65);

	//	chrono::duration<double, nano> duration;
	int round = 1;
	{

		// Process 1st section of 128 bits
		uint16_t* bptr1 = input.data()+start;

		*SA(0) = substitutionTable[*SA(0)];
		*SA(1) = *SA(0) ^ substitutionTable[*SA(1)];
		*SA(2) = *SA(1) ^ substitutionTable[*SA(2)];
		*SA(3) = *SA(2) ^ substitutionTable[*SA(3)];

		*SA(4) = *SA(3) ^ substitutionTable[*SA(4)];
		*SA(5) = *SA(4) ^ substitutionTable[*SA(5)];
		*SA(6) = *SA(5) ^ substitutionTable[*SA(6)];
		*SA(7) = *SA(6) ^ substitutionTable[*SA(7)];

		*SA(0) ^= *SA(7); //Circular-xor across section


		uint64_t* ukptr1 = reinterpret_cast<uint64_t *>(EPUK.data()+start);

#define EPUK_SHORT(a)	*(reinterpret_cast<uint16_t *>(ukptr1)+a)

		uint16_t cachedEPUK = EPUK[startOTPSectionCount];

		*SA(0) ^= EPUK_SHORT(0) ^ OTPs[cachedEPUK][0];
		*SA(1) ^= EPUK_SHORT(1) ^ OTPs[cachedEPUK][1];
		*SA(2) ^= EPUK_SHORT(2) ^ OTPs[cachedEPUK][2];
		*SA(3) ^= EPUK_SHORT(3) ^ OTPs[cachedEPUK][3];
		*SA(4) ^= EPUK_SHORT(4) ^ OTPs[cachedEPUK][4];
		*SA(5) ^= EPUK_SHORT(5) ^ OTPs[cachedEPUK][5];
		*SA(6) ^= EPUK_SHORT(6) ^ OTPs[cachedEPUK][6];
		*SA(7) ^= EPUK_SHORT(7) ^ OTPs[cachedEPUK][7];


		// Process 2nd section of 128 bits
		*SB(0) = substitutionTable[*SB(0)];
		*SB(1) = *SB(0) ^ substitutionTable[*SB(1)];
		*SB(2) = *SB(1) ^ substitutionTable[*SB(2)];
		*SB(3) = *SB(2) ^ substitutionTable[*SB(3)];

		*SB(4) = *SB(3) ^ substitutionTable[*SB(4)];
		*SB(5) = *SB(4) ^ substitutionTable[*SB(5)];
		*SB(6) = *SB(5) ^ substitutionTable[*SB(6)];
		*SB(7) = *SB(6) ^ substitutionTable[*SB(7)];

		*SB(0) ^= *SB(7); //Circular-xor across section

		*SB(0) ^= EPUK_SHORT(8) ^ OTPs[cachedEPUK][8];
		*SB(1) ^= EPUK_SHORT(9) ^ OTPs[cachedEPUK][9];
		*SB(2) ^= EPUK_SHORT(10) ^ OTPs[cachedEPUK][10];
		*SB(3) ^= EPUK_SHORT(11) ^ OTPs[cachedEPUK][11];
		*SB(4) ^= EPUK_SHORT(12) ^ OTPs[cachedEPUK][12];
		*SB(5) ^= EPUK_SHORT(13) ^ OTPs[cachedEPUK][13];
		*SB(6) ^= EPUK_SHORT(14) ^ OTPs[cachedEPUK][14];
		*SB(7) ^= EPUK_SHORT(15) ^ OTPs[cachedEPUK][15];

		// Process 3rd section of 128 bits
		*SC(0) = substitutionTable[*SC(0)];
		*SC(1) = *SC(0) ^ substitutionTable[*SC(1)];
		*SC(2) = *SC(1) ^ substitutionTable[*SC(2)];
		*SC(3) = *SC(2) ^ substitutionTable[*SC(3)];

		*SC(4) = *SC(3) ^ substitutionTable[*SC(4)];
		*SC(5) = *SC(4) ^ substitutionTable[*SC(5)];
		*SC(6) = *SC(5) ^ substitutionTable[*SC(6)];
		*SC(7) = *SC(6) ^ substitutionTable[*SC(7)];

		*SC(0) ^= *SC(7); //Circular-xor across section

		*SC(0) ^= EPUK_SHORT(16) ^ OTPs[cachedEPUK][16];
		*SC(1) ^= EPUK_SHORT(17) ^ OTPs[cachedEPUK][17];
		*SC(2) ^= EPUK_SHORT(18) ^ OTPs[cachedEPUK][18];
		*SC(3) ^= EPUK_SHORT(19) ^ OTPs[cachedEPUK][19];
		*SC(4) ^= EPUK_SHORT(20) ^ OTPs[cachedEPUK][20];
		*SC(5) ^= EPUK_SHORT(21) ^ OTPs[cachedEPUK][21];
		*SC(6) ^= EPUK_SHORT(22) ^ OTPs[cachedEPUK][22];
		*SC(7) ^= EPUK_SHORT(23) ^ OTPs[cachedEPUK][23];

		// Process 4th section of 128 bits
		*SD(0) = substitutionTable[*SD(0)];
		*SD(1) = *SD(0) ^ substitutionTable[*SD(1)];
		*SD(2) = *SD(1) ^ substitutionTable[*SD(2)];
		*SD(3) = *SD(2) ^ substitutionTable[*SD(3)];

		*SD(4) = *SD(3) ^ substitutionTable[*SD(4)];
		*SD(5) = *SD(4) ^ substitutionTable[*SD(5)];
		*SD(6) = *SD(5) ^ substitutionTable[*SD(6)];
		*SD(7) = *SD(6) ^ substitutionTable[*SD(7)];

		*SD(0) ^= *SD(7); //Circular-xor across section

		*SD(0) ^= EPUK_SHORT(24) ^ OTPs[cachedEPUK][24];
		*SD(1) ^= EPUK_SHORT(25) ^ OTPs[cachedEPUK][25];
		*SD(2) ^= EPUK_SHORT(26) ^ OTPs[cachedEPUK][26];
		*SD(3) ^= EPUK_SHORT(27) ^ OTPs[cachedEPUK][27];
		*SD(4) ^= EPUK_SHORT(28) ^ OTPs[cachedEPUK][28];
		*SD(5) ^= EPUK_SHORT(29) ^ OTPs[cachedEPUK][29];
		*SD(6) ^= EPUK_SHORT(30) ^ OTPs[cachedEPUK][30];
		*SD(7) ^= EPUK_SHORT(31) ^ OTPs[cachedEPUK][31];
		++startOTPSectionCount;
		cachedEPUK = EPUK[startOTPSectionCount];

		// Process 5th section of 128 bits
		*SE(0) = substitutionTable[*SE(0)];
		*SE(1) = *SE(0) ^ substitutionTable[*SE(1)];
		*SE(2) = *SE(1) ^ substitutionTable[*SE(2)];
		*SE(3) = *SE(2) ^ substitutionTable[*SE(3)];


		*SE(4) = *SE(3) ^ substitutionTable[*SE(4)];
		*SE(5) = *SE(4) ^ substitutionTable[*SE(5)];
		*SE(6) = *SE(5) ^ substitutionTable[*SE(6)];
		*SE(7) = *SE(6) ^ substitutionTable[*SE(7)];

		*SE(0) ^= *SE(7); //Circular-xor across section

		*SE(0) ^= EPUK_SHORT(32) ^ OTPs[cachedEPUK][0];
		*SE(1) ^= EPUK_SHORT(33) ^ OTPs[cachedEPUK][1];
		*SE(2) ^= EPUK_SHORT(34) ^ OTPs[cachedEPUK][2];
		*SE(3) ^= EPUK_SHORT(35) ^ OTPs[cachedEPUK][3];
		*SE(4) ^= EPUK_SHORT(36) ^ OTPs[cachedEPUK][4];
		*SE(5) ^= EPUK_SHORT(37) ^ OTPs[cachedEPUK][5];
		*SE(6) ^= EPUK_SHORT(38) ^ OTPs[cachedEPUK][6];
		*SE(7) ^= EPUK_SHORT(39) ^ OTPs[cachedEPUK][7];

		// Process 6th section of 128 bits
		*SF(0) = substitutionTable[*SF(0)];
		*SF(1) = *SF(0) ^ substitutionTable[*SF(1)];
		*SF(2) = *SF(1) ^ substitutionTable[*SF(2)];
		*SF(3) = *SF(2) ^ substitutionTable[*SF(3)];

		*SF(4) = *SF(3) ^ substitutionTable[*SF(4)];
		*SF(5) = *SF(4) ^ substitutionTable[*SF(5)];
		*SF(6) = *SF(5) ^ substitutionTable[*SF(6)];
		*SF(7) = *SF(6) ^ substitutionTable[*SF(7)];

		*SF(0) ^= *SF(7); //Circular-xor across section

		*SF(0) ^= EPUK_SHORT(40) ^ OTPs[cachedEPUK][8];
		*SF(1) ^= EPUK_SHORT(41) ^ OTPs[cachedEPUK][9];
		*SF(2) ^= EPUK_SHORT(42) ^ OTPs[cachedEPUK][10];
		*SF(3) ^= EPUK_SHORT(43) ^ OTPs[cachedEPUK][11];
		*SF(4) ^= EPUK_SHORT(44) ^ OTPs[cachedEPUK][12];
		*SF(5) ^= EPUK_SHORT(45) ^ OTPs[cachedEPUK][13];
		*SF(6) ^= EPUK_SHORT(46) ^ OTPs[cachedEPUK][14];
		*SF(7) ^= EPUK_SHORT(47) ^ OTPs[cachedEPUK][15];

		// Process 7th section of 128 bits
		*SG(0) = substitutionTable[*SG(0)];
		*SG(1) = *SG(0) ^ substitutionTable[*SG(1)];
		*SG(2) = *SG(1) ^ substitutionTable[*SG(2)];
		*SG(3) = *SG(2) ^ substitutionTable[*SG(3)];

		*SG(4) = *SG(3) ^ substitutionTable[*SG(4)];
		*SG(5) = *SG(4) ^ substitutionTable[*SG(5)];
		*SG(6) = *SG(5) ^ substitutionTable[*SG(6)];
		*SG(7) = *SG(6) ^ substitutionTable[*SG(7)];

		*SG(0) ^= *SG(7); //Circular-xor across section

		*SG(0) ^= EPUK_SHORT(48) ^ OTPs[cachedEPUK][16];
		*SG(1) ^= EPUK_SHORT(49) ^ OTPs[cachedEPUK][17];
		*SG(2) ^= EPUK_SHORT(50) ^ OTPs[cachedEPUK][18];
		*SG(3) ^= EPUK_SHORT(51) ^ OTPs[cachedEPUK][19];
		*SG(4) ^= EPUK_SHORT(52) ^ OTPs[cachedEPUK][20];
		*SG(5) ^= EPUK_SHORT(53) ^ OTPs[cachedEPUK][21];
		*SG(6) ^= EPUK_SHORT(54) ^ OTPs[cachedEPUK][22];
		*SG(7) ^= EPUK_SHORT(55) ^ OTPs[cachedEPUK][23];

		// Process 8th section of 128 bits
		*SH(0) = substitutionTable[*SH(0)];
		*SH(1) = *SH(0) ^ substitutionTable[*SH(1)];
		*SH(2) = *SH(1) ^ substitutionTable[*SH(2)];
		*SH(3) = *SH(2) ^ substitutionTable[*SH(3)];

		*SH(4) = *SH(3) ^ substitutionTable[*SH(4)];
		*SH(5) = *SH(4) ^ substitutionTable[*SH(5)];
		*SH(6) = *SH(5) ^ substitutionTable[*SH(6)];
		*SH(7) = *SH(6) ^ substitutionTable[*SH(7)];

		*SH(0) ^= *SH(7); //Circular-xor across section
		//cout << "1ENCRYPTION: circular xor *SH(0) with " << hex << *SH(7) << " to get " << *SH(0) << endl;

		*SH(0) ^= EPUK_SHORT(56) ^ OTPs[cachedEPUK][24];
		*SH(1) ^= EPUK_SHORT(57) ^ OTPs[cachedEPUK][25];
		*SH(2) ^= EPUK_SHORT(58) ^ OTPs[cachedEPUK][26];
		*SH(3) ^= EPUK_SHORT(59) ^ OTPs[cachedEPUK][27];
		*SH(4) ^= EPUK_SHORT(60) ^ OTPs[cachedEPUK][28];
		//cout << "1ENCRYPTION: PUK/OTP xor *SH(4) with " << hex << EPUK_SHORT(60) << " and " << OTPs[cachedEPUK][28] << "to get " << *SH(4) << endl;
		*SH(5) ^= EPUK_SHORT(61) ^ OTPs[cachedEPUK][29];
		//cout << "1ENCRYPTION: PUK/OTP xor *SH(5) with " << hex << EPUK_SHORT(61) << " and " << OTPs[cachedEPUK][29] << "to get " << *SH(5) << endl;
		*SH(6) ^= EPUK_SHORT(62) ^ OTPs[cachedEPUK][30];
		//cout << "1ENCRYPTION: PUK/OTP xor *SH(6) with " << hex << EPUK_SHORT(62) << " and " << OTPs[cachedEPUK][30] << "to get " << *SH(6) << endl;
		*SH(7) ^= EPUK_SHORT(63) ^ OTPs[cachedEPUK][31];
		//cout << "1ENCRYPTION: PUK/OTP xor *SH(7) with " << hex << EPUK_SHORT(63) << " and " << OTPs[cachedEPUK][31] << "to get " << *SH(7) << endl;
		++startOTPSectionCount;
		cachedEPUK = EPUK[startOTPSectionCount];

		for (int i = 0; i < 3; ++i) {
			++round;

			// Section 1
			*SA(0) = substitutionTable[*SA(0)];
			*SA(1) = *SA(0) ^ substitutionTable[*SA(1)];
			*SA(2) = *SA(1) ^ substitutionTable[*SA(2)];
			*SA(3) = *SA(2) ^ substitutionTable[*SA(3)];

			*SA(4) = *SA(3) ^ substitutionTable[*SA(4)];
			*SA(5) = *SA(4) ^ substitutionTable[*SA(5)];
			*SA(6) = *SA(5) ^ substitutionTable[*SA(6)];
			*SA(7) = *SA(6) ^ substitutionTable[*SA(7)];
			*SA(0) ^= *SA(7); //Circular-xor across section

			*SA(0) ^= EPUK_SHORT(0) ^ OTPs[cachedEPUK][0];
			*SA(1) ^= EPUK_SHORT(1) ^ OTPs[cachedEPUK][1];
			*SA(2) ^= EPUK_SHORT(2) ^ OTPs[cachedEPUK][2];
			*SA(3) ^= EPUK_SHORT(3) ^ OTPs[cachedEPUK][3];
			*SA(4) ^= EPUK_SHORT(4) ^ OTPs[cachedEPUK][4];
			*SA(5) ^= EPUK_SHORT(5) ^ OTPs[cachedEPUK][5];
			*SA(6) ^= EPUK_SHORT(6) ^ OTPs[cachedEPUK][6];
			*SA(7) ^= EPUK_SHORT(7) ^ OTPs[cachedEPUK][7];

			// Section 2
			*SB(0) = substitutionTable[*SB(0)];
			*SB(1) = *SB(0) ^ substitutionTable[*SB(1)];
			*SB(2) = *SB(1) ^ substitutionTable[*SB(2)];
			*SB(3) = *SB(2) ^ substitutionTable[*SB(3)];

			*SB(4) = *SB(3) ^ substitutionTable[*SB(4)];
			*SB(5) = *SB(4) ^ substitutionTable[*SB(5)];
			*SB(6) = *SB(5) ^ substitutionTable[*SB(6)];
			*SB(7) = *SB(6) ^ substitutionTable[*SB(7)];
			*SB(0) ^= *SB(7); //Circular-xor across section

			*SB(0) ^= EPUK_SHORT(8) ^ OTPs[cachedEPUK][8];
			*SB(1) ^= EPUK_SHORT(9) ^ OTPs[cachedEPUK][9];
			*SB(2) ^= EPUK_SHORT(10) ^ OTPs[cachedEPUK][10];
			*SB(3) ^= EPUK_SHORT(11) ^ OTPs[cachedEPUK][11];
			*SB(4) ^= EPUK_SHORT(12) ^ OTPs[cachedEPUK][12];
			*SB(5) ^= EPUK_SHORT(13) ^ OTPs[cachedEPUK][13];
			*SB(6) ^= EPUK_SHORT(14) ^ OTPs[cachedEPUK][14];
			*SB(7) ^= EPUK_SHORT(15) ^ OTPs[cachedEPUK][15];

			// Section 3
			*SC(0) =  substitutionTable[*SC(0)];
			*SC(1) = *SC(0) ^ substitutionTable[*SC(1)];
			*SC(2) = *SC(1) ^ substitutionTable[*SC(2)];
			*SC(3) = *SC(2) ^ substitutionTable[*SC(3)];

			*SC(4) = *SC(3) ^ substitutionTable[*SC(4)];
			*SC(5) = *SC(4) ^ substitutionTable[*SC(5)];
			*SC(6) = *SC(5) ^ substitutionTable[*SC(6)];
			*SC(7) = *SC(6) ^ substitutionTable[*SC(7)];
			*SC(0) ^= *SC(7); //Circular-xor across section

			*SC(0) ^= EPUK_SHORT(16) ^ OTPs[cachedEPUK][16];
			*SC(1) ^= EPUK_SHORT(17) ^ OTPs[cachedEPUK][17];
			*SC(2) ^= EPUK_SHORT(18) ^ OTPs[cachedEPUK][18];
			*SC(3) ^= EPUK_SHORT(19) ^ OTPs[cachedEPUK][19];
			*SC(4) ^= EPUK_SHORT(20) ^ OTPs[cachedEPUK][20];
			*SC(5) ^= EPUK_SHORT(21) ^ OTPs[cachedEPUK][21];
			*SC(6) ^= EPUK_SHORT(22) ^ OTPs[cachedEPUK][22];
			*SC(7) ^= EPUK_SHORT(23) ^ OTPs[cachedEPUK][23];

			// Section 4
			*SD(0) = substitutionTable[*SD(0)];
			*SD(1) = *SD(0) ^ substitutionTable[*SD(1)];
			*SD(2) = *SD(1) ^ substitutionTable[*SD(2)];
			*SD(3) = *SD(2) ^ substitutionTable[*SD(3)];

			*SD(4) = *SD(3) ^ substitutionTable[*SD(4)];
			*SD(5) = *SD(4) ^ substitutionTable[*SD(5)];
			*SD(6) = *SD(5) ^ substitutionTable[*SD(6)];
			*SD(7) = *SD(6) ^ substitutionTable[*SD(7)];
			*SD(0) ^= *SD(7); //Circular-xor across section

			*SD(0) ^= EPUK_SHORT(24) ^ OTPs[cachedEPUK][24];
			*SD(1) ^= EPUK_SHORT(25) ^ OTPs[cachedEPUK][25];
			*SD(2) ^= EPUK_SHORT(26) ^ OTPs[cachedEPUK][26];
			*SD(3) ^= EPUK_SHORT(27) ^ OTPs[cachedEPUK][27];
			*SD(4) ^= EPUK_SHORT(28) ^ OTPs[cachedEPUK][28];
			*SD(5) ^= EPUK_SHORT(29) ^ OTPs[cachedEPUK][29];
			*SD(6) ^= EPUK_SHORT(30) ^ OTPs[cachedEPUK][30];
			*SD(7) ^= EPUK_SHORT(31) ^ OTPs[cachedEPUK][31];
			++startOTPSectionCount;
			cachedEPUK = EPUK[startOTPSectionCount];

			// Section 5
			*SE(0) = substitutionTable[*SE(0)];
			*SE(1) = *SE(0) ^ substitutionTable[*SE(1)];
			*SE(2) = *SE(1) ^ substitutionTable[*SE(2)];
			*SE(3) = *SE(2) ^ substitutionTable[*SE(3)];

			*SE(4) = *SE(3) ^ substitutionTable[*SE(4)];
			*SE(5) = *SE(4) ^ substitutionTable[*SE(5)];
			*SE(6) = *SE(5) ^ substitutionTable[*SE(6)];
			*SE(7) = *SE(6) ^ substitutionTable[*SE(7)];
			*SE(0) ^= *SE(7); //Circular-xor across section

			*SE(0) ^= EPUK_SHORT(32) ^ OTPs[cachedEPUK][0];
			*SE(1) ^= EPUK_SHORT(33) ^ OTPs[cachedEPUK][1];
			*SE(2) ^= EPUK_SHORT(34) ^ OTPs[cachedEPUK][2];
			*SE(3) ^= EPUK_SHORT(35) ^ OTPs[cachedEPUK][3];
			*SE(4) ^= EPUK_SHORT(36) ^ OTPs[cachedEPUK][4];
			*SE(5) ^= EPUK_SHORT(37) ^ OTPs[cachedEPUK][5];
			*SE(6) ^= EPUK_SHORT(38) ^ OTPs[cachedEPUK][6];
			*SE(7) ^= EPUK_SHORT(39) ^ OTPs[cachedEPUK][7];

			// Section 6
			*SF(0) = substitutionTable[*SF(0)];
			*SF(1) = *SF(0) ^ substitutionTable[*SF(1)];
			*SF(2) = *SF(1) ^ substitutionTable[*SF(2)];
			*SF(3) = *SF(2) ^ substitutionTable[*SF(3)];

			*SF(4) = *SF(3) ^ substitutionTable[*SF(4)];
			*SF(5) = *SF(4) ^ substitutionTable[*SF(5)];
			*SF(6) = *SF(5) ^ substitutionTable[*SF(6)];
			*SF(7) = *SF(6) ^ substitutionTable[*SF(7)];
			*SF(0) ^= *SF(7); //Circular-xor across section

			*SF(0) ^= EPUK_SHORT(40) ^ OTPs[cachedEPUK][8];
			*SF(1) ^= EPUK_SHORT(41) ^ OTPs[cachedEPUK][9];
			*SF(2) ^= EPUK_SHORT(42) ^ OTPs[cachedEPUK][10];
			*SF(3) ^= EPUK_SHORT(43) ^ OTPs[cachedEPUK][11];
			*SF(4) ^= EPUK_SHORT(44) ^ OTPs[cachedEPUK][12];
			*SF(5) ^= EPUK_SHORT(45) ^ OTPs[cachedEPUK][13];
			*SF(6) ^= EPUK_SHORT(46) ^ OTPs[cachedEPUK][14];
			*SF(7) ^= EPUK_SHORT(47) ^ OTPs[cachedEPUK][15];

			// Section 7
			*SG(0) = substitutionTable[*SG(0)];
			*SG(1) = *SG(0) ^ substitutionTable[*SG(1)];
			*SG(2) = *SG(1) ^ substitutionTable[*SG(2)];
			*SG(3) = *SG(2) ^ substitutionTable[*SG(3)];

			*SG(4) = *SG(3) ^ substitutionTable[*SG(4)];
			*SG(5) = *SG(4) ^ substitutionTable[*SG(5)];
			*SG(6) = *SG(5) ^ substitutionTable[*SG(6)];
			*SG(7) = *SG(6) ^ substitutionTable[*SG(7)];
			*SG(0) ^= *SG(7); //Circular-xor across section

			*SG(0) ^= EPUK_SHORT(48) ^ OTPs[cachedEPUK][16];
			*SG(1) ^= EPUK_SHORT(49) ^ OTPs[cachedEPUK][17];
			*SG(2) ^= EPUK_SHORT(50) ^ OTPs[cachedEPUK][18];
			*SG(3) ^= EPUK_SHORT(51) ^ OTPs[cachedEPUK][19];
			*SG(4) ^= EPUK_SHORT(52) ^ OTPs[cachedEPUK][20];
			*SG(5) ^= EPUK_SHORT(53) ^ OTPs[cachedEPUK][21];
			*SG(6) ^= EPUK_SHORT(54) ^ OTPs[cachedEPUK][22];
			*SG(7) ^= EPUK_SHORT(55) ^ OTPs[cachedEPUK][23];

			// Section 8
			*SH(0) = substitutionTable[*SH(0)];
			*SH(1) = *SH(0) ^ substitutionTable[*SH(1)];
			*SH(2) = *SH(1) ^ substitutionTable[*SH(2)];
			*SH(3) = *SH(2) ^ substitutionTable[*SH(3)];

			*SH(4) = *SH(3) ^ substitutionTable[*SH(4)];
			*SH(5) = *SH(4) ^ substitutionTable[*SH(5)];
			*SH(6) = *SH(5) ^ substitutionTable[*SH(6)];
			*SH(7) = *SH(6) ^ substitutionTable[*SH(7)];
			*SH(0) ^= *SH(7); //Circular-xor across section

			*SH(0) ^= EPUK_SHORT(56) ^ OTPs[cachedEPUK][24];
			*SH(1) ^= EPUK_SHORT(57) ^ OTPs[cachedEPUK][25];
			*SH(2) ^= EPUK_SHORT(58) ^ OTPs[cachedEPUK][26];
			*SH(3) ^= EPUK_SHORT(59) ^ OTPs[cachedEPUK][27];
			*SH(4) ^= EPUK_SHORT(60) ^ OTPs[cachedEPUK][28];
			*SH(5) ^= EPUK_SHORT(61) ^ OTPs[cachedEPUK][29];
			*SH(6) ^= EPUK_SHORT(62) ^ OTPs[cachedEPUK][30];
			*SH(7) ^= EPUK_SHORT(63) ^ OTPs[cachedEPUK][31];
			++startOTPSectionCount;
			cachedEPUK = EPUK[startOTPSectionCount];

			if (round == 2 && numRounds == 4) {
				// Reflect rearrangement in memory
				SWAP_SHORTS(*SA(1), *SB(0));
				SWAP_SHORTS(*SA(2), *SC(0));
				SWAP_SHORTS(*SA(3), *SD(0));
				SWAP_SHORTS(*SA(4), *SE(0));
				SWAP_SHORTS(*SA(5), *SF(0));
				SWAP_SHORTS(*SA(6), *SG(0));
				SWAP_SHORTS(*SA(7), *SH(0));

				SWAP_SHORTS(*SB(2), *SC(1));
				SWAP_SHORTS(*SB(3), *SD(1));
				SWAP_SHORTS(*SB(4), *SE(1));
				SWAP_SHORTS(*SB(5), *SF(1));
				SWAP_SHORTS(*SB(6), *SG(1));
				SWAP_SHORTS(*SB(7), *SH(1));

				SWAP_SHORTS(*SC(3), *SD(2));
				SWAP_SHORTS(*SC(4), *SE(2));
				SWAP_SHORTS(*SC(5), *SF(2));
				SWAP_SHORTS(*SC(6), *SG(2));
				SWAP_SHORTS(*SC(7), *SH(2));

				SWAP_SHORTS(*SD(4), *SE(3));
				SWAP_SHORTS(*SD(5), *SF(3));
				SWAP_SHORTS(*SD(6), *SG(3));
				SWAP_SHORTS(*SD(7), *SH(3));

				SWAP_SHORTS(*SE(5), *SF(4));
				SWAP_SHORTS(*SE(6), *SG(4));
				SWAP_SHORTS(*SE(7), *SH(4));

				SWAP_SHORTS(*SF(6), *SG(5));
				SWAP_SHORTS(*SF(7), *SH(5));

				SWAP_SHORTS(*SG(7), *SH(6));
			}
		}

	}
}

void replaceFoldCycles_old(vector<uint16_t>& input, uint32_t start, uint32_t end, int numRounds, vector<uint16_t>& EPUK, uint32_t& startOTPSectionCount) {
	if (end == 0) end = input.size();
	//	DASSERT((end - start) == (k1 / (sizeof(uint16_t) * 8))); // input must be 1024 bits long

	//	DASSERT(numRounds == 2 || numRounds == 4);
	//cout << "Encryption input " << endl;
	//printVector(input, 56, 65);

	//	chrono::duration<double, nano> duration;
	int round = 1;
	{

		// Process 1st section of 128 bits
		register uint16_t* bptr1 = input.data()+start;

		register uint16_t va1 = substitutionTable[*SA(0)];
		register uint16_t va2 = substitutionTable[*SA(1)];
		register uint16_t va3 = substitutionTable[*SA(2)];
		register uint16_t va4 = substitutionTable[*SA(3)];

		//cout << hex << "Substutited va1 to get new va1 : " << va1 << endl;

		va2 ^= va1;
		va3 ^= va2;
		va4 ^= va3;

		register uint16_t va5 = substitutionTable[*SA(4)];
		register uint16_t va6 = substitutionTable[*SA(5)];
		register uint16_t va7 = substitutionTable[*SA(6)];
		register uint16_t va8 = substitutionTable[*SA(7)];

		va5 ^= va4;
		va6 ^= va5;
		va7 ^= va6;
		va8 ^= va7;
		va1 ^= va8;


		register uint64_t* ukptr1 = reinterpret_cast<uint64_t *>(EPUK.data()+start);

#define EPUK_SHORT(a)	*(reinterpret_cast<uint16_t *>(ukptr1)+a)

		va1 ^= EPUK_SHORT(0) ^ OTPs[EPUK[startOTPSectionCount]][0];
		va2 ^= EPUK_SHORT(1) ^ OTPs[EPUK[startOTPSectionCount]][1];
		va3 ^= EPUK_SHORT(2) ^ OTPs[EPUK[startOTPSectionCount]][2];
		va4 ^= EPUK_SHORT(3) ^ OTPs[EPUK[startOTPSectionCount]][3];
		va5 ^= EPUK_SHORT(4) ^ OTPs[EPUK[startOTPSectionCount]][4];
		va6 ^= EPUK_SHORT(5) ^ OTPs[EPUK[startOTPSectionCount]][5];
		va7 ^= EPUK_SHORT(6) ^ OTPs[EPUK[startOTPSectionCount]][6];
		va8 ^= EPUK_SHORT(7) ^ OTPs[EPUK[startOTPSectionCount]][7];


		// Process 2nd section of 128 bits
		register uint16_t vb1 = substitutionTable[*SB(0)];
		register uint16_t vb2 = substitutionTable[*SB(1)];
		register uint16_t vb3 = substitutionTable[*SB(2)];
		register uint16_t vb4 = substitutionTable[*SB(3)];

		vb2 ^= vb1;
		vb3 ^= vb2;
		vb4 ^= vb3;

		register uint16_t vb5 = substitutionTable[*SB(4)];
		register uint16_t vb6 = substitutionTable[*SB(5)];
		register uint16_t vb7 = substitutionTable[*SB(6)];
		register uint16_t vb8 = substitutionTable[*SB(7)];

		vb5 ^= vb4;
		vb6 ^= vb5;
		vb7 ^= vb6;
		vb8 ^= vb7;
		vb1 ^= vb8; //Circular-xor across section

		vb1 ^= EPUK_SHORT(8) ^ OTPs[EPUK[startOTPSectionCount]][8];
		vb2 ^= EPUK_SHORT(9) ^ OTPs[EPUK[startOTPSectionCount]][9];
		vb3 ^= EPUK_SHORT(10) ^ OTPs[EPUK[startOTPSectionCount]][10];
		vb4 ^= EPUK_SHORT(11) ^ OTPs[EPUK[startOTPSectionCount]][11];
		vb5 ^= EPUK_SHORT(12) ^ OTPs[EPUK[startOTPSectionCount]][12];
		vb6 ^= EPUK_SHORT(13) ^ OTPs[EPUK[startOTPSectionCount]][13];
		vb7 ^= EPUK_SHORT(14) ^ OTPs[EPUK[startOTPSectionCount]][14];
		vb8 ^= EPUK_SHORT(15) ^ OTPs[EPUK[startOTPSectionCount]][15];

		// Process 3rd section of 128 bits
		register uint16_t vc1 = substitutionTable[*SC(0)];
		register uint16_t vc2 = substitutionTable[*SC(1)];
		register uint16_t vc3 = substitutionTable[*SC(2)];
		register uint16_t vc4 = substitutionTable[*SC(3)];

		vc2 ^= vc1;
		vc3 ^= vc2;
		vc4 ^= vc3;

		register uint16_t vc5 = substitutionTable[*SC(4)];
		register uint16_t vc6 = substitutionTable[*SC(5)];
		register uint16_t vc7 = substitutionTable[*SC(6)];
		register uint16_t vc8 = substitutionTable[*SC(7)];

		vc5 ^= vc4;
		vc6 ^= vc5;
		vc7 ^= vc6;
		vc8 ^= vc7;
		vc1 ^= vc8; //Circular-xor across section

		vc1 ^= EPUK_SHORT(16) ^ OTPs[EPUK[startOTPSectionCount]][16];
		vc2 ^= EPUK_SHORT(17) ^ OTPs[EPUK[startOTPSectionCount]][17];
		vc3 ^= EPUK_SHORT(18) ^ OTPs[EPUK[startOTPSectionCount]][18];
		vc4 ^= EPUK_SHORT(19) ^ OTPs[EPUK[startOTPSectionCount]][19];
		vc5 ^= EPUK_SHORT(20) ^ OTPs[EPUK[startOTPSectionCount]][20];
		vc6 ^= EPUK_SHORT(21) ^ OTPs[EPUK[startOTPSectionCount]][21];
		vc7 ^= EPUK_SHORT(22) ^ OTPs[EPUK[startOTPSectionCount]][22];
		vc8 ^= EPUK_SHORT(23) ^ OTPs[EPUK[startOTPSectionCount]][23];

		// Process 4th section of 128 bits
		register uint16_t vd1 = substitutionTable[*SD(0)];
		register uint16_t vd2 = substitutionTable[*SD(1)];
		register uint16_t vd3 = substitutionTable[*SD(2)];
		register uint16_t vd4 = substitutionTable[*SD(3)];

		vd2 ^= vd1;
		vd3 ^= vd2;
		vd4 ^= vd3;

		register uint16_t vd5 = substitutionTable[*SD(4)];
		register uint16_t vd6 = substitutionTable[*SD(5)];
		register uint16_t vd7 = substitutionTable[*SD(6)];
		register uint16_t vd8 = substitutionTable[*SD(7)];

		vd5 ^= vd4;
		vd6 ^= vd5;
		vd7 ^= vd6;
		vd8 ^= vd7;
		vd1 ^= vd8; //Circular-xor across section

		vd1 ^= EPUK_SHORT(24) ^ OTPs[EPUK[startOTPSectionCount]][24];
		vd2 ^= EPUK_SHORT(25) ^ OTPs[EPUK[startOTPSectionCount]][25];
		vd3 ^= EPUK_SHORT(26) ^ OTPs[EPUK[startOTPSectionCount]][26];
		vd4 ^= EPUK_SHORT(27) ^ OTPs[EPUK[startOTPSectionCount]][27];
		vd5 ^= EPUK_SHORT(28) ^ OTPs[EPUK[startOTPSectionCount]][28];
		vd6 ^= EPUK_SHORT(29) ^ OTPs[EPUK[startOTPSectionCount]][29];
		vd7 ^= EPUK_SHORT(30) ^ OTPs[EPUK[startOTPSectionCount]][30];
		vd8 ^= EPUK_SHORT(31) ^ OTPs[EPUK[startOTPSectionCount]][31];
		++startOTPSectionCount;

		// Process 5th section of 128 bits
		register uint16_t ve1 = substitutionTable[*SE(0)];
		register uint16_t ve2 = substitutionTable[*SE(1)];
		register uint16_t ve3 = substitutionTable[*SE(2)];
		register uint16_t ve4 = substitutionTable[*SE(3)];


		ve2 ^= ve1;
		ve3 ^= ve2;
		ve4 ^= ve3;

		register uint16_t ve5 = substitutionTable[*SE(4)];
		register uint16_t ve6 = substitutionTable[*SE(5)];
		register uint16_t ve7 = substitutionTable[*SE(6)];
		register uint16_t ve8 = substitutionTable[*SE(7)];

		ve5 ^= ve4;
		ve6 ^= ve5;
		ve7 ^= ve6;
		ve8 ^= ve7;
		ve1 ^= ve8; //Circular-xor across section

		ve1 ^= EPUK_SHORT(32) ^ OTPs[EPUK[startOTPSectionCount]][0];
		ve2 ^= EPUK_SHORT(33) ^ OTPs[EPUK[startOTPSectionCount]][1];
		ve3 ^= EPUK_SHORT(34) ^ OTPs[EPUK[startOTPSectionCount]][2];
		ve4 ^= EPUK_SHORT(35) ^ OTPs[EPUK[startOTPSectionCount]][3];
		ve5 ^= EPUK_SHORT(36) ^ OTPs[EPUK[startOTPSectionCount]][4];
		ve6 ^= EPUK_SHORT(37) ^ OTPs[EPUK[startOTPSectionCount]][5];
		ve7 ^= EPUK_SHORT(38) ^ OTPs[EPUK[startOTPSectionCount]][6];
		ve8 ^= EPUK_SHORT(39) ^ OTPs[EPUK[startOTPSectionCount]][7];

		// Process 6th section of 128 bits
		register uint16_t vf1 = substitutionTable[*SF(0)];
		register uint16_t vf2 = substitutionTable[*SF(1)];
		register uint16_t vf3 = substitutionTable[*SF(2)];
		register uint16_t vf4 = substitutionTable[*SF(3)];

		vf2 ^= vf1;
		vf3 ^= vf2;
		vf4 ^= vf3;

		register uint16_t vf5 = substitutionTable[*SF(4)];
		register uint16_t vf6 = substitutionTable[*SF(5)];
		register uint16_t vf7 = substitutionTable[*SF(6)];
		register uint16_t vf8 = substitutionTable[*SF(7)];

		vf5 ^= vf4;
		vf6 ^= vf5;
		vf7 ^= vf6;
		vf8 ^= vf7;
		vf1 ^= vf8; //Circular-xor across section

		vf1 ^= EPUK_SHORT(40) ^ OTPs[EPUK[startOTPSectionCount]][8];
		vf2 ^= EPUK_SHORT(41) ^ OTPs[EPUK[startOTPSectionCount]][9];
		vf3 ^= EPUK_SHORT(42) ^ OTPs[EPUK[startOTPSectionCount]][10];
		vf4 ^= EPUK_SHORT(43) ^ OTPs[EPUK[startOTPSectionCount]][11];
		vf5 ^= EPUK_SHORT(44) ^ OTPs[EPUK[startOTPSectionCount]][12];
		vf6 ^= EPUK_SHORT(45) ^ OTPs[EPUK[startOTPSectionCount]][13];
		vf7 ^= EPUK_SHORT(46) ^ OTPs[EPUK[startOTPSectionCount]][14];
		vf8 ^= EPUK_SHORT(47) ^ OTPs[EPUK[startOTPSectionCount]][15];

		// Process 7th section of 128 bits
		register uint16_t vg1 = substitutionTable[*SG(0)];
		register uint16_t vg2 = substitutionTable[*SG(1)];
		register uint16_t vg3 = substitutionTable[*SG(2)];
		register uint16_t vg4 = substitutionTable[*SG(3)];

		vg2 ^= vg1;
		vg3 ^= vg2;
		vg4 ^= vg3;

		register uint16_t vg5 = substitutionTable[*SG(4)];
		register uint16_t vg6 = substitutionTable[*SG(5)];
		register uint16_t vg7 = substitutionTable[*SG(6)];
		register uint16_t vg8 = substitutionTable[*SG(7)];

		vg5 ^= vg4;
		vg6 ^= vg5;
		vg7 ^= vg6;
		vg8 ^= vg7;
		vg1 ^= vg8; //Circular-xor across section

		vg1 ^= EPUK_SHORT(48) ^ OTPs[EPUK[startOTPSectionCount]][16];
		vg2 ^= EPUK_SHORT(49) ^ OTPs[EPUK[startOTPSectionCount]][17];
		vg3 ^= EPUK_SHORT(50) ^ OTPs[EPUK[startOTPSectionCount]][18];
		vg4 ^= EPUK_SHORT(51) ^ OTPs[EPUK[startOTPSectionCount]][19];
		vg5 ^= EPUK_SHORT(52) ^ OTPs[EPUK[startOTPSectionCount]][20];
		vg6 ^= EPUK_SHORT(53) ^ OTPs[EPUK[startOTPSectionCount]][21];
		vg7 ^= EPUK_SHORT(54) ^ OTPs[EPUK[startOTPSectionCount]][22];
		vg8 ^= EPUK_SHORT(55) ^ OTPs[EPUK[startOTPSectionCount]][23];

		// Process 8th section of 128 bits
		register uint16_t vh1 = substitutionTable[*SH(0)];
		register uint16_t vh2 = substitutionTable[*SH(1)];
		register uint16_t vh3 = substitutionTable[*SH(2)];
		register uint16_t vh4 = substitutionTable[*SH(3)];

		vh2 ^= vh1;
		vh3 ^= vh2;
		vh4 ^= vh3;

		register uint16_t vh5 = substitutionTable[*SH(4)];
		//cout << "1ENCRYPTION: starting vh5: " << hex << *SH(5) << " and substituted to: " << vh5 << dec << endl;
		register uint16_t vh6 = substitutionTable[*SH(5)];
		//cout << "1ENCRYPTION: starting vh6: " << hex << *SH(6) << " and substituted to: " << vh6 << dec << endl;
		register uint16_t vh7 = substitutionTable[*SH(6)];
		//cout << "1ENCRYPTION: starting vh7: " << hex << *SH(7) << " and substituted to: " << vh7 << dec << endl;
		register uint16_t vh8 = substitutionTable[*SH(7)];
		//cout << "1ENCRYPTION: starting vh8: " << hex << *SH(8) << " and substituted to: " << vh8 << dec << endl;

		vh5 ^= vh4;
		//cout << "1ENCRYPTION: propogation xor vh5 with " << hex << vh4 << " to get " << vh5 << endl;
		vh6 ^= vh5;
		//cout << "1ENCRYPTION: propagation xor vh6 with " << hex << vh5 << " to get " << vh6 << endl;
		vh7 ^= vh6;
		//cout << "1ENCRYPTION: propagation xor vh7 with " << hex << vh6 << " to get " << vh7 << endl;
		vh8 ^= vh7;
		//cout << "1ENCRYPTION: propagation xor vh8 with " << hex << vh7 << " to get " << vh8 << endl;
		vh1 ^= vh8; //Circular-xor across section
		//cout << "1ENCRYPTION: circular xor vh1 with " << hex << vh8 << " to get " << vh1 << endl;

		vh1 ^= EPUK_SHORT(56) ^ OTPs[EPUK[startOTPSectionCount]][24];
		vh2 ^= EPUK_SHORT(57) ^ OTPs[EPUK[startOTPSectionCount]][25];
		vh3 ^= EPUK_SHORT(58) ^ OTPs[EPUK[startOTPSectionCount]][26];
		vh4 ^= EPUK_SHORT(59) ^ OTPs[EPUK[startOTPSectionCount]][27];
		vh5 ^= EPUK_SHORT(60) ^ OTPs[EPUK[startOTPSectionCount]][28];
		//cout << "1ENCRYPTION: PUK/OTP xor vh5 with " << hex << EPUK_SHORT(60) << " and " << OTPs[EPUK[startOTPSectionCount]][28] << "to get " << vh5 << endl;
		vh6 ^= EPUK_SHORT(61) ^ OTPs[EPUK[startOTPSectionCount]][29];
		//cout << "1ENCRYPTION: PUK/OTP xor vh6 with " << hex << EPUK_SHORT(61) << " and " << OTPs[EPUK[startOTPSectionCount]][29] << "to get " << vh6 << endl;
		vh7 ^= EPUK_SHORT(62) ^ OTPs[EPUK[startOTPSectionCount]][30];
		//cout << "1ENCRYPTION: PUK/OTP xor vh7 with " << hex << EPUK_SHORT(62) << " and " << OTPs[EPUK[startOTPSectionCount]][30] << "to get " << vh7 << endl;
		vh8 ^= EPUK_SHORT(63) ^ OTPs[EPUK[startOTPSectionCount]][31];
		//cout << "1ENCRYPTION: PUK/OTP xor vh8 with " << hex << EPUK_SHORT(63) << " and " << OTPs[EPUK[startOTPSectionCount]][31] << "to get " << vh8 << endl;
		++startOTPSectionCount;

		for (int i = 0; i < 3; ++i) {
			++round;

			// Section 1
			va1 = substitutionTable[va1];
			va2 = substitutionTable[va2];
			va3 = substitutionTable[va3];
			va4 = substitutionTable[va4];
			va2 ^= va1;
			va3 ^= va2;
			va4 ^= va3;
			va5 = substitutionTable[va5];
			va6 = substitutionTable[va6];
			va7 = substitutionTable[va7];
			va8 = substitutionTable[va8];
			va5 ^= va4;
			va6 ^= va5;
			va7 ^= va6;
			va8 ^= va7;
			va1 ^= va8; //Circular-xor across section

			va1 ^= EPUK_SHORT(0) ^ OTPs[EPUK[startOTPSectionCount]][0];
			va2 ^= EPUK_SHORT(1) ^ OTPs[EPUK[startOTPSectionCount]][1];
			va3 ^= EPUK_SHORT(2) ^ OTPs[EPUK[startOTPSectionCount]][2];
			va4 ^= EPUK_SHORT(3) ^ OTPs[EPUK[startOTPSectionCount]][3];
			va5 ^= EPUK_SHORT(4) ^ OTPs[EPUK[startOTPSectionCount]][4];
			va6 ^= EPUK_SHORT(5) ^ OTPs[EPUK[startOTPSectionCount]][5];
			va7 ^= EPUK_SHORT(6) ^ OTPs[EPUK[startOTPSectionCount]][6];
			va8 ^= EPUK_SHORT(7) ^ OTPs[EPUK[startOTPSectionCount]][7];

			// Section 2
			vb1 = substitutionTable[vb1];
			vb2 = substitutionTable[vb2];
			vb3 = substitutionTable[vb3];
			vb4 = substitutionTable[vb4];
			vb2 ^= vb1;
			vb3 ^= vb2;
			vb4 ^= vb3;
			vb5 = substitutionTable[vb5];
			vb6 = substitutionTable[vb6];
			vb7 = substitutionTable[vb7];
			vb8 = substitutionTable[vb8];
			vb5 ^= vb4;
			vb6 ^= vb5;
			vb7 ^= vb6;
			vb8 ^= vb7;
			vb1 ^= vb8; //Circular-xor across section

			vb1 ^= EPUK_SHORT(8) ^ OTPs[EPUK[startOTPSectionCount]][8];
			vb2 ^= EPUK_SHORT(9) ^ OTPs[EPUK[startOTPSectionCount]][9];
			vb3 ^= EPUK_SHORT(10) ^ OTPs[EPUK[startOTPSectionCount]][10];
			vb4 ^= EPUK_SHORT(11) ^ OTPs[EPUK[startOTPSectionCount]][11];
			vb5 ^= EPUK_SHORT(12) ^ OTPs[EPUK[startOTPSectionCount]][12];
			vb6 ^= EPUK_SHORT(13) ^ OTPs[EPUK[startOTPSectionCount]][13];
			vb7 ^= EPUK_SHORT(14) ^ OTPs[EPUK[startOTPSectionCount]][14];
			vb8 ^= EPUK_SHORT(15) ^ OTPs[EPUK[startOTPSectionCount]][15];

			// Section 3
			vc1 = substitutionTable[vc1];
			vc2 = substitutionTable[vc2];
			vc3 = substitutionTable[vc3];
			vc4 = substitutionTable[vc4];
			vc2 ^= vc1;
			vc3 ^= vc2;
			vc4 ^= vc3;
			vc5 = substitutionTable[vc5];
			vc6 = substitutionTable[vc6];
			vc7 = substitutionTable[vc7];
			vc8 = substitutionTable[vc8];
			vc5 ^= vc4;
			vc6 ^= vc5;
			vc7 ^= vc6;
			vc8 ^= vc7;
			vc1 ^= vc8; //Circular-xor across section

			vc1 ^= EPUK_SHORT(16) ^ OTPs[EPUK[startOTPSectionCount]][16];
			vc2 ^= EPUK_SHORT(17) ^ OTPs[EPUK[startOTPSectionCount]][17];
			vc3 ^= EPUK_SHORT(18) ^ OTPs[EPUK[startOTPSectionCount]][18];
			vc4 ^= EPUK_SHORT(19) ^ OTPs[EPUK[startOTPSectionCount]][19];
			vc5 ^= EPUK_SHORT(20) ^ OTPs[EPUK[startOTPSectionCount]][20];
			vc6 ^= EPUK_SHORT(21) ^ OTPs[EPUK[startOTPSectionCount]][21];
			vc7 ^= EPUK_SHORT(22) ^ OTPs[EPUK[startOTPSectionCount]][22];
			vc8 ^= EPUK_SHORT(23) ^ OTPs[EPUK[startOTPSectionCount]][23];

			// Section 4
			vd1 = substitutionTable[vd1];
			vd2 = substitutionTable[vd2];
			vd3 = substitutionTable[vd3];
			vd4 = substitutionTable[vd4];
			vd2 ^= vd1;
			vd3 ^= vd2;
			vd4 ^= vd3;
			vd5 = substitutionTable[vd5];
			vd6 = substitutionTable[vd6];
			vd7 = substitutionTable[vd7];
			vd8 = substitutionTable[vd8];
			vd5 ^= vd4;
			vd6 ^= vd5;
			vd7 ^= vd6;
			vd8 ^= vd7;
			vd1 ^= vd8; //Circular-xor across section

			vd1 ^= EPUK_SHORT(24) ^ OTPs[EPUK[startOTPSectionCount]][24];
			vd2 ^= EPUK_SHORT(25) ^ OTPs[EPUK[startOTPSectionCount]][25];
			vd3 ^= EPUK_SHORT(26) ^ OTPs[EPUK[startOTPSectionCount]][26];
			vd4 ^= EPUK_SHORT(27) ^ OTPs[EPUK[startOTPSectionCount]][27];
			vd5 ^= EPUK_SHORT(28) ^ OTPs[EPUK[startOTPSectionCount]][28];
			vd6 ^= EPUK_SHORT(29) ^ OTPs[EPUK[startOTPSectionCount]][29];
			vd7 ^= EPUK_SHORT(30) ^ OTPs[EPUK[startOTPSectionCount]][30];
			vd8 ^= EPUK_SHORT(31) ^ OTPs[EPUK[startOTPSectionCount]][31];
			++startOTPSectionCount;

			// Section 5
			ve1 = substitutionTable[ve1];
			ve2 = substitutionTable[ve2];
			ve3 = substitutionTable[ve3];
			ve4 = substitutionTable[ve4];
			ve2 ^= ve1;
			ve3 ^= ve2;
			ve4 ^= ve3;
			ve5 = substitutionTable[ve5];
			ve6 = substitutionTable[ve6];
			ve7 = substitutionTable[ve7];
			ve8 = substitutionTable[ve8];
			ve5 ^= ve4;
			ve6 ^= ve5;
			ve7 ^= ve6;
			ve8 ^= ve7;
			ve1 ^= ve8; //Circular-xor across section

			ve1 ^= EPUK_SHORT(32) ^ OTPs[EPUK[startOTPSectionCount]][0];
			ve2 ^= EPUK_SHORT(33) ^ OTPs[EPUK[startOTPSectionCount]][1];
			ve3 ^= EPUK_SHORT(34) ^ OTPs[EPUK[startOTPSectionCount]][2];
			ve4 ^= EPUK_SHORT(35) ^ OTPs[EPUK[startOTPSectionCount]][3];
			ve5 ^= EPUK_SHORT(36) ^ OTPs[EPUK[startOTPSectionCount]][4];
			ve6 ^= EPUK_SHORT(37) ^ OTPs[EPUK[startOTPSectionCount]][5];
			ve7 ^= EPUK_SHORT(38) ^ OTPs[EPUK[startOTPSectionCount]][6];
			ve8 ^= EPUK_SHORT(39) ^ OTPs[EPUK[startOTPSectionCount]][7];

			// Section 6
			vf1 = substitutionTable[vf1];
			vf2 = substitutionTable[vf2];
			vf3 = substitutionTable[vf3];
			vf4 = substitutionTable[vf4];
			vf2 ^= vf1;
			vf3 ^= vf2;
			vf4 ^= vf3;
			vf5 = substitutionTable[vf5];
			vf6 = substitutionTable[vf6];
			vf7 = substitutionTable[vf7];
			vf8 = substitutionTable[vf8];
			vf5 ^= vf4;
			vf6 ^= vf5;
			vf7 ^= vf6;
			vf8 ^= vf7;
			vf1 ^= vf8; //Circular-xor across section

			vf1 ^= EPUK_SHORT(40) ^ OTPs[EPUK[startOTPSectionCount]][8];
			vf2 ^= EPUK_SHORT(41) ^ OTPs[EPUK[startOTPSectionCount]][9];
			vf3 ^= EPUK_SHORT(42) ^ OTPs[EPUK[startOTPSectionCount]][10];
			vf4 ^= EPUK_SHORT(43) ^ OTPs[EPUK[startOTPSectionCount]][11];
			vf5 ^= EPUK_SHORT(44) ^ OTPs[EPUK[startOTPSectionCount]][12];
			vf6 ^= EPUK_SHORT(45) ^ OTPs[EPUK[startOTPSectionCount]][13];
			vf7 ^= EPUK_SHORT(46) ^ OTPs[EPUK[startOTPSectionCount]][14];
			vf8 ^= EPUK_SHORT(47) ^ OTPs[EPUK[startOTPSectionCount]][15];

			// Section 7
			vg1 = substitutionTable[vg1];
			vg2 = substitutionTable[vg2];
			vg3 = substitutionTable[vg3];
			vg4 = substitutionTable[vg4];
			vg2 ^= vg1;
			vg3 ^= vg2;
			vg4 ^= vg3;
			vg5 = substitutionTable[vg5];
			vg6 = substitutionTable[vg6];
			vg7 = substitutionTable[vg7];
			vg8 = substitutionTable[vg8];
			vg5 ^= vg4;
			vg6 ^= vg5;
			vg7 ^= vg6;
			vg8 ^= vg7;
			vg1 ^= vg8; //Circular-xor across section

			vg1 ^= EPUK_SHORT(48) ^ OTPs[EPUK[startOTPSectionCount]][16];
			vg2 ^= EPUK_SHORT(49) ^ OTPs[EPUK[startOTPSectionCount]][17];
			vg3 ^= EPUK_SHORT(50) ^ OTPs[EPUK[startOTPSectionCount]][18];
			vg4 ^= EPUK_SHORT(51) ^ OTPs[EPUK[startOTPSectionCount]][19];
			vg5 ^= EPUK_SHORT(52) ^ OTPs[EPUK[startOTPSectionCount]][20];
			vg6 ^= EPUK_SHORT(53) ^ OTPs[EPUK[startOTPSectionCount]][21];
			vg7 ^= EPUK_SHORT(54) ^ OTPs[EPUK[startOTPSectionCount]][22];
			vg8 ^= EPUK_SHORT(55) ^ OTPs[EPUK[startOTPSectionCount]][23];

			// Section 8
			vh1 = substitutionTable[vh1];
			vh2 = substitutionTable[vh2];
			vh3 = substitutionTable[vh3];
			vh4 = substitutionTable[vh4];
			vh2 ^= vh1;
			vh3 ^= vh2;
			vh4 ^= vh3;
			vh5 = substitutionTable[vh5];
			vh6 = substitutionTable[vh6];
			////cout << "ENCRYPTION: starting vh7 " << hex << vh7 << endl;
			vh7 = substitutionTable[vh7];
			//cout << "ENCRYPTION: substituted vh7 " << hex << vh7 << endl;
			vh8 = substitutionTable[vh8];
			vh5 ^= vh4;
			vh6 ^= vh5;
			vh7 ^= vh6;
			//cout << "ENCRYPTION: xored v7 with " << hex << vh6 << " to get " << vh7 << endl;
			vh8 ^= vh7;
			vh1 ^= vh8; //Circular-xor across section

			vh1 ^= EPUK_SHORT(56) ^ OTPs[EPUK[startOTPSectionCount]][24];
			vh2 ^= EPUK_SHORT(57) ^ OTPs[EPUK[startOTPSectionCount]][25];
			vh3 ^= EPUK_SHORT(58) ^ OTPs[EPUK[startOTPSectionCount]][26];
			vh4 ^= EPUK_SHORT(59) ^ OTPs[EPUK[startOTPSectionCount]][27];
			vh5 ^= EPUK_SHORT(60) ^ OTPs[EPUK[startOTPSectionCount]][28];
			//cout << "2ENCRYPTION: PUK/OTP xor vh5 with " << hex << EPUK_SHORT(60) << " and " << OTPs[EPUK[startOTPSectionCount]][28] << "to get " << vh5 << endl;
			vh6 ^= EPUK_SHORT(61) ^ OTPs[EPUK[startOTPSectionCount]][29];
			//cout << "2ENCRYPTION: PUK/OTP xor vh6 with " << hex << EPUK_SHORT(61) << " and " << OTPs[EPUK[startOTPSectionCount]][29] << "to get " << vh6 << endl;
			vh7 ^= EPUK_SHORT(62) ^ OTPs[EPUK[startOTPSectionCount]][30];
			//cout << "2ENCRYPTION: PUK/OTP xor vh7 with " << hex << EPUK_SHORT(62) << " and " << OTPs[EPUK[startOTPSectionCount]][30] << "to get " << vh7 << endl;
			vh8 ^= EPUK_SHORT(63) ^ OTPs[EPUK[startOTPSectionCount]][31];
			//cout << "2ENCRYPTION: PUK/OTP xor vh8 with " << hex << EPUK_SHORT(63) << " and " << OTPs[EPUK[startOTPSectionCount]][31] << "to get " << vh8 << endl;
			++startOTPSectionCount;

			if (round == 2 && numRounds == 4) {
				// Reflect rearrangement in memory
				va1 = va1;
				SWAP_SHORTS(va2, vb1);
				SWAP_SHORTS(va3, vc1);
				SWAP_SHORTS(va4, vd1);
				SWAP_SHORTS(va5, ve1);
				SWAP_SHORTS(va6, vf1);
				SWAP_SHORTS(va7, vg1);
				SWAP_SHORTS(va8, vh1);

				vb2 = vb2;
				SWAP_SHORTS(vb3, vc2);
				SWAP_SHORTS(vb4, vd2);
				SWAP_SHORTS(vb5, ve2);
				SWAP_SHORTS(vb6, vf2);
				SWAP_SHORTS(vb7, vg2);
				SWAP_SHORTS(vb8, vh2);

				vc3 = vc3;
				SWAP_SHORTS(vc4, vd3);
				SWAP_SHORTS(vc5, ve3);
				SWAP_SHORTS(vc6, vf3);
				SWAP_SHORTS(vc7, vg3);
				SWAP_SHORTS(vc8, vh3);

				vd4 = vd4;
				SWAP_SHORTS(vd5, ve4);
				SWAP_SHORTS(vd6, vf4);
				SWAP_SHORTS(vd7, vg4);
				SWAP_SHORTS(vd8, vh4);

				ve5 = ve5;
				SWAP_SHORTS(ve6, vf5);
				SWAP_SHORTS(ve7, vg5);
				SWAP_SHORTS(ve8, vh5);

				vf6 = vf6;
				SWAP_SHORTS(vf7, vg6);
				SWAP_SHORTS(vf8, vh6);

				vg7 = vg7;
				SWAP_SHORTS(vg8, vh7);

				vh8 = vh8;
			}
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

void reverseReplaceFoldCycles(vector<uint16_t>& input, uint32_t start, uint32_t end, int numRounds, vector<uint16_t>& EPUK, uint32_t& startingOTPSection) {
	DASSERT((end - start) == (k1 / (sizeof(uint16_t) * 8))); // input must be 1024 bits long

	DASSERT(numRounds == 2 || numRounds == 4);

	//cout << "Decryption input " << endl;
	//printVector(input, 56, 65);

	uint32_t startOTPSectionCount = 0;
	int round = 1;
	{


		startOTPSectionCount = startingOTPSection - (2*round);
		// Process 1st section of 128 bits
		uint16_t* bptr1 = input.data()+start;

		uint16_t va1 = *SA(0);
		uint16_t va2 = *SA(1);
		uint16_t va3 = *SA(2);
		uint16_t va4 = *SA(3);

		//cout << hex << "Starting va1 : " << va1;

		uint64_t* ukptr1 = reinterpret_cast<uint64_t *>(EPUK.data()+start);

#define EPUK_SHORT(a)	*(reinterpret_cast<uint16_t *>(ukptr1)+a)

		va1 ^= EPUK_SHORT(0) ^ OTPs[EPUK[startOTPSectionCount]][0];
		va2 ^= EPUK_SHORT(1) ^ OTPs[EPUK[startOTPSectionCount]][1];
		va3 ^= EPUK_SHORT(2) ^ OTPs[EPUK[startOTPSectionCount]][2];
		va4 ^= EPUK_SHORT(3) ^ OTPs[EPUK[startOTPSectionCount]][3];

		//cout << hex << "Xor va1 with EPUK " << EPUK_SHORT(0) << " and OTP " << OTPs[EPUK[startOTPSectionCount]][0]  << " to get new va1 : " << va1 << endl;
		uint16_t va5 = *SA(4);
		uint16_t va6 = *SA(5);
		uint16_t va7 = *SA(6);
		uint16_t va8 = *SA(7);

		va5 ^= EPUK_SHORT(4) ^ OTPs[EPUK[startOTPSectionCount]][4];
		va6 ^= EPUK_SHORT(5) ^ OTPs[EPUK[startOTPSectionCount]][5];
		va7 ^= EPUK_SHORT(6) ^ OTPs[EPUK[startOTPSectionCount]][6];
		va8 ^= EPUK_SHORT(7) ^ OTPs[EPUK[startOTPSectionCount]][7];

		va1 ^= va8; //Circular-xor across section
		va8 ^= va7;
		va7 ^= va6;
		va6 ^= va5;
		va5 ^= va4;

		//cout << hex << "Circular xor va1 with " << va8 << " to get new va1 : " << va1  << va1;

		va4 ^= va3;
		va3 ^= va2;
		va2 ^= va1;

		va1 = reverseSubstitutionTable[va1];
		va2 = reverseSubstitutionTable[va2];
		va3 = reverseSubstitutionTable[va3];
		va4 = reverseSubstitutionTable[va4];

		//cout << hex << "Substitution to get new va1 : " << va1  << va1;

		va5 = reverseSubstitutionTable[va5];
		va6 = reverseSubstitutionTable[va6];
		va7 = reverseSubstitutionTable[va7];
		va8 = reverseSubstitutionTable[va8];

		// Process 2nd section of 128 bits
		uint16_t vb1 = *SB(0);
		uint16_t vb2 = *SB(1);
		uint16_t vb3 = *SB(2);
		uint16_t vb4 = *SB(3);

		vb1 ^= EPUK_SHORT(8) ^ OTPs[EPUK[startOTPSectionCount]][8];
		vb2 ^= EPUK_SHORT(9) ^ OTPs[EPUK[startOTPSectionCount]][9];
		vb3 ^= EPUK_SHORT(10) ^ OTPs[EPUK[startOTPSectionCount]][10];
		vb4 ^= EPUK_SHORT(11) ^ OTPs[EPUK[startOTPSectionCount]][11];

		uint16_t vb5 = *SB(4);
		uint16_t vb6 = *SB(5);
		uint16_t vb7 = *SB(6);
		uint16_t vb8 = *SB(7);

		vb5 ^= EPUK_SHORT(12) ^ OTPs[EPUK[startOTPSectionCount]][12];
		vb6 ^= EPUK_SHORT(13) ^ OTPs[EPUK[startOTPSectionCount]][13];
		vb7 ^= EPUK_SHORT(14) ^ OTPs[EPUK[startOTPSectionCount]][14];
		vb8 ^= EPUK_SHORT(15) ^ OTPs[EPUK[startOTPSectionCount]][15];

		vb1 ^= vb8; //Circular-xor across section
		vb8 ^= vb7;
		vb7 ^= vb6;
		vb6 ^= vb5;
		vb5 ^= vb4;

		vb4 ^= vb3;
		vb3 ^= vb2;
		vb2 ^= vb1;

		vb1 = reverseSubstitutionTable[vb1];
		vb2 = reverseSubstitutionTable[vb2];
		vb3 = reverseSubstitutionTable[vb3];
		vb4 = reverseSubstitutionTable[vb4];

		vb5 = reverseSubstitutionTable[vb5];
		vb6 = reverseSubstitutionTable[vb6];
		vb7 = reverseSubstitutionTable[vb7];
		vb8 = reverseSubstitutionTable[vb8];

		// Process 3rd section of 128 bits
		uint16_t vc1 = *SC(0);
		uint16_t vc2 = *SC(1);
		uint16_t vc3 = *SC(2);
		uint16_t vc4 = *SC(3);

		vc1 ^= EPUK_SHORT(16) ^ OTPs[EPUK[startOTPSectionCount]][16];
		vc2 ^= EPUK_SHORT(17) ^ OTPs[EPUK[startOTPSectionCount]][17];
		vc3 ^= EPUK_SHORT(18) ^ OTPs[EPUK[startOTPSectionCount]][18];
		vc4 ^= EPUK_SHORT(19) ^ OTPs[EPUK[startOTPSectionCount]][19];

		uint16_t vc5 = *SC(4);
		uint16_t vc6 = *SC(5);
		uint16_t vc7 = *SC(6);
		uint16_t vc8 = *SC(7);

		vc5 ^= EPUK_SHORT(20) ^ OTPs[EPUK[startOTPSectionCount]][20];
		vc6 ^= EPUK_SHORT(21) ^ OTPs[EPUK[startOTPSectionCount]][21];
		vc7 ^= EPUK_SHORT(22) ^ OTPs[EPUK[startOTPSectionCount]][22];
		vc8 ^= EPUK_SHORT(23) ^ OTPs[EPUK[startOTPSectionCount]][23];

		vc1 ^= vc8; //Circular-xor across section
		vc8 ^= vc7;
		vc7 ^= vc6;
		vc6 ^= vc5;
		vc5 ^= vc4;

		vc4 ^= vc3;
		vc3 ^= vc2;
		vc2 ^= vc1;

		vc1 = reverseSubstitutionTable[vc1];
		vc2 = reverseSubstitutionTable[vc2];
		vc3 = reverseSubstitutionTable[vc3];
		vc4 = reverseSubstitutionTable[vc4];

		vc5 = reverseSubstitutionTable[vc5];
		vc6 = reverseSubstitutionTable[vc6];
		vc7 = reverseSubstitutionTable[vc7];
		vc8 = reverseSubstitutionTable[vc8];


		// Process 4th section of 128 bits
		uint16_t vd1 = *SD(0);
		uint16_t vd2 = *SD(1);
		uint16_t vd3 = *SD(2);
		uint16_t vd4 = *SD(3);

		vd1 ^= EPUK_SHORT(24) ^ OTPs[EPUK[startOTPSectionCount]][24];
		vd2 ^= EPUK_SHORT(25) ^ OTPs[EPUK[startOTPSectionCount]][25];
		vd3 ^= EPUK_SHORT(26) ^ OTPs[EPUK[startOTPSectionCount]][26];
		vd4 ^= EPUK_SHORT(27) ^ OTPs[EPUK[startOTPSectionCount]][27];

		uint16_t vd5 = *SD(4);
		uint16_t vd6 = *SD(5);
		uint16_t vd7 = *SD(6);
		uint16_t vd8 = *SD(7);

		vd5 ^= EPUK_SHORT(28) ^ OTPs[EPUK[startOTPSectionCount]][28];
		vd6 ^= EPUK_SHORT(29) ^ OTPs[EPUK[startOTPSectionCount]][29];
		vd7 ^= EPUK_SHORT(30) ^ OTPs[EPUK[startOTPSectionCount]][30];
		vd8 ^= EPUK_SHORT(31) ^ OTPs[EPUK[startOTPSectionCount]][31];
		++startOTPSectionCount;

		vd1 ^= vd8; //Circular-xor across section
		vd8 ^= vd7;
		vd7 ^= vd6;
		vd6 ^= vd5;
		vd5 ^= vd4;

		vd4 ^= vd3;
		vd3 ^= vd2;
		vd2 ^= vd1;

		vd1 = reverseSubstitutionTable[vd1];
		vd2 = reverseSubstitutionTable[vd2];
		vd3 = reverseSubstitutionTable[vd3];
		vd4 = reverseSubstitutionTable[vd4];

		vd5 = reverseSubstitutionTable[vd5];
		vd6 = reverseSubstitutionTable[vd6];
		vd7 = reverseSubstitutionTable[vd7];
		vd8 = reverseSubstitutionTable[vd8];

		// Process 5th section of 128 bits
		uint16_t ve1 = *SE(0);
		uint16_t ve2 = *SE(1);
		uint16_t ve3 = *SE(2);
		uint16_t ve4 = *SE(3);

		ve1 ^= EPUK_SHORT(32) ^ OTPs[EPUK[startOTPSectionCount]][0];
		ve2 ^= EPUK_SHORT(33) ^ OTPs[EPUK[startOTPSectionCount]][1];
		ve3 ^= EPUK_SHORT(34) ^ OTPs[EPUK[startOTPSectionCount]][2];
		ve4 ^= EPUK_SHORT(35) ^ OTPs[EPUK[startOTPSectionCount]][3];

		uint16_t ve5 = *SE(4);
		uint16_t ve6 = *SE(5);
		uint16_t ve7 = *SE(6);
		uint16_t ve8 = *SE(7);

		ve5 ^= EPUK_SHORT(36) ^ OTPs[EPUK[startOTPSectionCount]][4];
		ve6 ^= EPUK_SHORT(37) ^ OTPs[EPUK[startOTPSectionCount]][5];
		ve7 ^= EPUK_SHORT(38) ^ OTPs[EPUK[startOTPSectionCount]][6];
		ve8 ^= EPUK_SHORT(39) ^ OTPs[EPUK[startOTPSectionCount]][7];

		ve1 ^= ve8; //Circular-xor across section
		ve8 ^= ve7;
		ve7 ^= ve6;
		ve6 ^= ve5;
		ve5 ^= ve4;

		ve4 ^= ve3;
		ve3 ^= ve2;
		ve2 ^= ve1;

		ve1 = reverseSubstitutionTable[ve1];
		ve2 = reverseSubstitutionTable[ve2];
		ve3 = reverseSubstitutionTable[ve3];
		ve4 = reverseSubstitutionTable[ve4];

		ve5 = reverseSubstitutionTable[ve5];
		ve6 = reverseSubstitutionTable[ve6];
		ve7 = reverseSubstitutionTable[ve7];
		ve8 = reverseSubstitutionTable[ve8];

		// Process 6th section of 128 bits
		uint16_t vf1 = *SF(0);
		uint16_t vf2 = *SF(1);
		uint16_t vf3 = *SF(2);
		uint16_t vf4 = *SF(3);

		vf1 ^= EPUK_SHORT(40) ^ OTPs[EPUK[startOTPSectionCount]][8];
		vf2 ^= EPUK_SHORT(41) ^ OTPs[EPUK[startOTPSectionCount]][9];
		vf3 ^= EPUK_SHORT(42) ^ OTPs[EPUK[startOTPSectionCount]][10];
		vf4 ^= EPUK_SHORT(43) ^ OTPs[EPUK[startOTPSectionCount]][11];

		uint16_t vf5 = *SF(4);
		uint16_t vf6 = *SF(5);
		uint16_t vf7 = *SF(6);
		uint16_t vf8 = *SF(7);

		vf5 ^= EPUK_SHORT(44) ^ OTPs[EPUK[startOTPSectionCount]][12];
		vf6 ^= EPUK_SHORT(45) ^ OTPs[EPUK[startOTPSectionCount]][13];
		vf7 ^= EPUK_SHORT(46) ^ OTPs[EPUK[startOTPSectionCount]][14];
		vf8 ^= EPUK_SHORT(47) ^ OTPs[EPUK[startOTPSectionCount]][15];

		vf1 ^= vf8; //Circular-xor across section
		vf8 ^= vf7;
		vf7 ^= vf6;
		vf6 ^= vf5;
		vf5 ^= vf4;

		vf4 ^= vf3;
		vf3 ^= vf2;
		vf2 ^= vf1;

		vf1 = reverseSubstitutionTable[vf1];
		vf2 = reverseSubstitutionTable[vf2];
		vf3 = reverseSubstitutionTable[vf3];
		vf4 = reverseSubstitutionTable[vf4];

		vf5 = reverseSubstitutionTable[vf5];
		vf6 = reverseSubstitutionTable[vf6];
		vf7 = reverseSubstitutionTable[vf7];
		vf8 = reverseSubstitutionTable[vf8];

		// Process 7th section of 128 bits
		uint16_t vg1 = *SG(0);
		uint16_t vg2 = *SG(1);
		uint16_t vg3 = *SG(2);
		uint16_t vg4 = *SG(3);

		vg1 ^= EPUK_SHORT(48) ^ OTPs[EPUK[startOTPSectionCount]][16];
		vg2 ^= EPUK_SHORT(49) ^ OTPs[EPUK[startOTPSectionCount]][17];
		vg3 ^= EPUK_SHORT(50) ^ OTPs[EPUK[startOTPSectionCount]][18];
		vg4 ^= EPUK_SHORT(51) ^ OTPs[EPUK[startOTPSectionCount]][19];

		uint16_t vg5 = *SG(4);
		uint16_t vg6 = *SG(5);
		uint16_t vg7 = *SG(6);
		uint16_t vg8 = *SG(7);

		vg5 ^= EPUK_SHORT(52) ^ OTPs[EPUK[startOTPSectionCount]][20];
		vg6 ^= EPUK_SHORT(53) ^ OTPs[EPUK[startOTPSectionCount]][21];
		vg7 ^= EPUK_SHORT(54) ^ OTPs[EPUK[startOTPSectionCount]][22];
		vg8 ^= EPUK_SHORT(55) ^ OTPs[EPUK[startOTPSectionCount]][23];

		vg1 ^= vg8; //Circular-xor across section
		vg8 ^= vg7;
		vg7 ^= vg6;
		vg6 ^= vg5;
		vg5 ^= vg4;

		vg4 ^= vg3;
		vg3 ^= vg2;
		vg2 ^= vg1;

		vg1 = reverseSubstitutionTable[vg1];
		vg2 = reverseSubstitutionTable[vg2];
		vg3 = reverseSubstitutionTable[vg3];
		vg4 = reverseSubstitutionTable[vg4];

		vg5 = reverseSubstitutionTable[vg5];
		vg6 = reverseSubstitutionTable[vg6];
		vg7 = reverseSubstitutionTable[vg7];
		vg8 = reverseSubstitutionTable[vg8];

		// Process 8th section of 128 bits
		uint16_t vh1 = *SH(0);
		uint16_t vh2 = *SH(1);
		uint16_t vh3 = *SH(2);
		uint16_t vh4 = *SH(3);

		vh1 ^= EPUK_SHORT(56) ^ OTPs[EPUK[startOTPSectionCount]][24];
		vh2 ^= EPUK_SHORT(57) ^ OTPs[EPUK[startOTPSectionCount]][25];
		vh3 ^= EPUK_SHORT(58) ^ OTPs[EPUK[startOTPSectionCount]][26];
		vh4 ^= EPUK_SHORT(59) ^ OTPs[EPUK[startOTPSectionCount]][27];

		uint16_t vh5 = *SH(4);
		uint16_t vh6 = *SH(5);
		uint16_t vh7 = *SH(6);
		uint16_t vh8 = *SH(7);

		//cout << "1DECRYPTION: starting vh5 : " << hex << vh5 << endl;
		//cout << "1DECRYPTION: starting vh6 : " << hex << vh6 << endl;
		//cout << "1DECRYPTION: starting vh7 : " << hex << vh7 << endl;
		//cout << "1DECRYPTION: starting vh8 : " << hex << vh8 << endl;

		vh5 ^= EPUK_SHORT(60) ^ OTPs[EPUK[startOTPSectionCount]][28];
		//cout << "1DECRYPTION: xored vh5 with " << hex << EPUK_SHORT(60) << " and " << OTPs[EPUK[startOTPSectionCount]][28] << " to get " << vh5 << endl;
		vh6 ^= EPUK_SHORT(61) ^ OTPs[EPUK[startOTPSectionCount]][29];
		//cout << "1DECRYPTION: xored vh6 with " << hex << EPUK_SHORT(61) << " and " << OTPs[EPUK[startOTPSectionCount]][29] << " to get " << vh6 << endl;
		vh7 ^= EPUK_SHORT(62) ^ OTPs[EPUK[startOTPSectionCount]][30];
		//cout << "1DECRYPTION: xored vh7 with " << hex << EPUK_SHORT(62) << " and " << OTPs[EPUK[startOTPSectionCount]][30] << " to get " << vh7 << endl;
		vh8 ^= EPUK_SHORT(63) ^ OTPs[EPUK[startOTPSectionCount]][31];
		//cout << "1DECRYPTION: xored vh8 with " << hex << EPUK_SHORT(63) << " and " << OTPs[EPUK[startOTPSectionCount]][31] << " to get " << vh8 << endl;

		vh1 ^= vh8; //Circular-xor across section
		vh8 ^= vh7;
		//cout << "1DECRYPTION: xored vh8 with vh7 (" << hex << vh7 << ") to get : " << vh8 << endl;
		vh7 ^= vh6;
		//cout << "1DECRYPTION: xored vh7 with vh6 (" << hex << vh6 << ") to get : " << vh7 << endl;
		vh6 ^= vh5;
		//cout << "1DECRYPTION: xored vh6 with vh5 (" << hex << vh5 << ") to get : " << vh6 << endl;
		vh5 ^= vh4;
		//cout << "1DECRYPTION: xored vh5 with vh4 (" << hex << vh4 << ") to get : " << vh5 << endl;

		vh4 ^= vh3;
		vh3 ^= vh2;
		vh2 ^= vh1;

		vh1 = reverseSubstitutionTable[vh1];
		vh2 = reverseSubstitutionTable[vh2];
		vh3 = reverseSubstitutionTable[vh3];
		vh4 = reverseSubstitutionTable[vh4];

		vh5 = reverseSubstitutionTable[vh5];
		//cout << "1DECRYPTION: reverseSubstituted vh5 to get final vh5 : " << hex << vh5 << endl;
		vh6 = reverseSubstitutionTable[vh6];
		//cout << "1DECRYPTION: reverseSubstituted vh7 to get final vh6 : " << hex << vh6 << endl;
		vh7 = reverseSubstitutionTable[vh7];
		//cout << "1DECRYPTION: reverseSubstituted vh7 to get final vh7 : " << hex << vh7 << endl;
		vh8 = reverseSubstitutionTable[vh8];
		//cout << "1DECRYPTION: reverseSubstituted vh8 to get final vh8 : " << hex << vh8 << endl;

		for (int i = 0; i < 3; ++i) {

			++round;
			startOTPSectionCount = startingOTPSection - (2*round);
			// Section 1
			va1 ^= EPUK_SHORT(0) ^ OTPs[EPUK[startOTPSectionCount]][0];
			va2 ^= EPUK_SHORT(1) ^ OTPs[EPUK[startOTPSectionCount]][1];
			va3 ^= EPUK_SHORT(2) ^ OTPs[EPUK[startOTPSectionCount]][2];
			va4 ^= EPUK_SHORT(3) ^ OTPs[EPUK[startOTPSectionCount]][3];
			va5 ^= EPUK_SHORT(4) ^ OTPs[EPUK[startOTPSectionCount]][4];
			va6 ^= EPUK_SHORT(5) ^ OTPs[EPUK[startOTPSectionCount]][5];
			va7 ^= EPUK_SHORT(6) ^ OTPs[EPUK[startOTPSectionCount]][6];
			va8 ^= EPUK_SHORT(7) ^ OTPs[EPUK[startOTPSectionCount]][7];

			va1 ^= va8; //Circular-xor across section
			va8 ^= va7;
			va7 ^= va6;
			va6 ^= va5;
			va5 ^= va4;
			va4 ^= va3;
			va3 ^= va2;
			va2 ^= va1;

			va1 = reverseSubstitutionTable[va1];
			va2 = reverseSubstitutionTable[va2];
			va3 = reverseSubstitutionTable[va3];
			va4 = reverseSubstitutionTable[va4];
			va5 = reverseSubstitutionTable[va5];
			va6 = reverseSubstitutionTable[va6];
			va7 = reverseSubstitutionTable[va7];
			va8 = reverseSubstitutionTable[va8];

			// Section 2
			vb1 ^= EPUK_SHORT(8) ^ OTPs[EPUK[startOTPSectionCount]][8];
			vb2 ^= EPUK_SHORT(9) ^ OTPs[EPUK[startOTPSectionCount]][9];
			vb3 ^= EPUK_SHORT(10) ^ OTPs[EPUK[startOTPSectionCount]][10];
			vb4 ^= EPUK_SHORT(11) ^ OTPs[EPUK[startOTPSectionCount]][11];
			vb5 ^= EPUK_SHORT(12) ^ OTPs[EPUK[startOTPSectionCount]][12];
			vb6 ^= EPUK_SHORT(13) ^ OTPs[EPUK[startOTPSectionCount]][13];
			vb7 ^= EPUK_SHORT(14) ^ OTPs[EPUK[startOTPSectionCount]][14];
			vb8 ^= EPUK_SHORT(15) ^ OTPs[EPUK[startOTPSectionCount]][15];

			vb1 ^= vb8; //Circular-xor across section
			vb8 ^= vb7;
			vb7 ^= vb6;
			vb6 ^= vb5;
			vb5 ^= vb4;
			vb4 ^= vb3;
			vb3 ^= vb2;
			vb2 ^= vb1;

			vb1 = reverseSubstitutionTable[vb1];
			vb2 = reverseSubstitutionTable[vb2];
			vb3 = reverseSubstitutionTable[vb3];
			vb4 = reverseSubstitutionTable[vb4];
			vb5 = reverseSubstitutionTable[vb5];
			vb6 = reverseSubstitutionTable[vb6];
			vb7 = reverseSubstitutionTable[vb7];
			vb8 = reverseSubstitutionTable[vb8];

			// Section 3
			vc1 ^= EPUK_SHORT(16) ^ OTPs[EPUK[startOTPSectionCount]][16];
			vc2 ^= EPUK_SHORT(17) ^ OTPs[EPUK[startOTPSectionCount]][17];
			vc3 ^= EPUK_SHORT(18) ^ OTPs[EPUK[startOTPSectionCount]][18];
			vc4 ^= EPUK_SHORT(19) ^ OTPs[EPUK[startOTPSectionCount]][19];
			vc5 ^= EPUK_SHORT(20) ^ OTPs[EPUK[startOTPSectionCount]][20];
			vc6 ^= EPUK_SHORT(21) ^ OTPs[EPUK[startOTPSectionCount]][21];
			vc7 ^= EPUK_SHORT(22) ^ OTPs[EPUK[startOTPSectionCount]][22];
			vc8 ^= EPUK_SHORT(23) ^ OTPs[EPUK[startOTPSectionCount]][23];

			vc1 ^= vc8; //Circular-xor across section
			vc8 ^= vc7;
			vc7 ^= vc6;
			vc6 ^= vc5;
			vc5 ^= vc4;
			vc4 ^= vc3;
			vc3 ^= vc2;
			vc2 ^= vc1;

			vc1 = reverseSubstitutionTable[vc1];
			vc2 = reverseSubstitutionTable[vc2];
			vc3 = reverseSubstitutionTable[vc3];
			vc4 = reverseSubstitutionTable[vc4];
			vc5 = reverseSubstitutionTable[vc5];
			vc6 = reverseSubstitutionTable[vc6];
			vc7 = reverseSubstitutionTable[vc7];
			vc8 = reverseSubstitutionTable[vc8];

			// Section 4
			vd1 ^= EPUK_SHORT(24) ^ OTPs[EPUK[startOTPSectionCount]][24];
			vd2 ^= EPUK_SHORT(25) ^ OTPs[EPUK[startOTPSectionCount]][25];
			vd3 ^= EPUK_SHORT(26) ^ OTPs[EPUK[startOTPSectionCount]][26];
			vd4 ^= EPUK_SHORT(27) ^ OTPs[EPUK[startOTPSectionCount]][27];
			vd5 ^= EPUK_SHORT(28) ^ OTPs[EPUK[startOTPSectionCount]][28];
			vd6 ^= EPUK_SHORT(29) ^ OTPs[EPUK[startOTPSectionCount]][29];
			vd7 ^= EPUK_SHORT(30) ^ OTPs[EPUK[startOTPSectionCount]][30];
			vd8 ^= EPUK_SHORT(31) ^ OTPs[EPUK[startOTPSectionCount]][31];
			++startOTPSectionCount;

			vd1 ^= vd8; //Circular-xor across section
			vd8 ^= vd7;
			vd7 ^= vd6;
			vd6 ^= vd5;
			vd5 ^= vd4;
			vd4 ^= vd3;
			vd3 ^= vd2;
			vd2 ^= vd1;

			vd1 = reverseSubstitutionTable[vd1];
			vd2 = reverseSubstitutionTable[vd2];
			vd3 = reverseSubstitutionTable[vd3];
			vd4 = reverseSubstitutionTable[vd4];
			vd5 = reverseSubstitutionTable[vd5];
			vd6 = reverseSubstitutionTable[vd6];
			vd7 = reverseSubstitutionTable[vd7];
			vd8 = reverseSubstitutionTable[vd8];

			// Section 5
			ve1 ^= EPUK_SHORT(32) ^ OTPs[EPUK[startOTPSectionCount]][0];
			ve2 ^= EPUK_SHORT(33) ^ OTPs[EPUK[startOTPSectionCount]][1];
			ve3 ^= EPUK_SHORT(34) ^ OTPs[EPUK[startOTPSectionCount]][2];
			ve4 ^= EPUK_SHORT(35) ^ OTPs[EPUK[startOTPSectionCount]][3];
			ve5 ^= EPUK_SHORT(36) ^ OTPs[EPUK[startOTPSectionCount]][4];
			ve6 ^= EPUK_SHORT(37) ^ OTPs[EPUK[startOTPSectionCount]][5];
			ve7 ^= EPUK_SHORT(38) ^ OTPs[EPUK[startOTPSectionCount]][6];
			ve8 ^= EPUK_SHORT(39) ^ OTPs[EPUK[startOTPSectionCount]][7];

			ve1 ^= ve8; //Circular-xor across section
			ve8 ^= ve7;
			ve7 ^= ve6;
			ve6 ^= ve5;
			ve5 ^= ve4;
			ve4 ^= ve3;
			ve3 ^= ve2;
			ve2 ^= ve1;

			ve1 = reverseSubstitutionTable[ve1];
			ve2 = reverseSubstitutionTable[ve2];
			ve3 = reverseSubstitutionTable[ve3];
			ve4 = reverseSubstitutionTable[ve4];
			ve5 = reverseSubstitutionTable[ve5];
			ve6 = reverseSubstitutionTable[ve6];
			ve7 = reverseSubstitutionTable[ve7];
			ve8 = reverseSubstitutionTable[ve8];

			// Section 6
			vf1 ^= EPUK_SHORT(40) ^ OTPs[EPUK[startOTPSectionCount]][8];
			vf2 ^= EPUK_SHORT(41) ^ OTPs[EPUK[startOTPSectionCount]][9];
			vf3 ^= EPUK_SHORT(42) ^ OTPs[EPUK[startOTPSectionCount]][10];
			vf4 ^= EPUK_SHORT(43) ^ OTPs[EPUK[startOTPSectionCount]][11];
			vf5 ^= EPUK_SHORT(44) ^ OTPs[EPUK[startOTPSectionCount]][12];
			vf6 ^= EPUK_SHORT(45) ^ OTPs[EPUK[startOTPSectionCount]][13];
			vf7 ^= EPUK_SHORT(46) ^ OTPs[EPUK[startOTPSectionCount]][14];
			vf8 ^= EPUK_SHORT(47) ^ OTPs[EPUK[startOTPSectionCount]][15];

			vf1 ^= vf8; //Circular-xor across section
			vf8 ^= vf7;
			vf7 ^= vf6;
			vf6 ^= vf5;
			vf5 ^= vf4;
			vf4 ^= vf3;
			vf3 ^= vf2;
			vf2 ^= vf1;

			vf1 = reverseSubstitutionTable[vf1];
			vf2 = reverseSubstitutionTable[vf2];
			vf3 = reverseSubstitutionTable[vf3];
			vf4 = reverseSubstitutionTable[vf4];
			vf5 = reverseSubstitutionTable[vf5];
			vf6 = reverseSubstitutionTable[vf6];
			vf7 = reverseSubstitutionTable[vf7];
			vf8 = reverseSubstitutionTable[vf8];

			// Section 7
			vg1 ^= EPUK_SHORT(48) ^ OTPs[EPUK[startOTPSectionCount]][16];
			vg2 ^= EPUK_SHORT(49) ^ OTPs[EPUK[startOTPSectionCount]][17];
			vg3 ^= EPUK_SHORT(50) ^ OTPs[EPUK[startOTPSectionCount]][18];
			vg4 ^= EPUK_SHORT(51) ^ OTPs[EPUK[startOTPSectionCount]][19];
			vg5 ^= EPUK_SHORT(52) ^ OTPs[EPUK[startOTPSectionCount]][20];
			vg6 ^= EPUK_SHORT(53) ^ OTPs[EPUK[startOTPSectionCount]][21];
			vg7 ^= EPUK_SHORT(54) ^ OTPs[EPUK[startOTPSectionCount]][22];
			vg8 ^= EPUK_SHORT(55) ^ OTPs[EPUK[startOTPSectionCount]][23];

			vg1 ^= vg8; //Circular-xor across section
			vg8 ^= vg7;
			vg7 ^= vg6;
			vg6 ^= vg5;
			vg5 ^= vg4;
			vg4 ^= vg3;
			vg3 ^= vg2;
			vg2 ^= vg1;

			vg1 = reverseSubstitutionTable[vg1];
			vg2 = reverseSubstitutionTable[vg2];
			vg3 = reverseSubstitutionTable[vg3];
			vg4 = reverseSubstitutionTable[vg4];
			vg5 = reverseSubstitutionTable[vg5];
			vg6 = reverseSubstitutionTable[vg6];
			vg7 = reverseSubstitutionTable[vg7];
			vg8 = reverseSubstitutionTable[vg8];

			// Section 8
			//cout << "2DECRYPTION: starting vh5 : " << hex << vh5 << endl;
			//cout << "2DECRYPTION: starting vh6 : " << hex << vh6 << endl;
			//cout << "2DECRYPTION: starting vh7 : " << hex << vh7 << endl;
			//cout << "2DECRYPTION: starting vh8 : " << hex << vh8 << endl;
			vh1 ^= EPUK_SHORT(56) ^ OTPs[EPUK[startOTPSectionCount]][24];
			vh2 ^= EPUK_SHORT(57) ^ OTPs[EPUK[startOTPSectionCount]][25];
			vh3 ^= EPUK_SHORT(58) ^ OTPs[EPUK[startOTPSectionCount]][26];
			vh4 ^= EPUK_SHORT(59) ^ OTPs[EPUK[startOTPSectionCount]][27];
			vh5 ^= EPUK_SHORT(60) ^ OTPs[EPUK[startOTPSectionCount]][28];
			//cout << "2DECRYPTION: xored vh5 with " << hex << EPUK_SHORT(60) << " and " << OTPs[EPUK[startOTPSectionCount]][28] << " to get " << vh5 << endl;
			vh6 ^= EPUK_SHORT(61) ^ OTPs[EPUK[startOTPSectionCount]][29];
			//cout << "2DECRYPTION: xored vh6 with " << hex << EPUK_SHORT(61) << " and " << OTPs[EPUK[startOTPSectionCount]][29] << " to get " << vh6 << endl;
			vh7 ^= EPUK_SHORT(62) ^ OTPs[EPUK[startOTPSectionCount]][30];
			//cout << "2DECRYPTION: xored vh7 with " << hex << EPUK_SHORT(62) << " and " << OTPs[EPUK[startOTPSectionCount]][30] << " to get " << vh7 << endl;
			vh8 ^= EPUK_SHORT(63) ^ OTPs[EPUK[startOTPSectionCount]][31];
			//cout << "2DECRYPTION: xored vh8 with " << hex << EPUK_SHORT(63) << " and " << OTPs[EPUK[startOTPSectionCount]][31] << " to get " << vh8 << endl;

			++startOTPSectionCount;

			vh1 ^= vh8; //Circular-xor across section
			vh8 ^= vh7;
			//cout << "2DECRYPTION: xored vh8 with vh7 (" << hex << vh7 << ") to get : " << vh8 << endl;
			vh7 ^= vh6;
			//cout << "2DECRYPTION: xored vh7 with vh6 (" << hex << vh6 << ") to get : " << vh7 << endl;
			vh6 ^= vh5;
			//cout << "2DECRYPTION: xored vh6 with vh5 (" << hex << vh5 << ") to get : " << vh6 << endl;
			vh5 ^= vh4;
			//cout << "2DECRYPTION: xored vh5 with vh4 (" << hex << vh4 << ") to get : " << vh5 << endl;
			vh4 ^= vh3;
			vh3 ^= vh2;
			vh2 ^= vh1;

			vh1 = reverseSubstitutionTable[vh1];
			vh2 = reverseSubstitutionTable[vh2];
			vh3 = reverseSubstitutionTable[vh3];
			vh4 = reverseSubstitutionTable[vh4];
			vh5 = reverseSubstitutionTable[vh5];
			//cout << "2DECRYPTION: reverseSubstituted vh5 to get final vh5 : " << hex << vh5 << endl;
			vh6 = reverseSubstitutionTable[vh6];
			//cout << "2DECRYPTION: reverseSubstituted vh7 to get final vh6 : " << hex << vh6 << endl;
			vh7 = reverseSubstitutionTable[vh7];
			//cout << "2DECRYPTION: reverseSubstituted vh7 to get final vh7 : " << hex << vh7 << endl;
			vh8 = reverseSubstitutionTable[vh8];
			//cout << "2DECRYPTION: reverseSubstituted vh8 to get final vh8 : " << hex << vh8 << endl;


			if (false && round == 2 && numRounds == 2) {
				// Reflect rearrangement in memory
				*SA(0) = va1;
				*SA(1) = vb1;
				*SA(2) = vc1;
				*SA(3) = vd1;
				*SA(4) = ve1;
				*SA(5) = vf1;
				*SA(6) = vg1;
				*SA(7) = vh1;

				*SB(0) = va2;
				*SB(1) = vb2;
				*SB(2) = vc2;
				*SB(3) = vd2;
				*SB(4) = ve2;
				*SB(5) = vf2;
				*SB(6) = vg2;
				*SB(7) = vh2;

				*SC(0) = va3;
				*SC(1) = vb3;
				*SC(2) = vc3;
				*SC(3) = vd3;
				*SC(4) = ve3;
				*SC(5) = vf3;
				*SC(6) = vg3;
				*SC(7) = vh3;

				*SD(0) = va4;
				*SD(1) = vb4;
				*SD(2) = vc4;
				*SD(3) = vd4;
				*SD(4) = ve4;
				*SD(5) = vf4;
				*SD(6) = vg4;
				*SD(7) = vh4;

				*SE(0) = va5;
				*SE(1) = vb5;
				*SE(2) = vc5;
				*SE(3) = vd5;
				*SE(4) = ve5;
				*SE(5) = vf5;
				*SE(6) = vg5;
				*SE(7) = vh5;

				*SF(0) = va6;
				*SF(1) = vb6;
				*SF(2) = vc6;
				*SF(3) = vd6;
				*SF(4) = ve6;
				*SF(5) = vf6;
				*SF(6) = vg6;
				*SF(7) = vh6;

				*SG(0) = va7;
				*SG(1) = vb7;
				*SG(2) = vc7;
				*SG(3) = vd7;
				*SG(4) = ve7;
				*SG(5) = vf7;
				*SG(6) = vg7;
				*SG(7) = vh7;

				*SH(0) = va8;
				*SH(1) = vb8;
				*SH(2) = vc8;
				*SH(3) = vd8;
				*SH(4) = ve8;
				*SH(5) = vf8;
				*SH(6) = vg8;
				*SH(7) = vh8;

				startingOTPSection = startOTPSectionCount - 2;		
				return;
			} else if (round == 2 && numRounds == 4) {
				// Reflect rearrangement in memory
				va1 = va1;
				SWAP_SHORTS(va2, vb1);
				SWAP_SHORTS(va3, vc1);
				SWAP_SHORTS(va4, vd1);
				SWAP_SHORTS(va5, ve1);
				SWAP_SHORTS(va6, vf1);
				SWAP_SHORTS(va7, vg1);
				SWAP_SHORTS(va8, vh1);

				vb2 = vb2;
				SWAP_SHORTS(vb3, vc2);
				SWAP_SHORTS(vb4, vd2);
				SWAP_SHORTS(vb5, ve2);
				SWAP_SHORTS(vb6, vf2);
				SWAP_SHORTS(vb7, vg2);
				SWAP_SHORTS(vb8, vh2);

				vc3 = vc3;
				SWAP_SHORTS(vc4, vd3);
				SWAP_SHORTS(vc5, ve3);
				SWAP_SHORTS(vc6, vf3);
				SWAP_SHORTS(vc7, vg3);
				SWAP_SHORTS(vc8, vh3);

				vd4 = vd4;
				SWAP_SHORTS(vd5, ve4);
				SWAP_SHORTS(vd6, vf4);
				SWAP_SHORTS(vd7, vg4);
				SWAP_SHORTS(vd8, vh4);

				ve5 = ve5;
				SWAP_SHORTS(ve6, vf5);
				SWAP_SHORTS(ve7, vg5);
				SWAP_SHORTS(ve8, vh5);

				vf6 = vf6;
				SWAP_SHORTS(vf7, vg6);
				SWAP_SHORTS(vf8, vh6);

				vg7 = vg7;
				SWAP_SHORTS(vg8, vh7);

				vh8 = vh8;
			}
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

		//cout << "Start otp section count: " << startOTPSectionCount << endl;
		startingOTPSection = startOTPSectionCount - 2;		
	}
}


inline void insert4Bits(uint64_t bits, uint16_t nibbleIndex, uint8_t* input) {
	uint8_t* ptr = input + (nibbleIndex/2);
	if ((nibbleIndex & 0x01) == 0) {
		DASSERT(((*ptr) & 0x0F) == 0);
		*ptr |= bits;
	} else {
		DASSERT(((*ptr) & 0xF0) == 0);
		*ptr |= (bits << 4);
	}
}

void superRearrangementUpdated1(vector<uint16_t>& input, bool forwardRearrangement) {
	uint32_t size = k4 / sizeof(uint16_t);
	DASSERT(input.size() == size); // the input must be 4K bytes
	uint8_t output[k4];
	memset(output, 0U, size*sizeof(uint16_t));

	for (int k = 0; k < k4; ++k) {
		assert(output[k] == 0x00);
	}

	uint64_t* pointer = reinterpret_cast<uint64_t*>(input.data());

	for(uint32_t i = 0; i < k4/sizeof(uint64_t); ++i) {
		register uint64_t val = *pointer;
		for(uint32_t j = 0; j < 16; ++j) { // there are 16 nibbles in uint64_t
			uint16_t newInd;
			if (forwardRearrangement) {
				newInd = reverseSuperRearrangementTable[(i*16)+j];
				//cout << "ENCRYPTION: Moving index " << hex << (i*16)+j << " (val: " << (val & 0x0F) << ") to " << newInd << dec << endl;
			} else {
				newInd = superRearrangementTable[(i*16)+j];
				//cout << "DECRYPTION: Moving index " << hex << (i*16)+j << " (val: " << (val & 0x0F) << ") to " << newInd << dec << endl;
			}
			insert4Bits(val & 0x0F, newInd, output);
			val = val >> 4;
		}
		++pointer;
	}
	memcpy(input.data(), output, k4);
}

#define SUB(a, b)	SUB_(a, b)
#define SUB_(a, b)					\
	BOOST_PP_SUB(a, b)				\

#define DIV(a, b)	DIV_(a, b)
#define DIV_(a, b)					\
	BOOST_PP_DIV(a, b)				\

#define ADD(a, b)	ADD_(a, b)
#define ADD_(a, b)					\
	BOOST_PP_ADD(a, b)				\

#define MUL(a, b)	MUL_(a, b)
#define MUL_(a, b)					\
	BOOST_PP_MUL(a, b)				\

#define S_ALT(ptr, increment)			((reinterpret_cast<uint16_t *>(ptr)) + increment)

#define V_impl(type, portionN, sectionN, shortN)		v ## _ ## type ## _ ## portionN ## _ ## sectionN ## _ ## shortN 
#define V_impl1(type, portionN, sectionN, shortN)		V_impl(type, portionN, sectionN, shortN)
#define V(...)							V_impl1(__VA_ARGS__)	


#define SUPER_REARRANGEMENT_PORTION_ALL()											\
	SUPER_REARRANGEMENT_PORTION0(d);											\
SUPER_REARRANGEMENT_PORTION1(d);											\
SUPER_REARRANGEMENT_PORTION2(d);											\
SUPER_REARRANGEMENT_PORTION3(d);											\
SUPER_REARRANGEMENT_PORTION4(d);											\
SUPER_REARRANGEMENT_PORTION5(d);											\
SUPER_REARRANGEMENT_PORTION6(d);											\
SUPER_REARRANGEMENT_PORTION7(d);											\
SUPER_REARRANGEMENT_PORTION8(d);											\
SUPER_REARRANGEMENT_PORTION9(d);											\
SUPER_REARRANGEMENT_PORTION10(d);											\
SUPER_REARRANGEMENT_PORTION11(d);											\
SUPER_REARRANGEMENT_PORTION12(d);											\
SUPER_REARRANGEMENT_PORTION13(d);											\
SUPER_REARRANGEMENT_PORTION14(d);											\
SUPER_REARRANGEMENT_PORTION15(d);											\
SUPER_REARRANGEMENT_PORTION16(d);											\
SUPER_REARRANGEMENT_PORTION17(d);											\
SUPER_REARRANGEMENT_PORTION18(d);											\
SUPER_REARRANGEMENT_PORTION19(d);											\
SUPER_REARRANGEMENT_PORTION20(d);											\
SUPER_REARRANGEMENT_PORTION21(d);											\
SUPER_REARRANGEMENT_PORTION22(d);											\
SUPER_REARRANGEMENT_PORTION23(d);											\
SUPER_REARRANGEMENT_PORTION24(d);											\
SUPER_REARRANGEMENT_PORTION25(d);											\
SUPER_REARRANGEMENT_PORTION26(d);											\
SUPER_REARRANGEMENT_PORTION27(d);											\
SUPER_REARRANGEMENT_PORTION28(d);											\
SUPER_REARRANGEMENT_PORTION29(d);											\
SUPER_REARRANGEMENT_PORTION30(d);											\

#define SUPER_REARRANGEMENT_PORTION0_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 0, sectionN, 0), V(type, MUL(sectionN, 4), 0, 0));							\
SWAP_SHORTS(V(type, 0, sectionN, 1), V(type, MUL(sectionN, 4), 0, 1));							\
SWAP_SHORTS(V(type, 0, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 0, 0));						\
SWAP_SHORTS(V(type, 0, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 0, 1));						\
SWAP_SHORTS(V(type, 0, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 0, 0));						\
SWAP_SHORTS(V(type, 0, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 0, 1));						\
SWAP_SHORTS(V(type, 0, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 0, 0));						\
SWAP_SHORTS(V(type, 0, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 0, 1))

#define SUPER_REARRANGEMENT_PORTION0(type) 											\
	SWAP_SHORTS(V(type, 0, 0, 2), V(type, 1, 0, 0));									\
SWAP_SHORTS(V(type, 0, 0, 3), V(type, 1, 0, 1));									\
SWAP_SHORTS(V(type, 0, 0, 4), V(type, 2, 0, 0));									\
SWAP_SHORTS(V(type, 0, 0, 5), V(type, 2, 0, 1));									\
SWAP_SHORTS(V(type, 0, 0, 6), V(type, 3, 0, 0));									\
SWAP_SHORTS(V(type, 0, 0, 7), V(type, 3, 0, 1));									\
\
SUPER_REARRANGEMENT_PORTION0_HELPER(type, 1);										\
SUPER_REARRANGEMENT_PORTION0_HELPER(type, 2);										\
SUPER_REARRANGEMENT_PORTION0_HELPER(type, 3);										\
SUPER_REARRANGEMENT_PORTION0_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION0_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION0_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION0_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION1_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 1, sectionN, 0), V(type, MUL(sectionN, 4), 0, 2));							\
SWAP_SHORTS(V(type, 1, sectionN, 1), V(type, MUL(sectionN, 4), 0, 3));							\
SWAP_SHORTS(V(type, 1, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 0, 2));						\
SWAP_SHORTS(V(type, 1, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 0, 3));						\
SWAP_SHORTS(V(type, 1, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 0, 2));						\
SWAP_SHORTS(V(type, 1, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 0, 3));						\
SWAP_SHORTS(V(type, 1, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 0, 2));						\
SWAP_SHORTS(V(type, 1, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 0, 3))

#define SUPER_REARRANGEMENT_PORTION1(type) 											\
	SWAP_SHORTS(V(type, 1, 0, 4), V(type, 2, 0, 2));									\
SWAP_SHORTS(V(type, 1, 0, 5), V(type, 2, 0, 3));									\
SWAP_SHORTS(V(type, 1, 0, 6), V(type, 3, 0, 2));									\
SWAP_SHORTS(V(type, 1, 0, 7), V(type, 3, 0, 3));									\
\
SUPER_REARRANGEMENT_PORTION1_HELPER(type, 1);										\
SUPER_REARRANGEMENT_PORTION1_HELPER(type, 2);										\
SUPER_REARRANGEMENT_PORTION1_HELPER(type, 3);										\
SUPER_REARRANGEMENT_PORTION1_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION1_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION1_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION1_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION2_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 2, sectionN, 0), V(type, MUL(sectionN, 4), 0, 4));							\
SWAP_SHORTS(V(type, 2, sectionN, 1), V(type, MUL(sectionN, 4), 0, 5));							\
SWAP_SHORTS(V(type, 2, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 0, 4));						\
SWAP_SHORTS(V(type, 2, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 0, 5));						\
SWAP_SHORTS(V(type, 2, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 0, 4));						\
SWAP_SHORTS(V(type, 2, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 0, 5));						\
SWAP_SHORTS(V(type, 2, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 0, 4));						\
SWAP_SHORTS(V(type, 2, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 0, 5))

#define SUPER_REARRANGEMENT_PORTION2(type) 											\
	SWAP_SHORTS(V(type, 2, 0, 6), V(type, 3, 0, 4));									\
SWAP_SHORTS(V(type, 2, 0, 7), V(type, 3, 0, 5));									\
\
SUPER_REARRANGEMENT_PORTION2_HELPER(type, 1);										\
SUPER_REARRANGEMENT_PORTION2_HELPER(type, 2);										\
SUPER_REARRANGEMENT_PORTION2_HELPER(type, 3);										\
SUPER_REARRANGEMENT_PORTION2_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION2_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION2_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION2_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION3_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 3, sectionN, 0), V(type, MUL(sectionN, 4), 0, 6));							\
SWAP_SHORTS(V(type, 3, sectionN, 1), V(type, MUL(sectionN, 4), 0, 7));							\
SWAP_SHORTS(V(type, 3, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 0, 6));						\
SWAP_SHORTS(V(type, 3, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 0, 7));						\
SWAP_SHORTS(V(type, 3, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 0, 6));						\
SWAP_SHORTS(V(type, 3, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 0, 7));						\
SWAP_SHORTS(V(type, 3, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 0, 6));						\
SWAP_SHORTS(V(type, 3, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 0, 7))

#define SUPER_REARRANGEMENT_PORTION3(type) 											\
	SUPER_REARRANGEMENT_PORTION3_HELPER(type, 1);										\
SUPER_REARRANGEMENT_PORTION3_HELPER(type, 2);										\
SUPER_REARRANGEMENT_PORTION3_HELPER(type, 3);										\
SUPER_REARRANGEMENT_PORTION3_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION3_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION3_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION3_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION4_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 4, sectionN, 0), V(type, MUL(sectionN, 4), 1, 0));							\
SWAP_SHORTS(V(type, 4, sectionN, 1), V(type, MUL(sectionN, 4), 1, 1));							\
SWAP_SHORTS(V(type, 4, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 1, 0));						\
SWAP_SHORTS(V(type, 4, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 1, 1));						\
SWAP_SHORTS(V(type, 4, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 1, 0));						\
SWAP_SHORTS(V(type, 4, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 1, 1));						\
SWAP_SHORTS(V(type, 4, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 1, 0));						\
SWAP_SHORTS(V(type, 4, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 1, 1))

#define SUPER_REARRANGEMENT_PORTION4(type) 											\
	SWAP_SHORTS(V(type, 4, 1, 2), V(type, 5, 1, 0)); 									\
SWAP_SHORTS(V(type, 4, 1, 3), V(type, 5, 1, 1)); 									\
SWAP_SHORTS(V(type, 4, 1, 4), V(type, 6, 1, 0)); 									\
SWAP_SHORTS(V(type, 4, 1, 5), V(type, 6, 1, 1)); 									\
SWAP_SHORTS(V(type, 4, 1, 6), V(type, 7, 1, 0)); 									\
SWAP_SHORTS(V(type, 4, 1, 7), V(type, 7, 1, 1)); 									\
\
SUPER_REARRANGEMENT_PORTION4_HELPER(type, 2);										\
SUPER_REARRANGEMENT_PORTION4_HELPER(type, 3);										\
SUPER_REARRANGEMENT_PORTION4_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION4_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION4_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION4_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION5_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 5, sectionN, 0), V(type, MUL(sectionN, 4), 1, 2));							\
SWAP_SHORTS(V(type, 5, sectionN, 1), V(type, MUL(sectionN, 4), 1, 3));							\
SWAP_SHORTS(V(type, 5, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 1, 2));						\
SWAP_SHORTS(V(type, 5, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 1, 3));						\
SWAP_SHORTS(V(type, 5, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 1, 2));						\
SWAP_SHORTS(V(type, 5, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 1, 3));						\
SWAP_SHORTS(V(type, 5, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 1, 2));						\
SWAP_SHORTS(V(type, 5, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 1, 3))

#define SUPER_REARRANGEMENT_PORTION5(type) 											\
	SWAP_SHORTS(V(type, 5, 1, 4), V(type, 6, 1, 2)); 									\
SWAP_SHORTS(V(type, 5, 1, 5), V(type, 6, 1, 3)); 									\
SWAP_SHORTS(V(type, 5, 1, 6), V(type, 7, 1, 2)); 									\
SWAP_SHORTS(V(type, 5, 1, 7), V(type, 7, 1, 3)); 									\
\
SUPER_REARRANGEMENT_PORTION5_HELPER(type, 2);										\
SUPER_REARRANGEMENT_PORTION5_HELPER(type, 3);										\
SUPER_REARRANGEMENT_PORTION5_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION5_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION5_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION5_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION6_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 6, sectionN, 0), V(type, MUL(sectionN, 4), 1, 4));							\
SWAP_SHORTS(V(type, 6, sectionN, 1), V(type, MUL(sectionN, 4), 1, 5));							\
SWAP_SHORTS(V(type, 6, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 1, 4));						\
SWAP_SHORTS(V(type, 6, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 1, 5));						\
SWAP_SHORTS(V(type, 6, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 1, 4));						\
SWAP_SHORTS(V(type, 6, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 1, 5));						\
SWAP_SHORTS(V(type, 6, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 1, 4));						\
SWAP_SHORTS(V(type, 6, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 1, 5))

#define SUPER_REARRANGEMENT_PORTION6(type) 											\
	SWAP_SHORTS(V(type, 6, 1, 6), V(type, 7, 1, 4)); 									\
SWAP_SHORTS(V(type, 6, 1, 7), V(type, 7, 1, 5)); 									\
\
SUPER_REARRANGEMENT_PORTION6_HELPER(type, 2);										\
SUPER_REARRANGEMENT_PORTION6_HELPER(type, 3);										\
SUPER_REARRANGEMENT_PORTION6_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION6_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION6_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION6_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION7_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 7, sectionN, 0), V(type, MUL(sectionN, 4), 1, 6));							\
SWAP_SHORTS(V(type, 7, sectionN, 1), V(type, MUL(sectionN, 4), 1, 7));							\
SWAP_SHORTS(V(type, 7, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 1, 6));						\
SWAP_SHORTS(V(type, 7, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 1, 7));						\
SWAP_SHORTS(V(type, 7, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 1, 6));						\
SWAP_SHORTS(V(type, 7, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 1, 7));						\
SWAP_SHORTS(V(type, 7, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 1, 6));						\
SWAP_SHORTS(V(type, 7, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 1, 7))

#define SUPER_REARRANGEMENT_PORTION7(type) 											\
	SUPER_REARRANGEMENT_PORTION7_HELPER(type, 2);										\
SUPER_REARRANGEMENT_PORTION7_HELPER(type, 3);										\
SUPER_REARRANGEMENT_PORTION7_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION7_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION7_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION7_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION8_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 8, sectionN, 0), V(type, MUL(sectionN, 4), 2, 0));							\
SWAP_SHORTS(V(type, 8, sectionN, 1), V(type, MUL(sectionN, 4), 2, 1));							\
SWAP_SHORTS(V(type, 8, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 2, 0));						\
SWAP_SHORTS(V(type, 8, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 2, 1));						\
SWAP_SHORTS(V(type, 8, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 2, 0));						\
SWAP_SHORTS(V(type, 8, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 2, 1));						\
SWAP_SHORTS(V(type, 8, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 2, 0));						\
SWAP_SHORTS(V(type, 8, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 2, 1))

#define SUPER_REARRANGEMENT_PORTION8(type) 											\
	SWAP_SHORTS(V(type, 8, 2, 2), V(type, 9, 2, 0)); 									\
SWAP_SHORTS(V(type, 8, 2, 3), V(type, 9, 2, 1)); 									\
SWAP_SHORTS(V(type, 8, 2, 4), V(type, 10, 2, 0)); 									\
SWAP_SHORTS(V(type, 8, 2, 5), V(type, 10, 2, 1)); 									\
SWAP_SHORTS(V(type, 8, 2, 6), V(type, 11, 2, 0)); 									\
SWAP_SHORTS(V(type, 8, 2, 7), V(type, 11, 2, 1)); 									\
\
SUPER_REARRANGEMENT_PORTION8_HELPER(type, 3);										\
SUPER_REARRANGEMENT_PORTION8_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION8_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION8_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION8_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION9_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 9, sectionN, 0), V(type, MUL(sectionN, 4), 2, 2));							\
SWAP_SHORTS(V(type, 9, sectionN, 1), V(type, MUL(sectionN, 4), 2, 3));							\
SWAP_SHORTS(V(type, 9, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 2, 2));						\
SWAP_SHORTS(V(type, 9, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 2, 3));						\
SWAP_SHORTS(V(type, 9, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 2, 2));						\
SWAP_SHORTS(V(type, 9, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 2, 3));						\
SWAP_SHORTS(V(type, 9, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 2, 2));						\
SWAP_SHORTS(V(type, 9, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 2, 3))

#define SUPER_REARRANGEMENT_PORTION9(type) 											\
	SWAP_SHORTS(V(type, 9, 2, 4), V(type, 10, 2, 2)); 									\
SWAP_SHORTS(V(type, 9, 2, 5), V(type, 10, 2, 3)); 									\
SWAP_SHORTS(V(type, 9, 2, 6), V(type, 11, 2, 2)); 									\
SWAP_SHORTS(V(type, 9, 2, 7), V(type, 11, 2, 3)); 									\
\
SUPER_REARRANGEMENT_PORTION9_HELPER(type, 3);										\
SUPER_REARRANGEMENT_PORTION9_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION9_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION9_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION9_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION10_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 10, sectionN, 0), V(type, MUL(sectionN, 4), 2, 4));							\
SWAP_SHORTS(V(type, 10, sectionN, 1), V(type, MUL(sectionN, 4), 2, 5));							\
SWAP_SHORTS(V(type, 10, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 2, 4));						\
SWAP_SHORTS(V(type, 10, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 2, 5));						\
SWAP_SHORTS(V(type, 10, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 2, 4));						\
SWAP_SHORTS(V(type, 10, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 2, 5));						\
SWAP_SHORTS(V(type, 10, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 2, 4));						\
SWAP_SHORTS(V(type, 10, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 2, 5))

#define SUPER_REARRANGEMENT_PORTION10(type) 											\
	SWAP_SHORTS(V(type, 10, 2, 6), V(type, 11, 2, 4)); 									\
SWAP_SHORTS(V(type, 10, 2, 7), V(type, 11, 2, 5)); 									\
\
SUPER_REARRANGEMENT_PORTION10_HELPER(type, 3);										\
SUPER_REARRANGEMENT_PORTION10_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION10_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION10_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION10_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION11_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 11, sectionN, 0), V(type, MUL(sectionN, 4), 2, 6));							\
SWAP_SHORTS(V(type, 11, sectionN, 1), V(type, MUL(sectionN, 4), 2, 7));							\
SWAP_SHORTS(V(type, 11, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 2, 6));						\
SWAP_SHORTS(V(type, 11, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 2, 7));						\
SWAP_SHORTS(V(type, 11, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 2, 6));						\
SWAP_SHORTS(V(type, 11, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 2, 7));						\
SWAP_SHORTS(V(type, 11, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 2, 6));						\
SWAP_SHORTS(V(type, 11, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 2, 7))

#define SUPER_REARRANGEMENT_PORTION11(type) 											\
	SUPER_REARRANGEMENT_PORTION11_HELPER(type, 3);										\
SUPER_REARRANGEMENT_PORTION11_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION11_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION11_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION11_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION12_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 12, sectionN, 0), V(type, MUL(sectionN, 4), 3, 0));							\
SWAP_SHORTS(V(type, 12, sectionN, 1), V(type, MUL(sectionN, 4), 3, 1));							\
SWAP_SHORTS(V(type, 12, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 3, 0));						\
SWAP_SHORTS(V(type, 12, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 3, 1));						\
SWAP_SHORTS(V(type, 12, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 3, 0));						\
SWAP_SHORTS(V(type, 12, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 3, 1));						\
SWAP_SHORTS(V(type, 12, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 3, 0));						\
SWAP_SHORTS(V(type, 12, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 3, 1))

#define SUPER_REARRANGEMENT_PORTION12(type) 											\
	SWAP_SHORTS(V(type, 12, 3, 2), V(type, 13, 3, 0)); 									\
SWAP_SHORTS(V(type, 12, 3, 3), V(type, 13, 3, 1)); 									\
SWAP_SHORTS(V(type, 12, 3, 4), V(type, 14, 3, 0)); 									\
SWAP_SHORTS(V(type, 12, 3, 5), V(type, 14, 3, 1)); 									\
SWAP_SHORTS(V(type, 12, 3, 6), V(type, 15, 3, 0)); 									\
SWAP_SHORTS(V(type, 12, 3, 7), V(type, 15, 3, 1)); 									\
\
SUPER_REARRANGEMENT_PORTION12_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION12_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION12_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION12_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION13_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 13, sectionN, 0), V(type, MUL(sectionN, 4), 3, 2));							\
SWAP_SHORTS(V(type, 13, sectionN, 1), V(type, MUL(sectionN, 4), 3, 3));							\
SWAP_SHORTS(V(type, 13, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 3, 2));						\
SWAP_SHORTS(V(type, 13, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 3, 3));						\
SWAP_SHORTS(V(type, 13, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 3, 2));						\
SWAP_SHORTS(V(type, 13, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 3, 3));						\
SWAP_SHORTS(V(type, 13, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 3, 2));						\
SWAP_SHORTS(V(type, 13, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 3, 3))

#define SUPER_REARRANGEMENT_PORTION13(type) 											\
	SWAP_SHORTS(V(type, 13, 3, 4), V(type, 14, 3, 2)); 									\
SWAP_SHORTS(V(type, 13, 3, 5), V(type, 14, 3, 3)); 									\
SWAP_SHORTS(V(type, 13, 3, 6), V(type, 15, 3, 2)); 									\
SWAP_SHORTS(V(type, 13, 3, 7), V(type, 15, 3, 3)); 									\
\
SUPER_REARRANGEMENT_PORTION13_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION13_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION13_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION13_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION14_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 14, sectionN, 0), V(type, MUL(sectionN, 4), 3, 4));							\
SWAP_SHORTS(V(type, 14, sectionN, 1), V(type, MUL(sectionN, 4), 3, 5));							\
SWAP_SHORTS(V(type, 14, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 3, 4));						\
SWAP_SHORTS(V(type, 14, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 3, 5));						\
SWAP_SHORTS(V(type, 14, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 3, 4));						\
SWAP_SHORTS(V(type, 14, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 3, 5));						\
SWAP_SHORTS(V(type, 14, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 3, 4));						\
SWAP_SHORTS(V(type, 14, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 3, 5))

#define SUPER_REARRANGEMENT_PORTION14(type) 											\
	SWAP_SHORTS(V(type, 14, 3, 6), V(type, 15, 3, 4)); 									\
SWAP_SHORTS(V(type, 14, 3, 7), V(type, 15, 3, 5)); 									\
\
SUPER_REARRANGEMENT_PORTION14_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION14_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION14_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION14_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION15_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 15, sectionN, 0), V(type, MUL(sectionN, 4), 3, 6));							\
SWAP_SHORTS(V(type, 15, sectionN, 1), V(type, MUL(sectionN, 4), 3, 7));							\
SWAP_SHORTS(V(type, 15, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 3, 6));						\
SWAP_SHORTS(V(type, 15, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 3, 7));						\
SWAP_SHORTS(V(type, 15, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 3, 6));						\
SWAP_SHORTS(V(type, 15, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 3, 7));						\
SWAP_SHORTS(V(type, 15, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 3, 6));						\
SWAP_SHORTS(V(type, 15, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 3, 7))

#define SUPER_REARRANGEMENT_PORTION15(type) 											\
	SUPER_REARRANGEMENT_PORTION15_HELPER(type, 4);										\
SUPER_REARRANGEMENT_PORTION15_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION15_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION15_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION16_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 16, sectionN, 0), V(type, MUL(sectionN, 4), 4, 0));							\
SWAP_SHORTS(V(type, 16, sectionN, 1), V(type, MUL(sectionN, 4), 4, 1));							\
SWAP_SHORTS(V(type, 16, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 4, 0));						\
SWAP_SHORTS(V(type, 16, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 4, 1));						\
SWAP_SHORTS(V(type, 16, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 4, 0));						\
SWAP_SHORTS(V(type, 16, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 4, 1));						\
SWAP_SHORTS(V(type, 16, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 4, 0));						\
SWAP_SHORTS(V(type, 16, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 4, 1))

#define SUPER_REARRANGEMENT_PORTION16(type) 											\
	SWAP_SHORTS(V(type, 16, 4, 2), V(type, 17, 4, 0)); 									\
SWAP_SHORTS(V(type, 16, 4, 3), V(type, 17, 4, 1)); 									\
SWAP_SHORTS(V(type, 16, 4, 4), V(type, 18, 4, 0)); 									\
SWAP_SHORTS(V(type, 16, 4, 5), V(type, 18, 4, 1)); 									\
SWAP_SHORTS(V(type, 16, 4, 6), V(type, 19, 4, 0)); 									\
SWAP_SHORTS(V(type, 16, 4, 7), V(type, 19, 4, 1)); 									\
\
SUPER_REARRANGEMENT_PORTION16_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION16_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION16_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION17_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 17, sectionN, 0), V(type, MUL(sectionN, 4), 4, 2));							\
SWAP_SHORTS(V(type, 17, sectionN, 1), V(type, MUL(sectionN, 4), 4, 3));							\
SWAP_SHORTS(V(type, 17, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 4, 2));						\
SWAP_SHORTS(V(type, 17, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 4, 3));						\
SWAP_SHORTS(V(type, 17, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 4, 2));						\
SWAP_SHORTS(V(type, 17, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 4, 3));						\
SWAP_SHORTS(V(type, 17, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 4, 2));						\
SWAP_SHORTS(V(type, 17, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 4, 3))

#define SUPER_REARRANGEMENT_PORTION17(type) 											\
	SWAP_SHORTS(V(type, 17, 4, 4), V(type, 18, 4, 2)); 									\
SWAP_SHORTS(V(type, 17, 4, 5), V(type, 18, 4, 3)); 									\
SWAP_SHORTS(V(type, 17, 4, 6), V(type, 19, 4, 2)); 									\
SWAP_SHORTS(V(type, 17, 4, 7), V(type, 19, 4, 3)); 									\
\
SUPER_REARRANGEMENT_PORTION17_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION17_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION17_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION18_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 18, sectionN, 0), V(type, MUL(sectionN, 4), 4, 4));							\
SWAP_SHORTS(V(type, 18, sectionN, 1), V(type, MUL(sectionN, 4), 4, 5));							\
SWAP_SHORTS(V(type, 18, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 4, 4));						\
SWAP_SHORTS(V(type, 18, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 4, 5));						\
SWAP_SHORTS(V(type, 18, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 4, 4));						\
SWAP_SHORTS(V(type, 18, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 4, 5));						\
SWAP_SHORTS(V(type, 18, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 4, 4));						\
SWAP_SHORTS(V(type, 18, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 4, 5))

#define SUPER_REARRANGEMENT_PORTION18(type) 											\
	SWAP_SHORTS(V(type, 18, 4, 6), V(type, 19, 4, 4)); 									\
SWAP_SHORTS(V(type, 18, 4, 7), V(type, 19, 4, 5)); 									\
\
SUPER_REARRANGEMENT_PORTION18_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION18_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION18_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION19_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 19, sectionN, 0), V(type, MUL(sectionN, 4), 4, 6));							\
SWAP_SHORTS(V(type, 19, sectionN, 1), V(type, MUL(sectionN, 4), 4, 7));							\
SWAP_SHORTS(V(type, 19, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 4, 6));						\
SWAP_SHORTS(V(type, 19, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 4, 7));						\
SWAP_SHORTS(V(type, 19, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 4, 6));						\
SWAP_SHORTS(V(type, 19, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 4, 7));						\
SWAP_SHORTS(V(type, 19, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 4, 6));						\
SWAP_SHORTS(V(type, 19, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 4, 7))

#define SUPER_REARRANGEMENT_PORTION19(type) 											\
	SUPER_REARRANGEMENT_PORTION19_HELPER(type, 5);										\
SUPER_REARRANGEMENT_PORTION19_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION19_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION20_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 20, sectionN, 0), V(type, MUL(sectionN, 4), 5, 0));							\
SWAP_SHORTS(V(type, 20, sectionN, 1), V(type, MUL(sectionN, 4), 5, 1));							\
SWAP_SHORTS(V(type, 20, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 5, 0));						\
SWAP_SHORTS(V(type, 20, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 5, 1));						\
SWAP_SHORTS(V(type, 20, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 5, 0));						\
SWAP_SHORTS(V(type, 20, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 5, 1));						\
SWAP_SHORTS(V(type, 20, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 5, 0));						\
SWAP_SHORTS(V(type, 20, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 5, 1))

#define SUPER_REARRANGEMENT_PORTION20(type) 											\
	SWAP_SHORTS(V(type, 20, 5, 2), V(type, 21, 5, 0)); 									\
SWAP_SHORTS(V(type, 20, 5, 3), V(type, 21, 5, 1)); 									\
SWAP_SHORTS(V(type, 20, 5, 4), V(type, 22, 5, 0)); 									\
SWAP_SHORTS(V(type, 20, 5, 5), V(type, 22, 5, 1)); 									\
SWAP_SHORTS(V(type, 20, 5, 6), V(type, 23, 5, 0)); 									\
SWAP_SHORTS(V(type, 20, 5, 7), V(type, 23, 5, 1)); 									\
\
SUPER_REARRANGEMENT_PORTION20_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION20_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION21_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 21, sectionN, 0), V(type, MUL(sectionN, 4), 5, 2));							\
SWAP_SHORTS(V(type, 21, sectionN, 1), V(type, MUL(sectionN, 4), 5, 3));							\
SWAP_SHORTS(V(type, 21, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 5, 2));						\
SWAP_SHORTS(V(type, 21, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 5, 3));						\
SWAP_SHORTS(V(type, 21, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 5, 2));						\
SWAP_SHORTS(V(type, 21, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 5, 3));						\
SWAP_SHORTS(V(type, 21, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 5, 2));						\
SWAP_SHORTS(V(type, 21, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 5, 3))

#define SUPER_REARRANGEMENT_PORTION21(type) 											\
	SWAP_SHORTS(V(type, 21, 5, 4), V(type, 22, 5, 2)); 									\
SWAP_SHORTS(V(type, 21, 5, 5), V(type, 22, 5, 3)); 									\
SWAP_SHORTS(V(type, 21, 5, 6), V(type, 23, 5, 2)); 									\
SWAP_SHORTS(V(type, 21, 5, 7), V(type, 23, 5, 3)); 									\
\
SUPER_REARRANGEMENT_PORTION21_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION21_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION22_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 22, sectionN, 0), V(type, MUL(sectionN, 4), 5, 4));							\
SWAP_SHORTS(V(type, 22, sectionN, 1), V(type, MUL(sectionN, 4), 5, 5));							\
SWAP_SHORTS(V(type, 22, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 5, 4));						\
SWAP_SHORTS(V(type, 22, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 5, 5));						\
SWAP_SHORTS(V(type, 22, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 5, 4));						\
SWAP_SHORTS(V(type, 22, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 5, 5));						\
SWAP_SHORTS(V(type, 22, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 5, 4));						\
SWAP_SHORTS(V(type, 22, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 5, 5))

#define SUPER_REARRANGEMENT_PORTION22(type) 											\
	SWAP_SHORTS(V(type, 22, 5, 6), V(type, 23, 5, 4)); 									\
SWAP_SHORTS(V(type, 22, 5, 7), V(type, 23, 5, 5)); 									\
\
SUPER_REARRANGEMENT_PORTION22_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION22_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION23_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 23, sectionN, 0), V(type, MUL(sectionN, 4), 5, 6));							\
SWAP_SHORTS(V(type, 23, sectionN, 1), V(type, MUL(sectionN, 4), 5, 7));							\
SWAP_SHORTS(V(type, 23, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 5, 6));						\
SWAP_SHORTS(V(type, 23, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 5, 7));						\
SWAP_SHORTS(V(type, 23, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 5, 6));						\
SWAP_SHORTS(V(type, 23, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 5, 7));						\
SWAP_SHORTS(V(type, 23, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 5, 6));						\
SWAP_SHORTS(V(type, 23, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 5, 7))

#define SUPER_REARRANGEMENT_PORTION23(type) 											\
	SUPER_REARRANGEMENT_PORTION23_HELPER(type, 6);										\
SUPER_REARRANGEMENT_PORTION23_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION24_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 24, sectionN, 0), V(type, MUL(sectionN, 4), 6, 0));							\
SWAP_SHORTS(V(type, 24, sectionN, 1), V(type, MUL(sectionN, 4), 6, 1));							\
SWAP_SHORTS(V(type, 24, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 6, 0));						\
SWAP_SHORTS(V(type, 24, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 6, 1));						\
SWAP_SHORTS(V(type, 24, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 6, 0));						\
SWAP_SHORTS(V(type, 24, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 6, 1));						\
SWAP_SHORTS(V(type, 24, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 6, 0));						\
SWAP_SHORTS(V(type, 24, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 6, 1))

#define SUPER_REARRANGEMENT_PORTION24(type) 											\
	SWAP_SHORTS(V(type, 24, 6, 2), V(type, 25, 6, 0)); 									\
SWAP_SHORTS(V(type, 24, 6, 3), V(type, 25, 6, 1)); 									\
SWAP_SHORTS(V(type, 24, 6, 4), V(type, 26, 6, 0)); 									\
SWAP_SHORTS(V(type, 24, 6, 5), V(type, 26, 6, 1)); 									\
SWAP_SHORTS(V(type, 24, 6, 6), V(type, 27, 6, 0)); 									\
SWAP_SHORTS(V(type, 24, 6, 7), V(type, 27, 6, 1)); 									\
\
SUPER_REARRANGEMENT_PORTION24_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION25_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 25, sectionN, 0), V(type, MUL(sectionN, 4), 6, 2));							\
SWAP_SHORTS(V(type, 25, sectionN, 1), V(type, MUL(sectionN, 4), 6, 3));							\
SWAP_SHORTS(V(type, 25, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 6, 2));						\
SWAP_SHORTS(V(type, 25, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 6, 3));						\
SWAP_SHORTS(V(type, 25, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 6, 2));						\
SWAP_SHORTS(V(type, 25, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 6, 3));						\
SWAP_SHORTS(V(type, 25, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 6, 2));						\
SWAP_SHORTS(V(type, 25, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 6, 3))

#define SUPER_REARRANGEMENT_PORTION25(type) 											\
	SWAP_SHORTS(V(type, 25, 6, 4), V(type, 26, 6, 2)); 									\
SWAP_SHORTS(V(type, 25, 6, 5), V(type, 26, 6, 3)); 									\
SWAP_SHORTS(V(type, 25, 6, 6), V(type, 27, 6, 2)); 									\
SWAP_SHORTS(V(type, 25, 6, 7), V(type, 27, 6, 3)); 									\
\
SUPER_REARRANGEMENT_PORTION25_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION26_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 26, sectionN, 0), V(type, MUL(sectionN, 4), 6, 4));							\
SWAP_SHORTS(V(type, 26, sectionN, 1), V(type, MUL(sectionN, 4), 6, 5));							\
SWAP_SHORTS(V(type, 26, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 6, 4));						\
SWAP_SHORTS(V(type, 26, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 6, 5));						\
SWAP_SHORTS(V(type, 26, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 6, 4));						\
SWAP_SHORTS(V(type, 26, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 6, 5));						\
SWAP_SHORTS(V(type, 26, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 6, 4));						\
SWAP_SHORTS(V(type, 26, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 6, 5))

#define SUPER_REARRANGEMENT_PORTION26(type) 											\
	SWAP_SHORTS(V(type, 26, 6, 6), V(type, 27, 6, 4)); 									\
SWAP_SHORTS(V(type, 26, 6, 7), V(type, 27, 6, 5)); 									\
\
SUPER_REARRANGEMENT_PORTION26_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION27_HELPER(type, sectionN)									\
	SWAP_SHORTS(V(type, 27, sectionN, 0), V(type, MUL(sectionN, 4), 6, 6));							\
SWAP_SHORTS(V(type, 27, sectionN, 1), V(type, MUL(sectionN, 4), 6, 7));							\
SWAP_SHORTS(V(type, 27, sectionN, 2), V(type, ADD(MUL(sectionN, 4), 1), 6, 6));						\
SWAP_SHORTS(V(type, 27, sectionN, 3), V(type, ADD(MUL(sectionN, 4), 1), 6, 7));						\
SWAP_SHORTS(V(type, 27, sectionN, 4), V(type, ADD(MUL(sectionN, 4), 2), 6, 6));						\
SWAP_SHORTS(V(type, 27, sectionN, 5), V(type, ADD(MUL(sectionN, 4), 2), 6, 7));						\
SWAP_SHORTS(V(type, 27, sectionN, 6), V(type, ADD(MUL(sectionN, 4), 3), 6, 6));						\
SWAP_SHORTS(V(type, 27, sectionN, 7), V(type, ADD(MUL(sectionN, 4), 3), 6, 7))

#define SUPER_REARRANGEMENT_PORTION27(type) 											\
	SUPER_REARRANGEMENT_PORTION27_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION28(type) 											\
	SWAP_SHORTS(V(type, 28, 7, 2), V(type, 29, 7, 0)); 									\
SWAP_SHORTS(V(type, 28, 7, 3), V(type, 29, 7, 1)); 									\
SWAP_SHORTS(V(type, 28, 7, 4), V(type, 30, 7, 0)); 									\
SWAP_SHORTS(V(type, 28, 7, 5), V(type, 30, 7, 1)); 									\
SWAP_SHORTS(V(type, 28, 7, 6), V(type, 31, 7, 0)); 									\
SWAP_SHORTS(V(type, 28, 7, 7), V(type, 31, 7, 1));

#define SUPER_REARRANGEMENT_PORTION29(type) 											\
	SWAP_SHORTS(V(type, 29, 7, 4), V(type, 30, 7, 2)); 									\
SWAP_SHORTS(V(type, 29, 7, 5), V(type, 30, 7, 3)); 									\
SWAP_SHORTS(V(type, 29, 7, 6), V(type, 31, 7, 2)); 									\
SWAP_SHORTS(V(type, 29, 7, 7), V(type, 31, 7, 3))

#define SUPER_REARRANGEMENT_PORTION30(type) 											\
	SWAP_SHORTS(V(type, 30, 7, 6), V(type, 31, 7, 4)); 									\
SWAP_SHORTS(V(type, 30, 7, 7), V(type, 31, 7, 5))


#define LOAD_SECTION_ALT(type, ptr, increment, portionN, sectionN)										\
	uint16_t V(type, portionN, sectionN, 0) = *S_ALT(ptr, increment); 						\
uint16_t V(type, portionN, sectionN, 1) = *S_ALT(ptr, increment+1); 						\
uint16_t V(type, portionN, sectionN, 2) = *S_ALT(ptr, increment+2); 						\
uint16_t V(type, portionN, sectionN, 3) = *S_ALT(ptr, increment+3); 						\
uint16_t V(type, portionN, sectionN, 4) = *S_ALT(ptr, increment+4); 						\
uint16_t V(type, portionN, sectionN, 5) = *S_ALT(ptr, increment+5); 						\
uint16_t V(type, portionN, sectionN, 6) = *S_ALT(ptr, increment+6); 						\
uint16_t V(type, portionN, sectionN, 7) = *S_ALT(ptr, increment+7)

#define LOAD_PORTION_ALT(type, ptr, increment, portionN)											\
	LOAD_SECTION_ALT(type, ptr, increment, portionN, 0);											\
LOAD_SECTION_ALT(type, ptr, increment+8, portionN, 1);											\
LOAD_SECTION_ALT(type, ptr, increment+16, portionN, 2);											\
LOAD_SECTION_ALT(type, ptr, increment+24, portionN, 3);											\
LOAD_SECTION_ALT(type, ptr, increment+32, portionN, 4);											\
LOAD_SECTION_ALT(type, ptr, increment+40, portionN, 5);											\
LOAD_SECTION_ALT(type, ptr, increment+48, portionN, 6);											\
LOAD_SECTION_ALT(type, ptr, increment+56, portionN, 7);

#define LOAD_PORTION_ALL_ALT(type, ptr)												\
	LOAD_PORTION_ALT(type, ptr, 0, 0);												\
LOAD_PORTION_ALT(type, ptr, 64, 1);												\
LOAD_PORTION_ALT(type, ptr, 128, 2);												\
LOAD_PORTION_ALT(type, ptr, 192, 3);												\
LOAD_PORTION_ALT(type, ptr, 256, 4);												\
LOAD_PORTION_ALT(type, ptr, 320, 5);												\
LOAD_PORTION_ALT(type, ptr, 384, 6);												\
LOAD_PORTION_ALT(type, ptr, 448, 7);												\
LOAD_PORTION_ALT(type, ptr, 512, 8);												\
LOAD_PORTION_ALT(type, ptr, 576, 9);												\
LOAD_PORTION_ALT(type, ptr, 640, 10);												\
LOAD_PORTION_ALT(type, ptr, 704, 11);												\
LOAD_PORTION_ALT(type, ptr, 768, 12);												\
LOAD_PORTION_ALT(type, ptr, 832, 13);												\
LOAD_PORTION_ALT(type, ptr, 896, 14);												\
LOAD_PORTION_ALT(type, ptr, 960, 15);												\
LOAD_PORTION_ALT(type, ptr, 1024, 16);												\
LOAD_PORTION_ALT(type, ptr, 1088, 17);												\
LOAD_PORTION_ALT(type, ptr, 1152, 18);												\
LOAD_PORTION_ALT(type, ptr, 1216, 19);												\
LOAD_PORTION_ALT(type, ptr, 1280, 20);												\
LOAD_PORTION_ALT(type, ptr, 1344, 21);												\
LOAD_PORTION_ALT(type, ptr, 1408, 22);												\
LOAD_PORTION_ALT(type, ptr, 1472, 23);												\
LOAD_PORTION_ALT(type, ptr, 1536, 24);												\
LOAD_PORTION_ALT(type, ptr, 1600, 25);												\
LOAD_PORTION_ALT(type, ptr, 1664, 26);												\
LOAD_PORTION_ALT(type, ptr, 1728, 27);												\
LOAD_PORTION_ALT(type, ptr, 1792, 28);												\
LOAD_PORTION_ALT(type, ptr, 1856, 29);												\
LOAD_PORTION_ALT(type, ptr, 1920, 30);												\
LOAD_PORTION_ALT(type, ptr, 1984, 31)

#define SET_SECTION_ALT(type, ptr, increment, portionN, sectionN)							\
	*S_ALT(ptr, increment) = V(type, portionN, sectionN, 0);						\
*S_ALT(ptr, increment+1) = V(type, portionN, sectionN, 1);						\
*S_ALT(ptr, increment+2) = V(type, portionN, sectionN, 2);						\
*S_ALT(ptr, increment+3) = V(type, portionN, sectionN, 3);						\
*S_ALT(ptr, increment+4) = V(type, portionN, sectionN, 4);						\
*S_ALT(ptr, increment+5) = V(type, portionN, sectionN, 5);						\
*S_ALT(ptr, increment+6) = V(type, portionN, sectionN, 6);						\
*S_ALT(ptr, increment+7) = V(type, portionN, sectionN, 7);						\

#define SET_PORTION_ALT(type, ptr, increment, portionN)											\
	SET_SECTION_ALT(type, ptr, increment, portionN, 0);										\
SET_SECTION_ALT(type, ptr, increment+8, portionN, 1);										\
SET_SECTION_ALT(type, ptr, increment+16, portionN, 2);										\
SET_SECTION_ALT(type, ptr, increment+24, portionN, 3);										\
SET_SECTION_ALT(type, ptr, increment+32, portionN, 4);										\
SET_SECTION_ALT(type, ptr, increment+40, portionN, 5);										\
SET_SECTION_ALT(type, ptr, increment+48, portionN, 6);										\
SET_SECTION_ALT(type, ptr, increment+56, portionN, 7)

#define SET_PORTION_ALL_ALT(type, ptr)												\
	SET_PORTION_ALT(type, ptr, 0, 0);												\
SET_PORTION_ALT(type, ptr, 64, 1);												\
SET_PORTION_ALT(type, ptr, 128, 2);												\
SET_PORTION_ALT(type, ptr, 192, 3);												\
SET_PORTION_ALT(type, ptr, 256, 4);												\
SET_PORTION_ALT(type, ptr, 320, 5);												\
SET_PORTION_ALT(type, ptr, 384, 6);												\
SET_PORTION_ALT(type, ptr, 448, 7);												\
SET_PORTION_ALT(type, ptr, 512, 8);												\
SET_PORTION_ALT(type, ptr, 576, 9);												\
SET_PORTION_ALT(type, ptr, 640, 10);												\
SET_PORTION_ALT(type, ptr, 704, 11);												\
SET_PORTION_ALT(type, ptr, 768, 12);												\
SET_PORTION_ALT(type, ptr, 832, 13);												\
SET_PORTION_ALT(type, ptr, 896, 14);												\
SET_PORTION_ALT(type, ptr, 960, 15);												\
SET_PORTION_ALT(type, ptr, 1024, 16);												\
SET_PORTION_ALT(type, ptr, 1088, 17);												\
SET_PORTION_ALT(type, ptr, 1152, 18);												\
SET_PORTION_ALT(type, ptr, 1216, 19);												\
SET_PORTION_ALT(type, ptr, 1280, 20);												\
SET_PORTION_ALT(type, ptr, 1344, 21);												\
SET_PORTION_ALT(type, ptr, 1408, 22);												\
SET_PORTION_ALT(type, ptr, 1472, 23);												\
SET_PORTION_ALT(type, ptr, 1536, 24);												\
SET_PORTION_ALT(type, ptr, 1600, 25);												\
SET_PORTION_ALT(type, ptr, 1664, 26);												\
SET_PORTION_ALT(type, ptr, 1728, 27);												\
SET_PORTION_ALT(type, ptr, 1792, 28);												\
SET_PORTION_ALT(type, ptr, 1856, 29);												\
SET_PORTION_ALT(type, ptr, 1920, 30);												\
SET_PORTION_ALT(type, ptr, 1984, 31)


void encryption_1(vector<uint16_t>& input, vector<uint16_t>& EPUK, uint32_t& OTPsectionCount) {
	uint32_t elementsIn1024bits = k1 / (sizeof(uint16_t) * 8);
	for (uint32_t i = 0; i < 32; ++i){
		replaceFoldCycles_old(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}
	uint16_t* ptr = input.data();
	LOAD_PORTION_ALL_ALT(d, ptr);
	SUPER_REARRANGEMENT_PORTION_ALL();
	SET_PORTION_ALL_ALT(d, ptr);
	for (uint32_t i = 0; i < 32; ++i){
		replaceFoldCycles_old(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}
}

void decryption_1(vector<uint16_t>& input, vector<uint16_t>& EPUK, uint32_t& OTPsectionCount) {
	uint32_t elementsIn1024bits = k1 / (sizeof(uint16_t) * 8);
	for (int32_t i = 32 - 1; i >= 0; i -= 1) {
		reverseReplaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}
	uint16_t* ptr = input.data();
	LOAD_PORTION_ALL_ALT(d, ptr);
	SUPER_REARRANGEMENT_PORTION_ALL();
	SET_PORTION_ALL_ALT(d, ptr);
	for (int32_t i = 32 - 1; i >= 0; i -= 1) {
		reverseReplaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}
}


void encryption(vector<uint16_t>& input, vector<uint16_t>& EPUK, uint32_t& OTPsectionCount) {
	uint32_t elementsIn1024bits = k1 / (sizeof(uint16_t) * 8);
	for (uint32_t i = 0; i < 32; ++i){
		replaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}
	superRearrangementUpdated1(input, true);
	for (uint32_t i = 0; i < 32; ++i){
		replaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}
}

void decryption(vector<uint16_t>& input, vector<uint16_t>& EPUK, uint32_t& OTPsectionCount) {
	uint32_t elementsIn1024bits = k1 / (sizeof(uint16_t) * 8);
	for (int32_t i = 32 - 1; i >= 0; i -= 1) {
		reverseReplaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}
	superRearrangementUpdated1(input, false);
	for (int32_t i = 32 - 1; i >= 0; i -= 1) {
		reverseReplaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}
}

void encryption_2(vector<uint16_t>& input, vector<uint16_t>& EPUK, uint32_t& OTPsectionCount) {
	uint32_t elementsIn1024bits = k1 / (sizeof(uint16_t) * 8);
	for (uint32_t i = 0; i < 32; ++i){
		replaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}
	uint16_t* ptr = input.data();
	LOAD_PORTION_ALL_ALT(d, ptr);
	SUPER_REARRANGEMENT_PORTION_ALL();
	SET_PORTION_ALL_ALT(d, ptr);
	for (uint32_t i = 0; i < 32; ++i){
		replaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}
}

void decryption_2(vector<uint16_t>& input, vector<uint16_t>& EPUK, uint32_t& OTPsectionCount) {
	uint32_t elementsIn1024bits = k1 / (sizeof(uint16_t) * 8);
	for (int32_t i = 32 - 1; i >= 0; i -= 1) {
		reverseReplaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}
	uint16_t* ptr = input.data();
	LOAD_PORTION_ALL_ALT(d, ptr);
	SUPER_REARRANGEMENT_PORTION_ALL();
	SET_PORTION_ALL_ALT(d, ptr);
	for (int32_t i = 32 - 1; i >= 0; i -= 1) {
		reverseReplaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}
}

void initialize() {
	initializeOTPsections();
	initializeSuperRearrangementTable();

	uint32_t size = (k4 / sizeof(uint16_t)); // 4K bytes
	EPUK.resize(size);

	for (uint32_t i = 0; i < size; ++i) {
		EPUK[i] = rand();
	}

	initializeXoredEPUK_OTP();
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

	uint32_t elementsIn1024bits = k1 / (sizeof(uint16_t) * 8);
	uint32_t start = elementsIn1024bits*3;
	uint32_t OTPsectionCount = 0;
	chrono::duration<double, nano> duration = chrono::duration<double, nano>();

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	{
		Timer timer("Time taken for first 4 replace and fold cycles on 1024-bit input : ", duration);
		replaceFoldCycles(input, start, start+elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), start) == 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()+start+elementsIn1024bits), reinterpret_cast<const char*>(original.data()+start+elementsIn1024bits), size-start-elementsIn1024bits) == 0);

	cout << "OTP Count: " << OTPsectionCount << endl;

	{
		Timer timer("Time taken for first 4 reverse replace and fold cycles on 1024-bit input : ", duration);
		reverseReplaceFoldCycles(input, start, start+elementsIn1024bits, 4, EPUK, OTPsectionCount);
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);


	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	{
		Timer timer("Time taken for first 4 replace and fold cycles on 4K byte input : ", duration);
		for (uint32_t i = 0; i < 32; ++i) {
			replaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
		}
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	{
		Timer timer("Time taken for first 4 reverse replace and fold cycles on 4K byte input : ", duration);
		for (int32_t i = 32 - 1; i >= 0; i -= 1) {
			reverseReplaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);


	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	{
		Timer timer("Time taken for forward super rearrangement on 4K byte input : ", duration);
		superRearrangementUpdated1(input, true);
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	{
		Timer timer("Time taken for reverse super rearrangement on 4K byte input : ", duration);
		superRearrangementUpdated1(input, false);
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);


	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	{
		Timer timer("Time taken for first 4 replace and fold cycles on 4K byte input : ", duration);
		for (uint32_t i = 0; i < 32; ++i) {
			replaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
		}
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	{
		Timer timer("Time taken for first 4 reverse replace and fold cycles on 4K byte input : ", duration);
		for (int32_t i = 32 - 1; i >= 0; i -= 1) {
			reverseReplaceFoldCycles(input, i*elementsIn1024bits, (i+1)*elementsIn1024bits, 4, EPUK, OTPsectionCount);
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << endl << endl;

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

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	{
		Timer timer("Time taken for encryption : ", duration);
		encryption(input, EPUK, OTPsectionCount);
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	{
		Timer timer("Time taken for decryption : ", duration);
		decryption(input, EPUK, OTPsectionCount);
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << endl << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	{
		Timer timer("Time taken for encryption_1 : ", duration);
		encryption_1(input, EPUK, OTPsectionCount);
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	{
		Timer timer("Time taken for decryption_1 : ", duration);
		decryption_1(input, EPUK, OTPsectionCount);
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << endl << endl;

	for (uint32_t i = 0; i < size; ++i) {
		original[i] = 0x0000;
		input[i] = 0x0000;
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	int count = 1000000;
	uint32_t copyOTPsectionCount = 0;
	/***************************************************************************************************************
	  {
	  Timer timer("Time taken to encrypt " + to_string(count) + " times : ", duration);
	  for (int i = 0; i < count; ++i) {
	  OTPsectionCount = 0;
	  encryption(input, EPUK, OTPsectionCount);
	  }
	  }
	  assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	  copyOTPsectionCount = OTPsectionCount;
	  {
	  Timer timer("Time taken to decrypt " + to_string(count) + " times : ", duration);
	  for (int i = count-1; i >= 0; i -= 1) {
	  decryption(input, EPUK, OTPsectionCount);
	  OTPsectionCount = copyOTPsectionCount;
	  }
	  }
	  assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	 ***************************************************************************************************************/
	uint32_t k = 51;
	{
		Timer timer("Time taken to decrement: ", duration);
		int32_t i = 0xffffffff;
		uint32_t g = 0;
		while(i > 0) {
			uint32_t j = i*47;
			j = j + 1;
			g = j;	
			uint32_t* l = &g;
			*l = j;
			i -= 1;
			test = i;
		}
	}

	cout << endl << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	{
		Timer timer("Time taken to encrypt " + to_string(count) + " times using encryption_1 : ", duration);
		for (int i = 0; i < count; ++i) {
			OTPsectionCount = 0;
			encryption_1(input, EPUK, OTPsectionCount);
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	copyOTPsectionCount = OTPsectionCount;
	{
		Timer timer("Time taken to decrypt " + to_string(count) + " times using decryption_1 : ", duration);
		for (int i = count-1; i >= 0; i -= 1) {
			decryption_1(input, EPUK, OTPsectionCount);
			OTPsectionCount = copyOTPsectionCount;
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << endl << endl;

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

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	{
		Timer timer("Time taken to encrypt " + to_string(count) + " times using encryption_2 : ", duration);
		for (int i = 0; i < count; ++i) {
			OTPsectionCount = 0;
			encryption_2(input, EPUK, OTPsectionCount);
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	copyOTPsectionCount = OTPsectionCount;
	{
		Timer timer("Time taken to decrypt " + to_string(count) + " times using decryption_2 : ", duration);
		for (int i = count-1; i >= 0; i -= 1) {
			decryption_2(input, EPUK, OTPsectionCount);
			OTPsectionCount = copyOTPsectionCount;
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);



	return 0;
}

