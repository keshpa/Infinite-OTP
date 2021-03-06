#include <assert.h>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <vector>

using namespace std;

constexpr uint32_t k1 = 1024;
constexpr uint32_t k4 = 4*k1;
constexpr uint32_t k32 = 32*k1;
constexpr uint32_t k64 = 64*k1;
constexpr uint32_t elementsIn1024b = k1 / (sizeof(uint8_t) * 8);

vector<uint32_t> rearrangementTable;
vector<uint16_t> superRearrangementTable;
vector<uint16_t> reverseSuperRearrangementTable;

void initializeRearrangementTable();
uint32_t getNewIndex(uint32_t j);
void initializeSuperRearrangementTable();
uint16_t getSuperIndex(uint16_t j, uint16_t i);
vector<uint8_t> extendedKey(vector<uint8_t> PUK);

static constexpr bool CHECKASSERT = false;  
/**  
 * Used to disable/enable asserts based on the value of CHECKASSERT
 */ 
#define DASSERT(cond) if (CHECKASSERT) { assert(cond); } 

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

vector<uint64_t> substitutionTable;

void initSubstitutionTable() {
	substitutionTable.resize(k64);
	for(uint32_t i = 0; i < k64 / 2; ++i) {
		// TODO Replace this with Galois field multiplication
		substitutionTable[i] = uint16_t(i + (k64 / 2));
		substitutionTable[i + (k64 / 2)] = uint16_t(i);
	}
}

vector<vector<uint16_t>> OTPs;
uint32_t sectionCount = k64;
uint32_t sectionSize = 64 / sizeof(uint16_t); // The size of each OTP section is only 64 bytes.


void initOTPsections() {
	OTPs.resize(sectionCount);
	srand(0);
	for (uint32_t i = 0; i < sectionCount; ++i) {
		OTPs[i].resize(sectionSize);
		for (uint32_t j = 0; j < sectionSize; ++j) {
			OTPs[i][j] = rand();
		}
	}
}

inline void apply(vector<uint8_t>& input, vector<uint8_t>& EPUK, uint32_t index1, uint32_t index2, uint32_t OTPind1, uint32_t OTPind2) {
	uint64_t* pointer1 = reinterpret_cast<uint64_t*>(input.data() + index1);
	uint64_t* pointer2 = reinterpret_cast<uint64_t*>(input.data() + index2);
	uint64_t* EPUKpointer1 = reinterpret_cast<uint64_t*>(EPUK.data() + index1);
	uint64_t* EPUKpointer2 = reinterpret_cast<uint64_t*>(EPUK.data() + index2);
	for (uint32_t i = 0; i < sectionSize; ++i) {
		uint64_t OTP1 = OTPs[OTPind1][i];
		uint64_t OTP2 = OTPs[OTPind2][i];
		*(pointer1 + i) = *(pointer1 + i) ^ OTP1 ^ *(EPUKpointer1 + i);
		*(pointer2 + i) = *(pointer2 + i) ^ OTP2 ^ *(EPUKpointer2 + i);
	}
}

void initialize() {
	initOTPsections();
	initSubstitutionTable();
	initializeRearrangementTable();
	initializeSuperRearrangementTable();
}

void printVector(vector<uint8_t>& input, uint32_t start, uint32_t end) {
	cout << "Vector : ";
	for (uint32_t i = start; i < end; ++i) {
		uint8_t value = input[i];
		cout << hex << setw(3) << (uint16_t)value << dec << " "; 
	}
	cout << endl; 
}

void rotateRight(vector<uint8_t>& input, int32_t startingInd, uint32_t rotateBetweenBits) {
	uint32_t length = rotateBetweenBits / (sizeof(uint8_t) * 8);	// Divide by 8 to convert into bytes and then sizeof(uint8_t)
	uint8_t* pointer = reinterpret_cast<uint8_t*>(input.data());
	pointer += startingInd * sizeof(uint8_t);
	uint8_t lastByte = *(pointer+length-1);
	memmove(pointer + 1, pointer, (rotateBetweenBits/8) - 1);
	*pointer = lastByte;
}

void rotateLeft(vector<uint8_t>& input, int32_t startingInd, uint32_t rotateBetweenBits) {
	uint32_t length = rotateBetweenBits / (sizeof(uint8_t) * 8);
	uint8_t* pointer = reinterpret_cast<uint8_t*>(input.data());
	pointer += startingInd * sizeof(uint8_t);
	uint8_t firstByte = *pointer;
	memmove(pointer, pointer + 1, (rotateBetweenBits/8) - 1);
	*(pointer + length - 1) = firstByte;
}

// TODO: Take care of little-Endian/big-Endian
void reverseSubstitution(vector<uint8_t>& input, uint32_t start = 0, uint32_t end = 0, int numRounds = 4) {
	if (end == 0) end = input.size();
	DASSERT((end - start) == (k1 / (sizeof(uint8_t) * 8))); // input must be 1024 bits long

	bool skipRotate;
	bool skipRearrangement;

	chrono::duration<double, nano> duration;

	{
		/****************************************************************
		  string str("Time to reverse substitute 128 byte portion ");
		  if (skipRotate) {
		  str += "without circular-xor, ";
		  } else {
		  str += "withcircular-xor, ";
		  }
		  if (skipRearrangement) {
		  str += " and without rearrangement ";
		  } else {
		  str += " and with rearrangement ";
		  }
		  Timer timer1(str, duration);
		 ****************************************************************/

		// Process 1st section of 128 bits
		uint64_t* bptr1 = reinterpret_cast<uint64_t *>(input.data()+start);
		uint16_t* sa1 = reinterpret_cast<uint16_t *>(bptr1);
		uint16_t* sa2 = reinterpret_cast<uint16_t *>(bptr1)+1;
		uint16_t* sa3 = reinterpret_cast<uint16_t *>(bptr1)+2;
		uint16_t* sa4 = reinterpret_cast<uint16_t *>(bptr1)+3;

		uint16_t va1 = *sa1;
		uint16_t va2 = *sa2;
		uint16_t va3 = *sa3;
		uint16_t va4 = *sa4;

		uint64_t* bptr2 = reinterpret_cast<uint64_t *>(input.data()+start+8);
		uint16_t* sa5 = reinterpret_cast<uint16_t *>(bptr2);
		uint16_t* sa6 = reinterpret_cast<uint16_t *>(bptr2)+1;
		uint16_t* sa7 = reinterpret_cast<uint16_t *>(bptr2)+2;
		uint16_t* sa8 = reinterpret_cast<uint16_t *>(bptr2)+3;

		uint16_t va5 = *sa5;
		uint16_t va6 = *sa6;
		uint16_t va7 = *sa7;
		uint16_t va8 = *sa8;

		if (not skipRotate) {
			va1 ^= va8; //Circular-xor across section
		}
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

		if (skipRearrangement) {
			*sa1 = va1;
			*sa2 = va2;
			*sa3 = va3;
			*sa4 = va4;
			*sa5 = va5;
			*sa6 = va6;
			*sa7 = va7;
			*sa8 = va8;
		}

		// Process 2nd section of 128 bits
		uint64_t* bptr3 = reinterpret_cast<uint64_t *>(input.data()+start+16);
		uint16_t* sb1 = reinterpret_cast<uint16_t *>(bptr3);
		uint16_t* sb2 = reinterpret_cast<uint16_t *>(bptr3)+1;
		uint16_t* sb3 = reinterpret_cast<uint16_t *>(bptr3)+2;
		uint16_t* sb4 = reinterpret_cast<uint16_t *>(bptr3)+3;

		uint16_t vb1 = *sb1;
		uint16_t vb2 = *sb2;
		uint16_t vb3 = *sb3;
		uint16_t vb4 = *sb4;

		uint64_t* bptr4 = reinterpret_cast<uint64_t *>(input.data()+start+24);
		uint16_t* sb5 = reinterpret_cast<uint16_t *>(bptr4);
		uint16_t* sb6 = reinterpret_cast<uint16_t *>(bptr4)+1;
		uint16_t* sb7 = reinterpret_cast<uint16_t *>(bptr4)+2;
		uint16_t* sb8 = reinterpret_cast<uint16_t *>(bptr4)+3;

		uint16_t vb5 = *sb5;
		uint16_t vb6 = *sb6;
		uint16_t vb7 = *sb7;
		uint16_t vb8 = *sb8;

		if (not skipRotate) {
			vb1 ^= vb8; //Circular-xor across section
		}
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

		if (skipRearrangement) {
			*sb1 = vb1;
			*sb2 = vb2;
			*sb3 = vb3;
			*sb4 = vb4;
			*sb5 = vb5;
			*sb6 = vb6;
			*sb7 = vb7;
			*sb8 = vb8;
		}

		// Process 3rd section of 128 bits
		uint64_t* bptr5 = reinterpret_cast<uint64_t *>(input.data()+start+32);
		uint16_t* sc1 = reinterpret_cast<uint16_t *>(bptr5);
		uint16_t* sc2 = reinterpret_cast<uint16_t *>(bptr5)+1;
		uint16_t* sc3 = reinterpret_cast<uint16_t *>(bptr5)+2;
		uint16_t* sc4 = reinterpret_cast<uint16_t *>(bptr5)+3;

		uint16_t vc1 = *sc1;
		uint16_t vc2 = *sc2;
		uint16_t vc3 = *sc3;
		uint16_t vc4 = *sc4;

		uint64_t* bptr6 = reinterpret_cast<uint64_t *>(input.data()+start+40);
		uint16_t* sc5 = reinterpret_cast<uint16_t *>(bptr6);
		uint16_t* sc6 = reinterpret_cast<uint16_t *>(bptr6)+1;
		uint16_t* sc7 = reinterpret_cast<uint16_t *>(bptr6)+2;
		uint16_t* sc8 = reinterpret_cast<uint16_t *>(bptr6)+3;

		uint16_t vc5 = *sc5;
		uint16_t vc6 = *sc6;
		uint16_t vc7 = *sc7;
		uint16_t vc8 = *sc8;

		if (not skipRotate) {
			vc1 ^= vc8; //Circular-xor across section
		}
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

		if (skipRearrangement) {
			*sc1 = vc1;
			*sc2 = vc2;
			*sc3 = vc3;
			*sc4 = vc4;
			*sc5 = vc5;
			*sc6 = vc6;
			*sc7 = vc7;
			*sc8 = vc8;
		}

		// Process 4th section of 128 bits
		uint64_t* bptr7 = reinterpret_cast<uint64_t *>(input.data()+start+48);
		uint16_t* sd1 = reinterpret_cast<uint16_t *>(bptr7);
		uint16_t* sd2 = reinterpret_cast<uint16_t *>(bptr7)+1;
		uint16_t* sd3 = reinterpret_cast<uint16_t *>(bptr7)+2;
		uint16_t* sd4 = reinterpret_cast<uint16_t *>(bptr7)+3;

		uint16_t vd1 = *sd1;
		uint16_t vd2 = *sd2;
		uint16_t vd3 = *sd3;
		uint16_t vd4 = *sd4;

		uint64_t* bptr8 = reinterpret_cast<uint64_t *>(input.data()+start+56);
		uint16_t* sd5 = reinterpret_cast<uint16_t *>(bptr8);
		uint16_t* sd6 = reinterpret_cast<uint16_t *>(bptr8)+1;
		uint16_t* sd7 = reinterpret_cast<uint16_t *>(bptr8)+2;
		uint16_t* sd8 = reinterpret_cast<uint16_t *>(bptr8)+3;

		uint16_t vd5 = *sd5;
		uint16_t vd6 = *sd6;
		uint16_t vd7 = *sd7;
		uint16_t vd8 = *sd8;

		if (not skipRotate) {
			vd1 ^= vd8; //Circular-xor across section
		}
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

		if (skipRearrangement) {
			*sd1 = vd1;
			*sd2 = vd2;
			*sd3 = vd3;
			*sd4 = vd4;
			*sd5 = vd5;
			*sd6 = vd6;
			*sd7 = vd7;
			*sd8 = vd8;
		}

		// Process 5th section of 128 bits
		uint64_t* bptr9 = reinterpret_cast<uint64_t *>(input.data()+start+64);
		uint16_t* se1 = reinterpret_cast<uint16_t *>(bptr9);
		uint16_t* se2 = reinterpret_cast<uint16_t *>(bptr9)+1;
		uint16_t* se3 = reinterpret_cast<uint16_t *>(bptr9)+2;
		uint16_t* se4 = reinterpret_cast<uint16_t *>(bptr9)+3;

		uint16_t ve1 = *se1;
		uint16_t ve2 = *se2;
		uint16_t ve3 = *se3;
		uint16_t ve4 = *se4;

		uint64_t* bptr10 = reinterpret_cast<uint64_t *>(input.data()+start+72);
		uint16_t* se5 = reinterpret_cast<uint16_t *>(bptr10);
		uint16_t* se6 = reinterpret_cast<uint16_t *>(bptr10)+1;
		uint16_t* se7 = reinterpret_cast<uint16_t *>(bptr10)+2;
		uint16_t* se8 = reinterpret_cast<uint16_t *>(bptr10)+3;

		uint16_t ve5 = *se5;
		uint16_t ve6 = *se6;
		uint16_t ve7 = *se7;
		uint16_t ve8 = *se8;

		if (not skipRotate) {
			ve1 ^= ve8; //Circular-xor across section
		}
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

		if (skipRearrangement) {
			*se1 = ve1;
			*se2 = ve2;
			*se3 = ve3;
			*se4 = ve4;
			*se5 = ve5;
			*se6 = ve6;
			*se7 = ve7;
			*se8 = ve8;
		}

		// Process 6th section of 128 bits
		uint64_t* bptr11 = reinterpret_cast<uint64_t *>(input.data()+start+80);
		uint16_t* sf1 = reinterpret_cast<uint16_t *>(bptr11);
		uint16_t* sf2 = reinterpret_cast<uint16_t *>(bptr11)+1;
		uint16_t* sf3 = reinterpret_cast<uint16_t *>(bptr11)+2;
		uint16_t* sf4 = reinterpret_cast<uint16_t *>(bptr11)+3;

		uint16_t vf1 = *sf1;
		uint16_t vf2 = *sf2;
		uint16_t vf3 = *sf3;
		uint16_t vf4 = *sf4;

		uint64_t* bptr12 = reinterpret_cast<uint64_t *>(input.data()+start+88);
		uint16_t* sf5 = reinterpret_cast<uint16_t *>(bptr12);
		uint16_t* sf6 = reinterpret_cast<uint16_t *>(bptr12)+1;
		uint16_t* sf7 = reinterpret_cast<uint16_t *>(bptr12)+2;
		uint16_t* sf8 = reinterpret_cast<uint16_t *>(bptr12)+3;

		uint16_t vf5 = *sf5;
		uint16_t vf6 = *sf6;
		uint16_t vf7 = *sf7;
		uint16_t vf8 = *sf8;

		if (not skipRotate) {
			vf1 ^= vf8; //Circular-xor across section
		}
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

		if (skipRearrangement) {
			*sf1 = vf1;
			*sf2 = vf2;
			*sf3 = vf3;
			*sf4 = vf4;
			*sf5 = vf5;
			*sf6 = vf6;
			*sf7 = vf7;
			*sf8 = vf8;
		}

		// Process 7th section of 128 bits
		uint64_t* bptr13 = reinterpret_cast<uint64_t *>(input.data()+start+96);
		uint16_t* sg1 = reinterpret_cast<uint16_t *>(bptr13);
		uint16_t* sg2 = reinterpret_cast<uint16_t *>(bptr13)+1;
		uint16_t* sg3 = reinterpret_cast<uint16_t *>(bptr13)+2;
		uint16_t* sg4 = reinterpret_cast<uint16_t *>(bptr13)+3;

		uint16_t vg1 = *sg1;
		uint16_t vg2 = *sg2;
		uint16_t vg3 = *sg3;
		uint16_t vg4 = *sg4;

		uint64_t* bptr14 = reinterpret_cast<uint64_t *>(input.data()+start+104);
		uint16_t* sg5 = reinterpret_cast<uint16_t *>(bptr14);
		uint16_t* sg6 = reinterpret_cast<uint16_t *>(bptr14)+1;
		uint16_t* sg7 = reinterpret_cast<uint16_t *>(bptr14)+2;
		uint16_t* sg8 = reinterpret_cast<uint16_t *>(bptr14)+3;

		uint16_t vg5 = *sg5;
		uint16_t vg6 = *sg6;
		uint16_t vg7 = *sg7;
		uint16_t vg8 = *sg8;

		if (not skipRotate) {
			vg1 ^= vg8; //Circular-xor across section
		}
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

		if (skipRearrangement) {
			*sg1 = vg1;
			*sg2 = vg2;
			*sg3 = vg3;
			*sg4 = vg4;
			*sg5 = vg5;
			*sg6 = vg6;
			*sg7 = vg7;
			*sg8 = vg8;
		}

		// Process 8th section of 128 bits
		uint64_t* bptr15 = reinterpret_cast<uint64_t *>(input.data()+start+112);
		uint16_t* sh1 = reinterpret_cast<uint16_t *>(bptr15);
		uint16_t* sh2 = reinterpret_cast<uint16_t *>(bptr15)+1;
		uint16_t* sh3 = reinterpret_cast<uint16_t *>(bptr15)+2;
		uint16_t* sh4 = reinterpret_cast<uint16_t *>(bptr15)+3;

		uint16_t vh1 = *sh1;
		uint16_t vh2 = *sh2;
		uint16_t vh3 = *sh3;
		uint16_t vh4 = *sh4;

		uint64_t* bptr16 = reinterpret_cast<uint64_t *>(input.data()+start+120);
		uint16_t* sh5 = reinterpret_cast<uint16_t *>(bptr16);
		uint16_t* sh6 = reinterpret_cast<uint16_t *>(bptr16)+1;
		uint16_t* sh7 = reinterpret_cast<uint16_t *>(bptr16)+2;
		uint16_t* sh8 = reinterpret_cast<uint16_t *>(bptr16)+3;

		uint16_t vh5 = *sh5;
		uint16_t vh6 = *sh6;
		uint16_t vh7 = *sh7;
		uint16_t vh8 = *sh8;

		if (not skipRotate) {
			vh1 ^= vh8; //Circular-xor across section
		}
		vh8 ^= vh7;
		vh7 ^= vh6;
		vh6 ^= vh5;
		vh5 ^= vh4;

		vh4 ^= vh3;
		vh3 ^= vh2;
		vh2 ^= vh1;

		vh1 = substitutionTable[vh1];
		vh2 = substitutionTable[vh2];
		vh3 = substitutionTable[vh3];
		vh4 = substitutionTable[vh4];

		vh5 = substitutionTable[vh5];
		vh6 = substitutionTable[vh6];
		vh7 = substitutionTable[vh7];
		vh8 = substitutionTable[vh8];

		if (skipRearrangement) {
			*sh1 = vh1;
			*sh2 = vh2;
			*sh3 = vh3;
			*sh4 = vh4;
			*sh5 = vh5;
			*sh6 = vh6;
			*sh7 = vh7;
			*sh8 = vh8;
		}



		if (not skipRearrangement) {

		}
	}
}

void forwardSubstitution(vector<uint16_t>& input, uint32_t start = 0, uint32_t end = 0) { 
}
void forwardSubstitution(vector<uint16_t>& input, uint32_t start = 0, uint32_t end = 0, int numRounds, vector<uint16_t>& EPUK, uint32_t& startOTPSectionCount) {
	if (end == 0) end = input.size();
	DASSERT((end - start) == (k1 / (sizeof(uint16_t) * 8))); // input must be 1024 bits long

	DASSERT(numRounds == 2 || numRounds == 4);

	//	chrono::duration<double, nano> duration;
	int round = 1;
	{

		// Process 1st section of 128 bits
		uint64_t* bptr1 = reinterpret_cast<uint64_t *>(input.data()+start);
		uint16_t* sa1 = reinterpret_cast<uint16_t *>(bptr1);
		uint16_t* sa2 = reinterpret_cast<uint16_t *>(bptr1)+1;
		uint16_t* sa3 = reinterpret_cast<uint16_t *>(bptr1)+2;
		uint16_t* sa4 = reinterpret_cast<uint16_t *>(bptr1)+3;

		uint16_t va1 = substitutionTable[*sa1];
		uint16_t va2 = substitutionTable[*sa2];
		uint16_t va3 = substitutionTable[*sa3];
		uint16_t va4 = substitutionTable[*sa4];

		va2 ^= va1;
		va3 ^= va2;
		va4 ^= va3;

		uint16_t* sa5 = reinterpret_cast<uint16_t *>(bptr1)+4;
		uint16_t* sa6 = reinterpret_cast<uint16_t *>(bptr1)+5;
		uint16_t* sa7 = reinterpret_cast<uint16_t *>(bptr1)+6;
		uint16_t* sa8 = reinterpret_cast<uint16_t *>(bptr1)+7;

		uint16_t va5 = substitutionTable[*sa5];
		uint16_t va6 = substitutionTable[*sa6];
		uint16_t va7 = substitutionTable[*sa7];
		uint16_t va8 = substitutionTable[*sa8];

		va5 ^= va4;
		va6 ^= va5;
		va7 ^= va6;
		va8 ^= va7;
		va1 ^= va8; //Circular-xor across section

		uint64_t* ukptr1 = reinterpret_cast<uint64_t *>(EPUK.data()+start);

#define EPUK_SHORT(a)	*(reinterpret_cast<uint16_t *>(ukptr1)+a);

		va1 ^= EPUK_SHORT(0) ^ OTPs[EPUK[startOTPSectionCount]][0];
		va2 ^= EPUK_SHORT(1) ^ OTPs[EPUK[startOTPSectionCount]][1];
		va3 ^= EPUK_SHORT(2) ^ OTPs[EPUK[startOTPSectionCount]][2];
		va4 ^= EPUK_SHORT(3) ^ OTPs[EPUK[startOTPSectionCount]][3];
		va5 ^= EPUK_SHORT(4) ^ OTPs[EPUK[startOTPSectionCount]][4];
		va6 ^= EPUK_SHORT(5) ^ OTPs[EPUK[startOTPSectionCount]][5];
		va7 ^= EPUK_SHORT(6) ^ OTPs[EPUK[startOTPSectionCount]][6];
		va8 ^= EPUK_SHORT(7) ^ OTPs[EPUK[startOTPSectionCount]][7];

		// Process 2nd section of 128 bits
		uint16_t* sb1 = reinterpret_cast<uint16_t *>(bptr1)+8;
		uint16_t* sb2 = reinterpret_cast<uint16_t *>(bptr1)+9;
		uint16_t* sb3 = reinterpret_cast<uint16_t *>(bptr1)+10;
		uint16_t* sb4 = reinterpret_cast<uint16_t *>(bptr1)+11;


		uint16_t vb1 = substitutionTable[*sb1];
		uint16_t vb2 = substitutionTable[*sb2];
		uint16_t vb3 = substitutionTable[*sb3];
		uint16_t vb4 = substitutionTable[*sb4];

		vb2 ^= vb1;
		vb3 ^= vb2;
		vb4 ^= vb3;

		uint16_t* sb5 = reinterpret_cast<uint16_t *>(bptr1)+12;
		uint16_t* sb6 = reinterpret_cast<uint16_t *>(bptr1)+13;
		uint16_t* sb7 = reinterpret_cast<uint16_t *>(bptr1)+14;
		uint16_t* sb8 = reinterpret_cast<uint16_t *>(bptr1)+15;

		uint16_t vb5 = substitutionTable[*sb5];
		uint16_t vb6 = substitutionTable[*sb6];
		uint16_t vb7 = substitutionTable[*sb7];
		uint16_t vb8 = substitutionTable[*sb8];

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
		uint64_t* bptr5 = reinterpret_cast<uint64_t *>(input.data()+start+32);
		uint16_t* sc1 = reinterpret_cast<uint16_t *>(bptr5);
		uint16_t* sc2 = reinterpret_cast<uint16_t *>(bptr5)+1;
		uint16_t* sc3 = reinterpret_cast<uint16_t *>(bptr5)+2;
		uint16_t* sc4 = reinterpret_cast<uint16_t *>(bptr5)+3;


		uint16_t vc1 = substitutionTable[*sc1];
		uint16_t vc2 = substitutionTable[*sc2];
		uint16_t vc3 = substitutionTable[*sc3];
		uint16_t vc4 = substitutionTable[*sc4];

		vc2 ^= vc1;
		vc3 ^= vc2;
		vc4 ^= vc3;

		uint64_t* bptr6 = reinterpret_cast<uint64_t *>(input.data()+start+40);
		uint16_t* sc5 = reinterpret_cast<uint16_t *>(bptr6);
		uint16_t* sc6 = reinterpret_cast<uint16_t *>(bptr6)+1;
		uint16_t* sc7 = reinterpret_cast<uint16_t *>(bptr6)+2;
		uint16_t* sc8 = reinterpret_cast<uint16_t *>(bptr6)+3;

		uint16_t vc5 = substitutionTable[*sc5];
		uint16_t vc6 = substitutionTable[*sc6];
		uint16_t vc7 = substitutionTable[*sc7];
		uint16_t vc8 = substitutionTable[*sc8];

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

		// Process 4rd section of 128 bits
		uint64_t* bptr7 = reinterpret_cast<uint64_t *>(input.data()+start+48);
		uint16_t* sd1 = reinterpret_cast<uint16_t *>(bptr7);
		uint16_t* sd2 = reinterpret_cast<uint16_t *>(bptr7)+1;
		uint16_t* sd3 = reinterpret_cast<uint16_t *>(bptr7)+2;
		uint16_t* sd4 = reinterpret_cast<uint16_t *>(bptr7)+3;


		uint16_t vd1 = substitutionTable[*sd1];
		uint16_t vd2 = substitutionTable[*sd2];
		uint16_t vd3 = substitutionTable[*sd3];
		uint16_t vd4 = substitutionTable[*sd4];

		vd2 ^= vd1;
		vd3 ^= vd2;
		vd4 ^= vd3;

		uint64_t* bptr8 = reinterpret_cast<uint64_t *>(input.data()+start+56);
		uint16_t* sd5 = reinterpret_cast<uint16_t *>(bptr8);
		uint16_t* sd6 = reinterpret_cast<uint16_t *>(bptr8)+1;
		uint16_t* sd7 = reinterpret_cast<uint16_t *>(bptr8)+2;
		uint16_t* sd8 = reinterpret_cast<uint16_t *>(bptr8)+3;

		uint16_t vd5 = substitutionTable[*sd5];
		uint16_t vd6 = substitutionTable[*sd6];
		uint16_t vd7 = substitutionTable[*sd7];
		uint16_t vd8 = substitutionTable[*sd8];

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
		uint64_t* bptr9 = reinterpret_cast<uint64_t *>(input.data()+start+64);
		uint16_t* se1 = reinterpret_cast<uint16_t *>(bptr9);
		uint16_t* se2 = reinterpret_cast<uint16_t *>(bptr9)+1;
		uint16_t* se3 = reinterpret_cast<uint16_t *>(bptr9)+2;
		uint16_t* se4 = reinterpret_cast<uint16_t *>(bptr9)+3;


		uint16_t ve1 = substitutionTable[*se1];
		uint16_t ve2 = substitutionTable[*se2];
		uint16_t ve3 = substitutionTable[*se3];
		uint16_t ve4 = substitutionTable[*se4];

		ve2 ^= ve1;
		ve3 ^= ve2;
		ve4 ^= ve3;

		uint64_t* bptr10 = reinterpret_cast<uint64_t *>(input.data()+start+72);
		uint16_t* se5 = reinterpret_cast<uint16_t *>(bptr10);
		uint16_t* se6 = reinterpret_cast<uint16_t *>(bptr10)+1;
		uint16_t* se7 = reinterpret_cast<uint16_t *>(bptr10)+2;
		uint16_t* se8 = reinterpret_cast<uint16_t *>(bptr10)+3;

		uint16_t ve5 = substitutionTable[*se5];
		uint16_t ve6 = substitutionTable[*se6];
		uint16_t ve7 = substitutionTable[*se7];
		uint16_t ve8 = substitutionTable[*se8];

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
		uint64_t* bptr11= reinterpret_cast<uint64_t *>(input.data()+start+80);
		uint16_t* sf1 = reinterpret_cast<uint16_t *>(bptr11);
		uint16_t* sf2 = reinterpret_cast<uint16_t *>(bptr11)+1;
		uint16_t* sf3 = reinterpret_cast<uint16_t *>(bptr11)+2;
		uint16_t* sf4 = reinterpret_cast<uint16_t *>(bptr11)+3;


		uint16_t vf1 = substitutionTable[*sf1];
		uint16_t vf2 = substitutionTable[*sf2];
		uint16_t vf3 = substitutionTable[*sf3];
		uint16_t vf4 = substitutionTable[*sf4];

		vf2 ^= vf1;
		vf3 ^= vf2;
		vf4 ^= vf3;

		uint64_t* bptr12 = reinterpret_cast<uint64_t *>(input.data()+start+88);
		uint16_t* sf5 = reinterpret_cast<uint16_t *>(bptr12);
		uint16_t* sf6 = reinterpret_cast<uint16_t *>(bptr12)+1;
		uint16_t* sf7 = reinterpret_cast<uint16_t *>(bptr12)+2;
		uint16_t* sf8 = reinterpret_cast<uint16_t *>(bptr12)+3;

		uint16_t vf5 = substitutionTable[*sf5];
		uint16_t vf6 = substitutionTable[*sf6];
		uint16_t vf7 = substitutionTable[*sf7];
		uint16_t vf8 = substitutionTable[*sf8];

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
		uint64_t* bptr13= reinterpret_cast<uint64_t *>(input.data()+start+96);
		uint16_t* sg1 = reinterpret_cast<uint16_t *>(bptr13);
		uint16_t* sg2 = reinterpret_cast<uint16_t *>(bptr13)+1;
		uint16_t* sg3 = reinterpret_cast<uint16_t *>(bptr13)+2;
		uint16_t* sg4 = reinterpret_cast<uint16_t *>(bptr13)+3;


		uint16_t vg1 = substitutionTable[*sg1];
		uint16_t vg2 = substitutionTable[*sg2];
		uint16_t vg3 = substitutionTable[*sg3];
		uint16_t vg4 = substitutionTable[*sg4];

		vg2 ^= vg1;
		vg3 ^= vg2;
		vg4 ^= vg3;

		uint64_t* bptr14 = reinterpret_cast<uint64_t *>(input.data()+start+104);
		uint16_t* sg5 = reinterpret_cast<uint16_t *>(bptr14);
		uint16_t* sg6 = reinterpret_cast<uint16_t *>(bptr14)+1;
		uint16_t* sg7 = reinterpret_cast<uint16_t *>(bptr14)+2;
		uint16_t* sg8 = reinterpret_cast<uint16_t *>(bptr14)+3;

		uint16_t vg5 = substitutionTable[*sg5];
		uint16_t vg6 = substitutionTable[*sg6];
		uint16_t vg7 = substitutionTable[*sg7];
		uint16_t vg8 = substitutionTable[*sg8];

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
		uint64_t* bptr15= reinterpret_cast<uint64_t *>(input.data()+start+112);
		uint16_t* sh1 = reinterpret_cast<uint16_t *>(bptr15);
		uint16_t* sh2 = reinterpret_cast<uint16_t *>(bptr15)+1;
		uint16_t* sh3 = reinterpret_cast<uint16_t *>(bptr15)+2;
		uint16_t* sh4 = reinterpret_cast<uint16_t *>(bptr15)+3;


		uint16_t vh1 = substitutionTable[*sh1];
		uint16_t vh2 = substitutionTable[*sh2];
		uint16_t vh3 = substitutionTable[*sh3];
		uint16_t vh4 = substitutionTable[*sh4];

		vh2 ^= vh1;
		vh3 ^= vh2;
		vh4 ^= vh3;

		uint64_t* bptr16 = reinterpret_cast<uint64_t *>(input.data()+start+120);
		uint16_t* sh5 = reinterpret_cast<uint16_t *>(bptr16);
		uint16_t* sh6 = reinterpret_cast<uint16_t *>(bptr16)+1;
		uint16_t* sh7 = reinterpret_cast<uint16_t *>(bptr16)+2;
		uint16_t* sh8 = reinterpret_cast<uint16_t *>(bptr16)+3;

		uint16_t vh5 = substitutionTable[*sh5];
		uint16_t vh6 = substitutionTable[*sh6];
		uint16_t vh7 = substitutionTable[*sh7];
		uint16_t vh8 = substitutionTable[*sh8];

		vh5 ^= vh4;
		vh6 ^= vh5;
		vh7 ^= vh6;
		vh8 ^= vh7;
		vh1 ^= vh8; //Circular-xor across section

		vh1 ^= EPUK_SHORT(56) ^ OTPs[EPUK[startOTPSectionCount]][14];
		vh2 ^= EPUK_SHORT(57) ^ OTPs[EPUK[startOTPSectionCount]][15];
		vh3 ^= EPUK_SHORT(58) ^ OTPs[EPUK[startOTPSectionCount]][16];
		vh4 ^= EPUK_SHORT(59) ^ OTPs[EPUK[startOTPSectionCount]][17];
		vh5 ^= EPUK_SHORT(60) ^ OTPs[EPUK[startOTPSectionCount]][28];
		vh6 ^= EPUK_SHORT(61) ^ OTPs[EPUK[startOTPSectionCount]][29];
		vh7 ^= EPUK_SHORT(62) ^ OTPs[EPUK[startOTPSectionCount]][30];
		vh8 ^= EPUK_SHORT(63) ^ OTPs[EPUK[startOTPSectionCount]][31];
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
			vg3 ^= EPUK_SHORT(40) ^ OTPs[EPUK[startOTPSectionCount]][18];
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
			vh7 = substitutionTable[vh7];
			vh8 = substitutionTable[vh8];
			vh5 ^= vh4;
			vh6 ^= vh5;
			vh7 ^= vh6;
			vh8 ^= vh7;
			vh1 ^= vh8; //Circular-xor across section

			vh1 ^= EPUK_SHORT(56) ^ OTPs[EPUK[startOTPSectionCount]][14];
			vh2 ^= EPUK_SHORT(57) ^ OTPs[EPUK[startOTPSectionCount]][15];
			vh3 ^= EPUK_SHORT(58) ^ OTPs[EPUK[startOTPSectionCount]][16];
			vh4 ^= EPUK_SHORT(59) ^ OTPs[EPUK[startOTPSectionCount]][17];
			vh5 ^= EPUK_SHORT(50) ^ OTPs[EPUK[startOTPSectionCount]][28];
			vh6 ^= EPUK_SHORT(61) ^ OTPs[EPUK[startOTPSectionCount]][29];
			vh7 ^= EPUK_SHORT(62) ^ OTPs[EPUK[startOTPSectionCount]][30];
			vh8 ^= EPUK_SHORT(63) ^ OTPs[EPUK[startOTPSectionCount]][31];
			++startOTPSectionCount;

			if (round == 2 && numRounds == 2) {
				// Reflect rearrangement in memory
				*sa1 = va1;
				*sa2 = vb1;
				*sa3 = vc1;
				*sa4 = vd1;
				*sa5 = ve1;
				*sa6 = vf1;
				*sa7 = vg1;
				*sa8 = vh1;

				*sb1 = va2;
				*sb2 = vb2;
				*sb3 = vc2;
				*sb4 = vd2;
				*sb5 = ve2;
				*sb6 = vf2;
				*sb7 = vg2;
				*sb8 = vh2;

				*sc1 = va3;
				*sc2 = vb3;
				*sc3 = vc3;
				*sc4 = vd3;
				*sc5 = ve3;
				*sc6 = vf3;
				*sc7 = vg3;
				*sc8 = vh3;

				*sd1 = va4;
				*sd2 = vb4;
				*sd3 = vc4;
				*sd4 = vd4;
				*sd5 = ve4;
				*sd6 = vf4;
				*sd7 = vg4;
				*sd8 = vh4;

				*se1 = va5;
				*se2 = vb5;
				*se3 = vc5;
				*se4 = vd5;
				*se5 = ve5;
				*se6 = vf5;
				*se7 = vg5;
				*se8 = vh5;

				*sf1 = va6;
				*sf2 = vb6;
				*sf3 = vc6;
				*sf4 = vd6;
				*sf5 = ve6;
				*sf6 = vf6;
				*sf7 = vg6;
				*sf8 = vh6;

				*sg1 = va7;
				*sg2 = vb7;
				*sg3 = vc7;
				*sg4 = vd7;
				*sg5 = ve7;
				*sg6 = vf7;
				*sg7 = vg7;
				*sg8 = vh7;

				*sh1 = va8;
				*sh2 = vb8;
				*sh3 = vc8;
				*sh4 = vd8;
				*sh5 = ve8;
				*sh6 = vf8;
				*sh7 = vg8;
				*sh8 = vh8;

				return;
			} else if (round == 2 && numRounds == 4) {
				// Reflect rearrangement in memory
#define SWAP_SHORTS(a, b)			\
				do {						\
					uint16_t temp = a;			\
					a = b;					\
					b = temp;				\
				} while(0)
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
				SWAP_SHORTS(vf8, vh7);

				vh8 = vh8;
			}
		}

		*sa1 = va1;
		*sa2 = va2;
		*sa3 = va3;
		*sa4 = va4;
		*sa5 = va5;
		*sa6 = va6;
		*sa7 = va7;
		*sa8 = va8;

		*sb1 = vb1;
		*sb2 = vb2;
		*sb3 = vb3;
		*sb4 = vb4;
		*sb5 = vb5;
		*sb6 = vb6;
		*sb7 = vb7;
		*sb8 = vb8;

		*sc1 = vc1;
		*sc2 = vc2;
		*sc3 = vc3;
		*sc4 = vc4;
		*sc5 = vc5;
		*sc6 = vc6;
		*sc7 = vc7;
		*sc8 = vc8;

		*sd1 = vd1;
		*sd2 = vd2;
		*sd3 = vd3;
		*sd4 = vd4;
		*sd5 = vd5;
		*sd6 = vd6;
		*sd7 = vd7;
		*sd8 = vd8;

		*se1 = ve1;
		*se2 = ve2;
		*se3 = ve3;
		*se4 = ve4;
		*se5 = ve5;
		*se6 = ve6;
		*se7 = ve7;
		*se8 = ve8;

		*sf1 = vf1;
		*sf2 = vf2;
		*sf3 = vf3;
		*sf4 = vf4;
		*sf5 = vf5;
		*sf6 = vf6;
		*sf7 = vf7;
		*sf8 = vf8;

		*sg1 = vg1;
		*sg2 = vg2;
		*sg3 = vg3;
		*sg4 = vg4;
		*sg5 = vg5;
		*sg6 = vg6;
		*sg7 = vg7;
		*sg8 = vg8;

		*sh1 = vh1;
		*sh2 = vh2;
		*sh3 = vh3;
		*sh4 = vh4;
		*sh5 = vh5;
		*sh6 = vh6;
		*sh7 = vh7;
		*sh8 = vh8;
	}

	/*******************************************************************************************

	  uint64_t xorValue = 0;
	  uint64_t first64;
	  uint16_t prime = 0x31F4;

	//cout << "Initial: " << endl;
	//printVector(input, start, end);

	for (uint32_t i = start; i < end; i += 8) {
	uint64_t* src = reinterpret_cast<uint64_t*>(input.data() + i);
	uint64_t val = *src;
	uint64_t temp = 0UL;

	if (i % 16 == 0) {
	xorValue = 0UL;
	}

	for (uint8_t j = 6; j >= 0; j -= 2) {
	//cout << "XORVal " << hex << xorValue  << " at i " << i << " and j " << (uint16_t)j  << " with " << (val & 0xFFFF) << " to get subtraction value " << ((((val & 0xFFFF) + prime) % 0x10000)) << dec << endl;
	xorValue = ((((val & 0xFFFF) + prime) % 0x10000) ^ xorValue);
	temp |= xorValue << (8*j);
	val = val >> 16;	
	}

	//cout << "Initial value: " << hex << *src << dec << endl;

	 *src = temp;

	//cout << "Initial value: " << hex << *src << dec << endl;

	if (i != start && i % 16 == 0) {
	if(!skipRotate) {
	rotateRight(input, i - 16, 128);
	uint8_t last;
	uint8_t first;
	first = temp & 0xff;
	temp >>= 8;
	last = first64 & 0xff;
	temp |= last << 56;
	first64 >>= 8;
	first64 |= first << 56;
	 *src = temp;
	 *(src - 1) = first64;
	 }
	 }
	 first64 = temp;
	 }

	//cout << "Output: " << endl;
	//printVector(input, start, end);

	for (uint32_t i = start; i < end; i += 2) {
	uint16_t* src = reinterpret_cast<uint16_t *>(input.data() + i);

	if (i != start && i % 16 == 0) {
	xorValue = 0;
	}

	//uint16_t temp = sTable->getSubstitutionValue(*src) ^ xorValue;	
	uint16_t temp = ((*src + prime) % 0x10000) ^ xorValue;	

	xorValue = temp;

	if (i != start && i % 16 == 0) {
	if(!skipRotate) {
	rotateRight(input, i - 16, 128);
	}
	}

	 *src = temp;
}

if(!skipRotate) {
	rotateRight(input, end - 16, 128);
}
*******************************************************************************************/
}

void reverseSubstitution1(vector<uint8_t>& input, int32_t start = 0, uint32_t end = 0, bool skipRotate = false) {
	if (end == 0) end = input.size();
	DASSERT((end - start) == (k1 / (sizeof(uint8_t) * 8))); // input must be 1024 bits long
	uint64_t xorValue = 0;
	uint16_t prime = 0x31F4;

	//cout << "Input: " << endl;
	//printVector(input, start, end);

	if (!skipRotate) {
		rotateLeft(input, end - 16, 128);
	}

	for (int32_t k = end - 8; k >= start; k -= 8) {
		uint64_t* src = reinterpret_cast<uint64_t*>(input.data() + k);
		uint64_t val = *src;
		uint64_t temp = 0UL;

		if (k != start && k % 16 == 0) {
			if (!skipRotate) {
				rotateLeft(input, k - 16, 128);
			}
		}

		for (int8_t j = 6; j >= 0; j -= 2) {
			if ((k+j) % 16 != 0) {
				xorValue = *reinterpret_cast<uint16_t *>(input.data() + k + j - 2);
			} else {
				xorValue = 0;
			}
			//cout << "XORVal " << hex << xorValue  << " at k " << k << " and j " << (uint16_t)j << " with " << (val & 0xFFFF) << dec << endl;

			uint64_t t = (val & 0xFFFF000000000000) >> 48;
			t ^= xorValue;
			//cout << "Subtracting from: " << hex << t << dec << endl;
			if (t > prime) {
				temp |= (t - prime) << (8*j);
			} else {
				temp |= (0x10000 + t - prime) << (8*j);
			}
			val = val << 16;	
		}

		//cout << "Initial value : " << hex << (*src) << dec << endl;
		*src = temp;
		//cout << "Substituted : " << hex << (*src) << dec << endl;
	}

	//cout << "Output: " << endl;
	//printVector(input, start, end);

	/*********************************************************************************

	  for (int32_t i = int32_t(end - 1); i >= start; i -= 2) {
	  if ((i - 1) != start && (i - 1) % 16 == 0) {
	  DASSERT(i >= start+16);
	  if (!skipRotate) {
	  rotateLeft(input, i - 1 - 16, 128);
	  }
	  }

	  if ((i - 1) % 16 != 0) {
	  DASSERT(i > start+2);
	  xorValue = *reinterpret_cast<uint16_t *>(input.data() + i - 3);
	  } else {
	  xorValue = 0;
	  }

	  uint16_t src = *reinterpret_cast<uint16_t *>((input.data() + i - 1)) ^ xorValue;

	//uint16_t substitutedVal = uint16_t(sTable->getSubstitutionValue(src));
	uint16_t substitutedVal;
	if (src < prime) {
	substitutedVal = (0x10000 + src - prime);
	} else {
	substitutedVal = (src - prime);
	}

	 *reinterpret_cast<uint16_t *>((input.data() + i - 1)) = substitutedVal;
	 }
	 *********************************************************************************/
}

uint32_t getNewIndex(uint32_t j) {
	if (j % 8 == 0) {
		//cout << "Index " << j << " should move to index " << ( ((8 - 1) * 8) + ((j / 8)) ) << endl;
		return ( ((8 - 1) * 8) + ((j / 8)) );
	}
	//cout << "Index " << j << " should move to index " << ( (((j % 8) - 1) * 8) + ((j / 8) + 1) ) << endl;
	return ( (((j % 8) - 1) * 8) + ((j / 8) + 1) );
}

void initializeRearrangementTable() {
	rearrangementTable.resize(64);
	for (int j = 0; j < 64; ++j) {
		rearrangementTable[j] = getNewIndex(j+1)-1;
	}
}

void rearrangement(vector<uint8_t>& input, uint32_t start = 0, uint32_t end = 0) {
	if (end == 0) end = input.size();
	DASSERT((end - start) == k1 / (sizeof(uint8_t) * 8)); // the input must be 1024 bits


	int option1 = 0;

	//cout << "Starting rearrangement at " << start << " to " << end << endl;

	for(uint32_t j = 0; j < 64; ++j) {
		uint32_t i = rearrangementTable[j];
		if (i < j) {
			continue;
		}
		//cout << j << ". Moving " << (j*2)+start << " to " << (i*2)+start << endl;
		uint16_t temp2 = *(reinterpret_cast<uint16_t *>(input.data() + (j*2)+start));
		*(reinterpret_cast<uint16_t *>(input.data() + (j*2)+start)) = *(reinterpret_cast<uint16_t *>(input.data() + (i*2)+start));
		*(reinterpret_cast<uint16_t *>(input.data() + (i*2)+start)) =  temp2;

		/*******************************************************************
		  if (option1) {
		  uint16_t temp2 = *(reinterpret_cast<uint16_t *>(input.data() + (j*2)+start));
		 *(reinterpret_cast<uint16_t *>(input.data() + (j*2)+start)) = *(reinterpret_cast<uint16_t *>(input.data() + (i*2)+start));
		 *(reinterpret_cast<uint16_t *>(input.data() + (i*2)+start)) =  temp2;
		 } else {
		 uint16_t temp = input[(j*2)+start];
		 input[(j*2)+start] = input[(i*2)+start];
		 input[(i*2)+start] = temp;
		 temp = input[(j*2)+start+1];
		 input[(j*2)+start+1] = input[(i*2)+start+1];
		 input[(i*2)+start+1] = temp;
		 }
		 *******************************************************************/
	}
}

uint16_t getSuperIndex(uint16_t j, uint16_t i) { // Note: i and j should be 1-indexed
	uint16_t newIndex = i + (256 * (j - 1)) - 1;
	//cout << "Index Section " << i << " Nibble " << j << " goes to index " << newIndex << "(Section " << (newIndex / 32) << " Nibble " << (newIndex % 32) << ")" << endl; 
	return newIndex;
}

void initializeSuperRearrangementTable() {
	superRearrangementTable.resize(256*32);
	reverseSuperRearrangementTable.resize(256*32);
	//cout << endl << "Super rearrangement: " << endl;
	for(uint16_t i = 0; i < 256*32; ++i) {
		uint16_t newInd = getSuperIndex((i % 32) + 1, (i / 32) + 1);
		superRearrangementTable[i] = newInd;
		//cout << i << " => " << newInd  << endl;
		reverseSuperRearrangementTable[newInd] = i;
	}
	//cout << endl;
}

inline void insertBits(uint16_t nibbleIndex, bool replaceFirst4Bits, uint8_t byte, uint8_t* input, uint16_t insertionIndex) {
	uint8_t temp = input[insertionIndex];
	if (replaceFirst4Bits) {
		DASSERT((temp & 0xF0) == 0); // the first 4 bits in temp should be 0
		if (nibbleIndex % 2 == 0) {
			// Place the first 4-bits of byte as the first 4-bits of temp
			temp |= byte & 0xF0;
		} else {
			// Place the last 4-bits of byte as the first 4-bits of temp
			temp |= (byte & 0xF) << 4;
		}
	} else {
		DASSERT((temp & 0xF) == 0); // the last 4 bits in temp should be 0
		if (nibbleIndex % 2 == 0) {
			// Place the first 4-bits of byte as the last 4-bits of temp
			temp |= (byte & 0xF0) >> 4;
		} else {
			// Place the last 4-bits of byte as the last 4-bits of temp
			temp |= byte & 0xF;
		}
	}
	input[insertionIndex] = temp;
}

inline uint16_t get8bitIndex(uint16_t fourbitIndex) {
	if (fourbitIndex % 2 == 0) {
		return fourbitIndex / 2;
	}
	return (fourbitIndex - 1) / 2;
}

inline void insert4Bits(uint64_t bits, uint16_t nibbleIndex, uint8_t* input) {
	if (nibbleIndex % 2 == 0) {
		input[nibbleIndex/2] |= bits;
	} else {
		input[nibbleIndex/2] |= bits << 4;
	}
}

void superRearrangementUpdated1(vector<uint8_t>& input, bool forwardRearrangement) {
	uint32_t size = k4;
	DASSERT(input.size() == size); // the input must be 4K bytes
	uint8_t output[size];
	bzero(output, size);

	uint64_t* pointer = reinterpret_cast<uint64_t*>(input.data());

	for(uint32_t i = 0; i < size/sizeof(uint64_t); ++i) {
		register uint64_t val = *pointer;
		for(uint32_t j = 0; j < sizeof(uint64_t)*2; ++j) {
			uint16_t newInd;
			if (forwardRearrangement) {
				newInd = reverseSuperRearrangementTable[(i*16)+j];
			} else {
				newInd = superRearrangementTable[(i*16)+j];
			}
			insert4Bits(val & 0x0F, newInd, output);
			val = val >> 4;
		}
		++pointer;
	}
	memcpy(input.data(), output, size);
}

void superRearrangement(vector<uint8_t>& input, bool forwardRearrangement) {
	uint32_t size = k4;
	DASSERT(input.size() == size); // the input must be 4K bytes
	uint8_t output[size];
	bzero(output, size);

	for(uint32_t i = 0; i < size; ++i) {
		uint16_t newInd1;
		uint16_t newInd2;
		if (!forwardRearrangement) {
			newInd1 = reverseSuperRearrangementTable[i*2];
			newInd2 = reverseSuperRearrangementTable[(i*2)+1];
		} else {
			newInd1 = superRearrangementTable[i*2];
			newInd2 = superRearrangementTable[(i*2) + 1];
		}

		uint16_t insertionIndex1 = get8bitIndex(newInd1);
		uint16_t insertionIndex2 = get8bitIndex(newInd2);

		//cout << "Moving " << hex << uint16_t(input[insertionIndex1]) << " at " << insertionIndex1 << " currently holding " << uint16_t(output[i]) << "(" << newInd1 << ")" << dec;

		insertBits(newInd1, true, input[insertionIndex1], output, i);

		//cout << hex << " to get " << uint16_t(output[i]) << dec;

		//cout << hex << " and at " << insertionIndex2 << " currently holding ";
		//cout << hex << uint16_t(output[i]) << "(" << newInd2 << ")" << dec; 

		insertBits(newInd2, false, input[insertionIndex2], output, i);
		//cout << hex << " to get " << uint16_t(output[i]) << dec << endl;
		//cout << "Done" << endl;
	}
	//cout << "Original: " << endl;
	//printVector(input, 0, (k1 / (sizeof(uint8_t) * 8)));
	memcpy(input.data(), output, size);
	//cout << "Modified : " << endl;
	//printVector(input, 0, (k1 / (sizeof(uint8_t) * 8)));
}

void forwardReplaceAndFold(vector<uint8_t>& input, uint32_t start = 0, uint32_t end = 0) {
	if (end == 0) {
		end = input.size();
	}
	chrono::duration<double, nano> duration;
	DASSERT((end - start) == k1 / (sizeof(uint8_t) * 8)); // user data must be 1024 bits
	{
		//Timer timer("Forward substitution : ", duration);
		forwardSubstitution(input, start, end);
	}
	{
		//Timer timer("Rearrangement : ", duration);
		rearrangement(input, start, end);
	}
}

void reverseReplaceAndFold(vector<uint8_t>& encryptedData, uint32_t start = 0, uint32_t end = 0) {
	if (end == 0) end = encryptedData.size();
	DASSERT((end - start) == k1 / (sizeof(uint8_t) * 8)); // encrypted data must be 1024 bits
	rearrangement(encryptedData, start, end);
	reverseSubstitution(encryptedData, start, end);
}

void XorEPUKwithUserData(vector<uint8_t>& userData, vector<uint8_t> EPUK) {
	DASSERT(EPUK.size() == userData.size()); // both must be 4K bytes
	uint64_t* userPointer = reinterpret_cast<uint64_t*>(userData.data());
	uint64_t* EPUKpointer = reinterpret_cast<uint64_t*>(EPUK.data());
	for (uint32_t i = 0; i < userData.size() / sizeof(uint64_t); ++i) {
		*(userPointer + i) = *(userPointer + i) ^ *(EPUKpointer + i);
	}
}

void encryption(vector<uint8_t>& EPUK, vector<uint8_t>& userData) {
	chrono::duration<double, nano> encryptionDuration = chrono::duration<double, nano>();

	int32_t OTPCount = 0;
	uint32_t userKeySize = 128;

	encryptionDuration = encryptionDuration.zero();

	cout << endl << endl << "Real encryption begins" << endl;

	// 2. Pass userData through 4 rounds of replace and fold+Xor with OTP&EPUK
	{
		Timer timer("Encryption: Time taken for 4 rounds of replace and fold of user data+XOR with EPUK and OTP : ", encryptionDuration);
		for (uint32_t i = 0; i < 4; ++i) {
			for (uint32_t j = 0; j < 32; ++j) {
				forwardReplaceAndFold(userData, j*elementsIn1024b, (j+1)*elementsIn1024b);
			}
			for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
				uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
				uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
				OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
			}
			++OTPCount;
			//XorEPUKwithUserData(userData, EPUK);
		}
	}

	// 3. Pass userData through 1 round of super rearrangement
	{
		Timer timer("Encryption: Time taken for super-rearrangement round on data : ", encryptionDuration);
		superRearrangement(userData, true);
	}

	// 4. Pass userData through 1 round of substitution+Xor with OTP&EPUK
	{
		Timer timer("Encryption: Time taken for just replace (substitution) of user data : ", encryptionDuration);
		for (uint32_t j = 0; j < 32; ++j) {
			forwardSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b);
		}
	}


	{
		Timer timer("Encryption: Time taken for xoring EPUK, OTP, and user data : ", encryptionDuration);
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
			OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
		}
		++OTPCount;
		//XorEPUKwithUserData(userData, EPUK);
	}

	// 5. Pass userData through 2 rounds of replace and fold+Xor with OTP&EPUK
	{
		Timer timer("Encryption: Time taken for 2 rounds of replace and fold, and xor of EPUK, OTP, and user data : ", encryptionDuration);
		for (uint32_t i = 0; i < 2; ++i) {
			for (uint32_t j = 0; j < 32; ++j) {
				forwardReplaceAndFold(userData, j*elementsIn1024b, (j+1)*elementsIn1024b);
			}
			for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
				uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
				uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
				OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
			}
			++OTPCount;
			//XorEPUKwithUserData(userData, EPUK);
		}
	}

	// 6. Pass userData through 1 round of super rearrangement
	{
		Timer timer("Encryption: Time taken for 1 round of super-rearrangement : ", encryptionDuration);
		superRearrangement(userData, true);
	}

	// 7. Pass userData through 1 round of replace and fold+Xor with OTP&EPUK followed by super rearrangement
	{
		Timer timer("Encryption: Time taken for 1 round of replace and fold on user data : ", encryptionDuration);
		for (uint32_t j = 0; j < 32; ++j) {
			forwardReplaceAndFold(userData, j*elementsIn1024b, (j+1)*elementsIn1024b);
		}
	}
	{
		Timer timer("Encryption: Time taken for 1 round of xor of OTP and user data : ", encryptionDuration);
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
			OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
		}
		++OTPCount;
	}
	{
		Timer timer("Encryption: Time taken for 1 round of xor of EPUK and user data : ", encryptionDuration);
		//XorEPUKwithUserData(userData, EPUK);
	}
	{
		Timer timer("Encryption: Time taken for 1 round of super-rearrangement : ", encryptionDuration);
		superRearrangement(userData, true);
	}

	cout << "Total time take by the encryption algorithm : " << convertDurationToString(encryptionDuration) << endl;
}

void encryptionUpdated1(vector<uint8_t>& EPUK, vector<uint8_t>& userData) {
	// STEPS:
	// 1. Pass userData through 1 round of substitution and rotate right followed by Xor with EPUK+OTP
	// 2. Pass userData through 
	// 		  i. 1 round of substitution 
	// 		 ii. 1 round of rearrangement 
	// 		iii. followed by Xor with EPUK+OTP
	// 		 iv. 1 round of substution and rotate right 
	// 		  v. followed by Xor with EPUK+OTP
	// 3. Pass userData through 1 round of forward Substitution followed by Xor with EPUK+OTP
	// 4. Pass userData through 1 round of super rearrangement
	// 5. Pass userData through 1 round of forward replace and fold followed by Xor with EPUK+OTP
	// 6. Pass userData through 1 round of substitution and rotate right followed by Xor with EPUK+OTP
	// 7. Pass userData throuhg 1 round of substitution followed by Xor with EPUK+OTP

	chrono::duration<double, nano> encryptionDuration = chrono::duration<double, nano>();

	int32_t OTPCount = 0;
	uint32_t userKeySize = 128;

	encryptionDuration = encryptionDuration.zero();

	cout << endl << endl << "Real encryption begins" << endl;

	/*************************************************************************************************************

	// 1. Pass userData through 1 round of substitution and rotate right followed by Xor with EPUK+OTP
	{
	Timer timer("Encryption: Time taken for 1 round of substitution and rotate right : ", encryptionDuration);
	for (uint32_t j = 0; j < 32; ++j) {
	forwardSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b, false);
	}
	} 
	{ 
	Timer timer("Encryption: Time taken for Xor with EPUK+OTP : ", encryptionDuration);
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
	uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
	uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
	OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	++OTPCount;
	//XorEPUKwithUserData(userData, EPUK);
	}

	// 2. Pass userData through 
	// 		  i. 1 round of substitution 
	// 		 ii. 1 round of rearrangement 
	// 		iii. followed by Xor with EPUK+OTP
	// 		 iv. 1 round of substution and rotate right 
	// 		  v. followed by Xor with EPUK+OTP
	{
	Timer timer("Encryption: Time taken for 1 round of substitution : ", encryptionDuration);
	for (uint32_t j = 0; j < 32; ++j) {
	forwardSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b, true);
	}
	} 
	{
	Timer timer("Encryption: Time taken for 1 round of rearrangement : ", encryptionDuration);
	for (uint32_t j = 0; j < 32; ++j) {
	rearrangement(userData, j*elementsIn1024b, (j+1)*elementsIn1024b);
	}
	} 
	{ 
	Timer timer("Encryption: Time taken for Xor with EPUK+OTP : ", encryptionDuration);
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
	uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
	uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
	OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	++OTPCount;
	//XorEPUKwithUserData(userData, EPUK);
	}
	{
	Timer timer("Encryption: Time taken for 1 round of substitution and rotate right : ", encryptionDuration);
	for (uint32_t j = 0; j < 32; ++j) {
	forwardSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b, false);
	}
	} 
	{ 
	Timer timer("Encryption: Time taken for Xor with EPUK+OTP : ", encryptionDuration);
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
	uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
	uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
	OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	++OTPCount;
	//XorEPUKwithUserData(userData, EPUK);
	}

	// 3. Pass userData through 1 round of substitution followed by Xor with EPUK+OTP
	{
	Timer timer("Encryption: Time taken for substitution of user data : ", encryptionDuration);
	for (uint32_t j = 0; j < 32; ++j) {
	forwardSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b, true);
	}
}
{
	Timer timer("Encryption: Time taken for xoring EPUK, OTP, and user data : ", encryptionDuration);
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
		uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
		OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	++OTPCount;
	//XorEPUKwithUserData(userData, EPUK);
}

// 4. Pass userData through 1 round of super rearrangement
{
	Timer timer("Encryption: Time taken for super-rearrangement round on data : ", encryptionDuration);
	superRearrangementUpdated1(userData, true);
}

// 5. Pass userData through 1 round of forward replace and fold followed by Xor with EPUK+OTP
{
	Timer timer("Encryption: Time taken for replace and fold of user data : ", encryptionDuration);
	for (uint32_t j = 0; j < 32; ++j) {
		forwardReplaceAndFold(userData, j*elementsIn1024b, (j+1)*elementsIn1024b);
	}
}
{
	Timer timer("Encryption: Time taken for xoring EPUK, OTP, and user data : ", encryptionDuration);
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
		uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
		OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	++OTPCount;
	//XorEPUKwithUserData(userData, EPUK);
}

*************************************************************************************************************/
// 6. Pass userData through 1 round of substitution and rotate right followed by Xor with EPUK+OTP
{
	Timer timer("Encryption: Time taken for substitution & rotate right of user data : ", encryptionDuration);
	for (uint32_t j = 0; j < 32; ++j) {
		forwardSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b, false);
	}
}
{
	Timer timer("Encryption: Time taken for xoring EPUK, OTP, and user data : ", encryptionDuration);
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
		uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
		OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	++OTPCount;
	//XorEPUKwithUserData(userData, EPUK);
}

// 7. Pass userData throuhg 1 round of substitution followed by Xor with EPUK+OTP
{
	Timer timer("Encryption: Time taken for substitution of user data : ", encryptionDuration);
	for (uint32_t j = 0; j < 32; ++j) {
		forwardSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b, true);
	}
}
{
	Timer timer("Encryption: Time taken for xoring EPUK, OTP, and user data : ", encryptionDuration);
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
		uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
		OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	++OTPCount;
	//XorEPUKwithUserData(userData, EPUK);
}

cout << "Total time take by the encryption algorithm : " << convertDurationToString(encryptionDuration) << endl;
}

void decryption(vector<uint8_t>& EPUK, vector<uint8_t>& encryptedData) {
	uint32_t userKeySize = 128;
	int32_t OTPCount = 7;

	// 2. Pass userData through 1 round of replace and fold+Xor with OTP&EPUK by super rearrangement (7)
	superRearrangement(encryptedData, false);
	//XorEPUKwithUserData(encryptedData, EPUK);
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
		uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
		OTPs->apply(encryptedData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	--OTPCount;
	for (uint32_t j = 0; j < 32; ++j) {
		reverseReplaceAndFold(encryptedData, j*elementsIn1024b, (j+1)*elementsIn1024b);
	}

	// 3. Pass encryptedData through 1 round of super rearrangement (6)
	superRearrangement(encryptedData, false);

	// 4. Pass encryptedData through 2 rounds of replace and fold+Xor with OTP&EPUK (5)
	for (uint32_t i = 0; i < 2; ++i) {
		//XorEPUKwithUserData(encryptedData, EPUK);
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
			OTPs->apply(encryptedData, EPUK, (k*128), (k*128+64), index1, index2);
		}
		--OTPCount;
		for (uint32_t j = 0; j < 32; ++j) {
			reverseReplaceAndFold(encryptedData, j*elementsIn1024b, (j+1)*elementsIn1024b);
		}
	}

	// 5. Pass encryptedData through 1 round of substitution+Xor with OTP&EPUK (4)
	//XorEPUKwithUserData(encryptedData, EPUK);
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
		uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
		OTPs->apply(encryptedData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	--OTPCount;
	for (uint32_t j = 0; j < 32; ++j) {
		reverseSubstitution(encryptedData, j*elementsIn1024b, (j+1)*elementsIn1024b);
	}

	// 6. Pass encryptedData through 1 round of super rearrangement (3)
	superRearrangement(encryptedData, false);

	// 7. Pass encryptedData through 4 rounds of replace and fold+Xor with OTP&EPUK (2)
	for (uint32_t i = 0; i < 4; ++i) {
		//XorEPUKwithUserData(encryptedData, EPUK);
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
			OTPs->apply(encryptedData, EPUK, (k*128), (k*128+64), index1, index2);
		}
		--OTPCount;
		for (uint32_t j = 0; j < 32; ++j) {
			reverseReplaceAndFold(encryptedData, j*elementsIn1024b, (j+1)*elementsIn1024b);
		}
	}
}

void decryptionUpdated1(vector<uint8_t>& EPUK, vector<uint8_t>& userData) {
	// STEPS:
	// 1. Pass userData throuhg Xor with EPUK+OTP followed by 1 round of substitution (7)
	// 2. Pass userData through Xor with EPUK+OTP followed by 1 round of rotate left and substitution (6)
	// 3. Pass userData through Xor with EPUK+OTP followed by 1 round of reverse replace and fold (5)
	// 4. Pass userData through 1 round of super rearrangement (4)
	// 5. Pass userData through Xor with EPUK+OTP followed by 1 round of substitution (3)
	// 6. Pass userData through (2)
	// 		  i. followed by Xor with EPUK+OTP (v)
	// 		 ii. 1 round of substution and rotate right (iv)
	// 		iii. followed by Xor with EPUK+OTP (iii)
	// 		 iv. 1 round of rearrangement (ii)
	// 		  v. 1 round of substitution (i)
	// 7. Pass userData through Xor with EPUK+OTP followed by 1 round of rotate left and substitution (1) 

	chrono::duration<double, nano> decryptionDuration = chrono::duration<double, nano>();
	uint32_t userKeySize = 128;
	int32_t OTPCount = 2 - 1;
	decryptionDuration = decryptionDuration.zero();

	// 1. Pass userData throuhg Xor with EPUK+OTP followed by 1 round of substitution (7)
	{ Timer timer("Decryption: Time taken to xor user data, EPUK, and OTP : ", decryptionDuration); 
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
			OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
		}
		--OTPCount;
	}
	//XorEPUKwithUserData(userData, EPUK);
	{ Timer timer("Decryption: Time taken to substitute : ", decryptionDuration); 
		for (uint32_t j = 0; j < 32; ++j) {
			reverseSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b, true);
		}
	}

	// 2. Pass userData through Xor with EPUK+OTP followed by 1 round of substitution and rotate left (6)
	{ Timer timer("Decryption: Time taken to xor user data, EPUK, and OTP : ", decryptionDuration); 
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
			OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
		}
		--OTPCount;
	}
	//XorEPUKwithUserData(userData, EPUK);
	{
		Timer timer("Decryption: Time taken to substitute & rotate left user data : ", decryptionDuration);
		for (uint32_t j = 0; j < 32; ++j) {
			reverseSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b, false);
		}
	}
	/**************************************************************************************************************

	// 3. Pass userData through Xor with EPUK+OTP followed by 1 round of reverse replace and fold (5)
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
	uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
	uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
	OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	--OTPCount;
	//XorEPUKwithUserData(userData, EPUK);
	for (uint32_t j = 0; j < 32; ++j) {
	reverseReplaceAndFold(userData, j*elementsIn1024b, (j+1)*elementsIn1024b);
	}

	// 4. Pass userData through 1 round of super rearrangement (4)
	superRearrangementUpdated1(userData, false);

	// 5. Pass userData through Xor with EPUK+OTP followed by 1 round of substitution (3)
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
	uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
	uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
	OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	--OTPCount;
	//XorEPUKwithUserData(userData, EPUK);
	for (uint32_t j = 0; j < 32; ++j) {
	reverseSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b, true);
	}

	// 6. Pass userData through (2)
	// 		  i. followed by Xor with EPUK+OTP (v)
	// 		 ii. 1 round of substution and rotate right (iv)
	// 		iii. followed by Xor with EPUK+OTP (iii)
	// 		 iv. 1 round of rearrangement (ii)
	// 		  v. 1 round of substitution (i)
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
	uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
	uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
	OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	--OTPCount;
	//XorEPUKwithUserData(userData, EPUK);
	for (uint32_t j = 0; j < 32; ++j) {
	reverseSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b, false);
	}
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
	uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
	uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
	OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	--OTPCount;
	//XorEPUKwithUserData(userData, EPUK);
	for (uint32_t j = 0; j < 32; ++j) {
	rearrangement(userData, j*elementsIn1024b, (j+1)*elementsIn1024b);
	}
	for (uint32_t j = 0; j < 32; ++j) {
	reverseSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b, true);
	}

	// 7. Pass userData through Xor with EPUK+OTP followed by 1 round of rotate left and substitution (1)
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
	uint32_t index1 = EPUK[(OTPCount*userKeySize) + k]; 
	uint32_t index2 = EPUK[(OTPCount*userKeySize) + k + 1]; 
	OTPs->apply(userData, EPUK, (k*128), (k*128+64), index1, index2);
	}
	--OTPCount;
	//XorEPUKwithUserData(userData, EPUK);
	for (uint32_t j = 0; j < 32; ++j) {
	reverseSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b, false);
	}

	**************************************************************************************************************/

		cout << "Total time take by the decryption algorithm : " << convertDurationToString(decryptionDuration) << endl;
}

vector<uint8_t> extendedKey(vector<uint8_t> PUK) { // extends 1024-bit PUK to 4K bytes
	DASSERT(PUK.size() == k1 / (sizeof(uint8_t) * 8)); // PUK must be 1024 bits
	vector<uint8_t> EPUK;
	EPUK.resize(k32 / (sizeof(uint8_t) * 8));
	for (uint32_t i = 0; i < 32; ++i) {
		rotateLeft(PUK, 0, PUK.size()*sizeof(uint8_t)*8); 
		for (uint32_t j = 0; j < PUK.size(); ++j) {
			EPUK[j+(i*PUK.size())] = PUK[j];
		}
	}
	return EPUK;
}

int main() {
	chrono::high_resolution_clock::time_point startTime;
	chrono::high_resolution_clock::time_point endTime;
	chrono::duration<double, nano> duration;

	initialize();


	// TEST 1: Test OTP generation and apply function ===================================================
	uint32_t size = k1 / (sizeof(uint8_t) * 8);
	vector<uint8_t> original;
	original.resize(size);
	for (uint32_t i = 0; i < size; ++i) {
		original[i] = rand() & 0xFF;
	}

	vector<uint8_t> input(original);
	vector<uint8_t> EPUK(original);
	uint32_t startingInd = 2;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	OTPs->apply(input, EPUK, startingInd, startingInd + 8, 2, 5); // "Encypt" the input
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	OTPs->apply(input, EPUK, startingInd, startingInd + 8, 2, 5); // "Decrypt" the input
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << "Test 1 is complete. OTPs can be generated and are successfully XORed with input" << endl;


	// TEST 2: Test rotateLeft and rotateRight functions ================================================
	size = (k1 / (sizeof(uint8_t) * 8));
	original.resize(size);
	for (uint32_t i = 0; i < size; ++i) {
		original[i] = rand() & 0xFF;
	}

	input = original;
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	startTime = chrono::high_resolution_clock::now();
	rotateLeft(input, 16, 128);
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken to left shift a 1024-bit section of user data : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	startTime = chrono::high_resolution_clock::now();
	rotateRight(input, 16, 128);
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken to right shift a 1024-bit section of user data : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << "Test 2 is complete. Input can be rotated 1 byte to the left and right" << endl;

	// TEST 3: Test forwardSubstition and reverseSubstition functions ===================================
	size = k32 / (sizeof(uint8_t) * 8); // 32K bits
	uint32_t start = 0;//3968-(elementsIn1024b*2); // Start must be a multiple of 16 (else rotation logic will break)
	original.resize(size);
	for (uint32_t i = 0; i < size; ++i) {
		original[i] = rand() & 0xFF;
	}

	input.resize(size);
	input = original;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	{ Timer timer ("Time taken to forward substitution 1024 bits", duration);
		forwardSubstitution(input, start, start+elementsIn1024b, true, true);
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	{ Timer timer ("Time taken to reverse substitution 1024 bits", duration);
		reverseSubstitution(input, start, start+elementsIn1024b, true, true);
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Finished substitution on 1024 bits" << endl;

	{ Timer timer ("Time taken to forward substitution and rotate right 1024 bits", duration);
		forwardSubstitution(input, start, start+elementsIn1024b, false, true);
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	{ Timer timer ("Time taken to reverse substitution and rotate left 1024 bits", duration);
		reverseSubstitution(input, start, start+elementsIn1024b, false, true);
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Finished substitution and rotate on 1024 bits" << endl;

	{
		Timer timer("Time taken to forward substitute & rotate right 4K bytes : ", duration);
		//startTime = chrono::high_resolution_clock::now();
		for (uint32_t j = 0; j < 32; ++j) {
			forwardSubstitution(input, j*elementsIn1024b, (j+1)*elementsIn1024b, false, true);
		}
		//endTime = chrono::high_resolution_clock::now();
		//duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
		//cout << "Time taken to forward substitute and rotate right 4K bytes : " << convertDurationToString(duration) << endl;
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	{
		Timer timer("Time taken to reverse substitute & rotate left 4K bytes : ", duration);
		//startTime = chrono::high_resolution_clock::now();
		for (uint32_t j = 0; j < 32; ++j) {
			reverseSubstitution(input, j*elementsIn1024b, (j+1)*elementsIn1024b, false, true);
		}
		//endTime = chrono::high_resolution_clock::now();
		//duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
		//cout << "Time taken to reverse substitute and rotate left 4K bytes : " << convertDurationToString(duration) << endl;
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << "Test 3 is complete. Substition algorithm works; it can encrypt and decrypt data" << endl;

	// TEST 4: Test rearrangement function ==============================================================
	start = 3968;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	startTime = chrono::high_resolution_clock::now();
	rearrangement(input, start, start+elementsIn1024b);
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken to rearrange 1024-bit section of user data : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	rearrangement(input, start, start+elementsIn1024b);

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << "Test 4 is complete. Rearrangement algorithm works" << endl;

	// TEST 5: Test superRearrangement function ==============================================================
	// Test insertBits function
	uint8_t test[1];
	uint32_t nibbleIndex = 1;
	uint32_t originalIndex = 4;
	uint32_t testIndex = (nibbleIndex % 2 == 0) ? nibbleIndex : (nibbleIndex - 1);

	cout << "Placing byte " << hex << uint16_t(original[originalIndex]) << " at nibble index " << nibbleIndex << " of " << uint16_t(test[testIndex]) << endl;
	cout << "Placing 4-bits at start: " << endl;
	insertBits(nibbleIndex, true, original[originalIndex], test, testIndex); 
	cout << "Results: " << hex <<  uint16_t(test[testIndex]) << endl;
	cout << "Placing 4-bits at end: " << endl;
	insertBits(nibbleIndex, false, original[originalIndex], test, testIndex); 
	cout << "Results: " << hex <<  uint16_t(test[testIndex]) << endl;

	// Test superRearrangement function
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	startTime = chrono::high_resolution_clock::now();
	superRearrangement(input, true);
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken to super rearrange user data : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	startTime = chrono::high_resolution_clock::now();
	superRearrangement(input, false);
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken to reverse super rearrangement of user data : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	startTime = chrono::high_resolution_clock::now();
	superRearrangementUpdated1(input, true);
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken to super rearrange user data with new function : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	startTime = chrono::high_resolution_clock::now();
	superRearrangementUpdated1(input, false);
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken to reverse super rearrangement of user data with new function : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Test 5 is complete. Super Rearrangement algorithm works" << endl;

	// TEST 6: Test forward and reverse replace and fold function ============================================
	// Test replace and fold on 4K byte input
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	startTime = chrono::high_resolution_clock::now();
	for(uint32_t i = 0; i < 32; ++i) {
		forwardReplaceAndFold(input, i*elementsIn1024b, (i+1)*elementsIn1024b);
	}
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken to pass user data through 1 round of forward replace and fold (forwardRepalceAndFold called 32 times) : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	startTime = chrono::high_resolution_clock::now();
	for(uint32_t i = 0; i < 32; ++i) {
		reverseReplaceAndFold(input, i*elementsIn1024b, (i+1)*elementsIn1024b);
	}
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken to pass user data through 1 round reverse of forward replace and fold (reverseRepalceAndFold called 32 times) : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	// Test multiple cycles of replace and fold on 1024 bit input
	vector<uint8_t> userKey;
	size = (k1 / (sizeof(uint8_t)*8));
	userKey.resize(size);

	for (uint32_t i = 0; i < size; ++i) {
		userKey[i] = rand() & 0xFF;
	}

	vector<uint8_t> puk(userKey);

	startTime = chrono::high_resolution_clock::now();
	for(uint32_t i = 0; i < 4; ++i) {
		forwardReplaceAndFold(puk, 0, size);
	}
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken to process user key (i.e. 4 rounds of replace and fold) : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(userKey.data()), reinterpret_cast<const char*>(puk.data()), size) != 0);
	for(uint32_t i = 0; i < 4; ++i) {
		reverseReplaceAndFold(puk, 0, size);
	}
	assert(memcmp(reinterpret_cast<const char*>(userKey.data()), reinterpret_cast<const char*>(puk.data()), size) == 0);

	cout << "Test 6 is complete. Replace and fold algorithm works (for forward and reverse)" << endl;

	// TEST 7: Test extendedKey function =====================================================================
	startTime = chrono::high_resolution_clock::now();
	EPUK = extendedKey(puk);
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken to extend processed user key : " << convertDurationToString(duration) << endl;

	//cout << "PUK: " << endl;
	//printVector(puk, 0, puk.size());
	//cout << "EPUK first 1024 bits: " << endl;
	//printVector(EPUK, 0, puk.size());
	//cout << "EPUK second 1024 bits: " << endl;
	//printVector(EPUK, puk.size(), puk.size()*2);

	cout << "Test 7 is complete. Extended Key function works" << endl;

	// TEST 8: Test XOR with OTP and EPUK =====================================================================
	vector<uint8_t> copy(input);

	startTime = chrono::high_resolution_clock::now();
	XorEPUKwithUserData(copy, EPUK);
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(copy.data()), size) != 0);

	XorEPUKwithUserData(copy, EPUK);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(copy.data()), size) == 0);
	cout << "Time taken for Xoring user data and EPUK : " << convertDurationToString(duration) << endl;


	startTime = chrono::high_resolution_clock::now();
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(1*userKey.size()) + k]; 
		uint32_t index2 = EPUK[(1*userKey.size()) + k + 1]; 
		OTPs->apply(copy, EPUK, (k*128), (k*128+64), index1, index2);
	}
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(copy.data()), size) != 0);

	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(1*userKey.size()) + k]; 
		uint32_t index2 = EPUK[(1*userKey.size()) + k + 1]; 
		OTPs->apply(copy, EPUK, (k*128), (k*128+64), index1, index2);
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(copy.data()), size) == 0);
	cout << "Time taken for Xoring user data and OTP : " << convertDurationToString(duration) << endl;


	startTime = chrono::high_resolution_clock::now();
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(1*userKey.size()) + k]; 
		uint32_t index2 = EPUK[(1*userKey.size()) + k + 1]; 
		OTPs->apply(copy, EPUK, (k*128), (k*128+64), index1, index2);
	}
	XorEPUKwithUserData(copy, EPUK);
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(copy.data()), size) != 0);

	XorEPUKwithUserData(copy, EPUK);
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(1*userKey.size()) + k]; 
		uint32_t index2 = EPUK[(1*userKey.size()) + k + 1]; 
		OTPs->apply(copy, EPUK, (k*128), (k*128+64), index1, index2);
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(copy.data()), size) == 0);
	cout << "Time taken for Xoring user data, OTP, and EPUK : " << convertDurationToString(duration) << endl;


	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << "Test 8 is complete. User Data can be xored with EPUK and OTP" << endl;

	// TEST 9: Test Encryption step 2 =========================================================================
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	int32_t i = 1;

	startTime = chrono::high_resolution_clock::now();
	for(int32_t OTPCount = 0; OTPCount < i; ++OTPCount) {
		for (uint32_t j = 0; j < 32; ++j) {
			forwardReplaceAndFold(input, j*elementsIn1024b, (j+1)*elementsIn1024b);
		}
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKey.size()) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKey.size()) + k + 1]; 
			OTPs->apply(input, EPUK, (k*128), (k*128+64), index1, index2);
		}
		XorEPUKwithUserData(input, EPUK);
	}
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken for encrypting user data (4K Bytes) through " << i << " rounds of replace and fold + Xor with EPUK and UTP: " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	startTime = chrono::high_resolution_clock::now();
	for(int32_t OTPCount = i-1; OTPCount >= 0; --OTPCount) {
		XorEPUKwithUserData(input, EPUK);
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKey.size()) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKey.size()) + k + 1]; 
			OTPs->apply(input, EPUK, (k*128), (k*128+64), index1, index2);
		}
		for (uint32_t j = 0; j < 32; ++j) {
			reverseReplaceAndFold(input, j*elementsIn1024b, (j+1)*elementsIn1024b);
		}
	}
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken for decrypting user data (4K Bytes) through " << i << " rounds of replace and fold + Xor with EPUK and UTP: " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Test 9 is complete. EPUK and OTP-Xor works (even with rotation and replace&fold/tested with " << i << ")" << endl;


	// TEST 10: Final Test: Put everything together ===========================================================
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	encryption(EPUK, input);

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	startTime = chrono::high_resolution_clock::now();
	decryption(EPUK, input);
	endTime = chrono::high_resolution_clock::now();
	duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
	cout << "Time taken to decrypt user data: " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << "Test 10 is complete. Encryption/decryption algorithm works." << endl;

	// TEST 11: Final Test: Put everything together (Updated1) ================================================
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	encryptionUpdated1(EPUK, input);

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	decryptionUpdated1(EPUK, input);

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << "Test 11 is complete. Updated encryption/decryption algorithm works." << endl;

	cout << "DONE" << endl;

	return 0;
}
