#ifndef sha256
#define sha256



#include<string>
#include "utils.h"
using namespace std;
string shapiece(const string& filename, int piece_number);
string computeHash(string& filePath);

#endif



