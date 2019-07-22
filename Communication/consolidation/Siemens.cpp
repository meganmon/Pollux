#include "Siemens.h"


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
	int res = Cli_ListBlocks(Client, &List);
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
		cout << S7_GetStringAt(data, offset) << "\n";
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
