/*
What is done:

TO DO:
- MODEL FK -> given only serial, how do we know model foreign key, or is name also serial number? 
station number vs station fk -> will these correspond or no?
IF start is now(), what is finish?


-~-~- DONE UNTIL QUESTIONS/TESTING-~-~-~
results table -> improve after question (station fk is station number or what will I use as identifier?)
createPart(); --> improve after questions (what is finish?), (will i search model for model fk? how to get name? -> to be answered by Gian)
Redo for Rockwell - need to test and ensure correct set up
-~~-~-~-~-~-~ CONTINUE -~-~-~-~-~-~-~-~
rockwell set up and testing
*/


#ifdef OS_WINDOWS
# define WIN32_LEAN_AND_MEAN
# include <windows.h>
#endif
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
#include "Rockwell.h"
#include "Siemens.h"
#include <array>
using namespace rapidxml;
using namespace std;


string Make;
string Model;
string IP_address;

#define NOTREAD 0
#define ROCKWELL 1
#define SIEMENS300 2
#define SIEMENS1500 3
#define ERRORS 4
int state;


S7Object Client;
const char *Address;     // PLC IP Address
int Rack, Slot; // Default Rack and Slot

/* set up for Siemens: 
each datablock is stored in the respective loadDBs, unloadDBs, and stationsDBs (this is the COMMS datablock), 
the respective station # of the COMM datablock is then stored in the stations vector in case a station does not start at 1 -> assumes that load, unload, and COMMS blocks will be ordered the same within the XML


set up for Rockwell:
# of total stations is saved in numStations, then tag names are aggregated within the search 

Both use numStations to know how many datablocks to read
both use resultSize to allow the Real results to be an adjustable size
*/
//this set can probably be gottten rid of
vector <string> tagTypes;
vector <string> tagNames;
vector <int32_t> myTags;
vector <string> tagSizes;
vector <int> tagOffsets;
//end of set that could be deleted (probably)
vector <float> real;		//stores results from results in Unload datablock
vector <int> stations;		//stores the station #'s (in case not starting from 0)
size_t numStations;			//number of total stations
int resultSize;				//number of reals in results variable in unload datablock
//Siemens only
vector <int> stationDBs;		//stores offsets for COMM variables
vector <int> loadDBs;			//stores offsets for Load datablock variables
vector <int> unloadDBs;			//stores offsets for Unload datablock variables
//Rockwell only
vector <string> loadNames;		//data names for load data -> to be aggregated with station number to make tag name
vector <string> unloadNames;	//data names for unload data
vector <string> commNames;		//data names for COMM data
vector <int32_t> loadtags;		//tags for load
vector <int32_t> unloadtags;	//tags for unload
vector <int32_t> commtags;		//tags for COMM
vector <int> elemCount;			//number of elements in each of unload tags
vector <string> dataTypeLoad;	//type of elements in Load data
vector <string> dataTypeUnload;	//type of element in unload data
vector <string> dataTypeComm;	//type of element in COMM data
bool loadSet = false;			//
bool unloadSet = false;			//
bool commSet = false;			//

/*set up for Postgressql*/
//the libraries and set up for the Postgresql
PGconn *dbconn;
PGconn *newconn;
PGresult *query;
PGresult *res;
const char *conninfo = "host=localhost port=5432 user=postgres password=Pollux123";
/*end set up for Postgressql*/


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
long int boolsToInt(vector <bool> bools) {			// for converting status and failure bits to dint to upload to database
	int ret = 0;
	int tmp;
	int count = 32;
	for (int i = 0; i < count; i++) {
		tmp = bools[i];
		ret |= tmp << (count - i - 1);
	}
	return ret;
}

void readXML(const char *file) {		//reads through XML document "file" to assign variables and tags to read, then assigns state according to PLC type
	/*READ THE XML Doc*/
	xml_document<> doc;
	xml_node<> * root_node;
	cout << file << "\n";
	ifstream myfile(file); // open whatever file the data is in (should be reading from Communication folder)
	if (myfile.fail()) {
		cout << "to test";
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
		IP_address = plc_node->first_attribute("IPAddress")->value(); //assign the IP Address to use in gateway
		numStations = stoi(plc_node->first_attribute("numberOfStations")->value());
		 //make the make readable regardless of upper or lower case
		transform(Make.begin(), Make.end(), Make.begin(), ::toupper);
		string tempType;
		for (xml_node<> *datablock_node = root_node->first_node("DATABLOCK"); datablock_node; datablock_node= datablock_node->next_sibling()){
			string dbname = datablock_node->first_attribute("name")->value();
			transform(dbname.begin(), dbname.end(), dbname.begin(), ::tolower);
			if (Make == "SIEMENS") {
				if (dbname == "load") {
					loadDBs.push_back(stoi(datablock_node->first_attribute("dataBlock")->value()));			//store the datablock # for each station (must be in order in xml)
				}
				else if (dbname == "unload") {
					unloadDBs.push_back(stoi(datablock_node->first_attribute("dataBlock")->value()));		//store the datablock # for unload of each station (mush be in order in xml)
					for (xml_node<> *data_node = datablock_node->first_node("DATA"); data_node; data_node = data_node->next_sibling()) {
						string name = data_node->first_attribute("name")->value();
						transform(name.begin(), name.end(), name.begin(), ::tolower);
						if (name == "result") {
							resultSize = stoi(data_node->first_attribute("array")->value());		//tells how big results array will be
						}
					}
				}
				else if (dbname == "comm") {
					stationDBs.push_back(stoi(datablock_node->first_attribute("dataBlock")->value()));		//store the datablock # for comm of each station (must be in order)
					stations.push_back(stoi(datablock_node->first_attribute("station")->value()));			// also sets the station blocks in case a station is not to be read (e.g. station 3 is skipped)
				}
			}
			else if (Make == "ROCKWELL") {			//assumes all sections with be the same accross stations
				if (dbname == "load" && !loadSet) {
					for (xml_node<> *data_node = datablock_node->first_node("DATA"); data_node; data_node = data_node->next_sibling()) {
						string name = data_node->first_attribute("name")->value();		//stores the name of the data for each tag to be read  ->> this is aggregated later to be turned into tag names according to convention
						string type = data_node->first_attribute("type")->value();		//stores data type of each tag
						transform(type.begin(), type.end(), type.begin(), ::tolower);
						dataTypeLoad.push_back(type);
						loadNames.push_back(name);

					}
					loadSet = true;
				}
				else if (dbname == "unload" && !unloadSet) {	
					for (xml_node<> *data_node = datablock_node->first_node("DATA"); data_node; data_node = data_node->next_sibling()) {
						string name = data_node->first_attribute("name")->value();		//stores the name of the data for each tag of this sections
						unloadNames.push_back(name);
						transform(name.begin(), name.end(), name.begin(), ::tolower);
						if (name == "result") {
							resultSize = stoi(data_node->first_attribute("array")->value());		//tells big results array will be
						}
						elemCount.push_back(stoi(data_node->first_attribute("array")->value()));	//vector for knowing size of other arrays (even though the only other two arrays should be boolean arrays of size 32, but just in case)
						string type = data_node->first_attribute("type")->value();
						transform(type.begin(), type.end(), type.begin(), ::tolower);
						dataTypeUnload.push_back(type);
					}
					unloadSet = true;
				}
				else if (dbname == "comm" && !commSet) {
					stations.push_back(stoi(datablock_node->first_attribute("station")->value()));		//set the station blocks in case a station is not to be read (e.g. station 3 is purposely skipped)
					for (xml_node<> *data_node = datablock_node->first_node("DATA"); data_node; data_node = data_node->next_sibling()) {
						string name = data_node->first_attribute("name")->value();
						commNames.push_back(name);
						string type = data_node->first_attribute("type")->value();
						transform(type.begin(), type.end(), type.begin(), ::tolower);
						dataTypeComm.push_back(type);
					}
					commSet = true;
				}
			}
			/* THIS IS CURRENTLY SET UP FOR A STANDARD DATABLOCK, IF NOT GOING TO BE STANDARD, NEEDS TO ADAPT*/

		}
		//assign the state that 
		if (Make == "ROCKWELL") {
			cout << "Model recognized as Rockwell\n";
			state = 1;
		}
		if (Make == "SIEMENS") {
			Client = Cli_Create(); // initialize the client connection
			Model = plc_node->first_attribute("Model")->value(); // assign the model 
			if (Model == "300") { // fill out for the different models that use this set up rather than the other one
				state = 2;
			}
			else {
				state = 3;
			}
		}
	}
}
void readDB() {		//no longer a super necessary function, but can be change to take in const char of any table
	res = PQexec(dbconn, "SELECT * FROM testing");
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		printf("No data retrieved\n");
		PQclear(res);
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
bool ifExists(string identifier, string column, string table) {			//CHECKS IF THE IDENTIFIER IS ALREADY IN GIVEN TABLE
	cout << ("SELECT * FROM " + table + " WHERE " + column + " = '" + identifier + "'").c_str() << "\n";
	res = PQexec(dbconn, ("SELECT * FROM " + table + " WHERE " + column + " = '" + identifier + "'").c_str());
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		printf("No data retrieved\n");
		PQclear(res);
		return 0;
	}
	int rows = PQntuples(res);
	if (rows == 0) {
		PQclear(res);
		printf("Does not exist\n");
		return 0;
	}
	else {
		PQclear(res);
		return 1;
	}
}
void checkexec(PGresult *res, const char *function) {			//checks to make sure Postgresql command went through	 -- it clears result after checking, so do not use if trying to read data from the result after calling
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("could not execute command: %s\n", function);
		getchar();
	}
	PQclear(res);

}
bool rockwellUnload(int station, string part_fk, int cycle) {		//reads unload block of Rockwell, looks for fail bits; if no failure, returns true; all results, status bits, and fail bits are uploaded to database regardless of failures
	vector <float> reals;
	vector <bool> status;
	vector <bool> fail;
	bool out = true;
	int offset = 0;
	int bit = 0;
	//read real block:
	reals = read_realR(unloadtags[station* 7], resultSize);
	status = readBoolArray(unloadtags[station * 7 + 1], 32);
	fail = readBoolArray(unloadtags[station * 7 + 2], 32);
	//read the 32 bit array of bools for fail bits and status bits
	for (int i = 0; i < 32; i++) {

		bool hasfailed = fail[i];
		if (hasfailed) { out = false; }		// if one of the failbits is equal to 1 (something has gone wrong)
	}
	//if some thing has gone wrong, print out the error code
	if (!out) {
		int errorCode = read_intR(unloadtags[station*7+4]); //ASSUMES: standard datablock position of error code double integer
		string errorMessage = read_stringR(unloadtags[station*7+5]);	//ASUMES: standard datablock posiition of error Message string
		cout << "error code: " << errorCode << "\n";
		cout << "Error Message: " << errorMessage << "\n";

	}
	//add the information to the results table
	string Real = floatsToString(reals);
	string fails = to_string(boolsToInt(fail));
	string statuses = to_string(boolsToInt(status));
	cout << ("INSERT INTO results VALUES(DEFAULT, " + part_fk + ", " + to_string(stations[station]) + ", " + to_string(cycle) + ", " + statuses + ", " + fails + ", ARRAY " + Real + ")").c_str() << "\n";		//stations in this code are indexed at zero, but in postgres indexed at 1, so add one to statoin number when inputing
	res = PQexec(dbconn, ("INSERT INTO results VALUES(DEFAULT, " + part_fk + ", " + to_string(stations[station]) + ", " + to_string(cycle) + ", " + statuses + ", " + fails + ", ARRAY " + Real + ")").c_str());
	checkexec(res, "insert values into results");
	return out;
}
bool siemensStartUnload(int station, string part_fk, int cycle) {		//reads unload block of Siemens, looks for fail bits; if no failure, returns true; all results, status bits, and fail bits are uploaded to database regardless of failures
	byte buffer[342];
	vector <float> reals;
	vector <bool> status;
	vector <bool> fail; 
	bool out = true;
	int offset = 0;
	int bit = 0;
	//read datablock
	Cli_DBRead(Client, unloadDBs[station], 0, 342, &buffer);
	//go through the array of reals
	for (int i = 0; i < resultSize; i++) {
		reals.push_back(S7_GetRealAt(buffer, offset));
		offset += 4;
	}
	//read the 32 bit array of bools for fail bits and status bits
	for (int i = 0; i < 32; i++) {

		status.push_back(S7_GetBitAt(buffer, offset, bit));
		bool hasfailed = S7_GetBitAt(buffer, offset + 4, bit);
		if (hasfailed) { out = false; }		// if one of the failbits is equal to 1 (something has gone wrong)
		fail.push_back(hasfailed);
		bit++;
		if (bit == 8) {
			bit = 0;
			offset += 1;
		}
	}
	//if some thing has gone wrong, print out the error code
	if (!out){
		int errorCode = S7_GetDIntAt(buffer, 292); //ASSUMES: standard datablock position of error code double integer
		string errorMessage = S7_GetStringAt(buffer, 296);	//ASUMES: standard datablock posiition of error Message string
		cout << "error code: " << errorCode << "\n";
		cout << "Error Message: " << errorMessage << "\n";

	}
	//add the information to the results table
	string Real = floatsToString(reals);
	string fails = to_string(boolsToInt(fail));
	string statuses = to_string(boolsToInt(status));
	cout << ("INSERT INTO results VALUES(DEFAULT, " + part_fk + ", " + to_string(stations[station]) + ", " + to_string(cycle) + ", " + statuses + ", " + fails + ", ARRAY " + Real + ")").c_str() << "\n";		//stations in this code are indexed at zero, but in postgres indexed at 1, so add one to statoin number when inputing
	res = PQexec(dbconn, ("INSERT INTO results VALUES(DEFAULT, " + part_fk + ", " + to_string(stations[station]) + ", " + to_string(cycle) + ", " + statuses + ", " + fails + ", ARRAY " + Real + ")").c_str());
	checkexec(res, "insert values into results");
	return out;
}
void aggregateComp(string Serial,int station,string part2_fk) {		//takes serial 1 and aggregates it to part2_fk (given by serial 2) in the aggregate_comp table
	res = PQexec(dbconn, ("INSERT INTO aggregate_comp VALUES(DEFAULT, " + part2_fk + ", '" + Serial + "', " + to_string(stations[station]) + ")").c_str());
	checkexec(res, "aggregate Serial 1");

}

bool createPart(string Serial) {	//add part to database, if something goes wrong, return false
	bool out = true;
	printf(("INSERT INTO part VALUES(DEFAULT, '" + Serial + "', 1, FALSE, FALSE)\n").c_str());
	res = PQexec(dbconn, ("INSERT INTO part VALUES(DEFAULT, '" + Serial + "', 1, NOW(), NOW())").c_str());			//*start and finish will probably have to be adjusted*  *model fk question still needs to be confirmed by Gian*
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("could not execute command: Create Part\n");
		out = false;
	}
	return out;
}

string Read(int station, int start = 0, int end = 0, int tagIndex = 0) {		//reads for Serial string in either rockwall or siemens, thakes in offset if second string for siemens (start and end for siemens, tagIndex for rockwell)
	if (Make == "ROCKWELL") {
		return read_stringR(loadtags[tagIndex]);
	}
	else if (Make == "SIEMENS") {
		byte string0[42];
		Cli_DBRead(Client, loadDBs[station], start,end, &string0);	//reads from LOAD datablock to find Serial of part to check
		return S7_GetStringAt(string0, 0);
	}
}
void setBit(int station, int pos, int bit = 0, int tagIndex = 0) {		//sets COMM boolean at given position (pos and bit to be used for Siemens, tagIndex for rockwell)
	if (Make == "SIEMENS") {
		byte checkWork[1];
		Cli_DBRead(Client, stationDBs[station], pos, 1, &checkWork);
		S7_SetBitAt(checkWork, 0, bit, 1);
		Cli_DBWrite(Client, stationDBs[station], pos, 1, &checkWork);
	
	}
	else if (Make == "ROCKWELL") {
		toggle_boolR(commtags[tagIndex], 1);
	}
}
void commCheck(int task, int station, int cycle) {		// takes in task 1:4, station, and cycle #, to run through check work, create part, load serial comp, or start unload
	//CHECK WORK:
	if (task == 1) 
	{
		cout << "check work\n";
		cout << "Station: " << stations[station] << "\n";
		//adds nothing to database, only checks for existence, then function at previous station (assumes that will not be called at station 0?)
		bool checkWork_NOK = FALSE;
		bool checkWork_OK = FALSE;
		string Serial = Read(station, 0, 42, station*6);		//(for rockwell -> comm has 6 variables, so every 6 tags will be Serial 1) 
		if (ifExists(Serial, "serial", "part")) {  //check to see if Serial is in part table (i.e. part has been created)
			//get part pk given Serial:
			res = PQexec(dbconn, ("SELECT * from part where serial = '" + Serial + "'").c_str());
			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
				printf("could not execute command: Find Serial in Part Table\n");
			}
			string part_fk = PQgetvalue(res, 0, 0);
			//Assumes check work is not being done on station 0 --- but should VERIFY that this is an accurate assumption
			//check through past station to make sure no failure
			res = PQexec(dbconn, ("SELECT * FROM results WHERE station_fk ='" + to_string(stations[station]-1) + "' AND part_fk = '" + part_fk + "'").c_str()); 
			if (PQresultStatus(res) != PGRES_TUPLES_OK) {
				printf("No data retrieved\n");
				PQclear(res);
				getchar();
			}
			int rows = PQntuples(res);
			if (rows == 0) {
				printf("Part does not exist in previous station\n");
				checkWork_NOK = TRUE;
			}
			for (int i = 0; i<rows; i++) {
				//PQgetvalue(res, i, 5)) is the fail bit dint reported from each cycle -> probably can just look at last entry into the database rather than all to speed up, but this set up is just in case some error get overlooked
				if (stoi(PQgetvalue(res, i, 5)) != 0) { // if a failbit is not zero
					printf("part has failed on previous station with failbit %s\n", PQgetvalue(res, i, 5));
					checkWork_NOK = TRUE;
				}
			}
			if (!checkWork_NOK) {
				printf("Check Work OK\n");
				checkWork_OK = TRUE;
			}
			PQclear(res);
		}
		else {
			printf("part does not exist in system\n");
			checkWork_NOK = TRUE;
		}

		//set necessary COMM bit to TRUE
		if (checkWork_NOK) {
			setBit(station, 2, 1, station*12+1);
			printf("should set NOK to true\n");
		}
		else if (checkWork_OK) {
			setBit(station, 2, 0, station*12);
			printf("checkWork_OK set to true\n");
		}
		else {
			printf("some path was missed in task 1 where neither were set to true\n");
		}
	}
	// CREATE PART
	if (task == 2) {
		cout << "create part\n";
		bool CreatePart_NOK = FALSE;
		bool CreatePart_OK = TRUE;
		//CHECK Part tbl if Serial
		string Serial = Read(station, 0, 42,station*6);
		//check if part serial is already in database
		if (ifExists(Serial, "serial", "part")) {
			CreatePart_NOK = TRUE;
			printf("part already exists in system\n");
		}
		// something about Checksum????
		else {
			if (createPart(Serial)){ CreatePart_OK = TRUE; }
			else { CreatePart_NOK = TRUE; }
		}

		//set necessary COMM bit to TRUE
		if (CreatePart_NOK) {
			printf("createPart_NOK = true\n");
			setBit(station, 2, 3, station * 12 + 3);
		}
		else if (CreatePart_OK) {
			printf("createPart_OK = true\n");
			setBit(station, 2, 2, station * 12 + 2);
		}
		else {
			printf("some path was missed in task 2 where neither were set to true\n");
		}
	}
	// LOAD SERIAL COMP
	//checks if part 1 serial has been aggregated to part 2 table, then aggregates if not
	if (task == 3) {	 
		cout << "load serial comp\n";
		//check both serials
		//check both in aggregate
		bool loadSerialComp_NOK = FALSE;
		bool loadSerialComp_OK = FALSE;
		string Serial1 = Read(station, 0, 42, station * 6); //CHECK THE 0,41 and 42,84!!!!!!!!!
		string Serial2 = Read(station, 42, 84, station * 6 + 1);
		string part1_pk;
		string part2_pk;
		cout << "string1: " << Serial1 << "\n";
		cout << "string2: " << Serial2 << "\n";
		if (ifExists(Serial1, "serial", "part")) {
			if (ifExists(Serial2, "serial", "part")) {
				//set part2 pk from db
				res = PQexec(dbconn, ("SELECT * FROM part WHERE serial='" + Serial2 + "'").c_str());
				if (PQresultStatus(res) != PGRES_COMMAND_OK) {
					printf("could not execute command: Find Serial2 in Part Table\n");
				}
				part2_pk = PQgetvalue(res, 0, 0); 
				PQclear(res);
				if (ifExists(Serial1, "serial_number", "aggregate_comp")) {
					printf("Serial %s has already been aggregated \n", Serial1.c_str()); // add station # later and part later
					loadSerialComp_NOK = TRUE;
				}
				else {
					loadSerialComp_OK = TRUE;
				}
			}
			else {
				printf("serial: %s does not exist in system\n", Serial2.c_str());
				loadSerialComp_NOK = TRUE;
			}
		}
		else { 
			printf("serial: %s does not exist in system\n", Serial1.c_str()); 
			loadSerialComp_NOK = TRUE;
		}

		//set necessary COMM bit to TRUE
		if (loadSerialComp_NOK) {
			cout << "Serial Comp NOK\n";
			setBit(station, 2, 5, station * 12 + 5);
		}
		else if (loadSerialComp_OK) {
			cout << "Serial Comp OK\n";
			setBit(station, 2, 4, station * 12 + 4);
			aggregateComp(Serial1,station, part2_pk);
		}
		else {
			printf("some path was missed in task 3 where neither were set to true\n");
		}
	} 

	// START UNLOAD -- SAME RULES AS CHECKWorK ??? BUT WITH CHECKSUM TO BE IMPLEMENTED LATER
	if (task == 4) {
		cout << "start unload\n";
		bool startUnload_NOK = FALSE;
		bool startUnload_OK = FALSE;
		string part_fk;	//reads from LOAD datablock to find Serial of part to check (should be first entry to data block)
		string Serial = Read(station,0,42,station*6);
		if (ifExists(Serial, "serial", "part")) {  //check to see if Serial is in part table (i.e. part has been created)
			//									   //get part pk given Serial:
			res = PQexec(dbconn, ("SELECT * from part where serial = '" + Serial + "'").c_str());
			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
				printf("could not execute command: Find Serial in Part Table\n");
				//getchar();
			}
			part_fk = PQgetvalue(res, 0, 0);
			cout << part_fk << "\n";
			//check through current station database to make sure no failure (THIS IS DIFFERENT THAN CHECK WORK, SO VERIFY!)
			cout << ("SELECT * FROM results WHERE station_fk ='" + to_string(stations[station]) + "' AND part_fk = '" + part_fk + "'").c_str() << "\n";
			res = PQexec(dbconn, ("SELECT * FROM results WHERE station_fk ='" + to_string(stations[station]) + "' AND part_fk = '" + part_fk + "'").c_str());
			if (PQresultStatus(res) != PGRES_TUPLES_OK) {
				printf("No data retrieved\n");
				PQclear(res);
				getchar();
			}
			int rows = PQntuples(res);

			//check to see if part has input data in the past, if yes: check is part has failed in past
			if (rows != 0) {
				for (int i = 0; i<rows; i++) {
					if (stoi(PQgetvalue(res, i, 5)) != 0) { // if a failbit is not zero
						printf("part has failed on station with failbit %s\n", PQgetvalue(res, i, 5));
						startUnload_NOK = TRUE;
					}
				}
			}

			// ALSO SOMETHING ABOUT CHECKSUM????
			
			if (!startUnload_NOK) { // if startUnload_NOK has not been set to False, then should be good to read unload and upload
				if (Make == "SIEMENS") {
					if (siemensStartUnload(station, part_fk, cycle)) {	//runs through the unload datablock and checks for failure (returns false if there is a failbit = 1) and uploads to database
						startUnload_OK = TRUE;
					}
					else { startUnload_NOK = TRUE; }
				}
				else if (Make == "ROCKWELL") {
					if (rockwellUnload(station, part_fk, cycle)) {	//runs through the unload datablock and checks for failure (returns false if there is a failbit = 1) and uploads to database
						startUnload_OK = TRUE;
					}
					else { startUnload_NOK = TRUE; }
				}
			}
		}
		else {
			printf("part does not exist in system\n");
			startUnload_NOK = TRUE;
		}

		//now set the COMM bit of startUnload_(N)OK to TRUE
		if (startUnload_NOK) {
			printf("startUnload_NOK=true \n");
			setBit(station, 2, 7, station * 12 + 7);
		}
		else if (startUnload_OK) {
			printf("startUnload_OK = true\n");
			setBit(station, 2, 6, station * 12 + 6);
			//HOW TO DO CYCLE NUMBER???
		}
		else {
			printf("some path was missed in task 4 where neither were set to true\n");
		}

	}
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
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS model(pk SERIAL PRIMARY KEY, name TEXT)");
	checkexec(res, "create model table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS part(pk SERIAL PRIMARY KEY, serial TEXT, model_fk INT REFERENCES model(pk), start TIMESTAMP, finish TIMESTAMP)");
	checkexec(res, "Create part table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS line(pk SERIAL PRIMARY KEY, name TEXT)");
	checkexec(res, "create line table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS station(pk SERIAL PRIMARY KEY, name TEXT, line_fk INT REFERENCES line(pk), final BOOL)");
	checkexec(res, "create station table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS results(pk SERIAL PRIMARY KEY, part_fk INT REFERENCES part(pk), station_fk INT REFERENCES station(pk), cycle_number INTEGER, status_bit INTEGER, fail_bit INTEGER, real REAL[])");
	checkexec(res, "create results table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS results_names(pk SERIAL PRIMARY KEY, station_fk INT REFERENCES station(pk), status_bit TEXT[], real TEXT[], real_unit TEXT[])");
	checkexec(res, "create results names table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS results_limits(pk SERIAL PRIMARY KEY, station_fk INT REFERENCES station(pk), real_max REAL[], real_min REAL[])");
	checkexec(res, "create results limits table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS aggregate_comp(pk SERIAL PRIMARY KEY, part_fk INT REFERENCES part(pk), serial_number TEXT, station_fk INT REFERENCES station(pk))");
	checkexec(res, "create results aggregate_comp table");

	//end database area

	//readxml reads through given xml file and adjusts script state according to plc type and model - also assigns tags and types to read
	readXML("EXAMPLE - rockwell.xml");				// PUT NAME OF XML TO BE READ HERE, can also be an input variable at the top of the script too i guess....

	switch (state) {
	default:
	{
		cout << "No Recognized PLC Models found in XML document \n";
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

		*/
		//current datablock set to DB1
		bool exit = FALSE;
		int cycle = 0;
		//create tags:
		for (int i = 0; i < numStations; i++) {
			//create tags for loads 
			for (int j = 0; j < 6; j++) {
				loadtags.push_back(create_tagR(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size="+ assignTagSize(dataTypeLoad[j])+"&elem_count=1&name=" + loadNames[j] + "_" + to_string(stations[i])).c_str()));
			}
			//create tags for unloads
			for (int j = 0; j < 7; j++) {
				unloadtags.push_back(create_tagR(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=" + assignTagSize(dataTypeUnload[j]) + "&elem_count="+ to_string(elemCount[j])+"&name=" + unloadNames[j] + "_" + to_string(stations[i])).c_str()));
				printf(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=" + assignTagSize(dataTypeUnload[j]) + "&elem_count=" + to_string(elemCount[j]) + "&name=" + unloadNames[j] + "_" + to_string(stations[i]) + "\n").c_str());
			}
			//create tags for comm
			for (int j = 0; j < 12; j++) {
				commtags.push_back(create_tagR(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=" + assignTagSize(dataTypeComm[j]) + "&elem_count=1&name=" + commNames[j] + "_" + to_string(stations[i])).c_str()));	
			}
		}
		while (!exit) {
			//continuous read of COMM for all stations:
			for (int i = 0; i < numStations; i++) {
				for (int j = 0; j < 4; j++) {
					//try to set up async read here to speed up process -> probably won't work based on examples :/
					bool task = read_boolR(commtags[i*12+j]);
					//
					if (task) { commCheck(j + 1, i, cycle); } 
				}
			}
			cout << "exit?\n";
			if (getchar() != 'n') {
				cout << "exiting\n";
				Cli_Disconnect(Client);
				exit = TRUE;
			}
			getchar();
			cycle++;
		}

		cout << "exit?\n";
		if (getchar() != 'n') {
			destroyRockTags();
			PQfinish(dbconn);
			getchar();
			return 0;
		}
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
			bool exit = FALSE;
			int cycle = 0;
			while(!exit){
			//continuous read of COMM for all stations:
				//create vector of 2 byte buffers to store data
				array < byte, 20> buffer;
				// Prepare struct
				TS7DataItem Items[20];		//ASSUMES NUMBER OF STATIONS WILL BE NO MORE THAN 20, IF # STATIONS WILL BE MORE, THEN DO A SECOND MULTIREAD!

				// NOTE : *AMOUNT IS NOT SIZE* , it's the number of items
				for (int i=0; i<numStations;i++){
					byte tempBuffer;
					// datablock reads
					Items[i].Area = S7AreaDB;
					Items[i].WordLen = S7WLByte;
					Items[i].DBNumber = stationDBs[i];        // ASSUMES DB STATIONS ARE IN ORDER ---- NEED TO BE IN ORDER ON THE XML
					Items[i].Start = 0;        // Starting from 0
					Items[i].Amount = 1;       // only need to real the 4 bools continuously
					Items[i].pdata = &buffer[i];
				}

				int sres = Cli_ReadMultiVars(Client, &Items[0], numStations);
				if (Check(sres, "datablock multiRead")) {
					for (int i = 0; i < numStations; i++) {
						for (int j = 0; j < 4; j++) {
							bool task = S7_GetBitAt(&buffer[i], 0, j);
							if (task) { commCheck(j+1, i, cycle); }
						}
					}
				}
				cout << "exit?\n";
				if (getchar() != 'n') {
					cout << "exiting\n";
					Cli_Disconnect(Client);
					exit = TRUE;
				}
				getchar();
				cycle++;
			}				
	
			//upload results to db
			string reals = floatsToString(real);
			//res = PQexec(dbconn, ("INSERT INTO testing VALUES(DEFAULT, 4000, ARRAY " + reals + ")").c_str());
			//checkexec(res, "insert values into testing");
			readDB();
			cout << "exit?\n";
			if (getchar() != 'n') {
				//Cli_Disconnect(Client);
				PQfinish(dbconn);
				getchar();
				return 0;
			}

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
			bool exit = FALSE;
			int cycle = 0;
			while (!exit) {
				//continuous read of COMM for all stations:
				//create vector of 2 byte buffers to store data
				array < byte, 20> buffer;
				// Prepare struct
				TS7DataItem Items[20];		//ASSUMES NUMBER OF STATIONS WILL BE NO MORE THAN 20, IF # STATIONS WILL BE MORE, THEN DO A SECOND MULTIREAD!

											// NOTE : *AMOUNT IS NOT SIZE* , it's the number of items
				for (int i = 0; i<numStations; i++) {
					byte tempBuffer;
					// datablock reads
					Items[i].Area = S7AreaDB;
					Items[i].WordLen = S7WLByte;
					Items[i].DBNumber = stationDBs[i];        // ASSUMES DB STATIONS ARE IN ORDER ---- NEED TO BE IN ORDER ON THE XML
					Items[i].Start = 0;        // Starting from 0
					Items[i].Amount = 1;       // only need to real the 4 bools continuously
					Items[i].pdata = &buffer[i];
				}

				int sres = Cli_ReadMultiVars(Client, &Items[0], numStations);
				if (Check(sres, "datablock multiRead")) {
					for (int i = 0; i < numStations; i++) {
						for (int j = 0; j < 4; j++) {
							bool task = S7_GetBitAt(&buffer[i], 0, j);
							if (task) { commCheck(j + 1, i, cycle); }
						}
					}
				}
				cout << "exit?\n";
				if (getchar() != 'n') {
					cout << "exiting\n";
					Cli_Disconnect(Client);
					exit = TRUE;
				}
				getchar();
				cycle++;
			}

			//upload results to db
			string reals = floatsToString(real);
			//res = PQexec(dbconn, ("INSERT INTO testing VALUES(DEFAULT, 4000, ARRAY " + reals + ")").c_str());
			//checkexec(res, "insert values into testing");
			readDB();
			cout << "exit?\n";
			if (getchar() != 'n') {
				//Cli_Disconnect(Client);
				PQfinish(dbconn);
				getchar();
				return 0;
			}

		}
		cout << "press any key to EXIT";
		getchar();

		PQfinish(dbconn);
		return 0;
	}
	case ERROR:
	{
		// no current use for this, but set up in case of running into additional errors
		cout << "Error?";
		getchar();

		PQfinish(dbconn);
		return 0;
	}
	}


}


