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

vector<uint16_t> substitutionTable;
vector<vector<uint16_t>> OTPs;
vector<uint16_t> superRearrangementTable;
vector<uint16_t> reverseSuperRearrangementTable;

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

void initializeSubstitutionTable() {
	substitutionTable.resize(k64);
	for(uint16_t i = 0; i < k64 / 2; ++i) {
		substitutionTable[i] = (i + (k64/2));
		substitutionTable[i + (k64/2)] = i;
	}
}

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
	DASSERT((end - start) == (k1 / (sizeof(uint16_t) * 8))); // input must be 1024 bits long

	DASSERT(numRounds == 2 || numRounds == 4);
	//cout << "Encryption input " << endl;
	//printVector(input, 56, 65);

	//	chrono::duration<double, nano> duration;
	int round = 1;
	{

		// Process 1st section of 128 bits
		uint16_t* bptr1 = input.data()+start;

		uint16_t va1 = substitutionTable[*SA(0)];
		uint16_t va2 = substitutionTable[*SA(1)];
		uint16_t va3 = substitutionTable[*SA(2)];
		uint16_t va4 = substitutionTable[*SA(3)];

		//cout << hex << "Substutited va1 to get new va1 : " << va1 << endl;

		va2 ^= va1;
		va3 ^= va2;
		va4 ^= va3;

		uint16_t va5 = substitutionTable[*SA(4)];
		uint16_t va6 = substitutionTable[*SA(5)];
		uint16_t va7 = substitutionTable[*SA(6)];
		uint16_t va8 = substitutionTable[*SA(7)];

		va5 ^= va4;
		va6 ^= va5;
		va7 ^= va6;
		va8 ^= va7;
		va1 ^= va8; //Circular-xor across section


		uint64_t* ukptr1 = reinterpret_cast<uint64_t *>(EPUK.data()+start);

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
		uint16_t vb1 = substitutionTable[*SB(0)];
		uint16_t vb2 = substitutionTable[*SB(1)];
		uint16_t vb3 = substitutionTable[*SB(2)];
		uint16_t vb4 = substitutionTable[*SB(3)];

		vb2 ^= vb1;
		vb3 ^= vb2;
		vb4 ^= vb3;

		uint16_t vb5 = substitutionTable[*SB(4)];
		uint16_t vb6 = substitutionTable[*SB(5)];
		uint16_t vb7 = substitutionTable[*SB(6)];
		uint16_t vb8 = substitutionTable[*SB(7)];

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
		uint16_t vc1 = substitutionTable[*SC(0)];
		uint16_t vc2 = substitutionTable[*SC(1)];
		uint16_t vc3 = substitutionTable[*SC(2)];
		uint16_t vc4 = substitutionTable[*SC(3)];

		vc2 ^= vc1;
		vc3 ^= vc2;
		vc4 ^= vc3;

		uint16_t vc5 = substitutionTable[*SC(4)];
		uint16_t vc6 = substitutionTable[*SC(5)];
		uint16_t vc7 = substitutionTable[*SC(6)];
		uint16_t vc8 = substitutionTable[*SC(7)];

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
		uint16_t vd1 = substitutionTable[*SD(0)];
		uint16_t vd2 = substitutionTable[*SD(1)];
		uint16_t vd3 = substitutionTable[*SD(2)];
		uint16_t vd4 = substitutionTable[*SD(3)];

		vd2 ^= vd1;
		vd3 ^= vd2;
		vd4 ^= vd3;

		uint16_t vd5 = substitutionTable[*SD(4)];
		uint16_t vd6 = substitutionTable[*SD(5)];
		uint16_t vd7 = substitutionTable[*SD(6)];
		uint16_t vd8 = substitutionTable[*SD(7)];

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
		uint16_t ve1 = substitutionTable[*SE(0)];
		uint16_t ve2 = substitutionTable[*SE(1)];
		uint16_t ve3 = substitutionTable[*SE(2)];
		uint16_t ve4 = substitutionTable[*SE(3)];


		ve2 ^= ve1;
		ve3 ^= ve2;
		ve4 ^= ve3;

		uint16_t ve5 = substitutionTable[*SE(4)];
		uint16_t ve6 = substitutionTable[*SE(5)];
		uint16_t ve7 = substitutionTable[*SE(6)];
		uint16_t ve8 = substitutionTable[*SE(7)];

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
		uint16_t vf1 = substitutionTable[*SF(0)];
		uint16_t vf2 = substitutionTable[*SF(1)];
		uint16_t vf3 = substitutionTable[*SF(2)];
		uint16_t vf4 = substitutionTable[*SF(3)];

		vf2 ^= vf1;
		vf3 ^= vf2;
		vf4 ^= vf3;

		uint16_t vf5 = substitutionTable[*SF(4)];
		uint16_t vf6 = substitutionTable[*SF(5)];
		uint16_t vf7 = substitutionTable[*SF(6)];
		uint16_t vf8 = substitutionTable[*SF(7)];

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
		uint16_t vg1 = substitutionTable[*SG(0)];
		uint16_t vg2 = substitutionTable[*SG(1)];
		uint16_t vg3 = substitutionTable[*SG(2)];
		uint16_t vg4 = substitutionTable[*SG(3)];

		vg2 ^= vg1;
		vg3 ^= vg2;
		vg4 ^= vg3;

		uint16_t vg5 = substitutionTable[*SG(4)];
		uint16_t vg6 = substitutionTable[*SG(5)];
		uint16_t vg7 = substitutionTable[*SG(6)];
		uint16_t vg8 = substitutionTable[*SG(7)];

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
		uint16_t vh1 = substitutionTable[*SH(0)];
		uint16_t vh2 = substitutionTable[*SH(1)];
		uint16_t vh3 = substitutionTable[*SH(2)];
		uint16_t vh4 = substitutionTable[*SH(3)];

		vh2 ^= vh1;
		vh3 ^= vh2;
		vh4 ^= vh3;

		uint16_t vh5 = substitutionTable[*SH(4)];
		//cout << "1ENCRYPTION: starting vh5: " << hex << *SH(5) << " and substituted to: " << vh5 << dec << endl;
		uint16_t vh6 = substitutionTable[*SH(5)];
		//cout << "1ENCRYPTION: starting vh6: " << hex << *SH(6) << " and substituted to: " << vh6 << dec << endl;
		uint16_t vh7 = substitutionTable[*SH(6)];
		//cout << "1ENCRYPTION: starting vh7: " << hex << *SH(7) << " and substituted to: " << vh7 << dec << endl;
		uint16_t vh8 = substitutionTable[*SH(7)];
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

		va1 = substitutionTable[va1];
		va2 = substitutionTable[va2];
		va3 = substitutionTable[va3];
		va4 = substitutionTable[va4];

		//cout << hex << "Substitution to get new va1 : " << va1  << va1;

		va5 = substitutionTable[va5];
		va6 = substitutionTable[va6];
		va7 = substitutionTable[va7];
		va8 = substitutionTable[va8];

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

		vb1 = substitutionTable[vb1];
		vb2 = substitutionTable[vb2];
		vb3 = substitutionTable[vb3];
		vb4 = substitutionTable[vb4];

		vb5 = substitutionTable[vb5];
		vb6 = substitutionTable[vb6];
		vb7 = substitutionTable[vb7];
		vb8 = substitutionTable[vb8];

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

		vc1 = substitutionTable[vc1];
		vc2 = substitutionTable[vc2];
		vc3 = substitutionTable[vc3];
		vc4 = substitutionTable[vc4];

		vc5 = substitutionTable[vc5];
		vc6 = substitutionTable[vc6];
		vc7 = substitutionTable[vc7];
		vc8 = substitutionTable[vc8];


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

		vd1 = substitutionTable[vd1];
		vd2 = substitutionTable[vd2];
		vd3 = substitutionTable[vd3];
		vd4 = substitutionTable[vd4];

		vd5 = substitutionTable[vd5];
		vd6 = substitutionTable[vd6];
		vd7 = substitutionTable[vd7];
		vd8 = substitutionTable[vd8];

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

		ve1 = substitutionTable[ve1];
		ve2 = substitutionTable[ve2];
		ve3 = substitutionTable[ve3];
		ve4 = substitutionTable[ve4];

		ve5 = substitutionTable[ve5];
		ve6 = substitutionTable[ve6];
		ve7 = substitutionTable[ve7];
		ve8 = substitutionTable[ve8];

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

		vf1 = substitutionTable[vf1];
		vf2 = substitutionTable[vf2];
		vf3 = substitutionTable[vf3];
		vf4 = substitutionTable[vf4];

		vf5 = substitutionTable[vf5];
		vf6 = substitutionTable[vf6];
		vf7 = substitutionTable[vf7];
		vf8 = substitutionTable[vf8];

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

		vg1 = substitutionTable[vg1];
		vg2 = substitutionTable[vg2];
		vg3 = substitutionTable[vg3];
		vg4 = substitutionTable[vg4];

		vg5 = substitutionTable[vg5];
		vg6 = substitutionTable[vg6];
		vg7 = substitutionTable[vg7];
		vg8 = substitutionTable[vg8];

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

		vh1 = substitutionTable[vh1];
		vh2 = substitutionTable[vh2];
		vh3 = substitutionTable[vh3];
		vh4 = substitutionTable[vh4];

		vh5 = substitutionTable[vh5];
		//cout << "1DECRYPTION: substituted vh5 to get final vh5 : " << hex << vh5 << endl;
		vh6 = substitutionTable[vh6];
		//cout << "1DECRYPTION: substituted vh7 to get final vh6 : " << hex << vh6 << endl;
		vh7 = substitutionTable[vh7];
		//cout << "1DECRYPTION: substituted vh7 to get final vh7 : " << hex << vh7 << endl;
		vh8 = substitutionTable[vh8];
		//cout << "1DECRYPTION: substituted vh8 to get final vh8 : " << hex << vh8 << endl;

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

			va1 = substitutionTable[va1];
			va2 = substitutionTable[va2];
			va3 = substitutionTable[va3];
			va4 = substitutionTable[va4];
			va5 = substitutionTable[va5];
			va6 = substitutionTable[va6];
			va7 = substitutionTable[va7];
			va8 = substitutionTable[va8];

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

			vb1 = substitutionTable[vb1];
			vb2 = substitutionTable[vb2];
			vb3 = substitutionTable[vb3];
			vb4 = substitutionTable[vb4];
			vb5 = substitutionTable[vb5];
			vb6 = substitutionTable[vb6];
			vb7 = substitutionTable[vb7];
			vb8 = substitutionTable[vb8];

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

			vc1 = substitutionTable[vc1];
			vc2 = substitutionTable[vc2];
			vc3 = substitutionTable[vc3];
			vc4 = substitutionTable[vc4];
			vc5 = substitutionTable[vc5];
			vc6 = substitutionTable[vc6];
			vc7 = substitutionTable[vc7];
			vc8 = substitutionTable[vc8];

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

			vd1 = substitutionTable[vd1];
			vd2 = substitutionTable[vd2];
			vd3 = substitutionTable[vd3];
			vd4 = substitutionTable[vd4];
			vd5 = substitutionTable[vd5];
			vd6 = substitutionTable[vd6];
			vd7 = substitutionTable[vd7];
			vd8 = substitutionTable[vd8];

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

			ve1 = substitutionTable[ve1];
			ve2 = substitutionTable[ve2];
			ve3 = substitutionTable[ve3];
			ve4 = substitutionTable[ve4];
			ve5 = substitutionTable[ve5];
			ve6 = substitutionTable[ve6];
			ve7 = substitutionTable[ve7];
			ve8 = substitutionTable[ve8];

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

			vf1 = substitutionTable[vf1];
			vf2 = substitutionTable[vf2];
			vf3 = substitutionTable[vf3];
			vf4 = substitutionTable[vf4];
			vf5 = substitutionTable[vf5];
			vf6 = substitutionTable[vf6];
			vf7 = substitutionTable[vf7];
			vf8 = substitutionTable[vf8];

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

			vg1 = substitutionTable[vg1];
			vg2 = substitutionTable[vg2];
			vg3 = substitutionTable[vg3];
			vg4 = substitutionTable[vg4];
			vg5 = substitutionTable[vg5];
			vg6 = substitutionTable[vg6];
			vg7 = substitutionTable[vg7];
			vg8 = substitutionTable[vg8];

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

			vh1 = substitutionTable[vh1];
			vh2 = substitutionTable[vh2];
			vh3 = substitutionTable[vh3];
			vh4 = substitutionTable[vh4];
			vh5 = substitutionTable[vh5];
			//cout << "2DECRYPTION: substituted vh5 to get final vh5 : " << hex << vh5 << endl;
			vh6 = substitutionTable[vh6];
			//cout << "2DECRYPTION: substituted vh7 to get final vh6 : " << hex << vh6 << endl;
			vh7 = substitutionTable[vh7];
			//cout << "2DECRYPTION: substituted vh7 to get final vh7 : " << hex << vh7 << endl;
			vh8 = substitutionTable[vh8];
			//cout << "2DECRYPTION: substituted vh8 to get final vh8 : " << hex << vh8 << endl;


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

void initialize() {
	initializeSubstitutionTable();
	initializeOTPsections();
	initializeSuperRearrangementTable();
}


int main() {
	initialize();

	vector<uint16_t> original;
	vector<uint16_t> EPUK;
	uint32_t size = k4 / (sizeof(uint16_t)); // 4k bytes
	original.resize(size);
	EPUK.resize(size);

	for (uint32_t i = 0; i < size; ++i) {
		original[i] = rand();
		EPUK[i] = rand();
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
	{
		Timer timer("Time taken to encrypt " + to_string(count) + " times : ", duration);
		for (int i = 0; i < count; ++i) {
			OTPsectionCount = 0;
			encryption(input, EPUK, OTPsectionCount);
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	uint32_t copyOTPsectionCount = OTPsectionCount;
	{
		Timer timer("Time taken to decrypt " + to_string(count) + " times : ", duration);
		for (int i = count-1; i >= 0; i -= 1) {
			decryption(input, EPUK, OTPsectionCount);
			OTPsectionCount = copyOTPsectionCount;
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

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


	return 0;
}

