#include "ini.h"
#include "util.h"

bool iswhitespace(char c) {
  return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

char* trim_whitespace(char* str) {
  while (*str && iswhitespace(*str)) {
    str++;
  }
  char* cur = str;
  while (*cur && !iswhitespace(*cur)) {
    cur++;
  }
  *cur = '\0';
  return str;
}
// To minimize external dependencies (Just linux and glibc), here's a shitty
// ini parser. You're welcome.
bool ini_parse_file(const char* filename,
                    ini_parse_callback cb,
                    void* user_data) {
  FILE* fp defer(fclose) = fopen(filename, "r");
  if (!fp) {
    fprintf(stderr, "Failed to open config file '%s': %s\n", filename,
            strerror(errno));
    return false;
  }
  int line_no = 0;
  char line[2048];
  char line_orig[2048];
  while (fgets(line, sizeof(line), fp)) {
    // Copy the original line for error reporting
    strncpy(line_orig, line, sizeof(line_orig));
    line_no++;
    // If there is a ; it's a comment
    char* comment = strchr(line, ';');
    if (comment) {
      *comment = '\0';
    }
    // Check if we have a section header
    char* open_section = strchr(line, '[');
    char* close_section = strchr(line, ']');
    if (open_section && close_section) {
      *open_section = '\0';
      *close_section = '\0';
      char* section = open_section + 1;
      section = trim_whitespace(section);
      if (!cb(section, NULL, user_data)) {
        goto error;
      }
    } else if (!open_section && !close_section) {
      // This might be a key=value pair
      char* key = line;
      char* value = strchr(line, '=');
      if (value) {
        *value = '\0';
        value++;
        key = trim_whitespace(key);
        value = trim_whitespace(value);
        if (!cb(key, value, user_data)) {
          goto error;
        }
      } else {
        // Confirm that it's just whitespace
        for (char* p = line; *p; p++) {
          if (!iswhitespace(*p)) {
            fprintf(stderr, "Invalid line at %d: %s\n", line_no, line_orig);
            return false;
          }
        }
      }
    } else {
      fprintf(stderr, "Invalid section header at line %d: %s\n", line_no,
              line_orig);
      return false;
    }
  }
  if (!cb(NULL, NULL, user_data)) {
    goto error;
  }
  return true;
error:
  fprintf(stderr, "Error processing '%s' line %d: %s\n", filename, line_no,
          line_orig);
  return false;
}
