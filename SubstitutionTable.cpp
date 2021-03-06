#include <assert.h>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <vector>

using namespace std;

constexpr uint32_t k64 = 64*1024;

vector<int16_t> substitutionTable;
vector<int16_t> reverseSubstitutionTable;
vector<int8_t> galoisTable;

void initializeGaloisTable() {
	for (uint16_t i = 0; i < 256; ++i) {
		galoisTable.at(i) = (i << 1) ^ ((i & 0x80) ? 0x1d : 0);
	}
}

void initialize() {
	substitutionTable.resize(k64);
	fill(substitutionTable.begin(), substitutionTable.end(), -1);
	reverseSubstitutionTable.resize(k64);
	fill(reverseSubstitutionTable.begin(), reverseSubstitutionTable.end(), -1);
	galoisTable.resize(256);
	initializeGaloisTable();
}

void printReverseTable(uint32_t start = 0, uint32_t end = k64) {
	for(uint32_t i = start; i < end; ++i) {
		cout << hex << i << " => " << reverseSubstitutionTable.at(i) << dec << endl;
	}
}

void storeSubsTable(vector<int16_t>& table, string fileName) {
	ofstream myfile;
	myfile.open (fileName);
	for(uint32_t i = 0; i < table.size(); ++i) {
		myfile << "0x" << hex << table.at(i) << dec << ", "; 
	}
	myfile.close();
}

void printTable(uint32_t start = 0, uint32_t end = k64) {
	for(uint32_t i = start; i < end; ++i) {
		cout << hex << i << " => " << substitutionTable.at(i) << dec << endl;
	}
}

void printGaloisTable(uint32_t start = 0, uint32_t end = 256) {
	for(uint32_t i = start; i < end; ++i) {
		cout << hex << i << " => " << uint16_t(galoisTable.at(i)) << dec << endl;
	}
}

uint16_t computeSubstitution(uint16_t val) {
	uint8_t b = (val & 0xFF);
	uint8_t a = (val >> 8);

	cout << hex << "GF tranalation of: " << (uint16_t)a << " and " << (uint16_t)b << " is ";
	a = (a << 1) ^ ((a & 0x80) ? 0x1d : 0);// Galois field multiplication on a
	b = (b << 1) ^ ((b & 0x80) ? 0x1d : 0);// Galois field multiplecation on b

	cout << (uint16_t)a << " and " << (uint16_t)b << " 2nd level of " << (uint16_t)(b ^ a) << " and " << (uint16_t)a << " is ";

	b ^= a;

	a = (a << 1) ^ ((a & 0x80) ? 0x1d : 0);// Galois field multiplication on a
	b = (b << 1) ^ ((b & 0x80) ? 0x1d : 0);// Galois field multiplecation on b

	cout << (uint16_t)a << " and " << (uint16_t)b << " with final value " << (uint16_t)(a ^ b) << " and " << (uint16_t)b << dec << endl;
	a ^= b;

	return (uint16_t)(((uint16_t)(a) << 8) | b);
}

void createTable() {
	uint8_t* a = reinterpret_cast<uint8_t*>(galoisTable.data()) + 2;
	uint8_t* b = a + 47;
	uint8_t startingA = *a;
	uint8_t startingB = *b;
	uint8_t lastValue = galoisTable.at(255);
	for (uint32_t i = 0; i < k64; ++i) {
		//cout << hex << i << " " << uint16_t(*a) << " : " << uint16_t(*b)  << " => " << uint16_t((uint16_t(*a) << 8) | (*b)) << endl;
		uint16_t substitutionValue = (uint16_t(*a) << 8) | (*b);

		if (reverseSubstitutionTable.at(substitutionValue) != -1) {
			cout << "Oops " << hex << i << " and " << dec << hex << uint16_t(reverseSubstitutionTable.at(substitutionValue)) << dec;
			cout << " both mapped to " << hex << substitutionValue << dec << endl;
			cout << "Current a: " << hex << uint16_t(*a) << " and b: " << uint16_t(*b) << dec << endl;
			assert(0);
		}

		assert(reverseSubstitutionTable.at(substitutionValue) == -1);

		substitutionTable.at(i) = substitutionValue;
		reverseSubstitutionTable.at(substitutionValue) = i;

		if (*a == lastValue) {
			a = reinterpret_cast<uint8_t*>(galoisTable.data());
		} else {
			++a;
		}
		if ((i & 0xFF) == 0xFF) {
			//cout << "Reached " << hex << i << ". Old starting value: " << (uint16_t)startingA << " and " 
			//<< (uint16_t)startingB << endl;
			//cout << "Note: b+1 : " << hex << uint16_t(*(b+1)) << dec << endl;
			//cout << "Note: a+1 : " << hex << uint16_t(*(a+1)) << dec << endl;
			//cout << "Note: b-1 : " << hex << uint16_t(*(b-1)) << dec << endl;
			//cout << "Note: a-1 : " << hex << uint16_t(*(a-1)) << dec << endl;
			//cout << "Note: b : " << hex << uint16_t(*(b)) << dec << endl;

			assert(*a == startingA);
			if (*a == lastValue) {
				a = reinterpret_cast<uint8_t*>(galoisTable.data());
			} else {
				++a;
			}
			assert(*(b+1) == startingB);
			if (*b == galoisTable.at(0)) {
				b = reinterpret_cast<uint8_t*>(galoisTable.data()+255);
			} else {
				--b;
			}
			//cout << ". New: " << (uint16_t)(*a) << " and " << uint16_t(*(b+2)) << endl;

			startingA = *a;
			startingB = *b;
		} else {
			if (*b == lastValue) {
				b = reinterpret_cast<uint8_t*>(galoisTable.data());
			} else {
				++b;
			}
			assert(*a != startingA);
			assert(*b != startingB);
		}
	}
}

void createTableOLD() {
	for(uint32_t i = 0; i < k64; ++i) {
		if (substitutionTable.at(i) != -1) {
			continue;
		}
		uint16_t substitutedValue = computeSubstitution(i);
		if(substitutionTable.at(substitutedValue) != -1) { // No other number should have the same substitution value
			printTable(0, i);
			cout << hex << i << " mapped to " << substitutedValue << " which was already taken by " << substitutionTable.at(substitutedValue) << dec << endl;
			assert(false);
		}
		substitutionTable.at(i) = substitutedValue;
		substitutionTable.at(substitutedValue) = i;
	}
}

void correctSpread() {
	int32_t j = -1;
	for (int32_t i = 0; i < k64; ++i) {
		uint16_t val = substitutionTable.at(i);
		if ((val & 0xFF) == (i & 0xFF) || (val & 0xFF00) == (i & 0xFF00)) {
			if (j != -1) {
				uint16_t temp = substitutionTable.at(j);
				if ((val & 0xFF) == (j & 0xFF) || (val & 0xFF00) == (j & 0xFF00) || 
						(temp & 0xFF) == (i & 0xFF) || (temp & 0xFF00) == (i & 0xFF00)) {
				} else {
					reverseSubstitutionTable.at(temp) = i;
					reverseSubstitutionTable.at(val) = j;
					substitutionTable.at(j) = val;
					substitutionTable.at(i) = temp;
					j = -1;
				}
			} else {
				j = i;
			}
		}
	}
}

void verifySpread() {
	cout << "Pairs with common bytes: " << endl;
	for (uint32_t i = 0; i < k64; ++i) {
		uint16_t val = substitutionTable.at(i);
		if ((val & 0xFF) == (i & 0xFF) || (val & 0xFF00) == (i & 0xFF00)) {
			cout << hex << i << " and " << substitutionTable.at(i) << endl;
		}
	}
}

void verifyReverse() {
	cout << "Pairs that don't reverse : " << endl;
	for (uint32_t i = 0; i < k64; ++i) {
		uint16_t val = substitutionTable.at(i);
		if ((uint16_t)(reverseSubstitutionTable.at(val)) != i && reverseSubstitutionTable.at(val) != -1 ) {
			cout << hex << i << " substitutes to " << val << " which reverses to " << reverseSubstitutionTable.at(val) << "." << endl;
		}
	}
}

void ultraSpecialCase() {
	// I know for a fact that currently after doing verfySpread twice, c1a2 is the only value who has a byte with its 
	// substituted value. So, here I have hard-coded a swap between c1a2 and fff0, as a way to ensure no number has any byte in 
	// common with its substituted value.
	uint16_t i = 0xc1a2;
	uint16_t j = 0xfff0;
	uint16_t val = substitutionTable.at(i);
	uint16_t temp = substitutionTable.at(j);
	if ((val & 0xFF) == (j & 0xFF) || (val & 0xFF00) == (j & 0xFF00) || 
			(temp & 0xFF) == (i & 0xFF) || (temp & 0xFF00) == (i & 0xFF00)) {
		assert(false);
	}
	substitutionTable.at(j) = val;
	substitutionTable.at(i) = temp;
	reverseSubstitutionTable.at(val) = j;
	reverseSubstitutionTable.at(temp) = i;

}

int main() {
	initialize();
	//	printGaloisTable();
	createTable();
	correctSpread();
	correctSpread();
	ultraSpecialCase();
	verifyReverse();
	verifySpread();
	//	printTable();
	storeSubsTable(substitutionTable, "forwardSubsTable.txt");
	storeSubsTable(reverseSubstitutionTable, "reverseSubsTable.txt");
	return 0;
}
