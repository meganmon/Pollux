#include "Postgresql.h"

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
//checks to execution of Postgresql command	 -- it clears result after checking, so do not use if trying to read data from the result after calling
void checkexec(PGresult *res, const char *function) {
	if (PQresultStatus(res) != PGRES_COMMAND_OK) {
		printf("could not execute command: %s\n", function);
		getchar();
	}
	PQclear(res);

}


//reads default database given a table and a key (but key is not necessary)
void readDB(string table, string key = "*", string extras = "", int itemCount = 1) {
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
//takes serial 1 and aggregates it to part2_fk (given by serial 2) in the aggregate_comp table
void aggregateComp(string Serial, int station, string part2_fk) {
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
