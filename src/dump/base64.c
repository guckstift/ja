#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <ctype.h>

#define int2hex(x) ( \
	(x) <= 0x9 ? (x) + '0' : \
	(x) <= 0xf ? (x) + 'a' - 0xa : \
0)

char *base64(char *input)
{
	uint64_t len = strlen(input);
	uint8_t *output = malloc(len * 2);
	uint8_t *ip = (uint8_t*)input;
	uint8_t *op = output;
	while(*ip) {
		if(ip[1] == 0) {
			op[0] = ip[0] >> 2;
			op[1] = ip[0] << 4 & 0x30;
			op += 2;
			ip += 1;
		}
		else if(ip[2] == 0) {
			op[0] = ip[0] >> 2;
			op[1] = ip[0] << 4 & 0x30 | ip[1] >> 4 & 0x0f;
			op[2] = ip[1] << 2 & 0x3c;
			op += 3;
			ip += 2;
		}
		else {
			op[0] = ip[0] >> 2;
			op[1] = ip[0] << 4 & 0x30 | ip[1] >> 4 & 0x0f;
			op[2] = ip[1] << 2 & 0x3c | ip[2] >> 6 & 0x03;
			op[3] = ip[2] & 0x3f;
			op += 4;
			ip += 3;
		}
	}
	for(uint8_t *op2 = output; op2 < op; op2++) {
		if(*op2 < 10)
			*op2 += '0';
		else if(*op2 < 10+26)
			*op2 += 'a' - 10;
		else if(*op2 < 10+26+26)
			*op2 += 'A' - 10 - 26;
		else if(*op2 == 10+26+26)
			*op2 = '_';
		else
			*op2 = '-';
	}
	*op = 0;
	return (char*)output;
}

char *decode_base64(char *input)
{
	uint64_t len = strlen(input);
	uint8_t *output = malloc(len);
	uint8_t *ip = (uint8_t*)input;
	uint8_t *op = output;
	for(uint8_t *ip2 = ip; *ip2; ip2++) {
		if(*ip2 >= '0' && *ip2 <= '9')
			*ip2 -= '0';
		else if(*ip2 >= 'a' && *ip2 <= 'z')
			*ip2 -= 'a' - 10;
		else if(*ip2 >= 'A' && *ip2 <= 'Z')
			*ip2 -= 'A' - 10 - 26;
		else if(*ip2 == '_')
			*ip2 = 10 + 26 + 26;
		else
			*ip2 = 10 + 26 + 26 + 1;
	}
	while(ip[0] && ip[1]) {
		if(ip[2] == 0) {
			op[0] = ip[0] << 2 | ip[1] >> 4;
			op += 1;
			ip += 2;
		}
		else if(ip[3] == 0) {
			op[0] = ip[0] << 2 | ip[1] >> 4;
			op[1] = ip[1] << 4 | ip[2] >> 2;
			op += 2;
			ip += 3;
		}
		else {
			op[0] = ip[0] << 2 | ip[1] >> 4;
			op[1] = ip[1] << 4 | ip[2] >> 2;
			op[2] = ip[2] << 6 | ip[3];
			op += 3;
			ip += 4;
		}
	}
	*op = 0;
	return (char*)output;
}

char *deidfy(char *input)
{
	uint64_t len = strlen(input);
	char *output = malloc(len + 1);
	char *ip = input;
	char *op = output;
	while(*ip) {
		if(isalnum(*ip)) {
			*op++ = *ip++;
		}
		else if(*ip == '_') {
			ip ++;
			if(*ip == '_') {
				*op++ = *ip++;
			}
			else if(isdigit(*ip)) {
				uint8_t byte = 0;
				while(isdigit(*ip)) {
					byte *= 10;
					byte += *ip - '0';
					ip ++;
				}
				*op++ = (char)byte;
			}
			else if(*ip <= 'F') {
				// third puncts (A-F)
				*op++ = *ip++ - 'A' + '[';
			}
			else if(*ip <= 'K') {
				// last puncts (G-K)
				*op++ = *ip++ - 'G' + '{';
			}
			else if(*ip <= 'p') {
				// space and first puncts (a-p)
				*op++ = *ip++ - 'a' + ' ';
			}
			else if(*ip <= 'w') {
				// second puncts (q-w)
				*op++ = *ip++ - 'q' + ':';
			}
			else {
				*op++ = ' ';
				ip ++;
			}
		}
		else {
			*op++ = ' ';
			ip ++;
		}
	}
	*op = 0;
	return output;
}
