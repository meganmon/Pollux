/*
TO DO:
station number vs station fk -> current assumption is that station number will also be the foreign/primary key, but set up in place for other also

Major Questions:
Do I reset the values of the acc bools everytime, or assume that the other side will reset them after they have been read? *****

-~~-~-~-~-~-~ CONTINUE -~-~-~-~-~-~-~-~
rockwell testing
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
#include <cmath>
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

//end of set that could be deleted (probably)

vector <float> real;		//stores results from results in Unload datablock
vector <int> stations;		//stores the station #'s (in case not starting from 0)
int numStations;			//number of total stations
int resultSize;				//number of reals in results variable in unload datablock
vector <string> stationNames;
vector <string> stationFk;

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

/*SET UP FOR miscellaneous functions*/

//converts float array to string for uploading to DB
string floatsToString(vector <float> real) {		
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

//reads default database given a table and a key (but key is not necessary)
void readDB(string table, int itemCount = 1, string key = "*", string extras = "") {
	res = PQexec(dbconn, ("SELECT " + key + " FROM " + table + extras).c_str());
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		printf("No data retrieved\n");
		PQclear(res);
		getchar();
	}
	int rows = PQntuples(res);
	if (rows == 0) {
		printf("not exists");
	}
	int items = itemCount;
	for (int i = 0; i<rows; i++) {
		for (int j = 0; j < items; j++) {
			cout << PQgetvalue(res, i, j);
			if (j != items - 1) {
				cout << ", ";
			}
		}
		cout << "\n";
	}
	PQclear(res);
}

// for converting status and failure bits to dint to upload to database
double boolsToDInt(vector <bool> bools) {			
	double out = 0;
	int count = 32;
	for (int i = 0; i < count; i++) {
		if (bools[i]) {
			out += pow(2, i);
		}
	}
	return out;
}

//CHECKS IF THE IDENTIFIER IS ALREADY IN GIVEN TABLE
bool ifExists(string identifier, string column, string table) {
	//cout << ("SELECT * FROM " + table + " WHERE " + column + " = '" + identifier + "'").c_str() << "\n";
	res = PQexec(dbconn, ("SELECT * FROM " + table + " WHERE " + column + " = '" + identifier + "'").c_str());
	if (PQresultStatus(res) != PGRES_TUPLES_OK) {
		//printf("No data retrieved\n");
		PQclear(res);
		return 0;
	}
	int rows = PQntuples(res);
	if (rows == 0) {
		PQclear(res);
		//	printf("Does not exist\n");
		return 0;
	}
	else {
		PQclear(res);
		return 1;
	}
}

//reads through XML document "file" to assign variables and tags to read, then assigns state according to PLC type
void readXML(const char *file) {		
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
					stationNames.push_back(datablock_node->first_attribute("tagend")->value());
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

		//SET UP FOR IF WANTING TO SEARCH FOR STATION GIVEN NAME RATHER THAN ASSUMING PK:
		//for (int i = 0; i < numStations; i++) {
		//	if (ifExists(stationNames[i], "name", "station")) {
		//		res = PQexec(dbconn, ("SELECT pk from station where name = '" + stationNames[i] + "'").c_str());
		//		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		//			//	printf("could not execute command: Find Serial in Part Table\n");
		//		}
		//		stationFk.push_back(PQgetvalue(res, 0, 0)); //spits out string, so no longer need to use to_string() in all of the subsequent functions
		//	}
		//}

		//assign the state that 
		if (Make == "ROCKWELL") {
			cout << "Model recognized as Rockwell\n";
			state = 1;
		}
		if (Make == "SIEMENS") {
			Client = Cli_Create(); // initialize the client connection
			Model = plc_node->first_attribute("Model")->value(); // assign the model 
			if (Model == "300") { // fill out for the different models that use this set up rather than the other one
				Rack = 0;
				Slot = 2;
				state = 2;
			}
			else {
				Rack = 0;
				Slot = 1;		//this might need to be adjusted depending on hardware configuration and model of PLC. If to be adjusted, see set up guide: http://snap7.sourceforge.net/sharp7.html
				state = 2;
			}
		}
	}
}

//checks to execution of Postgresql command	 -- it clears result after checking, so do not use if trying to read data from the result after calling
void checkexec(PGresult *res, const char *function) {			
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("could not execute command: %s\n", function);
		getchar();
	}
	PQclear(res);

}

//reads unload block of Rockwell, looks for fail bits; if no failure, returns true; all results, status bits, and fail bits are uploaded to database regardless of failures
bool rockwellUnload(int station, string part_fk, int cycle) {		
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
		bool isgood = status[i];
		if (hasfailed) { out = false; }		// if one of the failbits is equal to 1 (something has gone wrong)
		else if (!isgood) { out = false; }
	}
	//if some thing has gone wrong, print out the error code
	if (!out) {
		double errorCode = read_intR(unloadtags[station*7+4]); 
		string errorMessage = read_stringR(unloadtags[station*7+5]);	
		cout << "error code: " << errorCode << "\n";
		cout << "Error Message: " << errorMessage << "\n";

	}
	//add the information to the results table
	string Real = floatsToString(reals);
	string fails = to_string(boolsToDInt(fail));
	string statuses = to_string(boolsToDInt(status));
//	cout << ("INSERT INTO results VALUES(DEFAULT, " + part_fk + ", " + to_string(stations[station]) + ", " + to_string(cycle) + ", " + statuses + ", " + fails + ", ARRAY " + Real + ")").c_str() << "\n";		//stations in this code are indexed at zero, but in postgres indexed at 1, so add one to statoin number when inputing
	res = PQexec(dbconn, ("INSERT INTO results VALUES(DEFAULT, " + part_fk + ", " + to_string(stations[station]) + ", " + to_string(cycle) + ", " + statuses + ", " + fails + ", ARRAY " + Real + ")").c_str());
	checkexec(res, "insert values into results");
	return out;
}

//reads unload block of Siemens, looks for fail bits; if no failure, returns true; all results, status bits, and fail bits are uploaded to database regardless of failures
bool siemensStartUnload(int station, string part_fk, int cycle) {		
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

		bool isgood = S7_GetBitAt(buffer, offset, bit);
		bool hasfailed = S7_GetBitAt(buffer, offset + 4, bit);
		if (hasfailed || !isgood) { out = false; }		// if one of the failbits is equal to 1 (something has gone wrong)
		fail.push_back(hasfailed);
		status.push_back(isgood);
		bit++;
		if (bit == 8) {
			bit = 0;
			offset += 1;
		}
	}
	//if some thing has gone wrong, print out the error code
	if (!out){
		double errorCode = S7_GetDIntAt(buffer, 292); //ASSUMES: standard datablock position of error code double integer
		string errorMessage = S7_GetStringAt(buffer, 296);	//ASUMES: standard datablock posiition of error Message string
		cout << "error code: " << errorCode << "\n";
		cout << "Error Message: " << errorMessage << "\n";

	}
	//add the information to the results table
	string Real = floatsToString(reals);
	string fails = to_string(boolsToDInt(fail));
	string statuses = to_string(boolsToDInt(status));
//	cout << ("INSERT INTO results VALUES(DEFAULT, " + part_fk + ", " + to_string(stations[station]) + ", " + to_string(cycle) + ", " + statuses + ", " + fails + ", ARRAY " + Real + ")").c_str() << "\n";		//stations in this code are indexed at zero, but in postgres indexed at 1, so add one to statoin number when inputing
	res = PQexec(dbconn, ("INSERT INTO results VALUES(DEFAULT, " + part_fk + ", " + to_string(stations[station]) + ", " + to_string(cycle) + ", " + statuses + ", " + fails + ", ARRAY " + Real + ")").c_str());
	checkexec(res, "insert values into results");
	return out;
}

//takes serial 1 and aggregates it to part2_fk (given by serial 2) in the aggregate_comp table
void aggregateComp(string Serial,int station,string part2_fk) {		
	res = PQexec(dbconn, ("INSERT INTO aggregate_comp VALUES(DEFAULT, " + part2_fk + ", '" + Serial + "', " + to_string(stations[station]) + ")").c_str());
	checkexec(res, "aggregate Serial 1");

}

//add part to database, if something goes wrong, return false
bool createPart(string Serial, string modelName) {	
	bool out = true;
	string model_fk;
	res = PQexec(dbconn, ("SELECT * FROM model WHERE name='" + modelName + "'").c_str());
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
	//	printf("could not execute command: Find model_fk in Part Table\n");
	}
	if (PQntuples(res) != 0) {
		model_fk = PQgetvalue(res, 0, 0);
	//	cout << model_fk << "\n";
		PQclear(res);
		//printf(("INSERT INTO part VALUES(DEFAULT, '" + Serial + "', " + model_fk + ", NOW(), NOW())\n").c_str());
		res = PQexec(dbconn, ("INSERT INTO part VALUES(DEFAULT, '" + Serial + "', " + model_fk + ", NOW(), NOW())").c_str());			//*start and finish will probably have to be adjusted*  *model fk question still needs to be confirmed by Gian*
		if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			printf("could not execute command: Create Part\n");
			out = false;
		}
	}
	else { out = false; }
	return out;
}

//reads for Serial string in either rockwall or siemens load datablocks, thakes in offset if second string for siemens (start and end for siemens, tagIndex for rockwell)
string Read(int station, int start, int end, int tagIndex = 0) {		
	if (Make == "ROCKWELL") {
		return read_stringR(loadtags[tagIndex]);
	}
	else if (Make == "SIEMENS") {
		byte string0[126];
		string out;
		Cli_DBRead(Client, loadDBs[station], start,end-start, &string0);	//reads from LOAD datablock to find Serial of part to check
		out = S7_GetStringAt(string0, 0);
		return out;
	}
}

//sets COMM boolean at given position (pos and bit to be used for Siemens, tagIndex for rockwell)
void setBit(int station, int pos, int bit = 0, int tagIndex = 0) {		
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

// takes in task 1:4, station, and cycle #, to run through check work, create part, load serial comp, or start unload
void commCheck(int task, int station, int cycle) {		
	
	//CHECK WORK:	- add in the status bit part
	if (task == 1) 
	{
		cout << "check work on station " << stations[station] <<"\n";
	//	cout << "Station: " << stations[station] << "\n";
		//adds nothing to database, only checks for existence, then function at previous station (assumes that will not be called at station 0?)
		bool checkWork_NOK = FALSE;
		bool checkWork_OK = FALSE;
		string Serial = Read(station, 0, 42, station*7);		//(for rockwell -> load has 7 variables, so every 7 tags will be Serial 1) 
		if (ifExists(Serial, "serial", "part")) {  //check to see if Serial is in part table (i.e. part has been created)
			//get part pk given Serial:
			res = PQexec(dbconn, ("SELECT * from part where serial = '" + Serial + "'").c_str());
			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			//	printf("could not execute command: Find Serial in Part Table\n");
			}
			string part_fk = PQgetvalue(res, 0, 0);
			//check through past station to make sure no failure
			res = PQexec(dbconn, ("SELECT * FROM results WHERE station_fk ='" + to_string(stations[station]-1) + "' AND part_fk = '" + part_fk + "'").c_str()); 
			if (PQresultStatus(res) != PGRES_TUPLES_OK) {
				printf("No data retrieved\n");
				PQclear(res);
			//	getchar();
			}
			int rows = PQntuples(res);
			if (rows == 0) {
				printf("Part does not exist in previous station\n");
				checkWork_NOK = TRUE;
			}
			for (int i = 0; i<rows; i++) {
				//PQgetvalue(res, i, 5)) is the fail bit dint reported from each cycle -> probably can just look at last entry into the database rather than all to speed up, but this set up is just in case some error got overlooked
				if (stoi(PQgetvalue(res, i, 5)) != 0) { // if a failbit is not zero
					printf("part has failed on previous station with failbit %s\n", PQgetvalue(res, i, 5));
					checkWork_NOK = TRUE;
				}
				else if (PQgetvalue(res, i, 4) != "4294967295") {				//if all of the status bits equal to one? add this in or nah?

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
			setBit(station, 2, 1, station*9+2);
			printf("should set NOK to true\n");
		}
		else if (checkWork_OK) {
			setBit(station, 2, 0, station*9+1);
			printf("checkWork_OK set to true\n");
		}
		else {
			printf("some path was missed in task 1 where neither were set to true\n");
		}
	}

	// CREATE PART
	if (task == 2) {
		cout << "create part on station " << stations[station] << "\n";
		bool CreatePart_NOK = FALSE;
		bool CreatePart_OK = TRUE;
		//CHECK Part tbl if Serial
		string modelName = Read(station, 84, 126);
		cout << "Model name: " << modelName << "	Serial: ";
		string Serial = Read(station, 0, 42,station*7);
		cout << Serial << "\n";
		//check if part serial is already in database
		if (ifExists(Serial, "serial", "part")) {
			CreatePart_NOK = TRUE;
			printf("part already exists in system\n");
		}
		// something about Checksum????
		else {
			if (createPart(Serial, modelName)){ CreatePart_OK = TRUE; }
			else { CreatePart_NOK = TRUE; }
		}
		//set necessary COMM bit to TRUE
		if (CreatePart_NOK) {
			printf("createPart_NOK = true\n");
			setBit(station, 2, 3, station * 9 + 4);
		}
		else if (CreatePart_OK) {
			printf("createPart_OK = true\n");
			setBit(station, 2, 2, station * 9 + 3);
		}
		else {
			printf("some path was missed in task 2 where neither were set to true\n");
		}
	}

	// LOAD SERIAL COMP
	//checks if part 1 serial has been aggregated to part 2 table, then aggregates if not
	if (task == 3) {	 
		cout << "load serial comp on station " << stations[station] << "\n";
		//check both serials
		//check both in aggregate
		bool loadSerialComp_NOK = FALSE;
		bool loadSerialComp_OK = FALSE;
		string Serial1 = Read(station, 0, 42, station * 7); 
		string Serial2 = Read(station, 42, 84, station * 7 + 1);
		string part1_pk;
		string part2_pk;
		cout << "Part 1: " << Serial1 << "\n";
		cout << "Part 2: " << Serial2 << "\n";
		if (ifExists(Serial1, "serial", "part")) {
			if (ifExists(Serial2, "serial", "part")) {
				//set part2 pk from db
				res = PQexec(dbconn, ("SELECT * FROM part WHERE serial='" + Serial2 + "'").c_str());
				if (PQresultStatus(res) != PGRES_COMMAND_OK) {
					//printf("could not execute command: Find Serial2 in Part Table\n");
				}
				part2_pk = PQgetvalue(res, 0, 0); 
				PQclear(res);
				if (ifExists(Serial1, "serial_number", "aggregate_comp")) {
					res = PQexec(dbconn,("SELECT station_fk FROM aggregate_comp WHERE serial_number = '" + Serial1 + "'").c_str());
					string stationfk = PQgetvalue(res, 0, 0);
					PQclear(res);
					printf("Serial %s has already been aggregated on station %s\n", Serial1.c_str(), stationfk.c_str()); // add station # later and part later
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
			setBit(station, 2, 5, station * 9 + 6);
		}
		else if (loadSerialComp_OK) {
			cout << "Serial Comp OK\n";
			setBit(station, 2, 4, station * 9 + 5);
			aggregateComp(Serial1,station, part2_pk);
		}
		else {
			printf("some path was missed in task 3 where neither were set to true\n");
		}
	} 

	// START UNLOAD -- SAME RULES AS CHECKWorK ??? BUT WITH CHECKSUM TO BE IMPLEMENTED LATER -- add in the status bit part
	if (task == 4) {
		cout << "start unload on station " << stations[station] << "\n";
		bool startUnload_NOK = FALSE;
		bool startUnload_OK = FALSE;
		string part_fk;	//reads from LOAD datablock to find Serial of part to check (should be first entry to data block)
		string Serial = Read(station,0,42,station*7);
		int cycleNum;
		if (ifExists(Serial, "serial", "part")) {  //check to see if Serial is in part table (i.e. part has been created)
			//									   //get part pk given Serial:
			res = PQexec(dbconn, ("SELECT * from part where serial = '" + Serial + "'").c_str());
			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
			//	printf("could not execute command: Find Serial in Part Table\n");
				//getchar();
			}
			part_fk = PQgetvalue(res, 0, 0);
			//cout << part_fk << "\n";
			//check through current station database to make sure no failure													 *! do we need to do this?
			//cout << ("SELECT * FROM results WHERE station_fk ='" + to_string(stations[station]) + "' AND part_fk = '" + part_fk + "'").c_str() << "\n";
			res = PQexec(dbconn, ("SELECT * FROM results WHERE station_fk ='" + to_string(stations[station]) + "' AND part_fk = '" + part_fk + "'").c_str());
			if (PQresultStatus(res) != PGRES_TUPLES_OK) {
				//printf("No data retrieved\n");
				PQclear(res);
			//	getchar();
			}
			int rows = PQntuples(res);

			//check to see if part has input data in the past, if yes: check is part has failed in past
			if (rows != 0) {
				for (int i = 0; i<rows; i++) {
					if (stoi(PQgetvalue(res, i, 5)) != 0) { // if a failbit is not zero
						printf("part has failed on current station with failbit %s\n", PQgetvalue(res, i, 5));
						startUnload_NOK = TRUE;
					}
				}
			}

			// ALSO SOMETHING ABOUT CHECKSUM????
			
			if (!startUnload_NOK) { // if startUnload_NOK has not been set to False, then should be good to read unload and upload
				if (Make == "SIEMENS") {
					byte data[4];
					Cli_DBRead(Client, loadDBs[station], 176, 4, &data);	//reads from LOAD datablock to find Serial of part to check
					cycleNum = S7_GetDIntAt(data, 0);			//set up for if reading cycle from datablock
					if (siemensStartUnload(station, part_fk, cycleNum)) {	//runs through the unload datablock and checks for failure (returns false if there is a failbit = 1) and uploads to database
						startUnload_OK = TRUE;
					}
					else { startUnload_NOK = TRUE; }
				}
				else if (Make == "ROCKWELL") {
					int cycleNum = read_intR(loadtags[station*12+6]);
					if (rockwellUnload(station, part_fk, cycleNum)) {	//runs through the unload datablock and checks for failure (returns false if there is a failbit = 1) and uploads to database
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
			setBit(station, 2, 7, station * 12 + 8);
		}
		else if (startUnload_OK) {
			printf("startUnload_OK = true\n");
			setBit(station, 2, 6, station * 12 + 7);
		}
		else {
			printf("some path was missed in task 4 where neither values were set to true\n");
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
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS results(pk SERIAL PRIMARY KEY, part_fk INT REFERENCES part(pk), station_fk INT REFERENCES station(pk), cycle_number INTEGER, status_bit BIGINT, fail_bit BIGINT, real REAL[])");
	checkexec(res, "create results table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS results_names(pk SERIAL PRIMARY KEY, station_fk INT REFERENCES station(pk), status_bit TEXT[], real TEXT[], real_unit TEXT[])");
	checkexec(res, "create results names table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS results_limits(pk SERIAL PRIMARY KEY, station_fk INT REFERENCES station(pk), real_max REAL[], real_min REAL[])");
	checkexec(res, "create results limits table");
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS aggregate_comp(pk SERIAL PRIMARY KEY, part_fk INT REFERENCES part(pk), serial_number TEXT, station_fk INT REFERENCES station(pk))");
	checkexec(res, "create results aggregate_comp table");

	//end database area

	//readxml reads through given xml file and adjusts script state according to plc type and model - also assigns tags and types to read
	readXML("EXAMPLE.xml");				// PUT NAME OF XML TO BE READ HERE, can also be an input variable at the top of the script too i guess....

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
			for (int j = 0; j < 7; j++) {
				loadtags.push_back(create_tagR(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size="+ assignTagSize(dataTypeLoad[j])+"&elem_count=1&name=" + stationNames[i] + "_Load_" + loadNames[j]).c_str()));
				printf(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=" + assignTagSize(dataTypeLoad[j]) + "&elem_count=1&name=" + stationNames[i] + "_Load_" + loadNames[j] + "\n").c_str());
			}
			//create tags for unloads
			for (int j = 0; j < 7; j++) {
				unloadtags.push_back(create_tagR(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=" + assignTagSize(dataTypeUnload[j]) + "&elem_count="+ to_string(elemCount[j])+"&name=" + stationNames[i] + "_Unload_" + unloadNames[j]).c_str()));
				printf(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=" + assignTagSize(dataTypeUnload[j]) + "&elem_count=" + to_string(elemCount[j]) + "&name=" + stationNames[i] + "_Unload_" + unloadNames[j] + "\n").c_str());
			}
			//create tags for comm
			for (int j = 0; j < 9; j++) {
				int count;
				if (j == 0) { count = 4; }
				else { count = 1; }
				commtags.push_back(create_tagR(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=" + assignTagSize(dataTypeComm[j]) + "&elem_count=" + to_string(count) + "&name=" + stationNames[i] + "_Comm_" + commNames[j]).c_str()));
				printf(("protocol=ab_eip&gateway=" + IP_address + "&path=1,0&cpu=LGX&elem_size=" + assignTagSize(dataTypeComm[j]) + "&elem_count=" + to_string(count) + "&name=" + stationNames[i] + "_Comm_" + commNames[j] + "\n").c_str());
			}
		}
		getchar();
		while (!exit) {
			if (GetAsyncKeyState(VK_ESCAPE)) {
				cout << "Exiting\n";
				exit = true;
			}
			//continuous read of COMM for all stations:
			vector <bool> taskArray = { false,false,false,true };
			for (int i = 0; i < numStations; i++) {
				//vector <bool> taskArray = readBoolArray(commtags[i * 9], 4);
				for (int j = 0; j < 4; j++) {
					//try to set up async read here to speed up process -> probably won't work based on examples :/
					bool task = taskArray[j];
					//
					if (task) { commCheck(j + 1, i, cycle); } 
				}
			}
			cycle++;
			Sleep(1500);
		}

		///* we are done */
		printf("Press any key to exit");
		PQfinish(dbconn);
		getchar();
		return 0;
	}
	case SIEMENS300:
	{
		Address = IP_address.c_str();
//		Address = "10.0.0.9";
		cout << "Siemens" << "\n";
		if (CliConnect()){
			bool exit = FALSE;
			int cycle = 0;
			cout << "Press ESC to exit\n";
			while(!exit){
				if (GetAsyncKeyState(VK_ESCAPE)) {
					cout << "Exiting\n";
					Cli_Disconnect(Client);
					exit = true; 
				}
			//continuous read of COMM for all stations:
				//create vector of 2 byte buffers to store data
				array < byte, 20> buffer;
				TS7DataItem Items[20];		//ASSUMES NUMBER OF STATIONS WILL BE NO MORE THAN 20, IF # STATIONS WILL BE MORE, THEN DO A SECOND MULTIREAD!

				//// NOTE : *AMOUNT IS NOT SIZE* , it's the number of items, so for size, multiple wordLen by amount
				for (int i=0; i<numStations;i++){
					// datablock reads
					Items[i].Area = S7AreaDB;
					Items[i].WordLen = S7WLByte;			//go to snap7.h to find more about the length and area definitions
					Items[i].DBNumber = stationDBs[i];        // ASSUMES DB STATIONS ARE IN ORDER ---- NEED TO BE IN ORDER ON THE XML
					Items[i].Start = 0;        // Starting from beginning of datablock (booleans to read are the first part of datablock, if that order changes, then adjust here)
					Items[i].Amount = 1;       // only need to real the 4 bools continuously -> so therefore only need to grab 1 byte
					Items[i].pdata = &buffer[i];		//where the info is saved
				}
				int sres = Cli_ReadMultiVars(Client, &Items[0], numStations);
				if (Check(sres, "datablock multiRead")) {
					for (int i = 0; i < numStations; i++) {
						for (int j = 0; j < 4; j++) {
							//cycle is changed in task 4 of commCheck under variable cycleNum
							bool task = S7_GetBitAt(&buffer[i], 0, j);
							if (task) { commCheck(j+1, i, cycle); }
						}
					}
				}
				cycle++;				//only if cycle is not to be read off of datablocks
				Sleep(1500);			//only for testing/reading
			}		
			//read results form db
			string reals = floatsToString(real);
			readDB("results", 7, "*", " ORDER BY PK DESC LIMIT 10");
			cout << "Exit?\n";
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


