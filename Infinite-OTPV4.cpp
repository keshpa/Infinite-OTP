#include <assert.h>
#include <chrono>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <string.h>
#include <vector>
#include <boost/preprocessor/arithmetic/mul.hpp>

using namespace std;

constexpr uint16_t k1 = 1024;
constexpr uint16_t k4 = 4*k1;
constexpr uint32_t k64 = 64*k1;

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

vector<uint16_t> substitutionTable;
vector<uint16_t> reverseSubstitutionTable;
vector<vector<uint16_t>> OTPs;
vector<uint16_t> EPUK;

/* The following macros help create variables to short each short in 4K input. portionN is the 1024-bit portion number, sectionN is the 
 * 128-bit section number within the given portion, and shortN is the short position within the given section. All these variables must 
 * be 0-indexed.
 */

#define PORTION_SIZE			1024	/* bits */
#define SECTION_SIZE			128	/* bits */

#define SWAP_SHORTS(...)	SWAP_SHORTS_(__VA_ARGS__)

#define SWAP_SHORTS_(a, b)				\
	do {						\
		uint16_t temp = a;			\
		a = b;					\
		b = temp;				\
	} while(0)

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

#define S(ptr, portionN, sectionN, shortN)	(reinterpret_cast<uint16_t *>(ptr)+(portionN*(PORTION_SIZE/(sizeof(uint16_t)*8)))+(sectionN*(SECTION_SIZE/(sizeof(uint16_t)*8)))+shortN)

#define V_impl(type, portionN, sectionN, shortN)		v ## _ ## type ## _ ## portionN ## _ ## sectionN ## _ ## shortN 
#define V_impl1(type, portionN, sectionN, shortN)		V_impl(type, portionN, sectionN, shortN)
//#define V(type, portionN, sectionN, shortN)			V_impl1(type, portionN, sectionN, shortN)	
#define V(...)							V_impl1(__VA_ARGS__)	

#define XOR_OTP_EPUK_PORTION(portionN, EPUK_PORTION, EPUK_SECTION, EPUK_SHORT)							\
{																\
	XOR_OTP_EPUK_FOUR_SECTIONS(portionN, 0, EPUK_PORTION, EPUK_SECTION, EPUK_SHORT);					\
	XOR_OTP_EPUK_FOUR_SECTIONS(portionN, 4, EPUK_PORTION, EPUK_SECTION, ADD(EPUK_SHORT, 1));				\
}

#define XOR_OTP_EPUK_FOUR_SECTIONS(portionN, sectionN, EPUK_PORTION, EPUK_SECTION, EPUK_SHORT)					\
	do {																\
		uint16_t OTP_INDEX = V(k, EPUK_PORTION, EPUK_SECTION, EPUK_SHORT);							\
		V(d, portionN, sectionN, 0) ^= V(k, portionN, sectionN, 0) ^ OTPs[OTP_INDEX][0];					\
		V(d, portionN, sectionN, 1) ^= V(k, portionN, sectionN, 1) ^ OTPs[OTP_INDEX][1];					\
		V(d, portionN, sectionN, 2) ^= V(k, portionN, sectionN, 2) ^ OTPs[OTP_INDEX][2];					\
		V(d, portionN, sectionN, 3) ^= V(k, portionN, sectionN, 3) ^ OTPs[OTP_INDEX][3];					\
		V(d, portionN, sectionN, 4) ^= V(k, portionN, sectionN, 4) ^ OTPs[OTP_INDEX][4];					\
		V(d, portionN, sectionN, 5) ^= V(k, portionN, sectionN, 5) ^ OTPs[OTP_INDEX][5];					\
		V(d, portionN, sectionN, 6) ^= V(k, portionN, sectionN, 6) ^ OTPs[OTP_INDEX][6];					\
		V(d, portionN, sectionN, 7) ^= V(k, portionN, sectionN, 7) ^ OTPs[OTP_INDEX][7];					\
		\
		V(d, portionN, ADD(sectionN, 1), 0) ^= V(k, portionN, ADD(sectionN, 1), 0) ^ OTPs[OTP_INDEX][8];			\
		V(d, portionN, ADD(sectionN, 1), 1) ^= V(k, portionN, ADD(sectionN, 1), 1) ^ OTPs[OTP_INDEX][9];			\
		V(d, portionN, ADD(sectionN, 1), 2) ^= V(k, portionN, ADD(sectionN, 1), 2) ^ OTPs[OTP_INDEX][10];			\
		V(d, portionN, ADD(sectionN, 1), 3) ^= V(k, portionN, ADD(sectionN, 1), 3) ^ OTPs[OTP_INDEX][11];			\
		V(d, portionN, ADD(sectionN, 1), 4) ^= V(k, portionN, ADD(sectionN, 1), 4) ^ OTPs[OTP_INDEX][12];			\
		V(d, portionN, ADD(sectionN, 1), 5) ^= V(k, portionN, ADD(sectionN, 1), 5) ^ OTPs[OTP_INDEX][13];			\
		V(d, portionN, ADD(sectionN, 1), 6) ^= V(k, portionN, ADD(sectionN, 1), 6) ^ OTPs[OTP_INDEX][14];			\
		V(d, portionN, ADD(sectionN, 1), 7) ^= V(k, portionN, ADD(sectionN, 1), 7) ^ OTPs[OTP_INDEX][15];			\
		\
		V(d, portionN, ADD(sectionN, 2), 0) ^= V(k, portionN, ADD(sectionN, 2), 0) ^ OTPs[OTP_INDEX][16];			\
		V(d, portionN, ADD(sectionN, 2), 1) ^= V(k, portionN, ADD(sectionN, 2), 1) ^ OTPs[OTP_INDEX][17];			\
		V(d, portionN, ADD(sectionN, 2), 2) ^= V(k, portionN, ADD(sectionN, 2), 2) ^ OTPs[OTP_INDEX][18];			\
		V(d, portionN, ADD(sectionN, 2), 3) ^= V(k, portionN, ADD(sectionN, 2), 3) ^ OTPs[OTP_INDEX][19];			\
		V(d, portionN, ADD(sectionN, 2), 4) ^= V(k, portionN, ADD(sectionN, 2), 4) ^ OTPs[OTP_INDEX][20];			\
		V(d, portionN, ADD(sectionN, 2), 5) ^= V(k, portionN, ADD(sectionN, 2), 5) ^ OTPs[OTP_INDEX][21];			\
		V(d, portionN, ADD(sectionN, 2), 6) ^= V(k, portionN, ADD(sectionN, 2), 6) ^ OTPs[OTP_INDEX][22];			\
		V(d, portionN, ADD(sectionN, 2), 7) ^= V(k, portionN, ADD(sectionN, 2), 7) ^ OTPs[OTP_INDEX][23];			\
		\
		V(d, portionN, ADD(sectionN, 3), 0) ^= V(k, portionN, ADD(sectionN, 3), 0) ^ OTPs[OTP_INDEX][24];			\
		V(d, portionN, ADD(sectionN, 3), 1) ^= V(k, portionN, ADD(sectionN, 3), 1) ^ OTPs[OTP_INDEX][25];			\
		V(d, portionN, ADD(sectionN, 3), 2) ^= V(k, portionN, ADD(sectionN, 3), 2) ^ OTPs[OTP_INDEX][26];			\
		V(d, portionN, ADD(sectionN, 3), 3) ^= V(k, portionN, ADD(sectionN, 3), 3) ^ OTPs[OTP_INDEX][27];			\
		V(d, portionN, ADD(sectionN, 3), 4) ^= V(k, portionN, ADD(sectionN, 3), 4) ^ OTPs[OTP_INDEX][28];			\
		V(d, portionN, ADD(sectionN, 3), 5) ^= V(k, portionN, ADD(sectionN, 3), 5) ^ OTPs[OTP_INDEX][29];			\
		V(d, portionN, ADD(sectionN, 3), 6) ^= V(k, portionN, ADD(sectionN, 3), 6) ^ OTPs[OTP_INDEX][30];			\
		V(d, portionN, ADD(sectionN, 3), 7) ^= V(k, portionN, ADD(sectionN, 3), 7) ^ OTPs[OTP_INDEX][31];			\
	} while(0)

#define XOR_OTP_EPUK_PORTION_ALL(roundNumber)											\
	XOR_OTP_EPUK_PORTION(0, roundNumber, 0, 0);										\
	XOR_OTP_EPUK_PORTION(1, roundNumber, 0, 2);										\
	XOR_OTP_EPUK_PORTION(2, roundNumber, 0, 4);										\
	XOR_OTP_EPUK_PORTION(3, roundNumber, 0, 6);										\
	XOR_OTP_EPUK_PORTION(4, roundNumber, 1, 0);										\
	XOR_OTP_EPUK_PORTION(5, roundNumber, 1, 2);										\
	XOR_OTP_EPUK_PORTION(6, roundNumber, 1, 4);										\
	XOR_OTP_EPUK_PORTION(7, roundNumber, 1, 6);										\
	XOR_OTP_EPUK_PORTION(8, roundNumber, 2, 0);										\
	XOR_OTP_EPUK_PORTION(9, roundNumber, 2, 2);										\
	XOR_OTP_EPUK_PORTION(10, roundNumber, 2, 4);										\
	XOR_OTP_EPUK_PORTION(11, roundNumber, 2, 6);										\
	XOR_OTP_EPUK_PORTION(12, roundNumber, 3, 0);										\
	XOR_OTP_EPUK_PORTION(13, roundNumber, 3, 2);										\
	XOR_OTP_EPUK_PORTION(14, roundNumber, 3, 4);										\
	XOR_OTP_EPUK_PORTION(15, roundNumber, 3, 6);										\
	XOR_OTP_EPUK_PORTION(16, roundNumber, 4, 0);										\
	XOR_OTP_EPUK_PORTION(17, roundNumber, 4, 2);										\
	XOR_OTP_EPUK_PORTION(18, roundNumber, 4, 4);										\
	XOR_OTP_EPUK_PORTION(19, roundNumber, 4, 6);										\
	XOR_OTP_EPUK_PORTION(20, roundNumber, 5, 0);										\
	XOR_OTP_EPUK_PORTION(21, roundNumber, 5, 2);										\
	XOR_OTP_EPUK_PORTION(22, roundNumber, 5, 4);										\
	XOR_OTP_EPUK_PORTION(23, roundNumber, 5, 6);										\
	XOR_OTP_EPUK_PORTION(24, roundNumber, 6, 0);										\
	XOR_OTP_EPUK_PORTION(25, roundNumber, 6, 2);										\
	XOR_OTP_EPUK_PORTION(26, roundNumber, 6, 4);										\
	XOR_OTP_EPUK_PORTION(27, roundNumber, 6, 6);										\
	XOR_OTP_EPUK_PORTION(28, roundNumber, 7, 0);										\
	XOR_OTP_EPUK_PORTION(29, roundNumber, 7, 2);										\
	XOR_OTP_EPUK_PORTION(30, roundNumber, 7, 4);										\
	XOR_OTP_EPUK_PORTION(31, roundNumber, 7, 6)

#define ROTATE_SECTION_LEFT(type, portionN, sectionN)										\
	do {															\
		/* S0 - S1 - S2 - S3 - S4 - S5 - S6 - S7 */									\
		uint16_t prev, next;												\
		prev = (V(type, portionN, sectionN, 0) & 0xFF00) >> 8;								\
		next = (V(type, portionN, sectionN, 7) & 0xFF00) >> 8;								\
		\
		V(type, portionN, sectionN, 7) = (V(type, portionN, sectionN, 7) << 8) | prev;					\
		prev = next;													\
		next = (V(type, portionN, sectionN, 6) & 0xFF00) >> 8;								\
		\
		V(type, portionN, sectionN, 6) = (V(type, portionN, sectionN, 6) << 8) | prev;					\
		prev = next;													\
		next = (V(type, portionN, sectionN, 5) & 0xFF00) >> 8;								\
		\
		V(type, portionN, sectionN, 5) = (V(type, portionN, sectionN, 5) << 8) | prev;					\
		prev = next;													\
		next = (V(type, portionN, sectionN, 4) & 0xFF00) >> 8;								\
		\
		V(type, portionN, sectionN, 4) = (V(type, portionN, sectionN, 4) << 8) | prev;					\
		prev = next;													\
		next = (V(type, portionN, sectionN, 3) & 0xFF00) >> 8;								\
		\
		V(type, portionN, sectionN, 3) = (V(type, portionN, sectionN, 3) << 8) | prev;					\
		prev = next;													\
		next = (V(type, portionN, sectionN, 2) & 0xFF00) >> 8;								\
		\
		V(type, portionN, sectionN, 2) = (V(type, portionN, sectionN, 2) << 8) | prev;					\
		prev = next;													\
		next = (V(type, portionN, sectionN, 1) & 0xFF00) >> 8;								\
		\
		V(type, portionN, sectionN, 1) = (V(type, portionN, sectionN, 1) << 8) | prev;					\
		prev = next;													\
		\
		V(type, portionN, sectionN, 0) = (V(type, portionN, sectionN, 0) << 8) | prev;					\
	} while(0)

#define ROTATE_SECTION_RIGHT(type, portionN, sectionN)										\
	do {															\
		/* S0 - S1 - S2 - S3 - S4 - S5 - S6 - S7 */									\
		uint16_t prev, next;												\
		prev = (V(type, portionN, sectionN, 7) & 0xFF) << 8;								\
		next = (V(type, portionN, sectionN, 0) & 0xFF) << 8;								\
		\
		V(type, portionN, sectionN, 0) = (V(type, portionN, sectionN, 0) >> 8) | prev;					\
		prev = next;													\
		next = (V(type, portionN, sectionN, 1) & 0xFF) << 8;								\
		\
		V(type, portionN, sectionN, 1) = (V(type, portionN, sectionN, 1) >> 8) | prev;					\
		prev = next;													\
		next = (V(type, portionN, sectionN, 2) & 0xFF) << 8;								\
		\
		V(type, portionN, sectionN, 2) = (V(type, portionN, sectionN, 2) >> 8) | prev;					\
		prev = next;													\
		next = (V(type, portionN, sectionN, 3) & 0xFF) << 8;								\
		\
		V(type, portionN, sectionN, 3) = (V(type, portionN, sectionN, 3) >> 8) | prev;					\
		prev = next;													\
		next = (V(type, portionN, sectionN, 4) & 0xFF) << 8;								\
		\
		V(type, portionN, sectionN, 4) = (V(type, portionN, sectionN, 4) >> 8) | prev;					\
		prev = next;													\
		next = (V(type, portionN, sectionN, 5) & 0xFF) << 8;								\
		\
		V(type, portionN, sectionN, 5) = (V(type, portionN, sectionN, 5) >> 8) | prev;					\
		prev = next;													\
		next = (V(type, portionN, sectionN, 6) & 0xFF) << 8;								\
		\
		V(type, portionN, sectionN, 6) = (V(type, portionN, sectionN, 6) >> 8) | prev;					\
		prev = next;													\
		\
		V(type, portionN, sectionN, 7) = (V(type, portionN, sectionN, 7) >> 8) | prev;					\
	} while (0)

#define ROTATE_PORTION_LEFT(type, portionN) 											\
	ROTATE_SECTION_LEFT(type, portionN, 0);											\
ROTATE_SECTION_LEFT(type, portionN, 1);											\
ROTATE_SECTION_LEFT(type, portionN, 2);											\
ROTATE_SECTION_LEFT(type, portionN, 3);											\
ROTATE_SECTION_LEFT(type, portionN, 4);											\
ROTATE_SECTION_LEFT(type, portionN, 5);											\
ROTATE_SECTION_LEFT(type, portionN, 6);											\
ROTATE_SECTION_LEFT(type, portionN, 7)

#define ROTATE_PORTION_RIGHT(type, portionN) 											\
	ROTATE_SECTION_RIGHT(type, portionN, 0);										\
ROTATE_SECTION_RIGHT(type, portionN, 1);										\
ROTATE_SECTION_RIGHT(type, portionN, 2);										\
ROTATE_SECTION_RIGHT(type, portionN, 3);										\
ROTATE_SECTION_RIGHT(type, portionN, 4);										\
ROTATE_SECTION_RIGHT(type, portionN, 5);										\
ROTATE_SECTION_RIGHT(type, portionN, 6);										\
ROTATE_SECTION_RIGHT(type, portionN, 7)

#define ROTATE_PORTION_LEFT_ALL(type)												\
	ROTATE_PORTION_LEFT(type, 0);												\
ROTATE_PORTION_LEFT(type, 1);												\
ROTATE_PORTION_LEFT(type, 2);												\
ROTATE_PORTION_LEFT(type, 3);												\
ROTATE_PORTION_LEFT(type, 4);												\
ROTATE_PORTION_LEFT(type, 5);												\
ROTATE_PORTION_LEFT(type, 6);												\
ROTATE_PORTION_LEFT(type, 7);												\
ROTATE_PORTION_LEFT(type, 8);												\
ROTATE_PORTION_LEFT(type, 9);												\
ROTATE_PORTION_LEFT(type, 10);												\
ROTATE_PORTION_LEFT(type, 11);												\
ROTATE_PORTION_LEFT(type, 12);												\
ROTATE_PORTION_LEFT(type, 14);												\
ROTATE_PORTION_LEFT(type, 15);												\
ROTATE_PORTION_LEFT(type, 16);												\
ROTATE_PORTION_LEFT(type, 17);												\
ROTATE_PORTION_LEFT(type, 18);												\
ROTATE_PORTION_LEFT(type, 19);												\
ROTATE_PORTION_LEFT(type, 20);												\
ROTATE_PORTION_LEFT(type, 21);												\
ROTATE_PORTION_LEFT(type, 22);												\
ROTATE_PORTION_LEFT(type, 23);												\
ROTATE_PORTION_LEFT(type, 24);												\
ROTATE_PORTION_LEFT(type, 25);												\
ROTATE_PORTION_LEFT(type, 26);												\
ROTATE_PORTION_LEFT(type, 27);												\
ROTATE_PORTION_LEFT(type, 28);												\
ROTATE_PORTION_LEFT(type, 29);												\
ROTATE_PORTION_LEFT(type, 30);												\
ROTATE_PORTION_LEFT(type, 31)

#define ROTATE_PORTION_RIGHT_ALL(type)												\
	ROTATE_PORTION_RIGHT(type, 0);												\
ROTATE_PORTION_RIGHT(type, 1);												\
ROTATE_PORTION_RIGHT(type, 2);												\
ROTATE_PORTION_RIGHT(type, 3);												\
ROTATE_PORTION_RIGHT(type, 4);												\
ROTATE_PORTION_RIGHT(type, 5);												\
ROTATE_PORTION_RIGHT(type, 6);												\
ROTATE_PORTION_RIGHT(type, 7);												\
ROTATE_PORTION_RIGHT(type, 8);												\
ROTATE_PORTION_RIGHT(type, 9);												\
ROTATE_PORTION_RIGHT(type, 10);												\
ROTATE_PORTION_RIGHT(type, 11);												\
ROTATE_PORTION_RIGHT(type, 12);												\
ROTATE_PORTION_RIGHT(type, 14);												\
ROTATE_PORTION_RIGHT(type, 15);												\
ROTATE_PORTION_RIGHT(type, 16);												\
ROTATE_PORTION_RIGHT(type, 17);												\
ROTATE_PORTION_RIGHT(type, 18);												\
ROTATE_PORTION_RIGHT(type, 19);												\
ROTATE_PORTION_RIGHT(type, 20);												\
ROTATE_PORTION_RIGHT(type, 21);												\
ROTATE_PORTION_RIGHT(type, 22);												\
ROTATE_PORTION_RIGHT(type, 23);												\
ROTATE_PORTION_RIGHT(type, 24);												\
ROTATE_PORTION_RIGHT(type, 25);												\
ROTATE_PORTION_RIGHT(type, 26);												\
ROTATE_PORTION_RIGHT(type, 27);												\
ROTATE_PORTION_RIGHT(type, 28);												\
ROTATE_PORTION_RIGHT(type, 29);												\
ROTATE_PORTION_RIGHT(type, 30);												\
ROTATE_PORTION_RIGHT(type, 31)

#define LOAD_SECTION(type, ptr, portionN, sectionN)										\
	uint16_t V(type, portionN, sectionN, 0) = *S(ptr, portionN, sectionN, 0); 						\
uint16_t V(type, portionN, sectionN, 1) = *S(ptr, portionN, sectionN, 1); 						\
uint16_t V(type, portionN, sectionN, 2) = *S(ptr, portionN, sectionN, 2); 						\
uint16_t V(type, portionN, sectionN, 3) = *S(ptr, portionN, sectionN, 3); 						\
uint16_t V(type, portionN, sectionN, 4) = *S(ptr, portionN, sectionN, 4); 						\
uint16_t V(type, portionN, sectionN, 5) = *S(ptr, portionN, sectionN, 5); 						\
uint16_t V(type, portionN, sectionN, 6) = *S(ptr, portionN, sectionN, 6); 						\
uint16_t V(type, portionN, sectionN, 7) = *S(ptr, portionN, sectionN, 7)

#define LOAD_PORTION(type, ptr, portionN)											\
	LOAD_SECTION(type, ptr, portionN, 0);											\
LOAD_SECTION(type, ptr, portionN, 1);											\
LOAD_SECTION(type, ptr, portionN, 2);											\
LOAD_SECTION(type, ptr, portionN, 3);											\
LOAD_SECTION(type, ptr, portionN, 4);											\
LOAD_SECTION(type, ptr, portionN, 5);											\
LOAD_SECTION(type, ptr, portionN, 6);											\
LOAD_SECTION(type, ptr, portionN, 7);

#define LOAD_PORTION_ALL(type, ptr)												\
	LOAD_PORTION(type, ptr, 0);												\
LOAD_PORTION(type, ptr, 1);												\
LOAD_PORTION(type, ptr, 2);												\
LOAD_PORTION(type, ptr, 3);												\
LOAD_PORTION(type, ptr, 4);												\
LOAD_PORTION(type, ptr, 5);												\
LOAD_PORTION(type, ptr, 6);												\
LOAD_PORTION(type, ptr, 7);												\
LOAD_PORTION(type, ptr, 8);												\
LOAD_PORTION(type, ptr, 9);												\
LOAD_PORTION(type, ptr, 10);												\
LOAD_PORTION(type, ptr, 11);												\
LOAD_PORTION(type, ptr, 12);												\
LOAD_PORTION(type, ptr, 13);												\
LOAD_PORTION(type, ptr, 14);												\
LOAD_PORTION(type, ptr, 15);												\
LOAD_PORTION(type, ptr, 16);												\
LOAD_PORTION(type, ptr, 17);												\
LOAD_PORTION(type, ptr, 18);												\
LOAD_PORTION(type, ptr, 19);												\
LOAD_PORTION(type, ptr, 20);												\
LOAD_PORTION(type, ptr, 21);												\
LOAD_PORTION(type, ptr, 22);												\
LOAD_PORTION(type, ptr, 23);												\
LOAD_PORTION(type, ptr, 24);												\
LOAD_PORTION(type, ptr, 25);												\
LOAD_PORTION(type, ptr, 26);												\
LOAD_PORTION(type, ptr, 27);												\
LOAD_PORTION(type, ptr, 28);												\
LOAD_PORTION(type, ptr, 29);												\
LOAD_PORTION(type, ptr, 30);												\
LOAD_PORTION(type, ptr, 31)

#define SET_SECTION(type, ptr, portionN, sectionN)										\
	*S(ptr, portionN, sectionN, 0) = V(type, portionN, sectionN, 0);							\
*S(ptr, portionN, sectionN, 1) = V(type, portionN, sectionN, 1);							\
*S(ptr, portionN, sectionN, 2) = V(type, portionN, sectionN, 2);							\
*S(ptr, portionN, sectionN, 3) = V(type, portionN, sectionN, 3);							\
*S(ptr, portionN, sectionN, 4) = V(type, portionN, sectionN, 4);							\
*S(ptr, portionN, sectionN, 5) = V(type, portionN, sectionN, 5);							\
*S(ptr, portionN, sectionN, 6) = V(type, portionN, sectionN, 6);							\
*S(ptr, portionN, sectionN, 7) = V(type, portionN, sectionN, 7)

#define SET_PORTION(type, ptr, portionN)											\
	SET_SECTION(type, ptr, portionN, 0);											\
SET_SECTION(type, ptr, portionN, 1);											\
SET_SECTION(type, ptr, portionN, 2);											\
SET_SECTION(type, ptr, portionN, 3);											\
SET_SECTION(type, ptr, portionN, 4);											\
SET_SECTION(type, ptr, portionN, 5);											\
SET_SECTION(type, ptr, portionN, 6);											\
SET_SECTION(type, ptr, portionN, 7)

#define LOAD_PORTION_FROM_OTHER_PORTION(type, destP, sourceP)									\
	LOAD_SECTION_FROM_OTHER_SECTION(type, destP, sourceP, 0, 0);								\
LOAD_SECTION_FROM_OTHER_SECTION(type, destP, sourceP, 1, 1);								\
LOAD_SECTION_FROM_OTHER_SECTION(type, destP, sourceP, 2, 2);								\
LOAD_SECTION_FROM_OTHER_SECTION(type, destP, sourceP, 3, 3);								\
LOAD_SECTION_FROM_OTHER_SECTION(type, destP, sourceP, 4, 4);								\
LOAD_SECTION_FROM_OTHER_SECTION(type, destP, sourceP, 5, 5);								\
LOAD_SECTION_FROM_OTHER_SECTION(type, destP, sourceP, 6, 6);								\
LOAD_SECTION_FROM_OTHER_SECTION(type, destP, sourceP, 7, 7)

#define LOAD_SECTION_FROM_OTHER_SECTION(type, destP, sourceP, destS, sourceS)							\
	uint16_t V(type, destP, destS, 0) = V(type, sourceP, sourceS, 0);							\
uint16_t V(type, destP, destS, 1) = V(type, sourceP, sourceS, 1);							\
uint16_t V(type, destP, destS, 2) = V(type, sourceP, sourceS, 2);							\
uint16_t V(type, destP, destS, 3) = V(type, sourceP, sourceS, 3);							\
uint16_t V(type, destP, destS, 4) = V(type, sourceP, sourceS, 4);							\
uint16_t V(type, destP, destS, 5) = V(type, sourceP, sourceS, 5);							\
uint16_t V(type, destP, destS, 6) = V(type, sourceP, sourceS, 6);							\
uint16_t V(type, destP, destS, 7) = V(type, sourceP, sourceS, 7)

#define SET_PORTION_ALL(type, ptr)												\
	SET_PORTION(type, ptr, 0);												\
SET_PORTION(type, ptr, 1);												\
SET_PORTION(type, ptr, 2);												\
SET_PORTION(type, ptr, 3);												\
SET_PORTION(type, ptr, 4);												\
SET_PORTION(type, ptr, 5);												\
SET_PORTION(type, ptr, 6);												\
SET_PORTION(type, ptr, 7);												\
SET_PORTION(type, ptr, 8);												\
SET_PORTION(type, ptr, 9);												\
SET_PORTION(type, ptr, 10);												\
SET_PORTION(type, ptr, 11);												\
SET_PORTION(type, ptr, 12);												\
SET_PORTION(type, ptr, 13);												\
SET_PORTION(type, ptr, 14);												\
SET_PORTION(type, ptr, 15);												\
SET_PORTION(type, ptr, 16);												\
SET_PORTION(type, ptr, 17);												\
SET_PORTION(type, ptr, 18);												\
SET_PORTION(type, ptr, 19);												\
SET_PORTION(type, ptr, 20);												\
SET_PORTION(type, ptr, 21);												\
SET_PORTION(type, ptr, 22);												\
SET_PORTION(type, ptr, 23);												\
SET_PORTION(type, ptr, 24);												\
SET_PORTION(type, ptr, 25);												\
SET_PORTION(type, ptr, 26);												\
SET_PORTION(type, ptr, 27);												\
SET_PORTION(type, ptr, 28);												\
SET_PORTION(type, ptr, 29);												\
SET_PORTION(type, ptr, 30);												\
SET_PORTION(type, ptr, 31)

#define SUBSTITUTE_SECTION(type, portionN, sectionN) 										\
	V(type, portionN, sectionN, 0) = substitutionTable[V(type, portionN, sectionN, 0)];					\
V(type, portionN, sectionN, 1) = substitutionTable[V(type, portionN, sectionN, 1)];					\
V(type, portionN, sectionN, 2) = substitutionTable[V(type, portionN, sectionN, 2)];					\
V(type, portionN, sectionN, 3) = substitutionTable[V(type, portionN, sectionN, 3)];					\
V(type, portionN, sectionN, 4) = substitutionTable[V(type, portionN, sectionN, 4)];					\
V(type, portionN, sectionN, 5) = substitutionTable[V(type, portionN, sectionN, 5)];					\
V(type, portionN, sectionN, 6) = substitutionTable[V(type, portionN, sectionN, 6)];					\
V(type, portionN, sectionN, 7) = substitutionTable[V(type, portionN, sectionN, 7)]

#define SUBSTITUTE_PORTION(type, portionN)											\
	SUBSTITUTE_SECTION(type, portionN, 0);											\
SUBSTITUTE_SECTION(type, portionN, 1);											\
SUBSTITUTE_SECTION(type, portionN, 2);											\
SUBSTITUTE_SECTION(type, portionN, 3);											\
SUBSTITUTE_SECTION(type, portionN, 4);											\
SUBSTITUTE_SECTION(type, portionN, 5);											\
SUBSTITUTE_SECTION(type, portionN, 6);											\
SUBSTITUTE_SECTION(type, portionN, 7)

#define REVERSE_SUBSTITUTE_SECTION(type, portionN, sectionN) 									\
	V(type, portionN, sectionN, 0) = reverseSubstitutionTable[V(type, portionN, sectionN, 0)];				\
V(type, portionN, sectionN, 1) = reverseSubstitutionTable[V(type, portionN, sectionN, 1)];				\
V(type, portionN, sectionN, 2) = reverseSubstitutionTable[V(type, portionN, sectionN, 2)];				\
V(type, portionN, sectionN, 3) = reverseSubstitutionTable[V(type, portionN, sectionN, 3)];				\
V(type, portionN, sectionN, 4) = reverseSubstitutionTable[V(type, portionN, sectionN, 4)];				\
V(type, portionN, sectionN, 5) = reverseSubstitutionTable[V(type, portionN, sectionN, 5)];				\
V(type, portionN, sectionN, 6) = reverseSubstitutionTable[V(type, portionN, sectionN, 6)];				\
V(type, portionN, sectionN, 7) = reverseSubstitutionTable[V(type, portionN, sectionN, 7)]

#define REVERSE_SUBSTITUTE_PORTION(type, portionN)										\
	REVERSE_SUBSTITUTE_SECTION(type, portionN, 0);										\
REVERSE_SUBSTITUTE_SECTION(type, portionN, 1);										\
REVERSE_SUBSTITUTE_SECTION(type, portionN, 2);										\
REVERSE_SUBSTITUTE_SECTION(type, portionN, 3);										\
REVERSE_SUBSTITUTE_SECTION(type, portionN, 4);										\
REVERSE_SUBSTITUTE_SECTION(type, portionN, 5);										\
REVERSE_SUBSTITUTE_SECTION(type, portionN, 6);										\
REVERSE_SUBSTITUTE_SECTION(type, portionN, 7)

#define XOR_SECTION_FORWARD(type, portionN, sectionN)										\
	V(type, portionN, sectionN, 1) ^= V(type, portionN, sectionN, 0);							\
V(type, portionN, sectionN, 2) ^= V(type, portionN, sectionN, 1);							\
V(type, portionN, sectionN, 3) ^= V(type, portionN, sectionN, 2);							\
V(type, portionN, sectionN, 4) ^= V(type, portionN, sectionN, 3);							\
V(type, portionN, sectionN, 5) ^= V(type, portionN, sectionN, 4);							\
V(type, portionN, sectionN, 6) ^= V(type, portionN, sectionN, 5);							\
V(type, portionN, sectionN, 7) ^= V(type, portionN, sectionN, 6);							\
V(type, portionN, sectionN, 0) ^= V(type, portionN, sectionN, 7)

#define XOR_SECTION_REVERSE(type, portionN, sectionN)										\
	V(type, portionN, sectionN, 0) ^= V(type, portionN, sectionN, 7);							\
V(type, portionN, sectionN, 7) ^= V(type, portionN, sectionN, 6);							\
V(type, portionN, sectionN, 6) ^= V(type, portionN, sectionN, 5);							\
V(type, portionN, sectionN, 5) ^= V(type, portionN, sectionN, 4);							\
V(type, portionN, sectionN, 4) ^= V(type, portionN, sectionN, 3);							\
V(type, portionN, sectionN, 3) ^= V(type, portionN, sectionN, 2);							\
V(type, portionN, sectionN, 2) ^= V(type, portionN, sectionN, 1);							\
V(type, portionN, sectionN, 1) ^= V(type, portionN, sectionN, 0)

#define XOR_PORTION_FORWARD(type, portionN)											\
	XOR_SECTION_FORWARD(type, portionN, 0);											\
XOR_SECTION_FORWARD(type, portionN, 1);											\
XOR_SECTION_FORWARD(type, portionN, 2);											\
XOR_SECTION_FORWARD(type, portionN, 3);											\
XOR_SECTION_FORWARD(type, portionN, 4);											\
XOR_SECTION_FORWARD(type, portionN, 5);											\
XOR_SECTION_FORWARD(type, portionN, 6);											\
XOR_SECTION_FORWARD(type, portionN, 7)

#define XOR_PORTION_REVERSE(type, portionN)											\
	XOR_SECTION_REVERSE(type, portionN, 0);											\
XOR_SECTION_REVERSE(type, portionN, 1);											\
XOR_SECTION_REVERSE(type, portionN, 2);											\
XOR_SECTION_REVERSE(type, portionN, 3);											\
XOR_SECTION_REVERSE(type, portionN, 4);											\
XOR_SECTION_REVERSE(type, portionN, 5);											\
XOR_SECTION_REVERSE(type, portionN, 6);											\
XOR_SECTION_REVERSE(type, portionN, 7)

#define REPLACEMENT_PORTION_FORWARD(type, portionN)										\
	SUBSTITUTE_PORTION(type, portionN);											\
XOR_PORTION_FORWARD(type, portionN)

#define REPLACEMENT_PORTION_FORWARD_ALL(type)											\
	REPLACEMENT_PORTION_FORWARD(type, 0);											\
REPLACEMENT_PORTION_FORWARD(type, 1);											\
REPLACEMENT_PORTION_FORWARD(type, 2);											\
REPLACEMENT_PORTION_FORWARD(type, 3);											\
REPLACEMENT_PORTION_FORWARD(type, 4);											\
REPLACEMENT_PORTION_FORWARD(type, 5);											\
REPLACEMENT_PORTION_FORWARD(type, 6);											\
REPLACEMENT_PORTION_FORWARD(type, 7);											\
REPLACEMENT_PORTION_FORWARD(type, 8);											\
REPLACEMENT_PORTION_FORWARD(type, 9);											\
REPLACEMENT_PORTION_FORWARD(type, 10);											\
REPLACEMENT_PORTION_FORWARD(type, 11);											\
REPLACEMENT_PORTION_FORWARD(type, 12);											\
REPLACEMENT_PORTION_FORWARD(type, 13);											\
REPLACEMENT_PORTION_FORWARD(type, 14);											\
REPLACEMENT_PORTION_FORWARD(type, 15);											\
REPLACEMENT_PORTION_FORWARD(type, 16);											\
REPLACEMENT_PORTION_FORWARD(type, 17);											\
REPLACEMENT_PORTION_FORWARD(type, 18);											\
REPLACEMENT_PORTION_FORWARD(type, 19);											\
REPLACEMENT_PORTION_FORWARD(type, 20);											\
REPLACEMENT_PORTION_FORWARD(type, 21);											\
REPLACEMENT_PORTION_FORWARD(type, 22);											\
REPLACEMENT_PORTION_FORWARD(type, 23);											\
REPLACEMENT_PORTION_FORWARD(type, 24);											\
REPLACEMENT_PORTION_FORWARD(type, 25);											\
REPLACEMENT_PORTION_FORWARD(type, 26);											\
REPLACEMENT_PORTION_FORWARD(type, 27);											\
REPLACEMENT_PORTION_FORWARD(type, 28);											\
REPLACEMENT_PORTION_FORWARD(type, 29);											\
REPLACEMENT_PORTION_FORWARD(type, 30);											\
REPLACEMENT_PORTION_FORWARD(type, 31)

#define REPLACEMENT_PORTION_REVERSE(type, portionN)										\
	XOR_PORTION_REVERSE(type, portionN);											\
REVERSE_SUBSTITUTE_PORTION(type, portionN)

#define REPLACEMENT_PORTION_REVERSE_ALL(type)											\
	REPLACEMENT_PORTION_REVERSE(type, 0);											\
REPLACEMENT_PORTION_REVERSE(type, 1);											\
REPLACEMENT_PORTION_REVERSE(type, 2);											\
REPLACEMENT_PORTION_REVERSE(type, 3);											\
REPLACEMENT_PORTION_REVERSE(type, 4);											\
REPLACEMENT_PORTION_REVERSE(type, 5);											\
REPLACEMENT_PORTION_REVERSE(type, 6);											\
REPLACEMENT_PORTION_REVERSE(type, 7);											\
REPLACEMENT_PORTION_REVERSE(type, 8);											\
REPLACEMENT_PORTION_REVERSE(type, 9);											\
REPLACEMENT_PORTION_REVERSE(type, 10);											\
REPLACEMENT_PORTION_REVERSE(type, 11);											\
REPLACEMENT_PORTION_REVERSE(type, 12);											\
REPLACEMENT_PORTION_REVERSE(type, 13);											\
REPLACEMENT_PORTION_REVERSE(type, 14);											\
REPLACEMENT_PORTION_REVERSE(type, 15);											\
REPLACEMENT_PORTION_REVERSE(type, 16);											\
REPLACEMENT_PORTION_REVERSE(type, 17);											\
REPLACEMENT_PORTION_REVERSE(type, 18);											\
REPLACEMENT_PORTION_REVERSE(type, 19);											\
REPLACEMENT_PORTION_REVERSE(type, 20);											\
REPLACEMENT_PORTION_REVERSE(type, 21);											\
REPLACEMENT_PORTION_REVERSE(type, 22);											\
REPLACEMENT_PORTION_REVERSE(type, 23);											\
REPLACEMENT_PORTION_REVERSE(type, 24);											\
REPLACEMENT_PORTION_REVERSE(type, 25);											\
REPLACEMENT_PORTION_REVERSE(type, 26);											\
REPLACEMENT_PORTION_REVERSE(type, 27);											\
REPLACEMENT_PORTION_REVERSE(type, 28);											\
REPLACEMENT_PORTION_REVERSE(type, 29);											\
REPLACEMENT_PORTION_REVERSE(type, 30);											\
REPLACEMENT_PORTION_REVERSE(type, 31)

#define PORTION_REARRANGE(type, portionN)											\
	SWAP_SHORTS(V(type, portionN, 0, 1), V(type, portionN, 1, 1));								\
SWAP_SHORTS(V(type, portionN, 0, 2), V(type, portionN, 2, 1));								\
SWAP_SHORTS(V(type, portionN, 0, 3), V(type, portionN, 3, 1));								\
SWAP_SHORTS(V(type, portionN, 0, 4), V(type, portionN, 4, 1));								\
SWAP_SHORTS(V(type, portionN, 0, 5), V(type, portionN, 5, 1));								\
SWAP_SHORTS(V(type, portionN, 0, 6), V(type, portionN, 6, 1));								\
SWAP_SHORTS(V(type, portionN, 0, 7), V(type, portionN, 7, 1));								\
\
SWAP_SHORTS(V(type, portionN, 1, 2), V(type, portionN, 2, 2));								\
SWAP_SHORTS(V(type, portionN, 1, 3), V(type, portionN, 3, 2));								\
SWAP_SHORTS(V(type, portionN, 1, 4), V(type, portionN, 4, 2));								\
SWAP_SHORTS(V(type, portionN, 1, 5), V(type, portionN, 5, 2));								\
SWAP_SHORTS(V(type, portionN, 1, 6), V(type, portionN, 6, 2));								\
SWAP_SHORTS(V(type, portionN, 1, 7), V(type, portionN, 7, 2));								\
\
SWAP_SHORTS(V(type, portionN, 2, 3), V(type, portionN, 3, 3));								\
SWAP_SHORTS(V(type, portionN, 2, 4), V(type, portionN, 4, 3));								\
SWAP_SHORTS(V(type, portionN, 2, 5), V(type, portionN, 5, 3));								\
SWAP_SHORTS(V(type, portionN, 2, 6), V(type, portionN, 6, 3));								\
SWAP_SHORTS(V(type, portionN, 2, 7), V(type, portionN, 7, 3));								\
\
SWAP_SHORTS(V(type, portionN, 3, 4), V(type, portionN, 4, 4));								\
SWAP_SHORTS(V(type, portionN, 3, 5), V(type, portionN, 5, 4));								\
SWAP_SHORTS(V(type, portionN, 3, 6), V(type, portionN, 6, 4));								\
SWAP_SHORTS(V(type, portionN, 3, 7), V(type, portionN, 7, 4));								\
\
SWAP_SHORTS(V(type, portionN, 4, 5), V(type, portionN, 5, 5));								\
SWAP_SHORTS(V(type, portionN, 4, 6), V(type, portionN, 6, 5));								\
SWAP_SHORTS(V(type, portionN, 4, 7), V(type, portionN, 7, 5));								\
\
SWAP_SHORTS(V(type, portionN, 5, 6), V(type, portionN, 6, 6));								\
SWAP_SHORTS(V(type, portionN, 5, 7), V(type, portionN, 7, 6));								\
\
SWAP_SHORTS(V(type, portionN, 6, 7), V(type, portionN, 7, 7))

#define REARRANGE_ALL_PORTIONS(type)												\
	PORTION_REARRANGE(type, 0);												\
PORTION_REARRANGE(type, 1);												\
PORTION_REARRANGE(type, 2);												\
PORTION_REARRANGE(type, 3);												\
PORTION_REARRANGE(type, 4);												\
PORTION_REARRANGE(type, 5);												\
PORTION_REARRANGE(type, 6);												\
PORTION_REARRANGE(type, 7);												\
PORTION_REARRANGE(type, 8);												\
PORTION_REARRANGE(type, 9);												\
PORTION_REARRANGE(type, 10);												\
PORTION_REARRANGE(type, 11);												\
PORTION_REARRANGE(type, 12);												\
PORTION_REARRANGE(type, 13);												\
PORTION_REARRANGE(type, 14);												\
PORTION_REARRANGE(type, 15);												\
PORTION_REARRANGE(type, 16);												\
PORTION_REARRANGE(type, 17);												\
PORTION_REARRANGE(type, 18);												\
PORTION_REARRANGE(type, 19);												\
PORTION_REARRANGE(type, 20);												\
PORTION_REARRANGE(type, 21);												\
PORTION_REARRANGE(type, 22);												\
PORTION_REARRANGE(type, 23);												\
PORTION_REARRANGE(type, 24);												\
PORTION_REARRANGE(type, 25);												\
PORTION_REARRANGE(type, 26);												\
PORTION_REARRANGE(type, 27);												\
PORTION_REARRANGE(type, 28);												\
PORTION_REARRANGE(type, 29);												\
PORTION_REARRANGE(type, 30);												\
PORTION_REARRANGE(type, 31)

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

#define stringify1_impl1(type, portionN, sectionN)		"v_" # type "_" # portionN "_" # sectionN "_"
#define stringify1_impl(type, portionN, sectionN)		stringify1_impl1(type, portionN, sectionN)		
#define stringify1(...)						stringify1_impl1(__VA_ARGS__)

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


void initialize() {
	substitutionTable.resize(k64);
	reverseSubstitutionTable.resize(k64);
	for (uint32_t i = 0; i < k64; ++i) {
		uint16_t temp = k64-i;
		substitutionTable[i] = temp;
		reverseSubstitutionTable[temp] = i;
	}

	OTPs.resize(k64);
	for (uint32_t i = 0; i < k64; ++i) {
		OTPs[i].resize(32);
		for (uint32_t j = 0; j < 32; ++j) {
			OTPs[i][j] = rand();
		}
	}
}

void printVector(uint32_t start, uint32_t end, vector<uint16_t>& vec) {
	for (uint32_t i = start; i < end; ++i) {
		cout << hex << uint64_t(vec[i]) << dec << " " << setw(3);
	}
	cout << endl;
}

#define EXTEND_USER_KEY()									\
	LOAD_PORTION_FROM_OTHER_PORTION(k, 1, 0);						\
ROTATE_PORTION_LEFT(k, 1);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 2, 1);						\
ROTATE_PORTION_LEFT(k, 2);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 3, 2);						\
ROTATE_PORTION_LEFT(k, 3);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 4, 3);						\
ROTATE_PORTION_LEFT(k, 4);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 5, 4);						\
ROTATE_PORTION_LEFT(k, 5);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 6, 5);						\
ROTATE_PORTION_LEFT(k, 6);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 7, 6);						\
ROTATE_PORTION_LEFT(k, 7);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 8, 7);						\
ROTATE_PORTION_LEFT(k, 8);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 9, 8);						\
ROTATE_PORTION_LEFT(k, 9);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 10, 9);						\
ROTATE_PORTION_LEFT(k, 10);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 11, 10);						\
ROTATE_PORTION_LEFT(k, 11);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 12, 11);						\
ROTATE_PORTION_LEFT(k, 12);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 13, 12);						\
ROTATE_PORTION_LEFT(k, 13);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 14, 13);						\
ROTATE_PORTION_LEFT(k, 14);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 15, 14);						\
ROTATE_PORTION_LEFT(k, 15);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 16, 15);						\
ROTATE_PORTION_LEFT(k, 16);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 17, 16);						\
ROTATE_PORTION_LEFT(k, 17);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 18, 17);						\
ROTATE_PORTION_LEFT(k, 18);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 19, 18);						\
ROTATE_PORTION_LEFT(k, 19);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 20, 19);						\
ROTATE_PORTION_LEFT(k, 20);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 21, 20);						\
ROTATE_PORTION_LEFT(k, 21);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 22, 21);						\
ROTATE_PORTION_LEFT(k, 22);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 23, 22);						\
ROTATE_PORTION_LEFT(k, 23);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 24, 23);						\
ROTATE_PORTION_LEFT(k, 24);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 25, 24);						\
ROTATE_PORTION_LEFT(k, 25);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 26, 25);						\
ROTATE_PORTION_LEFT(k, 26);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 27, 26);						\
ROTATE_PORTION_LEFT(k, 27);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 28, 27);						\
ROTATE_PORTION_LEFT(k, 28);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 29, 28);						\
ROTATE_PORTION_LEFT(k, 29);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 30, 29);						\
ROTATE_PORTION_LEFT(k, 30);								\
\
LOAD_PORTION_FROM_OTHER_PORTION(k, 31, 30);						\
ROTATE_PORTION_LEFT(k, 31)

#define PROCESS_USER_KEY()									\
	REPLACEMENT_PORTION_FORWARD(k, 0);							\
REPLACEMENT_PORTION_FORWARD(k, 0);							\
PORTION_REARRANGE(k, 0);								\
REPLACEMENT_PORTION_FORWARD(k, 0);							\
REPLACEMENT_PORTION_FORWARD(k, 0);							\
EXTEND_USER_KEY()

#define FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(portionN, roundNumber, sectionEPUK, shortEPUK)				\
	REPLACEMENT_PORTION_FORWARD(d, portionN);									\
	XOR_OTP_EPUK_PORTION(portionN, roundNumber, sectionEPUK, shortEPUK);						\
															\
	REPLACEMENT_PORTION_FORWARD(d, portionN);									\
	PORTION_REARRANGE(d, portionN);											\
	XOR_OTP_EPUK_PORTION(portionN, ADD(roundNumber, 1), sectionEPUK, shortEPUK);					\
															\
	REPLACEMENT_PORTION_FORWARD(d, portionN);									\
	XOR_OTP_EPUK_PORTION(portionN, ADD(roundNumber, 2), sectionEPUK, shortEPUK);					\
															\
	REPLACEMENT_PORTION_FORWARD(d, portionN);									\
	XOR_OTP_EPUK_PORTION(portionN, ADD(roundNumber, 3), sectionEPUK, shortEPUK)


#define REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(portionN, roundNumber, sectionEPUK, shortEPUK)				\
	XOR_OTP_EPUK_PORTION(portionN, ADD(roundNumber, 3), sectionEPUK, shortEPUK);					\
	REPLACEMENT_PORTION_REVERSE(d, portionN);									\
															\
	XOR_OTP_EPUK_PORTION(portionN, ADD(roundNumber, 2), sectionEPUK, shortEPUK);					\
	REPLACEMENT_PORTION_REVERSE(d, portionN);									\
															\
	XOR_OTP_EPUK_PORTION(portionN, ADD(roundNumber, 1), sectionEPUK, shortEPUK);					\
	PORTION_REARRANGE(d, portionN);											\
	REPLACEMENT_PORTION_REVERSE(d, portionN);									\
															\
	XOR_OTP_EPUK_PORTION(portionN, roundNumber, sectionEPUK, shortEPUK);						\
	REPLACEMENT_PORTION_REVERSE(d, portionN)

void testSpeed(vector<uint16_t>&input, vector<uint16_t>&userKey) {
	{
		Timer timer("Time taken to load 1024-bit user key: ", duration);
		LOAD_PORTION(k, userKey.data(), 0);
	}
	{
		Timer timer("Time taken to process user key: ", duration);
		PROCESS_USER_KEY();
	}
	LOAD_PORTION(k, userKey.data(), 0);
	PROCESS_USER_KEY();

	{
		Timer timer("Time taken to load plus do 4 rounds of forward replacement and xor on 1024-bits: ", duration);
		LOAD_PORTION(d, input.data(), 0);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(0, 0, 0, 0);
	}	
	{
		Timer timer("Time taken to load and set plus do 4 rounds of forward replacement and xor on 1024-bits: ", duration);
		LOAD_PORTION(d, input.data(), 0);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(0, 0, 0, 0);
		SET_PORTION(d, input.data(), 0);
	}	

	{
		Timer timer("Time taken to load and do 4 rounds of forward replacement and xor on 4K. Followed by set on all portions: ", duration);
		LOAD_PORTION(d, input.data(), 0);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(0, 0, 0, 0);
		LOAD_PORTION(d, input.data(), 1);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(1, 0, 0, 2);
		LOAD_PORTION(d, input.data(), 2);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(2, 0, 0, 4);
		LOAD_PORTION(d, input.data(), 3);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(3, 0, 0, 6);
		LOAD_PORTION(d, input.data(), 4);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(4, 0, 1, 0);
		LOAD_PORTION(d, input.data(), 5);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(5, 0, 1, 2);
		LOAD_PORTION(d, input.data(), 6);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(6, 0, 1, 4);
		LOAD_PORTION(d, input.data(), 7);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(7, 0, 1, 6);
		LOAD_PORTION(d, input.data(), 8);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(8, 0, 2, 0);
		LOAD_PORTION(d, input.data(), 9);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(9, 0, 2, 2);
		LOAD_PORTION(d, input.data(), 10);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(10, 0, 2, 4);
		LOAD_PORTION(d, input.data(), 11);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(11, 0, 2, 6);
		LOAD_PORTION(d, input.data(), 12);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(12, 0, 3, 0);
		LOAD_PORTION(d, input.data(), 13);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(13, 0, 3, 2);
		LOAD_PORTION(d, input.data(), 14);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(14, 0, 3, 4);
		LOAD_PORTION(d, input.data(), 15);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(15, 0, 3, 6);
		LOAD_PORTION(d, input.data(), 16);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(16, 0, 4, 0);
		LOAD_PORTION(d, input.data(), 17);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(17, 0, 4, 2);
		LOAD_PORTION(d, input.data(), 18);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(18, 0, 4, 4);
		LOAD_PORTION(d, input.data(), 19);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(19, 0, 4, 6);
		LOAD_PORTION(d, input.data(), 20);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(20, 0, 5, 0);
		LOAD_PORTION(d, input.data(), 21);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(21, 0, 5, 2);
		LOAD_PORTION(d, input.data(), 22);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(22, 0, 5, 4);
		LOAD_PORTION(d, input.data(), 23);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(23, 0, 5, 6);
		LOAD_PORTION(d, input.data(), 24);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(24, 0, 6, 0);
		LOAD_PORTION(d, input.data(), 25);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(25, 0, 6, 2);
		LOAD_PORTION(d, input.data(), 26);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(26, 0, 6, 4);
		LOAD_PORTION(d, input.data(), 27);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(27, 0, 6, 6);
			LOAD_PORTION(d, input.data(), 28);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(28, 0, 7, 0);
		LOAD_PORTION(d, input.data(), 29);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(29, 0, 7, 2);
		LOAD_PORTION(d, input.data(), 30);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(30, 0, 7, 4);
		LOAD_PORTION(d, input.data(), 31);
		FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(31, 0, 7, 6);
		SET_PORTION_ALL(d, input.data());
	}
}

/**** Main function to trigger encryption *****/
void ENCRYPTION_II(vector<uint16_t>& input, vector<uint16_t>& userKey) {
	/* STEP 1: Load user key (1024-bits) and create EPUK */
	LOAD_PORTION(k, userKey.data(), 0);
	PROCESS_USER_KEY();

	
	LOAD_PORTION(d, input.data(), 0);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(0, 0, 0, 0);
	LOAD_PORTION(d, input.data(), 1);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(1, 0, 0, 2);
	LOAD_PORTION(d, input.data(), 2);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(2, 0, 0, 4);
	LOAD_PORTION(d, input.data(), 3);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(3, 0, 0, 6);
	LOAD_PORTION(d, input.data(), 4);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(4, 0, 1, 0);
	LOAD_PORTION(d, input.data(), 5);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(5, 0, 1, 2);
	LOAD_PORTION(d, input.data(), 6);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(6, 0, 1, 4);
	LOAD_PORTION(d, input.data(), 7);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(7, 0, 1, 6);
	LOAD_PORTION(d, input.data(), 8);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(8, 0, 2, 0);
	LOAD_PORTION(d, input.data(), 9);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(9, 0, 2, 2);
	LOAD_PORTION(d, input.data(), 10);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(10, 0, 2, 4);
	LOAD_PORTION(d, input.data(), 11);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(11, 0, 2, 6);
	LOAD_PORTION(d, input.data(), 12);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(12, 0, 3, 0);
	LOAD_PORTION(d, input.data(), 13);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(13, 0, 3, 2);
	LOAD_PORTION(d, input.data(), 14);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(14, 0, 3, 4);
	LOAD_PORTION(d, input.data(), 15);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(15, 0, 3, 6);
	LOAD_PORTION(d, input.data(), 16);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(16, 0, 4, 0);
	LOAD_PORTION(d, input.data(), 17);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(17, 0, 4, 2);
	LOAD_PORTION(d, input.data(), 18);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(18, 0, 4, 4);
	LOAD_PORTION(d, input.data(), 19);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(19, 0, 4, 6);
	LOAD_PORTION(d, input.data(), 20);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(20, 0, 5, 0);
	LOAD_PORTION(d, input.data(), 21);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(21, 0, 5, 2);
	LOAD_PORTION(d, input.data(), 22);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(22, 0, 5, 4);
	LOAD_PORTION(d, input.data(), 23);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(23, 0, 5, 6);
	LOAD_PORTION(d, input.data(), 24);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(24, 0, 6, 0);
	LOAD_PORTION(d, input.data(), 25);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(25, 0, 6, 2);
	LOAD_PORTION(d, input.data(), 26);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(26, 0, 6, 4);
	LOAD_PORTION(d, input.data(), 27);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(27, 0, 6, 6);
	LOAD_PORTION(d, input.data(), 28);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(28, 0, 7, 0);
	LOAD_PORTION(d, input.data(), 29);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(29, 0, 7, 2);
	LOAD_PORTION(d, input.data(), 30);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(30, 0, 7, 4);
	LOAD_PORTION(d, input.data(), 31);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(31, 0, 7, 6);

	SUPER_REARRANGEMENT_PORTION_ALL();

	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(0, 4, 0, 0);
	SET_PORTION(d, input.data(), 0);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(1, 4, 0, 2);
	SET_PORTION(d, input.data(), 1);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(2, 4, 0, 4);
	SET_PORTION(d, input.data(), 2);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(3, 4, 0, 6);
	SET_PORTION(d, input.data(), 3);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(4, 4, 1, 0);
	SET_PORTION(d, input.data(), 4);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(5, 4, 1, 2);
	SET_PORTION(d, input.data(), 5);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(6, 4, 1, 4);
	SET_PORTION(d, input.data(), 6);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(7, 4, 1, 6);
	SET_PORTION(d, input.data(), 7);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(8, 4, 2, 0);
	SET_PORTION(d, input.data(), 8);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(9, 4, 2, 2);
	SET_PORTION(d, input.data(), 9);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(10, 4, 2, 4);
	SET_PORTION(d, input.data(), 10);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(11, 4, 2, 6);
	SET_PORTION(d, input.data(), 11);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(12, 4, 3, 0);
	SET_PORTION(d, input.data(), 12);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(13, 4, 3, 2);
	SET_PORTION(d, input.data(), 13);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(14, 4, 3, 4);
	SET_PORTION(d, input.data(), 14);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(15, 4, 3, 6);
	SET_PORTION(d, input.data(), 15);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(16, 4, 4, 0);
	SET_PORTION(d, input.data(), 16);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(17, 4, 4, 2);
	SET_PORTION(d, input.data(), 17);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(18, 4, 4, 4);
	SET_PORTION(d, input.data(), 18);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(19, 4, 4, 6);
	SET_PORTION(d, input.data(), 19);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(20, 4, 5, 0);
	SET_PORTION(d, input.data(), 20);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(21, 4, 5, 2);
	SET_PORTION(d, input.data(), 21);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(22, 4, 5, 4);
	SET_PORTION(d, input.data(), 22);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(23, 4, 5, 6);
	SET_PORTION(d, input.data(), 23);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(24, 4, 6, 0);
	SET_PORTION(d, input.data(), 24);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(25, 4, 6, 2);
	SET_PORTION(d, input.data(), 25);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(26, 4, 6, 4);
	SET_PORTION(d, input.data(), 26);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(27, 4, 6, 6);
	SET_PORTION(d, input.data(), 27);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(28, 4, 7, 0);
	SET_PORTION(d, input.data(), 28);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(29, 4, 7, 2);
	SET_PORTION(d, input.data(), 29);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(30, 4, 7, 4);
	SET_PORTION(d, input.data(), 30);
	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(31, 4, 7, 6);
	SET_PORTION(d, input.data(), 31);
}

/**** Main function to trigger decryption ****/
void DECRYPTION_II(vector<uint16_t>& input, vector<uint16_t>& userKey) {
	/* STEP 1: Load user key (1024-bits) and create EPUK */
	LOAD_PORTION(k, userKey.data(), 0);
	PROCESS_USER_KEY();
	
	LOAD_PORTION(d, input.data(), 0);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(0, 4, 0, 0);
	LOAD_PORTION(d, input.data(), 1);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(1, 4, 0, 2);
	LOAD_PORTION(d, input.data(), 2);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(2, 4, 0, 4);
	LOAD_PORTION(d, input.data(), 3);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(3, 4, 0, 6);
	LOAD_PORTION(d, input.data(), 4);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(4, 4, 1, 0);
	LOAD_PORTION(d, input.data(), 5);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(5, 4, 1, 2);
	LOAD_PORTION(d, input.data(), 6);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(6, 4, 1, 4);
	LOAD_PORTION(d, input.data(), 7);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(7, 4, 1, 6);
	LOAD_PORTION(d, input.data(), 8);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(8, 4, 2, 0);
	LOAD_PORTION(d, input.data(), 9);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(9, 4, 2, 2);
	LOAD_PORTION(d, input.data(), 10);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(10, 4, 2, 4);
	LOAD_PORTION(d, input.data(), 11);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(11, 4, 2, 6);
	LOAD_PORTION(d, input.data(), 12);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(12, 4, 3, 0);
	LOAD_PORTION(d, input.data(), 13);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(13, 4, 3, 2);
	LOAD_PORTION(d, input.data(), 14);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(14, 4, 3, 4);
	LOAD_PORTION(d, input.data(), 15);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(15, 4, 3, 6);
	LOAD_PORTION(d, input.data(), 16);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(16, 4, 4, 0);
	LOAD_PORTION(d, input.data(), 17);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(17, 4, 4, 2);
	LOAD_PORTION(d, input.data(), 18);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(18, 4, 4, 4);
	LOAD_PORTION(d, input.data(), 19);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(19, 4, 4, 6);
	LOAD_PORTION(d, input.data(), 20);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(20, 4, 5, 0);
	LOAD_PORTION(d, input.data(), 21);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(21, 4, 5, 2);
	LOAD_PORTION(d, input.data(), 22);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(22, 4, 5, 4);
	LOAD_PORTION(d, input.data(), 23);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(23, 4, 5, 6);
	LOAD_PORTION(d, input.data(), 24);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(24, 4, 6, 0);
	LOAD_PORTION(d, input.data(), 25);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(25, 4, 6, 2);
	LOAD_PORTION(d, input.data(), 26);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(26, 4, 6, 4);
	LOAD_PORTION(d, input.data(), 27);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(27, 4, 6, 6);
	LOAD_PORTION(d, input.data(), 28);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(28, 4, 7, 0);
	LOAD_PORTION(d, input.data(), 29);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(29, 4, 7, 2);
	LOAD_PORTION(d, input.data(), 30);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(30, 4, 7, 4);
	LOAD_PORTION(d, input.data(), 31);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(31, 4, 7, 6);

	SUPER_REARRANGEMENT_PORTION_ALL();

	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(0, 0, 0, 0);
	SET_PORTION(d, input.data(), 0);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(1, 0, 0, 2);
	SET_PORTION(d, input.data(), 1);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(2, 0, 0, 4);
	SET_PORTION(d, input.data(), 2);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(3, 0, 0, 6);
	SET_PORTION(d, input.data(), 3);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(4, 0, 1, 0);
	SET_PORTION(d, input.data(), 4);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(5, 0, 1, 2);
	SET_PORTION(d, input.data(), 5);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(6, 0, 1, 4);
	SET_PORTION(d, input.data(), 6);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(7, 0, 1, 6);
	SET_PORTION(d, input.data(), 7);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(8, 0, 2, 0);
	SET_PORTION(d, input.data(), 8);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(9, 0, 2, 2);
	SET_PORTION(d, input.data(), 9);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(10, 0, 2, 4);
	SET_PORTION(d, input.data(), 10);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(11, 0, 2, 6);
	SET_PORTION(d, input.data(), 11);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(12, 0, 3, 0);
	SET_PORTION(d, input.data(), 12);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(13, 0, 3, 2);
	SET_PORTION(d, input.data(), 13);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(14, 0, 3, 4);
	SET_PORTION(d, input.data(), 14);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(15, 0, 3, 6);
	SET_PORTION(d, input.data(), 15);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(16, 0, 4, 0);
	SET_PORTION(d, input.data(), 16);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(17, 0, 4, 2);
	SET_PORTION(d, input.data(), 17);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(18, 0, 4, 4);
	SET_PORTION(d, input.data(), 18);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(19, 0, 4, 6);
	SET_PORTION(d, input.data(), 19);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(20, 0, 5, 0);
	SET_PORTION(d, input.data(), 20);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(21, 0, 5, 2);
	SET_PORTION(d, input.data(), 21);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(22, 0, 5, 4);
	SET_PORTION(d, input.data(), 22);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(23, 0, 5, 6);
	SET_PORTION(d, input.data(), 23);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(24, 0, 6, 0);
	SET_PORTION(d, input.data(), 24);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(25, 0, 6, 2);
	SET_PORTION(d, input.data(), 25);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(26, 0, 6, 4);
	SET_PORTION(d, input.data(), 26);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(27, 0, 6, 6);
	SET_PORTION(d, input.data(), 27);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(28, 0, 7, 0);
	SET_PORTION(d, input.data(), 28);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(29, 0, 7, 2);
	SET_PORTION(d, input.data(), 29);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(30, 0, 7, 4);
	SET_PORTION(d, input.data(), 30);
	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(31, 0, 7, 6);
	SET_PORTION(d, input.data(), 31);
}



int main() {
	initialize();

	uint32_t size = k4 / sizeof(uint16_t); // 4K bytes
	uint32_t sizeUK = k1 / sizeof(uint16_t); // 1K bytes
	vector<uint16_t> original(size);
	vector<uint16_t> userKey(sizeUK);
	for (uint16_t i = 0; i < userKey.size(); ++i) {
		userKey.at(i) = rand();
	}
	for (uint16_t i = 0; i < original.size(); ++i) {
		original.at(i) = rand();
	}
	vector<uint16_t> input(original);
	EPUK = original;

	#define portion 0
	LOAD_PORTION(d, input.data(), portion);
	LOAD_PORTION_ALL(k, EPUK.data());
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), k4) == 0);

	FORWARD_REPLACEMENT_AND_XOR_FOUR_ROUNDS(portion, 0, 0, 0);
	SET_PORTION(d, input.data(), portion);

	uint16_t skipElements = (portion + 1) * 128 / sizeof(char);
	uint16_t compareElements = (31 - portion) * k1 / 8;
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), k1*portion) == 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data())+skipElements, reinterpret_cast<const char*>(original.data())+skipElements, compareElements) == 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), k4) != 0);

	REVERSE_REPLACEMENT_AND_XOR_FOUR_ROUNDS(portion, 0, 0, 0);
	SET_PORTION(d, input.data(), portion);

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), k1*portion) == 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data())+skipElements, reinterpret_cast<const char*>(original.data())+skipElements, compareElements) == 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), k4) == 0);

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), k4) == 0);
	chrono::duration<double, nano> duration;
	uint32_t count = 1;
	{
		Timer timer1("Time taken for " + to_string(count) + " rounds encryption:", duration);
		for (int i = 0; i < count; ++i) {
			//ENCRYPTION(input, userKey);
			ENCRYPTION_II(input, userKey);
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), k4) != 0);
	{
		Timer timer1("Time taken for " + to_string(count) + " rounds decryption:", duration);
		for (int i = 0; i < count; ++i) {
			//DECRYPTION(input, userKey);
			DECRYPTION_II(input, userKey);
		}
	}
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), k4) == 0);
	cout << "Encryption and decryption of entire user data page" << endl;

	return 0;
}
