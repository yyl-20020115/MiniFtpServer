#ifndef _STRUTIL_H_
#define _STRUTIL_H_

void str_trim_crlf(char *str);
void str_split(const char *str , char *left, char *right, char c);
int str_all_space(const char *str);
void str_upper(char *str);
unsigned int str_octal_to_uint(const char *str);

#endif  /*_STRUTIL_H_*/
