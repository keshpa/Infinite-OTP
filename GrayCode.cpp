#include <assert.h>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <tuple>
#include <unistd.h>
#include <vector>

using namespace std;

constexpr int64_t k64 = 1024*64;

int32_t replacement[k64];

int32_t grayCode[k64];

void printGrayCode() {
	for (int32_t i = 0; i < k64; ++i) {
		bitset<32> b(grayCode[i]);
		cout << b.to_string() << endl;
	}
}

void buildGrayCode(int32_t numBits) {
	for (int32_t i = 0; i < (1 << numBits); ++i) {
		int32_t val = i ^ (i >> 1);
		grayCode[i] = val;
	}
}

void verifyGrayCode(int32_t numBits) {
	for (int32_t i = 1; i < (1 << numBits); ++i) {
		int32_t bitsChanged = grayCode[i] ^ grayCode[i-1];
		bitset<32> b1(bitsChanged);
		if (b1.count() != 1) {
			bitset<32> b2(grayCode[i]);
			bitset<32> b3(grayCode[i-1]);
			cout << "======================" << endl << b2.to_string() << endl << b3.to_string() << endl << "====================" << endl;
		}
		assert (b1.count() == 1);
	}
}

vector<tuple<int32_t, vector<int32_t>>> compatibilityList;

void computeCompatibility() {
	for (int32_t i = 0; i < k64; ++i) {
		vector<int32_t> compatible;
		for (int32_t j = i+1; j < k64; ++j) {
			int32_t bitsChanged = i ^ grayCode[j];
			if ((bitsChanged & 0x0F) && (bitsChanged & 0xF0) && (bitsChanged & 0xF00) && (bitsChanged & 0xF000)) {
				compatible.push_back(grayCode[j]);
			}
		}
		int32_t k = i;
		compatibilityList.push_back(make_tuple<int32_t, vector<int32_t>>(move(k), move(compatible)));
	}
}

bool findPerfectMatch(int32_t listIndex) {
	if (listIndex > compatibilityList.size()) { // I will keep this if for now, but it will never evaluate to true
		return true;
	}
	if (listIndex+1 == compatibilityList.size() && replacement[get<0>(compatibilityList.at(listIndex))] != -1) { // If we reached the last element and the last element has been "claimed" by someone, then we have created a substitution table that propopoages a bit-change accross 16-bits
		return true;
	}

	int32_t compatIndex = 0;
	bool selfMatchFound = false;

	while (compatIndex < get<1>(compatibilityList.at(listIndex)).size()) {
		if (replacement[get<0>(compatibilityList.at(listIndex))] == -1) {
			while (replacement[get<1>(compatibilityList.at(listIndex)).at(compatIndex)] != -1) {
				++compatIndex;
				if (compatIndex == get<1>(compatibilityList.at(listIndex)).size()) {
					return false;
				}
			}
			replacement[get<0>(compatibilityList.at(listIndex))] = get<1>(compatibilityList.at(listIndex)).at(compatIndex);
			replacement[get<1>(compatibilityList.at(listIndex)).at(compatIndex)] = get<0>(compatibilityList.at(listIndex));
			selfMatchFound = true;
		}

		bool success = findPerfectMatch(listIndex+1);
		if (success) {
			return true;
		}

		if (not selfMatchFound && not success) {
			return false;
		}

		if (selfMatchFound) {
			replacement[get<0>(compatibilityList.at(listIndex))] = -1;
			replacement[get<1>(compatibilityList.at(listIndex)).at(compatIndex)] = -1;
		}
		++compatIndex;
		selfMatchFound = false;
	}

	return false;
}

int main() {
	for (int32_t i = 0; i < k64; ++i) {
		replacement[i] = -1;
		grayCode[i] = -1;
	}

	buildGrayCode(16);
	verifyGrayCode(16);
	//	printGrayCode();
	computeCompatibility();

	/************************************************************
	vector<int32_t> vec = {1, 3, 21, 65534};
	compatibilityList.push_back(make_tuple(0, vec));
	vec.clear();
	vec = {21, 32, 65534};
	compatibilityList.push_back(make_tuple(19, vec));
	vec.clear();
	vec = {31};
	compatibilityList.push_back(make_tuple(21, vec));
	vec.clear();
	compatibilityList.push_back(make_tuple(65535, vec));

	for (auto& entry : compatibilityList) {
		int32_t index = get<0>(entry);
		vector<int32_t> &v = get<1>(entry);

		cout << "Compatibility of " << index << " : ";
		for (auto index : v) {
			cout << index << ", ";
		}
		cout << endl;
	}
	************************************************************/
	cout << "Checking compatibility equation ..." << endl;

	if (findPerfectMatch(0)) {
		cout << "Good news. Hard work pays!" << endl;
		for (int32_t k = 0; k < k64; ++k) {
			cout << setw(10) << k << " " << replacement[k] << endl;
			int32_t bitsChanged = k ^ replacement[k];
			assert ((bitsChanged & 0x0F) && (bitsChanged & 0xF0) && (bitsChanged & 0xF00) && (bitsChanged & 0xF000));
			assert (k == replacement[replacement[k]]);
		}
	} else {
		cout << "Bad news, but I tried really hard !" << endl;
	}
	return 0;
}
