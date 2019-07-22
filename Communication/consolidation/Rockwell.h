#pragma once
#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include "libplctag/libplctag.h"
#include "utils.h"
#include <iostream>
#include <string>
#include <fstream>
#include "rapidxml/rapidxml.hpp"
#include <vector>
#include <algorithm>
using namespace std;



/*define sizes*/
#define BOOL_SIZE 1
#define STRING_SIZE 88
#define ELEM_SIZE 4
#define INT_SIZE 4
#define FLOAT_SIZE 4
#define STRING_DATA_SIZE 82

/*define number of elements in arrays*/
#define ELEM_COUNT 1  //assumes we are not reading an array
#define DATA_TIMEOUT 5000


extern vector <string> tagTypes;
extern vector <string> tagNames;
extern vector <int32_t> myTags;

string assignTagSize(string type);
int32_t create_tagR(const char *path);
int read_intR(int32_t tag);
float read_realR(int32_t tag);
bool read_boolR(int32_t tag);
string read_stringR(int32_t tag);
string readRockwellType(int32_t tag, string type);
void update_stringR(int32_t tag, int i, string STR);
void update_floatR(int32_t tag, float f);
void update_intR(int32_t tag, int x);
void toggle_boolR(int32_t tag);
void destroyRockTags();

