//#ifdef OS_WINDOWS
//# define WIN32_LEAN_AND_MEAN
//# include <windows.h>
//#endif
////if includes are rearranged, solution will not compile
//#include <WinSock2.h>
//#include <stdio.h>
//#include <stdlib.h>
//#include "libplctag/libplctag.h"
//#include "utils.h"
//#include <iostream>
//#include <string>
//#include <fstream>
//#include "rapidxml/rapidxml.hpp"
//#include <vector>
//#include <algorithm>
//#include "snap7/s7.h"
//#include "postgresql/libpq-fe.h"
//using namespace rapidxml;
//using namespace std;
//
//string Make;
//string Model;
//string IP_address;
//
//#define NOTREAD 0
//#define ROCKWELL 1
//#define SIEMENS300 2
//#define SIEMENS1500 3
//#define ERROR 4
//int state;
//
//vector <string> tagTypes;
//vector <string> tagNames;
//vector <int32_t> myTags;
//vector <string> tagSizes;
//vector <int> tagOffsets;
//
///*set up for Postgressql*/
////the libraries and set up for the Postgresql
//PGconn *dbconn;
//PGconn *newconn;
//PGresult *query;
//PGresult *res;
//const char *conninfo = "host=localhost port=5432 user=postgres password=Pollux123";
///*end set up for Postgressql*/
//
////-.-.-.-.-.-.-..--..-.-.-.-.-.-.-.-..-.-.-.-.-..-.-.-.-.-.-.-..-.-.-.-.-.-.-.-.-.-.-..-
//
///*set up for SIEMENS SERVER*/
//S7Object Client;
//
//const char *Address;     // PLC IP Address
//						 //const char * Address;
//int Rack, Slot; // Default Rack and Slot
//
//int ok = 0; // Number of test pass
//int ko = 0; // Number of test failure
//
//
//			// Check error
//bool Check(int Result, const char * function)
//{
//	if (Result == 0) {
//		ok++;
//	}
//	else {
//		printf("| ERROR !!! \n");
//		if (Result < 0)
//			printf("| Library Error (-1)\n");
//		else
//			cout << "Error number: " << Result << "\n";
//		//		printf("| %s\n", Cli_ErrorText(Result).c_str());						//this function doesn't work, so just Google the error code 
//		printf("+-----------------------------------------------------\n");
//		ko++;
//	}
//	return Result == 0;
//}
//void ListBlocks()
//{
//	TS7BlocksList List;
//	int res = Cli_ListBlocks(Client, &List);
//	if (Check(res, "List Blocks in AG"))
//	{
//		printf("  OBCount  : %d\n", List.OBCount);
//		printf("  FBCount  : %d\n", List.FBCount);
//		printf("  FCCount  : %d\n", List.FCCount);
//		printf("  SFBCount : %d\n", List.SFBCount);
//		printf("  SFCCount : %d\n", List.SFCCount);
//		printf("  DBCount  : %d\n", List.DBCount);
//		printf("  SDBCount : %d\n", List.SDBCount);
//	};
//}
//void readSiemensType(string type, int offset, byte data[], int offBit = 0) {
//	if (type == "string") {
//		cout << S7_GetStringAt(data, offset) << "\n";
//	}	if (type == "int") {
//		cout << S7_GetIntAt(data, offset) << "\n";
//	}	if (type == "real" || type == "int") {
//		cout << S7_GetRealAt(data, offset) << "\n";
//	}	if (type == "dint") {
//		cout << S7_GetDIntAt(data, offset) << "\n";
//	}	if (type == "bool") {
//		cout <<
//	}
//}
//// get***At returns asked for data type given what datablock data is in and which byte to read from(in the case of boolean, also need to know what bit variable is at)
//int getIntAt(int offset, byte data[]) {
//	uint16_t integer = S7_GetUIntAt(data, 0);
//}
//float getRealAt(int db, int start) {
//	byte data[4];
//	float out = 0;
//	int res = Cli_DBRead(Client, db, start, 4, &data);
//	if (Check(res, "getRealAt")) {
//		out = S7_GetRealAt(data, 0);
//	}
//	return out;
//}
//bool getBoolAt(int db, int startByte, int startBit) {
//	byte data[1];
//	int temp;
//	bool out = 0;
//	int res = Cli_DBRead(Client, db, startByte, 1, &data);
//	if (Check(res, "getBoolAt")) {
//		out = S7_GetBitAt(data, 0, startBit);
//	}
//	return out;
//}
//string getStringAt(int offset, byte data[]) {
//	string out;
//	//int res = Cli_DBRead(Client, db, start, 256, &data);
//	out = S7_GetStringAt(data, offset);
//	return out;
//}
////similar to get***At but also need to input value
//void writeIntAt(int db, int start, int value) {
//	byte data[2]; // for 16 bit integer
//	S7_SetUIntAt(data, 0, value);
//	int res = Cli_DBWrite(Client, db, start, 2, data);
//	if (Check(res, "writeIntAt")) {
//		cout << "integer value reassigned\n";
//	}
//}
//void writeRealAt(int db, int start, float value) {
//	byte data[4];
//	S7_SetRealAt(data, 0, value);
//	int res = Cli_DBWrite(Client, db, start, 4, data);
//	if (Check(res, "writeRealAt")) {
//		cout << "float value reassigned\n";
//	}
//}
//void writeStringAt(int db, int start, string value) {
//	byte data[256];
//	S7_SetStringAt(data, 0, 256, value);
//	int res = Cli_DBWrite(Client, db, start, 256, data);
//	if (Check(res, "writeStringAt")) {
//		cout << "string value reassigned\n";
//	}
//}
//void writeBoolAt(int db, int startbyte, int startbit, bool value) {
//	byte data[1];
//	cout << value << "\n";
//	S7_SetBitAt(data, 0, startbit, value);
//	int res = Cli_DBWrite(Client, db, startbyte, 1, data);
//	if (Check(res, "writeBoolAt")) {
//		cout << "boolean reassigned\n";
//	}
//
//}
//// Unit Connection
//bool CliConnect()
//{
//	int res = Cli_ConnectTo(Client, Address, Rack, Slot);
//	if (Check(res, "UNIT Connection")) {
//		printf("  Connected to   : %s (Rack=%d, Slot=%d)\n", Address, Rack, Slot);
//		//printf("  PDU Requested  : %d bytes\n", Client->PDURequest);
//		//printf("  PDU Negotiated : %d bytes\n", Client->PDULength);
//	};
//	return res == 0;
//}
//
///*end set up for SIEMENS SERVER*/
//
////-.-.-.-.-.-.-.--.-.-.-.--.-.--.-.-.-.-..-.-..-.-.-.-.-.-..-.--.-.-.-.-.-.-.-.-.-.-.-.-.-..-.-.-.-.-.-.-..-.-.-.-.-
//string assignTagSize(string type) {
//	if (type == "string") {
//		return "88";
//	}	if (type == "real" || type == "float" || type == "dint" || type == "int") {
//		return "4";
//	}	if (type == "bool") {
//		return "1";
//	}
//
//}
//int assignTagOffset(string type) {
//	if (type == "string") {
//		return 256;
//	}	if (type == "real" || type == "float" || type == "dint") {
//		return 4;
//	}	if (type == "bool" || type == "int") {
//		return 2;
//	}
//
//}
///*set up for ROCKWELL SERVER*/
///* define tag paths*/
//
///*define sizes*/
//#define BOOL_SIZE 1
//#define STRING_SIZE 88
//#define ELEM_SIZE 4
//#define INT_SIZE 4
//#define FLOAT_SIZE 4
//#define STRING_DATA_SIZE 82
///*define number of elements in arrays*/
//#define ELEM_COUNT 1  //assumes we are not reading an array
//#define DATA_TIMEOUT 5000
///*set up variables for tags*/
//int32_t d_tag = 0;
//int32_t int_tag = 0;
//int32_t str_tag = 0;
//int32_t bool_tag = 0;
//int32_t float_tag = 0;
///*shorter way to destroy all tags*/
//void destroyRockTags() {
//	for (int i = 0; i < tagNames.size(); i++) {
//		plc_tag_destroy(myTags[i]);
//	}
//}
//int32_t create_tagR(const char *path)
//{
//	int rc = PLCTAG_STATUS_OK;
//	int32_t tag = 0;
//	tag = plc_tag_create(path, DATA_TIMEOUT);
//	if (tag < 0) {
//		fprintf(stderr, "ERROR %s: Could not create tag!\n", plc_tag_decode_error(tag));
//		return tag;
//	}
//	if ((rc = plc_tag_status(tag)) != PLCTAG_STATUS_OK) {
//		fprintf(stdout, "Error setting up tag internal state.\n");
//		return rc;
//	}
//	return tag;
//}
///*FOR READING TAGS: currently elem count is set to 1 element, but capability to read an array for each tag has been kept, except for in updateStringR*/
//int read_intR(int32_t tag) {
//	int rc = plc_tag_read(tag, DATA_TIMEOUT);
//	if (rc != PLCTAG_STATUS_OK) {
//		fprintf(stderr, "ERROR: Unable to read the data! Got error code %d: %s\n", rc, plc_tag_decode_error(rc));
//		plc_tag_destroy(tag);
//		return 0;
//	}
//	int i;
//	int out[ELEM_COUNT];
//	for (i = 0; i < ELEM_COUNT; i++) {
//		out[i] = plc_tag_get_int32(tag, (i*ELEM_SIZE));
//	}
//	return out[0];
//}
//float read_realR(int32_t tag) {
//	int rc = plc_tag_read(tag, DATA_TIMEOUT);
//	if (rc != PLCTAG_STATUS_OK) {
//		fprintf(stderr, "ERROR: Unable to read the data! Got error code %d: %s\n", rc, plc_tag_decode_error(rc));
//		plc_tag_destroy(tag);
//		return 0;
//	}
//	int i;
//	float out[ELEM_COUNT];
//	for (i = 0; i < ELEM_COUNT; i++) {
//		out[i] = plc_tag_get_float32(tag, (i*ELEM_SIZE));
//	}
//	return out[0];
//}
//bool read_boolR(int32_t tag) {
//	int rc = plc_tag_read(tag, DATA_TIMEOUT);
//	if (rc != PLCTAG_STATUS_OK) {
//		fprintf(stderr, "ERROR: Unable to read the data! Got error code %d: %s\n", rc, plc_tag_decode_error(rc));
//		plc_tag_destroy(tag);
//		return 0;
//	}
//	return plc_tag_get_uint8(bool_tag, 0);
//}
//string read_stringR(int32_t tag) {
//	int rc = plc_tag_read(tag, DATA_TIMEOUT);
//	if (rc != PLCTAG_STATUS_OK) {
//		fprintf(stderr, "ERROR: Unable to read the data! Got error code %d: %s\n", rc, plc_tag_decode_error(rc));
//		plc_tag_destroy(tag);
//		return "ERROR";
//	}
//	int i;
//	string strings[ELEM_COUNT];
//	for (i = 0; i < ELEM_COUNT; i++) {
//		int str_size = plc_tag_get_int32(tag, (i*STRING_SIZE));
//		char str[STRING_SIZE] = { 0 };
//		int j;
//		for (j = 0; j<str_size; j++) {
//			str[j] = (char)plc_tag_get_uint8(tag, (i*STRING_SIZE) + j + 4);
//		}
//		str[j] = (char)0;
//		string s(str);
//		strings[i] = s;
//	}
//	return strings[ELEM_COUNT - 1];
//}
//string readRockwellType(int32_t tag, string type) {
//	if (type == "string") {
//		return read_stringR(tag);
//	}
//	else {
//		if (type == "int" || type == "dint") {
//			return to_string(read_intR(tag));
//		}if (type == "real" || type == "float") {
//			return to_string(read_realR(tag));
//		}if (type == "bool") {
//			return to_string(read_boolR(tag));
//		}
//	}
//}
//void update_stringR(int32_t tag, int i, string STR)
//{
//	int str_len;
//	int base_offset = i * ELEM_SIZE;
//	int wait;
//	int str_index;
//	char *str = new char[STR.size() + 1];
//	strcpy(str, STR.c_str());
//	/* now write the data */
//	str_len = (int)strlen(str);
//	/* set the length */
//	plc_tag_set_int32(tag, base_offset, str_len);
//	/* copy the data */
//	for (str_index = 0; str_index < str_len && str_index < STRING_DATA_SIZE; str_index++) {
//		plc_tag_set_uint8(tag, base_offset + 4 + str_index, (uint8_t)str[str_index]);
//	}
//	/* pad with zeros */
//	for (; str_index<STRING_DATA_SIZE; str_index++) {
//		plc_tag_set_uint8(tag, base_offset + 4 + str_index, 0);
//	}
//	plc_tag_write(tag, DATA_TIMEOUT);
//}
//void update_floatR(int32_t tag, float f) {
//	int i;
//	for (i = 0; i < ELEM_COUNT; i++) {
//		float val = f;
//		plc_tag_set_float32(tag, (i*ELEM_SIZE), val);
//	}
//	plc_tag_write(tag, DATA_TIMEOUT);
//}
//void update_intR(int32_t tag, int x) {
//	int i;
//	for (i = 0; i < ELEM_COUNT; i++) {
//		int val = x;
//		plc_tag_set_int32(tag, (i*ELEM_SIZE), val);
//	}
//	plc_tag_write(tag, DATA_TIMEOUT);
//}
//void toggle_boolR(int32_t tag) {
//	bool b = read_boolR(tag);
//	if (b) { plc_tag_set_uint8(bool_tag, 0, 0); }
//	else { plc_tag_set_uint8(bool_tag, 0, 1); }
//	int rc = plc_tag_write(bool_tag, DATA_TIMEOUT);
//}
//
///*end set up for ROCKWELL SERVER*/
////-.-.-.-.-.-.-.-.-.-..-.-.-.-.-.-.-.-.-..-.-.-.-.-.-.-..-.-.-.-.-.-.--.-..-.-.-.-.-.-.-.-.-..--.-.-.
//
//int main()
//{
//	//database name that is to be created
//	string newDB = "test3";
//	dbconn = PQconnectdb(conninfo);
//	if (PQstatus(dbconn) == CONNECTION_BAD) {
//		printf("Unable to connect to database\n");
//		Sleep(3000); //to read console
//	}
//	printf("Currently connected to: %s \n", PQdb(dbconn));  //what db we are currently connected to
//															//disconnect from default
//	PQfinish(dbconn);
//	//end database area
//
//	/*READ THE XML Doc*/
//	xml_document<> doc;
//	xml_node<> * root_node;
//	ifstream myfile("TEST_XML.xml"); // open whatever file the data is in (should be reading from Communication folder)
//	if (myfile.fail()) {
//		printf("cannot find file");
//		getchar();
//		return 0;
//	}
//	vector<char> buffer((istreambuf_iterator<char>(myfile)), istreambuf_iterator<char>());
//	buffer.push_back('\0');
//	// Parse the buffer using the xml file parsing library into doc 
//	doc.parse<0>(&buffer[0]);
//	// Find our root node
//	root_node = doc.first_node("ROOT");
//	xml_node<> * plcs_node = root_node->first_node("PLCs");
//	xml_node<> * plc_node = plcs_node->first_node("PLC");
//	Make = plc_node->first_attribute("Type")->value(); // assign the brand
//	Model = plc_node->first_attribute("Model")->value(); // assign the model (need to create a case for Rockwell if Model is not specified)
//	IP_address = plc_node->first_attribute("IPAddress")->value(); //assign the IP Address to use in gateway
//	cout << "IP Address: " << IP_address << "\n";
//	//make the make readable regardless of upper or lower case
//	transform(Make.begin(), Make.end(), Make.begin(), ::toupper);
//	string tempType;
//	for (xml_node<> * data_node = root_node->first_node("DATA"); data_node; data_node = data_node->next_sibling())
//	{
//		tempType = data_node->first_attribute("Type")->value();
//		tagTypes.push_back(tempType);
//		tagNames.push_back(data_node->first_attribute("name")->value());
//		transform(tempType.begin(), tempType.end(), tempType.begin(), ::tolower);
//		if (Make == "ROCKWELL") {
//			tagSizes.push_back(assignTagSize(tempType));
//		}
//		else if (Make == "siemens") {
//			tagOffsets.push_back(assignTagOffset(tempType));
//		}
//	}
//	cout << tagTypes.size() << "\n";
//	if (Make == "ROCKWELL") {
//		cout << "Model recognized as Rockwell\n";
//		state = 1;
//	}
//	if (Make == "SIEMENS") {
//		Client = Cli_Create(); // initialize the client connection
//		if (Model == "300") { // fill out for the different models that use this set up rather than the other one
//			state = 2;
//		}
//		else {
//			state = 3;
//		}
//	}
//	switch (state) {
//	default:
//	{
//		cout << "No Recognized PLC Models found in XML document";
//		getchar();
//		break;
//	}
//	case ROCKWELL:
//	{
//		cout << "Case Rockwell\n";
//		/*
//		protocol for a PATH: protocol stays constant, IP for machine, path should stay constant, LGX stays constant, elem size is size of element, count is size of array in tag to read (if array, currently only reading one number for tag), name is name of the tag
//		/*example paths:
//		const char * INT_PATH = "protocol=ab_eip&gateway=10.0.0.1&path=1,0&cpu=LGX&elem_size=4&elem_count=1&name=INT_Test";
//
//		/* print out the integer data */
//		for (int i = 0; i < tagTypes.size(); i++) {
//			myTags.push_back(create_tagR(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=" + tagSizes[i] + "&elem_count=1&name=" + tagNames[i]).c_str()));
//			cout << tagNames[i] << ": " << readRockwellType(myTags[i], tagTypes[i]) << "\n";
//		}
//
//		cout << "exit?\n";
//		if (getchar() == 'y') {
//			destroyRockTags();
//			getchar();
//			return 0;
//		}
//		string newstr;
//
//		//printf("Input new dint for DINT_Test:\n");
//		//cin >> x;
//		//printf("Input new in for INT_Test:\n");
//		//cin >> p;
//		//printf("Input new string for STRING_Test:\n");
//		//cin >> newstr;
//		//printf("Input new float for FLOAT_Test:\n");
//		//cin >> f;
//
//		///* INTEGER AND DOUBLE INTEGER WRITE */
//		//update_intR(d_tag, x);
//		//update_intR(int_tag, p);
//		///*FLOAT WRITE*/
//		//update_floatR(float_tag, f);
//		///*STRING WRITE*/
//		//update_stringR(myTags[0], 0, newstr);
//		///*change the value of boolean*/
//		//toggle_boolR(bool_tag);
//
//		///* print out the data */
//		//printf("New DINT_TEST: %d\n", read_intR(d_tag));
//		//printf("New INT_TEST: %d\n", read_intR(int_tag));
//		//printf("New FLOAT_Test: %f\n", read_realR(float_tag));
//		//b = read_boolR(bool_tag);
//		//s = read_stringR(str_tag);
//		//cout << "New STRING_TEST: " << readStr << "\n";
//		//printf("Boolean: %d\n", b);
//		///* we are done */
//		getchar();
//		destroyRockTags();
//
//		printf("Press any key to exit");
//		getchar();
//
//		return 0;
//	}
//	case SIEMENS300:
//	{
//		Address = IP_address.c_str();
//		//		Address = "10.0.0.9";
//		Rack = 0;
//		Slot = 2;
//		cout << "Siemens 300" << "\n";
//		if (CliConnect())
//		{
//			ListBlocks();
//			int x;
//			float f;
//			string str;
//			//print out initial values
//			//getIntAt(datablock #, byte position within the datablock)
//			byte data[264];
//
//			int res = Cli_DBRead(Client, 0, 0, 264, &data);
//			if (Check(res, "datablock read")) {
//				for (int i = 0; i < tagTypes.size(); i++) {
//
//					cout << tagNames[i] << ": " << readRockwellType(myTags[i], tagTypes[i]) << "\n";
//				}
//			}
//			cout << "int: " << getIntAt(1, 0) << "\n";
//			//cout << "int2: " << getIntAt(1, 4) << "\n";
//			//cout << "int3: " << getIntAt(1, 6) << "\n";
//			cout << "float: " << getRealAt(1, 260) << "\n";
//			cout << "string: " << getStringAt(1, 2) << "\n";
//			cout << "boolean: " << getBoolAt(1, 0, 258) << "\n";
//			cout << "exit?\n";
//			if (getchar() == 'y') {
//				Cli_Disconnect(Client);
//				getchar();
//				return 0;
//			}
//			//reassign values
//			cout << "input new integer:\n";
//			cin >> x;
//			//same set up, but with last input being the value to set
//			writeIntAt(1, 0, x);
//			cout << "input new float value:\n";
//			cin >> f;
//			writeRealAt(1, 260, f);
//			cout << "input new string value:\n";
//			cin >> str;
//			writeStringAt(1, 2, str);
//			writeBoolAt(1, 258, 0, 1);
//			//print out new values
//			cout << "int2: " << getIntAt(1, 0) << "\n";
//			cout << "float: " << getRealAt(1, 260) << "\n";
//			cout << "string: " << getStringAt(1, 2) << "\n";
//			cout << "boolean: " << getBoolAt(1, 258, 0) << "\n";
//			Cli_Disconnect(Client);
//			getchar();
//
//		}
//		cout << "press any key to EXIT";
//		getchar();
//		return 0;
//	}
//	case SIEMENS1500:
//	{
//		Address = IP_address.c_str();
//		//Address = "10.0.0.9";
//		Rack = 0;
//		Slot = 1;
//		cout << "Siemens 1500" << "\n";
//		if (CliConnect())
//		{
//			int x;
//			float f;
//			string str;
//			//print out initial values
//			cout << "int: " << getIntAt(1, 0) << "\n";
//			cout << "float: " << getRealAt(1, 260) << "\n";
//			cout << "string: " << getStringAt(1, 4) << "\n";
//			cout << "boolean: " << getBoolAt(1, 2, 0) << "\n";
//			cout << "exit?\n";
//			if (getchar() == 'y') {
//				Cli_Disconnect(Client);
//				getchar();
//				return 0;
//			}			//reassign values
//			cout << "input new integer:\n";
//			cin >> x;
//			writeIntAt(1, 0, x);
//			cout << "input new float value:\n";
//			cin >> f;
//			writeRealAt(1, 260, f);
//			cout << "input new string value:\n";
//			cin >> str;
//			writeStringAt(1, 4, str);
//			writeBoolAt(1, 2, 0, 1);
//			//print out new values
//			cout << "int2: " << getIntAt(1, 0) << "\n";
//			cout << "float: " << getRealAt(1, 260) << "\n";
//			cout << "string: " << getStringAt(1, 4) << "\n";
//			cout << "boolean: " << getBoolAt(1, 2, 0) << "\n";
//			Cli_Disconnect(Client);
//			getchar();
//		}
//		cout << "press any key to EXIT";
//		getchar();
//		return 0;
//	}
//	case ERROR:
//	{
//		// no current use for this, but set up in case of running into additional errors
//		cout << "Error";
//		getchar();
//		break;
//	}
//	}
//
//
//}
//
//
