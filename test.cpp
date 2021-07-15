#include <iostream>
#include <string>

using namespace std;

#define myVar_impl(a, ...) a
#define myVar(sec, ...) myVar_impl(sec*4, __VA_ARGS__)
#define createV_impl(sec) a ## sec
#define createV(...) createV_impl(__VA_ARGS__)

int main() {
	int createV(myVar(1)) = 5;
	a4 = 10;
	
	return 0;
}
