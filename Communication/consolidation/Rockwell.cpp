#include "Rockwell.h"

string assignTagSize(string type) {
	if (type == "string") {
		return "88";
	}	if (type == "real" || type == "float" || type == "dint" || type == "int") {
		return "4";
	}	if (type == "bool") {
		return "1";
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

/*shorter way to destroy all tags*/
void destroyRockTags() {
	for (int i = 0; i < tagNames.size(); i++) {
		plc_tag_destroy(myTags[i]);
	}
}
