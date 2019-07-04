#ifdef OS_WINDOWS
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif
#include <WinSock2.h>
#include <stdio.h>
#include <stdlib.h>
#include "../lib/libplctag.h"
#include "utils.h"
#include <iostream>
#include <string>
//#include "s7_client.h"
//#include "s7_text.h"
#include <fstream>
#include "rapidxml.hpp"
#include <vector>
#include <algorithm>
//#include "snap7.h"
#include <s7.h>
using namespace rapidxml;
using namespace std;

string Make;
string Model;
string IP_address;

#define NOTREAD 0
#define ROCKWELL 1
#define SIEMENS300 2
#define SIEMENS1500 3
#define ERROR 4
int state;

/*set up for Postgressql*/
//the libraries and set up for the Postgresql
#include "libpq-fe.h"
PGconn *dbconn;
PGconn *newconn;
PGresult *query;
PGresult *res;
const char *conninfo = "host=localhost port=5432 user=postgres password=Pollux123";
/*end set up for Postgressql*/

//-.-.-.-.-.-.-..--..-.-.-.-.-.-.-.-..-.-.-.-.-..-.-.-.-.-.-.-..-.-.-.-.-.-.-.-.-.-.-..-

/*set up for SIEMENS SERVER*/
S7Object Client;
//TSnap7Client *Client;

byte Buffer[65536]; // 64 K buffer
int SampleDBNum = 1000;

const char *Address;     // PLC IP Address
						 //const char * Address;
int Rack, Slot; // Default Rack and Slot

int ok = 0; // Number of test pass
int ko = 0; // Number of test failure

bool JobDone = false;
int JobResult = 0;
//struct ComponentResult {
//	string SerialNumber;
//	int TestInt;
//	double TestReal;
//};

void S7API CliCompletion(void *usrPtr, int opCode, int opResult)
{
	JobResult = opResult;
	JobDone = true;
}

#ifndef HEXDUMP_COLS
#define HEXDUMP_COLS 16
#endif
void hexdump(void *mem, unsigned int len)
{
	unsigned int i, j;

	for (i = 0; i < len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++)
	{
		/* print offset */
		if (i % HEXDUMP_COLS == 0)
		{
			printf("0x%04x: ", i);
		}

		/* print hex data */
		if (i < len)
		{

			printf("%02x ", 0xFF & ((char*)mem)[i], "\n");
		}
		else /* end of block, just aligning for ASCII dump */
		{
			printf("   ");
		}

		/* print ASCII dump */
		//if (i % HEXDUMP_COLS == (HEXDUMP_COLS - 1))
		//{
		//	for (j = i - (HEXDUMP_COLS - 1); j <= i; j++)
		//	{
		//		if (j >= len) /* end of block, not really printing */
		//		{
		//			putchar(' ');
		//		}
		//		else if (isprint((((char*)mem)[j] & 0x7F))) /* printable char */
		//		{
		//			putchar(0xFF & ((char*)mem)[j]);
		//		}
		//		else /* other char */
		//		{
		//			putchar('.');
		//		}
		//	}
		//	putchar('\n');
		//}
	}
}
int hexToInt(void *db, int len)
{
	unsigned int i, j;
	int returnVal = 0;
	int base = 1;
	for (i = 0; i < len + ((len % HEXDUMP_COLS) ? (HEXDUMP_COLS - len % HEXDUMP_COLS) : 0); i++)
	{
		/* print offset */
		if (i % HEXDUMP_COLS == 0)
		{
			//printf("0x%04x: ", i);
		}

		/* print hex data */
		if (i < len)
		{
			int ones = ((0xFF & ((char*)db)[len - 1 - i]) % (0x10));
			returnVal += base * ones;
			//cout << "ones: " << ones << "\n";
			base *= 16;
			int tens = (0xFF & (((char*)db)[len - 1 - i] >> 4)) % (0x10);

			//cout << "tens: " << tens << "\n";
			returnVal += base * tens;
			base *= 16;
		}
	}
	return returnVal;
}
bool intToBool(int num, int pos) {
	bool temp[8];
	int toBool = num;
	for (int i = 0; i < 8; i++) {
		temp[i] = toBool % 2;
		//cout << temp[i] << "\n";
		toBool /= 2;
	}
	return temp[pos];
}
// Check error
bool Check(int Result, const char * function)
{
	//printf("\n");
	//printf("+-----------------------------------------------------\n");
	//printf("| %s\n", function);
	//printf("+-----------------------------------------------------\n");
	if (Result == 0) {
		//printf("| Result         : OK\n");
		//printf("| Execution time : %d ms\n", Client->Time());
		//printf("+-----------------------------------------------------\n");
		ok++;
	}
	else {
		printf("| ERROR !!! \n");
		if (Result < 0)
			printf("| Library Error (-1)\n");
		else
			cout << "Error number: " << Result << "\n";
		//		printf("| %s\n", ErrCliText(Result).c_str());						//FIGURE OUT WHAT THIS DOES!!!!!!!!!! or rather is supposed to do
		printf("+-----------------------------------------------------\n");
		ko++;
	}
	return Result == 0;
}
void ReadDB()
{
	byte db[2];
	int res = Cli_DBRead(Client, 1, 2, 2, &db);
	if (Check(res, "ReadDB")) {
		cout << &db << "\n";
		cout << hexToInt(&db, 1);
	}
}

int getIntAt(int db, int start) {
	byte data[2];
	int out;
	int res = Cli_DBRead(Client, db, start, 2, &data);
	if (Check(res, "getIntAt")) {
		out = hexToInt(&data, 2);
	}
	return out;
}
int getRealAt(int db, int start) {
	byte data[4];
	int out = 0;
	int res = Cli_DBRead(Client, db, start, 4, &data);
	if (Check(res, "getRealAt")) {
		cout << "data: " << data << "\n";
		hexdump(&data, 4);
		//		out = hexToInt(&data, 4);
	}
	return out;
}
//bool getBoolAt(int db, int startByte, int startBit) {
//	byte data;
//	int temp;
//	bool out = 0;
//	
//	int res = Client->DBRead(db, startByte, 1, &data);
//	if (Check(res, "getBoolAt")) {
//		temp = hexToInt(&data,1);
//		out = intToBool(temp, startBit);
//	}
//	return out;
//}

// PLC Status
void UnitStatus()
{
	//int res = 0;
	//int Status;
	//res = Client->PlcStatus();
	//if (Check(res, "CPU Status"))
	//{
	//	switch (Status)
	//	{
	//	case S7CpuStatusRun: printf("  RUN\n"); break;
	//	case S7CpuStatusStop: printf("  STOP\n"); break;
	//	default: printf("  UNKNOWN\n"); break;
	//	}
	//};
}
// Unit Connection
bool CliConnect()
{
	int res = Cli_ConnectTo(Client, Address, Rack, Slot);
	if (Check(res, "UNIT Connection")) {
		//printf("  Connected to   : %s (Rack=%d, Slot=%d)\n", Address, Rack, Slot);
		//printf("  PDU Requested  : %d bytes\n", Client->PDURequest);
		//printf("  PDU Negotiated : %d bytes\n", Client->PDULength);
	};
	return res == 0;
}

// Perform readonly tests, no cpu status modification
void PerformTests()
{/*
 OrderCode();
 CpuInfo();
 CpInfo();
 UnitStatus();
 ReadSzl_0011_0000();
 UploadDB0();
 AsCBUploadDB0();
 AsEWUploadDB0();
 AsPOUploadDB0();
 ListBlocks();
 MultiRead();
 Up_DownloadFC1();*/
}
// Tests Summary
void Summary()
{
	printf("\n");
	printf("+-----------------------------------------------------\n");
	printf("| Test Summary \n");
	printf("+-----------------------------------------------------\n");
	printf("| Performed : %d\n", (ok + ko));
	printf("| Passed    : %d\n", ok);
	printf("| Failed    : %d\n", ko);
	printf("+----------------------------------------[press a key]\n");
	getchar();
}
/*end set up for SIEMENS SERVER*/

//-.-.-.-.-.-.-.--.-.-.-.--.-.--.-.-.-.-..-.-..-.-.-.-.-.-..-.--.-.-.-.-.-.-.-.-.-.-.-.-.-..-.-.-.-.-.-.-..-.-.-.-.-

/*set up for ROCKWELL SERVER*/
/* define tag paths*/

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
/*set up variables for tags*/
int32_t d_tag = 0;
int32_t int_tag = 0;
int32_t str_tag = 0;
int32_t bool_tag = 0;
int32_t float_tag = 0;
/*shorter way to destroy all tags*/
void destroyRockTags() {
	plc_tag_destroy(d_tag);
	plc_tag_destroy(bool_tag);
	plc_tag_destroy(int_tag);
	plc_tag_destroy(str_tag);
	plc_tag_destroy(float_tag);
}
int32_t create_tagR(const char *path)
{
	int rc = PLCTAG_STATUS_OK;
	int32_t tag = 0;
	tag = plc_tag_create(path, DATA_TIMEOUT);
	if (tag < 0) {
		fprintf(stderr, "ERROR %s: Could not create tag!\n", plc_tag_decode_error(tag));
		return tag;
	}
	if ((rc = plc_tag_status(tag)) != PLCTAG_STATUS_OK) {
		fprintf(stdout, "Error setting up tag internal state.\n");
		return rc;
	}
	return tag;
}
/*FOR READING TAGS: currently elem count is set to 1 element, but if capability to read an array for each tag has been kept*/
int read_intR(int32_t tag) {
	int rc = plc_tag_read(tag, DATA_TIMEOUT);
	if (rc != PLCTAG_STATUS_OK) {
		fprintf(stderr, "ERROR: Unable to read the data! Got error code %d: %s\n", rc, plc_tag_decode_error(rc));
		plc_tag_destroy(tag);
		return 0;
	}
	int i;
	int out[ELEM_COUNT];
	for (i = 0; i < ELEM_COUNT; i++) {
		out[i] = plc_tag_get_int32(tag, (i*ELEM_SIZE));
	}
	return out[0];
}
float read_realR(int32_t tag) {
	int rc = plc_tag_read(tag, DATA_TIMEOUT);
	if (rc != PLCTAG_STATUS_OK) {
		fprintf(stderr, "ERROR: Unable to read the data! Got error code %d: %s\n", rc, plc_tag_decode_error(rc));
		plc_tag_destroy(tag);
		return 0;
	}
	int i;
	float out[ELEM_COUNT];
	for (i = 0; i < ELEM_COUNT; i++) {
		out[i] = plc_tag_get_float32(tag, (i*ELEM_SIZE));
	}
	return out[0];
}
bool read_boolR(int32_t tag) {
	int rc = plc_tag_read(tag, DATA_TIMEOUT);
	if (rc != PLCTAG_STATUS_OK) {
		fprintf(stderr, "ERROR: Unable to read the data! Got error code %d: %s\n", rc, plc_tag_decode_error(rc));
		plc_tag_destroy(tag);
		return 0;
	}
	return plc_tag_get_uint8(bool_tag, 0);
}
string read_stringR(int32_t tag) {
	int rc = plc_tag_read(tag, DATA_TIMEOUT);
	if (rc != PLCTAG_STATUS_OK) {
		fprintf(stderr, "ERROR: Unable to read the data! Got error code %d: %s\n", rc, plc_tag_decode_error(rc));
		plc_tag_destroy(tag);
		return "ERROR";
	}
	int i;
	string strings[ELEM_COUNT];
	for (i = 0; i < ELEM_COUNT; i++) {
		int str_size = plc_tag_get_int32(tag, (i*STRING_SIZE));
		char str[STRING_SIZE] = { 0 };
		int j;
		for (j = 0; j<str_size; j++) {
			str[j] = (char)plc_tag_get_uint8(tag, (i*STRING_SIZE) + j + 4);
		}
		str[j] = (char)0;
		string s(str);
		strings[i] = s;
	}
	return strings[ELEM_COUNT - 1];
}
void update_stringR(int32_t tag, int i, string STR)
{
	int str_len;
	int base_offset = i * ELEM_SIZE;
	int wait;
	int str_index;
	char *str = new char[STR.size() + 1];
	strcpy(str, STR.c_str());
	/* now write the data */
	str_len = (int)strlen(str);
	/* set the length */
	plc_tag_set_int32(tag, base_offset, str_len);
	/* copy the data */
	for (str_index = 0; str_index < str_len && str_index < STRING_DATA_SIZE; str_index++) {
		plc_tag_set_uint8(tag, base_offset + 4 + str_index, (uint8_t)str[str_index]);
	}
	/* pad with zeros */
	for (; str_index<STRING_DATA_SIZE; str_index++) {
		plc_tag_set_uint8(tag, base_offset + 4 + str_index, 0);
	}
	plc_tag_write(tag, DATA_TIMEOUT);
}
void update_floatR(int32_t tag, float f) {
	int i;
	for (i = 0; i < ELEM_COUNT; i++) {
		float val = f;
		plc_tag_set_float32(tag, (i*ELEM_SIZE), val);
	}
	plc_tag_write(tag, DATA_TIMEOUT);
}
void update_intR(int32_t tag, int x) {
	int i;
	for (i = 0; i < ELEM_COUNT; i++) {
		int val = x;
		plc_tag_set_int32(tag, (i*ELEM_SIZE), val);
	}
	plc_tag_write(tag, DATA_TIMEOUT);
}
void toggle_boolR(int32_t tag) {
	bool b = read_boolR(tag);
	if (b) { plc_tag_set_uint8(bool_tag, 0, 0); }
	else { plc_tag_set_uint8(bool_tag, 0, 1); }
	int rc = plc_tag_write(bool_tag, DATA_TIMEOUT);
}

/*end set up for ROCKWELL SERVER*/
//-.-.-.-.-.-.-.-.-.-..-.-.-.-.-.-.-.-.-..-.-.-.-.-.-.-..-.-.-.-.-.-.--.-..-.-.-.-.-.-.-.-.-..--.-.-.

int main()
{
	//database name that is to be created
	string newDB = "test3";
	dbconn = PQconnectdb(conninfo);
	if (PQstatus(dbconn) == CONNECTION_BAD) {
		printf("Unable to connect to database\n");
		Sleep(3000); //to read console
	}
	printf("Currently connected to: %s \n", PQdb(dbconn));  //what db we are currently connected to
															//disconnect from default
	PQfinish(dbconn);
	//end database area

	/*READ THE XML Doc*/
	xml_document<> doc;
	xml_node<> * root_node;
	ifstream myfile("TEST_XML.xml");
	if (myfile.fail()) {
		printf("cannot find file");
		getchar();
		return 0;
	}
	vector<char> buffer((istreambuf_iterator<char>(myfile)), istreambuf_iterator<char>());
	buffer.push_back('\0');
	// Parse the buffer using the xml file parsing library into doc 
	doc.parse<0>(&buffer[0]);
	// Find our root node
	root_node = doc.first_node("ROOT");
	xml_node<> * plcs_node = root_node->first_node("PLCs");
	xml_node<> * plc_node = plcs_node->first_node("PLC");
	Make = plc_node->first_attribute("Type")->value();
	cout << "Make: " << Make << "\n";
	Model = plc_node->first_attribute("Model")->value();
	cout << "Model: " << Model << "\n";
	IP_address = plc_node->first_attribute("IPAddress")->value();
	cout << "IP Address: " << IP_address << "\n";

	transform(Make.begin(), Make.end(), Make.begin(), ::toupper);
	if (Make == "ROCKWELL") {
		cout << "Model recognized as Rockwell\n";
		state = 1;
	}
	if (Make == "SIEMENS") {
		if (Model == "300") {
			state = 2;
		}
		else {
			state = 2;
		}
	}
	switch (state) {
	default:
	{
		cout << "No Recognized PLC Models found in XML document";
		getchar();
		break;
	}
	case ROCKWELL:
	{
		cout << "Case Rockwell";
		/*protocol for a PATH: protocol stays constant, IP for machine, path should stay constant, LGX stays constant, elem size is size of element, count is size of array in tag to read (if array, currently only reading one number for tag), name is name of the tag*/
		const char * DINT_PATH = ("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=4&elem_count=1&name=DINT_Test").c_str();
		printf(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=4&elem_count=1&name=DINT_Test").c_str());
		/*example paths:
		const char * INT_PATH = "protocol=ab_eip&gateway=10.0.0.1&path=1,0&cpu=LGX&elem_size=4&elem_count=1&name=INT_Test";
		const char * STRING_PATH = "protocol=ab_eip&gateway=10.0.0.1&path=1,0&cpu=LGX&elem_size=88&elem_count=1&name=STRING_Test";
		const char * BOOL_PATH = "protocol=ab_eip&gateway=10.0.0.1&path=1,0&cpu=LGX&elem_size=1&elem_count=1&name=BOOL_Test";
		const char * FLOAT_PATH = "protocol=ab_eip&gateway=10.0.0.1&path=1,0&cpu=LGX&elem_size=4&elem_count=1&name=FLOAT_Test";
		*/
		/*case ROCKWELL*/
		bool b;
		int x;
		int p;
		string s;
		float f;

		/* create the tag */

		str_tag = create_tagR(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=88&elem_count=1&name=STRING_Test").c_str());
		d_tag = create_tagR(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=4&elem_count=1&name=DINT_Test").c_str());
		int_tag = create_tagR(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=4&elem_count=1&name=INT_Test").c_str());
		bool_tag = create_tagR(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=1&elem_count=1&name=BOOL_Test").c_str());
		float_tag = create_tagR(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=4&elem_count=1&name=FLOAT_Test").c_str());


		/* print out the integer data */
		printf("DINT_Test: %d\n", read_intR(d_tag));
		printf("INT_Test: %d\n", read_intR(int_tag));
		/*print out float data*/
		printf("FLOAT_Test: %f\n", read_realR(float_tag));

		/* print out the string data */
		string str;
		str = read_stringR(str_tag);
		cout << "STRING_TEST = " << str << "\n";

		/*print out the boolean*/
		b = read_boolR(bool_tag);
		fprintf(stderr, "BOOL_TEST = %d\n", b);

		if (b) { printf("yeet\n"); }

		printf("Input new dint for DINT_Test:\n");
		cin >> x;
		printf("Input new in for INT_Test:\n");
		cin >> p;
		printf("Input new string for STRING_Test:\n");
		cin >> s;
		printf("Input new float for FLOAT_Test:\n");
		cin >> f;

		/* INTEGER AND DOUBLE INTEGER WRITE */
		update_intR(d_tag, x);
		update_intR(int_tag, p);
		/*FLOAT WRITE*/
		update_floatR(float_tag, f);
		/*STRING WRITE*/
		update_stringR(str_tag, 0, s);
		/*change the value of boolean*/
		toggle_boolR(bool_tag);

		/* print out the data */
		printf("New DINT_TEST: %d\n", read_intR(d_tag));
		printf("New INT_TEST: %d\n", read_intR(int_tag));
		printf("New FLOAT_Test: %f\n", read_realR(float_tag));
		b = read_boolR(bool_tag);
		s = read_stringR(str_tag);
		cout << "New STRING_TEST: " << s << "\n";
		printf("Boolean: %d\n", b);
		/* we are done */
		destroyRockTags();

		printf("Press any key to exit");
		getchar();

		return 0;
	}
	case SIEMENS300:
	{
		//strcpy(Address, IP_address.c_str());
		Address = IP_address.c_str();
		getchar();
		Rack = 0;
		Slot = 2;
		cout << "Siemens 300" << "\n";
		int Int;
		//Client->SetAsCallback(CliCompletion, NULL);
		if (CliConnect())
		{
			//readArea();
			//Int = getIntAt(1, 2);
			//cout << "int: " << Int << "\n";
			//	cout << "int2: " << getIntAt(1, 4) << "\n";
			//cout << "int3: " << getIntAt(1, 6) << "\n";
			//getRealAt(1,8);
			//getStringAt();
			//getBoolAt(1,0,1);
			Cli_Disconnect(Client);

		}

		/*
		int res = Client->ConnectTo("10.0.0.9", 0, 2);
		if (res == 0) {
		printf("connected");
		getchar();
		}
		cout << res << "\n";
		*/
		//Summary();
		getchar();
		return 0;
	}
	case SIEMENS1500:
	{
		//Address = "10.0.0.9";
		Address = IP_address.c_str();
		Rack = 0;
		Slot = 0; //could also be 1
				  //Client->SetAsCallback(CliCompletion, NULL);
		if (CliConnect())
		{
			//getIntAt(1,2);
			//getFloatAt();
			//getStringAt();
			//getBoolAt(1,0,1);
			/*
			OrderCode();
			CpuInfo();
			CpInfo();
			UnitStatus();
			ReadSzl_0011_0000();
			UploadDB0();
			AsCBUploadDB0();
			AsEWUploadDB0();
			AsPOUploadDB0();
			MultiRead();*/
			Cli_Disconnect(Client);
		}
		Summary();
		getchar();
		return 0;
	}
	case ERROR:
	{
		cout << "Error";
		getchar();
		break;
	}
	}


}


