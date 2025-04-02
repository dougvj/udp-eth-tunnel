#pragma once
#include "ini.h"
#include "util.h"

// If val is NULL, then the key is a section header
// If val is not NULL, then the key is a key=value pair
// if key is NULL and val is NULL, then this is the end of the file
typedef bool (*ini_parse_callback)(const char* key,
                                      const char* value,
                                      void* user_data);


bool ini_parse_file(const char* filename,
                        ini_parse_callback cb,
                        void* user_data);
