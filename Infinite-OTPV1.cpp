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

class SubstitutionTable {
	uint32_t size;
	vector<uint64_t> substitutionTable;

	void initSubstitutionTable() {
		substitutionTable.resize(size);
		for(uint32_t i = 0; i < size / 2; ++i) {
			// TODO Replace this with Galois field multiplication
			substitutionTable[i] = uint16_t(i + (size / 2));
			substitutionTable[i + (size / 2)] = uint16_t(i);
		}
	}

	public:
	SubstitutionTable() {
		size = k64;
		initSubstitutionTable();
	}

	SubstitutionTable(const SubstitutionTable& other) = delete;
	SubstitutionTable(SubstitutionTable&& other) = delete;

	uint64_t getSubstitutionValue(uint32_t index) {
		return substitutionTable[index];
	}
};

class OTPsections {
	uint32_t sectionCount;
	uint32_t sectionSize;
	vector<vector<uint64_t>> OTPs;

	void initOTPsections() {
		OTPs.resize(sectionCount);
		srand(0);
		for (uint32_t i = 0; i < sectionCount; ++i) {
			OTPs[i].resize(sectionSize);
			for (uint32_t j = 0; j < sectionSize; ++j) {
				uint64_t temp = rand();
				OTPs[i][j] = (temp << 32) | rand();
			}
		}
	}

	public:

	OTPsections() {
		sectionCount = k64;
		sectionSize = 64 / sizeof(uint64_t); // The size of each OTP section is only 64 bytes.
		initOTPsections();
	}

	OTPsections(const OTPsections& other) = delete;
	OTPsections(OTPsections&& other) = delete;

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
};

SubstitutionTable* sTable = nullptr;
OTPsections* OTPs = nullptr;

void initialize() {
	sTable = new SubstitutionTable();
	OTPs = new OTPsections();
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
void forwardSubstitution(vector<uint8_t>& input, uint32_t start = 0, uint32_t end = 0, bool skipRotate = false) {
	if (end == 0) end = input.size();
	DASSERT((end - start) == (k1 / (sizeof(uint8_t) * 8))); // input must be 1024 bits long
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
				/*************************
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
				 **************************/
			}
		}
		first64 = temp;
	}

	//cout << "Output: " << endl;
	//printVector(input, start, end);

	/*************************************************************************

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

	 *************************************************************************/

	if(!skipRotate) {
		rotateRight(input, end - 16, 128);
	}
}

void reverseSubstitution(vector<uint8_t>& input, int32_t start = 0, uint32_t end = 0, bool skipRotate = false) {
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
	uint32_t start = 3968-(elementsIn1024b*2); // Start must be a multiple of 16 (else rotation logic will break)
	original.resize(size);
	for (uint32_t i = 0; i < size; ++i) {
		original[i] = rand() & 0xFF;
	}

	input.resize(size);
	input = original;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	//startTime = chrono::high_resolution_clock::now();
	{ Timer timer("Timer: Time taken to forward substitute 1024-bits : ", duration);
		forwardSubstitution(input, start, start+elementsIn1024b, true);
		//endTime = chrono::high_resolution_clock::now();
		//duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
		//cout << "Time taken to forward substitute 1024-bits : " << convertDurationToString(duration) << endl;
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	//startTime = chrono::high_resolution_clock::now();
	{ Timer timer("Timer: Time taken to reverse substitute 1024-bits : ", duration);
		reverseSubstitution(input, start, start+elementsIn1024b, true);
		//endTime = chrono::high_resolution_clock::now();
		//duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
		//cout << "Time taken to reverse substitute 1024-bits : " << convertDurationToString(duration) << endl;
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	//startTime = chrono::high_resolution_clock::now();
	{ Timer timer("Timer: Time taken to forward substitute and rotate right 1024-bits : ", duration);
		forwardSubstitution(input, start, start+elementsIn1024b, false);
		//endTime = chrono::high_resolution_clock::now();
		//duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
		//cout << "Time taken to forward substitute and rotate right 1024-bits : " << convertDurationToString(duration) << endl;
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	//startTime = chrono::high_resolution_clock::now();
	{ Timer timer("Timer: Time taken to reverse substitute and rotate left 1024-bits : ", duration);
		reverseSubstitution(input, start, start+elementsIn1024b, false);
		//endTime = chrono::high_resolution_clock::now();
		//duration = chrono::duration_cast<chrono::duration<double, nano>>(endTime - startTime);
		//cout << "Time taken to reverse substitute and rotate left 1024-bits : " << convertDurationToString(duration) << endl;
	}

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	{
		Timer timer("Time taken to forward substitute & rotate right 4K bytes : ", duration);
		//startTime = chrono::high_resolution_clock::now();
		for (uint32_t j = 0; j < 32; ++j) {
			forwardSubstitution(input, j*elementsIn1024b, (j+1)*elementsIn1024b, false);
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
			reverseSubstitution(input, j*elementsIn1024b, (j+1)*elementsIn1024b, false);
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
