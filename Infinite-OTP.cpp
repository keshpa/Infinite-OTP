#include <assert.h>
#include <chrono>
#include <cuchar>
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
vector<uint32_t> superRearrangementTable;
vector<uint32_t> reverseSuperRearrangementTable;

void initializeRearrangementTable();
uint32_t getNewIndex(uint32_t j);
void initializeSuperRearrangementTable();
uint32_t getSuperIndex(uint32_t j, uint32_t i);
vector<uint8_t> extendedKey(vector<uint8_t> PUK);

static constexpr bool CHECKASSERT = false;  
/**  
 * Used to disable/enable asserts based on the value of CHECKASSERT
 */ 
#define DASSERT(cond) if (CHECKASSERT) { assert(cond); } 

string convertDurationToString(chrono::duration<double, nano> nanoSeconds) {
	uint64_t timeDuration = nanoSeconds.count();
	if (timeDuration > 1000 && timeDuration < 10000*1000) {
		return std::to_string(std::chrono::duration_cast<std::chrono::microseconds>(nanoSeconds).count()) + " microSeconds";  
	}
	if (timeDuration > 10000*1000 && timeDuration < uint64_t(10000000)*1000) {
		return std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>(nanoSeconds).count()) + " milliSeconds";
	}
	if (timeDuration > uint64_t(10000000)*1000 && timeDuration < uint64_t(10000000)*1000*60) {
		return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(nanoSeconds).count()) + " seconds";
	}
	if (timeDuration > uint64_t(10000000)*1000*60) {
		return std::to_string(std::chrono::duration_cast<std::chrono::minutes>(nanoSeconds).count()) + " minutes";
	}
	return std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(nanoSeconds).count()) + " nanoSeconds";  
}

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

	void apply(vector<uint8_t>& input, uint32_t index1, uint32_t index2, uint32_t OTPind1, uint32_t OTPind2) {
		for (uint32_t i = 0; i < sectionSize; ++i) {
			uint64_t OTP1=  OTPs[OTPind1][i];
			uint64_t OTP2=  OTPs[OTPind2][i];
			for (uint32_t j = 0; j < 8; ++j) { // 8 bytes from input need to be sewn together and then XORed with OTP
				input[j+index1+(i*sizeof(uint64_t))] =  (OTP1 >> (8*(sizeof(uint64_t)-j-1))) ^  input[j+index1+(i*sizeof(uint64_t))];
				input[j+index2+(i*sizeof(uint64_t))] =  (OTP2 >> (8*(sizeof(uint64_t)-j-1))) ^  input[j+index2+(i*sizeof(uint64_t))];
			}
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

void rotateRight(vector<uint8_t>& input, int32_t startingInd, uint32_t rotateBetweenBits, int32_t rotateBytes = 1) {
	//cout << "Rotating between : " << rotateBetweenBits << endl;
	uint32_t length = rotateBetweenBits / (sizeof(uint8_t) * 8);	// Divide by 8 to convert into bytes and then sizeof(uint8_t)
	//printVector(input, startingInd, length + startingInd);

	uint8_t* pointer = reinterpret_cast<uint8_t*>(input.data());
	pointer += startingInd * sizeof(uint8_t);
	vector<uint8_t> lastBytes;
	lastBytes.resize(rotateBytes);
	for (uint32_t i = 0; i < rotateBytes; ++i) {
		lastBytes[i] = *(pointer+length-1-i);
	}

	memmove(pointer + rotateBytes, pointer, (rotateBetweenBits/8) - rotateBytes);
	for (uint32_t i = 0; i < rotateBytes; ++i) {
		*(pointer+i) = lastBytes[rotateBytes - 1 - i];
	}

	//cout << "End rotation" << endl;
	//printVector(input, startingInd, length + startingInd);
}

void rotateLeft(vector<uint8_t>& input, int32_t startingInd, uint32_t rotateBetweenBits, int32_t rotateBytes = 1) {
	//cout << "Rotating between : " << rotateBetweenBits << endl;
	uint32_t length = rotateBetweenBits / (sizeof(uint8_t) * 8);
	//printVector(input, startingInd, length + startingInd);

	uint8_t* pointer = reinterpret_cast<uint8_t*>(input.data());
	pointer += startingInd * sizeof(uint8_t);
	vector<uint8_t> firstBytes;
	firstBytes.resize(rotateBytes);
	for (uint32_t i = 0; i < rotateBytes; ++i) {
		//cout << i << " : " << rotateBytes << " storing ";
		//cout << hex << (uint16_t)(*(pointer+i)) << dec << endl;
		firstBytes[i] = (*(pointer+i));
	}
	//cout << "First bytes: "<< endl;
	//printVector(firstBytes, 0, rotateBytes);

	memmove(pointer, pointer + rotateBytes, (rotateBetweenBits/8) - rotateBytes);
	for (int32_t i = rotateBytes - 1; i >= 0; --i) {
		*(pointer + length - 1 - i) = firstBytes[rotateBytes - 1 - i];
	}

	//cout << "End rotation" << endl;
	//printVector(input, startingInd, length + startingInd);
}

// TODO: Take care of little-Endian/big-Endian
void forwardSubstitution(vector<uint8_t>& input, uint32_t start = 0, uint32_t end = 0) {
	if (end == 0) end = input.size();
	DASSERT((end - start) == (k1 / (sizeof(uint8_t) * 8))); // input must be 1024 bits long
	uint16_t xorValue = 0;
	for (uint32_t i = start; i < end; i += 2) {
		//cout << "Input offset_64b " << i << endl;

		uint16_t src = (input[i] << 8) | input[i+1];

		if (i != start && i % 16 == 0) {
			xorValue = 0;
		}

		uint16_t temp = sTable->getSubstitutionValue(src) ^ xorValue;	

		//cout << "Substituting " << hex << src << " to " << (temp ^ xorValue) << ". Xored with " << xorValue << " to get " << temp << dec << endl;

		xorValue = temp;

		if (i != start && i % 16 == 0) {
			//cout << "Rotation # " << (i / 16) << " : Starting at " << (i - 16) << endl;
			rotateRight(input, i - 16, 128);
		}

		input[i + 1] = temp & 0xFF;
		input[i] = temp >> 8;
	}
	rotateRight(input, end - 16, 128);
}

void reverseSubstitution(vector<uint8_t>& input, int32_t start = 0, uint32_t end = 0) {
	if (end == 0) end = input.size();
	DASSERT((end - start) == (k1 / (sizeof(uint8_t) * 8))); // input must be 1024 bits long
	uint16_t xorValue = 0;
	rotateLeft(input, end - 16, 128);
	for (int32_t i = int32_t(end - 1); i >= start; i -= 2) {
		//cout << "Input offset_64b " << i << " with start " << start << endl;

		if ((i - 1) != start && (i - 1) % 16 == 0) {
			DASSERT(i >= start+16);
			rotateLeft(input, i - 1 - 16, 128);
		}

		if ((i - 1) % 16 != 0) {
			DASSERT(i > start+2);
			xorValue = (input[i - 3] << 8) | input[i - 2];
		} else {
			xorValue = 0;
		}

		uint16_t src = ((input[i - 1] << 8) | input[i]) ^ xorValue;

		uint16_t substitutedVal = uint16_t(sTable->getSubstitutionValue(src));

		//cout << "Xored " << hex << (src ^ xorValue)  << " with " << xorValue << " to get " << src << ". Substituting " << src << " to " << substitutedVal << dec << endl;

		input[i] = substitutedVal & 0xFF;
		input[i - 1] = substitutedVal >> 8;
	}
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

	//cout << "Starting rearrangement at " << start << " to " << end << endl;

	for(uint32_t j = 0; j < 64; ++j) {
		uint32_t i = rearrangementTable[j];
		if (i < j) {
			continue;
		}
		//cout << j << ". Moving " << (j*2)+start << " to " << (i*2)+start << endl;
		uint16_t temp = input[(j*2)+start];
		input[(j*2)+start] = input[(i*2)+start];
		input[(i*2)+start] = temp;
		temp = input[(j*2)+start+1];
		input[(j*2)+start+1] = input[(i*2)+start+1];
		input[(i*2)+start+1] = temp;
	}
}

uint32_t getSuperIndex(uint32_t j, uint32_t i) { // Note: i and j should be 1-indexed
	uint32_t newIndex = i + (256 * (j - 1)) - 1;
	//cout << "Index Section " << i << " Nibble " << j << " goes to index " << newIndex << "(Section " << (newIndex / 32) << " Nibble " << (newIndex % 32) << ")" << endl; 
	return newIndex;
}

void initializeSuperRearrangementTable() {
	superRearrangementTable.resize(256*32);
	reverseSuperRearrangementTable.resize(256*32);
	for(uint32_t i = 0; i < 256*32; ++i) {
		uint32_t newInd = getSuperIndex((i % 32) + 1, (i / 32) + 1);
		superRearrangementTable[i] = newInd;
		reverseSuperRearrangementTable[newInd] = i;

	}
}

void insertBits(uint32_t nibbleIndex, bool replaceFirst4Bits, uint8_t byte, vector<uint8_t>& input, uint32_t insertionIndex) {
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

uint32_t get8bitIndex(uint32_t fourbitIndex, uint32_t i) {
	if (fourbitIndex % 2 == 0) {
		return fourbitIndex / 2;
	}
	return (fourbitIndex - 1) / 2;
}

void superRearrangement(vector<uint8_t>& input, bool forwardRearrangement) {
	DASSERT(input.size() == k32 / (sizeof(uint8_t) * 8)); // the input must be 4K bytes
	vector<uint8_t> output;
	output.resize(input.size());

	for(uint32_t i = 0; i < input.size(); ++i) {
		uint32_t newInd1;
		uint32_t newInd2;
		if (!forwardRearrangement) {
			newInd1 = reverseSuperRearrangementTable[i*2];
			newInd2 = reverseSuperRearrangementTable[(i*2)+1];
		} else {
			newInd1 = superRearrangementTable[i*2];
			newInd2 = superRearrangementTable[(i*2) + 1];
		}

		uint32_t insertionIndex1 = get8bitIndex(newInd1, i);
		uint32_t insertionIndex2 = get8bitIndex(newInd2, i);

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
	input = output;
	//cout << "Modified : " << endl;
	//printVector(input, 0, (k1 / (sizeof(uint8_t) * 8)));
}

void forwardReplaceAndFold(vector<uint8_t>& input, uint32_t start = 0, uint32_t end = 0) {
	if (end == 0) end = input.size();
	DASSERT((end - start) == k1 / (sizeof(uint8_t) * 8)); // user data must be 1024 bits
	forwardSubstitution(input, start, end);
	rearrangement(input, start, end);
}

void reverseReplaceAndFold(vector<uint8_t>& encryptedData, uint32_t start = 0, uint32_t end = 0) {
	if (end == 0) end = encryptedData.size();
	DASSERT((end - start) == k1 / (sizeof(uint8_t) * 8)); // encrypted data must be 1024 bits
	rearrangement(encryptedData, start, end);
	reverseSubstitution(encryptedData, start, end);
}

void XorEPUKwithUserData(vector<uint8_t>& userData, vector<uint8_t> EPUK) {
	DASSERT(EPUK.size() == userData.size()); // both must be 4K bytes
	for (uint32_t i = 0; i < userData.size(); ++i) {
		userData[i] = userData[i] ^ EPUK[i];
	}
}

void encryption(vector<uint8_t> userKey, vector<uint8_t>& userData) {
	vector<uint8_t> original(userData);
	int32_t OTPCount = 0;

	// 1. Pass userKey through 4 rounds of replace and fold	and then extend it
	//startTime = std::chrono::high_resolution_clock::now();
	for (uint32_t i = 0; i < 4; ++i) {
		forwardReplaceAndFold(userKey);
	}
	vector<uint8_t> EPUK = extendedKey(userKey);
	//endTime = std::chrono::high_resolution_clock::now();
	//duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	//cout << "Time taken for encryption step 1 : " << convertDurationToString(duration) << endl;

	// 2. Pass userData through 4 rounds of replace and fold+Xor with OTP&EPUK
	//startTime = std::chrono::high_resolution_clock::now();
	for (uint32_t i = 0; i < 4; ++i) {
		for (uint32_t j = 0; j < 32; ++j) {
			forwardReplaceAndFold(userData, j*elementsIn1024b, (j+1)*elementsIn1024b);
		}
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKey.size()) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKey.size()) + k + 1]; 
			OTPs->apply(userData, (k*128), (k*128+64), index1, index2);
		}
		++OTPCount;
		XorEPUKwithUserData(userData, EPUK);
	}
	//endTime = std::chrono::high_resolution_clock::now();
	//duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	//cout << "Time taken for encryption step 2 : " << convertDurationToString(duration) << endl;

	// 3. Pass userData through 1 round of super rearrangement
	superRearrangement(userData, true);

	// 4. Pass userData through 1 round of substitution+Xor with OTP&EPUK
	for (uint32_t j = 0; j < 32; ++j) {
		forwardSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b);
	}
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(OTPCount*userKey.size()) + k]; 
		uint32_t index2 = EPUK[(OTPCount*userKey.size()) + k + 1]; 
		OTPs->apply(userData, (k*128), (k*128+64), index1, index2);
	}
	++OTPCount;
	XorEPUKwithUserData(userData, EPUK);

	// 5. Pass userData through 2 rounds of replace and fold+Xor with OTP&EPUK
	for (uint32_t i = 0; i < 2; ++i) {
		for (uint32_t j = 0; j < 32; ++j) {
			forwardReplaceAndFold(userData, j*elementsIn1024b, (j+1)*elementsIn1024b);
		}
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKey.size()) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKey.size()) + k + 1]; 
			OTPs->apply(userData, (k*128), (k*128+64), index1, index2);
		}
		++OTPCount;
		XorEPUKwithUserData(userData, EPUK);
	}

	// 6. Pass userData through 1 round of super rearrangement
	superRearrangement(userData, true);

	// 7. Pass userData through 1 round of replace and fold+Xor with OTP&EPUK followed by super rearrangement
	for (uint32_t j = 0; j < 32; ++j) {
		forwardSubstitution(userData, j*elementsIn1024b, (j+1)*elementsIn1024b);
	}
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(OTPCount*userKey.size()) + k]; 
		uint32_t index2 = EPUK[(OTPCount*userKey.size()) + k + 1]; 
		OTPs->apply(userData, (k*128), (k*128+64), index1, index2);
	}
	++OTPCount;
	XorEPUKwithUserData(userData, EPUK);
	superRearrangement(userData, true);
}

void decryption(vector<uint8_t> userKey, vector<uint8_t>& encryptedData) {
	int32_t OTPCount = 7;
	// 1. Pass userKey through 4 rounds of replace and fold	and then extend it (1)
	for (uint32_t i = 0; i < 4; ++i) {
		forwardReplaceAndFold(userKey);
	}
	vector<uint8_t> EPUK = extendedKey(userKey);

	// 2. Pass userData through 1 round of replace and fold+Xor with OTP&EPUK by super rearrangement (7)
	superRearrangement(encryptedData, false);
	XorEPUKwithUserData(encryptedData, EPUK);
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(OTPCount*userKey.size()) + k]; 
		uint32_t index2 = EPUK[(OTPCount*userKey.size()) + k + 1]; 
		OTPs->apply(encryptedData, (k*128), (k*128+64), index1, index2);
	}
	--OTPCount;
	for (uint32_t j = 0; j < 32; ++j) {
		reverseSubstitution(encryptedData, j*elementsIn1024b, (j+1)*elementsIn1024b);
	}

	// 3. Pass encryptedData through 1 round of super rearrangement (6)
	superRearrangement(encryptedData, false);

	// 4. Pass encryptedData through 2 rounds of replace and fold+Xor with OTP&EPUK (5)
	for (uint32_t i = 0; i < 2; ++i) {
		XorEPUKwithUserData(encryptedData, EPUK);
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKey.size()) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKey.size()) + k + 1]; 
			OTPs->apply(encryptedData, (k*128), (k*128+64), index1, index2);
		}
		--OTPCount;
		for (uint32_t j = 0; j < 32; ++j) {
			reverseReplaceAndFold(encryptedData, j*elementsIn1024b, (j+1)*elementsIn1024b);
		}
	}

	// 5. Pass encryptedData through 1 round of substitution+Xor with OTP&EPUK (4)
	XorEPUKwithUserData(encryptedData, EPUK);
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(OTPCount*userKey.size()) + k]; 
		uint32_t index2 = EPUK[(OTPCount*userKey.size()) + k + 1]; 
		OTPs->apply(encryptedData, (k*128), (k*128+64), index1, index2);
	}
	--OTPCount;
	for (uint32_t j = 0; j < 32; ++j) {
		reverseSubstitution(encryptedData, j*elementsIn1024b, (j+1)*elementsIn1024b);
	}

	// 6. Pass encryptedData through 1 round of super rearrangement (3)
	superRearrangement(encryptedData, false);

	// 7. Pass encryptedData through 4 rounds of replace and fold+Xor with OTP&EPUK (2)
	for (uint32_t i = 0; i < 4; ++i) {
		XorEPUKwithUserData(encryptedData, EPUK);
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKey.size()) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKey.size()) + k + 1]; 
			OTPs->apply(encryptedData, (k*128), (k*128+64), index1, index2);
		}
		--OTPCount;
		for (uint32_t j = 0; j < 32; ++j) {
			reverseReplaceAndFold(encryptedData, j*elementsIn1024b, (j+1)*elementsIn1024b);
		}
	}
}

vector<uint8_t> extendedKey(vector<uint8_t> PUK) { // extends 1024-bit PUK to 4K bytes
	DASSERT(PUK.size() == k1 / (sizeof(uint8_t) * 8)); // PUK must be 1024 bits
	vector<uint8_t> EPUK;
	EPUK.resize(k32 / (sizeof(uint8_t) * 8));
	for (uint32_t i = 0; i < 32; ++i) {
		rotateLeft(PUK, 0, PUK.size()*sizeof(uint8_t)*8, 1); 
		for (uint32_t j = 0; j < PUK.size(); ++j) {
			EPUK[j+(i*PUK.size())] = PUK[j];
		}
	}
	return EPUK;
}

int main() {
	std::chrono::high_resolution_clock::time_point startTime;
	std::chrono::high_resolution_clock::time_point endTime;
	std::chrono::duration<double, std::nano> duration;

	initialize();

	// TEST 1: Test OTP generation and apply function ===================================================
	uint32_t size = k1 / (sizeof(uint8_t) * 8);
	vector<uint8_t> original;
	original.resize(size);
	for (uint32_t i = 0; i < size; ++i) {
		original[i] = rand() & 0xFF;
	}

	vector<uint8_t> input(original);
	uint32_t startingInd = 2;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	OTPs->apply(input, 2, 5, startingInd, startingInd + 8); // "Encypt" the input
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	OTPs->apply(input, 2, 5, startingInd, startingInd + 8); // "Decrypt" the input
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

	startTime = std::chrono::high_resolution_clock::now();
	rotateLeft(input, 16, 128, 2);
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken to left shift a 1024-bit section of user data : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	startTime = std::chrono::high_resolution_clock::now();
	rotateRight(input, 16, 128, 2);
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
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

	startTime = std::chrono::high_resolution_clock::now();
	forwardSubstitution(input, start, start+elementsIn1024b);
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken to forward substitute a 1024-bit section of user data : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	startTime = std::chrono::high_resolution_clock::now();
	reverseSubstitution(input, start, start+elementsIn1024b);
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken to reverse substitute a 1024-bit section of user data : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << "Test 3 is complete. Substition algorithm works; it can encrypt and decrypt data" << endl;

	// TEST 4: Test rearrangement function ==============================================================
	start = 3968;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	startTime = std::chrono::high_resolution_clock::now();
	rearrangement(input, start, start+elementsIn1024b);
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken to rearrange 1024-bit section of user data : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	rearrangement(input, start, start+elementsIn1024b);

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << "Test 4 is complete. Rearrangement algorithm works" << endl;

	// TEST 5: Test superRearrangement function ==============================================================
	/*********************************************************************************************************
	// Test insertBits function
	vector<uint8_t> test;
	test.resize(1);
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
	 ********************************************************************************************************/

	// Test superRearrangement function
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	startTime = std::chrono::high_resolution_clock::now();
	superRearrangement(input, true);
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken to super rearrange user data : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	startTime = std::chrono::high_resolution_clock::now();
	superRearrangement(input, false);
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken to reverse super rearrangement of user data : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << "Test 5 is complete. Super Rearrangement algorithm works" << endl;

	// TEST 6: Test forward and reverse replace and fold function ============================================
	// Test replace and fold on 4K byte input
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	startTime = std::chrono::high_resolution_clock::now();
	for(uint32_t i = 0; i < 32; ++i) {
		forwardReplaceAndFold(input, i*elementsIn1024b, (i+1)*elementsIn1024b);
	}
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken to pass user data through 1 round of forward replace and fold (forwardRepalceAndFold called 32 times) : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	startTime = std::chrono::high_resolution_clock::now();
	for(uint32_t i = 0; i < 32; ++i) {
		reverseReplaceAndFold(input, i*elementsIn1024b, (i+1)*elementsIn1024b);
	}
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
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

	startTime = std::chrono::high_resolution_clock::now();
	for(uint32_t i = 0; i < 4; ++i) {
		forwardReplaceAndFold(puk, 0, size);
	}
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken to process user key (i.e. 4 rounds of replace and fold) : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(userKey.data()), reinterpret_cast<const char*>(puk.data()), size) != 0);
	cout << "reversing" << endl;
	for(uint32_t i = 0; i < 4; ++i) {
		reverseReplaceAndFold(puk, 0, size);
	}
	assert(memcmp(reinterpret_cast<const char*>(userKey.data()), reinterpret_cast<const char*>(puk.data()), size) == 0);

	cout << "Test 6 is complete. Replace and fold algorithm works (for forward and reverse)" << endl;

	// TEST 7: Test extendedKey function =====================================================================
	startTime = std::chrono::high_resolution_clock::now();
	vector<uint8_t> EPUK = extendedKey(puk);
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
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

	startTime = std::chrono::high_resolution_clock::now();
	XorEPUKwithUserData(copy, EPUK);
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken for Xoring user data and EPUK : " << convertDurationToString(duration) << endl;

	startTime = std::chrono::high_resolution_clock::now();
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(1*userKey.size()) + k]; 
		uint32_t index2 = EPUK[(1*userKey.size()) + k + 1]; 
		OTPs->apply(copy, (k*128), (k*128+64), index1, index2);
	}
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken for Xoring user data and OTP : " << convertDurationToString(duration) << endl;

	startTime = std::chrono::high_resolution_clock::now();
	for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
		uint32_t index1 = EPUK[(1*userKey.size()) + k]; 
		uint32_t index2 = EPUK[(1*userKey.size()) + k + 1]; 
		OTPs->apply(copy, (k*128), (k*128+64), index1, index2);
	}
	XorEPUKwithUserData(copy, EPUK);
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken for Xoring user data, OTP, and EPUK : " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << "Test 8 is complete. User Data can be xored with EPUK and OTP" << endl;

	// TEST 9: Test Encryption step 2 =========================================================================
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	int32_t i = 1;

	startTime = std::chrono::high_resolution_clock::now();
	for(int32_t OTPCount = 0; OTPCount < i; ++OTPCount) {
		for (uint32_t j = 0; j < 32; ++j) {
			forwardReplaceAndFold(input, j*elementsIn1024b, (j+1)*elementsIn1024b);
		}
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKey.size()) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKey.size()) + k + 1]; 
			OTPs->apply(input, (k*128), (k*128+64), index1, index2);
		}
		XorEPUKwithUserData(input, EPUK);
	}
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken for encrypting user data (4K Bytes) through " << i << " rounds of replace and fold + Xor with EPUK and UTP: " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	startTime = std::chrono::high_resolution_clock::now();
	for(int32_t OTPCount = i-1; OTPCount >= 0; --OTPCount) {
		XorEPUKwithUserData(input, EPUK);
		for (uint32_t k = 0; k < 4*8; ++k) { // We xor OTPs 128-bytes at a time
			uint32_t index1 = EPUK[(OTPCount*userKey.size()) + k]; 
			uint32_t index2 = EPUK[(OTPCount*userKey.size()) + k + 1]; 
			OTPs->apply(input, (k*128), (k*128+64), index1, index2);
		}
		for (uint32_t j = 0; j < 32; ++j) {
			reverseReplaceAndFold(input, j*elementsIn1024b, (j+1)*elementsIn1024b);
		}
	}
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken for decrypting user data (4K Bytes) through " << i << " rounds of replace and fold + Xor with EPUK and UTP: " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Test 9 is complete. EPUK and OTP-Xor works (even with rotation and replace&fold/tested with " << i << ")" << endl;


	// TEST 10: Final Test: Put everything together ===========================================================
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	startTime = std::chrono::high_resolution_clock::now();
	encryption(puk, input);
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken to encryption user data: " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);

	startTime = std::chrono::high_resolution_clock::now();
	decryption(puk, input);
	endTime = std::chrono::high_resolution_clock::now();
	duration = std::chrono::duration_cast<std::chrono::duration<double, std::nano>>(endTime - startTime);
	cout << "Time taken to decryption user data: " << convertDurationToString(duration) << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);

	cout << "DONE" << endl;

	return 0;
}
