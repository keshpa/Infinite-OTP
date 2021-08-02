#include <assert.h>
#include <cstdint>
#include <iomanip>
#include <chrono>
#include <iostream>
#include <string.h>
#include <pmmintrin.h>
#include <immintrin.h>
#include <vector>

using namespace std;

constexpr uint32_t k1 = 1024;
constexpr uint32_t k64 = 64*k1;
constexpr uint32_t k32 = 32*k1;
constexpr uint32_t m1 = k1*k1;
constexpr uint32_t m4 = 4*m1;

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

template<typename T>
void printVector(vector<T> vec, uint32_t start = 0, uint32_t end = 0) {
	for (uint32_t i = start; i < end; ++i) {
		cout << hex << vec[i] << "  " << setw(3) << dec;
	}		
	cout << endl;
}

template<typename T>
void printType(T* ptr, uint32_t size, string str = "") {
	cout << str << endl;
	for (uint32_t i = 0; i < size; ++i) {
		cout << hex << (*(ptr+i)) << "  " << setw(3) << dec;
	}
	cout << endl;
}

vector<uint32_t> substitutionTable;
vector<vector<uint32_t>> OTP;
vector<uint32_t> EPUK;
uint32_t* subsTablePtr;
uint32_t* OTPptr;
uint32_t* EPUKptr;

void initialize() {
	substitutionTable.resize(k64);
	for (uint32_t i = 0; i < k64; ++i) {
		substitutionTable[i] = (k64 - i - 1);
	}
	subsTablePtr = substitutionTable.data();

	EPUK.resize(m4);
	for (uint32_t i = 0; i < m4; ++i) {
		EPUK[i] = i & 0xFFFF;
	}
	OTP.resize(k64);
	for (uint32_t i = 0; i < k64; ++i) {
		OTP[i].resize(64);
		for (uint32_t j = 0; j < 64; ++j) {
			OTP[i][j] = rand() & 0xFFFF;
		}
	}
	EPUKptr = reinterpret_cast<uint32_t *>(EPUK.data())+16;
	OTPptr = reinterpret_cast<uint32_t *>(OTP.data())+16;
	EPUKptr = reinterpret_cast<uint32_t *>(reinterpret_cast<uint64_t>(EPUKptr) & ~(64UL - 1));
	OTPptr = reinterpret_cast<uint32_t *>(reinterpret_cast<uint64_t>(OTPptr) & ~(64UL - 1));
}



uint16_t test;
uint16_t test1;

void loadVariables(uint16_t* ptr, uint32_t size) {
	chrono::duration<double, nano> duration;
	Timer timer("Times to load array of size " + to_string(size) + " into 16-bit variables: ", duration);

	uint32_t x0 = *(EPUKptr) ^ *(OTPptr);
	uint32_t x1 = *(EPUKptr+1) ^ *(OTPptr+1);
	uint32_t x2 = *(EPUKptr+2) ^ *(OTPptr+2);
	uint32_t x3 = *(EPUKptr+3) ^ *(OTPptr+3);
	uint32_t x4 = *(EPUKptr+4) ^ *(OTPptr+4);
	uint32_t x5 = *(EPUKptr+5) ^ *(OTPptr+5);
	uint32_t x6 = *(EPUKptr+6) ^ *(OTPptr+6);
	uint32_t x7 = *(EPUKptr+7) ^ *(OTPptr+7);
	uint32_t x8 = *(EPUKptr+8) ^ *(OTPptr+8);
	uint32_t x9 = *(EPUKptr+9) ^ *(OTPptr+9);
	uint32_t x10 = *(EPUKptr+10) ^ *(OTPptr+10);
	uint32_t x11 = *(EPUKptr+11) ^ *(OTPptr+11);
	uint32_t x12 = *(EPUKptr+12) ^ *(OTPptr+12);
	uint32_t x13 = *(EPUKptr+13) ^ *(OTPptr+13);
	uint32_t x14 = *(EPUKptr+14) ^ *(OTPptr+14);
	uint32_t x15 = *(EPUKptr+15) ^ *(OTPptr+15);

	for(uint32_t i = 0; i < size; i += 16) {
		// Load and substitute
		uint16_t v0, v1, v2, v3, v4, v5, v6, v7, v8, v9, v10, v11, v12, v13, v14, v15;
		v0 = subsTablePtr[*ptr];
		v1 = subsTablePtr[*(1+ptr)];
		v2 = subsTablePtr[*(2+ptr)];
		v3 = subsTablePtr[*(3+ptr)];
		v4 = subsTablePtr[*(4+ptr)];
		v5 = subsTablePtr[*(5+ptr)];
		v6 = subsTablePtr[*(6+ptr)];
		v7 = subsTablePtr[*(7+ptr)];
		v8 = subsTablePtr[*(8+ptr)];
		v9 = subsTablePtr[*(9+ptr)];
		v10 = subsTablePtr[*(10+ptr)];
		v11 = subsTablePtr[*(11+ptr)];
		v12 = subsTablePtr[*(12+ptr)];
		v13 = subsTablePtr[*(13+ptr)];
		v14 = subsTablePtr[*(14+ptr)];
		v15 = subsTablePtr[*(15+ptr)];

		// XORs
		v14 ^= v15;
		test = v14;
		test1 = test;
		v13 ^= v14;
		test = v13;
		test1 = test;
		v12 ^= v13;
		test = v12;
		test1 = test;
		v11 ^= v12;
		test = v11;
		test1 = test;
		v10 ^= v11;
		test = v10;
		test1 = test;
		v9 ^= v10;
		test = v9;
		test1 = test;
		v8 ^= v9;
		test = v8;
		test1 = test;
		v7 ^= v8;
		test = v7;
		test1 = test;
		v6 ^= v7;
		test = v6;
		test1 = test;
		v5 ^= v6;
		test = v5;
		test1 = test;
		v4 ^= v5;
		test = v4;
		test1 = test;
		v3 ^= v4;
		test = v3;
		test1 = test;
		v2 ^= v3;
		test = v2;
		test1 = test;
		v1 ^= v2;
		test = v1;
		test1 = test;
		v0 ^= v1;
		test = v0;
		test1 = test;

		// circular xor
		v15 ^= v0;
		test = v15;
		test1 = test;

		// XOR with EPUK & OTP
		v0 ^= x0;
		v1 ^= x1;
		v2 ^= x2;
		v3 ^= x3;
		v4 ^= x4;
		v5 ^= x5;
		v6 ^= x6;
		v7 ^= x7;
		v8 ^= x8;
		v9 ^= x9;
		v10 ^= x10;
		v11 ^= x11;
		v12 ^= x12;
		v13 ^= x13;
		v14 ^= x14;
		v15 ^= x15;

		// Store
		/*
		*(ptr) = v0;
		*(ptr+1) = v1;
		*(ptr+2) = v2;
		*(ptr+3) = v3;
		*(ptr+4) = v4;
		*(ptr+5) = v5;
		*(ptr+6) = v6;
		*(ptr+7) = v7;
		*(ptr+8) = v8;
		*(ptr+9) = v9;
		*(ptr+10) = v10;
		*(ptr+11) = v11;
		*(ptr+12) = v12;
		*(ptr+13) = v13;
		*(ptr+14) = v14;
		*(ptr+15) = v15;
		*/

		ptr += 16;	
	}
}

__m512i testsimd;
__m512i testsimd1;

void simdLoad(uint16_t* ptr, uint32_t size) {
	vector<uint64_t> temp;
	temp.resize(size+32);
	fill(temp.begin(), temp.end(), 0L);
	// Align dptr (required for store operation)
	uint32_t* dptr = reinterpret_cast<uint32_t *>(temp.data())+16;
	dptr = reinterpret_cast<uint32_t *>(reinterpret_cast<uint64_t>(dptr) & ~(64UL - 1));
	// Test alignment of dptr
	assert((reinterpret_cast<uint64_t>(dptr)/64)*64 == (reinterpret_cast<uint64_t>(dptr)));

	uint32_t i = 0;
	{
		__m512i loadedOTP_EPUK = _mm512_xor_epi32(_mm512_load_epi32(OTPptr), _mm512_load_epi32(EPUKptr));
		loadedOTP_EPUK = _mm512_and_epi32(loadedOTP_EPUK, _mm512_set1_epi32(0x0000FFFF));

		__m512i tmp4 = _mm512_set_epi32(0x0000000F, 0x0000000F, 0x0000000E, 0x0000000D, 0x0000000C, 0x0000000B, 0x0000000A, 0x00000009, 0x00000008, 0x00000007, 0x00000006, 0x00000005, 0x00000004, 0x00000003, 0x00000002, 0x00000001);
		__m512i circularXORtmpl = _mm512_set_epi32(0x00000000, 0x0000000F, 0x0000000E, 0x0000000D, 0x0000000C, 0x0000000B, 0x0000000A, 0x00000009, 0x00000008, 0x00000007, 0x00000006, 0x00000005, 0x00000004, 0x00000003, 0x00000002, 0x00000001);

		chrono::duration<double, nano> duration;
		Timer timer("Times to load array of size " + to_string(size) + " using SIMD: ", duration);

		for(i = 0; i < size; i += 16) {
			// loads 256-bits (16 shorts), that we will be working with.
			__m256i tmp1 = _mm256_lddqu_si256(reinterpret_cast<const __m256i *>(ptr + i));
			// Convert the shorts into sign extended ints and later mask the upper 16 bits of the int.
			__m512i tmp2 = _mm512_cvtepi16_epi32(tmp1);
			// Mask the upper 16 bits of each int32_t
			tmp2 = _mm512_and_epi32(tmp2, _mm512_set1_epi32(0x0000FFFF));
			// Perform substitution
			tmp2 = _mm512_i32gather_epi32(tmp2, subsTablePtr, 4);
			/*
			// We have to move 32 bits to the right and XOR everything except the top 32 bits with tmp2.
			__m512i tmp3 = _mm512_permutexvar_epi32(tmp4, tmp2);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x7FFF, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 64 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x3FFF, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 64 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x1FFF, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 64 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x0FFF, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 32 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x07FF, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 64 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x03FF, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 64 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x01FF, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 64 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x00FF, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 32 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x007F, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 64 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x003F, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 64 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x001F, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 64 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x000F, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 32 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x0007, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 64 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x0003, tmp2, tmp3);
			// We have to move 32 bits to the right and XOR everything except the top 64 bits with tmp2.
			tmp3 = _mm512_permutexvar_epi32(tmp4, tmp3);
			tmp2 = _mm512_mask_xor_epi32 (tmp2, 0x0001, tmp2, tmp3);
			// Circular xor
			tmp3 = _mm512_permutexvar_epi32(circularXORtmpl, tmp2);
			tmp2 = _mm512_mask_xor_epi32(tmp2, 0x8000, tmp2, tmp3);
			// XOR with EPUK and OTP
			tmp2 = _mm512_xor_epi32(tmp2, loadedOTP_EPUK); 
			*/


			testsimd = tmp2;
			testsimd1 = testsimd;


			// Stores

			// TEST ===== ~4000 micro
			/*
			 uint16_t* tmpPtr = reinterpret_cast<uint16_t*>(&tmp2);
			uint16_t* dptrcpy = reinterpret_cast<uint16_t*>(dptr)+i;
			 *(dptrcpy) = *tmpPtr;
			 *(dptrcpy+1) = *(tmpPtr+2);
			 *(dptrcpy+2) = *(tmpPtr+4);
			 *(dptrcpy+3) = *(tmpPtr+6);
			 *(dptrcpy+4) = *(tmpPtr+8);
			 *(dptrcpy+5) = *(tmpPtr+10);
			 *(dptrcpy+6) = *(tmpPtr+12);
			 *(dptrcpy+7) = *(tmpPtr+14);
			 *(dptrcpy+8) = *(tmpPtr+16);
			 *(dptrcpy+9) = *(tmpPtr+18);
			 *(dptrcpy+10) = *(tmpPtr+20);
			 *(dptrcpy+11) = *(tmpPtr+22);
			 *(dptrcpy+12) = *(tmpPtr+24);
			 *(dptrcpy+13) = *(tmpPtr+26);
			 *(dptrcpy+14) = *(tmpPtr+28);
			 *(dptrcpy+15) = *(tmpPtr+30);
			 */
			// TEST =====

			// TEST ===== ~4000 micro
			/*
			uint32_t v0 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+0) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v1 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+1) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v2 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+2) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v3 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+3) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v4 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+4) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v5 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+5) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v6 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+6) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v7 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+7) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v8 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+8) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v9 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+9) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v10 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+10) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v11 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+11) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v12 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+12) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v13 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+13) = v0;
			//tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v14 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+14) = v0;
			//			tmp2 = _mm512_permutexvar_epi32(tmp4, tmp2);
			uint32_t v15 = _mm_cvtsi128_si32(_mm512_castsi512_si128(tmp2));
			//			*(dptrcpy+15) = v0;
			*(ptr) = v0;
			*(ptr+1) = v1;
			*(ptr+2) = v2;
			*(ptr+3) = v3;
			*(ptr+4) = v4;
			*(ptr+5) = v5;
			*(ptr+6) = v6;
			*(ptr+7) = v7;
			*(ptr+8) = v8;
			*(ptr+9) = v9;
			*(ptr+10) = v10;
			*(ptr+11) = v11;
			*(ptr+12) = v12;
			*(ptr+13) = v13;
			*(ptr+14) = v14;
			*(ptr+15) = v15;
			*/
			// TEST =====

			// Simd stores
			//	_mm512_mask_i32scatter_epi32((reinterpret_cast<uint16_t*>(dptr))+i, 0xFFFF, tmp2, tmp2, 4); DOES NOT WORK
			//	_mm512_stream_si512(reinterpret_cast<__m512i*>(dptr+i), tmp2); ~2000 micro
			//	_mm512_storeu_si512(reinterpret_cast<__m512i*>(dptr+i), tmp2); //~2000 micro
			//	_mm512_mask_cvtepi32_storeu_epi16(dptr+i, 0xFFFF, tmp2); ~2000 micro
		}
	}
	//	printType<uint16_t>(reinterpret_cast<uint16_t*>(ptr), 16, "Source 256 bits as shorts.");
	//	printType<uint32_t>(reinterpret_cast<uint32_t*>(subsTablePtr), 16, "Substitution table contents.");
	//	printType<uint16_t>(reinterpret_cast<uint16_t*>(dptr), 16, "simdLoad values as shorts after one subs and XOR.");
	printType<uint16_t>(reinterpret_cast<uint16_t*>(ptr), 16, "simdLoad values as shorts after one subs and XOR.");
}

/* AA BB CC DD EE FF ... */
//		const __m512i tmp2 = _mm512_permutexvar_epi16(
//		    _mm512_set_epi32(0x000F000F, 0x000E000E, 0x000D000D, 0x000C000C, 0x000B000B, 0x000A000A, 0x00090009, 0x00080008, 0x00070007, 0x00060006, 0x00050005, 0x00040004, 0x00030003, 0x00020002, 0x00010001, 0x00000000),
//		    tmp1
//		);
/* AA BB AA BB CC DD CC DD ... */
//		const __m512i tmp3 = _mm512_mask_srli_epi32(tmp2, 0x5555, tmp2, 16); 
/* .. AA AA BB .. CC CC DD ... */
//		const __m512i tmp4 = _mm512_and_epi32(tmp3, _mm512_set1_epi32(0x0000ffff));
/* .. AA .. BB .. CC .. DD */

//		vector<uint16_t> temp(size);
//		_mm512_mask_cvtepi32_storeu_epi16(temp.data(), 0xffff, tmp4);
//	}
//}

int main() {
	initialize();

	uint32_t size = m4 / sizeof(uint16_t);
	vector<uint16_t> array;
	array.resize(size);

	for (uint32_t i = 0; i < size; ++i) {
		array[i] = i % k64;
	}
	vector<uint16_t> copy(array);

	loadVariables(reinterpret_cast<uint16_t*>(array.data()), size);
	printType<uint16_t>(reinterpret_cast<uint16_t*>(array.data()), 16, "loadVariables values as shorts after one subs and XOR.");
	simdLoad(reinterpret_cast<uint16_t*>(copy.data()), size);

	return 0;
}
