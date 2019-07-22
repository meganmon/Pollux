#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include "utils.h"
#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <algorithm>
#include "snap7/s7.h"
using namespace std;

/*set up for SIEMENS SERVER*/
extern S7Object Client;
extern vector <float> real;
extern const char *Address;     // PLC IP Address
extern int Rack, Slot;


int assignTagOffset(string type);
bool Check(int Result, const char * function);
bool CliConnect();
void ListBlocks();
void readSiemensType(string type, int offset, int Bit, byte data[]);
void writeBoolAt(int db, int startbyte, int startbit, bool value);
void writeIntAt(int db, int start, int value);
void writeRealAt(int db, int start, float value);
void writeStringAt(int db, int start, string value);
