#ifndef LIBUSEFUL_HASH_H
#define LIBUSEFUL_HASH_H

#include "file.h"
#include "includes.h"


#define ENCODE_HEX 1
#define ENCODE_BASE64 2

#ifdef __cplusplus
extern "C" {
#endif

typedef struct t_hash THash;

typedef void (*HASH_UPDATE)(THash *Hash, char *Data, int DataLen);
typedef int (*HASH_FINISH)(THash *Hash, int Encoding, char **RetStr);

struct t_hash
{
char *Type;
void *Ctx;
HASH_UPDATE Update;
HASH_FINISH Finish;
};

THash *HashInit(char *Type);
void DestroyHash(THash *Hash);
char *EncodeBase64(char *Return, char *Text, int len);
char *DecodeBase64(char *Return, int *len, char *Text);
int HashBytes(char **Return, char *Type, char *text, int len, int Encoding);
int HashFile(char **Return, char *Type, char *Path, int Encoding);
int HMAC(char **Return, char *Algo, char *iKey, int iKeyLen, char *iText, int iTextLen);

#ifdef __cplusplus
}
#endif



#endif
