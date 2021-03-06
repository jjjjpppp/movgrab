#include "DataProcessing.h"


#ifdef HAVE_LIBSSL

#include <openssl/crypto.h>
#include <openssl/x509.h>
#include <openssl/pem.h>
#include <openssl/ssl.h>
#include <openssl/err.h>
#include <openssl/evp.h>


typedef struct
{
char *Key;
int KeyLen;
char *InputVector;
int InputVectorLen;
int BlockSize;
const EVP_CIPHER *Cipher;
EVP_CIPHER_CTX *enc_ctx;
EVP_CIPHER_CTX *dec_ctx;
} libCryptoProcessorData;
#endif


void DataProcessorDestroy(void *In)
{
TProcessingModule *Mod;

Mod=(TProcessingModule *) In;
if (! Mod) return;
Mod->Close(Mod);
DestroyString(Mod->Name);
DestroyString(Mod->Args);
DestroyString(Mod->ReadBuff);
DestroyString(Mod->WriteBuff);
free(Mod);
}



char *DataProcessorGetValue(TProcessingModule *M, const char *Name)
{
char *ptr;
ListNode *Curr;

if (! M->Values) return(NULL);
Curr=ListFindNamedItem(M->Values,Name);
if (Curr) return(Curr->Item);
return(NULL);
}


void DataProcessorSetValue(TProcessingModule *M, const char *Name, const char *Value)
{
char *ptr;
ListNode *Curr;

if (! M->Values) M->Values=ListCreate();
Curr=ListFindNamedItem(M->Values,Name);
if (Curr) Curr->Item = (void *) CopyStr( (char *) Curr->Item, Value);
else ListAddNamedItem(M->Values,Name,CopyStr(NULL,Value));
}



void DataProcessorUpdateBuffer(char **Buffer, int *Used, int *Size, const char *Data, int DataLen)
{
int len;

if (DataLen < 1) return;

len=*Used+DataLen;


if (len > *Size)
{
 *Buffer=(char *) realloc(*Buffer,len);
 *Size=len;
}

//if we've been supplied actual data to put in the buffer, then do so
//otherwise just expand it if needed
if (Data) 
{
	memcpy((*Buffer) + (*Used) ,Data,DataLen);
	*Used=len;
}
}


int PipeCommandProcessorInit(TProcessingModule *ProcMod, const char *Args)
{
int result=FALSE;
char *Tempstr=NULL;
char *Name=NULL, *Value=NULL, *ptr;
STREAM *S;

ptr=GetNameValuePair(Args,"\\S","=",&Name,&Value);
while (ptr)
{
  if (strcasecmp(Name,"Command")==0) Tempstr=CopyStr(Tempstr,Value);

ptr=GetNameValuePair(ptr,"\\S","=",&Name,&Value);
}

if (! StrLen(Tempstr) )
{
	DestroyString(Tempstr);
	return(FALSE);
}

S=STREAMSpawnCommand(Tempstr, COMMS_BY_PIPE);
ProcMod->Data=(void *) S;
result=TRUE;

DestroyString(Name);
DestroyString(Value);
DestroyString(Tempstr);

return(result);
}


int PipeCommandProcessorWrite(TProcessingModule *ProcMod, const char *InData, int InLen, char **OutData, int *OutLen, int Flush)
{
STREAM *S;

S=(STREAM *) ProcMod->Data;
if (InLen > 0)
{
STREAMWriteBytes(S,InData,InLen);
STREAMFlush(S);
}

if (Flush) 
{
	if (S->out_fd > -1) close(S->out_fd);
	S->out_fd=-1;
}

if (! STREAMCheckForBytes(S)) return(0);
return(STREAMReadBytes(S,*OutData,*OutLen));
}


int PipeCommandProcessorClose(TProcessingModule *ProcMod)
{
STREAMClose((STREAM *) ProcMod->Data);
ProcMod->Data=NULL;

return(TRUE);
}



void InitialiseEncryptionComponents(const char *Args, char **Cipher, char **InputVector, int *IVLen,  char **Key, int *KeyLen, int *Flags)
{
char *TmpKey=NULL, *Tempstr=NULL;
int klen=0, slen=0;
char *Name=NULL, *Value=NULL, *ptr;
char *Salt=NULL;

*IVLen=0;
ptr=GetNameValuePair(Args,"\\S","=",&Name,&Value);
while (ptr)
{
  if (StrLen(Name))
  {
  if (strcasecmp(Name,"Cipher")==0)
  {
	  *Cipher=CopyStr(*Cipher,Value);
  }


  if (strcasecmp(Name,"Key")==0)
  {
	  TmpKey=CopyStr(TmpKey,Value);
	  klen=StrLen(TmpKey);
  }

  if (strcasecmp(Name,"Salt")==0)
  {
	  Salt=CopyStr(Salt,Value);
	  slen=StrLen(Salt);
  }



  if (
	(strcasecmp(Name,"iv")==0) ||
  	(strcasecmp(Name,"InputVector")==0)
    )
  {
    *InputVector=CopyStr(*InputVector,Value);
    *IVLen=StrLen(*InputVector);
  }

  if (strcasecmp(Name,"HexKey")==0)
  {
    klen=HexStrToBytes(&TmpKey, Value);
  }

  if (
	(strcasecmp(Name,"HexIV")==0) ||
  	(strcasecmp(Name,"HexInputVector")==0)
    )

  {
    *IVLen=HexStrToBytes(InputVector,Value);
  }

  if (strcasecmp(Name,"PadBlock")==0) 
  {
    if (strcasecmp(Value,"N")==0) *Flags |= DPM_NOPAD_DATA;
  }

  }

ptr=GetNameValuePair(ptr,"\\S","=",&Name,&Value);
}


Tempstr=SetStrLen(Tempstr,klen+slen);
memcpy(Tempstr,Salt,slen);
memcpy(Tempstr+slen,TmpKey,klen);

*KeyLen=HashBytes(Key,"md5",Tempstr,slen+klen,0);


DestroyString(Name);
DestroyString(Value);
DestroyString(Tempstr);
DestroyString(TmpKey);
DestroyString(Salt);
}



#ifdef HAVE_LIBCRYPTO

typedef enum {CI_BLOWFISH, CI_RC2, CI_RC4, CI_RC5, CI_DES, CI_DESX, CI_CAST,CI_IDEA,CI_AES, CI_AES_256} LIBUSEFUL_CRYPT_CIPHERS;

int libCryptoCipherAvailable(int CipherNum)
{
switch(CipherNum)
{
	case CI_BLOWFISH:
		#ifdef HAVE_EVP_BF_CBC
		return(TRUE);
		#endif
		break;

	case CI_RC2:
		#ifdef HAVE_EVP_RC2_CBC
		return(TRUE);
		#endif
		break;

	case CI_RC4:
		#ifdef HAVE_EVP_RC4_CBC
		return(TRUE);
		#endif
		break;

	case CI_RC5:
		#ifdef HAVE_EVP_RC5_CBC
		return(TRUE);
		#endif
		break;

	case CI_DES:
		#ifdef HAVE_EVP_DES_CBC
		return(TRUE);
		#endif
		break;

	case CI_DESX:
		#ifdef HAVE_EVP_DESX_CBC
		return(TRUE);
		#endif
		break;

	case CI_CAST:
		#ifdef HAVE_EVP_CAST5_CBC
		return(TRUE);
		#endif
		break;

	case CI_IDEA:
		#ifdef HAVE_EVP_IDEA_CBC
		return(TRUE);
		#endif
		break;

	case CI_AES:
		#ifdef HAVE_EVP_AES_129_CBC
		return(TRUE);
		#endif
		break;

	case CI_AES_256:
		#ifdef HAVE_EVP_AES_256_CBC
		return(TRUE);
		#endif
		break;
}
return(FALSE);
}


int libCryptoProcessorInit(TProcessingModule *ProcMod, const char *Args)
{
int result=FALSE;

#ifdef HAVE_LIBSSL
libCryptoProcessorData *Data;
EVP_CIPHER_CTX *ctx;
char *CipherList[]={"blowfish","rc2","rc4","rc5","des","desx","cast","idea","aes","aes-256",NULL};
int val;
char *Tempstr=NULL;

val=MatchTokenFromList(ProcMod->Name,CipherList,0);
if (val==-1) return(FALSE);
if (! libCryptoCipherAvailable(val)) return(FALSE);
Data=(libCryptoProcessorData *) calloc(1,sizeof(libCryptoProcessorData));

//Tempstr here holds the cipher name
InitialiseEncryptionComponents(Args, &Tempstr, &Data->InputVector, &Data->InputVectorLen, & Data->Key, &Data->KeyLen,&ProcMod->Flags);

if (StrLen(ProcMod->Name)==0) ProcMod->Name=CopyStr(ProcMod->Name,Tempstr);

switch(val)
{
/*
	case CI_NONE:
	Data->Cipher=EVP_enc_null();
	break;
*/

	case CI_BLOWFISH:
		#ifdef HAVE_EVP_BF_CBC
		Data->Cipher=EVP_bf_cbc();
		#endif
		break;

	case CI_RC2:
		#ifdef HAVE_EVP_RC2_CBC
		Data->Cipher=EVP_rc2_cbc();
		#endif
		break;

	case CI_RC4:
		#ifdef HAVE_EVP_RC4_CBC
		Data->Cipher=EVP_rc4();
		#endif
		break;

	case CI_RC5:
		#ifdef HAVE_EVP_RC5_32_12_16_CBC
		//Data->Cipher=EVP_rc5_32_12_16_cbc();
		#endif
		break;

	case CI_DES:
		#ifdef HAVE_EVP_DES_CBC
		Data->Cipher=EVP_des_cbc();
		#endif
		break;

	case CI_DESX:
		#ifdef HAVE_EVP_DESX_CBC
		Data->Cipher=EVP_desx_cbc();
		#endif
		break;

	case CI_CAST:
		#ifdef HAVE_EVP_CAST5_CBC
		Data->Cipher=EVP_cast5_cbc();
		#endif
		break;

	case CI_IDEA:
		#ifdef HAVE_EVP_IDEA_CBC
		Data->Cipher=EVP_idea_cbc();
		#endif
		break;

	case CI_AES:
		#ifdef HAVE_EVP_AES_128_CBC
		Data->Cipher=EVP_aes_128_cbc();
		#endif
		break;

	case CI_AES_256:
		#ifdef HAVE_EVP_AES_256_CBC
		Data->Cipher=EVP_aes_256_cbc();
		#endif
		break;
}


if (Data->Cipher)
{
Data->enc_ctx=(EVP_CIPHER_CTX *) calloc(1,sizeof(EVP_CIPHER_CTX));
Data->dec_ctx=(EVP_CIPHER_CTX *) calloc(1,sizeof(EVP_CIPHER_CTX));
EVP_CIPHER_CTX_init(Data->enc_ctx);
EVP_CIPHER_CTX_init(Data->dec_ctx);
Data->BlockSize=EVP_CIPHER_block_size(Data->Cipher);

EVP_EncryptInit_ex(Data->enc_ctx,Data->Cipher,NULL,Data->Key,Data->InputVector);
EVP_DecryptInit_ex(Data->dec_ctx,Data->Cipher,NULL,Data->Key,Data->InputVector);

if (ProcMod->Flags & DPM_NOPAD_DATA) EVP_CIPHER_CTX_set_padding(Data->enc_ctx,FALSE);

ProcMod->Data=Data;
result=TRUE;

DataProcessorSetValue(ProcMod,"Cipher",Tempstr);
Tempstr=FormatStr(Tempstr,"%d",Data->BlockSize);
DataProcessorSetValue(ProcMod,"BlockSize",Tempstr);
}

DestroyString(Tempstr);
#endif
return(result);
}


int libCryptoProcessorClose(TProcessingModule *ProcMod)
{
#ifdef HAVE_LIBSSL
libCryptoProcessorData *Data;
EVP_CIPHER_CTX *ctx;

Data=(libCryptoProcessorData *) ProcMod->Data;
if (Data)
{
EVP_CIPHER_CTX_cleanup(Data->enc_ctx);
EVP_CIPHER_CTX_cleanup(Data->dec_ctx);

DestroyString(Data->Key);
DestroyString(Data->InputVector);
free(Data);
}
ProcMod->Data=NULL;
#endif
return(TRUE);
}







int libCryptoProcessorWrite(TProcessingModule *ProcMod, const char *InData, int InLen, char **OutData, int *OutLen, int Flush)
{
int wrote=0;

#ifdef HAVE_LIBSSL
int len, result, val;
libCryptoProcessorData *Data;
EVP_CIPHER_CTX *ctx;
char *ptr, *Tempstr=NULL;

/*
if (ProcMod->Flags & DPM_WRITE_FINAL) return(0);
ptr=OutData;

Data=(libCryptoProcessorData *) ProcMod->Data;
ctx=Data->enc_ctx;

ProcMod->Flags = ProcMod->Flags & ~DPM_WRITE_FINAL;
*/

/*
if (ProcMod->Flags & DPM_NOPAD_DATA)
{
	val=InLen % Data->BlockSize;
	Tempstr=CopyStrLen(Tempstr,InData,InLen);
	if (val !=0) 
	{
		Tempstr=SetStrLen(Tempstr,InLen + (Data->BlockSize-val));
		memset(Tempstr+InLen,' ', (Data->BlockSize-val));
		val=InLen+(Data->BlockSize-val);
	}
	else val=InLen;

	result=EVP_EncryptUpdate(ctx, ptr, &len, Tempstr, val);
}
else 
*/
{
//result=EVP_EncryptUpdate(ctx, ptr, &len, InData, InLen);
}


if (! result) wrote=0;
else wrote=len;

DestroyString(Tempstr);
#endif
return(wrote);
}



int libCryptoProcessorFlush(TProcessingModule *ProcMod, const char *InData, int InLen, char *OutData, int OutLen)
{
int result=0, wrote=0, len;
libCryptoProcessorData *Data;

/*
if (ProcMod->Flags & DPM_WRITE_FINAL) return(0);
Data=(libCryptoProcessorData *) ProcMod->Data;

if (Data)
{
if (InLen > 0)
{
result=libCryptoProcessorWrite(ProcMod, InData, InLen, OutData, OutLen,TRUE);
if (result > 0) return(result);
}

len=OutLen;
result=EVP_EncryptFinal_ex(Data->enc_ctx, OutData, &len);
ProcMod->Flags |= DPM_WRITE_FINAL;
}
if (! result) wrote=0;
else wrote=len;

*/

return(wrote);
}


int libCryptoProcessorRead(TProcessingModule *ProcMod, const char *InData, int InLen, char **OutData, int *OutLen, int Flush)
{
int bytes_read=0;
#ifdef HAVE_LIBSSL
int len, ivlen, result, val;
libCryptoProcessorData *Data;
EVP_CIPHER_CTX *ctx;
char *ptr;

/*
ptr=OutData;

Data=(libCryptoProcessorData *) ProcMod->Data;
if (!Data) return(0);
*/

/*
if (ProcMod->Flags & DPM_READ_FINAL)
{
  if (InLen==0)	return(0);
  EVP_DecryptInit_ex(Data->dec_ctx,Data->Cipher,NULL,Data->Key,Data->InputVector);

}
*/

/*
ctx=Data->dec_ctx;

if (InLen==0) 
{
  len=0;
  result=EVP_DecryptFinal_ex(ctx, ptr, &len);
  ProcMod->Flags |= DPM_READ_FINAL; //this so we don't try 
				    //another read
	
}
else 
{
	len=OutLen;
	result=EVP_DecryptUpdate(ctx, ptr, &len, InData, InLen);
}

if (! result) bytes_read=-1;
else bytes_read+=InLen; //should be 'len' but DecryptUpdate returns the
												//number of bytes output, not the number consumed
*/

#endif
return(bytes_read);
}

#endif



#ifdef HAVE_LIBZ

#include <zlib.h>

typedef struct
{
z_stream z_in;
z_stream z_out;
} zlibData;
#endif


int zlibProcessorInit(TProcessingModule *ProcMod, const char *Args)
{
int result=FALSE;

#ifdef HAVE_LIBZ
zlibData *ZData;
int CompressionLevel=5;
char *ptr, *Name=NULL, *Value=NULL;

ptr=GetNameValuePair(Args,"\\S","=",&Name,&Value);
while (ptr)
{
  if (strcasecmp(Name,"CompressionLevel")==0) CompressionLevel=atoi(Value);
  if (strcasecmp(Name,"Level")==0) CompressionLevel=atoi(Value);
ptr=GetNameValuePair(ptr,"\\S","=",&Name,&Value);
}


ProcMod->ReadMax=4096;
ProcMod->WriteMax=4096;


ZData=(zlibData *) calloc(1,sizeof(zlibData));
ZData->z_in.avail_in=0;
ZData->z_in.avail_out=0;
result=inflateInit(&ZData->z_in);

ZData->z_out.avail_in=0;
ZData->z_out.avail_out=0;
deflateInit(&ZData->z_out,CompressionLevel);


ProcMod->Data=(void *) ZData;
result=TRUE;

DestroyString(Name);
DestroyString(Value);

#endif
return(result);
}


int gzipProcessorInit(TProcessingModule *ProcMod, const char *Args)
{
int result=FALSE;

#ifdef HAVE_LIBZ
zlibData *ZData;
int CompressionLevel=5;
char *ptr, *Name=NULL, *Value=NULL;

ptr=GetNameValuePair(Args,"\\S","=",&Name,&Value);
while (ptr)
{
  if (strcasecmp(Name,"CompressionLevel")==0) CompressionLevel=atoi(Value);
  if (strcasecmp(Name,"Level")==0) CompressionLevel=atoi(Value);
ptr=GetNameValuePair(ptr,"\\S","=",&Name,&Value);
}

ProcMod->ReadMax=4096;
ProcMod->WriteMax=4096;

ZData=(zlibData *) calloc(1,sizeof(zlibData));
ZData->z_in.avail_in=0;
ZData->z_in.avail_out=0;
result=inflateInit2(&ZData->z_in,47);

ZData->z_out.avail_in=0;
ZData->z_out.avail_out=0;
deflateInit2(&ZData->z_out,5,Z_DEFLATED,30,8,Z_DEFAULT_STRATEGY);

ProcMod->Data=(void *) ZData;
result=TRUE;

DestroyString(Name);
DestroyString(Value);

#endif
return(result);
}


//Zlib is a little weird. It accepts a pointer to a buffer (next_in) and a buffer length (avail_in) to specify the input
//and another buffer (next_out) and length (avail_out) to write data into. When called it reads bytes from next_in, updates 
//next_in to point to the end of what it read, and subtracts the number of bytes it read from avail_in so that avail_in
//now says how many UNUSED bytes there are pointed to by next_in. Similarly it writes to next_out, updating that pointer
//to point to the end of the write, and updating avail_out to say how much room is LEFT usused in the output buffer
//
//However, if zlib doesn't use all avail_in, then you can't mess with that buffer until it has. Hence you can't take the unusued
//data from next_in/avail_in and copy it to a new buffer and pass that buffer into deflate/inflate on the next call. If zlib
//doesn't use all the input the only way to handle it is to grow the output buffer and call inflate/deflate again, so that it
//can write into the expanded buffer until it's used up all input. 
//
//Finally, when you've supplied all the input you've got, you have to call deflate with 'Z_FINISH' so that it knows there's no
//more data coming. 

int zlibProcessorWrite(TProcessingModule *ProcMod, const char *InData, int InLen, char **OutData, int *OutLen, int Flush)
{
int wrote=0, val=0;
#ifdef HAVE_LIBZ
zlibData *ZData;

if (ProcMod->Flags & DPM_WRITE_FINAL) return(EOF);
ZData=(zlibData *) ProcMod->Data;


	ZData->z_out.avail_in=InLen;
	ZData->z_out.next_in=InData;
	ZData->z_out.avail_out=*OutLen;
	ZData->z_out.next_out=*OutData;

	while ((ZData->z_out.avail_in > 0) || Flush)
	{
		if (Flush) val=deflate(& ZData->z_out, Z_FINISH);
		else val=deflate(& ZData->z_out, Z_NO_FLUSH);

		wrote=*OutLen-ZData->z_out.avail_out;
		if (val==Z_STREAM_END) 
		{
			ProcMod->Flags |= DPM_WRITE_FINAL;
			break;
		}

		if ((ZData->z_out.avail_in > 0) || Flush)
		{
			printf("Grow output buff!\n");
			*OutLen+=BUFSIZ;
			*OutData=(char *) realloc(*OutData,*OutLen);
			ZData->z_out.avail_out+=BUFSIZ;
		}

	}



#endif
return(wrote);
}


int zlibProcessorRead(TProcessingModule *ProcMod, const char *InData, int InLen, char **OutData, int *OutLen, int Flush)
{
int wrote=0, result=0;
#ifdef HAVE_LIBZ
zlibData *ZData;

if (ProcMod->Flags & DPM_READ_FINAL) return(EOF);
ZData=(zlibData *) ProcMod->Data;


	ZData->z_in.avail_in=InLen;
	ZData->z_in.next_in=InData;
	ZData->z_in.avail_out=*OutLen;
	ZData->z_in.next_out=*OutData;

	while ((ZData->z_in.avail_in > 0) || Flush)
	{
		if (Flush) result=inflate(& ZData->z_in, Z_FINISH);
		else result=inflate(& ZData->z_in, Z_NO_FLUSH);

		wrote=(*OutLen)-ZData->z_in.avail_out;


		if (result==Z_DATA_ERROR) inflateSync(&ZData->z_in);
		if (result==Z_STREAM_END) 
		{
			ProcMod->Flags |= DPM_READ_FINAL;
			break;
		}

		if ((ZData->z_in.avail_in > 0) || Flush)
		{
			(*OutLen)+=BUFSIZ;
			*OutData=(char *) realloc(*OutData,*OutLen);
			ZData->z_in.next_out=(*OutData) + wrote;
			ZData->z_in.avail_out=(*OutLen) - wrote;
		}

	}



#endif
return(wrote);
}


/*
int zlibProcessorRead(TProcessingModule *ProcMod, const char *InData, int InLen, char **OutData, int *OutLen, int Flush)
{
int wrote=0, result;
#ifdef HAVE_LIBZ
zlibData *ZData;
int len;

ZData=(zlibData *) ProcMod->Data;

if (InLen > 0)
{
DataProcessorUpdateBuffer(&ProcMod->ReadBuff, &ProcMod->ReadUsed, &ProcMod->ReadSize, InData, InLen);

ZData->z_in.next_in=ProcMod->ReadBuff;
ZData->z_in.avail_in=ProcMod->ReadUsed;

ZData->z_in.avail_out=*OutLen;
ZData->z_in.next_out=*OutData;

if (InLen==0) result=inflate(& ZData->z_in, Z_FINISH);
else result=inflate(& ZData->z_in, Z_NO_FLUSH);

if (ZData->z_in.avail_in > 0) memmove(ProcMod->ReadBuff,ZData->z_in.next_in,ZData->z_in.avail_in);
ProcMod->ReadUsed=ZData->z_in.avail_in;

wrote=OutLen-ZData->z_in.avail_out;
}

#endif
return(wrote);
}
*/


int zlibProcessorClose(TProcessingModule *ProcMod)
{
#ifdef HAVE_LIBZ
zlibData *ZData;

ZData=(zlibData *) ProcMod->Data;
if (ZData)
{
inflateEnd(&ZData->z_in);
deflateEnd(&ZData->z_out);

free(ZData);
ProcMod->Data=NULL;
}
#endif
return(TRUE);
}



TProcessingModule *StandardDataProcessorCreate(const char *Class, const char *Name, const char *iArgs)
{
TProcessingModule *Mod=NULL;
char *Args=NULL;


Args=CopyStr(Args,iArgs);

#ifdef HAVE_LIBSSL
#ifdef HAVE_LIBCRYPTO
if (strcasecmp(Class,"crypto")==0)
{
   Mod=(TProcessingModule *) calloc(1,sizeof(TProcessingModule));
   Mod->Args=CopyStr(Mod->Args,Args);
   Mod->Name=CopyStr(Mod->Name,Name);
   Mod->Init=libCryptoProcessorInit;
   Mod->Write=libCryptoProcessorWrite;
   Mod->Read=libCryptoProcessorRead;
   Mod->Close=libCryptoProcessorClose;

   if (Mod && Mod->Init(Mod, Args)) return(Mod);
   else 
	{
		DataProcessorDestroy(Mod);
		Mod=NULL;
	}
}
#endif
#endif



if (strcasecmp(Class,"compression")==0)
{
   Mod=(TProcessingModule *) calloc(1,sizeof(TProcessingModule));
   Mod->Args=CopyStr(Mod->Args,Args);
   Mod->Name=CopyStr(Mod->Name,Name);

   if (strcasecmp(Name,"zlib")==0) 
	 {
		#ifdef HAVE_LIBZ 
		Mod->Init=zlibProcessorInit;
   	Mod->Write=zlibProcessorWrite;
   	Mod->Read=zlibProcessorRead;
   	Mod->Close=zlibProcessorClose;
		#endif
   }
	 else if (strcasecmp(Name,"gzip")==0)
	 {
		#ifdef HAVE_LIBZ 
		Mod->Init=gzipProcessorInit;
   	Mod->Write=zlibProcessorWrite;
   	Mod->Read=zlibProcessorRead;
   	Mod->Close=zlibProcessorClose;
		#endif
   }
 	 else if (strcasecmp(Name,"bzip2")==0)
	 {
		Args=MCopyStr(Args,"Command='/usr/bin/bzip2 --stdout -' ",iArgs,NULL);
		Mod->Init=PipeCommandProcessorInit;
   	Mod->Write=PipeCommandProcessorWrite;
   	Mod->Close=PipeCommandProcessorClose;
   }


  if (Mod && Mod->Init && Mod->Init(Mod, Args)) return(Mod);
  else 
	{
		DataProcessorDestroy(Mod);
		Mod=NULL;
	}
}

return(NULL);
}





int STREAMAddDataProcessor(STREAM *S, TProcessingModule *Mod, const char *Args)
{
ListNode *Curr;
char *Tempstr=NULL;
int len;

STREAMFlush(S);

if (! S->ProcessingModules) S->ProcessingModules=ListCreate();
Tempstr=MCopyStr(Tempstr,Mod->Name,NULL);
ListAddNamedItem(S->ProcessingModules,Tempstr,Mod);

len=S->InEnd - S->InStart;
Tempstr=SetStrLen(Tempstr,len);
memcpy(Tempstr,S->InputBuff + S->InStart,len);
STREAMResetInputBuffers(S);

Curr=ListGetNext(Mod->Values);
while (Curr)
{
	STREAMSetValue(S,Curr->Tag,(char *) Curr->Item);
	Curr=ListGetNext(Curr);
}

STREAMReadThroughProcessors(S, Tempstr, len);

DestroyString(Tempstr);
return(TRUE);
}


int STREAMDeleteDataProcessor(STREAM *S, char *Class, char *Name)
{
ListNode *Curr;
char *Tempstr=NULL;
int len;

STREAMFlush(S);

Tempstr=MCopyStr(Tempstr,Class,":",Name,NULL);
Curr=ListFindNamedItem(S->ProcessingModules,Tempstr);
ListDeleteNode(Curr);

DestroyString(Tempstr);
return(TRUE);
}



int DataProcessorAvailable(const char *Class, const char *Name)
{
int result=FALSE;
TProcessingModule *Mod;

Mod=StandardDataProcessorCreate(Class,Name,"");
if (Mod) result=TRUE;

DataProcessorDestroy(Mod);
return(result);
}


int STREAMAddStandardDataProcessor(STREAM *S, const char *Class, const char *Name, const char *Args)
{
TProcessingModule *Mod=NULL;

Mod=StandardDataProcessorCreate(Class,Name,Args);
if (Mod) 
{
STREAMAddDataProcessor(S, Mod, Args);
return(TRUE);
}

return(FALSE);
}


void STREAMClearDataProcessors(STREAM *S)
{
TProcessingModule *Mod;

STREAMFlush(S);
STREAMResetInputBuffers(S);
ListDestroy(S->ProcessingModules, DataProcessorDestroy);
}

