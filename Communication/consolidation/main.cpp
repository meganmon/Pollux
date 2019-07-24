/*
MONDAY:
message Gian or Jonas to go over progress and address create part, station fk, and aggregate parts questions -> then go over how it will work for Rockwell
	-confirm that current method of predetermined datablock will work (otherwise ~1/2-1 more day of work)
TO DO:
- MODEL FK -> given only serial, how do we know model foreign key, or is name also serial number? 
AGG TABLE IN GENERAL
station number vs station fk -> will these correspond or no?
START AND FINISH - add part -> what do these do/ what even are the data types? How do I know what to set them to

-~-~- DONE UNTIL QUESTIONS/TESTING-~-~-~
results table -> improve after question (station fk is station number or what will I use as identifier?)
createPart(); --> improve after questions (what is start finsih?), (will i search model for model fk? how to get name?)
aggregateParts();  --> confused about how to do this one..... (just what? both Serials added to aggregate table?)
check through commCheck
-~~-~-~-~-~-~ CONTINUE -~-~-~-~-~-~-~-~
Redo for Rockwell!!!

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

vector <string> tagTypes;
vector <string> tagNames;
vector <int32_t> myTags;
vector <string> tagSizes;
vector <int> tagOffsets;
vector <float> real;
vector <int> stations;
vector <int> stationDBs;
vector <int> loadDBs;
vector <int> unloadDBs;
vector <string> loadtags;
vector <string> unloadtags;
vector <string> stationtags;
/*IS THERE ONLY ONE LOAD AND UNLOAD DB OR IS IT PER STATION*/
size_t numStations;
int resultSize;
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
long int boolsToInt(vector <bool> bools) {
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
					loadDBs.push_back(stoi(datablock_node->first_attribute("dataBlock")->value()));
				}
				else if (dbname == "unload") {
					unloadDBs.push_back(stoi(datablock_node->first_attribute("dataBlock")->value()));
					for (xml_node<> *data_node = datablock_node->first_node("DATA"); data_node; data_node = data_node->next_sibling()) {
						string name = data_node->first_attribute("name")->value();
						transform(name.begin(), name.end(), name.begin(), ::tolower);
						if (name == "result") {
							resultSize = stoi(data_node->first_attribute("array")->value());
						}
					}
				}
				else if (dbname == "comm") {
					stationDBs.push_back(stoi(datablock_node->first_attribute("dataBlock")->value()));
					stations.push_back(stoi(datablock_node->first_attribute("station")->value()));
				}
			}
			else if (Make == "ROCKWELL") {
				if (dbname == "load") {
					loadtags.push_back(datablock_node->first_attribute("tag")->value());
				}
				else if (dbname == "unload") {
					unloadtags.push_back(datablock_node->first_attribute("tag")->value());
					for (xml_node<> *data_node = datablock_node->first_node("DATA"); data_node; data_node = data_node->next_sibling()) {
						string name = data_node->first_attribute("name")->value();
						transform(name.begin(), name.end(), name.begin(), ::tolower);
						if (name == "result") {
							resultSize = stoi(data_node->first_attribute("array")->value());
						}
					}
				}
				else if (dbname == "comm") {
					stationDBs.push_back(stoi(datablock_node->first_attribute("dataBlock")->value()));
				}
			}
			//else {
			//	for (xml_node<> * data_node = datablock_node->first_node("DATA"); data_node; data_node = data_node->next_sibling()) {
			//		tempType = data_node->first_attribute("Type")->value();
			//		transform(tempType.begin(), tempType.end(), tempType.begin(), ::tolower);
			//		tagTypes.push_back(tempType);
			//		tagNames.push_back(data_node->first_attribute("name")->value());
			//		if (Make == "ROCKWELL") {
			//			tagSizes.push_back(assignTagSize(tempType));
			//		}
			//		else if (Make == "SIEMENS") {
			//			tagOffsets.push_back(assignTagOffset(tempType));
			//		}
			//	}
			//}
			/* THIS IS CURRENTLY SET UP FOR A STANDARD DATABLOCK, IF NOT GOING TO BE STANDARD, NEEDS TO ADAPT*/

		}
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
void readDB() {
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
bool ifExists(string identifier, string column, string table) {			//CHECKS IF THE IDENTIFIER IS ALREADY IN TABLE
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
void checkexec(PGresult *res, const char *function) {			//checks to make sure Postgresql command went through	
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("could not execute command: %s\n", function);
		getchar();
	}
	PQclear(res);

}

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
void aggregateComp(string Serial1,string Serial2,int station,string part1_fk,string part2_fk) {
	res = PQexec(dbconn, ("INSERT INTO aggregate_comp VALUES(DEFAULT, " + part1_fk + ", '" + Serial1 + "', " + to_string(stations[station]) + ")").c_str());
	checkexec(res, "aggregate Serial 1");
	res = PQexec(dbconn, ("INSERT INTO aggregate_comp VALUES(DEFAULT, " + part2_fk + ", '" + Serial2 + "', " + to_string(stations[station]) + ")").c_str());
	checkexec(res, "aggregate Serial 2");

}

bool createPart(string Serial) {
	//add part to database, if something goes wrong, return false
	bool out = true;
	printf(("INSERT INTO part VALUES(DEFAULT, '" + Serial + "', 1, FALSE, FALSE)\n").c_str());
	res = PQexec(dbconn, ("INSERT INTO part VALUES(DEFAULT, '" + Serial + "', 1, FALSE, FALSE)").c_str());
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("could not execute command: Create Part\n");
		out = false;
	}
	return out;
}
void commCheck(int task, int station, int cycle) {		// takes in task 0:3, station, and cycle #(?), to run through check work, create part, load serial comp, or start unload
	//CHECK WORK:
	if (task == 1) 
	{
		cout << "check work\n";
		cout << "Station: " << stations[station] << "\n";
		//adds nothing to database, only checks for existence, then function at previous station (assumes that will not be called at station 0?)
		bool checkWork_NOK = FALSE;
		bool checkWork_OK = FALSE;
		byte string0[42];
		Cli_DBRead(Client, loadDBs[station], 0, 42, &string0);	//reads from LOAD datablock to find Serial of part to check (should be first entry to data block)
		string Serial = S7_GetStringAt(string0, 0);
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
		byte checkWork[1];
		Cli_DBRead(Client, stationDBs[station], 2, 1, &checkWork);
		//THIS IS WHERE MORE WORK CAN BE DONE TO READ OFF OF XML AND SET POSITION RATHER THAN HARDCODING
		if (checkWork_NOK) {
			S7_SetBitAt(checkWork, 0, 1, 1);		//0.5 is where CheckWork_NOK should be
			printf("should set NOK to true\n");
			Cli_DBWrite(Client, stationDBs[station], 2, 1, &checkWork);
		}
		else if (checkWork_OK) {
			S7_SetBitAt(checkWork, 0, 0, 1);		// 0.4 is where CheckWork_OK should be
			Cli_DBWrite(Client, stationDBs[station], 2, 1, &checkWork);
		}
		else {
			printf("some path was missed in task 1 where neither were set to true\n");
		}/*
		byte check;
		Cli_DBRead(Client, stationDBs[station], 2, 1, &check);
		for (int i = 0; i < 8; i++) {
			cout << S7_GetBitAt(&check, 0, i) << "\n";
		}*/
	}
	// CREATE PART
	if (task == 2) {
		cout << "create part\n";
		bool CreatePart_NOK = FALSE;
		bool CreatePart_OK = TRUE;
		//CHECK Part tbl if Serial
		byte string0[42];
		Cli_DBRead(Client, loadDBs[station], 0, 42, &string0);
		string Serial = S7_GetStringAt(string0, 0);
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
		byte createPart;
		Cli_DBRead(Client, stationDBs[station], 2, 1, &createPart);
		//THIS IS WHERE MORE WORK CAN BE DONE TO READ OFF OF XML AND SET POSITION RATHER THAN HARDCODING
		if (CreatePart_NOK) {
			S7_SetBitAt(&createPart, 0, 3, 1);		//0.5 is where createPart_NOK should be
		}
		else if (CreatePart_OK) {
			S7_SetBitAt(&createPart, 0, 2, 1);		// 0.4 is where createPart_OK should be
		}
		else {
			printf("some path was missed in task 2 where neither were set to true\n");
		}
		Cli_DBWrite(Client, stationDBs[station], 2, 1, &createPart);
	}
	// LOAD SERIAL COMP
	if (task == 3) {	 
		cout << "load serial comp\n";
		//check both serials
		//check both in aggregate
		bool loadSerialComp_NOK = FALSE;
		bool loadSerialComp_OK = FALSE;
		byte string0[84];
		Cli_DBRead(Client, loadDBs[station], 0, 84, &string0);
		string Serial1 = S7_GetStringAt(string0, 0);
		string Serial2 = S7_GetStringAt(string0, 42);
		string part1_pk;
		string part2_pk;
		cout << "string1: " << Serial1 << "\n";
		cout << "string2: " << Serial2 << "\n";
		if (ifExists(Serial1, "serial", "part")) {
			//set part1 pk from db
			res = PQexec(dbconn, ("SELECT * FROM part WHERE serial='" + Serial1 + "'").c_str());
			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
				printf("could not execute command: Find Serial1 in Part Table\n");
			}
			part1_pk = PQgetvalue(res, 0, 0); 
			PQclear(res);
			if (ifExists(Serial2, "serial", "part")) {
				//set part2 pk from db
				res = PQexec(dbconn, ("SELECT * FROM part WHERE serial='" + Serial2 + "'").c_str());
				if (PQresultStatus(res) != PGRES_COMMAND_OK) {
					printf("could not execute command: Find Serial2 in Part Table\n");
				}
				part2_pk = PQgetvalue(res, 0, 0); 
				PQclear(res);
				if (ifExists(part1_pk, "part_fk", "aggregate_comp")) {
					printf("Serial %s has already been aggregated \n", Serial1.c_str()); // add station # later
					loadSerialComp_NOK = TRUE;
				}
				else {
					if (ifExists(part2_pk, "part_fk", "aggregate_comp")) {
						printf("Serial %s has already been aggregated\n", Serial2.c_str());
						loadSerialComp_NOK = TRUE;
					}else {
						loadSerialComp_OK = TRUE;
					}
				}
			}
			else {
				printf("serial: %s does not exist in system", Serial2.c_str());
				loadSerialComp_NOK = TRUE;
			}
		}
		else { 
			printf("serial: %s does not exist in system", Serial1.c_str()); 
			loadSerialComp_NOK = TRUE;
		}

		//set necessary COMM bit to TRUE
		byte buffer;
		Cli_DBRead(Client, stationDBs[station], 2, 1, &buffer);
		//THIS IS WHERE MORE WORK CAN BE DONE TO READ OFF OF XML AND SET POSITION RATHER THAN HARDCODING
		if (loadSerialComp_NOK) {
			S7_SetBitAt(&buffer, 0, 5, 1);		//0.5 is where createPart_NOK should be
		}
		else if (loadSerialComp_OK) {
			S7_SetBitAt(&buffer, 0, 4, 1);		// 0.4 is where createPart_OK should be
			aggregateComp(Serial1,Serial2,station, part1_pk, part2_pk);
		}
		else {
			printf("some path was missed in task 3 where neither were set to true\n");
		}
		Cli_DBWrite(Client, stationDBs[station], 2, 1, &buffer);
	} 

	// START UNLOAD -- SAME RULES AS CHECKWorK ??? BUT WITH CHECKSUM TO BE IMPLEMENTED LATER
	if (task == 4) {
		cout << "start unload\n";
		bool startUnload_NOK = FALSE;
		bool startUnload_OK = FALSE;
		byte string0[42];
		string part_fk;
		Cli_DBRead(Client, loadDBs[station], 0, 42, &string0);	//reads from LOAD datablock to find Serial of part to check (should be first entry to data block)
		string Serial = S7_GetStringAt(string0, 0);
		if (ifExists(Serial, "serial", "part")) {  //check to see if Serial is in part table (i.e. part has been created)
			//									   //get part pk given Serial:
			cout << ("SELECT * from part where serial = '" + Serial + "'").c_str() << "\n";
			res = PQexec(dbconn, ("SELECT * FROM part"));
			if (PQresultStatus(res) != PGRES_COMMAND_OK) {
				printf("could not execute command: Find Serial in Part Table\n");
				//getchar();
			}
			part_fk = PQgetvalue(res, 1, 0);
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
				if (siemensStartUnload(station, part_fk, cycle)) {	//runs through the unload datablock and checks for failure (returns false if there is a failbit = 1) and uploads to database
					startUnload_OK = TRUE;
				}
				else { startUnload_NOK = TRUE; }
			}
		}
		else {
			printf("part does not exist in system\n");
			startUnload_NOK = TRUE;
		}

		//now set the COMM bit of startUnload_(N)OK to TRUE
		byte startUnload[1];
		Cli_DBRead(Client, stationDBs[station], 2, 1, &startUnload);
		//THIS IS WHERE MORE WORK CAN BE DONE TO READ OFF OF XML AND SET POSITION RATHER THAN HARDCODING
		if (startUnload_NOK) {
			printf("setting NOK \n");
			S7_SetBitAt(startUnload, 0, 7, 1);		//0.5 is where startUnload_NOK should be
		}
		else if (startUnload_OK) {
			S7_SetBitAt(startUnload, 0, 6, 1);		// 0.4 is where startUnload_OK should be
			//HOW TO DO CYCLE NUMBER???
		//	siemensStartUnload(station, part_fk, cycle);
		}
		else {
			printf("some path was missed in task 4 where neither were set to true\n");
		}
		Cli_DBWrite(Client, stationDBs[station], 2, 1, &startUnload);

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
	res = PQexec(dbconn, "CREATE TABLE IF NOT EXISTS part(pk SERIAL PRIMARY KEY, serial TEXT, model_fk INT REFERENCES model(pk), start BOOL, finish BOOL)");
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
	readXML("EXAMPLE.xml");				// PUT NAME OF XML TO BE READ HERE, can also be an input variable at the top of the script too i guess....

	switch (state) {
	default:
	{
		cout << "No Recognized PLC Models found in XML document \n";
		//if (ifPKexists("1", "testing")) { printf("yeet"); }		//just testing fuction... to be taken out later
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
			int x;
			float f;
			string str;
			byte data;
			int offset = 0;
			int bitTracker = 0;
			bool secondbyte = FALSE;
			//current datablock set to DB1
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
			/*int sres = Cli_DBRead(Client, 0, 0, 286, &data);
			if (Check(sres, "datablock read")) {
				for (int i = 0; i < tagTypes.size(); i++) {
					cout << tagNames[i] << ": ";
					readSiemensType(tagTypes[i], offset, bitTracker, data);
					cout << "\n";
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
*/
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
		//	Cli_Disconnect(Client);
			getchar();

		}
		cout << "press any key to EXIT";
		getchar();

		PQfinish(dbconn);
		return 0;
	}
	case SIEMENS1500:
	{
	//	Address = IP_address.c_str();
	//	//Address = "10.0.0.9";
	//	Rack = 0;
	//	Slot = 1;
	//	cout << "Siemens 1500" << "\n";
	//	if (CliConnect())
	//	{
	//		int x;
	//		float f;
	//		string str;
	//		byte data[500];
	//		int offset = 0;
	//		int bitTracker = 0;
	//		//current datablock set to DB1
	//		int sres = Cli_DBRead(Client, 1, 0, 280, &data);
	//		if (Check(sres, "datablock read")) {
	//			for (int i = 0; i < tagTypes.size(); i++) {
	//				cout << tagNames[i] << ": ";
	//				readSiemensType(tagTypes[i], offset, bitTracker, data);
	//				cout << "\n";
	//				if (tagTypes[i] == "bool" && tagTypes[i + 1] == "bool") {
	//					bitTracker += 1;
	//					if (bitTracker == 8) {
	//						offset += 1;
	//						bitTracker = 0;
	//					}
	//				}
	//				else {
	//					bitTracker = 0;
	//					offset += tagOffsets[i];
	//				}

	//			}
	//		}
	//		cout << "exit?\n";
	//		if (getchar() != 'n') {
	//			Cli_Disconnect(Client);
	//			getchar();

	//			PQfinish(dbconn);
	//			return 0;
	//		}
	//		Cli_Disconnect(Client);
	//		getchar();
	//	}
	//	cout << "press any key to EXIT";
	//	getchar();

	//	PQfinish(dbconn);
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


