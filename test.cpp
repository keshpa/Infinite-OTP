#include <iostream>
#include <string>
#include <boost/preprocessor/arithmetic/mul.hpp>

using namespace std;

#define myVar(var1, ...)	BOOST_PP_MUL(var1, 4)
#define createV(var1)		createV_(var1)
#define createV_(var1)		a ## var1

int main() {
	int createV(BOOST_PP_MUL(1, 4)) = 5;
	int b = BOOST_PP_MUL(1, 4);
	a4 = 10;
	
	return 0;
}
