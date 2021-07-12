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

constexpr int64_t k64 = 1024*64;

uint16_t replacement[k64];

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

int main() {

	int j = 0;
	for (int i = k64-1; i >= 0; --i) {
		replacement[i] = j;
		++j;
	}

	uint64_t base[k64];
	for (int i = 0; i < k64; ++i) {
		base[i] = 0x4142434445464748+i;
	}

	chrono::duration<double, nano> duration;

	{
		Timer timer1("Time to substitute 64 shorts of a 128 byte portion without shifting: ", duration);

		for (int k = 0; k < k64; k += 2) {
			uint64_t* bptr = &base[k];
			uint16_t* sa1 = reinterpret_cast<uint16_t *>(bptr);
			uint16_t* sa2 = reinterpret_cast<uint16_t *>(bptr)+1;
			uint16_t* sa3 = reinterpret_cast<uint16_t *>(bptr)+2;
			uint16_t* sa4 = reinterpret_cast<uint16_t *>(bptr)+3;

			uint16_t va1 = replacement[*sa1];
			uint16_t va2 = replacement[*sa2];
			uint16_t va3 = replacement[*sa3];
			uint16_t va4 = replacement[*sa4];

			va2 ^= va1;
			va3 ^= va2;
			va4 ^= va3;

			uint64_t* bptr1 = &base[k+1];
			uint16_t* sb1 = reinterpret_cast<uint16_t *>(bptr1);
			uint16_t* sb2 = reinterpret_cast<uint16_t *>(bptr1)+1;
			uint16_t* sb3 = reinterpret_cast<uint16_t *>(bptr1)+2;
			uint16_t* sb4 = reinterpret_cast<uint16_t *>(bptr1)+3;

			uint16_t vb1 = replacement[*sb1];
			uint16_t vb2 = replacement[*sb2];
			uint16_t vb3 = replacement[*sb3];
			uint16_t vb4 = replacement[*sb4];

			vb1 ^= va4;
			vb2 ^= vb1;
			vb3 ^= vb2;
			vb4 ^= vb3;

			va1 ^= vb4;


			*sa1 = va1;
			*sa2 = va2;
			*sa3 = va3;
			*sa4 = va4;
			*sb1 = vb1;
			*sb2 = vb2;
			*sb3 = vb3;
			*sb4 = vb4;
		}
	}

	{
		Timer timer1("Time to substitute 8 shorts of a uint64 without shifting: ", duration);

		for (int k = 0; k < k64; ++k) {
			uint64_t* bptr = &base[k];
			uint16_t* s1 = reinterpret_cast<uint16_t *>(bptr);
			uint16_t* s2 = reinterpret_cast<uint16_t *>(bptr)+1;
			uint16_t* s3 = reinterpret_cast<uint16_t *>(bptr)+2;
			uint16_t* s4 = reinterpret_cast<uint16_t *>(bptr)+3;

			uint16_t v1 = replacement[*s1];
			uint16_t v2 = replacement[*s2];
			uint16_t v3 = replacement[*s3];
			uint16_t v4 = replacement[*s4];

			v2 ^= v1;
			v3 ^= v2;
			v4 ^= v3;
			v1 ^= v4;

			*s1 = v1;
			*s2 = v2;
			*s3 = v3;
			*s4 = v4;
		}
	}

	{
		Timer timer1("Time to substitute 8 shorts of a uint64: ", duration);

		for (int k = 0; k < k64; ++k) {
			uint64_t* bptr = &base[k];
			uint16_t* s1 = reinterpret_cast<uint16_t *>(bptr);
			uint16_t* s2 = reinterpret_cast<uint16_t *>(bptr)+1;
			uint16_t* s3 = reinterpret_cast<uint16_t *>(bptr)+2;
			uint16_t* s4 = reinterpret_cast<uint16_t *>(bptr)+3;

			uint16_t v1 = replacement[*s1];
			uint16_t v2 = replacement[*s2];
			uint16_t v3 = replacement[*s3];
			uint16_t v4 = replacement[*s4];

			v2 ^= v1;
			v3 ^= v2;
			v4 ^= v3;

			uint8_t lastByte = v4 & 0x00FF;
			v1 ^= (lastByte << 8);

			*s1 = v1;
			*s2 = v2;
			*s3 = v3;
			*s4 = v4;
		}
	}

	{
		Timer timer1("Time to substitute 8 shorts of a uint64 but as one uint64: ", duration);

		for (int k = 0; k < k64; ++k) {
			uint64_t* bptr = &base[k];
			uint16_t* s1 = reinterpret_cast<uint16_t *>(bptr);
			uint16_t* s2 = reinterpret_cast<uint16_t *>(bptr)+1;
			uint16_t* s3 = reinterpret_cast<uint16_t *>(bptr)+2;
			uint16_t* s4 = reinterpret_cast<uint16_t *>(bptr)+3;

			uint64_t v1 = replacement[*s1];
			uint64_t v2 = replacement[*s2];
			uint64_t v3 = replacement[*s3];
			uint64_t v4 = replacement[*s4];

			v2 ^= v1;
			v3 ^= v2;
			v4 ^= v3;

			uint8_t lastByte = v4 & 0x00FF;
			v1 ^= (lastByte << 8);

			uint64_t val = v1 | (v2 << 16) | (v3 << 32) | (v4 << 48);

			*bptr = val;
		}
	}

	{
		Timer timer1("Time to substitute 8 shorts of 8 bytes as a wave: ", duration);
		for (int l = 0; l < k64; l = l+8) {
			uint64_t* bptr1 = &base[l];
			uint64_t* bptr2 = &base[1+l];
			uint64_t* bptr3 = &base[2+l];
			uint64_t* bptr4 = &base[3+l];
			uint64_t* bptr5 = &base[4+l];
			uint64_t* bptr6 = &base[5+l];
			uint64_t* bptr7 = &base[6+l];
			uint64_t* bptr8 = &base[7+l];

			uint16_t* s1a = reinterpret_cast<uint16_t *>(bptr1);
			uint16_t* s1b = reinterpret_cast<uint16_t *>(bptr2);
			uint16_t* s1c = reinterpret_cast<uint16_t *>(bptr3);
			uint16_t* s1d = reinterpret_cast<uint16_t *>(bptr4);
			uint16_t* s1e = reinterpret_cast<uint16_t *>(bptr5);
			uint16_t* s1f = reinterpret_cast<uint16_t *>(bptr6);
			uint16_t* s1g = reinterpret_cast<uint16_t *>(bptr7);
			uint16_t* s1h = reinterpret_cast<uint16_t *>(bptr8);

			uint16_t v1a =  replacement[*s1a];
			uint16_t v1b =  replacement[*s1b];
			uint16_t v1c =  replacement[*s1c];
			uint16_t v1d =  replacement[*s1d];
			uint16_t v1e =  replacement[*s1e];
			uint16_t v1f =  replacement[*s1f];
			uint16_t v1g =  replacement[*s1g];
			uint16_t v1h =  replacement[*s1h];


			uint16_t* s2a = reinterpret_cast<uint16_t *>(bptr1)+1;
			uint16_t* s2b = reinterpret_cast<uint16_t *>(bptr2)+1;
			uint16_t* s2c = reinterpret_cast<uint16_t *>(bptr3)+1;
			uint16_t* s2d = reinterpret_cast<uint16_t *>(bptr4)+1;
			uint16_t* s2e = reinterpret_cast<uint16_t *>(bptr5)+1;
			uint16_t* s2f = reinterpret_cast<uint16_t *>(bptr6)+1;
			uint16_t* s2g = reinterpret_cast<uint16_t *>(bptr7)+1;
			uint16_t* s2h = reinterpret_cast<uint16_t *>(bptr8)+1;

			uint16_t v2a =  replacement[*s2a];
			uint16_t v2b =  replacement[*s2b];
			uint16_t v2c =  replacement[*s2c];
			uint16_t v2d =  replacement[*s2d];
			uint16_t v2e =  replacement[*s2e];
			uint16_t v2f =  replacement[*s2f];
			uint16_t v2g =  replacement[*s2g];
			uint16_t v2h =  replacement[*s2h];

			uint16_t* s3a = reinterpret_cast<uint16_t *>(bptr1)+2;
			uint16_t* s3b = reinterpret_cast<uint16_t *>(bptr2)+2;
			uint16_t* s3c = reinterpret_cast<uint16_t *>(bptr3)+2;
			uint16_t* s3d = reinterpret_cast<uint16_t *>(bptr4)+2;
			uint16_t* s3e = reinterpret_cast<uint16_t *>(bptr5)+2;
			uint16_t* s3f = reinterpret_cast<uint16_t *>(bptr6)+2;
			uint16_t* s3g = reinterpret_cast<uint16_t *>(bptr7)+2;
			uint16_t* s3h = reinterpret_cast<uint16_t *>(bptr8)+2;

			uint16_t v3a =  replacement[*s3a];
			uint16_t v3b =  replacement[*s3b];
			uint16_t v3c =  replacement[*s3c];
			uint16_t v3d =  replacement[*s3d];
			uint16_t v3e =  replacement[*s3e];
			uint16_t v3f =  replacement[*s3f];
			uint16_t v3g =  replacement[*s3g];
			uint16_t v3h =  replacement[*s3h];


			uint16_t* s4a = reinterpret_cast<uint16_t *>(bptr1)+3;
			uint16_t* s4b = reinterpret_cast<uint16_t *>(bptr2)+3;
			uint16_t* s4c = reinterpret_cast<uint16_t *>(bptr3)+3;
			uint16_t* s4d = reinterpret_cast<uint16_t *>(bptr4)+3;
			uint16_t* s4e = reinterpret_cast<uint16_t *>(bptr5)+3;
			uint16_t* s4f = reinterpret_cast<uint16_t *>(bptr6)+3;
			uint16_t* s4g = reinterpret_cast<uint16_t *>(bptr7)+3;
			uint16_t* s4h = reinterpret_cast<uint16_t *>(bptr8)+3;

			uint16_t v4a =  replacement[*s4a];
			uint16_t v4b =  replacement[*s4b];
			uint16_t v4c =  replacement[*s4c];
			uint16_t v4d =  replacement[*s4d];
			uint16_t v4e =  replacement[*s4e];
			uint16_t v4f =  replacement[*s4f];
			uint16_t v4g =  replacement[*s4g];
			uint16_t v4h =  replacement[*s4h];

			uint8_t lastByte1 = v4a & 0xFF;
			uint8_t lastByte2 = v4b & 0xFF;
			uint8_t lastByte3 = v4c & 0xFF;
			uint8_t lastByte4 = v4d & 0xFF;
			uint8_t lastByte5 = v4e & 0xFF;
			uint8_t lastByte6 = v4f & 0xFF;
			uint8_t lastByte7 = v4g & 0xFF;
			uint8_t lastByte8 = v4h & 0xFF;


			v2a ^= v1a;
			v2b ^= v1b;
			v2c ^= v1c;
			v2d ^= v1d;
			v2e ^= v1e;
			v2f ^= v1f;
			v2g ^= v1g;
			v2h ^= v1h;


			v3a ^= v2a;
			v3b ^= v2b;
			v3c ^= v2c;
			v3d ^= v2d;
			v3e ^= v2e;
			v3f ^= v2f;
			v3g ^= v2g;
			v3h ^= v2h;


			v4a ^= v3a;
			v4b ^= v3b;
			v4c ^= v3c;
			v4d ^= v3d;
			v4e ^= v3e;
			v4f ^= v3f;
			v4g ^= v3g;
			v4h ^= v3h;


			v1a ^= (lastByte1 << 8);
			v1b ^= (lastByte2 << 8);
			v1c ^= (lastByte3 << 8);
			v1d ^= (lastByte4 << 8);
			v1e ^= (lastByte5 << 8);
			v1f ^= (lastByte6 << 8);
			v1g ^= (lastByte7 << 8);
			v1h ^= (lastByte8 << 8);

			*s1a = v1a;	
			*s1b = v1b;	
			*s1c = v1c;	
			*s1d = v1d;	
			*s1e = v1e;	
			*s1f = v1f;	
			*s1g = v1g;	
			*s1h = v1h;	


			*s2a = v2a;	
			*s2b = v2b;	
			*s2c = v2c;	
			*s2d = v2d;	
			*s2e = v2e;	
			*s2f = v2f;	
			*s2g = v2g;	
			*s2h = v2h;	


			*s3a = v3a;	
			*s3b = v3b;	
			*s3c = v3c;	
			*s3d = v3d;	
			*s3e = v3e;	
			*s3f = v3f;	
			*s3g = v3g;	
			*s3h = v3h;	


			*s4a = v4a;	
			*s4b = v4b;	
			*s4c = v4c;	
			*s4d = v4d;	
			*s4e = v4e;	
			*s4f = v4f;	
			*s4g = v4g;	
			*s4h = v4h;	
		}
	}

	{
		Timer timer1("Time to substitute 8 shorts of 8 bytes as a wave but storing uint64 together: ", duration);
		for (int l = 0; l < k64; l = l+8) {
			uint64_t* bptr1 = &base[l];
			uint64_t* bptr2 = &base[1+l];
			uint64_t* bptr3 = &base[2+l];
			uint64_t* bptr4 = &base[3+l];
			uint64_t* bptr5 = &base[4+l];
			uint64_t* bptr6 = &base[5+l];
			uint64_t* bptr7 = &base[6+l];
			uint64_t* bptr8 = &base[7+l];

			uint16_t* s1a = reinterpret_cast<uint16_t *>(bptr1);
			uint16_t* s1b = reinterpret_cast<uint16_t *>(bptr2);
			uint16_t* s1c = reinterpret_cast<uint16_t *>(bptr3);
			uint16_t* s1d = reinterpret_cast<uint16_t *>(bptr4);
			uint16_t* s1e = reinterpret_cast<uint16_t *>(bptr5);
			uint16_t* s1f = reinterpret_cast<uint16_t *>(bptr6);
			uint16_t* s1g = reinterpret_cast<uint16_t *>(bptr7);
			uint16_t* s1h = reinterpret_cast<uint16_t *>(bptr8);

			uint16_t v1a =  replacement[*s1a];
			uint16_t v1b =  replacement[*s1b];
			uint16_t v1c =  replacement[*s1c];
			uint16_t v1d =  replacement[*s1d];
			uint16_t v1e =  replacement[*s1e];
			uint16_t v1f =  replacement[*s1f];
			uint16_t v1g =  replacement[*s1g];
			uint16_t v1h =  replacement[*s1h];


			uint16_t* s2a = reinterpret_cast<uint16_t *>(bptr1)+1;
			uint16_t* s2b = reinterpret_cast<uint16_t *>(bptr2)+1;
			uint16_t* s2c = reinterpret_cast<uint16_t *>(bptr3)+1;
			uint16_t* s2d = reinterpret_cast<uint16_t *>(bptr4)+1;
			uint16_t* s2e = reinterpret_cast<uint16_t *>(bptr5)+1;
			uint16_t* s2f = reinterpret_cast<uint16_t *>(bptr6)+1;
			uint16_t* s2g = reinterpret_cast<uint16_t *>(bptr7)+1;
			uint16_t* s2h = reinterpret_cast<uint16_t *>(bptr8)+1;

			uint16_t v2a =  replacement[*s2a];
			uint16_t v2b =  replacement[*s2b];
			uint16_t v2c =  replacement[*s2c];
			uint16_t v2d =  replacement[*s2d];
			uint16_t v2e =  replacement[*s2e];
			uint16_t v2f =  replacement[*s2f];
			uint16_t v2g =  replacement[*s2g];
			uint16_t v2h =  replacement[*s2h];

			uint16_t* s3a = reinterpret_cast<uint16_t *>(bptr1)+2;
			uint16_t* s3b = reinterpret_cast<uint16_t *>(bptr2)+2;
			uint16_t* s3c = reinterpret_cast<uint16_t *>(bptr3)+2;
			uint16_t* s3d = reinterpret_cast<uint16_t *>(bptr4)+2;
			uint16_t* s3e = reinterpret_cast<uint16_t *>(bptr5)+2;
			uint16_t* s3f = reinterpret_cast<uint16_t *>(bptr6)+2;
			uint16_t* s3g = reinterpret_cast<uint16_t *>(bptr7)+2;
			uint16_t* s3h = reinterpret_cast<uint16_t *>(bptr8)+2;

			uint16_t v3a =  replacement[*s3a];
			uint16_t v3b =  replacement[*s3b];
			uint16_t v3c =  replacement[*s3c];
			uint16_t v3d =  replacement[*s3d];
			uint16_t v3e =  replacement[*s3e];
			uint16_t v3f =  replacement[*s3f];
			uint16_t v3g =  replacement[*s3g];
			uint16_t v3h =  replacement[*s3h];


			uint16_t* s4a = reinterpret_cast<uint16_t *>(bptr1)+3;
			uint16_t* s4b = reinterpret_cast<uint16_t *>(bptr2)+3;
			uint16_t* s4c = reinterpret_cast<uint16_t *>(bptr3)+3;
			uint16_t* s4d = reinterpret_cast<uint16_t *>(bptr4)+3;
			uint16_t* s4e = reinterpret_cast<uint16_t *>(bptr5)+3;
			uint16_t* s4f = reinterpret_cast<uint16_t *>(bptr6)+3;
			uint16_t* s4g = reinterpret_cast<uint16_t *>(bptr7)+3;
			uint16_t* s4h = reinterpret_cast<uint16_t *>(bptr8)+3;

			uint16_t v4a =  replacement[*s4a];
			uint16_t v4b =  replacement[*s4b];
			uint16_t v4c =  replacement[*s4c];
			uint16_t v4d =  replacement[*s4d];
			uint16_t v4e =  replacement[*s4e];
			uint16_t v4f =  replacement[*s4f];
			uint16_t v4g =  replacement[*s4g];
			uint16_t v4h =  replacement[*s4h];

			uint8_t lastByte1 = v4a & 0xFF;
			uint8_t lastByte2 = v4b & 0xFF;
			uint8_t lastByte3 = v4c & 0xFF;
			uint8_t lastByte4 = v4d & 0xFF;
			uint8_t lastByte5 = v4e & 0xFF;
			uint8_t lastByte6 = v4f & 0xFF;
			uint8_t lastByte7 = v4g & 0xFF;
			uint8_t lastByte8 = v4h & 0xFF;

			v1a ^= (lastByte1 << 8);
			v1b ^= (lastByte2 << 8);
			v1c ^= (lastByte3 << 8);
			v1d ^= (lastByte4 << 8);
			v1e ^= (lastByte5 << 8);
			v1f ^= (lastByte6 << 8);
			v1g ^= (lastByte7 << 8);
			v1h ^= (lastByte8 << 8);


			v2a ^= v1a;
			v2b ^= v1b;
			v2c ^= v1c;
			v2d ^= v1d;
			v2e ^= v1e;
			v2f ^= v1f;
			v2g ^= v1g;
			v2h ^= v1h;


			v3a ^= v2a;
			v3b ^= v2b;
			v3c ^= v2c;
			v3d ^= v2d;
			v3e ^= v2e;
			v3f ^= v2f;
			v3g ^= v2g;
			v3h ^= v2h;


			v4a ^= v3a;
			v4b ^= v3b;
			v4c ^= v3c;
			v4d ^= v3d;
			v4e ^= v3e;
			v4f ^= v3f;
			v4g ^= v3g;
			v4h ^= v3h;




			*s1a = v1a;	
			*s2a = v2a;	
			*s3a = v3a;	
			*s4a = v4a;	


			*s1b = v1b;	
			*s2b = v2b;	
			*s3b = v3b;	
			*s4b = v4b;	


			*s1c = v1c;	
			*s2c = v2c;	
			*s3c = v3c;	
			*s4c = v4c;	


			*s1d = v1d;	
			*s2d = v2d;	
			*s3d = v3d;	
			*s4d = v4d;	


			*s1e = v1e;	
			*s2e = v2e;	
			*s3e = v3e;	
			*s4e = v4e;	


			*s1f = v1f;	
			*s2f = v2f;	
			*s3f = v3f;	
			*s4f = v4f;	


			*s1g = v1g;	
			*s2g = v2g;	
			*s3g = v3g;	
			*s4g = v4g;	


			*s1h = v1h;	
			*s2h = v2h;	
			*s3h = v3h;	
			*s4h = v4h;	
		}
	}
	return 0;
}
