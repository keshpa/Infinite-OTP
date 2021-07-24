#include <assert.h>
#include <chrono>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <openssl/evp.h>
#include <openssl/aes.h>

using namespace std;

constexpr int32_t k4 = 1024*4;
constexpr int32_t m1 = 1024*1024;

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

/**
 *  * Create a 256 bit key and IV using the supplied key_data. salt can be added for taste.
 *   * Fills in the encryption and decryption ctx objects and returns 0 on success
 *    **/
int32_t aes_init(unsigned char *key_data, int32_t key_data_len, unsigned char *salt, EVP_CIPHER_CTX *e_ctx, 
		EVP_CIPHER_CTX *d_ctx)
{
	int32_t i, nrounds = 5;
	unsigned char key[32], iv[32];

	/*
	 *    * Gen key & IV for AES 256 CBC mode. A SHA1 digest is used to hash the supplied key material.
	 *       * nrounds is the number of times the we hash the material. More rounds are more secure but
	 *          * slower.
	 *             */
	i = EVP_BytesToKey(EVP_aes_256_cbc(), EVP_sha1(), salt, key_data, key_data_len, nrounds, key, iv);
	if (i != 32) {
		printf("Key size is %d bits - should be 256 bits\n", i);
		return -1;
	}

	EVP_CIPHER_CTX_init(e_ctx);
	EVP_EncryptInit_ex(e_ctx, EVP_aes_256_cbc(), NULL, key, iv);
	EVP_CIPHER_CTX_init(d_ctx);
	EVP_DecryptInit_ex(d_ctx, EVP_aes_256_cbc(), NULL, key, iv);

	return 0;
}

/*
 *  * Encrypt *len bytes of data
 *   * All data going in & out is considered binary (unsigned char[])
 *    */
void aes_encrypt(EVP_CIPHER_CTX *e, unsigned char *plaintext, int32_t *len)
{
	/* max ciphertext len for a n bytes of plaintext is n + AES_BLOCK_SIZE -1 bytes */
	int32_t c_len = *len + AES_BLOCK_SIZE, f_len = 0;
	uint8_t ciphertext[k4];

	/* allows reusing of 'e' for multiple encryption cycles */
	EVP_EncryptInit_ex(e, NULL, NULL, NULL, NULL);

	/* update ciphertext, c_len is filled with the length of ciphertext generated,
	 *     *len is the size of plaintext in bytes */
	EVP_EncryptUpdate(e, ciphertext, &c_len, plaintext, *len);

	/* update ciphertext with the final remaining bytes */
	EVP_EncryptFinal_ex(e, ciphertext+c_len, &f_len);
}

/*
 *  * Decrypt *len bytes of ciphertext
 *   */
void aes_decrypt(EVP_CIPHER_CTX *e, unsigned char *ciphertext, int32_t *len)
{
	/* plaintext will always be equal to or lesser than length of ciphertext*/
	int32_t p_len = *len, f_len = 0;
	uint8_t plaintext[k4];

	EVP_DecryptInit_ex(e, NULL, NULL, NULL, NULL);
	EVP_DecryptUpdate(e, plaintext, &p_len, ciphertext, *len);
	EVP_DecryptFinal_ex(e, plaintext+p_len, &f_len);
}

int main(int32_t argc, char **argv)
{
	/* "opaque" encryption, decryption ctx structures that libcrypto uses to record
	 *      status of enc/dec operations */
	EVP_CIPHER_CTX en, de;

	/* 8 bytes to salt the key_data during key generation. This is an example of
	 *      compiled in salt. We just read the bit pattern created by these two 4 byte 
	 *           integers on the stack as 64 bits of contigous salt material - 
	 *                ofcourse this only works if sizeof(int) >= 4 */
	int32_t salt[] = {12345, 54321};
	unsigned char *key_data;
	int32_t key_data_len, i;
	vector<uint8_t> ip;
	srand(0);

	ip.resize(k4);
	int32_t len = k4;
	for (int32_t i = 0; i < k4; ++i) {
		ip[i] = (uint8_t)rand();
	}

	key_data = (unsigned char *)(malloc(32));
	for (int32_t i = 0; i < 32; ++i) {
		key_data[i] = (unsigned char)rand();
	}
	key_data_len = 32;

	/* gen key and iv. init the cipher ctx object */
	if (aes_init(key_data, key_data_len, (unsigned char *)&salt, &en, &de)) {
		printf("Couldn't initialize AES cipher\n");
		return -1;
	}

	chrono::duration<double, nano> duration;
	{
		Timer timer1("Time taken to encrypt 4K chunk, a million times: ", duration);
		for (int32_t i = 0; i < m1; i++) {

			aes_encrypt(&en, (unsigned char *)ip.data(), &len);
			//plaintext = (char *)aes_decrypt(&de, ciphertext, &len);
		}
	}

	EVP_CIPHER_CTX_cleanup(&en);
	EVP_CIPHER_CTX_cleanup(&de);

	return 0;
}
