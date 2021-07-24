#include <iostream>

using namespace std;

constexpr uint32_t k1 = 1024;
constexpr uint32_t k64 = 64*k1;
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

void loadVariables(uint16_t* ptr, uint32_t size) {
	chrono::duration<double, nano> duration;
	Timer timer("Times to load array of size " + to_string(size) + " into 16-bit variables: ", duration);

	for(uint32_t i = 0; i < size; ++i) {
		uint16_t var = *ptr;
		++ptr;
	}
}

void simdLoad(uint16_t* ptr, uint32_t size) {
	chrono::duration<double, nano> duration;
	Timer timer("Times to load array of size " + to_string(size) + " using SIMD: ", duration);

	for(uint32_t i = 0; i < size; i += sizeof(uint32_t)) {
		const __m512i tmp1 = _mm512_loadu_si512(ptr + i); // loads 512-bits, but we will only be working with the first 32 bits
		/* AA BB CC DD EE FF ... */
		const __m512i tmp2 = _mm512_permutexvar_epi16(
		    _mm512_set_epi16(15, 15, 14, 14, 13, 13, 12, 12, 11, 11, 10, 10, 9, 9, 8, 8, 7, 7, 6, 6, 5, 5, 4, 4, 3, 3, 2, 2, 1, 1, 0, 0),
		    tmp1
		);
		/* AA BB AA BB CC DD CC DD ... */
		const __m512i tmp3 = _mm512_mask_srli_epi32(tmp2, 0x5555, tmp2, 16); 
		/* .. AA AA BB .. CC CC DD ... */
		const __m512 tmp4 = _mm_512_and_epi32(tmp3, _mm512_set1_epi32(0x0000ffff));
		/* .. AA .. BB .. CC .. DD */
			
		vector<uint16_t> temp(size);
		_mm512_mask_cvtepi32_storeu_epi16(temp.data(), 0xffff, tmp4);
	}
}

int main() {

	uint32_t size = m4 / sizeof(uint16_t);
	vector<uint16_t> array;
	array.resize(size);

	for (uint32_t i = 0; i < size; ++i) {
		array[i] = i % k64;
	}

	loadVariables(reinterpret_cast<uint16_t*>array.data(), size);

	return 0;
}
