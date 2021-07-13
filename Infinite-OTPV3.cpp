/* The following macros help create variables to short each short in 4K input. portionN is the 1024-bit portion number, sectionN is the 
 * 128-bit section number within the given portion, and shortN is the short position within the given section. All these variables must 
 * be 0-indexed.
 */
#define S(portionN, sectionN, shortN) bptr+(portionN*1024/sizeof(uint16_t))+(sectionN*128/sizeof(uint16_t))+shortN 
#define V(portionN, sectionN, shortN) v_portionN_sectionN_shortN 
#define createV(portionN, sectionN, shortN) uint16_t V(portionN, sectionN, shortN) = substitutionTable[*S(portionN, sectionN, shortN)]


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
#define SWAP_SHORTS(a, b)			\
				do {						\
					uint16_t temp = a;			\
					a = b;					\
					b = temp;				\
				} while(0)
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

				vh8 = vh8;
			}
		}

		*SA(0) = va1;
		*SA(1) = va2;
		*SA(2) = va3;
		*SA(3) = va4;
		*SA(4) = va5;
		*SA(5) = va6;
		*SA(6) = va7;
		*SA(7) = va8;

		*SB(0) = vb1;
		*SB(1) = vb2;
		*SB(2) = vb3;
		*SB(3) = vb4;
		*SB(4) = vb5;
		*SB(5) = vb6;
		*SB(6) = vb7;
		*SB(7) = vb8;

		*SC(0) = vc1;
		*SC(1) = vc2;
		*SC(2) = vc3;
		*SC(3) = vc4;
		*SC(4) = vc5;
		*SC(5) = vc6;
		*SC(6) = vc7;
		*SC(7) = vc8;

		*SD(0) = vd1;
		*SD(1) = vd2;
		*SD(2) = vd3;
		*SD(3) = vd4;
		*SD(4) = vd5;
		*SD(5) = vd6;
		*SD(6) = vd7;
		*SD(7) = vd8;

		*SE(0) = ve1;
		*SE(1) = ve2;
		*SE(2) = ve3;
		*SE(3) = ve4;
		*SE(4) = ve5;
		*SE(5) = ve6;
		*SE(6) = ve7;
		*SE(7) = ve8;

		*SF(0) = vf1;
		*SF(1) = vf2;
		*SF(2) = vf3;
		*SF(3) = vf4;
		*SF(4) = vf5;
		*SF(5) = vf6;
		*SF(6) = vf7;
		*SF(7) = vf8;

		*SG(0) = vg1;
		*SG(1) = vg2;
		*SG(2) = vg3;
		*SG(3) = vg4;
		*SG(4) = vg5;
		*SG(5) = vg6;
		*SG(6) = vg7;
		*SG(7) = vg8;

		*SH(0) = vh1;
		*SH(1) = vh2;
		*SH(2) = vh3;
		*SH(3) = vh4;
		*SH(4) = vh5;
		*SH(5) = vh6;
		*SH(6) = vh7;
		*SH(7) = vh8;

		//cout << "Encrypted " << endl;
		//printVector(input, 56, 65);
	}
}
