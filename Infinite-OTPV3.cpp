#include <iostream>
#include <vector>
#include <assert.h>
#include <cstdint>

using namespace std;

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

#define V(portionN, sectionN, shortN)		v_ ## portionN_ ## sectionN_ ## shortN 

#define ROTATE_SECTION_LEFT(portionN, sectionN)						\
/* S0 - S1 - S2 - S3 - S4 - S5 - S6 - S7 */						\
do {											\
	uint16_t prev, next;								\
	prev = (V(portionN, sectionN, 0) & 0xFF);					\
	next = (V(portionN, sectionN, 7) & 0xFF);					\
											\
	V(portionN, sectionN, 7) = (V(portionN, sectionN, 7) << 8) | prev;		\
	prev = next;									\
	next = V(portionN, sectionN, 6) & 0xFF;						\
											\
	V(portionN, sectionN, 6) = (V(portionN, sectionN, 6) << 8) | prev;		\
	prev = next;									\
	next = V(portionN, sectionN, 5) & 0xFF;						\
											\
	V(portionN, sectionN, 5) = (V(portionN, sectionN, 5) << 8) | prev;		\
	prev = next;									\
	next = V(portionN, sectionN, 4) & 0xFF;						\
											\
	V(portionN, sectionN, 4) = (V(portionN, sectionN, 4) << 8) | prev;		\
	prev = next;									\
	next = V(portionN, sectionN, 3) & 0xFF;						\
											\
	V(portionN, sectionN, 3) = (V(portionN, sectionN, 3) << 8) | prev;		\
	prev = next;									\
	next = V(portionN, sectionN, 2) & 0xFF;						\
											\
	V(portionN, sectionN, 2) = (V(portionN, sectionN, 2) << 8) | prev;		\
	prev = next;									\
	next = V(portionN, sectionN, 1) & 0xFF;						\
											\
	V(portionN, sectionN, 1) = (V(portionN, sectionN, 1) << 8) | prev;		\
	prev = next;									\
											\
	V(portionN, sectionN, 0) = (V(portionN, sectionN, 0) << 8) | prev;		\
} while(0)

#define ROTATE_SECTION_RIGHT(portionN, sectionN)					\
/* S0 - S1 - S2 - S3 - S4 - S5 - S6 - S7 */						\
do {											\
	uint16_t prev, next;								\
	prev = (V(portionN, sectionN, 7) & 0xFF) << 8;					\
	next = (V(portionN, sectionN, 0) & 0xFF) << 8;					\
											\
	V(portionN, sectionN, 0) = (V(portionN, sectionN, 0) >> 8) | prev;		\
	prev = next;									\
	next = (V(portionN, sectionN, 1) & 0xFF) << 8;					\
											\
	V(portionN, sectionN, 1) = (V(portionN, sectionN, 1) >> 8) | prev;		\
	prev = next;									\
	next = (V(portionN, sectionN, 1) & 0xFF) << 8;					\
											\
	V(portionN, sectionN, 2) = (V(portionN, sectionN, 2) >> 8) | prev;		\
	prev = next;									\
	next = (V(portionN, sectionN, 1) & 0xFF) << 8;					\
											\
	V(portionN, sectionN, 3) = (V(portionN, sectionN, 3) >> 8) | prev;		\
	prev = next;									\
	next = (V(portionN, sectionN, 1) & 0xFF) << 8;					\
											\
	V(portionN, sectionN, 4) = (V(portionN, sectionN, 4) >> 8) | prev;		\
	prev = next;									\
	next = (V(portionN, sectionN, 1) & 0xFF) << 8;					\
											\
	V(portionN, sectionN, 5) = (V(portionN, sectionN, 5) >> 8) | prev;		\
	prev = next;									\
	next = (V(portionN, sectionN, 1) & 0xFF) << 8;					\
											\
	V(portionN, sectionN, 6) = (V(portionN, sectionN, 6) >> 8) | prev;		\
	prev = next;									\
											\
	V(portionN, sectionN, 7) = (V(portionN, sectionN, 7) >> 8) | prev;		\
} while (0)

#define ROTATE_PORTION_LEFT(portionN) 							\
	ROTATE_SECTION_LEFT(portionN, 0);						\
	ROTATE_SECTION_LEFT(portionN, 1);						\
	ROTATE_SECTION_LEFT(portionN, 2);						\
	ROTATE_SECTION_LEFT(portionN, 3);						\
	ROTATE_SECTION_LEFT(portionN, 4);						\
	ROTATE_SECTION_LEFT(portionN, 5);						\
	ROTATE_SECTION_LEFT(portionN, 6);						\

#define ROTATE_PORTION_RIGHT(portionN) 							\
	ROTATE_SECTION_RIGHT(portionN, 0);						\
	ROTATE_SECTION_RIGHT(portionN, 1);						\
	ROTATE_SECTION_RIGHT(portionN, 2);						\
	ROTATE_SECTION_RIGHT(portionN, 3);						\
	ROTATE_SECTION_RIGHT(portionN, 4);						\
	ROTATE_SECTION_RIGHT(portionN, 5);						\
	ROTATE_SECTION_RIGHT(portionN, 6);						\
	ROTATE_SECTION_RIGHT(portionN, 7)

#define ROTATE_PORTION_LEFT_ALL()							\
	ROTATE_PORTION_LEFT(0);							\
	ROTATE_PORTION_LEFT(1);							\
	ROTATE_PORTION_LEFT(2);							\
	ROTATE_PORTION_LEFT(3);							\
	ROTATE_PORTION_LEFT(4);							\
	ROTATE_PORTION_LEFT(5);							\
	ROTATE_PORTION_LEFT(6);							\
	ROTATE_PORTION_LEFT(7);							\
	ROTATE_PORTION_LEFT(8);							\
	ROTATE_PORTION_LEFT(9);							\
	ROTATE_PORTION_LEFT(10);							\
	ROTATE_PORTION_LEFT(11);							\
	ROTATE_PORTION_LEFT(12);							\
	ROTATE_PORTION_LEFT(14);							\
	ROTATE_PORTION_LEFT(15);							\
	ROTATE_PORTION_LEFT(16);							\
	ROTATE_PORTION_LEFT(17);							\
	ROTATE_PORTION_LEFT(18);							\
	ROTATE_PORTION_LEFT(19);							\
	ROTATE_PORTION_LEFT(20);							\
	ROTATE_PORTION_LEFT(21);							\
	ROTATE_PORTION_LEFT(22);							\
	ROTATE_PORTION_LEFT(23);							\
	ROTATE_PORTION_LEFT(24);							\
	ROTATE_PORTION_LEFT(25);							\
	ROTATE_PORTION_LEFT(26);							\
	ROTATE_PORTION_LEFT(27);							\
	ROTATE_PORTION_LEFT(28);							\
	ROTATE_PORTION_LEFT(29);							\
	ROTATE_PORTION_LEFT(30);							\
	ROTATE_PORTION_LEFT(31)

#define ROTATE_PORTION_RIGHT_ALL()							\
	ROTATE_PORTION_RIGHT(0);							\
	ROTATE_PORTION_RIGHT(1);							\
	ROTATE_PORTION_RIGHT(2);							\
	ROTATE_PORTION_RIGHT(3);							\
	ROTATE_PORTION_RIGHT(4);							\
	ROTATE_PORTION_RIGHT(5);							\
	ROTATE_PORTION_RIGHT(6);							\
	ROTATE_PORTION_RIGHT(7);							\
	ROTATE_PORTION_RIGHT(8);							\
	ROTATE_PORTION_RIGHT(9);							\
	ROTATE_PORTION_RIGHT(10);							\
	ROTATE_PORTION_RIGHT(11);							\
	ROTATE_PORTION_RIGHT(12);							\
	ROTATE_PORTION_RIGHT(14);							\
	ROTATE_PORTION_RIGHT(15);							\
	ROTATE_PORTION_RIGHT(16);							\
	ROTATE_PORTION_RIGHT(17);							\
	ROTATE_PORTION_RIGHT(18);							\
	ROTATE_PORTION_RIGHT(19);							\
	ROTATE_PORTION_RIGHT(20);							\
	ROTATE_PORTION_RIGHT(21);							\
	ROTATE_PORTION_RIGHT(22);							\
	ROTATE_PORTION_RIGHT(23);							\
	ROTATE_PORTION_RIGHT(24);							\
	ROTATE_PORTION_RIGHT(25);							\
	ROTATE_PORTION_RIGHT(26);							\
	ROTATE_PORTION_RIGHT(27);							\
	ROTATE_PORTION_RIGHT(28);							\
	ROTATE_PORTION_RIGHT(29);							\
	ROTATE_PORTION_RIGHT(30);							\
	ROTATE_PORTION_RIGHT(31)

#define LOAD_SECTION(ptr, portionN, sectionN)						\
	uint16_t V(portionN, sectionN, 0) = *S(ptr, portionN, sectionN, 0); 		\
	uint16_t V(portionN, sectionN, 1) = *S(ptr, portionN, sectionN, 1); 		\
	uint16_t V(portionN, sectionN, 2) = *S(ptr, portionN, sectionN, 2); 		\
	uint16_t V(portionN, sectionN, 3) = *S(ptr, portionN, sectionN, 3); 		\
	uint16_t V(portionN, sectionN, 4) = *S(ptr, portionN, sectionN, 4); 		\
	uint16_t V(portionN, sectionN, 5) = *S(ptr, portionN, sectionN, 5); 		\
	uint16_t V(portionN, sectionN, 6) = *S(ptr, portionN, sectionN, 6); 		\
	uint16_t V(portionN, sectionN, 7) = *S(ptr, portionN, sectionN, 7)

#define LOAD_PORTION(ptr, portionN)							\
	LOAD_SECTION(ptr, portionN, 0);							\
	LOAD_SECTION(ptr, portionN, 1);							\
	LOAD_SECTION(ptr, portionN, 2);							\
	LOAD_SECTION(ptr, portionN, 3);							\
	LOAD_SECTION(ptr, portionN, 4);							\
	LOAD_SECTION(ptr, portionN, 5);							\
	LOAD_SECTION(ptr, portionN, 6);							\
	LOAD_SECTION(ptr, portionN, 7);

#define SUBSTITUTE_SECTION(portionN, sectionN) 						\
	V(portionN, sectionN, 0) = substitutionTable[V(portionN, sectionN, 0)];		\
	V(portionN, sectionN, 1) = substitutionTable[V(portionN, sectionN, 1)];		\
	V(portionN, sectionN, 2) = substitutionTable[V(portionN, sectionN, 2)];		\
	V(portionN, sectionN, 3) = substitutionTable[V(portionN, sectionN, 3)];		\
	V(portionN, sectionN, 4) = substitutionTable[V(portionN, sectionN, 4)];		\
	V(portionN, sectionN, 5) = substitutionTable[V(portionN, sectionN, 5)];		\
	V(portionN, sectionN, 6) = substitutionTable[V(portionN, sectionN, 6)];		\
	V(portionN, sectionN, 7) = substitutionTable[V(portionN, sectionN, 7)]

#define SUBSTITUTE_PORTION(portionN)							\
	SUBSTITUTE_SECTION(portionN, 0);						\
	SUBSTITUTE_SECTION(portionN, 1);						\
	SUBSTITUTE_SECTION(portionN, 2);						\
	SUBSTITUTE_SECTION(portionN, 3);						\
	SUBSTITUTE_SECTION(portionN, 4);						\
	SUBSTITUTE_SECTION(portionN, 5);						\
	SUBSTITUTE_SECTION(portionN, 6);						\
	SUBSTITUTE_SECTION(portionN, 7)

#define XOR_SECTION_FORWARD(portionN, sectionN)						\
	V(portionN, sectionN, 1) ^= V(portionN, sectionN, 0);				\
	V(portionN, sectionN, 2) ^= V(portionN, sectionN, 1);				\
	V(portionN, sectionN, 3) ^= V(portionN, sectionN, 2);				\
	V(portionN, sectionN, 4) ^= V(portionN, sectionN, 3);				\
	V(portionN, sectionN, 5) ^= V(portionN, sectionN, 4);				\
	V(portionN, sectionN, 6) ^= V(portionN, sectionN, 5);				\
	V(portionN, sectionN, 7) ^= V(portionN, sectionN, 6);				\
	V(portionN, sectionN, 0) ^= V(portionN, sectionN, 7)

#define XOR_SECTION_REVERSE(portionN, sectionN)						\
	V(portionN, sectionN, 0) ^= V(portionN, sectionN, 7)				\
	V(portionN, sectionN, 7) ^= V(portionN, sectionN, 6);				\
	V(portionN, sectionN, 6) ^= V(portionN, sectionN, 5);				\
	V(portionN, sectionN, 5) ^= V(portionN, sectionN, 4);				\
	V(portionN, sectionN, 4) ^= V(portionN, sectionN, 3);				\
	V(portionN, sectionN, 3) ^= V(portionN, sectionN, 2);				\
	V(portionN, sectionN, 2) ^= V(portionN, sectionN, 1);				\
	V(portionN, sectionN, 1) ^= V(portionN, sectionN, 0);

#define XOR_PORTION_FORWARD(portionN)							\
	XOR_SECTION_FORWARD(portionN, 0);						\
	XOR_SECTION_FORWARD(portionN, 1);						\
	XOR_SECTION_FORWARD(portionN, 2);						\
	XOR_SECTION_FORWARD(portionN, 3);						\
	XOR_SECTION_FORWARD(portionN, 4);						\
	XOR_SECTION_FORWARD(portionN, 5);						\
	XOR_SECTION_FORWARD(portionN, 6);						\
	XOR_SECTION_FORWARD(portionN, 7)

#define XOR_PORTION_REVERSE(portionN)							\
	XOR_SECTION_REVERSE(portionN, 0);						\
	XOR_SECTION_REVERSE(portionN, 1);						\
	XOR_SECTION_REVERSE(portionN, 2);						\
	XOR_SECTION_REVERSE(portionN, 3);						\
	XOR_SECTION_REVERSE(portionN, 4);						\
	XOR_SECTION_REVERSE(portionN, 5);						\
	XOR_SECTION_REVERSE(portionN, 6);						\
	XOR_SECTION_REVERSE(portionN, 7)

#define OLD_ROTATE_SECTION_RIGHT(portionN, sectionN)					\
	do {										\
		uint16_t addLeft, carry;						\
		addLeft = (V(portionN, sectionN, 7) & 0xFF) << 8;			\
		carry = (V(portionN, sectionN, 0) & 0xFF) << 8;				\
		(V(portionN, sectionN, 0) & 0xFF) << 8;				\
	while (0)

#define REPLACEMENT_PORTION_FORWARD(portionN)						\
	SUBSTITUTION_PORTION(portionN);							\
	XOR_PORTION_FORWARD(portionN)

#define REPLACEMENT_PORTION_FORWARD_ALL()						\
	REPLACEMENT_PORTION_FORWARD(0);							\
	REPLACEMENT_PORTION_FORWARD(1);							\
	REPLACEMENT_PORTION_FORWARD(2);							\
	REPLACEMENT_PORTION_FORWARD(3);							\
	REPLACEMENT_PORTION_FORWARD(4);							\
	REPLACEMENT_PORTION_FORWARD(5);							\
	REPLACEMENT_PORTION_FORWARD(6);							\
	REPLACEMENT_PORTION_FORWARD(7);							\
	REPLACEMENT_PORTION_FORWARD(8);							\
	REPLACEMENT_PORTION_FORWARD(9);							\
	REPLACEMENT_PORTION_FORWARD(10);						\
	REPLACEMENT_PORTION_FORWARD(11);						\
	REPLACEMENT_PORTION_FORWARD(12);						\
	REPLACEMENT_PORTION_FORWARD(13);						\
	REPLACEMENT_PORTION_FORWARD(14);						\
	REPLACEMENT_PORTION_FORWARD(15);						\
	REPLACEMENT_PORTION_FORWARD(16);						\
	REPLACEMENT_PORTION_FORWARD(17);						\
	REPLACEMENT_PORTION_FORWARD(18);						\
	REPLACEMENT_PORTION_FORWARD(19);						\
	REPLACEMENT_PORTION_FORWARD(20);						\
	REPLACEMENT_PORTION_FORWARD(21);						\
	REPLACEMENT_PORTION_FORWARD(22);						\
	REPLACEMENT_PORTION_FORWARD(23);						\
	REPLACEMENT_PORTION_FORWARD(24);						\
	REPLACEMENT_PORTION_FORWARD(25);						\
	REPLACEMENT_PORTION_FORWARD(26);						\
	REPLACEMENT_PORTION_FORWARD(27);						\
	REPLACEMENT_PORTION_FORWARD(28);						\
	REPLACEMENT_PORTION_FORWARD(29);						\
	REPLACEMENT_PORTION_FORWARD(30);						\
	REPLACEMENT_PORTION_FORWARD(31)

#define REPLACEMENT_PORTION_REVERSE(portionN)						\
	XOR_PORTION_REVERSE(portionN);							\
	SUBSTITUTION_PORTION(portionN)

#define REPLACEMENT_PORTION_REVERSE_ALL()						\
	REPLACEMENT_PORTION_REVERSE(0);							\
	REPLACEMENT_PORTION_REVERSE(1);							\
	REPLACEMENT_PORTION_REVERSE(2);							\
	REPLACEMENT_PORTION_REVERSE(3);							\
	REPLACEMENT_PORTION_REVERSE(4);							\
	REPLACEMENT_PORTION_REVERSE(5);							\
	REPLACEMENT_PORTION_REVERSE(6);							\
	REPLACEMENT_PORTION_REVERSE(7);							\
	REPLACEMENT_PORTION_REVERSE(8);							\
	REPLACEMENT_PORTION_REVERSE(9);							\
	REPLACEMENT_PORTION_REVERSE(10);						\
	REPLACEMENT_PORTION_REVERSE(11);						\
	REPLACEMENT_PORTION_REVERSE(12);						\
	REPLACEMENT_PORTION_REVERSE(13);						\
	REPLACEMENT_PORTION_REVERSE(14);						\
	REPLACEMENT_PORTION_REVERSE(15);						\
	REPLACEMENT_PORTION_REVERSE(16);						\
	REPLACEMENT_PORTION_REVERSE(17);						\
	REPLACEMENT_PORTION_REVERSE(18);						\
	REPLACEMENT_PORTION_REVERSE(19);						\
	REPLACEMENT_PORTION_REVERSE(20);						\
	REPLACEMENT_PORTION_REVERSE(21);						\
	REPLACEMENT_PORTION_REVERSE(22);						\
	REPLACEMENT_PORTION_REVERSE(23);						\
	REPLACEMENT_PORTION_REVERSE(24);						\
	REPLACEMENT_PORTION_REVERSE(25);						\
	REPLACEMENT_PORTION_REVERSE(26);						\
	REPLACEMENT_PORTION_REVERSE(27);						\
	REPLACEMENT_PORTION_REVERSE(28);						\
	REPLACEMENT_PORTION_REVERSE(29);						\
	REPLACEMENT_PORTION_REVERSE(30);						\
	REPLACEMENT_PORTION_REVERSE(31)

#define PORTION_REARRANGE(portionN)							\
	SWAP_SHORTS(V(portionN, 0, 1), V(portionN, 1, 1));				\
	SWAP_SHORTS(V(portionN, 0, 2), V(portionN, 2, 1));				\
	SWAP_SHORTS(V(portionN, 0, 3), V(portionN, 3, 1));				\
	SWAP_SHORTS(V(portionN, 0, 4), V(portionN, 4, 1));				\
	SWAP_SHORTS(V(portionN, 0, 5), V(portionN, 5, 1));				\
	SWAP_SHORTS(V(portionN, 0, 6), V(portionN, 6, 1));				\
	SWAP_SHORTS(V(portionN, 0, 7), V(portionN, 7, 1));				\
											\
	SWAP_SHORTS(V(portionN, 1, 2), V(portionN, 2, 2));				\
	SWAP_SHORTS(V(portionN, 1, 3), V(portionN, 3, 2));				\
	SWAP_SHORTS(V(portionN, 1, 4), V(portionN, 4, 2));				\
	SWAP_SHORTS(V(portionN, 1, 5), V(portionN, 5, 2));				\
	SWAP_SHORTS(V(portionN, 1, 6), V(portionN, 6, 2));				\
	SWAP_SHORTS(V(portionN, 1, 7), V(portionN, 7, 2));				\
											\
	SWAP_SHORTS(V(portionN, 2, 3), V(portionN, 3, 3));				\
	SWAP_SHORTS(V(portionN, 2, 4), V(portionN, 4, 3));				\
	SWAP_SHORTS(V(portionN, 2, 5), V(portionN, 5, 3));				\
	SWAP_SHORTS(V(portionN, 2, 6), V(portionN, 6, 3));				\
	SWAP_SHORTS(V(portionN, 2, 7), V(portionN, 7, 3));				\
											\
	SWAP_SHORTS(V(portionN, 3, 4), V(portionN, 4, 4));				\
	SWAP_SHORTS(V(portionN, 3, 5), V(portionN, 5, 4));				\
	SWAP_SHORTS(V(portionN, 3, 6), V(portionN, 6, 4));				\
	SWAP_SHORTS(V(portionN, 3, 7), V(portionN, 7, 4));				\
											\
	SWAP_SHORTS(V(portionN, 4, 5), V(portionN, 5, 5));				\
	SWAP_SHORTS(V(portionN, 4, 6), V(portionN, 6, 5));				\
	SWAP_SHORTS(V(portionN, 4, 7), V(portionN, 7, 5));				\
											\
	SWAP_SHORTS(V(portionN, 5, 6), V(portionN, 6, 6));				\
	SWAP_SHORTS(V(portionN, 5, 7), V(portionN, 7, 6));				\
											\
	SWAP_SHORTS(V(portionN, 6, 7), V(portionN, 7, 7));

#define REARRANGE_ALL_PORTIONS()							\
	PORTION_REARRANGE(0);								\
	PORTION_REARRANGE(1);								\
	PORTION_REARRANGE(2);								\
	PORTION_REARRANGE(3);								\
	PORTION_REARRANGE(4);								\
	PORTION_REARRANGE(5);								\
	PORTION_REARRANGE(6);								\
	PORTION_REARRANGE(7);								\
	PORTION_REARRANGE(8);								\
	PORTION_REARRANGE(9);								\
	PORTION_REARRANGE(10);								\
	PORTION_REARRANGE(11);								\
	PORTION_REARRANGE(12);								\
	PORTION_REARRANGE(13);								\
	PORTION_REARRANGE(14);								\
	PORTION_REARRANGE(15);								\
	PORTION_REARRANGE(16);								\
	PORTION_REARRANGE(17);								\
	PORTION_REARRANGE(18);								\
	PORTION_REARRANGE(19);								\
	PORTION_REARRANGE(20);								\
	PORTION_REARRANGE(21);								\
	PORTION_REARRANGE(22);								\
	PORTION_REARRANGE(23);								\
	PORTION_REARRANGE(24);								\
	PORTION_REARRANGE(25);								\
	PORTION_REARRANGE(26);								\
	PORTION_REARRANGE(27);								\
	PORTION_REARRANGE(28);								\
	PORTION_REARRANGE(29);								\
	PORTION_REARRANGE(30);								\
	PORTION_REARRANGE(31)

#define SUPER_REARRANGEMENT_PORTION0_HELPER(sectionN)					\
	SWAP_SHORTS(V(0, sectionN, 0), V((sectionN*4), 0, 0));				\
	SWAP_SHORTS(V(0, sectionN, 1), V((sectionN*4), 0, 1));				\
	SWAP_SHORTS(V(0, sectionN, 2), V((sectionN*4+1), 0, 0));			\
	SWAP_SHORTS(V(0, sectionN, 3), V((sectionN*4+1), 0, 1));			\
	SWAP_SHORTS(V(0, sectionN, 4), V((sectionN*4+2), 0, 0));			\
	SWAP_SHORTS(V(0, sectionN, 5), V((sectionN*4+2), 0, 1));			\
	SWAP_SHORTS(V(0, sectionN, 6), V((sectionN*4+3), 0, 0));			\
	SWAP_SHORTS(V(0, sectionN, 7), V((sectionN*4+3), 0, 1))

#define SUPER_REARRANGEMENT_PORTION0() 							\
	SWAP_SHORTS(V(0, 0, 2), V(1, 0, 0));						\
	SWAP_SHORTS(V(0, 0, 3), V(1, 0, 1));						\
	SWAP_SHORTS(V(0, 0, 4), V(2, 0, 0));						\
	SWAP_SHORTS(V(0, 0, 5), V(2, 0, 1));						\
	SWAP_SHORTS(V(0, 0, 6), V(3, 0, 0));						\
	SWAP_SHORTS(V(0, 0, 7), V(3, 0, 1));						\
											\
	SUPER_REARRANGEMENT_PORTION0_HELPER(1);						\
	SUPER_REARRANGEMENT_PORTION0_HELPER(2);						\
	SUPER_REARRANGEMENT_PORTION0_HELPER(3);						\
	SUPER_REARRANGEMENT_PORTION0_HELPER(4);						\
	SUPER_REARRANGEMENT_PORTION0_HELPER(5);						\
	SUPER_REARRANGEMENT_PORTION0_HELPER(6);						\
	SUPER_REARRANGEMENT_PORTION0_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION1_HELPER(sectionN)					\
	SWAP_SHORTS(V(1, sectionN, 0), V((sectionN*4), 0, 2));				\
	SWAP_SHORTS(V(1, sectionN, 1), V((sectionN*4), 0, 3));				\
	SWAP_SHORTS(V(1, sectionN, 2), V((sectionN*4+1), 0, 2));			\
	SWAP_SHORTS(V(1, sectionN, 3), V((sectionN*4+1), 0, 3));			\
	SWAP_SHORTS(V(1, sectionN, 4), V((sectionN*4+2), 0, 2));			\
	SWAP_SHORTS(V(1, sectionN, 5), V((sectionN*4+2), 0, 3));			\
	SWAP_SHORTS(V(1, sectionN, 6), V((sectionN*4+3), 0, 2));			\
	SWAP_SHORTS(V(1, sectionN, 7), V((sectionN*4+3), 0, 3))

#define SUPER_REARRANGEMENT_PORTION1() 							\
	SWAP_SHORTS(V(1, 0, 4), V(2, 0, 2));						\
	SWAP_SHORTS(V(1, 0, 5), V(2, 0, 3));						\
	SWAP_SHORTS(V(1, 0, 6), V(3, 0, 2));						\
	SWAP_SHORTS(V(1, 0, 7), V(3, 0, 3));						\
											\
	SUPER_REARRANGEMENT_PORTION1_HELPER(1);						\
	SUPER_REARRANGEMENT_PORTION1_HELPER(2);						\
	SUPER_REARRANGEMENT_PORTION1_HELPER(3);						\
	SUPER_REARRANGEMENT_PORTION1_HELPER(4);						\
	SUPER_REARRANGEMENT_PORTION1_HELPER(5);						\
	SUPER_REARRANGEMENT_PORTION1_HELPER(6);						\
	SUPER_REARRANGEMENT_PORTION1_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION2_HELPER(sectionN)					\
	SWAP_SHORTS(V(2, sectionN, 0), V((sectionN*4), 0, 4));				\
	SWAP_SHORTS(V(2, sectionN, 1), V((sectionN*4), 0, 5));				\
	SWAP_SHORTS(V(2, sectionN, 2), V((sectionN*4+1), 0, 4));			\
	SWAP_SHORTS(V(2, sectionN, 3), V((sectionN*4+1), 0, 5));			\
	SWAP_SHORTS(V(2, sectionN, 4), V((sectionN*4+2), 0, 4));			\
	SWAP_SHORTS(V(2, sectionN, 5), V((sectionN*4+2), 0, 5));			\
	SWAP_SHORTS(V(2, sectionN, 6), V((sectionN*4+3), 0, 4));			\
	SWAP_SHORTS(V(2, sectionN, 7), V((sectionN*4+3), 0, 5))

#define SUPER_REARRANGEMENT_PORTION2() 							\
	SWAP_SHORTS(V(2, 0, 6), V(3, 0, 4));						\
	SWAP_SHORTS(V(2, 0, 7), V(3, 0, 5));						\
											\
	SUPER_REARRANGEMENT_PORTION2_HELPER(1);						\
	SUPER_REARRANGEMENT_PORTION2_HELPER(2);						\
	SUPER_REARRANGEMENT_PORTION2_HELPER(3);						\
	SUPER_REARRANGEMENT_PORTION2_HELPER(4);						\
	SUPER_REARRANGEMENT_PORTION2_HELPER(5);						\
	SUPER_REARRANGEMENT_PORTION2_HELPER(6);						\
	SUPER_REARRANGEMENT_PORTION2_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION3_HELPER(sectionN)					\
	SWAP_SHORTS(V(3, sectionN, 0), V((sectionN*4), 0, 6));				\
	SWAP_SHORTS(V(3, sectionN, 1), V((sectionN*4), 0, 7));				\
	SWAP_SHORTS(V(3, sectionN, 2), V((sectionN*4+1), 0, 6));			\
	SWAP_SHORTS(V(3, sectionN, 3), V((sectionN*4+1), 0, 7));			\
	SWAP_SHORTS(V(3, sectionN, 4), V((sectionN*4+2), 0, 6));			\
	SWAP_SHORTS(V(3, sectionN, 5), V((sectionN*4+2), 0, 7));			\
	SWAP_SHORTS(V(3, sectionN, 6), V((sectionN*4+3), 0, 6));			\
	SWAP_SHORTS(V(3, sectionN, 7), V((sectionN*4+3), 0, 7))

#define SUPER_REARRANGEMENT_PORTION3() 							\
	SUPER_REARRANGEMENT_PORTION3_HELPER(1);						\
	SUPER_REARRANGEMENT_PORTION3_HELPER(2);						\
	SUPER_REARRANGEMENT_PORTION3_HELPER(3);						\
	SUPER_REARRANGEMENT_PORTION3_HELPER(4);						\
	SUPER_REARRANGEMENT_PORTION3_HELPER(5);						\
	SUPER_REARRANGEMENT_PORTION3_HELPER(6);						\
	SUPER_REARRANGEMENT_PORTION3_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION4_HELPER(sectionN)					\
	SWAP_SHORTS(V(4, sectionN, 0), V((sectionN*4), 1, 0));				\
	SWAP_SHORTS(V(4, sectionN, 1), V((sectionN*4), 1, 1));				\
	SWAP_SHORTS(V(4, sectionN, 2), V((sectionN*4+1), 1, 0));			\
	SWAP_SHORTS(V(4, sectionN, 3), V((sectionN*4+1), 1, 1));			\
	SWAP_SHORTS(V(4, sectionN, 4), V((sectionN*4+2), 1, 0));			\
	SWAP_SHORTS(V(4, sectionN, 5), V((sectionN*4+2), 1, 1));			\
	SWAP_SHORTS(V(4, sectionN, 6), V((sectionN*4+3), 1, 0));			\
	SWAP_SHORTS(V(4, sectionN, 7), V((sectionN*4+3), 1, 1))

#define SUPER_REARRANGEMENT_PORTION4() 							\
	SWAP_SHORTS(V(4, 1, 2)) = SWAP_SHORT(V(5, 1, 0)); 				\
	SWAP_SHORTS(V(4, 1, 3)) = SWAP_SHORT(V(5, 1, 1)); 				\
	SWAP_SHORTS(V(4, 1, 4)) = SWAP_SHORT(V(6, 1, 0)); 				\
	SWAP_SHORTS(V(4, 1, 5)) = SWAP_SHORT(V(6, 1, 1)); 				\
	SWAP_SHORTS(V(4, 1, 6)) = SWAP_SHORT(V(7, 1, 0)); 				\
	SWAP_SHORTS(V(4, 1, 7)) = SWAP_SHORT(V(7, 1, 1)); 				\
											\
	SUPER_REARRANGEMENT_PORTION4_HELPER(2);						\
	SUPER_REARRANGEMENT_PORTION4_HELPER(3);						\
	SUPER_REARRANGEMENT_PORTION4_HELPER(4);						\
	SUPER_REARRANGEMENT_PORTION4_HELPER(5);						\
	SUPER_REARRANGEMENT_PORTION4_HELPER(6);						\
	SUPER_REARRANGEMENT_PORTION4_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION5_HELPER(sectionN)					\
	SWAP_SHORTS(V(5, sectionN, 0), V((sectionN*4), 1, 2));				\
	SWAP_SHORTS(V(5, sectionN, 1), V((sectionN*4), 1, 3));				\
	SWAP_SHORTS(V(5, sectionN, 2), V((sectionN*4+1), 1, 2));			\
	SWAP_SHORTS(V(5, sectionN, 3), V((sectionN*4+1), 1, 3));			\
	SWAP_SHORTS(V(5, sectionN, 4), V((sectionN*4+2), 1, 2));			\
	SWAP_SHORTS(V(5, sectionN, 5), V((sectionN*4+2), 1, 3));			\
	SWAP_SHORTS(V(5, sectionN, 6), V((sectionN*4+3), 1, 2));			\
	SWAP_SHORTS(V(5, sectionN, 7), V((sectionN*4+3), 1, 3))

#define SUPER_REARRANGEMENT_PORTION5() 							\
	SWAP_SHORTS(V(5, 1, 4)) = SWAP_SHORT(V(6, 1, 2)); 				\
	SWAP_SHORTS(V(5, 1, 5)) = SWAP_SHORT(V(6, 1, 3)); 				\
	SWAP_SHORTS(V(5, 1, 6)) = SWAP_SHORT(V(7, 1, 2)); 				\
	SWAP_SHORTS(V(5, 1, 7)) = SWAP_SHORT(V(7, 1, 3)); 				\
											\
	SUPER_REARRANGEMENT_PORTION5_HELPER(2);						\
	SUPER_REARRANGEMENT_PORTION5_HELPER(3);						\
	SUPER_REARRANGEMENT_PORTION5_HELPER(4);						\
	SUPER_REARRANGEMENT_PORTION5_HELPER(5);						\
	SUPER_REARRANGEMENT_PORTION5_HELPER(6);						\
	SUPER_REARRANGEMENT_PORTION5_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION6_HELPER(sectionN)					\
	SWAP_SHORTS(V(6, sectionN, 0), V((sectionN*4), 1, 4));				\
	SWAP_SHORTS(V(6, sectionN, 1), V((sectionN*4), 1, 5));				\
	SWAP_SHORTS(V(6, sectionN, 2), V((sectionN*4+1), 1, 4));			\
	SWAP_SHORTS(V(6, sectionN, 3), V((sectionN*4+1), 1, 5));			\
	SWAP_SHORTS(V(6, sectionN, 4), V((sectionN*4+2), 1, 4));			\
	SWAP_SHORTS(V(6, sectionN, 5), V((sectionN*4+2), 1, 5));			\
	SWAP_SHORTS(V(6, sectionN, 6), V((sectionN*4+3), 1, 4));			\
	SWAP_SHORTS(V(6, sectionN, 7), V((sectionN*4+3), 1, 5))

#define SUPER_REARRANGEMENT_PORTION6() 							\
	SWAP_SHORTS(V(6, 1, 6)) = SWAP_SHORT(V(7, 1, 4)); 				\
	SWAP_SHORTS(V(6, 1, 7)) = SWAP_SHORT(V(7, 1, 5)); 				\
											\
	SUPER_REARRANGEMENT_PORTION6_HELPER(2);						\
	SUPER_REARRANGEMENT_PORTION6_HELPER(3);						\
	SUPER_REARRANGEMENT_PORTION6_HELPER(4);						\
	SUPER_REARRANGEMENT_PORTION6_HELPER(5);						\
	SUPER_REARRANGEMENT_PORTION6_HELPER(6);						\
	SUPER_REARRANGEMENT_PORTION6_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION7_HELPER(sectionN)					\
	SWAP_SHORTS(V(7, sectionN, 0), V((sectionN*4), 1, 6));				\
	SWAP_SHORTS(V(7, sectionN, 1), V((sectionN*4), 1, 7));				\
	SWAP_SHORTS(V(7, sectionN, 2), V((sectionN*4+1), 1, 6));			\
	SWAP_SHORTS(V(7, sectionN, 3), V((sectionN*4+1), 1, 7));			\
	SWAP_SHORTS(V(7, sectionN, 4), V((sectionN*4+2), 1, 6));			\
	SWAP_SHORTS(V(7, sectionN, 5), V((sectionN*4+2), 1, 7));			\
	SWAP_SHORTS(V(7, sectionN, 6), V((sectionN*4+3), 1, 6));			\
	SWAP_SHORTS(V(7, sectionN, 7), V((sectionN*4+3), 1, 7))

#define SUPER_REARRANGEMENT_PORTION7() 							\
	SUPER_REARRANGEMENT_PORTION7_HELPER(2);						\
	SUPER_REARRANGEMENT_PORTION7_HELPER(3);						\
	SUPER_REARRANGEMENT_PORTION7_HELPER(4);						\
	SUPER_REARRANGEMENT_PORTION7_HELPER(5);						\
	SUPER_REARRANGEMENT_PORTION7_HELPER(6);						\
	SUPER_REARRANGEMENT_PORTION7_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION8_HELPER(sectionN)					\
	SWAP_SHORTS(V(8, sectionN, 0), V((sectionN*4), 2, 0));				\
	SWAP_SHORTS(V(8, sectionN, 1), V((sectionN*4), 2, 1));				\
	SWAP_SHORTS(V(8, sectionN, 2), V((sectionN*4+1), 2, 0));			\
	SWAP_SHORTS(V(8, sectionN, 3), V((sectionN*4+1), 2, 1));			\
	SWAP_SHORTS(V(8, sectionN, 4), V((sectionN*4+2), 2, 0));			\
	SWAP_SHORTS(V(8, sectionN, 5), V((sectionN*4+2), 2, 1));			\
	SWAP_SHORTS(V(8, sectionN, 6), V((sectionN*4+3), 2, 0));			\
	SWAP_SHORTS(V(8, sectionN, 7), V((sectionN*4+3), 2, 1))

#define SUPER_REARRANGEMENT_PORTION8() 							\
	SWAP_SHORTS(V(8, 2, 2)) = SWAP_SHORT(V(9, 2, 0)); 				\
	SWAP_SHORTS(V(8, 2, 3)) = SWAP_SHORT(V(9, 2, 1)); 				\
	SWAP_SHORTS(V(8, 2, 4)) = SWAP_SHORT(V(10, 2, 0)); 				\
	SWAP_SHORTS(V(8, 2, 5)) = SWAP_SHORT(V(10, 2, 1)); 				\
	SWAP_SHORTS(V(8, 2, 6)) = SWAP_SHORT(V(11, 2, 0)); 				\
	SWAP_SHORTS(V(8, 2, 7)) = SWAP_SHORT(V(11, 2, 1)); 				\
											\
	SUPER_REARRANGEMENT_PORTION8_HELPER(3);						\
	SUPER_REARRANGEMENT_PORTION8_HELPER(4);						\
	SUPER_REARRANGEMENT_PORTION8_HELPER(5);						\
	SUPER_REARRANGEMENT_PORTION8_HELPER(6);						\
	SUPER_REARRANGEMENT_PORTION8_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION9_HELPER(sectionN)					\
	SWAP_SHORTS(V(9, sectionN, 0), V((sectionN*4), 2, 2));				\
	SWAP_SHORTS(V(9, sectionN, 1), V((sectionN*4), 2, 3));				\
	SWAP_SHORTS(V(9, sectionN, 2), V((sectionN*4+1), 2, 2));			\
	SWAP_SHORTS(V(9, sectionN, 3), V((sectionN*4+1), 2, 3));			\
	SWAP_SHORTS(V(9, sectionN, 4), V((sectionN*4+2), 2, 2));			\
	SWAP_SHORTS(V(9, sectionN, 5), V((sectionN*4+2), 2, 3));			\
	SWAP_SHORTS(V(9, sectionN, 6), V((sectionN*4+3), 2, 2));			\
	SWAP_SHORTS(V(9, sectionN, 7), V((sectionN*4+3), 2, 3))

#define SUPER_REARRANGEMENT_PORTION9() 							\
	SWAP_SHORTS(V(9, 2, 4)) = SWAP_SHORT(V(10, 2, 2)); 				\
	SWAP_SHORTS(V(9, 2, 5)) = SWAP_SHORT(V(10, 2, 3)); 				\
	SWAP_SHORTS(V(9, 2, 6)) = SWAP_SHORT(V(11, 2, 2)); 				\
	SWAP_SHORTS(V(9, 2, 7)) = SWAP_SHORT(V(11, 2, 3)); 				\
											\
	SUPER_REARRANGEMENT_PORTION9_HELPER(3);						\
	SUPER_REARRANGEMENT_PORTION9_HELPER(4);						\
	SUPER_REARRANGEMENT_PORTION9_HELPER(5);						\
	SUPER_REARRANGEMENT_PORTION9_HELPER(6);						\
	SUPER_REARRANGEMENT_PORTION9_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION10_HELPER(sectionN)					\
	SWAP_SHORTS(V(10, sectionN, 0), V((sectionN*4), 2, 4));				\
	SWAP_SHORTS(V(10, sectionN, 1), V((sectionN*4), 2, 5));				\
	SWAP_SHORTS(V(10, sectionN, 2), V((sectionN*4+1), 2, 4));			\
	SWAP_SHORTS(V(10, sectionN, 3), V((sectionN*4+1), 2, 5));			\
	SWAP_SHORTS(V(10, sectionN, 4), V((sectionN*4+2), 2, 4));			\
	SWAP_SHORTS(V(10, sectionN, 5), V((sectionN*4+2), 2, 5));			\
	SWAP_SHORTS(V(10, sectionN, 6), V((sectionN*4+3), 2, 4));			\
	SWAP_SHORTS(V(10, sectionN, 7), V((sectionN*4+3), 2, 5))

#define SUPER_REARRANGEMENT_PORTION10() 						\
	SWAP_SHORTS(V(10, 2, 6)) = SWAP_SHORT(V(11, 2, 4)); 				\
	SWAP_SHORTS(V(10, 2, 7)) = SWAP_SHORT(V(11, 2, 5)); 				\
											\
	SUPER_REARRANGEMENT_PORTION10_HELPER(3);					\
	SUPER_REARRANGEMENT_PORTION10_HELPER(4);					\
	SUPER_REARRANGEMENT_PORTION10_HELPER(5);					\
	SUPER_REARRANGEMENT_PORTION10_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION10_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION11_HELPER(sectionN)					\
	SWAP_SHORTS(V(11, sectionN, 0), V((sectionN*4), 2, 6));				\
	SWAP_SHORTS(V(11, sectionN, 1), V((sectionN*4), 2, 7));				\
	SWAP_SHORTS(V(11, sectionN, 2), V((sectionN*4+1), 2, 6));			\
	SWAP_SHORTS(V(11, sectionN, 3), V((sectionN*4+1), 2, 7));			\
	SWAP_SHORTS(V(11, sectionN, 4), V((sectionN*4+2), 2, 6));			\
	SWAP_SHORTS(V(11, sectionN, 5), V((sectionN*4+2), 2, 7));			\
	SWAP_SHORTS(V(11, sectionN, 6), V((sectionN*4+3), 2, 6));			\
	SWAP_SHORTS(V(11, sectionN, 7), V((sectionN*4+3), 2, 7))

#define SUPER_REARRANGEMENT_PORTION11() 						\
	SUPER_REARRANGEMENT_PORTION11_HELPER(3);					\
	SUPER_REARRANGEMENT_PORTION11_HELPER(4);					\
	SUPER_REARRANGEMENT_PORTION11_HELPER(5);					\
	SUPER_REARRANGEMENT_PORTION11_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION11_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION12_HELPER(sectionN)					\
	SWAP_SHORTS(V(12, sectionN, 0), V((sectionN*4), 3, 0));				\
	SWAP_SHORTS(V(12, sectionN, 1), V((sectionN*4), 3, 1));				\
	SWAP_SHORTS(V(12, sectionN, 2), V((sectionN*4+1), 3, 0));			\
	SWAP_SHORTS(V(12, sectionN, 3), V((sectionN*4+1), 3, 1));			\
	SWAP_SHORTS(V(12, sectionN, 4), V((sectionN*4+2), 3, 0));			\
	SWAP_SHORTS(V(12, sectionN, 5), V((sectionN*4+2), 3, 1));			\
	SWAP_SHORTS(V(12, sectionN, 6), V((sectionN*4+3), 3, 0));			\
	SWAP_SHORTS(V(12, sectionN, 7), V((sectionN*4+3), 3, 1))

#define SUPER_REARRANGEMENT_PORTION12() 						\
	SWAP_SHORTS(V(12, 3, 2)) = SWAP_SHORT(V(13, 3, 0)); 				\
	SWAP_SHORTS(V(12, 3, 3)) = SWAP_SHORT(V(13, 3, 1)); 				\
	SWAP_SHORTS(V(12, 3, 4)) = SWAP_SHORT(V(14, 3, 0)); 				\
	SWAP_SHORTS(V(12, 3, 5)) = SWAP_SHORT(V(14, 3, 1)); 				\
	SWAP_SHORTS(V(12, 3, 6)) = SWAP_SHORT(V(15, 3, 0)); 				\
	SWAP_SHORTS(V(12, 3, 7)) = SWAP_SHORT(V(15, 3, 1)); 				\
											\
	SUPER_REARRANGEMENT_PORTION12_HELPER(4);					\
	SUPER_REARRANGEMENT_PORTION12_HELPER(5);					\
	SUPER_REARRANGEMENT_PORTION12_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION12_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION13_HELPER(sectionN)					\
	SWAP_SHORTS(V(13, sectionN, 0), V((sectionN*4), 3, 2));				\
	SWAP_SHORTS(V(13, sectionN, 1), V((sectionN*4), 3, 3));				\
	SWAP_SHORTS(V(13, sectionN, 2), V((sectionN*4+1), 3, 2));			\
	SWAP_SHORTS(V(13, sectionN, 3), V((sectionN*4+1), 3, 3));			\
	SWAP_SHORTS(V(13, sectionN, 4), V((sectionN*4+2), 3, 2));			\
	SWAP_SHORTS(V(13, sectionN, 5), V((sectionN*4+2), 3, 3));			\
	SWAP_SHORTS(V(13, sectionN, 6), V((sectionN*4+3), 3, 2));			\
	SWAP_SHORTS(V(13, sectionN, 7), V((sectionN*4+3), 3, 3))

#define SUPER_REARRANGEMENT_PORTION13() 						\
	SWAP_SHORTS(V(13, 3, 4)) = SWAP_SHORT(V(14, 3, 2)); 				\
	SWAP_SHORTS(V(13, 3, 5)) = SWAP_SHORT(V(14, 3, 3)); 				\
	SWAP_SHORTS(V(13, 3, 6)) = SWAP_SHORT(V(15, 3, 2)); 				\
	SWAP_SHORTS(V(13, 3, 7)) = SWAP_SHORT(V(15, 3, 3)); 				\
											\
	SUPER_REARRANGEMENT_PORTION13_HELPER(4);					\
	SUPER_REARRANGEMENT_PORTION13_HELPER(5);					\
	SUPER_REARRANGEMENT_PORTION13_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION13_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION14_HELPER(sectionN)					\
	SWAP_SHORTS(V(14, sectionN, 0), V((sectionN*4), 3, 4));				\
	SWAP_SHORTS(V(14, sectionN, 1), V((sectionN*4), 3, 5));				\
	SWAP_SHORTS(V(14, sectionN, 2), V((sectionN*4+1), 3, 4));			\
	SWAP_SHORTS(V(14, sectionN, 3), V((sectionN*4+1), 3, 5));			\
	SWAP_SHORTS(V(14, sectionN, 4), V((sectionN*4+2), 3, 4));			\
	SWAP_SHORTS(V(14, sectionN, 5), V((sectionN*4+2), 3, 5));			\
	SWAP_SHORTS(V(14, sectionN, 6), V((sectionN*4+3), 3, 4));			\
	SWAP_SHORTS(V(14, sectionN, 7), V((sectionN*4+3), 3, 5))

#define SUPER_REARRANGEMENT_PORTION14() 						\
	SWAP_SHORTS(V(14, 3, 6)) = SWAP_SHORT(V(15, 3, 4)); 				\
	SWAP_SHORTS(V(14, 3, 7)) = SWAP_SHORT(V(15, 3, 5)); 				\
											\
	SUPER_REARRANGEMENT_PORTION14_HELPER(4);					\
	SUPER_REARRANGEMENT_PORTION14_HELPER(5);					\
	SUPER_REARRANGEMENT_PORTION14_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION14_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION15_HELPER(sectionN)					\
	SWAP_SHORTS(V(15, sectionN, 0), V((sectionN*4), 3, 6));				\
	SWAP_SHORTS(V(15, sectionN, 1), V((sectionN*4), 3, 7));				\
	SWAP_SHORTS(V(15, sectionN, 2), V((sectionN*4+1), 3, 6));			\
	SWAP_SHORTS(V(15, sectionN, 3), V((sectionN*4+1), 3, 7));			\
	SWAP_SHORTS(V(15, sectionN, 4), V((sectionN*4+2), 3, 6));			\
	SWAP_SHORTS(V(15, sectionN, 5), V((sectionN*4+2), 3, 7));			\
	SWAP_SHORTS(V(15, sectionN, 6), V((sectionN*4+3), 3, 6));			\
	SWAP_SHORTS(V(15, sectionN, 7), V((sectionN*4+3), 3, 7))

#define SUPER_REARRANGEMENT_PORTION15() 						\
	SUPER_REARRANGEMENT_PORTION15_HELPER(4);					\
	SUPER_REARRANGEMENT_PORTION15_HELPER(5);					\
	SUPER_REARRANGEMENT_PORTION15_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION15_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION16_HELPER(sectionN)					\
	SWAP_SHORTS(V(16, sectionN, 0), V((sectionN*4), 4, 0));				\
	SWAP_SHORTS(V(16, sectionN, 1), V((sectionN*4), 4, 1));				\
	SWAP_SHORTS(V(16, sectionN, 2), V((sectionN*4+1), 4, 0));			\
	SWAP_SHORTS(V(16, sectionN, 3), V((sectionN*4+1), 4, 1));			\
	SWAP_SHORTS(V(16, sectionN, 4), V((sectionN*4+2), 4, 0));			\
	SWAP_SHORTS(V(16, sectionN, 5), V((sectionN*4+2), 4, 1));			\
	SWAP_SHORTS(V(16, sectionN, 6), V((sectionN*4+3), 4, 0));			\
	SWAP_SHORTS(V(16, sectionN, 7), V((sectionN*4+3), 4, 1))

#define SUPER_REARRANGEMENT_PORTION16() 						\
	SWAP_SHORTS(V(16, 4, 2)) = SWAP_SHORT(V(17, 4, 0)); 				\
	SWAP_SHORTS(V(16, 4, 3)) = SWAP_SHORT(V(17, 4, 1)); 				\
	SWAP_SHORTS(V(16, 4, 4)) = SWAP_SHORT(V(18, 4, 0)); 				\
	SWAP_SHORTS(V(16, 4, 5)) = SWAP_SHORT(V(18, 4, 1)); 				\
	SWAP_SHORTS(V(16, 4, 6)) = SWAP_SHORT(V(19, 4, 0)); 				\
	SWAP_SHORTS(V(16, 4, 7)) = SWAP_SHORT(V(19, 4, 1)); 				\
											\
	SUPER_REARRANGEMENT_PORTION16_HELPER(5);					\
	SUPER_REARRANGEMENT_PORTION16_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION16_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION17_HELPER(sectionN)					\
	SWAP_SHORTS(V(17, sectionN, 0), V((sectionN*4), 4, 2));				\
	SWAP_SHORTS(V(17, sectionN, 1), V((sectionN*4), 4, 3));				\
	SWAP_SHORTS(V(17, sectionN, 2), V((sectionN*4+1), 4, 2));			\
	SWAP_SHORTS(V(17, sectionN, 3), V((sectionN*4+1), 4, 3));			\
	SWAP_SHORTS(V(17, sectionN, 4), V((sectionN*4+2), 4, 2));			\
	SWAP_SHORTS(V(17, sectionN, 5), V((sectionN*4+2), 4, 3));			\
	SWAP_SHORTS(V(17, sectionN, 6), V((sectionN*4+3), 4, 2));			\
	SWAP_SHORTS(V(17, sectionN, 7), V((sectionN*4+3), 4, 3))

#define SUPER_REARRANGEMENT_PORTION17() 						\
	SWAP_SHORTS(V(17, 4, 4)) = SWAP_SHORT(V(18, 4, 2)); 				\
	SWAP_SHORTS(V(17, 4, 5)) = SWAP_SHORT(V(18, 4, 3)); 				\
	SWAP_SHORTS(V(17, 4, 6)) = SWAP_SHORT(V(19, 4, 2)); 				\
	SWAP_SHORTS(V(17, 4, 7)) = SWAP_SHORT(V(19, 4, 3)); 				\
											\
	SUPER_REARRANGEMENT_PORTION17_HELPER(5);					\
	SUPER_REARRANGEMENT_PORTION17_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION17_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION18_HELPER(sectionN)					\
	SWAP_SHORTS(V(18, sectionN, 0), V((sectionN*4), 4, 4));				\
	SWAP_SHORTS(V(18, sectionN, 1), V((sectionN*4), 4, 5));				\
	SWAP_SHORTS(V(18, sectionN, 2), V((sectionN*4+1), 4, 4));			\
	SWAP_SHORTS(V(18, sectionN, 3), V((sectionN*4+1), 4, 5));			\
	SWAP_SHORTS(V(18, sectionN, 4), V((sectionN*4+2), 4, 4));			\
	SWAP_SHORTS(V(18, sectionN, 5), V((sectionN*4+2), 4, 5));			\
	SWAP_SHORTS(V(18, sectionN, 6), V((sectionN*4+3), 4, 4));			\
	SWAP_SHORTS(V(18, sectionN, 7), V((sectionN*4+3), 4, 5))

#define SUPER_REARRANGEMENT_PORTION18() 						\
	SWAP_SHORTS(V(18, 4, 6)) = SWAP_SHORT(V(19, 4, 4)); 				\
	SWAP_SHORTS(V(18, 4, 7)) = SWAP_SHORT(V(19, 4, 5)); 				\
											\
	SUPER_REARRANGEMENT_PORTION18_HELPER(5);					\
	SUPER_REARRANGEMENT_PORTION18_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION18_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION19_HELPER(sectionN)					\
	SWAP_SHORTS(V(19, sectionN, 0), V((sectionN*4), 4, 6));				\
	SWAP_SHORTS(V(19, sectionN, 1), V((sectionN*4), 4, 7));				\
	SWAP_SHORTS(V(19, sectionN, 2), V((sectionN*4+1), 4, 6));			\
	SWAP_SHORTS(V(19, sectionN, 3), V((sectionN*4+1), 4, 7));			\
	SWAP_SHORTS(V(19, sectionN, 4), V((sectionN*4+2), 4, 6));			\
	SWAP_SHORTS(V(19, sectionN, 5), V((sectionN*4+2), 4, 7));			\
	SWAP_SHORTS(V(19, sectionN, 6), V((sectionN*4+3), 4, 6));			\
	SWAP_SHORTS(V(19, sectionN, 7), V((sectionN*4+3), 4, 7))

#define SUPER_REARRANGEMENT_PORTION19() 						\
	SUPER_REARRANGEMENT_PORTION19_HELPER(5);					\
	SUPER_REARRANGEMENT_PORTION19_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION19_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION20_HELPER(sectionN)					\
	SWAP_SHORTS(V(20, sectionN, 0), V((sectionN*4), 5, 0));				\
	SWAP_SHORTS(V(20, sectionN, 1), V((sectionN*4), 5, 1));				\
	SWAP_SHORTS(V(20, sectionN, 2), V((sectionN*4+1), 5, 0));			\
	SWAP_SHORTS(V(20, sectionN, 3), V((sectionN*4+1), 5, 1));			\
	SWAP_SHORTS(V(20, sectionN, 4), V((sectionN*4+2), 5, 0));			\
	SWAP_SHORTS(V(20, sectionN, 5), V((sectionN*4+2), 5, 1));			\
	SWAP_SHORTS(V(20, sectionN, 6), V((sectionN*4+3), 5, 0));			\
	SWAP_SHORTS(V(20, sectionN, 7), V((sectionN*4+3), 5, 1))

#define SUPER_REARRANGEMENT_PORTION20() 						\
	SWAP_SHORTS(V(20, 5, 2)) = SWAP_SHORT(V(21, 5, 0)); 				\
	SWAP_SHORTS(V(20, 5, 3)) = SWAP_SHORT(V(21, 5, 1)); 				\
	SWAP_SHORTS(V(20, 5, 4)) = SWAP_SHORT(V(22, 5, 0)); 				\
	SWAP_SHORTS(V(20, 5, 5)) = SWAP_SHORT(V(22, 5, 1)); 				\
	SWAP_SHORTS(V(20, 5, 6)) = SWAP_SHORT(V(23, 5, 0)); 				\
	SWAP_SHORTS(V(20, 5, 7)) = SWAP_SHORT(V(23, 5, 1)); 				\
											\
	SUPER_REARRANGEMENT_PORTION20_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION20_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION21_HELPER(sectionN)					\
	SWAP_SHORTS(V(21, sectionN, 0), V((sectionN*4), 5, 2));				\
	SWAP_SHORTS(V(21, sectionN, 1), V((sectionN*4), 5, 3));				\
	SWAP_SHORTS(V(21, sectionN, 2), V((sectionN*4+1), 5, 2));			\
	SWAP_SHORTS(V(21, sectionN, 3), V((sectionN*4+1), 5, 3));			\
	SWAP_SHORTS(V(21, sectionN, 4), V((sectionN*4+2), 5, 2));			\
	SWAP_SHORTS(V(21, sectionN, 5), V((sectionN*4+2), 5, 3));			\
	SWAP_SHORTS(V(21, sectionN, 6), V((sectionN*4+3), 5, 2));			\
	SWAP_SHORTS(V(21, sectionN, 7), V((sectionN*4+3), 5, 3))

#define SUPER_REARRANGEMENT_PORTION21() 						\
	SWAP_SHORTS(V(21, 5, 4)) = SWAP_SHORT(V(22, 5, 2)); 				\
	SWAP_SHORTS(V(21, 5, 5)) = SWAP_SHORT(V(22, 5, 3)); 				\
	SWAP_SHORTS(V(21, 5, 6)) = SWAP_SHORT(V(23, 5, 2)); 				\
	SWAP_SHORTS(V(21, 5, 7)) = SWAP_SHORT(V(23, 5, 3)); 				\
											\
	SUPER_REARRANGEMENT_PORTION21_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION21_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION22_HELPER(sectionN)					\
	SWAP_SHORTS(V(22, sectionN, 0), V((sectionN*4), 5, 4));				\
	SWAP_SHORTS(V(22, sectionN, 1), V((sectionN*4), 5, 5));				\
	SWAP_SHORTS(V(22, sectionN, 2), V((sectionN*4+1), 5, 4));			\
	SWAP_SHORTS(V(22, sectionN, 3), V((sectionN*4+1), 5, 5));			\
	SWAP_SHORTS(V(22, sectionN, 4), V((sectionN*4+2), 5, 4));			\
	SWAP_SHORTS(V(22, sectionN, 5), V((sectionN*4+2), 5, 5));			\
	SWAP_SHORTS(V(22, sectionN, 6), V((sectionN*4+3), 5, 4));			\
	SWAP_SHORTS(V(22, sectionN, 7), V((sectionN*4+3), 5, 5))

#define SUPER_REARRANGEMENT_PORTION22() 						\
	SWAP_SHORTS(V(22, 5, 6)) = SWAP_SHORT(V(23, 5, 4)); 				\
	SWAP_SHORTS(V(22, 5, 7)) = SWAP_SHORT(V(23, 5, 5)); 				\
											\
	SUPER_REARRANGEMENT_PORTION22_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION22_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION23_HELPER(sectionN)					\
	SWAP_SHORTS(V(23, sectionN, 0), V((sectionN*4), 5, 6));				\
	SWAP_SHORTS(V(23, sectionN, 1), V((sectionN*4), 5, 7));				\
	SWAP_SHORTS(V(23, sectionN, 2), V((sectionN*4+1), 5, 6));			\
	SWAP_SHORTS(V(23, sectionN, 3), V((sectionN*4+1), 5, 7));			\
	SWAP_SHORTS(V(23, sectionN, 4), V((sectionN*4+2), 5, 6));			\
	SWAP_SHORTS(V(23, sectionN, 5), V((sectionN*4+2), 5, 7));			\
	SWAP_SHORTS(V(23, sectionN, 6), V((sectionN*4+3), 5, 6));			\
	SWAP_SHORTS(V(23, sectionN, 7), V((sectionN*4+3), 5, 7))

#define SUPER_REARRANGEMENT_PORTION23() 						\
	SUPER_REARRANGEMENT_PORTION23_HELPER(6);					\
	SUPER_REARRANGEMENT_PORTION23_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION24_HELPER(sectionN)					\
	SWAP_SHORTS(V(24, sectionN, 0), V((sectionN*4), 6, 0));				\
	SWAP_SHORTS(V(24, sectionN, 1), V((sectionN*4), 6, 1));				\
	SWAP_SHORTS(V(24, sectionN, 2), V((sectionN*4+1), 6, 0));			\
	SWAP_SHORTS(V(24, sectionN, 3), V((sectionN*4+1), 6, 1));			\
	SWAP_SHORTS(V(24, sectionN, 4), V((sectionN*4+2), 6, 0));			\
	SWAP_SHORTS(V(24, sectionN, 5), V((sectionN*4+2), 6, 1));			\
	SWAP_SHORTS(V(24, sectionN, 6), V((sectionN*4+3), 6, 0));			\
	SWAP_SHORTS(V(24, sectionN, 7), V((sectionN*4+3), 6, 1))

#define SUPER_REARRANGEMENT_PORTION24() 						\
	SWAP_SHORTS(V(24, 6, 2)) = SWAP_SHORT(V(25, 6, 0)); 				\
	SWAP_SHORTS(V(24, 6, 3)) = SWAP_SHORT(V(25, 6, 1)); 				\
	SWAP_SHORTS(V(24, 6, 4)) = SWAP_SHORT(V(26, 6, 0)); 				\
	SWAP_SHORTS(V(24, 6, 5)) = SWAP_SHORT(V(26, 6, 1)); 				\
	SWAP_SHORTS(V(24, 6, 6)) = SWAP_SHORT(V(27, 6, 0)); 				\
	SWAP_SHORTS(V(24, 6, 7)) = SWAP_SHORT(V(27, 6, 1)); 				\
											\
	SUPER_REARRANGEMENT_PORTION24_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION25_HELPER(sectionN)					\
	SWAP_SHORTS(V(25, sectionN, 0), V((sectionN*4), 6, 2));				\
	SWAP_SHORTS(V(25, sectionN, 1), V((sectionN*4), 6, 3));				\
	SWAP_SHORTS(V(25, sectionN, 2), V((sectionN*4+1), 6, 2));			\
	SWAP_SHORTS(V(25, sectionN, 3), V((sectionN*4+1), 6, 3));			\
	SWAP_SHORTS(V(25, sectionN, 4), V((sectionN*4+2), 6, 2));			\
	SWAP_SHORTS(V(25, sectionN, 5), V((sectionN*4+2), 6, 3));			\
	SWAP_SHORTS(V(25, sectionN, 6), V((sectionN*4+3), 6, 2));			\
	SWAP_SHORTS(V(25, sectionN, 7), V((sectionN*4+3), 6, 3))

#define SUPER_REARRANGEMENT_PORTION25() 						\
	SWAP_SHORTS(V(25, 6, 4)) = SWAP_SHORT(V(26, 6, 2)); 				\
	SWAP_SHORTS(V(25, 6, 5)) = SWAP_SHORT(V(26, 6, 3)); 				\
	SWAP_SHORTS(V(25, 6, 6)) = SWAP_SHORT(V(27, 6, 2)); 				\
	SWAP_SHORTS(V(25, 6, 7)) = SWAP_SHORT(V(27, 6, 3)); 				\
											\
	SUPER_REARRANGEMENT_PORTION25_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION26_HELPER(sectionN)					\
	SWAP_SHORTS(V(26, sectionN, 0), V((sectionN*4), 6, 4));				\
	SWAP_SHORTS(V(26, sectionN, 1), V((sectionN*4), 6, 5));				\
	SWAP_SHORTS(V(26, sectionN, 2), V((sectionN*4+1), 6, 4));			\
	SWAP_SHORTS(V(26, sectionN, 3), V((sectionN*4+1), 6, 5));			\
	SWAP_SHORTS(V(26, sectionN, 4), V((sectionN*4+2), 6, 4));			\
	SWAP_SHORTS(V(26, sectionN, 5), V((sectionN*4+2), 6, 5));			\
	SWAP_SHORTS(V(26, sectionN, 6), V((sectionN*4+3), 6, 4));			\
	SWAP_SHORTS(V(26, sectionN, 7), V((sectionN*4+3), 6, 5))

#define SUPER_REARRANGEMENT_PORTION26() 						\
	SWAP_SHORTS(V(26, 6, 6)) = SWAP_SHORT(V(27, 6, 4)); 				\
	SWAP_SHORTS(V(26, 6, 7)) = SWAP_SHORT(V(27, 6, 5)); 				\
											\
	SUPER_REARRANGEMENT_PORTION26_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION27_HELPER(sectionN)					\
	SWAP_SHORTS(V(27, sectionN, 0), V((sectionN*4), 6, 6));				\
	SWAP_SHORTS(V(27, sectionN, 1), V((sectionN*4), 6, 7));				\
	SWAP_SHORTS(V(27, sectionN, 2), V((sectionN*4+1), 6, 6));			\
	SWAP_SHORTS(V(27, sectionN, 3), V((sectionN*4+1), 6, 7));			\
	SWAP_SHORTS(V(27, sectionN, 4), V((sectionN*4+2), 6, 6));			\
	SWAP_SHORTS(V(27, sectionN, 5), V((sectionN*4+2), 6, 7));			\
	SWAP_SHORTS(V(27, sectionN, 6), V((sectionN*4+3), 6, 6));			\
	SWAP_SHORTS(V(27, sectionN, 7), V((sectionN*4+3), 6, 7))

#define SUPER_REARRANGEMENT_PORTION27() 						\
	SUPER_REARRANGEMENT_PORTION27_HELPER(7)

#define SUPER_REARRANGEMENT_PORTION28() 						\
	SWAP_SHORTS(V(28, 7, 2)) = SWAP_SHORT(V(29, 7, 0)); 				\
	SWAP_SHORTS(V(28, 7, 3)) = SWAP_SHORT(V(29, 7, 1)); 				\
	SWAP_SHORTS(V(28, 7, 4)) = SWAP_SHORT(V(30, 7, 0)); 				\
	SWAP_SHORTS(V(28, 7, 5)) = SWAP_SHORT(V(30, 7, 1)); 				\
	SWAP_SHORTS(V(28, 7, 6)) = SWAP_SHORT(V(31, 7, 0)); 				\
	SWAP_SHORTS(V(28, 7, 7)) = SWAP_SHORT(V(31, 7, 1));

#define SUPER_REARRANGEMENT_PORTION29() 						\
	SWAP_SHORTS(V(29, 7, 4)) = SWAP_SHORT(V(30, 7, 2)); 				\
	SWAP_SHORTS(V(29, 7, 5)) = SWAP_SHORT(V(30, 7, 3)); 				\
	SWAP_SHORTS(V(29, 7, 6)) = SWAP_SHORT(V(31, 7, 2)); 				\
	SWAP_SHORTS(V(29, 7, 7)) = SWAP_SHORT(V(31, 7, 3))

#define SUPER_REARRANGEMENT_PORTION30() 						\
	SWAP_SHORTS(V(30, 7, 6)) = SWAP_SHORT(V(31, 7, 4)); 				\
	SWAP_SHORTS(V(30, 7, 7)) = SWAP_SHORT(V(31, 7, 5))

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

		createV(pNumber, 0, 0);
		createV(pNumber, 0, 1);
		createV(pNumber, 0, 2);
		createV(pNumber, 0, 3);
		//uint16_t va1 = substitutionTable[*SA(0)];
		//uint16_t va2 = substitutionTable[*SA(1)];
		//uint16_t va3 = substitutionTable[*SA(2)];
		//uint16_t va4 = substitutionTable[*SA(3)];

		//cout << hex << "Substutited va1 to get new va1 : " << va1 << endl;

		V(pNumber, 0, 1) = V(pNumber, 0, 0);
		V(pNumber, 0, 2) = V(pNumber, 0, 1);
		V(pNumber, 0, 3) = V(pNumber, 0, 2);
		//va2 ^= va1;
		//va3 ^= va2;
		//va4 ^= va3;

		createV(pNumber, 0, 4);
		createV(pNumber, 0, 5);
		createV(pNumber, 0, 6);
		createV(pNumber, 0, 7);
		//uint16_t va5 = substitutionTable[*SA(4)];
		//uint16_t va6 = substitutionTable[*SA(5)];
		//uint16_t va7 = substitutionTable[*SA(6)];
		//uint16_t va8 = substitutionTable[*SA(7)];

		V(pNumber, 0, 4) = V(pNumber, 0, 3);
		V(pNumber, 0, 5) = V(pNumber, 0, 4);
		V(pNumber, 0, 6) = V(pNumber, 0, 5);
		V(pNumber, 0, 7) = V(pNumber, 0, 6);
		V(pNumber, 0, 1) = V(pNumber, 0, 7); // Circular-xor
		//va5 ^= va4;
		//va6 ^= va5;
		//va7 ^= va6;
		//va8 ^= va7;
		//va1 ^= va8; //Circular-xor across section


		uint64_t* ukptr1 = reinterpret_cast<uint64_t *>(EPUK.data()+start);

#define EPUK_SHORT(a)	*(reinterpret_cast<uint16_t *>(ukptr1)+a)

		V(pNumber, 0, 0) ^= EPUK_SHORT(0) ^ OTPs[EPUK[startOTPSectionCount]][0];
		V(pNumber, 0, 1) ^= EPUK_SHORT(1) ^ OTPs[EPUK[startOTPSectionCount]][1];
		V(pNumber, 0, 2) ^= EPUK_SHORT(2) ^ OTPs[EPUK[startOTPSectionCount]][2];
		V(pNumber, 0, 3) ^= EPUK_SHORT(3) ^ OTPs[EPUK[startOTPSectionCount]][3];
		V(pNumber, 0, 4) ^= EPUK_SHORT(4) ^ OTPs[EPUK[startOTPSectionCount]][4];
		V(pNumber, 0, 5) ^= EPUK_SHORT(5) ^ OTPs[EPUK[startOTPSectionCount]][5];
		V(pNumber, 0, 6) ^= EPUK_SHORT(6) ^ OTPs[EPUK[startOTPSectionCount]][6];
		V(pNumber, 0, 7) ^= EPUK_SHORT(7) ^ OTPs[EPUK[startOTPSectionCount]][7];
		//va1 ^= EPUK_SHORT(0) ^ OTPs[EPUK[startOTPSectionCount]][0];
		//va2 ^= EPUK_SHORT(1) ^ OTPs[EPUK[startOTPSectionCount]][1];
		//va3 ^= EPUK_SHORT(2) ^ OTPs[EPUK[startOTPSectionCount]][2];
		//va4 ^= EPUK_SHORT(3) ^ OTPs[EPUK[startOTPSectionCount]][3];
		//va5 ^= EPUK_SHORT(4) ^ OTPs[EPUK[startOTPSectionCount]][4];
		//va6 ^= EPUK_SHORT(5) ^ OTPs[EPUK[startOTPSectionCount]][5];
		//va7 ^= EPUK_SHORT(6) ^ OTPs[EPUK[startOTPSectionCount]][6];
		//va8 ^= EPUK_SHORT(7) ^ OTPs[EPUK[startOTPSectionCount]][7];


		// Process 2nd section of 128 bits
		createV(pNumber, 1, 0);
		createV(pNumber, 1, 1);
		createV(pNumber, 1, 2);
		createV(pNumber, 1, 3);
		//uint16_t vb1 = substitutionTable[*SB(0)];
		//uint16_t vb2 = substitutionTable[*SB(1)];
		//uint16_t vb3 = substitutionTable[*SB(2)];
		//uint16_t vb4 = substitutionTable[*SB(3)];

		V(pNumber, 1, 1) = V(pNumber, 1, 0);
		V(pNumber, 1, 2) = V(pNumber, 1, 1);
		V(pNumber, 1, 3) = V(pNumber, 1, 2);
		//vb2 ^= vb1;
		//vb3 ^= vb2;
		//vb4 ^= vb3;

		createV(pNumber, 1, 4);
		createV(pNumber, 1, 5);
		createV(pNumber, 1, 6);
		createV(pNumber, 1, 7);
		//uint16_t vb5 = substitutionTable[*SB(4)];
		//uint16_t vb6 = substitutionTable[*SB(5)];
		//uint16_t vb7 = substitutionTable[*SB(6)];
		//uint16_t vb8 = substitutionTable[*SB(7)];

		V(pNumber, 1, 4) = V(pNumber, 1, 3);
		V(pNumber, 1, 5) = V(pNumber, 1, 4);
		V(pNumber, 1, 6) = V(pNumber, 1, 5);
		V(pNumber, 1, 7) = V(pNumber, 1, 6);
		V(pNumber, 1, 0) = V(pNumber, 1, 7); // Circular-xor
		//vb5 ^= vb4;
		//vb6 ^= vb5;
		//vb7 ^= vb6;
		//vb8 ^= vb7;
		//vb1 ^= vb8; //Circular-xor across section

		V(pNumber, 1, 0) ^= EPUK_SHORT(8) ^ OTPs[EPUK[startOTPSectionCount]][8];
		V(pNumber, 1, 1) ^= EPUK_SHORT(9) ^ OTPs[EPUK[startOTPSectionCount]][9];
		V(pNumber, 1, 2) ^= EPUK_SHORT(10) ^ OTPs[EPUK[startOTPSectionCount]][10];
		V(pNumber, 1, 3) ^= EPUK_SHORT(11) ^ OTPs[EPUK[startOTPSectionCount]][11];
		V(pNumber, 1, 4) ^= EPUK_SHORT(12) ^ OTPs[EPUK[startOTPSectionCount]][12];
		V(pNumber, 1, 5) ^= EPUK_SHORT(13) ^ OTPs[EPUK[startOTPSectionCount]][13];
		V(pNumber, 1, 6) ^= EPUK_SHORT(14) ^ OTPs[EPUK[startOTPSectionCount]][14];
		V(pNumber, 1, 7) ^= EPUK_SHORT(15) ^ OTPs[EPUK[startOTPSectionCount]][15];
		//vb1 ^= EPUK_SHORT(8) ^ OTPs[EPUK[startOTPSectionCount]][8];
		//vb2 ^= EPUK_SHORT(9) ^ OTPs[EPUK[startOTPSectionCount]][9];
		//vb3 ^= EPUK_SHORT(10) ^ OTPs[EPUK[startOTPSectionCount]][10];
		//vb4 ^= EPUK_SHORT(11) ^ OTPs[EPUK[startOTPSectionCount]][11];
		//vb5 ^= EPUK_SHORT(12) ^ OTPs[EPUK[startOTPSectionCount]][12];
		//vb6 ^= EPUK_SHORT(13) ^ OTPs[EPUK[startOTPSectionCount]][13];
		//vb7 ^= EPUK_SHORT(14) ^ OTPs[EPUK[startOTPSectionCount]][14];
		//vb8 ^= EPUK_SHORT(15) ^ OTPs[EPUK[startOTPSectionCount]][15];

		// Process 3rd section of 128 bits
		createV(pNumber, 2, 0);
		createV(pNumber, 2, 1);
		createV(pNumber, 2, 2);
		createV(pNumber, 2, 3);

		V(pNumber, 2, 1) = V(pNumber, 2, 0);
		V(pNumber, 2, 2) = V(pNumber, 2, 1);
		V(pNumber, 2, 3) = V(pNumber, 2, 2);

		createV(pNumber, 2, 4);
		createV(pNumber, 2, 5);
		createV(pNumber, 2, 6);
		createV(pNumber, 2, 7);

		V(pNumber, 2, 4) = V(pNumber, 2, 3);
		V(pNumber, 2, 5) = V(pNumber, 2, 4);
		V(pNumber, 2, 6) = V(pNumber, 2, 5);
		V(pNumber, 2, 7) = V(pNumber, 2, 6);
		V(pNumber, 2, 0) = V(pNumber, 2, 7); // Circular-xor

		V(pNumber, 2, 0) ^= EPUK_SHORT(16) ^ OTPs[EPUK[startOTPSectionCount]][16];
		V(pNumber, 2, 1) ^= EPUK_SHORT(17) ^ OTPs[EPUK[startOTPSectionCount]][17];
		V(pNumber, 2, 2) ^= EPUK_SHORT(18) ^ OTPs[EPUK[startOTPSectionCount]][18];
		V(pNumber, 2, 3) ^= EPUK_SHORT(19) ^ OTPs[EPUK[startOTPSectionCount]][19];
		V(pNumber, 2, 4) ^= EPUK_SHORT(20) ^ OTPs[EPUK[startOTPSectionCount]][20];
		V(pNumber, 2, 5) ^= EPUK_SHORT(21) ^ OTPs[EPUK[startOTPSectionCount]][21];
		V(pNumber, 2, 6) ^= EPUK_SHORT(22) ^ OTPs[EPUK[startOTPSectionCount]][22];
		V(pNumber, 2, 7) ^= EPUK_SHORT(23) ^ OTPs[EPUK[startOTPSectionCount]][23];

		// Process 4th section of 128 bits
		createV(pNumber, 3, 0);
		createV(pNumber, 3, 1);
		createV(pNumber, 3, 2);
		createV(pNumber, 3, 3);

		V(pNumber, 3, 1) = V(pNumber, 3, 0);
		V(pNumber, 3, 2) = V(pNumber, 3, 1);
		V(pNumber, 3, 3) = V(pNumber, 3, 2);

		createV(pNumber, 3, 4);
		createV(pNumber, 3, 5);
		createV(pNumber, 3, 6);
		createV(pNumber, 3, 7);

		V(pNumber, 3, 4) = V(pNumber, 3, 3);
		V(pNumber, 3, 5) = V(pNumber, 3, 4);
		V(pNumber, 3, 6) = V(pNumber, 3, 5);
		V(pNumber, 3, 7) = V(pNumber, 3, 6);
		V(pNumber, 3, 0) = V(pNumber, 3, 7); // Circular-xor

		V(pNumber, 3, 0) ^= EPUK_SHORT(24) ^ OTPs[EPUK[startOTPSectionCount]][24];
		V(pNumber, 3, 1) ^= EPUK_SHORT(25) ^ OTPs[EPUK[startOTPSectionCount]][25];
		V(pNumber, 3, 2) ^= EPUK_SHORT(26) ^ OTPs[EPUK[startOTPSectionCount]][26];
		V(pNumber, 3, 3) ^= EPUK_SHORT(27) ^ OTPs[EPUK[startOTPSectionCount]][27];
		V(pNumber, 3, 4) ^= EPUK_SHORT(28) ^ OTPs[EPUK[startOTPSectionCount]][28];
		V(pNumber, 3, 5) ^= EPUK_SHORT(29) ^ OTPs[EPUK[startOTPSectionCount]][29];
		V(pNumber, 3, 6) ^= EPUK_SHORT(30) ^ OTPs[EPUK[startOTPSectionCount]][30];
		V(pNumber, 3, 7) ^= EPUK_SHORT(31) ^ OTPs[EPUK[startOTPSectionCount]][31];
		++startOTPSectionCount;

		// Process 5th section of 128 bits
		createV(pNumber, 4, 0);
		createV(pNumber, 4, 1);
		createV(pNumber, 4, 2);
		createV(pNumber, 4, 3);

		V(pNumber, 4, 1) = V(pNumber, 4, 0);
		V(pNumber, 4, 2) = V(pNumber, 4, 1);
		V(pNumber, 4, 3) = V(pNumber, 4, 2);

		createV(pNumber, 4, 4);
		createV(pNumber, 4, 5);
		createV(pNumber, 4, 6);
		createV(pNumber, 4, 7);

		V(pNumber, 4, 4) = V(pNumber, 4, 3);
		V(pNumber, 4, 5) = V(pNumber, 4, 4);
		V(pNumber, 4, 6) = V(pNumber, 4, 5);
		V(pNumber, 4, 7) = V(pNumber, 4, 6);
		V(pNumber, 4, 0) = V(pNumber, 4, 7); // Circular-xor

		V(pNumber, 4, 0) ^= EPUK_SHORT(32) ^ OTPs[EPUK[startOTPSectionCount]][0];
		V(pNumber, 4, 0) ^= EPUK_SHORT(33) ^ OTPs[EPUK[startOTPSectionCount]][1];
		V(pNumber, 4, 0) ^= EPUK_SHORT(34) ^ OTPs[EPUK[startOTPSectionCount]][2];
		V(pNumber, 4, 0) ^= EPUK_SHORT(35) ^ OTPs[EPUK[startOTPSectionCount]][3];
		V(pNumber, 4, 0) ^= EPUK_SHORT(36) ^ OTPs[EPUK[startOTPSectionCount]][4];
		V(pNumber, 4, 0) ^= EPUK_SHORT(37) ^ OTPs[EPUK[startOTPSectionCount]][5];
		V(pNumber, 4, 0) ^= EPUK_SHORT(38) ^ OTPs[EPUK[startOTPSectionCount]][6];
		V(pNumber, 4, 0) ^= EPUK_SHORT(39) ^ OTPs[EPUK[startOTPSectionCount]][7];

		// Process 6th section of 128 bits
		createV(pNumber, 5, 0);
		createV(pNumber, 5, 1);
		createV(pNumber, 5, 2);
		createV(pNumber, 5, 3);

		V(pNumber, 5, 1) = V(pNumber, 5, 0);
		V(pNumber, 5, 2) = V(pNumber, 5, 1);
		V(pNumber, 5, 3) = V(pNumber, 5, 2);

		createV(pNumber, 5, 4);
		createV(pNumber, 5, 5);
		createV(pNumber, 5, 6);
		createV(pNumber, 5, 7);

		V(pNumber, 5, 4) = V(pNumber, 5, 3);
		V(pNumber, 5, 5) = V(pNumber, 5, 4);
		V(pNumber, 5, 6) = V(pNumber, 5, 5);
		V(pNumber, 5, 7) = V(pNumber, 5, 6);
		V(pNumber, 5, 0) = V(pNumber, 5, 7); // Circular-xor

		V(pNumber, 5, 0) ^= EPUK_SHORT(40) ^ OTPs[EPUK[startOTPSectionCount]][8];
		V(pNumber, 5, 1) ^= EPUK_SHORT(41) ^ OTPs[EPUK[startOTPSectionCount]][9];
		V(pNumber, 5, 2) ^= EPUK_SHORT(42) ^ OTPs[EPUK[startOTPSectionCount]][10];
		V(pNumber, 5, 3) ^= EPUK_SHORT(43) ^ OTPs[EPUK[startOTPSectionCount]][11];
		V(pNumber, 5, 4) ^= EPUK_SHORT(44) ^ OTPs[EPUK[startOTPSectionCount]][12];
		V(pNumber, 5, 5) ^= EPUK_SHORT(45) ^ OTPs[EPUK[startOTPSectionCount]][13];
		V(pNumber, 5, 6) ^= EPUK_SHORT(46) ^ OTPs[EPUK[startOTPSectionCount]][14];
		V(pNumber, 5, 7) ^= EPUK_SHORT(47) ^ OTPs[EPUK[startOTPSectionCount]][15];

		// Process 7th section of 128 bits
		createV(pNumber, 6, 0);
		createV(pNumber, 6, 1);
		createV(pNumber, 6, 2);
		createV(pNumber, 6, 3);

		V(pNumber, 6, 1) = V(pNumber, 6, 0);
		V(pNumber, 6, 2) = V(pNumber, 6, 1);
		V(pNumber, 6, 3) = V(pNumber, 6, 2);

		createV(pNumber, 6, 4);
		createV(pNumber, 6, 5);
		createV(pNumber, 6, 6);
		createV(pNumber, 6, 7);

		V(pNumber, 6, 4) = V(pNumber, 6, 3);
		V(pNumber, 6, 5) = V(pNumber, 6, 4);
		V(pNumber, 6, 6) = V(pNumber, 6, 5);
		V(pNumber, 6, 7) = V(pNumber, 6, 6);
		V(pNumber, 6, 0) = V(pNumber, 6, 7); // Circular-xor

		V(pNumber, 6, 0) ^= EPUK_SHORT(48) ^ OTPs[EPUK[startOTPSectionCount]][16];
		V(pNumber, 6, 1) ^= EPUK_SHORT(49) ^ OTPs[EPUK[startOTPSectionCount]][17];
		V(pNumber, 6, 2) ^= EPUK_SHORT(50) ^ OTPs[EPUK[startOTPSectionCount]][18];
		V(pNumber, 6, 3) ^= EPUK_SHORT(51) ^ OTPs[EPUK[startOTPSectionCount]][19];
		V(pNumber, 6, 4) ^= EPUK_SHORT(52) ^ OTPs[EPUK[startOTPSectionCount]][20];
		V(pNumber, 6, 5) ^= EPUK_SHORT(53) ^ OTPs[EPUK[startOTPSectionCount]][21];
		V(pNumber, 6, 6) ^= EPUK_SHORT(54) ^ OTPs[EPUK[startOTPSectionCount]][22];
		V(pNumber, 6, 7) ^= EPUK_SHORT(55) ^ OTPs[EPUK[startOTPSectionCount]][23];

		// Process 8th section of 128 bits
		createV(pNumber, 7, 0);
		createV(pNumber, 7, 1);
		createV(pNumber, 7, 2);
		createV(pNumber, 7, 3);

		V(pNumber, 7, 1) = V(pNumber, 7, 0);
		V(pNumber, 7, 2) = V(pNumber, 7, 1);
		V(pNumber, 7, 3) = V(pNumber, 7, 2);

		createV(pNumber, 7, 4);
		createV(pNumber, 7, 5);
		createV(pNumber, 7, 6);
		createV(pNumber, 7, 7);

		V(pNumber, 7, 4) = V(pNumber, 7, 3);
		V(pNumber, 7, 5) = V(pNumber, 7, 4);
		V(pNumber, 7, 6) = V(pNumber, 7, 5);
		V(pNumber, 7, 7) = V(pNumber, 7, 6);
		V(pNumber, 7, 0) = V(pNumber, 7, 7); // Circular-xor

		V(pNumber, 7, 0) ^= EPUK_SHORT(56) ^ OTPs[EPUK[startOTPSectionCount]][24];
		V(pNumber, 7, 1) ^= EPUK_SHORT(57) ^ OTPs[EPUK[startOTPSectionCount]][25];
		V(pNumber, 7, 2) ^= EPUK_SHORT(58) ^ OTPs[EPUK[startOTPSectionCount]][26];
		V(pNumber, 7, 3) ^= EPUK_SHORT(59) ^ OTPs[EPUK[startOTPSectionCount]][27];
		V(pNumber, 7, 4) ^= EPUK_SHORT(60) ^ OTPs[EPUK[startOTPSectionCount]][28];
		V(pNumber, 7, 5) ^= EPUK_SHORT(61) ^ OTPs[EPUK[startOTPSectionCount]][29];
		V(pNumber, 7, 6) ^= EPUK_SHORT(62) ^ OTPs[EPUK[startOTPSectionCount]][30];
		V(pNumber, 7, 7) ^= EPUK_SHORT(63) ^ OTPs[EPUK[startOTPSectionCount]][31];
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

int main() {
return 0;
}
