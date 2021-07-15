#include <assert.h>
#include <cstdint>
#include <iostream>
#include <iomanip>
#include <string.h>
#include <vector>

using namespace std;

constexpr uint16_t k1 = 1024;
constexpr uint16_t k4 = 4*k1;
constexpr uint32_t k64 = 64*k1;

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

#define SWAP_SHORTS(a, b)				\
	do {						\
		uint16_t temp = a;			\
		a = b;					\
		b = temp;				\
	} while(0)


#define S(ptr, portionN, sectionN, shortN)	(reinterpret_cast<uint16_t *>(ptr)+(portionN*(PORTION_SIZE/(sizeof(uint16_t)*8)))+(sectionN*(SECTION_SIZE/(sizeof(uint16_t)*8)))+shortN)

#define V_impl(type, portionN, sectionN, shortN)		v ## _ ## type ## _ ## portionN ## _ ## sectionN ## _ ## shortN 
#define V_impl_wrapper(type, portionN, sectionN, shortN) 	V_impl(type, portionN, sectionN, shortN)
#define V_wrapper(type, portionN, sectionN, shortN)		V_impl_wrapper(type, portionN, sectionN, shortN)
#define V(type, portionN, sectionN, shortN)			V_wrapper(type, portionN, sectionN, shortN)	

// TODO: Decrement OTP section count after each BACKWARD_XOR_OTP_EPUK_PORTION()
#define BACKWARD_XOR_OTP_EPUK_PORTION_ALL(OTPSectionCount, roundNumber)								\
	BACKWARD_XOR_OTP_EPUK_PORTION(31, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(30, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(29, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(28, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(27, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(26, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(25, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(24, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(23, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(22, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(21, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(20, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(19, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(18, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(17, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(16, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(15, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(14, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(13, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(12, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(11, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(10, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(9, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(8, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(7, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(6, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(5, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(4, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(3, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(2, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(1, OTPSectionCount, roundNumber);								\
	BACKWARD_XOR_OTP_EPUK_PORTION(0, OTPSectionCount, roundNumber)
	

// TODO: Increment OTP section count after each FORWARD_XOR_OTP_EPUK_PORTION()
#define FORWARD_XOR_OTP_EPUK_PORTION_ALL(OTPSectionCount, roundNumber)								\
	FORWARD_XOR_OTP_EPUK_PORTION(0, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(1, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(2, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(3, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(4, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(5, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(6, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(7, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(8, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(9, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(10, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(11, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(12, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(13, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(15, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(16, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(17, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(18, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(19, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(20, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(21, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(22, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(23, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(25, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(26, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(27, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(28, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(29, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(30, OTPSectionCount, roundNumber);								\
	FORWARD_XOR_OTP_EPUK_PORTION(31, OTPSectionCount, roundNumber)
	
#define FORWARD_XOR_OTP_EPUK_PORTION(portionN, OTPSectionCount, roundNumber)							\
	{															\
	uint16_t OTPSectionC = (portionN*2)+(roundNumber*2*32);									\
	assert(OTPSectionCount == OTPSectionC);											\
	XOR_OTP_EPUK_FOUR_SECTIONS(portionN, 0, OTPSectionC, roundNumber);							\
																\
	XOR_OTP_EPUK_FOUR_SECTIONS(portionN, 4, OTPSectionC+1, roundNumber);							\
	}

#define BACKWARD_XOR_OTP_EPUK_PORTION(portionN, OTPSectionCount, roundNumber)							\
	{															\
	uint16_t OTPSectionC = (portionN*2)+(roundNumber*2*32);									\
	assert(OTPSectionCount == OTPSectionC);											\
	XOR_OTP_EPUK_FOUR_SECTIONS(portionN, 4, OTPSectionC, roundNumber);							\
																\
	XOR_OTP_EPUK_FOUR_SECTIONS(portionN, 0, OTPSectionC-1, roundNumber);							\
	}

#define XOR_OTP_EPUK_FOUR_SECTIONS(portionN, sectionN, OTPSectionCount, roundNumber) 						\
	uint8_t EPUK_portion = roundNumber;											\
	uint8_t EPUK_section = (OTPSectionCount / 8) - (roundNumber * 8); 							\
	uint8_t EPUK_short = (OTPSectionCount / 2) - (roundNumber * 32) - (EPUK_section * 4); 					\
	uint32_t index = V(k, roundNumber, (OTPSectionCount / 8) - (roundNumber * 8), (OTPSectionCount / 2) - (roundNumber * 32) - (((OTPSectionCount / 8) - (roundNumber * 8))*4));												\
																\
	V(d, portionN, sectionN, 0) ^= V(k, portionN, sectionN, 0) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][0];	\
	V(d, portionN, sectionN, 1) ^= V(k, portionN, sectionN, 1) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][1];	\
	V(d, portionN, sectionN, 2) ^= V(k, portionN, sectionN, 2) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][2];	\
	V(d, portionN, sectionN, 3) ^= V(k, portionN, sectionN, 3) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][3];	\
	V(d, portionN, sectionN, 4) ^= V(k, portionN, sectionN, 4) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][4];	\
	V(d, portionN, sectionN, 5) ^= V(k, portionN, sectionN, 5) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][5];	\
	V(d, portionN, sectionN, 6) ^= V(k, portionN, sectionN, 6) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][6];	\
	V(d, portionN, sectionN, 7) ^= V(k, portionN, sectionN, 7) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][7];	\

	/***********************************************************************************************************************
																\
	V(d, portionN, sectionN+1, 0) ^= V(k, portionN, sectionN, 0) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][8];	\
	V(d, portionN, sectionN+1, 1) ^= V(k, portionN, sectionN, 1) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][9];	\
	V(d, portionN, sectionN+1, 2) ^= V(k, portionN, sectionN, 2) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][10];	\
	V(d, portionN, sectionN+1, 3) ^= V(k, portionN, sectionN, 3) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][11];	\
	V(d, portionN, sectionN+1, 4) ^= V(k, portionN, sectionN, 4) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][12];	\
	V(d, portionN, sectionN+1, 5) ^= V(k, portionN, sectionN, 5) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][13];	\
	V(d, portionN, sectionN+1, 6) ^= V(k, portionN, sectionN, 6) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][14];	\
	V(d, portionN, sectionN+1, 7) ^= V(k, portionN, sectionN, 7) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][15];	\
																\
	V(d, portionN, sectionN+2, 0) ^= V(k, portionN, sectionN, 0) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][16];	\
	V(d, portionN, sectionN+2, 1) ^= V(k, portionN, sectionN, 1) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][17];	\
	V(d, portionN, sectionN+2, 2) ^= V(k, portionN, sectionN, 2) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][18];	\
	V(d, portionN, sectionN+2, 3) ^= V(k, portionN, sectionN, 3) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][19];	\
	V(d, portionN, sectionN+2, 4) ^= V(k, portionN, sectionN, 4) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][20];	\
	V(d, portionN, sectionN+2, 5) ^= V(k, portionN, sectionN, 5) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][21];	\
	V(d, portionN, sectionN+2, 6) ^= V(k, portionN, sectionN, 6) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][22];	\
	V(d, portionN, sectionN+2, 7) ^= V(k, portionN, sectionN, 7) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][23];	\
																\
	V(d, portionN, sectionN+3, 0) ^= V(k, portionN, sectionN, 0) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][24];	\
	V(d, portionN, sectionN+3, 1) ^= V(k, portionN, sectionN, 1) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][25];	\
	V(d, portionN, sectionN+3, 2) ^= V(k, portionN, sectionN, 2) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][26];	\
	V(d, portionN, sectionN+3, 3) ^= V(k, portionN, sectionN, 3) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][27];	\
	V(d, portionN, sectionN+3, 4) ^= V(k, portionN, sectionN, 4) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][28];	\
	V(d, portionN, sectionN+3, 5) ^= V(k, portionN, sectionN, 5) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][29];	\
	V(d, portionN, sectionN+3, 6) ^= V(k, portionN, sectionN, 6) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][30];	\
	V(d, portionN, sectionN+3, 7) ^= V(k, portionN, sectionN, 7) ^ OTPs[V(k, EPUK_portion, EPUK_section, EPUK_short)][31];	\
	***********************************************************************************************************************/


#define ROTATE_SECTION_LEFT(type, portionN, sectionN)					\
/* S0 - S1 - S2 - S3 - S4 - S5 - S6 - S7 */						\
do {											\
	uint16_t prev, next;								\
	prev = (V(type, portionN, sectionN, 0) & 0xFF00) >> 8;				\
	next = (V(type, portionN, sectionN, 7) & 0xFF00) >> 8;				\
											\
	V(type, portionN, sectionN, 7) = (V(type, portionN, sectionN, 7) << 8) | prev;	\
	prev = next;									\
	next = (V(type, portionN, sectionN, 6) & 0xFF00) >> 8;				\
											\
	V(type, portionN, sectionN, 6) = (V(type, portionN, sectionN, 6) << 8) | prev;	\
	prev = next;									\
	next = (V(type, portionN, sectionN, 5) & 0xFF00) >> 8;				\
											\
	V(type, portionN, sectionN, 5) = (V(type, portionN, sectionN, 5) << 8) | prev;	\
	prev = next;									\
	next = (V(type, portionN, sectionN, 4) & 0xFF00) >> 8;				\
											\
	V(type, portionN, sectionN, 4) = (V(type, portionN, sectionN, 4) << 8) | prev;	\
	prev = next;									\
	next = (V(type, portionN, sectionN, 3) & 0xFF00) >> 8;				\
											\
	V(type, portionN, sectionN, 3) = (V(type, portionN, sectionN, 3) << 8) | prev;	\
	prev = next;									\
	next = (V(type, portionN, sectionN, 2) & 0xFF00) >> 8;				\
											\
	V(type, portionN, sectionN, 2) = (V(type, portionN, sectionN, 2) << 8) | prev;	\
	prev = next;									\
	next = (V(type, portionN, sectionN, 1) & 0xFF00) >> 8;				\
											\
	V(type, portionN, sectionN, 1) = (V(type, portionN, sectionN, 1) << 8) | prev;	\
	prev = next;									\
											\
	V(type, portionN, sectionN, 0) = (V(type, portionN, sectionN, 0) << 8) | prev;	\
} while(0)

#define ROTATE_SECTION_RIGHT(type, portionN, sectionN)					\
/* S0 - S1 - S2 - S3 - S4 - S5 - S6 - S7 */						\
do {											\
	uint16_t prev, next;								\
	prev = (V(type, portionN, sectionN, 7) & 0xFF) << 8;				\
	next = (V(type, portionN, sectionN, 0) & 0xFF) << 8;				\
											\
	V(type, portionN, sectionN, 0) = (V(type, portionN, sectionN, 0) >> 8) | prev;	\
	prev = next;									\
	next = (V(type, portionN, sectionN, 1) & 0xFF) << 8;				\
											\
	V(type, portionN, sectionN, 1) = (V(type, portionN, sectionN, 1) >> 8) | prev;	\
	prev = next;									\
	next = (V(type, portionN, sectionN, 2) & 0xFF) << 8;				\
											\
	V(type, portionN, sectionN, 2) = (V(type, portionN, sectionN, 2) >> 8) | prev;	\
	prev = next;									\
	next = (V(type, portionN, sectionN, 3) & 0xFF) << 8;				\
											\
	V(type, portionN, sectionN, 3) = (V(type, portionN, sectionN, 3) >> 8) | prev;	\
	prev = next;									\
	next = (V(type, portionN, sectionN, 4) & 0xFF) << 8;				\
											\
	V(type, portionN, sectionN, 4) = (V(type, portionN, sectionN, 4) >> 8) | prev;	\
	prev = next;									\
	next = (V(type, portionN, sectionN, 5) & 0xFF) << 8;				\
											\
	V(type, portionN, sectionN, 5) = (V(type, portionN, sectionN, 5) >> 8) | prev;	\
	prev = next;									\
	next = (V(type, portionN, sectionN, 6) & 0xFF) << 8;				\
											\
	V(type, portionN, sectionN, 6) = (V(type, portionN, sectionN, 6) >> 8) | prev;	\
	prev = next;									\
											\
	V(type, portionN, sectionN, 7) = (V(type, portionN, sectionN, 7) >> 8) | prev;	\
} while (0)

#define ROTATE_PORTION_LEFT(type, portionN) 						\
	ROTATE_SECTION_LEFT(type, portionN, 0);						\
	ROTATE_SECTION_LEFT(type, portionN, 1);						\
	ROTATE_SECTION_LEFT(type, portionN, 2);						\
	ROTATE_SECTION_LEFT(type, portionN, 3);						\
	ROTATE_SECTION_LEFT(type, portionN, 4);						\
	ROTATE_SECTION_LEFT(type, portionN, 5);						\
	ROTATE_SECTION_LEFT(type, portionN, 6);						\
	ROTATE_SECTION_LEFT(type, portionN, 7)

#define ROTATE_PORTION_RIGHT(type, portionN) 							\
	ROTATE_SECTION_RIGHT(type, portionN, 0);						\
	ROTATE_SECTION_RIGHT(type, portionN, 1);						\
	ROTATE_SECTION_RIGHT(type, portionN, 2);						\
	ROTATE_SECTION_RIGHT(type, portionN, 3);						\
	ROTATE_SECTION_RIGHT(type, portionN, 4);						\
	ROTATE_SECTION_RIGHT(type, portionN, 5);						\
	ROTATE_SECTION_RIGHT(type, portionN, 6);						\
	ROTATE_SECTION_RIGHT(type, portionN, 7)

#define ROTATE_PORTION_LEFT_ALL(type)							\
	ROTATE_PORTION_LEFT(type, 0);							\
	ROTATE_PORTION_LEFT(type, 1);							\
	ROTATE_PORTION_LEFT(type, 2);							\
	ROTATE_PORTION_LEFT(type, 3);							\
	ROTATE_PORTION_LEFT(type, 4);							\
	ROTATE_PORTION_LEFT(type, 5);							\
	ROTATE_PORTION_LEFT(type, 6);							\
	ROTATE_PORTION_LEFT(type, 7);							\
	ROTATE_PORTION_LEFT(type, 8);							\
	ROTATE_PORTION_LEFT(type, 9);							\
	ROTATE_PORTION_LEFT(type, 10);							\
	ROTATE_PORTION_LEFT(type, 11);							\
	ROTATE_PORTION_LEFT(type, 12);							\
	ROTATE_PORTION_LEFT(type, 14);							\
	ROTATE_PORTION_LEFT(type, 15);							\
	ROTATE_PORTION_LEFT(type, 16);							\
	ROTATE_PORTION_LEFT(type, 17);							\
	ROTATE_PORTION_LEFT(type, 18);							\
	ROTATE_PORTION_LEFT(type, 19);							\
	ROTATE_PORTION_LEFT(type, 20);							\
	ROTATE_PORTION_LEFT(type, 21);							\
	ROTATE_PORTION_LEFT(type, 22);							\
	ROTATE_PORTION_LEFT(type, 23);							\
	ROTATE_PORTION_LEFT(type, 24);							\
	ROTATE_PORTION_LEFT(type, 25);							\
	ROTATE_PORTION_LEFT(type, 26);							\
	ROTATE_PORTION_LEFT(type, 27);							\
	ROTATE_PORTION_LEFT(type, 28);							\
	ROTATE_PORTION_LEFT(type, 29);							\
	ROTATE_PORTION_LEFT(type, 30);							\
	ROTATE_PORTION_LEFT(type, 31)

#define ROTATE_PORTION_RIGHT_ALL(type)							\
	ROTATE_PORTION_RIGHT(type, 0);							\
	ROTATE_PORTION_RIGHT(type, 1);							\
	ROTATE_PORTION_RIGHT(type, 2);							\
	ROTATE_PORTION_RIGHT(type, 3);							\
	ROTATE_PORTION_RIGHT(type, 4);							\
	ROTATE_PORTION_RIGHT(type, 5);							\
	ROTATE_PORTION_RIGHT(type, 6);							\
	ROTATE_PORTION_RIGHT(type, 7);							\
	ROTATE_PORTION_RIGHT(type, 8);							\
	ROTATE_PORTION_RIGHT(type, 9);							\
	ROTATE_PORTION_RIGHT(type, 10);							\
	ROTATE_PORTION_RIGHT(type, 11);							\
	ROTATE_PORTION_RIGHT(type, 12);							\
	ROTATE_PORTION_RIGHT(type, 14);							\
	ROTATE_PORTION_RIGHT(type, 15);							\
	ROTATE_PORTION_RIGHT(type, 16);							\
	ROTATE_PORTION_RIGHT(type, 17);							\
	ROTATE_PORTION_RIGHT(type, 18);							\
	ROTATE_PORTION_RIGHT(type, 19);							\
	ROTATE_PORTION_RIGHT(type, 20);							\
	ROTATE_PORTION_RIGHT(type, 21);							\
	ROTATE_PORTION_RIGHT(type, 22);							\
	ROTATE_PORTION_RIGHT(type, 23);							\
	ROTATE_PORTION_RIGHT(type, 24);							\
	ROTATE_PORTION_RIGHT(type, 25);							\
	ROTATE_PORTION_RIGHT(type, 26);							\
	ROTATE_PORTION_RIGHT(type, 27);							\
	ROTATE_PORTION_RIGHT(type, 28);							\
	ROTATE_PORTION_RIGHT(type, 29);							\
	ROTATE_PORTION_RIGHT(type, 30);							\
	ROTATE_PORTION_RIGHT(type, 31)

#define LOAD_SECTION(type, ptr, portionN, sectionN)						\
	uint16_t V(type, portionN, sectionN, 0) = *S(ptr, portionN, sectionN, 0); 		\
	uint16_t V(type, portionN, sectionN, 1) = *S(ptr, portionN, sectionN, 1); 		\
	uint16_t V(type, portionN, sectionN, 2) = *S(ptr, portionN, sectionN, 2); 		\
	uint16_t V(type, portionN, sectionN, 3) = *S(ptr, portionN, sectionN, 3); 		\
	uint16_t V(type, portionN, sectionN, 4) = *S(ptr, portionN, sectionN, 4); 		\
	uint16_t V(type, portionN, sectionN, 5) = *S(ptr, portionN, sectionN, 5); 		\
	uint16_t V(type, portionN, sectionN, 6) = *S(ptr, portionN, sectionN, 6); 		\
	uint16_t V(type, portionN, sectionN, 7) = *S(ptr, portionN, sectionN, 7)

#define LOAD_PORTION(type, ptr, portionN)							\
	LOAD_SECTION(type, ptr, portionN, 0);							\
	LOAD_SECTION(type, ptr, portionN, 1);							\
	LOAD_SECTION(type, ptr, portionN, 2);							\
	LOAD_SECTION(type, ptr, portionN, 3);							\
	LOAD_SECTION(type, ptr, portionN, 4);							\
	LOAD_SECTION(type, ptr, portionN, 5);							\
	LOAD_SECTION(type, ptr, portionN, 6);							\
	LOAD_SECTION(type, ptr, portionN, 7);

#define LOAD_PORTION_ALL(ptr)									\
	LOAD_PORTION(d, ptr, 0);								\
	LOAD_PORTION(d, ptr, 1);								\
	LOAD_PORTION(d, ptr, 2);								\
	LOAD_PORTION(d, ptr, 3);								\
	LOAD_PORTION(d, ptr, 4);								\
	LOAD_PORTION(d, ptr, 5);								\
	LOAD_PORTION(d, ptr, 6);								\
	LOAD_PORTION(d, ptr, 7);								\
	LOAD_PORTION(d, ptr, 8);								\
	LOAD_PORTION(d, ptr, 9);								\
	LOAD_PORTION(d, ptr, 10);								\
	LOAD_PORTION(d, ptr, 11);								\
	LOAD_PORTION(d, ptr, 12);								\
	LOAD_PORTION(d, ptr, 13);								\
	LOAD_PORTION(d, ptr, 14);								\
	LOAD_PORTION(d, ptr, 15);								\
	LOAD_PORTION(d, ptr, 16);								\
	LOAD_PORTION(d, ptr, 17);								\
	LOAD_PORTION(d, ptr, 18);								\
	LOAD_PORTION(d, ptr, 19);								\
	LOAD_PORTION(d, ptr, 20);								\
	LOAD_PORTION(d, ptr, 21);								\
	LOAD_PORTION(d, ptr, 22);								\
	LOAD_PORTION(d, ptr, 23);								\
	LOAD_PORTION(d, ptr, 24);								\
	LOAD_PORTION(d, ptr, 25);								\
	LOAD_PORTION(d, ptr, 26);								\
	LOAD_PORTION(d, ptr, 27);								\
	LOAD_PORTION(d, ptr, 28);								\
	LOAD_PORTION(d, ptr, 29);								\
	LOAD_PORTION(d, ptr, 30);								\
	LOAD_PORTION(d, ptr, 31)

#define SET_SECTION(type, ptr, portionN, sectionN)						\
	*S(ptr, portionN, sectionN, 0) = V(type, portionN, sectionN, 0);			\
	*S(ptr, portionN, sectionN, 1) = V(type, portionN, sectionN, 1);			\
	*S(ptr, portionN, sectionN, 2) = V(type, portionN, sectionN, 2);			\
	*S(ptr, portionN, sectionN, 3) = V(type, portionN, sectionN, 3);			\
	*S(ptr, portionN, sectionN, 4) = V(type, portionN, sectionN, 4);			\
	*S(ptr, portionN, sectionN, 5) = V(type, portionN, sectionN, 5);			\
	*S(ptr, portionN, sectionN, 6) = V(type, portionN, sectionN, 6);			\
	*S(ptr, portionN, sectionN, 7) = V(type, portionN, sectionN, 7)

#define SET_PORTION(type, ptr, portionN)							\
	SET_SECTION(type, ptr, portionN, 0);							\
	SET_SECTION(type, ptr, portionN, 1);							\
	SET_SECTION(type, ptr, portionN, 2);							\
	SET_SECTION(type, ptr, portionN, 3);							\
	SET_SECTION(type, ptr, portionN, 4);							\
	SET_SECTION(type, ptr, portionN, 5);							\
	SET_SECTION(type, ptr, portionN, 6);							\
	SET_SECTION(type, ptr, portionN, 7)

#define SET_PORTION_ALL(ptr)									\
	SET_PORTION(d, ptr, 0);								\
	SET_PORTION(d, ptr, 1);								\
	SET_PORTION(d, ptr, 2);								\
	SET_PORTION(d, ptr, 3);								\
	SET_PORTION(d, ptr, 4);								\
	SET_PORTION(d, ptr, 5);								\
	SET_PORTION(d, ptr, 6);								\
	SET_PORTION(d, ptr, 7);								\
	SET_PORTION(d, ptr, 8);								\
	SET_PORTION(d, ptr, 9);								\
	SET_PORTION(d, ptr, 10);								\
	SET_PORTION(d, ptr, 11);								\
	SET_PORTION(d, ptr, 12);								\
	SET_PORTION(d, ptr, 13);								\
	SET_PORTION(d, ptr, 14);								\
	SET_PORTION(d, ptr, 15);								\
	SET_PORTION(d, ptr, 16);								\
	SET_PORTION(d, ptr, 17);								\
	SET_PORTION(d, ptr, 18);								\
	SET_PORTION(d, ptr, 19);								\
	SET_PORTION(d, ptr, 20);								\
	SET_PORTION(d, ptr, 21);								\
	SET_PORTION(d, ptr, 22);								\
	SET_PORTION(d, ptr, 23);								\
	SET_PORTION(d, ptr, 24);								\
	SET_PORTION(d, ptr, 25);								\
	SET_PORTION(d, ptr, 26);								\
	SET_PORTION(d, ptr, 27);								\
	SET_PORTION(d, ptr, 28);								\
	SET_PORTION(d, ptr, 29);								\
	SET_PORTION(d, ptr, 30);								\

#define SUBSTITUTE_SECTION(type, portionN, sectionN) 						\
	V(type, portionN, sectionN, 0) = substitutionTable[V(type, portionN, sectionN, 0)];		\
	V(type, portionN, sectionN, 1) = substitutionTable[V(type, portionN, sectionN, 1)];		\
	V(type, portionN, sectionN, 2) = substitutionTable[V(type, portionN, sectionN, 2)];		\
	V(type, portionN, sectionN, 3) = substitutionTable[V(type, portionN, sectionN, 3)];		\
	V(type, portionN, sectionN, 4) = substitutionTable[V(type, portionN, sectionN, 4)];		\
	V(type, portionN, sectionN, 5) = substitutionTable[V(type, portionN, sectionN, 5)];		\
	V(type, portionN, sectionN, 6) = substitutionTable[V(type, portionN, sectionN, 6)];		\
	V(type, portionN, sectionN, 7) = substitutionTable[V(type, portionN, sectionN, 7)]

#define SUBSTITUTE_PORTION(type, portionN)							\
	SUBSTITUTE_SECTION(type, portionN, 0);						\
	SUBSTITUTE_SECTION(type, portionN, 1);						\
	SUBSTITUTE_SECTION(type, portionN, 2);						\
	SUBSTITUTE_SECTION(type, portionN, 3);						\
	SUBSTITUTE_SECTION(type, portionN, 4);						\
	SUBSTITUTE_SECTION(type, portionN, 5);						\
	SUBSTITUTE_SECTION(type, portionN, 6);						\
	SUBSTITUTE_SECTION(type, portionN, 7)

#define REVERSE_SUBSTITUTE_SECTION(type, portionN, sectionN) 						\
	V(type, portionN, sectionN, 0) = reverseSubstitutionTable[V(type, portionN, sectionN, 0)];		\
	V(type, portionN, sectionN, 1) = reverseSubstitutionTable[V(type, portionN, sectionN, 1)];		\
	V(type, portionN, sectionN, 2) = reverseSubstitutionTable[V(type, portionN, sectionN, 2)];		\
	V(type, portionN, sectionN, 3) = reverseSubstitutionTable[V(type, portionN, sectionN, 3)];		\
	V(type, portionN, sectionN, 4) = reverseSubstitutionTable[V(type, portionN, sectionN, 4)];		\
	V(type, portionN, sectionN, 5) = reverseSubstitutionTable[V(type, portionN, sectionN, 5)];		\
	V(type, portionN, sectionN, 6) = reverseSubstitutionTable[V(type, portionN, sectionN, 6)];		\
	V(type, portionN, sectionN, 7) = reverseSubstitutionTable[V(type, portionN, sectionN, 7)]

#define REVERSE_SUBSTITUTE_PORTION(type, portionN)							\
	REVERSE_SUBSTITUTE_SECTION(type, portionN, 0);						\
	REVERSE_SUBSTITUTE_SECTION(type, portionN, 1);						\
	REVERSE_SUBSTITUTE_SECTION(type, portionN, 2);						\
	REVERSE_SUBSTITUTE_SECTION(type, portionN, 3);						\
	REVERSE_SUBSTITUTE_SECTION(type, portionN, 4);						\
	REVERSE_SUBSTITUTE_SECTION(type, portionN, 5);						\
	REVERSE_SUBSTITUTE_SECTION(type, portionN, 6);						\
	REVERSE_SUBSTITUTE_SECTION(type, portionN, 7)

#define XOR_SECTION_FORWARD(type, portionN, sectionN)						\
	V(type, portionN, sectionN, 1) ^= V(type, portionN, sectionN, 0);				\
	V(type, portionN, sectionN, 2) ^= V(type, portionN, sectionN, 1);				\
	V(type, portionN, sectionN, 3) ^= V(type, portionN, sectionN, 2);				\
	V(type, portionN, sectionN, 4) ^= V(type, portionN, sectionN, 3);				\
	V(type, portionN, sectionN, 5) ^= V(type, portionN, sectionN, 4);				\
	V(type, portionN, sectionN, 6) ^= V(type, portionN, sectionN, 5);				\
	V(type, portionN, sectionN, 7) ^= V(type, portionN, sectionN, 6);				\
	V(type, portionN, sectionN, 0) ^= V(type, portionN, sectionN, 7)

#define XOR_SECTION_REVERSE(type, portionN, sectionN)							\
	V(type, portionN, sectionN, 0) ^= V(type, portionN, sectionN, 7);				\
	V(type, portionN, sectionN, 7) ^= V(type, portionN, sectionN, 6);				\
	V(type, portionN, sectionN, 6) ^= V(type, portionN, sectionN, 5);				\
	V(type, portionN, sectionN, 5) ^= V(type, portionN, sectionN, 4);				\
	V(type, portionN, sectionN, 4) ^= V(type, portionN, sectionN, 3);				\
	V(type, portionN, sectionN, 3) ^= V(type, portionN, sectionN, 2);				\
	V(type, portionN, sectionN, 2) ^= V(type, portionN, sectionN, 1);				\
	V(type, portionN, sectionN, 1) ^= V(type, portionN, sectionN, 0)

#define XOR_PORTION_FORWARD(type, portionN)							\
	XOR_SECTION_FORWARD(type, portionN, 0);						\
	XOR_SECTION_FORWARD(type, portionN, 1);						\
	XOR_SECTION_FORWARD(type, portionN, 2);						\
	XOR_SECTION_FORWARD(type, portionN, 3);						\
	XOR_SECTION_FORWARD(type, portionN, 4);						\
	XOR_SECTION_FORWARD(type, portionN, 5);						\
	XOR_SECTION_FORWARD(type, portionN, 6);						\
	XOR_SECTION_FORWARD(type, portionN, 7)

#define XOR_PORTION_REVERSE(type, portionN)							\
	XOR_SECTION_REVERSE(type, portionN, 0);						\
	XOR_SECTION_REVERSE(type, portionN, 1);						\
	XOR_SECTION_REVERSE(type, portionN, 2);						\
	XOR_SECTION_REVERSE(type, portionN, 3);						\
	XOR_SECTION_REVERSE(type, portionN, 4);						\
	XOR_SECTION_REVERSE(type, portionN, 5);						\
	XOR_SECTION_REVERSE(type, portionN, 6);						\
	XOR_SECTION_REVERSE(type, portionN, 7)

#define OLD_ROTATE_SECTION_RIGHT(type, portionN, sectionN)					\
	do {										\
		uint16_t addLeft, carry;						\
		addLeft = (V(type, portionN, sectionN, 7) & 0xFF) << 8;			\
		carry = (V(type, portionN, sectionN, 0) & 0xFF) << 8;				\
		(V(type, portionN, sectionN, 0) & 0xFF) << 8;				\
	while (0)

#define REPLACEMENT_PORTION_FORWARD(type, portionN)						\
	SUBSTITUTE_PORTION(type, portionN);							\
	XOR_PORTION_FORWARD(type, portionN)

#define REPLACEMENT_PORTION_FORWARD_ALL(type)						\
	REPLACEMENT_PORTION_FORWARD(type, 0);							\
	REPLACEMENT_PORTION_FORWARD(type, 1);							\
	REPLACEMENT_PORTION_FORWARD(type, 2);							\
	REPLACEMENT_PORTION_FORWARD(type, 3);							\
	REPLACEMENT_PORTION_FORWARD(type, 4);							\
	REPLACEMENT_PORTION_FORWARD(type, 5);							\
	REPLACEMENT_PORTION_FORWARD(type, 6);							\
	REPLACEMENT_PORTION_FORWARD(type, 7);							\
	REPLACEMENT_PORTION_FORWARD(type, 8);							\
	REPLACEMENT_PORTION_FORWARD(type, 9);							\
	REPLACEMENT_PORTION_FORWARD(type, 10);						\
	REPLACEMENT_PORTION_FORWARD(type, 11);						\
	REPLACEMENT_PORTION_FORWARD(type, 12);						\
	REPLACEMENT_PORTION_FORWARD(type, 13);						\
	REPLACEMENT_PORTION_FORWARD(type, 14);						\
	REPLACEMENT_PORTION_FORWARD(type, 15);						\
	REPLACEMENT_PORTION_FORWARD(type, 16);						\
	REPLACEMENT_PORTION_FORWARD(type, 17);						\
	REPLACEMENT_PORTION_FORWARD(type, 18);						\
	REPLACEMENT_PORTION_FORWARD(type, 19);						\
	REPLACEMENT_PORTION_FORWARD(type, 20);						\
	REPLACEMENT_PORTION_FORWARD(type, 21);						\
	REPLACEMENT_PORTION_FORWARD(type, 22);						\
	REPLACEMENT_PORTION_FORWARD(type, 23);						\
	REPLACEMENT_PORTION_FORWARD(type, 24);						\
	REPLACEMENT_PORTION_FORWARD(type, 25);						\
	REPLACEMENT_PORTION_FORWARD(type, 26);						\
	REPLACEMENT_PORTION_FORWARD(type, 27);						\
	REPLACEMENT_PORTION_FORWARD(type, 28);						\
	REPLACEMENT_PORTION_FORWARD(type, 29);						\
	REPLACEMENT_PORTION_FORWARD(type, 30);						\
	REPLACEMENT_PORTION_FORWARD(type, 31)

#define REPLACEMENT_PORTION_REVERSE(type, portionN)						\
	XOR_PORTION_REVERSE(type, portionN);							\
	REVERSE_SUBSTITUTE_PORTION(type, portionN)

#define REPLACEMENT_PORTION_REVERSE_ALL(type)						\
	REPLACEMENT_PORTION_REVERSE(type, 0);							\
	REPLACEMENT_PORTION_REVERSE(type, 1);							\
	REPLACEMENT_PORTION_REVERSE(type, 2);							\
	REPLACEMENT_PORTION_REVERSE(type, 3);							\
	REPLACEMENT_PORTION_REVERSE(type, 4);							\
	REPLACEMENT_PORTION_REVERSE(type, 5);							\
	REPLACEMENT_PORTION_REVERSE(type, 6);							\
	REPLACEMENT_PORTION_REVERSE(type, 7);							\
	REPLACEMENT_PORTION_REVERSE(type, 8);							\
	REPLACEMENT_PORTION_REVERSE(type, 9);							\
	REPLACEMENT_PORTION_REVERSE(type, 10);						\
	REPLACEMENT_PORTION_REVERSE(type, 11);						\
	REPLACEMENT_PORTION_REVERSE(type, 12);						\
	REPLACEMENT_PORTION_REVERSE(type, 13);						\
	REPLACEMENT_PORTION_REVERSE(type, 14);						\
	REPLACEMENT_PORTION_REVERSE(type, 15);						\
	REPLACEMENT_PORTION_REVERSE(type, 16);						\
	REPLACEMENT_PORTION_REVERSE(type, 17);						\
	REPLACEMENT_PORTION_REVERSE(type, 18);						\
	REPLACEMENT_PORTION_REVERSE(type, 19);						\
	REPLACEMENT_PORTION_REVERSE(type, 20);						\
	REPLACEMENT_PORTION_REVERSE(type, 21);						\
	REPLACEMENT_PORTION_REVERSE(type, 22);						\
	REPLACEMENT_PORTION_REVERSE(type, 23);						\
	REPLACEMENT_PORTION_REVERSE(type, 24);						\
	REPLACEMENT_PORTION_REVERSE(type, 25);						\
	REPLACEMENT_PORTION_REVERSE(type, 26);						\
	REPLACEMENT_PORTION_REVERSE(type, 27);						\
	REPLACEMENT_PORTION_REVERSE(type, 28);						\
	REPLACEMENT_PORTION_REVERSE(type, 29);						\
	REPLACEMENT_PORTION_REVERSE(type, 30);						\
	REPLACEMENT_PORTION_REVERSE(type, 31)

#define PORTION_REARRANGE(type, portionN)							\
	SWAP_SHORTS(V(type, portionN, 0, 1), V(type, portionN, 1, 1));				\
	SWAP_SHORTS(V(type, portionN, 0, 2), V(type, portionN, 2, 1));				\
	SWAP_SHORTS(V(type, portionN, 0, 3), V(type, portionN, 3, 1));				\
	SWAP_SHORTS(V(type, portionN, 0, 4), V(type, portionN, 4, 1));				\
	SWAP_SHORTS(V(type, portionN, 0, 5), V(type, portionN, 5, 1));				\
	SWAP_SHORTS(V(type, portionN, 0, 6), V(type, portionN, 6, 1));				\
	SWAP_SHORTS(V(type, portionN, 0, 7), V(type, portionN, 7, 1));				\
											\
	SWAP_SHORTS(V(type, portionN, 1, 2), V(type, portionN, 2, 2));				\
	SWAP_SHORTS(V(type, portionN, 1, 3), V(type, portionN, 3, 2));				\
	SWAP_SHORTS(V(type, portionN, 1, 4), V(type, portionN, 4, 2));				\
	SWAP_SHORTS(V(type, portionN, 1, 5), V(type, portionN, 5, 2));				\
	SWAP_SHORTS(V(type, portionN, 1, 6), V(type, portionN, 6, 2));				\
	SWAP_SHORTS(V(type, portionN, 1, 7), V(type, portionN, 7, 2));				\
											\
	SWAP_SHORTS(V(type, portionN, 2, 3), V(type, portionN, 3, 3));				\
	SWAP_SHORTS(V(type, portionN, 2, 4), V(type, portionN, 4, 3));				\
	SWAP_SHORTS(V(type, portionN, 2, 5), V(type, portionN, 5, 3));				\
	SWAP_SHORTS(V(type, portionN, 2, 6), V(type, portionN, 6, 3));				\
	SWAP_SHORTS(V(type, portionN, 2, 7), V(type, portionN, 7, 3));				\
											\
	SWAP_SHORTS(V(type, portionN, 3, 4), V(type, portionN, 4, 4));				\
	SWAP_SHORTS(V(type, portionN, 3, 5), V(type, portionN, 5, 4));				\
	SWAP_SHORTS(V(type, portionN, 3, 6), V(type, portionN, 6, 4));				\
	SWAP_SHORTS(V(type, portionN, 3, 7), V(type, portionN, 7, 4));				\
											\
	SWAP_SHORTS(V(type, portionN, 4, 5), V(type, portionN, 5, 5));				\
	SWAP_SHORTS(V(type, portionN, 4, 6), V(type, portionN, 6, 5));				\
	SWAP_SHORTS(V(type, portionN, 4, 7), V(type, portionN, 7, 5));				\
											\
	SWAP_SHORTS(V(type, portionN, 5, 6), V(type, portionN, 6, 6));				\
	SWAP_SHORTS(V(type, portionN, 5, 7), V(type, portionN, 7, 6));				\
											\
	SWAP_SHORTS(V(type, portionN, 6, 7), V(type, portionN, 7, 7));

#define REARRANGE_ALL_PORTIONS()							\
	PORTION_REARRANGE(type, 0);								\
	PORTION_REARRANGE(type, 1);								\
	PORTION_REARRANGE(type, 2);								\
	PORTION_REARRANGE(type, 3);								\
	PORTION_REARRANGE(type, 4);								\
	PORTION_REARRANGE(type, 5);								\
	PORTION_REARRANGE(type, 6);								\
	PORTION_REARRANGE(type, 7);								\
	PORTION_REARRANGE(type, 8);								\
	PORTION_REARRANGE(type, 9);								\
	PORTION_REARRANGE(type, 10);								\
	PORTION_REARRANGE(type, 11);								\
	PORTION_REARRANGE(type, 12);								\
	PORTION_REARRANGE(type, 13);								\
	PORTION_REARRANGE(type, 14);								\
	PORTION_REARRANGE(type, 15);								\
	PORTION_REARRANGE(type, 16);								\
	PORTION_REARRANGE(type, 17);								\
	PORTION_REARRANGE(type, 18);								\
	PORTION_REARRANGE(type, 19);								\
	PORTION_REARRANGE(type, 20);								\
	PORTION_REARRANGE(type, 21);								\
	PORTION_REARRANGE(type, 22);								\
	PORTION_REARRANGE(type, 23);								\
	PORTION_REARRANGE(type, 24);								\
	PORTION_REARRANGE(type, 25);								\
	PORTION_REARRANGE(type, 26);								\
	PORTION_REARRANGE(type, 27);								\
	PORTION_REARRANGE(type, 28);								\
	PORTION_REARRANGE(type, 29);								\
	PORTION_REARRANGE(type, 30);								\
	PORTION_REARRANGE(type, 31)

#define SUPER_REARRANGEMENT_PORTION_ALL()							\
	SUPER_REARRANGEMENT_PORTION0(d);							\
	SUPER_REARRANGEMENT_PORTION1(d);							\
	SUPER_REARRANGEMENT_PORTION2(d);							\
	SUPER_REARRANGEMENT_PORTION3(d);							\
	SUPER_REARRANGEMENT_PORTION4(d);							\
	SUPER_REARRANGEMENT_PORTION5(d);							\
	SUPER_REARRANGEMENT_PORTION6(d);							\
	SUPER_REARRANGEMENT_PORTION7(d);							\
	SUPER_REARRANGEMENT_PORTION8(d);							\
	SUPER_REARRANGEMENT_PORTION9(d);							\
	SUPER_REARRANGEMENT_PORTION10(d);							\
	SUPER_REARRANGEMENT_PORTION11(d);							\
	SUPER_REARRANGEMENT_PORTION12(d);							\
	SUPER_REARRANGEMENT_PORTION13(d);							\
	SUPER_REARRANGEMENT_PORTION14(d);							\
	SUPER_REARRANGEMENT_PORTION15(d);							\
	SUPER_REARRANGEMENT_PORTION16(d);							\
	SUPER_REARRANGEMENT_PORTION17(d);							\
	SUPER_REARRANGEMENT_PORTION18(d);							\
	SUPER_REARRANGEMENT_PORTION19(d);							\
	SUPER_REARRANGEMENT_PORTION20(d);							\
	SUPER_REARRANGEMENT_PORTION21(d);							\
	SUPER_REARRANGEMENT_PORTION22(d);							\
	SUPER_REARRANGEMENT_PORTION23(d);							\
	SUPER_REARRANGEMENT_PORTION24(d);							\
	SUPER_REARRANGEMENT_PORTION25(d);							\
	SUPER_REARRANGEMENT_PORTION26(d);							\
	SUPER_REARRANGEMENT_PORTION27(d);							\
	SUPER_REARRANGEMENT_PORTION28(d);							\
	SUPER_REARRANGEMENT_PORTION29(d);							\
	SUPER_REARRANGEMENT_PORTION30(d);							\
	SUPER_REARRANGEMENT_PORTION31(d)

#define SUPER_REARRANGEMENT_PORTION0_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 0, sectionN, 0), V(type, (sectionN*4), 0, 0));			\
	SWAP_SHORTS(V(type, 0, sectionN, 1), V(type, (sectionN*4), 0, 1));			\
	SWAP_SHORTS(V(type, 0, sectionN, 2), V(type, (sectionN*4+1), 0, 0));			\
	SWAP_SHORTS(V(type, 0, sectionN, 3), V(type, (sectionN*4+1), 0, 1));			\
	SWAP_SHORTS(V(type, 0, sectionN, 4), V(type, (sectionN*4+2), 0, 0));			\
	SWAP_SHORTS(V(type, 0, sectionN, 5), V(type, (sectionN*4+2), 0, 1));			\
	SWAP_SHORTS(V(type, 0, sectionN, 6), V(type, (sectionN*4+3), 0, 0));			\
	SWAP_SHORTS(V(type, 0, sectionN, 7), V(type, (sectionN*4+3), 0, 1))

#define SUPER_REARRANGEMENT_PORTION0(type) 							\
	SWAP_SHORTS(V(type, 0, 0, 2), V(type, 1, 0, 0));					\
	SWAP_SHORTS(V(type, 0, 0, 3), V(type, 1, 0, 1));					\
	SWAP_SHORTS(V(type, 0, 0, 4), V(type, 2, 0, 0));					\
	SWAP_SHORTS(V(type, 0, 0, 5), V(type, 2, 0, 1));					\
	SWAP_SHORTS(V(type, 0, 0, 6), V(type, 3, 0, 0));					\
	SWAP_SHORTS(V(type, 0, 0, 7), V(type, 3, 0, 1));					\
												\
	SUPER_REARRANGEMENT_PORTION0_HELPER(type, 1);						\
	SUPER_REARRANGEMENT_PORTION0_HELPER(type, 2);						\
	SUPER_REARRANGEMENT_PORTION0_HELPER(type, 3);						\
	SUPER_REARRANGEMENT_PORTION0_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION0_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION0_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION0_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION1_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 1, sectionN, 0), V(type, (sectionN*4), 0, 2));			\
	SWAP_SHORTS(V(type, 1, sectionN, 1), V(type, (sectionN*4), 0, 3));			\
	SWAP_SHORTS(V(type, 1, sectionN, 2), V(type, (sectionN*4+1), 0, 2));			\
	SWAP_SHORTS(V(type, 1, sectionN, 3), V(type, (sectionN*4+1), 0, 3));			\
	SWAP_SHORTS(V(type, 1, sectionN, 4), V(type, (sectionN*4+2), 0, 2));			\
	SWAP_SHORTS(V(type, 1, sectionN, 5), V(type, (sectionN*4+2), 0, 3));			\
	SWAP_SHORTS(V(type, 1, sectionN, 6), V(type, (sectionN*4+3), 0, 2));			\
	SWAP_SHORTS(V(type, 1, sectionN, 7), V(type, (sectionN*4+3), 0, 3))

#define SUPER_REARRANGEMENT_PORTION1(type) 							\
	SWAP_SHORTS(V(type, 1, 0, 4), V(type, 2, 0, 2));					\
	SWAP_SHORTS(V(type, 1, 0, 5), V(type, 2, 0, 3));					\
	SWAP_SHORTS(V(type, 1, 0, 6), V(type, 3, 0, 2));					\
	SWAP_SHORTS(V(type, 1, 0, 7), V(type, 3, 0, 3));					\
												\
	SUPER_REARRANGEMENT_PORTION1_HELPER(type, 1);						\
	SUPER_REARRANGEMENT_PORTION1_HELPER(type, 2);						\
	SUPER_REARRANGEMENT_PORTION1_HELPER(type, 3);						\
	SUPER_REARRANGEMENT_PORTION1_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION1_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION1_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION1_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION2_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 2, sectionN, 0), V(type, (sectionN*4), 0, 4));			\
	SWAP_SHORTS(V(type, 2, sectionN, 1), V(type, (sectionN*4), 0, 5));			\
	SWAP_SHORTS(V(type, 2, sectionN, 2), V(type, (sectionN*4+1), 0, 4));			\
	SWAP_SHORTS(V(type, 2, sectionN, 3), V(type, (sectionN*4+1), 0, 5));			\
	SWAP_SHORTS(V(type, 2, sectionN, 4), V(type, (sectionN*4+2), 0, 4));			\
	SWAP_SHORTS(V(type, 2, sectionN, 5), V(type, (sectionN*4+2), 0, 5));			\
	SWAP_SHORTS(V(type, 2, sectionN, 6), V(type, (sectionN*4+3), 0, 4));			\
	SWAP_SHORTS(V(type, 2, sectionN, 7), V(type, (sectionN*4+3), 0, 5))

#define SUPER_REARRANGEMENT_PORTION2(type) 							\
	SWAP_SHORTS(V(type, 2, 0, 6), V(type, 3, 0, 4));					\
	SWAP_SHORTS(V(type, 2, 0, 7), V(type, 3, 0, 5));					\
												\
	SUPER_REARRANGEMENT_PORTION2_HELPER(type, 1);						\
	SUPER_REARRANGEMENT_PORTION2_HELPER(type, 2);						\
	SUPER_REARRANGEMENT_PORTION2_HELPER(type, 3);						\
	SUPER_REARRANGEMENT_PORTION2_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION2_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION2_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION2_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION3_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 3, sectionN, 0), V(type, (sectionN*4), 0, 6));			\
	SWAP_SHORTS(V(type, 3, sectionN, 1), V(type, (sectionN*4), 0, 7));			\
	SWAP_SHORTS(V(type, 3, sectionN, 2), V(type, (sectionN*4+1), 0, 6));			\
	SWAP_SHORTS(V(type, 3, sectionN, 3), V(type, (sectionN*4+1), 0, 7));			\
	SWAP_SHORTS(V(type, 3, sectionN, 4), V(type, (sectionN*4+2), 0, 6));			\
	SWAP_SHORTS(V(type, 3, sectionN, 5), V(type, (sectionN*4+2), 0, 7));			\
	SWAP_SHORTS(V(type, 3, sectionN, 6), V(type, (sectionN*4+3), 0, 6));			\
	SWAP_SHORTS(V(type, 3, sectionN, 7), V(type, (sectionN*4+3), 0, 7))

#define SUPER_REARRANGEMENT_PORTION3(type) 							\
	SUPER_REARRANGEMENT_PORTION3_HELPER(type, 1);						\
	SUPER_REARRANGEMENT_PORTION3_HELPER(type, 2);						\
	SUPER_REARRANGEMENT_PORTION3_HELPER(type, 3);						\
	SUPER_REARRANGEMENT_PORTION3_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION3_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION3_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION3_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION4_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 4, sectionN, 0), V(type, (sectionN*4), 1, 0));			\
	SWAP_SHORTS(V(type, 4, sectionN, 1), V(type, (sectionN*4), 1, 1));			\
	SWAP_SHORTS(V(type, 4, sectionN, 2), V(type, (sectionN*4+1), 1, 0));			\
	SWAP_SHORTS(V(type, 4, sectionN, 3), V(type, (sectionN*4+1), 1, 1));			\
	SWAP_SHORTS(V(type, 4, sectionN, 4), V(type, (sectionN*4+2), 1, 0));			\
	SWAP_SHORTS(V(type, 4, sectionN, 5), V(type, (sectionN*4+2), 1, 1));			\
	SWAP_SHORTS(V(type, 4, sectionN, 6), V(type, (sectionN*4+3), 1, 0));			\
	SWAP_SHORTS(V(type, 4, sectionN, 7), V(type, (sectionN*4+3), 1, 1))

#define SUPER_REARRANGEMENT_PORTION4(type) 							\
	SWAP_SHORTS(V(type, 4, 1, 2)) = SWAP_SHORT(V(type, 5, 1, 0)); 				\
	SWAP_SHORTS(V(type, 4, 1, 3)) = SWAP_SHORT(V(type, 5, 1, 1)); 				\
	SWAP_SHORTS(V(type, 4, 1, 4)) = SWAP_SHORT(V(type, 6, 1, 0)); 				\
	SWAP_SHORTS(V(type, 4, 1, 5)) = SWAP_SHORT(V(type, 6, 1, 1)); 				\
	SWAP_SHORTS(V(type, 4, 1, 6)) = SWAP_SHORT(V(type, 7, 1, 0)); 				\
	SWAP_SHORTS(V(type, 4, 1, 7)) = SWAP_SHORT(V(type, 7, 1, 1)); 				\
												\
	SUPER_REARRANGEMENT_PORTION4_HELPER(type, 2);						\
	SUPER_REARRANGEMENT_PORTION4_HELPER(type, 3);						\
	SUPER_REARRANGEMENT_PORTION4_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION4_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION4_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION4_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION5_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 5, sectionN, 0), V(type, (sectionN*4), 1, 2));			\
	SWAP_SHORTS(V(type, 5, sectionN, 1), V(type, (sectionN*4), 1, 3));			\
	SWAP_SHORTS(V(type, 5, sectionN, 2), V(type, (sectionN*4+1), 1, 2));			\
	SWAP_SHORTS(V(type, 5, sectionN, 3), V(type, (sectionN*4+1), 1, 3));			\
	SWAP_SHORTS(V(type, 5, sectionN, 4), V(type, (sectionN*4+2), 1, 2));			\
	SWAP_SHORTS(V(type, 5, sectionN, 5), V(type, (sectionN*4+2), 1, 3));			\
	SWAP_SHORTS(V(type, 5, sectionN, 6), V(type, (sectionN*4+3), 1, 2));			\
	SWAP_SHORTS(V(type, 5, sectionN, 7), V(type, (sectionN*4+3), 1, 3))

#define SUPER_REARRANGEMENT_PORTION5(type) 							\
	SWAP_SHORTS(V(type, 5, 1, 4)) = SWAP_SHORT(V(type, 6, 1, 2)); 				\
	SWAP_SHORTS(V(type, 5, 1, 5)) = SWAP_SHORT(V(type, 6, 1, 3)); 				\
	SWAP_SHORTS(V(type, 5, 1, 6)) = SWAP_SHORT(V(type, 7, 1, 2)); 				\
	SWAP_SHORTS(V(type, 5, 1, 7)) = SWAP_SHORT(V(type, 7, 1, 3)); 				\
												\
	SUPER_REARRANGEMENT_PORTION5_HELPER(type, 2);						\
	SUPER_REARRANGEMENT_PORTION5_HELPER(type, 3);						\
	SUPER_REARRANGEMENT_PORTION5_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION5_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION5_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION5_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION6_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 6, sectionN, 0), V(type, (sectionN*4), 1, 4));			\
	SWAP_SHORTS(V(type, 6, sectionN, 1), V(type, (sectionN*4), 1, 5));			\
	SWAP_SHORTS(V(type, 6, sectionN, 2), V(type, (sectionN*4+1), 1, 4));			\
	SWAP_SHORTS(V(type, 6, sectionN, 3), V(type, (sectionN*4+1), 1, 5));			\
	SWAP_SHORTS(V(type, 6, sectionN, 4), V(type, (sectionN*4+2), 1, 4));			\
	SWAP_SHORTS(V(type, 6, sectionN, 5), V(type, (sectionN*4+2), 1, 5));			\
	SWAP_SHORTS(V(type, 6, sectionN, 6), V(type, (sectionN*4+3), 1, 4));			\
	SWAP_SHORTS(V(type, 6, sectionN, 7), V(type, (sectionN*4+3), 1, 5))

#define SUPER_REARRANGEMENT_PORTION6(type) 							\
	SWAP_SHORTS(V(type, 6, 1, 6)) = SWAP_SHORT(V(type, 7, 1, 4)); 				\
	SWAP_SHORTS(V(type, 6, 1, 7)) = SWAP_SHORT(V(type, 7, 1, 5)); 				\
												\
	SUPER_REARRANGEMENT_PORTION6_HELPER(type, 2);						\
	SUPER_REARRANGEMENT_PORTION6_HELPER(type, 3);						\
	SUPER_REARRANGEMENT_PORTION6_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION6_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION6_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION6_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION7_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 7, sectionN, 0), V(type, (sectionN*4), 1, 6));			\
	SWAP_SHORTS(V(type, 7, sectionN, 1), V(type, (sectionN*4), 1, 7));			\
	SWAP_SHORTS(V(type, 7, sectionN, 2), V(type, (sectionN*4+1), 1, 6));			\
	SWAP_SHORTS(V(type, 7, sectionN, 3), V(type, (sectionN*4+1), 1, 7));			\
	SWAP_SHORTS(V(type, 7, sectionN, 4), V(type, (sectionN*4+2), 1, 6));			\
	SWAP_SHORTS(V(type, 7, sectionN, 5), V(type, (sectionN*4+2), 1, 7));			\
	SWAP_SHORTS(V(type, 7, sectionN, 6), V(type, (sectionN*4+3), 1, 6));			\
	SWAP_SHORTS(V(type, 7, sectionN, 7), V(type, (sectionN*4+3), 1, 7))

#define SUPER_REARRANGEMENT_PORTION7(type) 							\
	SUPER_REARRANGEMENT_PORTION7_HELPER(type, 2);						\
	SUPER_REARRANGEMENT_PORTION7_HELPER(type, 3);						\
	SUPER_REARRANGEMENT_PORTION7_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION7_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION7_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION7_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION8_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 8, sectionN, 0), V(type, (sectionN*4), 2, 0));			\
	SWAP_SHORTS(V(type, 8, sectionN, 1), V(type, (sectionN*4), 2, 1));			\
	SWAP_SHORTS(V(type, 8, sectionN, 2), V(type, (sectionN*4+1), 2, 0));			\
	SWAP_SHORTS(V(type, 8, sectionN, 3), V(type, (sectionN*4+1), 2, 1));			\
	SWAP_SHORTS(V(type, 8, sectionN, 4), V(type, (sectionN*4+2), 2, 0));			\
	SWAP_SHORTS(V(type, 8, sectionN, 5), V(type, (sectionN*4+2), 2, 1));			\
	SWAP_SHORTS(V(type, 8, sectionN, 6), V(type, (sectionN*4+3), 2, 0));			\
	SWAP_SHORTS(V(type, 8, sectionN, 7), V(type, (sectionN*4+3), 2, 1))

#define SUPER_REARRANGEMENT_PORTION8(type) 							\
	SWAP_SHORTS(V(type, 8, 2, 2)) = SWAP_SHORT(V(type, 9, 2, 0)); 				\
	SWAP_SHORTS(V(type, 8, 2, 3)) = SWAP_SHORT(V(type, 9, 2, 1)); 				\
	SWAP_SHORTS(V(type, 8, 2, 4)) = SWAP_SHORT(V(type, 10, 2, 0)); 				\
	SWAP_SHORTS(V(type, 8, 2, 5)) = SWAP_SHORT(V(type, 10, 2, 1)); 				\
	SWAP_SHORTS(V(type, 8, 2, 6)) = SWAP_SHORT(V(type, 11, 2, 0)); 				\
	SWAP_SHORTS(V(type, 8, 2, 7)) = SWAP_SHORT(V(type, 11, 2, 1)); 				\
												\
	SUPER_REARRANGEMENT_PORTION8_HELPER(type, 3);						\
	SUPER_REARRANGEMENT_PORTION8_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION8_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION8_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION8_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION9_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 9, sectionN, 0), V(type, (sectionN*4), 2, 2));			\
	SWAP_SHORTS(V(type, 9, sectionN, 1), V(type, (sectionN*4), 2, 3));			\
	SWAP_SHORTS(V(type, 9, sectionN, 2), V(type, (sectionN*4+1), 2, 2));			\
	SWAP_SHORTS(V(type, 9, sectionN, 3), V(type, (sectionN*4+1), 2, 3));			\
	SWAP_SHORTS(V(type, 9, sectionN, 4), V(type, (sectionN*4+2), 2, 2));			\
	SWAP_SHORTS(V(type, 9, sectionN, 5), V(type, (sectionN*4+2), 2, 3));			\
	SWAP_SHORTS(V(type, 9, sectionN, 6), V(type, (sectionN*4+3), 2, 2));			\
	SWAP_SHORTS(V(type, 9, sectionN, 7), V(type, (sectionN*4+3), 2, 3))

#define SUPER_REARRANGEMENT_PORTION9(type) 							\
	SWAP_SHORTS(V(type, 9, 2, 4)) = SWAP_SHORT(V(type, 10, 2, 2)); 				\
	SWAP_SHORTS(V(type, 9, 2, 5)) = SWAP_SHORT(V(type, 10, 2, 3)); 				\
	SWAP_SHORTS(V(type, 9, 2, 6)) = SWAP_SHORT(V(type, 11, 2, 2)); 				\
	SWAP_SHORTS(V(type, 9, 2, 7)) = SWAP_SHORT(V(type, 11, 2, 3)); 				\
												\
	SUPER_REARRANGEMENT_PORTION9_HELPER(type, 3);						\
	SUPER_REARRANGEMENT_PORTION9_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION9_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION9_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION9_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION10_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 10, sectionN, 0), V(type, (sectionN*4), 2, 4));			\
	SWAP_SHORTS(V(type, 10, sectionN, 1), V(type, (sectionN*4), 2, 5));			\
	SWAP_SHORTS(V(type, 10, sectionN, 2), V(type, (sectionN*4+1), 2, 4));			\
	SWAP_SHORTS(V(type, 10, sectionN, 3), V(type, (sectionN*4+1), 2, 5));			\
	SWAP_SHORTS(V(type, 10, sectionN, 4), V(type, (sectionN*4+2), 2, 4));			\
	SWAP_SHORTS(V(type, 10, sectionN, 5), V(type, (sectionN*4+2), 2, 5));			\
	SWAP_SHORTS(V(type, 10, sectionN, 6), V(type, (sectionN*4+3), 2, 4));			\
	SWAP_SHORTS(V(type, 10, sectionN, 7), V(type, (sectionN*4+3), 2, 5))

#define SUPER_REARRANGEMENT_PORTION10(type) 							\
	SWAP_SHORTS(V(type, 10, 2, 6)) = SWAP_SHORT(V(type, 11, 2, 4)); 			\
	SWAP_SHORTS(V(type, 10, 2, 7)) = SWAP_SHORT(V(type, 11, 2, 5)); 			\
												\
	SUPER_REARRANGEMENT_PORTION10_HELPER(type, 3);						\
	SUPER_REARRANGEMENT_PORTION10_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION10_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION10_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION10_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION11_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 11, sectionN, 0), V(type, (sectionN*4), 2, 6));			\
	SWAP_SHORTS(V(type, 11, sectionN, 1), V(type, (sectionN*4), 2, 7));			\
	SWAP_SHORTS(V(type, 11, sectionN, 2), V(type, (sectionN*4+1), 2, 6));			\
	SWAP_SHORTS(V(type, 11, sectionN, 3), V(type, (sectionN*4+1), 2, 7));			\
	SWAP_SHORTS(V(type, 11, sectionN, 4), V(type, (sectionN*4+2), 2, 6));			\
	SWAP_SHORTS(V(type, 11, sectionN, 5), V(type, (sectionN*4+2), 2, 7));			\
	SWAP_SHORTS(V(type, 11, sectionN, 6), V(type, (sectionN*4+3), 2, 6));			\
	SWAP_SHORTS(V(type, 11, sectionN, 7), V(type, (sectionN*4+3), 2, 7))

#define SUPER_REARRANGEMENT_PORTION11(type) 							\
	SUPER_REARRANGEMENT_PORTION11_HELPER(type, 3);						\
	SUPER_REARRANGEMENT_PORTION11_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION11_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION11_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION11_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION12_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 12, sectionN, 0), V(type, (sectionN*4), 3, 0));			\
	SWAP_SHORTS(V(type, 12, sectionN, 1), V(type, (sectionN*4), 3, 1));			\
	SWAP_SHORTS(V(type, 12, sectionN, 2), V(type, (sectionN*4+1), 3, 0));			\
	SWAP_SHORTS(V(type, 12, sectionN, 3), V(type, (sectionN*4+1), 3, 1));			\
	SWAP_SHORTS(V(type, 12, sectionN, 4), V(type, (sectionN*4+2), 3, 0));			\
	SWAP_SHORTS(V(type, 12, sectionN, 5), V(type, (sectionN*4+2), 3, 1));			\
	SWAP_SHORTS(V(type, 12, sectionN, 6), V(type, (sectionN*4+3), 3, 0));			\
	SWAP_SHORTS(V(type, 12, sectionN, 7), V(type, (sectionN*4+3), 3, 1))

#define SUPER_REARRANGEMENT_PORTION12(type) 							\
	SWAP_SHORTS(V(type, 12, 3, 2)) = SWAP_SHORT(V(type, 13, 3, 0)); 			\
	SWAP_SHORTS(V(type, 12, 3, 3)) = SWAP_SHORT(V(type, 13, 3, 1)); 			\
	SWAP_SHORTS(V(type, 12, 3, 4)) = SWAP_SHORT(V(type, 14, 3, 0)); 			\
	SWAP_SHORTS(V(type, 12, 3, 5)) = SWAP_SHORT(V(type, 14, 3, 1)); 			\
	SWAP_SHORTS(V(type, 12, 3, 6)) = SWAP_SHORT(V(type, 15, 3, 0)); 			\
	SWAP_SHORTS(V(type, 12, 3, 7)) = SWAP_SHORT(V(type, 15, 3, 1)); 			\
												\
	SUPER_REARRANGEMENT_PORTION12_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION12_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION12_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION12_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION13_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 13, sectionN, 0), V(type, (sectionN*4), 3, 2));			\
	SWAP_SHORTS(V(type, 13, sectionN, 1), V(type, (sectionN*4), 3, 3));			\
	SWAP_SHORTS(V(type, 13, sectionN, 2), V(type, (sectionN*4+1), 3, 2));			\
	SWAP_SHORTS(V(type, 13, sectionN, 3), V(type, (sectionN*4+1), 3, 3));			\
	SWAP_SHORTS(V(type, 13, sectionN, 4), V(type, (sectionN*4+2), 3, 2));			\
	SWAP_SHORTS(V(type, 13, sectionN, 5), V(type, (sectionN*4+2), 3, 3));			\
	SWAP_SHORTS(V(type, 13, sectionN, 6), V(type, (sectionN*4+3), 3, 2));			\
	SWAP_SHORTS(V(type, 13, sectionN, 7), V(type, (sectionN*4+3), 3, 3))

#define SUPER_REARRANGEMENT_PORTION13(type) 							\
	SWAP_SHORTS(V(type, 13, 3, 4)) = SWAP_SHORT(V(type, 14, 3, 2)); 			\
	SWAP_SHORTS(V(type, 13, 3, 5)) = SWAP_SHORT(V(type, 14, 3, 3)); 			\
	SWAP_SHORTS(V(type, 13, 3, 6)) = SWAP_SHORT(V(type, 15, 3, 2)); 			\
	SWAP_SHORTS(V(type, 13, 3, 7)) = SWAP_SHORT(V(type, 15, 3, 3)); 			\
												\
	SUPER_REARRANGEMENT_PORTION13_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION13_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION13_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION13_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION14_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 14, sectionN, 0), V(type, (sectionN*4), 3, 4));			\
	SWAP_SHORTS(V(type, 14, sectionN, 1), V(type, (sectionN*4), 3, 5));			\
	SWAP_SHORTS(V(type, 14, sectionN, 2), V(type, (sectionN*4+1), 3, 4));			\
	SWAP_SHORTS(V(type, 14, sectionN, 3), V(type, (sectionN*4+1), 3, 5));			\
	SWAP_SHORTS(V(type, 14, sectionN, 4), V(type, (sectionN*4+2), 3, 4));			\
	SWAP_SHORTS(V(type, 14, sectionN, 5), V(type, (sectionN*4+2), 3, 5));			\
	SWAP_SHORTS(V(type, 14, sectionN, 6), V(type, (sectionN*4+3), 3, 4));			\
	SWAP_SHORTS(V(type, 14, sectionN, 7), V(type, (sectionN*4+3), 3, 5))

#define SUPER_REARRANGEMENT_PORTION14(type) 							\
	SWAP_SHORTS(V(type, 14, 3, 6)) = SWAP_SHORT(V(type, 15, 3, 4)); 			\
	SWAP_SHORTS(V(type, 14, 3, 7)) = SWAP_SHORT(V(type, 15, 3, 5)); 			\
												\
	SUPER_REARRANGEMENT_PORTION14_HELPER(type, 4);						\
	SUPER_REARRANGEMENT_PORTION14_HELPER(type, 5);						\
	SUPER_REARRANGEMENT_PORTION14_HELPER(type, 6);						\
	SUPER_REARRANGEMENT_PORTION14_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION15_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 15, sectionN, 0), V(type, (sectionN*4), 3, 6));			\
	SWAP_SHORTS(V(type, 15, sectionN, 1), V(type, (sectionN*4), 3, 7));			\
	SWAP_SHORTS(V(type, 15, sectionN, 2), V(type, (sectionN*4+1), 3, 6));			\
	SWAP_SHORTS(V(type, 15, sectionN, 3), V(type, (sectionN*4+1), 3, 7));			\
	SWAP_SHORTS(V(type, 15, sectionN, 4), V(type, (sectionN*4+2), 3, 6));			\
	SWAP_SHORTS(V(type, 15, sectionN, 5), V(type, (sectionN*4+2), 3, 7));			\
	SWAP_SHORTS(V(type, 15, sectionN, 6), V(type, (sectionN*4+3), 3, 6));			\
	SWAP_SHORTS(V(type, 15, sectionN, 7), V(type, (sectionN*4+3), 3, 7))

#define SUPER_REARRANGEMENT_PORTION15(type) 						\
	SUPER_REARRANGEMENT_PORTION15_HELPER(type, 4);					\
	SUPER_REARRANGEMENT_PORTION15_HELPER(type, 5);					\
	SUPER_REARRANGEMENT_PORTION15_HELPER(type, 6);					\
	SUPER_REARRANGEMENT_PORTION15_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION16_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 16, sectionN, 0), V(type, (sectionN*4), 4, 0));				\
	SWAP_SHORTS(V(type, 16, sectionN, 1), V(type, (sectionN*4), 4, 1));				\
	SWAP_SHORTS(V(type, 16, sectionN, 2), V(type, (sectionN*4+1), 4, 0));			\
	SWAP_SHORTS(V(type, 16, sectionN, 3), V(type, (sectionN*4+1), 4, 1));			\
	SWAP_SHORTS(V(type, 16, sectionN, 4), V(type, (sectionN*4+2), 4, 0));			\
	SWAP_SHORTS(V(type, 16, sectionN, 5), V(type, (sectionN*4+2), 4, 1));			\
	SWAP_SHORTS(V(type, 16, sectionN, 6), V(type, (sectionN*4+3), 4, 0));			\
	SWAP_SHORTS(V(type, 16, sectionN, 7), V(type, (sectionN*4+3), 4, 1))

#define SUPER_REARRANGEMENT_PORTION16(type) 						\
	SWAP_SHORTS(V(type, 16, 4, 2)) = SWAP_SHORT(V(type, 17, 4, 0)); 				\
	SWAP_SHORTS(V(type, 16, 4, 3)) = SWAP_SHORT(V(type, 17, 4, 1)); 				\
	SWAP_SHORTS(V(type, 16, 4, 4)) = SWAP_SHORT(V(type, 18, 4, 0)); 				\
	SWAP_SHORTS(V(type, 16, 4, 5)) = SWAP_SHORT(V(type, 18, 4, 1)); 				\
	SWAP_SHORTS(V(type, 16, 4, 6)) = SWAP_SHORT(V(type, 19, 4, 0)); 				\
	SWAP_SHORTS(V(type, 16, 4, 7)) = SWAP_SHORT(V(type, 19, 4, 1)); 				\
											\
	SUPER_REARRANGEMENT_PORTION16_HELPER(type, 5);					\
	SUPER_REARRANGEMENT_PORTION16_HELPER(type, 6);					\
	SUPER_REARRANGEMENT_PORTION16_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION17_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 17, sectionN, 0), V(type, (sectionN*4), 4, 2));				\
	SWAP_SHORTS(V(type, 17, sectionN, 1), V(type, (sectionN*4), 4, 3));				\
	SWAP_SHORTS(V(type, 17, sectionN, 2), V(type, (sectionN*4+1), 4, 2));			\
	SWAP_SHORTS(V(type, 17, sectionN, 3), V(type, (sectionN*4+1), 4, 3));			\
	SWAP_SHORTS(V(type, 17, sectionN, 4), V(type, (sectionN*4+2), 4, 2));			\
	SWAP_SHORTS(V(type, 17, sectionN, 5), V(type, (sectionN*4+2), 4, 3));			\
	SWAP_SHORTS(V(type, 17, sectionN, 6), V(type, (sectionN*4+3), 4, 2));			\
	SWAP_SHORTS(V(type, 17, sectionN, 7), V(type, (sectionN*4+3), 4, 3))

#define SUPER_REARRANGEMENT_PORTION17(type) 						\
	SWAP_SHORTS(V(type, 17, 4, 4)) = SWAP_SHORT(V(type, 18, 4, 2)); 				\
	SWAP_SHORTS(V(type, 17, 4, 5)) = SWAP_SHORT(V(type, 18, 4, 3)); 				\
	SWAP_SHORTS(V(type, 17, 4, 6)) = SWAP_SHORT(V(type, 19, 4, 2)); 				\
	SWAP_SHORTS(V(type, 17, 4, 7)) = SWAP_SHORT(V(type, 19, 4, 3)); 				\
											\
	SUPER_REARRANGEMENT_PORTION17_HELPER(type, 5);					\
	SUPER_REARRANGEMENT_PORTION17_HELPER(type, 6);					\
	SUPER_REARRANGEMENT_PORTION17_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION18_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 18, sectionN, 0), V(type, (sectionN*4), 4, 4));				\
	SWAP_SHORTS(V(type, 18, sectionN, 1), V(type, (sectionN*4), 4, 5));				\
	SWAP_SHORTS(V(type, 18, sectionN, 2), V(type, (sectionN*4+1), 4, 4));			\
	SWAP_SHORTS(V(type, 18, sectionN, 3), V(type, (sectionN*4+1), 4, 5));			\
	SWAP_SHORTS(V(type, 18, sectionN, 4), V(type, (sectionN*4+2), 4, 4));			\
	SWAP_SHORTS(V(type, 18, sectionN, 5), V(type, (sectionN*4+2), 4, 5));			\
	SWAP_SHORTS(V(type, 18, sectionN, 6), V(type, (sectionN*4+3), 4, 4));			\
	SWAP_SHORTS(V(type, 18, sectionN, 7), V(type, (sectionN*4+3), 4, 5))

#define SUPER_REARRANGEMENT_PORTION18(type) 						\
	SWAP_SHORTS(V(type, 18, 4, 6)) = SWAP_SHORT(V(type, 19, 4, 4)); 				\
	SWAP_SHORTS(V(type, 18, 4, 7)) = SWAP_SHORT(V(type, 19, 4, 5)); 				\
											\
	SUPER_REARRANGEMENT_PORTION18_HELPER(type, 5);					\
	SUPER_REARRANGEMENT_PORTION18_HELPER(type, 6);					\
	SUPER_REARRANGEMENT_PORTION18_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION19_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 19, sectionN, 0), V(type, (sectionN*4), 4, 6));				\
	SWAP_SHORTS(V(type, 19, sectionN, 1), V(type, (sectionN*4), 4, 7));				\
	SWAP_SHORTS(V(type, 19, sectionN, 2), V(type, (sectionN*4+1), 4, 6));			\
	SWAP_SHORTS(V(type, 19, sectionN, 3), V(type, (sectionN*4+1), 4, 7));			\
	SWAP_SHORTS(V(type, 19, sectionN, 4), V(type, (sectionN*4+2), 4, 6));			\
	SWAP_SHORTS(V(type, 19, sectionN, 5), V(type, (sectionN*4+2), 4, 7));			\
	SWAP_SHORTS(V(type, 19, sectionN, 6), V(type, (sectionN*4+3), 4, 6));			\
	SWAP_SHORTS(V(type, 19, sectionN, 7), V(type, (sectionN*4+3), 4, 7))

#define SUPER_REARRANGEMENT_PORTION19(type) 						\
	SUPER_REARRANGEMENT_PORTION19_HELPER(type, 5);					\
	SUPER_REARRANGEMENT_PORTION19_HELPER(type, 6);					\
	SUPER_REARRANGEMENT_PORTION19_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION20_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 20, sectionN, 0), V(type, (sectionN*4), 5, 0));				\
	SWAP_SHORTS(V(type, 20, sectionN, 1), V(type, (sectionN*4), 5, 1));				\
	SWAP_SHORTS(V(type, 20, sectionN, 2), V(type, (sectionN*4+1), 5, 0));			\
	SWAP_SHORTS(V(type, 20, sectionN, 3), V(type, (sectionN*4+1), 5, 1));			\
	SWAP_SHORTS(V(type, 20, sectionN, 4), V(type, (sectionN*4+2), 5, 0));			\
	SWAP_SHORTS(V(type, 20, sectionN, 5), V(type, (sectionN*4+2), 5, 1));			\
	SWAP_SHORTS(V(type, 20, sectionN, 6), V(type, (sectionN*4+3), 5, 0));			\
	SWAP_SHORTS(V(type, 20, sectionN, 7), V(type, (sectionN*4+3), 5, 1))

#define SUPER_REARRANGEMENT_PORTION20(type) 						\
	SWAP_SHORTS(V(type, 20, 5, 2)) = SWAP_SHORT(V(type, 21, 5, 0)); 				\
	SWAP_SHORTS(V(type, 20, 5, 3)) = SWAP_SHORT(V(type, 21, 5, 1)); 				\
	SWAP_SHORTS(V(type, 20, 5, 4)) = SWAP_SHORT(V(type, 22, 5, 0)); 				\
	SWAP_SHORTS(V(type, 20, 5, 5)) = SWAP_SHORT(V(type, 22, 5, 1)); 				\
	SWAP_SHORTS(V(type, 20, 5, 6)) = SWAP_SHORT(V(type, 23, 5, 0)); 				\
	SWAP_SHORTS(V(type, 20, 5, 7)) = SWAP_SHORT(V(type, 23, 5, 1)); 				\
											\
	SUPER_REARRANGEMENT_PORTION20_HELPER(type, 6);					\
	SUPER_REARRANGEMENT_PORTION20_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION21_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 21, sectionN, 0), V(type, (sectionN*4), 5, 2));				\
	SWAP_SHORTS(V(type, 21, sectionN, 1), V(type, (sectionN*4), 5, 3));				\
	SWAP_SHORTS(V(type, 21, sectionN, 2), V(type, (sectionN*4+1), 5, 2));			\
	SWAP_SHORTS(V(type, 21, sectionN, 3), V(type, (sectionN*4+1), 5, 3));			\
	SWAP_SHORTS(V(type, 21, sectionN, 4), V(type, (sectionN*4+2), 5, 2));			\
	SWAP_SHORTS(V(type, 21, sectionN, 5), V(type, (sectionN*4+2), 5, 3));			\
	SWAP_SHORTS(V(type, 21, sectionN, 6), V(type, (sectionN*4+3), 5, 2));			\
	SWAP_SHORTS(V(type, 21, sectionN, 7), V(type, (sectionN*4+3), 5, 3))

#define SUPER_REARRANGEMENT_PORTION21(type) 						\
	SWAP_SHORTS(V(type, 21, 5, 4)) = SWAP_SHORT(V(type, 22, 5, 2)); 				\
	SWAP_SHORTS(V(type, 21, 5, 5)) = SWAP_SHORT(V(type, 22, 5, 3)); 				\
	SWAP_SHORTS(V(type, 21, 5, 6)) = SWAP_SHORT(V(type, 23, 5, 2)); 				\
	SWAP_SHORTS(V(type, 21, 5, 7)) = SWAP_SHORT(V(type, 23, 5, 3)); 				\
											\
	SUPER_REARRANGEMENT_PORTION21_HELPER(type, 6);					\
	SUPER_REARRANGEMENT_PORTION21_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION22_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 22, sectionN, 0), V(type, (sectionN*4), 5, 4));				\
	SWAP_SHORTS(V(type, 22, sectionN, 1), V(type, (sectionN*4), 5, 5));				\
	SWAP_SHORTS(V(type, 22, sectionN, 2), V(type, (sectionN*4+1), 5, 4));			\
	SWAP_SHORTS(V(type, 22, sectionN, 3), V(type, (sectionN*4+1), 5, 5));			\
	SWAP_SHORTS(V(type, 22, sectionN, 4), V(type, (sectionN*4+2), 5, 4));			\
	SWAP_SHORTS(V(type, 22, sectionN, 5), V(type, (sectionN*4+2), 5, 5));			\
	SWAP_SHORTS(V(type, 22, sectionN, 6), V(type, (sectionN*4+3), 5, 4));			\
	SWAP_SHORTS(V(type, 22, sectionN, 7), V(type, (sectionN*4+3), 5, 5))

#define SUPER_REARRANGEMENT_PORTION22(type) 						\
	SWAP_SHORTS(V(type, 22, 5, 6)) = SWAP_SHORT(V(type, 23, 5, 4)); 				\
	SWAP_SHORTS(V(type, 22, 5, 7)) = SWAP_SHORT(V(type, 23, 5, 5)); 				\
											\
	SUPER_REARRANGEMENT_PORTION22_HELPER(type, 6);					\
	SUPER_REARRANGEMENT_PORTION22_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION23_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 23, sectionN, 0), V(type, (sectionN*4), 5, 6));				\
	SWAP_SHORTS(V(type, 23, sectionN, 1), V(type, (sectionN*4), 5, 7));				\
	SWAP_SHORTS(V(type, 23, sectionN, 2), V(type, (sectionN*4+1), 5, 6));			\
	SWAP_SHORTS(V(type, 23, sectionN, 3), V(type, (sectionN*4+1), 5, 7));			\
	SWAP_SHORTS(V(type, 23, sectionN, 4), V(type, (sectionN*4+2), 5, 6));			\
	SWAP_SHORTS(V(type, 23, sectionN, 5), V(type, (sectionN*4+2), 5, 7));			\
	SWAP_SHORTS(V(type, 23, sectionN, 6), V(type, (sectionN*4+3), 5, 6));			\
	SWAP_SHORTS(V(type, 23, sectionN, 7), V(type, (sectionN*4+3), 5, 7))

#define SUPER_REARRANGEMENT_PORTION23(type) 						\
	SUPER_REARRANGEMENT_PORTION23_HELPER(type, 6);					\
	SUPER_REARRANGEMENT_PORTION23_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION24_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 24, sectionN, 0), V(type, (sectionN*4), 6, 0));				\
	SWAP_SHORTS(V(type, 24, sectionN, 1), V(type, (sectionN*4), 6, 1));				\
	SWAP_SHORTS(V(type, 24, sectionN, 2), V(type, (sectionN*4+1), 6, 0));			\
	SWAP_SHORTS(V(type, 24, sectionN, 3), V(type, (sectionN*4+1), 6, 1));			\
	SWAP_SHORTS(V(type, 24, sectionN, 4), V(type, (sectionN*4+2), 6, 0));			\
	SWAP_SHORTS(V(type, 24, sectionN, 5), V(type, (sectionN*4+2), 6, 1));			\
	SWAP_SHORTS(V(type, 24, sectionN, 6), V(type, (sectionN*4+3), 6, 0));			\
	SWAP_SHORTS(V(type, 24, sectionN, 7), V(type, (sectionN*4+3), 6, 1))

#define SUPER_REARRANGEMENT_PORTION24(type) 						\
	SWAP_SHORTS(V(type, 24, 6, 2)) = SWAP_SHORT(V(type, 25, 6, 0)); 				\
	SWAP_SHORTS(V(type, 24, 6, 3)) = SWAP_SHORT(V(type, 25, 6, 1)); 				\
	SWAP_SHORTS(V(type, 24, 6, 4)) = SWAP_SHORT(V(type, 26, 6, 0)); 				\
	SWAP_SHORTS(V(type, 24, 6, 5)) = SWAP_SHORT(V(type, 26, 6, 1)); 				\
	SWAP_SHORTS(V(type, 24, 6, 6)) = SWAP_SHORT(V(type, 27, 6, 0)); 				\
	SWAP_SHORTS(V(type, 24, 6, 7)) = SWAP_SHORT(V(type, 27, 6, 1)); 				\
											\
	SUPER_REARRANGEMENT_PORTION24_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION25_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 25, sectionN, 0), V(type, (sectionN*4), 6, 2));				\
	SWAP_SHORTS(V(type, 25, sectionN, 1), V(type, (sectionN*4), 6, 3));				\
	SWAP_SHORTS(V(type, 25, sectionN, 2), V(type, (sectionN*4+1), 6, 2));			\
	SWAP_SHORTS(V(type, 25, sectionN, 3), V(type, (sectionN*4+1), 6, 3));			\
	SWAP_SHORTS(V(type, 25, sectionN, 4), V(type, (sectionN*4+2), 6, 2));			\
	SWAP_SHORTS(V(type, 25, sectionN, 5), V(type, (sectionN*4+2), 6, 3));			\
	SWAP_SHORTS(V(type, 25, sectionN, 6), V(type, (sectionN*4+3), 6, 2));			\
	SWAP_SHORTS(V(type, 25, sectionN, 7), V(type, (sectionN*4+3), 6, 3))

#define SUPER_REARRANGEMENT_PORTION25(type) 						\
	SWAP_SHORTS(V(type, 25, 6, 4)) = SWAP_SHORT(V(type, 26, 6, 2)); 				\
	SWAP_SHORTS(V(type, 25, 6, 5)) = SWAP_SHORT(V(type, 26, 6, 3)); 				\
	SWAP_SHORTS(V(type, 25, 6, 6)) = SWAP_SHORT(V(type, 27, 6, 2)); 				\
	SWAP_SHORTS(V(type, 25, 6, 7)) = SWAP_SHORT(V(type, 27, 6, 3)); 				\
											\
	SUPER_REARRANGEMENT_PORTION25_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION26_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 26, sectionN, 0), V(type, (sectionN*4), 6, 4));				\
	SWAP_SHORTS(V(type, 26, sectionN, 1), V(type, (sectionN*4), 6, 5));				\
	SWAP_SHORTS(V(type, 26, sectionN, 2), V(type, (sectionN*4+1), 6, 4));			\
	SWAP_SHORTS(V(type, 26, sectionN, 3), V(type, (sectionN*4+1), 6, 5));			\
	SWAP_SHORTS(V(type, 26, sectionN, 4), V(type, (sectionN*4+2), 6, 4));			\
	SWAP_SHORTS(V(type, 26, sectionN, 5), V(type, (sectionN*4+2), 6, 5));			\
	SWAP_SHORTS(V(type, 26, sectionN, 6), V(type, (sectionN*4+3), 6, 4));			\
	SWAP_SHORTS(V(type, 26, sectionN, 7), V(type, (sectionN*4+3), 6, 5))

#define SUPER_REARRANGEMENT_PORTION26(type) 						\
	SWAP_SHORTS(V(type, 26, 6, 6)) = SWAP_SHORT(V(type, 27, 6, 4)); 				\
	SWAP_SHORTS(V(type, 26, 6, 7)) = SWAP_SHORT(V(type, 27, 6, 5)); 				\
											\
	SUPER_REARRANGEMENT_PORTION26_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION27_HELPER(type, sectionN)					\
	SWAP_SHORTS(V(type, 27, sectionN, 0), V(type, (sectionN*4), 6, 6));				\
	SWAP_SHORTS(V(type, 27, sectionN, 1), V(type, (sectionN*4), 6, 7));				\
	SWAP_SHORTS(V(type, 27, sectionN, 2), V(type, (sectionN*4+1), 6, 6));			\
	SWAP_SHORTS(V(type, 27, sectionN, 3), V(type, (sectionN*4+1), 6, 7));			\
	SWAP_SHORTS(V(type, 27, sectionN, 4), V(type, (sectionN*4+2), 6, 6));			\
	SWAP_SHORTS(V(type, 27, sectionN, 5), V(type, (sectionN*4+2), 6, 7));			\
	SWAP_SHORTS(V(type, 27, sectionN, 6), V(type, (sectionN*4+3), 6, 6));			\
	SWAP_SHORTS(V(type, 27, sectionN, 7), V(type, (sectionN*4+3), 6, 7))

#define SUPER_REARRANGEMENT_PORTION27(type) 						\
	SUPER_REARRANGEMENT_PORTION27_HELPER(type, 7)

#define SUPER_REARRANGEMENT_PORTION28(type) 						\
	SWAP_SHORTS(V(type, 28, 7, 2)) = SWAP_SHORT(V(type, 29, 7, 0)); 				\
	SWAP_SHORTS(V(type, 28, 7, 3)) = SWAP_SHORT(V(type, 29, 7, 1)); 				\
	SWAP_SHORTS(V(type, 28, 7, 4)) = SWAP_SHORT(V(type, 30, 7, 0)); 				\
	SWAP_SHORTS(V(type, 28, 7, 5)) = SWAP_SHORT(V(type, 30, 7, 1)); 				\
	SWAP_SHORTS(V(type, 28, 7, 6)) = SWAP_SHORT(V(type, 31, 7, 0)); 				\
	SWAP_SHORTS(V(type, 28, 7, 7)) = SWAP_SHORT(V(type, 31, 7, 1));

#define SUPER_REARRANGEMENT_PORTION29(type) 						\
	SWAP_SHORTS(V(type, 29, 7, 4)) = SWAP_SHORT(V(type, 30, 7, 2)); 				\
	SWAP_SHORTS(V(type, 29, 7, 5)) = SWAP_SHORT(V(type, 30, 7, 3)); 				\
	SWAP_SHORTS(V(type, 29, 7, 6)) = SWAP_SHORT(V(type, 31, 7, 2)); 				\
	SWAP_SHORTS(V(type, 29, 7, 7)) = SWAP_SHORT(V(type, 31, 7, 3))

#define SUPER_REARRANGEMENT_PORTION30(type) 						\
	SWAP_SHORTS(V(type, 30, 7, 6)) = SWAP_SHORT(V(type, 31, 7, 4)); 				\
	SWAP_SHORTS(V(type, 30, 7, 7)) = SWAP_SHORT(V(type, 31, 7, 5))

/*****************************************************************************************************
void replaceFoldCyclesUpdated3(vector<uint16_t>& input, uint32_t pNumber, int numRounds, vector<uint16_t>& EPUK, uint32_t& startOTPSectionCount) {
	DASSERT(input.size() == (k4 / sizeof(uint16_t))); // input must be 4K bytes long

	DASSERT(numRounds == 2 || numRounds == 4);
	//cout << "Encryption input " << endl;
	//printVector(input, 56, 65);

	//	chrono::duration<double, nano> duration;
	int round = 1;
	{

		// Process 1st section of 128 bits
		uint16_t* bptr1 = input.data();

		createV(type, pNumber, 0, 0);
		createV(type, pNumber, 0, 1);
		createV(type, pNumber, 0, 2);
		createV(type, pNumber, 0, 3);
		//uint16_t va1 = substitutionTable[*SA(0)];
		//uint16_t va2 = substitutionTable[*SA(1)];
		//uint16_t va3 = substitutionTable[*SA(2)];
		//uint16_t va4 = substitutionTable[*SA(3)];

		//cout << hex << "Substutited va1 to get new va1 : " << va1 << endl;

		V(type, pNumber, 0, 1) = V(type, pNumber, 0, 0);
		V(type, pNumber, 0, 2) = V(type, pNumber, 0, 1);
		V(type, pNumber, 0, 3) = V(type, pNumber, 0, 2);
		//va2 ^= va1;
		//va3 ^= va2;
		//va4 ^= va3;

		createV(type, pNumber, 0, 4);
		createV(type, pNumber, 0, 5);
		createV(type, pNumber, 0, 6);
		createV(type, pNumber, 0, 7);
		//uint16_t va5 = substitutionTable[*SA(4)];
		//uint16_t va6 = substitutionTable[*SA(5)];
		//uint16_t va7 = substitutionTable[*SA(6)];
		//uint16_t va8 = substitutionTable[*SA(7)];

		V(type, pNumber, 0, 4) = V(type, pNumber, 0, 3);
		V(type, pNumber, 0, 5) = V(type, pNumber, 0, 4);
		V(type, pNumber, 0, 6) = V(type, pNumber, 0, 5);
		V(type, pNumber, 0, 7) = V(type, pNumber, 0, 6);
		V(type, pNumber, 0, 1) = V(type, pNumber, 0, 7); // Circular-xor
		//va5 ^= va4;
		//va6 ^= va5;
		//va7 ^= va6;
		//va8 ^= va7;
		//va1 ^= va8; //Circular-xor across section


		uint64_t* ukptr1 = reinterpret_cast<uint64_t *>(EPUK.data()+start);

#define EPUK_SHORT(a)	*(reinterpret_cast<uint16_t *>(ukptr1)+a)

		V(type, pNumber, 0, 0) ^= EPUK_SHORT(0) ^ OTPs[EPUK[startOTPSectionCount]][0];
		V(type, pNumber, 0, 1) ^= EPUK_SHORT(1) ^ OTPs[EPUK[startOTPSectionCount]][1];
		V(type, pNumber, 0, 2) ^= EPUK_SHORT(2) ^ OTPs[EPUK[startOTPSectionCount]][2];
		V(type, pNumber, 0, 3) ^= EPUK_SHORT(3) ^ OTPs[EPUK[startOTPSectionCount]][3];
		V(type, pNumber, 0, 4) ^= EPUK_SHORT(4) ^ OTPs[EPUK[startOTPSectionCount]][4];
		V(type, pNumber, 0, 5) ^= EPUK_SHORT(5) ^ OTPs[EPUK[startOTPSectionCount]][5];
		V(type, pNumber, 0, 6) ^= EPUK_SHORT(6) ^ OTPs[EPUK[startOTPSectionCount]][6];
		V(type, pNumber, 0, 7) ^= EPUK_SHORT(7) ^ OTPs[EPUK[startOTPSectionCount]][7];
		//va1 ^= EPUK_SHORT(0) ^ OTPs[EPUK[startOTPSectionCount]][0];
		//va2 ^= EPUK_SHORT(1) ^ OTPs[EPUK[startOTPSectionCount]][1];
		//va3 ^= EPUK_SHORT(2) ^ OTPs[EPUK[startOTPSectionCount]][2];
		//va4 ^= EPUK_SHORT(3) ^ OTPs[EPUK[startOTPSectionCount]][3];
		//va5 ^= EPUK_SHORT(4) ^ OTPs[EPUK[startOTPSectionCount]][4];
		//va6 ^= EPUK_SHORT(5) ^ OTPs[EPUK[startOTPSectionCount]][5];
		//va7 ^= EPUK_SHORT(6) ^ OTPs[EPUK[startOTPSectionCount]][6];
		//va8 ^= EPUK_SHORT(7) ^ OTPs[EPUK[startOTPSectionCount]][7];


		// Process 2nd section of 128 bits
		createV(type, pNumber, 1, 0);
		createV(type, pNumber, 1, 1);
		createV(type, pNumber, 1, 2);
		createV(type, pNumber, 1, 3);
		//uint16_t vb1 = substitutionTable[*SB(0)];
		//uint16_t vb2 = substitutionTable[*SB(1)];
		//uint16_t vb3 = substitutionTable[*SB(2)];
		//uint16_t vb4 = substitutionTable[*SB(3)];

		V(type, pNumber, 1, 1) = V(type, pNumber, 1, 0);
		V(type, pNumber, 1, 2) = V(type, pNumber, 1, 1);
		V(type, pNumber, 1, 3) = V(type, pNumber, 1, 2);
		//vb2 ^= vb1;
		//vb3 ^= vb2;
		//vb4 ^= vb3;

		createV(type, pNumber, 1, 4);
		createV(type, pNumber, 1, 5);
		createV(type, pNumber, 1, 6);
		createV(type, pNumber, 1, 7);
		//uint16_t vb5 = substitutionTable[*SB(4)];
		//uint16_t vb6 = substitutionTable[*SB(5)];
		//uint16_t vb7 = substitutionTable[*SB(6)];
		//uint16_t vb8 = substitutionTable[*SB(7)];

		V(type, pNumber, 1, 4) = V(type, pNumber, 1, 3);
		V(type, pNumber, 1, 5) = V(type, pNumber, 1, 4);
		V(type, pNumber, 1, 6) = V(type, pNumber, 1, 5);
		V(type, pNumber, 1, 7) = V(type, pNumber, 1, 6);
		V(type, pNumber, 1, 0) = V(type, pNumber, 1, 7); // Circular-xor
		//vb5 ^= vb4;
		//vb6 ^= vb5;
		//vb7 ^= vb6;
		//vb8 ^= vb7;
		//vb1 ^= vb8; //Circular-xor across section

		V(type, pNumber, 1, 0) ^= EPUK_SHORT(8) ^ OTPs[EPUK[startOTPSectionCount]][8];
		V(type, pNumber, 1, 1) ^= EPUK_SHORT(9) ^ OTPs[EPUK[startOTPSectionCount]][9];
		V(type, pNumber, 1, 2) ^= EPUK_SHORT(10) ^ OTPs[EPUK[startOTPSectionCount]][10];
		V(type, pNumber, 1, 3) ^= EPUK_SHORT(11) ^ OTPs[EPUK[startOTPSectionCount]][11];
		V(type, pNumber, 1, 4) ^= EPUK_SHORT(12) ^ OTPs[EPUK[startOTPSectionCount]][12];
		V(type, pNumber, 1, 5) ^= EPUK_SHORT(13) ^ OTPs[EPUK[startOTPSectionCount]][13];
		V(type, pNumber, 1, 6) ^= EPUK_SHORT(14) ^ OTPs[EPUK[startOTPSectionCount]][14];
		V(type, pNumber, 1, 7) ^= EPUK_SHORT(15) ^ OTPs[EPUK[startOTPSectionCount]][15];
		//vb1 ^= EPUK_SHORT(8) ^ OTPs[EPUK[startOTPSectionCount]][8];
		//vb2 ^= EPUK_SHORT(9) ^ OTPs[EPUK[startOTPSectionCount]][9];
		//vb3 ^= EPUK_SHORT(10) ^ OTPs[EPUK[startOTPSectionCount]][10];
		//vb4 ^= EPUK_SHORT(11) ^ OTPs[EPUK[startOTPSectionCount]][11];
		//vb5 ^= EPUK_SHORT(12) ^ OTPs[EPUK[startOTPSectionCount]][12];
		//vb6 ^= EPUK_SHORT(13) ^ OTPs[EPUK[startOTPSectionCount]][13];
		//vb7 ^= EPUK_SHORT(14) ^ OTPs[EPUK[startOTPSectionCount]][14];
		//vb8 ^= EPUK_SHORT(15) ^ OTPs[EPUK[startOTPSectionCount]][15];

		// Process 3rd section of 128 bits
		createV(type, pNumber, 2, 0);
		createV(type, pNumber, 2, 1);
		createV(type, pNumber, 2, 2);
		createV(type, pNumber, 2, 3);

		V(type, pNumber, 2, 1) = V(type, pNumber, 2, 0);
		V(type, pNumber, 2, 2) = V(type, pNumber, 2, 1);
		V(type, pNumber, 2, 3) = V(type, pNumber, 2, 2);

		createV(type, pNumber, 2, 4);
		createV(type, pNumber, 2, 5);
		createV(type, pNumber, 2, 6);
		createV(type, pNumber, 2, 7);

		V(type, pNumber, 2, 4) = V(type, pNumber, 2, 3);
		V(type, pNumber, 2, 5) = V(type, pNumber, 2, 4);
		V(type, pNumber, 2, 6) = V(type, pNumber, 2, 5);
		V(type, pNumber, 2, 7) = V(type, pNumber, 2, 6);
		V(type, pNumber, 2, 0) = V(type, pNumber, 2, 7); // Circular-xor

		V(type, pNumber, 2, 0) ^= EPUK_SHORT(16) ^ OTPs[EPUK[startOTPSectionCount]][16];
		V(type, pNumber, 2, 1) ^= EPUK_SHORT(17) ^ OTPs[EPUK[startOTPSectionCount]][17];
		V(type, pNumber, 2, 2) ^= EPUK_SHORT(18) ^ OTPs[EPUK[startOTPSectionCount]][18];
		V(type, pNumber, 2, 3) ^= EPUK_SHORT(19) ^ OTPs[EPUK[startOTPSectionCount]][19];
		V(type, pNumber, 2, 4) ^= EPUK_SHORT(20) ^ OTPs[EPUK[startOTPSectionCount]][20];
		V(type, pNumber, 2, 5) ^= EPUK_SHORT(21) ^ OTPs[EPUK[startOTPSectionCount]][21];
		V(type, pNumber, 2, 6) ^= EPUK_SHORT(22) ^ OTPs[EPUK[startOTPSectionCount]][22];
		V(type, pNumber, 2, 7) ^= EPUK_SHORT(23) ^ OTPs[EPUK[startOTPSectionCount]][23];

		// Process 4th section of 128 bits
		createV(type, pNumber, 3, 0);
		createV(type, pNumber, 3, 1);
		createV(type, pNumber, 3, 2);
		createV(type, pNumber, 3, 3);

		V(type, pNumber, 3, 1) = V(type, pNumber, 3, 0);
		V(type, pNumber, 3, 2) = V(type, pNumber, 3, 1);
		V(type, pNumber, 3, 3) = V(type, pNumber, 3, 2);

		createV(type, pNumber, 3, 4);
		createV(type, pNumber, 3, 5);
		createV(type, pNumber, 3, 6);
		createV(type, pNumber, 3, 7);

		V(type, pNumber, 3, 4) = V(type, pNumber, 3, 3);
		V(type, pNumber, 3, 5) = V(type, pNumber, 3, 4);
		V(type, pNumber, 3, 6) = V(type, pNumber, 3, 5);
		V(type, pNumber, 3, 7) = V(type, pNumber, 3, 6);
		V(type, pNumber, 3, 0) = V(type, pNumber, 3, 7); // Circular-xor

		V(type, pNumber, 3, 0) ^= EPUK_SHORT(24) ^ OTPs[EPUK[startOTPSectionCount]][24];
		V(type, pNumber, 3, 1) ^= EPUK_SHORT(25) ^ OTPs[EPUK[startOTPSectionCount]][25];
		V(type, pNumber, 3, 2) ^= EPUK_SHORT(26) ^ OTPs[EPUK[startOTPSectionCount]][26];
		V(type, pNumber, 3, 3) ^= EPUK_SHORT(27) ^ OTPs[EPUK[startOTPSectionCount]][27];
		V(type, pNumber, 3, 4) ^= EPUK_SHORT(28) ^ OTPs[EPUK[startOTPSectionCount]][28];
		V(type, pNumber, 3, 5) ^= EPUK_SHORT(29) ^ OTPs[EPUK[startOTPSectionCount]][29];
		V(type, pNumber, 3, 6) ^= EPUK_SHORT(30) ^ OTPs[EPUK[startOTPSectionCount]][30];
		V(type, pNumber, 3, 7) ^= EPUK_SHORT(31) ^ OTPs[EPUK[startOTPSectionCount]][31];
		++startOTPSectionCount;

		// Process 5th section of 128 bits
		createV(type, pNumber, 4, 0);
		createV(type, pNumber, 4, 1);
		createV(type, pNumber, 4, 2);
		createV(type, pNumber, 4, 3);

		V(type, pNumber, 4, 1) = V(type, pNumber, 4, 0);
		V(type, pNumber, 4, 2) = V(type, pNumber, 4, 1);
		V(type, pNumber, 4, 3) = V(type, pNumber, 4, 2);

		createV(type, pNumber, 4, 4);
		createV(type, pNumber, 4, 5);
		createV(type, pNumber, 4, 6);
		createV(type, pNumber, 4, 7);

		V(type, pNumber, 4, 4) = V(type, pNumber, 4, 3);
		V(type, pNumber, 4, 5) = V(type, pNumber, 4, 4);
		V(type, pNumber, 4, 6) = V(type, pNumber, 4, 5);
		V(type, pNumber, 4, 7) = V(type, pNumber, 4, 6);
		V(type, pNumber, 4, 0) = V(type, pNumber, 4, 7); // Circular-xor

		V(type, pNumber, 4, 0) ^= EPUK_SHORT(32) ^ OTPs[EPUK[startOTPSectionCount]][0];
		V(type, pNumber, 4, 0) ^= EPUK_SHORT(33) ^ OTPs[EPUK[startOTPSectionCount]][1];
		V(type, pNumber, 4, 0) ^= EPUK_SHORT(34) ^ OTPs[EPUK[startOTPSectionCount]][2];
		V(type, pNumber, 4, 0) ^= EPUK_SHORT(35) ^ OTPs[EPUK[startOTPSectionCount]][3];
		V(type, pNumber, 4, 0) ^= EPUK_SHORT(36) ^ OTPs[EPUK[startOTPSectionCount]][4];
		V(type, pNumber, 4, 0) ^= EPUK_SHORT(37) ^ OTPs[EPUK[startOTPSectionCount]][5];
		V(type, pNumber, 4, 0) ^= EPUK_SHORT(38) ^ OTPs[EPUK[startOTPSectionCount]][6];
		V(type, pNumber, 4, 0) ^= EPUK_SHORT(39) ^ OTPs[EPUK[startOTPSectionCount]][7];

		// Process 6th section of 128 bits
		createV(type, pNumber, 5, 0);
		createV(type, pNumber, 5, 1);
		createV(type, pNumber, 5, 2);
		createV(type, pNumber, 5, 3);

		V(type, pNumber, 5, 1) = V(type, pNumber, 5, 0);
		V(type, pNumber, 5, 2) = V(type, pNumber, 5, 1);
		V(type, pNumber, 5, 3) = V(type, pNumber, 5, 2);

		createV(type, pNumber, 5, 4);
		createV(type, pNumber, 5, 5);
		createV(type, pNumber, 5, 6);
		createV(type, pNumber, 5, 7);

		V(type, pNumber, 5, 4) = V(type, pNumber, 5, 3);
		V(type, pNumber, 5, 5) = V(type, pNumber, 5, 4);
		V(type, pNumber, 5, 6) = V(type, pNumber, 5, 5);
		V(type, pNumber, 5, 7) = V(type, pNumber, 5, 6);
		V(type, pNumber, 5, 0) = V(type, pNumber, 5, 7); // Circular-xor

		V(type, pNumber, 5, 0) ^= EPUK_SHORT(40) ^ OTPs[EPUK[startOTPSectionCount]][8];
		V(type, pNumber, 5, 1) ^= EPUK_SHORT(41) ^ OTPs[EPUK[startOTPSectionCount]][9];
		V(type, pNumber, 5, 2) ^= EPUK_SHORT(42) ^ OTPs[EPUK[startOTPSectionCount]][10];
		V(type, pNumber, 5, 3) ^= EPUK_SHORT(43) ^ OTPs[EPUK[startOTPSectionCount]][11];
		V(type, pNumber, 5, 4) ^= EPUK_SHORT(44) ^ OTPs[EPUK[startOTPSectionCount]][12];
		V(type, pNumber, 5, 5) ^= EPUK_SHORT(45) ^ OTPs[EPUK[startOTPSectionCount]][13];
		V(type, pNumber, 5, 6) ^= EPUK_SHORT(46) ^ OTPs[EPUK[startOTPSectionCount]][14];
		V(type, pNumber, 5, 7) ^= EPUK_SHORT(47) ^ OTPs[EPUK[startOTPSectionCount]][15];

		// Process 7th section of 128 bits
		createV(type, pNumber, 6, 0);
		createV(type, pNumber, 6, 1);
		createV(type, pNumber, 6, 2);
		createV(type, pNumber, 6, 3);

		V(type, pNumber, 6, 1) = V(type, pNumber, 6, 0);
		V(type, pNumber, 6, 2) = V(type, pNumber, 6, 1);
		V(type, pNumber, 6, 3) = V(type, pNumber, 6, 2);

		createV(type, pNumber, 6, 4);
		createV(type, pNumber, 6, 5);
		createV(type, pNumber, 6, 6);
		createV(type, pNumber, 6, 7);

		V(type, pNumber, 6, 4) = V(type, pNumber, 6, 3);
		V(type, pNumber, 6, 5) = V(type, pNumber, 6, 4);
		V(type, pNumber, 6, 6) = V(type, pNumber, 6, 5);
		V(type, pNumber, 6, 7) = V(type, pNumber, 6, 6);
		V(type, pNumber, 6, 0) = V(type, pNumber, 6, 7); // Circular-xor

		V(type, pNumber, 6, 0) ^= EPUK_SHORT(48) ^ OTPs[EPUK[startOTPSectionCount]][16];
		V(type, pNumber, 6, 1) ^= EPUK_SHORT(49) ^ OTPs[EPUK[startOTPSectionCount]][17];
		V(type, pNumber, 6, 2) ^= EPUK_SHORT(50) ^ OTPs[EPUK[startOTPSectionCount]][18];
		V(type, pNumber, 6, 3) ^= EPUK_SHORT(51) ^ OTPs[EPUK[startOTPSectionCount]][19];
		V(type, pNumber, 6, 4) ^= EPUK_SHORT(52) ^ OTPs[EPUK[startOTPSectionCount]][20];
		V(type, pNumber, 6, 5) ^= EPUK_SHORT(53) ^ OTPs[EPUK[startOTPSectionCount]][21];
		V(type, pNumber, 6, 6) ^= EPUK_SHORT(54) ^ OTPs[EPUK[startOTPSectionCount]][22];
		V(type, pNumber, 6, 7) ^= EPUK_SHORT(55) ^ OTPs[EPUK[startOTPSectionCount]][23];

		// Process 8th section of 128 bits
		createV(type, pNumber, 7, 0);
		createV(type, pNumber, 7, 1);
		createV(type, pNumber, 7, 2);
		createV(type, pNumber, 7, 3);

		V(type, pNumber, 7, 1) = V(type, pNumber, 7, 0);
		V(type, pNumber, 7, 2) = V(type, pNumber, 7, 1);
		V(type, pNumber, 7, 3) = V(type, pNumber, 7, 2);

		createV(type, pNumber, 7, 4);
		createV(type, pNumber, 7, 5);
		createV(type, pNumber, 7, 6);
		createV(type, pNumber, 7, 7);

		V(type, pNumber, 7, 4) = V(type, pNumber, 7, 3);
		V(type, pNumber, 7, 5) = V(type, pNumber, 7, 4);
		V(type, pNumber, 7, 6) = V(type, pNumber, 7, 5);
		V(type, pNumber, 7, 7) = V(type, pNumber, 7, 6);
		V(type, pNumber, 7, 0) = V(type, pNumber, 7, 7); // Circular-xor

		V(type, pNumber, 7, 0) ^= EPUK_SHORT(56) ^ OTPs[EPUK[startOTPSectionCount]][24];
		V(type, pNumber, 7, 1) ^= EPUK_SHORT(57) ^ OTPs[EPUK[startOTPSectionCount]][25];
		V(type, pNumber, 7, 2) ^= EPUK_SHORT(58) ^ OTPs[EPUK[startOTPSectionCount]][26];
		V(type, pNumber, 7, 3) ^= EPUK_SHORT(59) ^ OTPs[EPUK[startOTPSectionCount]][27];
		V(type, pNumber, 7, 4) ^= EPUK_SHORT(60) ^ OTPs[EPUK[startOTPSectionCount]][28];
		V(type, pNumber, 7, 5) ^= EPUK_SHORT(61) ^ OTPs[EPUK[startOTPSectionCount]][29];
		V(type, pNumber, 7, 6) ^= EPUK_SHORT(62) ^ OTPs[EPUK[startOTPSectionCount]][30];
		V(type, pNumber, 7, 7) ^= EPUK_SHORT(63) ^ OTPs[EPUK[startOTPSectionCount]][31];
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

		*SH(7) = vh8;

		//cout << "Encrypted " << endl;
		//printVector(input, 56, 65);
	}
}
}
}
*****************************************************************************************************/

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

int main() {
	initialize();

	uint32_t size = k4 / sizeof(uint16_t); // 4K bytes
	vector<uint16_t> original(size);
	for (uint16_t i = 0; i < original.size(); ++i) {
		original.at(i) = rand();
	}
	vector<uint16_t> input(original);
	EPUK = original;

	// TEST 0: Load Input Vector ==========================================================================================
	LOAD_PORTION_ALL(reinterpret_cast<uint16_t*>(input.data()));

	// TEST 1: Substitution ===============================================================================================
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	SUBSTITUTE_SECTION(d, 5, 0);
	SET_SECTION(d, input.data(), 5, 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	REVERSE_SUBSTITUTE_SECTION(d, 5, 0);
	SET_SECTION(d, input.data(), 5, 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Forward and reverse substituted a section" << endl;
	
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	SUBSTITUTE_PORTION(d, 0);
	SET_PORTION(d, input.data(), 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	REVERSE_SUBSTITUTE_PORTION(d, 0);
	SET_PORTION(d, input.data(), 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Forward and reverse substituted a portion" << endl;

	// TEST 2: Rotate =====================================================================================================
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	ROTATE_SECTION_LEFT(d, 0, 0);
	SET_PORTION(d, input.data(), 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	ROTATE_SECTION_RIGHT(d, 0, 0);
	SET_PORTION(d, input.data(), 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Left and right rotated a section" << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	ROTATE_PORTION_LEFT(d, 5);
	SET_PORTION(d, input.data(), 5);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	ROTATE_PORTION_RIGHT(d, 5);
	SET_PORTION(d, input.data(), 5);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Left and right rotated a portion" << endl;

	// TEST 3: Rearragenment ==============================================================================================
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	PORTION_REARRANGE(d, 5);
	SET_PORTION(d, input.data(), 5);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	PORTION_REARRANGE(d, 5);
	SET_PORTION(d, input.data(), 5);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Rearrangement a portion" << endl;

	// TEST 4: Circular xor ===============================================================================================
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	XOR_SECTION_FORWARD(d, 0, 0);
	SET_PORTION(d, input.data(), 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	XOR_SECTION_REVERSE(d, 0, 0);
	SET_PORTION(d, input.data(), 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Circular xored a section" << endl;

	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	XOR_PORTION_FORWARD(d, 5);
	SET_PORTION(d, input.data(), 5);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	XOR_PORTION_REVERSE(d, 5);
	SET_PORTION(d, input.data(), 5);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Circular xored a portion" << endl;

	// TEST 5: Replacement ===============================================================================================
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	REPLACEMENT_PORTION_FORWARD(d, 0);
	SET_PORTION(d, input.data(), 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	REPLACEMENT_PORTION_REVERSE(d, 0);
	SET_PORTION(d, input.data(), 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Replacement on a portion" << endl;
	
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	REPLACEMENT_PORTION_FORWARD_ALL(d);
	SET_PORTION_ALL(input.data());
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	REPLACEMENT_PORTION_REVERSE_ALL(d);
	SET_PORTION_ALL(input.data());
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Replacement on entire 4K page" << endl;
	
	// TEST 6: Super rearrangement ========================================================================================
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	SUPER_REARRANGEMENT_PORTION0(d);
	SET_PORTION(d, input.data(), 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	SUPER_REARRANGEMENT_PORTION0(d);
	SET_PORTION(d, input.data(), 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Super rearrangement on a portion" << endl;
	
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	REPLACEMENT_PORTION_FORWARD_ALL(d);
	SET_PORTION_ALL(input.data());
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	REPLACEMENT_PORTION_REVERSE_ALL(d);
	SET_PORTION_ALL(input.data());
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Replacement on entire 4K page" << endl;

	// TEST 4: Xor data, EPUK, and OTP ====================================================================================
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	//XOR_OTP_EPUK_FOUR_SECTIONS(0, 0, 0, 0);
	//SET_SECTION(d, input.data(), 0, 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) != 0);
	//XOR_OTP_EPUK_FOUR_SECTIONS(0, 0, 0, 0);
	//SET_SECTION(d, input.data(), 0, 0);
	assert(memcmp(reinterpret_cast<const char*>(input.data()), reinterpret_cast<const char*>(original.data()), size) == 0);
	cout << "Forward and reverse xored four sections" << endl;
	
	
	return 0;
}
