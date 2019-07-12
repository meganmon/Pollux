#ifdef OS_WINDOWS
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif
//if includes are rearranged, solution will not compile
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
#include "snap7/s7.h"
#include "postgresql/libpq-fe.h"
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

vector <string> tagTypes;
vector <string> tagNames;
vector <int32_t> myTags;
vector <string> tagSizes;
vector <int> tagOffsets;
vector <float> real;

/*set up for Postgressql*/
//the libraries and set up for the Postgresql
PGconn *dbconn;
PGconn *newconn;
PGresult *query;
PGresult *res;
const char *conninfo = "host=localhost port=5432 user=postgres password=Pollux123";
/*end set up for Postgressql*/

//-.-.-.-.-.-.-..--..-.-.-.-.-.-.-.-..-.-.-.-.-..-.-.-.-.-.-.-..-.-.-.-.-.-.-.-.-.-.-..-

/*set up for SIEMENS SERVER*/
S7Object Client;

const char *Address;     // PLC IP Address
int Rack, Slot; // Default Rack and Slot

int ok = 0; // Number of test pass
int ko = 0; // Number of test failure


// Check error
bool Check(int Result, const char * function)
{
	if (Result == 0) {
		ok++;
	}
	else {
		printf("| ERROR !!! \n");
		if (Result < 0)
			printf("| Library Error (-1)\n");
		else
			cout << "Error number: " << Result << "\n";
		//		printf("| %s\n", Cli_ErrorText(Result).c_str());						//this function doesn't work, so just Google the error code 
		printf("+-----------------------------------------------------\n");
		ko++;
	}
	return Result == 0;
}
void ListBlocks()
{
	TS7BlocksList List;
	int res = Cli_ListBlocks(Client,&List);
	if (Check(res, "List Blocks in AG"))
	{
		printf("  OBCount  : %d\n", List.OBCount);
		printf("  FBCount  : %d\n", List.FBCount);
		printf("  FCCount  : %d\n", List.FCCount);
		printf("  SFBCount : %d\n", List.SFBCount);
		printf("  SFCCount : %d\n", List.SFCCount);
		printf("  DBCount  : %d\n", List.DBCount);
		printf("  SDBCount : %d\n", List.SDBCount);
	};
}
void readSiemensType(string type, int offset, int Bit, byte data[]) {
	if (type == "string") {
		cout << S7_GetStringAt(data,offset) << "\n";
	}	if (type == "int") {
		cout << S7_GetIntAt(data, offset) << "\n";
	}	if (type == "real" || type == "float") {
		cout << S7_GetRealAt(data, offset) << "\n";
		real.push_back(S7_GetRealAt(data, offset));
	}	if (type == "dint") {
		cout << S7_GetDIntAt(data, offset) << "\n";
	}	if (type == "bool") {
		cout << S7_GetBitAt(data, offset, Bit) << "\n";
	}
}

//similar to get***At but also need to input value
void writeIntAt(int db, int start, int value) {
	byte data[2]; // for 16 bit integer
	S7_SetUIntAt(data, 0, value);
	int res = Cli_DBWrite(Client, db, start, 2, data);
	if (Check(res, "writeIntAt")) {
		cout << "integer value reassigned\n";
	}
}
void writeRealAt(int db, int start, float value) {
	byte data[4];
	S7_SetRealAt(data, 0, value);
	int res = Cli_DBWrite(Client, db, start, 4, data);
	if (Check(res, "writeRealAt")) {
		cout << "float value reassigned\n";
	}
}
void writeStringAt(int db, int start, string value) {
	byte data[256];
	S7_SetStringAt(data, 0, 256, value);
	int res = Cli_DBWrite(Client, db, start, 256, data);
	if (Check(res, "writeStringAt")) {
		cout << "string value reassigned\n";
	}
}
void writeBoolAt(int db, int startbyte, int startbit, bool value) {
	byte data[1];
	cout << value << "\n";
	S7_SetBitAt(data, 0, startbit, value);
	int res = Cli_DBWrite(Client, db, startbyte, 1, data);
	if (Check(res, "writeBoolAt")) {
		cout << "boolean reassigned\n";
	}

}
// Unit Connection
bool CliConnect()
{
	int res = Cli_ConnectTo(Client, Address, Rack, Slot);
	if (Check(res, "UNIT Connection")) {
		printf("  Connected to   : %s (Rack=%d, Slot=%d)\n", Address, Rack, Slot);
		//printf("  PDU Requested  : %d bytes\n", Client->PDURequest);
		//printf("  PDU Negotiated : %d bytes\n", Client->PDULength);
	};
	return res == 0;
}

int assignTagOffset(string type) {
	if (type == "string") {
		return 256;
	}	if (type == "real" || type == "float" || type == "dint") {
		return 4;
	}	if (type == "bool" || type == "int") {
		return 2;
	}

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

string assignTagSize(string type) {
	if (type == "string") {
		return "88";
	}	if (type == "real" || type == "float" || type == "dint" || type == "int") {
		return "4";
	}	if (type == "bool") {
		return "1";
	}

}
/*define number of elements in arrays*/
#define ELEM_COUNT 1  //assumes we are not reading an array
#define DATA_TIMEOUT 5000

/*shorter way to destroy all tags*/
void destroyRockTags() {
	for (int i = 0; i < tagNames.size(); i++) {
		plc_tag_destroy(myTags[i]);
	}
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
	return plc_tag_get_uint8(tag, 0);
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
string readRockwellType(int32_t tag, string type) {
	if (type == "string") {
		return read_stringR(tag);
	}
	else {
		if (type == "int" || type == "dint") {
			return to_string(read_intR(tag));
		}if (type == "real" || type == "float") {
			return to_string(read_realR(tag));
		}if (type == "bool") {
			return to_string(read_boolR(tag));
		}
	}
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
	if (b) { plc_tag_set_uint8(tag, 0, 0); }
	else { plc_tag_set_uint8(tag, 0, 1); }
	int rc = plc_tag_write(tag, DATA_TIMEOUT);
}

/*end set up for ROCKWELL SERVER*/
//-.-.-.-.-.-.-.-.-.-..-.-.-.-.-.-.-.-.-..-.-.-.-.-.-.-..-.-.-.-.-.-.--.-..-.-.-.-.-.-.-.-.-..--.-.-.

/*SET UP FOR MISCELLANEOUS FUNCTIONS*/

string floatsToString(vector <float> real) {		//converts float array to string for uploading to DB
	string out;
	int max_size = real.size();
	for (int i = 0; i < max_size; i++) {
		if (i == 0) {
			out.append("[");
		}
		out.append(to_string(real[i]));
		if (i == max_size - 1) {
			out.append("]");
		}
		else {
			out.append(",");
		}
	}
	return out;
}

void readXML(const char *file) {		//reads through XML document "file" to assign variables and tags to read, then assigns state according to PLC type
	/*READ THE XML Doc*/
	xml_document<> doc;
	xml_node<> * root_node;
	ifstream myfile(file); // open whatever file the data is in (should be reading from Communication folder)
	if (myfile.fail()) {
		printf("cannot find file");
		getchar();
		state = 4;
	}
	else {
		vector<char> buffer((istreambuf_iterator<char>(myfile)), istreambuf_iterator<char>());
		buffer.push_back('\0');
		// Parse the buffer using the xml file parsing library into doc 
		doc.parse<0>(&buffer[0]);
		// Find our root node
		root_node = doc.first_node("ROOT");
		xml_node<> * plcs_node = root_node->first_node("PLCs");
		xml_node<> * plc_node = plcs_node->first_node("PLC");
		Make = plc_node->first_attribute("Type")->value(); // assign the brand
		Model = plc_node->first_attribute("Model")->value(); // assign the model (need to create a case for Rockwell if Model is not specified)
		IP_address = plc_node->first_attribute("IPAddress")->value(); //assign the IP Address to use in gateway
		 //make the make readable regardless of upper or lower case
		transform(Make.begin(), Make.end(), Make.begin(), ::toupper);
		string tempType;
		for (xml_node<> * data_node = root_node->first_node("DATA"); data_node; data_node = data_node->next_sibling())
		{
			tempType = data_node->first_attribute("Type")->value();
			transform(tempType.begin(), tempType.end(), tempType.begin(), ::tolower);
			tagTypes.push_back(tempType);
			tagNames.push_back(data_node->first_attribute("name")->value());
			if (Make == "ROCKWELL") {
				tagSizes.push_back(assignTagSize(tempType));
			}
			else if (Make == "SIEMENS") {
				tagOffsets.push_back(assignTagOffset(tempType));  
			}
		}
		if (Make == "ROCKWELL") {
			cout << "Model recognized as Rockwell\n";
			state = 1;
		}
		if (Make == "SIEMENS") {
			Client = Cli_Create(); // initialize the client connection
			if (Model == "300") { // fill out for the different models that use this set up rather than the other one
				state = 2;
			}
			else {
				state = 3;
			}
		}
	}
}
void readDB() {
	res = PQexec(dbconn, "SELECT * FROM testing");
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {

		printf("No data retrieved\n");
		PQclear(res);
		PQfinish(dbconn);
		getchar();
	}
	int rows = PQntuples(res);
	if (rows == 0) {
		printf("not exists");
	}
	for (int i = 0; i<rows; i++) {

		printf("%s %s %s\n", PQgetvalue(res, i, 0),
			PQgetvalue(res, i, 1), PQgetvalue(res, i, 2));
	}
	PQclear(res);
}
bool ifPKexists(string PK, string table) {			//CHECKS IF THE PRIMARY KEY IS ALREADY IN TABLE TO AVOID ERRORS
	res = PQexec(dbconn, ("SELECT * FROM " + table + " WHERE pk = '" + PK + "'").c_str());
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {

		printf("No data retrieved\n");
		PQclear(res);
		PQfinish(dbconn);
		getchar();
		return 0;
	}
	int rows = PQntuples(res);
	if (rows == 0) {
		PQclear(res);
		printf("not exists");
		return 0;
	}
	else {
		PQclear(res);
		return 1;
	}
}
void checkexec(PGresult *res, const char *function) {			//checks to make sure Postgresql command went through	
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("could not execute command: %s\n", function);
		PQfinish(dbconn);
		getchar();
	}
	PQclear(res);

}
//-~_~_~_~_~__~_~_~_~_~_~__~_~_~_~_~_~_~_~_~__~_~_~_~_ 
// START MAIN LOOP
//_~_~_~_~_~_~__~_~_~_~_~_~_~_~__~_~_~_~_~_~__~_~_~_~_~

int main()
{
	//connect to postgres given the const char conninfo as the destination
	dbconn = PQconnectdb(conninfo);
	if (PQstatus(dbconn) == CONNECTION_BAD) {
		printf("Unable to connect to database\n");
		getchar(); // to read console
	}
	
	// CREATE ALL NECESSARY TABLES IN SCHEMA (or overlook if already exists)
	PGresult *res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS testing(pk SERIAL PRIMARY KEY, fail_bit INTEGER, reals REAL[])");
	checkexec(res, "create testing table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS model(model_fk TEXT PRIMARY KEY, name TEXT)");
	checkexec(res, "create model table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS part(part_fk TEXT PRIMARY KEY, serial TEXT, model_fk TEXT REFERENCES model(model_fk), start BOOL, finish BOOL)");
	checkexec(res, "Create part table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS line(line_fk TEXT PRIMARY KEY, name TEXT)");
	checkexec(res, "create line table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS station(station_fk TEXT PRIMARY KEY, name TEXT, line_fk TEXT REFERENCES line(line_fk), final BOOL)");
	checkexec(res, "create station table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS results(pk TEXT PRIMARY KEY, part_fk TEXT REFERENCES part(part_fk), station_fk TEXT REFERENCES station(station_fk), cycle_number INTEGER, status_bit INTEGER, fail_bit INTEGER, real REAL[])");
	checkexec(res, "create results table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS results_names(pk TEXT PRIMARY KEY, station_fk TEXT REFERENCES station(station_fk), status_bit TEXT[], real TEXT[], real_unit TEXT[])");
	checkexec(res, "create results names table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS results_limits(pk TEXT PRIMARY KEY, station_fk TEXT REFERENCES station(station_fk), real_max REAL[], real_min REAL[])");
	checkexec(res, "create results limits table");

	//end database area

	//readxml reads through given xml file and adjusts script state according to plc type and model - also assigns tags and types to read
	readXML("TEST_XML.xml");				// PUT NAME OF XML TO BE READ HERE, can also be an input variable at the top of the script too i guess....

	switch (state) {
	default:
	{
		cout << "No Recognized PLC Models found in XML document \n";
		if (ifPKexists("test21", "testing")) { printf("yeet"); }		//just testing fuction... to be taken out later
		getchar();
		break;
	}
	case ROCKWELL:
	{
		cout << "Case Rockwell\n";
		/*
		protocol for a PATH: protocol stays constant, IP for machine, path should stay constant, LGX stays constant, elem size is size of element, count is size of array in tag to read (if array, currently only reading one number for tag), name is name of the tag
		/*example paths:
		const char * INT_PATH = "protocol=ab_eip&gateway=10.0.0.1&path=1,0&cpu=LGX&elem_size=4&elem_count=1&name=INT_Test";

		/* print out the integer data */
		for (int i = 0; i < tagTypes.size(); i++) {
			myTags.push_back(create_tagR(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=" + tagSizes[i] + "&elem_count=1&name=" + tagNames[i]).c_str()));
			cout << tagNames[i] << ": " << readRockwellType(myTags[i], tagTypes[i]) << "\n";
		}
	
		cout << "exit?\n";
		if (getchar() != 'n') {
			destroyRockTags();
			PQfinish(dbconn);
			getchar();
			return 0;
		}
		string newstr;
		//this is where you might update tags depending on what you want to do
		///* we are done */
		getchar();
		destroyRockTags();
		printf("Press any key to exit");
		PQfinish(dbconn);
		getchar();
		return 0;
	}
	case SIEMENS300:
	{
		Address = IP_address.c_str();
//		Address = "10.0.0.9";
		Rack = 0;
		Slot = 2;
		cout << "Siemens 300" << "\n";
		if (CliConnect())
		{
			ListBlocks();
			int x;
			float f;
			string str;
			byte data[500];
			int offset = 0;
			int bitTracker = 0;
			bool secondbyte = FALSE;
			//current datablock set to DB1
			int sres = Cli_DBRead(Client, 1, 0, 286, &data);
			if (Check(sres, "datablock read")) {
				for (int i = 0; i < tagTypes.size(); i++) {
					cout << tagNames[i] << ": ";
					readSiemensType(tagTypes[i], offset, bitTracker, data);
					cout << "\n";
					//might need to adjust this for when booleans follow each other (currently set that one boolean will take up 2 bytes, but if multiples booleans, will need to be adjusted):
					//TEST WITH MULTIPLE BOOLS - other option is just to input offsets in the xml though....
					if (i != tagTypes.size()-1){
						if (tagTypes[i] == "bool" && tagTypes[i + 1] == "bool") {
							bitTracker += 1;
							if (bitTracker == 8) {
								if (secondbyte) {
									secondbyte = FALSE;
								}
								else { secondbyte = TRUE; }
								offset += 1;
								bitTracker = 0;
							}
						}
						else {
							bitTracker = 0;
							if (secondbyte) { offset += 1; }
							else{ offset += tagOffsets[i]; }
						}
					}


				}
			}

			//upload results to db
			string reals = floatsToString(real);
			res = PQexec(dbconn, ("INSERT INTO testing VALUES(DEFAULT, 4000, ARRAY " + reals + ")").c_str());
			checkexec(res, "insert values into testing");
			readDB();
			cout << "exit?\n";
			if (getchar() != 'n') {
				Cli_Disconnect(Client);
				PQfinish(dbconn);
				getchar();
				return 0;
			}
			//reassign values
			cout << "input new integer:\n";
			cin >> x;
			//same set up, but with last input being the value to set
			writeIntAt(1, 0, x);
			cout << "input new float value:\n";
			cin >> f;
			writeRealAt(1, 260, f);
			cout << "input new string value:\n";
			cin >> str;
			writeStringAt(1, 2, str);
			writeBoolAt(1, 258, 0, 1);
			Cli_Disconnect(Client);
			getchar();

		}
		cout << "press any key to EXIT";
		getchar();

		PQfinish(dbconn);
		return 0;
	}
	case SIEMENS1500:
	{
		Address = IP_address.c_str();
		//Address = "10.0.0.9";
		Rack = 0;
		Slot = 1;
		cout << "Siemens 1500" << "\n";
		if (CliConnect())
		{
			int x;
			float f;
			string str;
			byte data[500];
			int offset = 0;
			int bitTracker = 0;
			//current datablock set to DB1
			int sres = Cli_DBRead(Client, 1, 0, 280, &data);
			if (Check(sres, "datablock read")) {
				for (int i = 0; i < tagTypes.size(); i++) {
					cout << tagNames[i] << ": ";
					readSiemensType(tagTypes[i], offset, bitTracker, data);
					cout << "\n";
					//might need to adjust this for when booleans follow each other (currently set that one boolean will take up 2 bytes, but if multiples booleans, will need to be adjusted):
					//TEST WITH MULTIPLE BOOLS - other option is just to input offsets in the xml though....
					if (tagTypes[i] == "bool" && tagTypes[i + 1] == "bool") {
						bitTracker += 1;
						if (bitTracker == 8) {
							offset += 1;
							bitTracker = 0;
						}
					}
					else {
						bitTracker = 0;
						offset += tagOffsets[i];
					}

				}
			}
			cout << "exit?\n";
			if (getchar() != 'n') {
				Cli_Disconnect(Client);
				getchar();

				PQfinish(dbconn);
				return 0;
			}
			Cli_Disconnect(Client);
			getchar();
		}
		cout << "press any key to EXIT";
		getchar();

		PQfinish(dbconn);
		return 0;
	}
	case ERROR:
	{
		// no current use for this, but set up in case of running into additional errors
		cout << "Error";
		getchar();

		PQfinish(dbconn);
		return 0;
	}
	}


}


