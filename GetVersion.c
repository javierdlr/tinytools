;/*
gcc GetVersion.c -o GetVersion -lcrypto -gstabs -Wall -D__AMIGADATE__=\"`c:date LFORMAT %d.%m.%Y`\"
quit
Or gcc md5.c GetVersion.c -o GetVersion -gstabs -Wall -D__AMIGADATE__=\"`c:date LFORMAT %d.%m.%Y`\"
Or VBCC: remove ';' from line 1 and from CLI/Shell: vc GetVersion.c -o GetVersion
*/
#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/utility.h>


#define VERSION   1
#define REVISION  2
#ifdef __VBCC__
#define DATE  __AMIGADATE__
#else
#define DATE  "("__AMIGADATE__")"
#endif
#define VERS     "GetVersion 1.2"
#define VSTRING  VERS" "DATE"\r\n"
#define VERSTAG  "\0$VER: "VERS" "DATE

#define TEMPLATE "FILE/A,N=NAME/S,V=VER=VERSION/S,D=DATE/S,RES=RESIDENT/S,EXTVER/S,NC=NOCOMMENT/S"

enum
{
 V_FULLNV    = -2,
 V_FULL      = -1,
 V_NAME      = 1, // # of ' ' to "cut" version string "$VER: name ver.rev (date) comment"
 V_VERSION,       // idem
 V_DATE,          // idem
 EXTVER    = 8,
 RESIDENT  = 9,
 NO_COMMENT = 10
};


// filename; destination_buffer; dst_bufsize; type:-2=full w/o '$VER: ', -1=full, 1=name, 2=ver.rev, 3=date
BOOL showVersion(STRPTR, STRPTR, int32, int32);
// filename; destination_buffer; dst_bufsize; "$VER"|"$EXTVER"
BOOL BruteForce(STRPTR, STRPTR, int32, STRPTR);


#ifdef __VBCC__
	#define USE_FIX_SOFTWARE 0 // DON'T CHANGE IT [WIP]
#else
	#define USE_FIX_SOFTWARE 1 // 1=enabled, 0=disabled : Workarounds for some software
#endif

#if USE_FIX_SOFTWARE
	#define HAVE_OPENSSL 0
	#include "md5.h"
	BOOL md5sum(STRPTR, unsigned char *); // filename; md5sum_result
	//unsigned int crc32b(STRPTR); // filename
#endif


struct Library *UtilityBase = NULL;
struct UtilityIFace *IUtility = NULL;


//const char *versionfake = "$VER:";
const char *version = VERSTAG;
const char *extversion = "\0$EXTVER: "VERS" "DATE" extraversion mode\0";


int main(void)
{
	int res = RETURN_OK;

	UtilityBase = IExec->OpenLibrary("utility.library", 52);
	if(!UtilityBase) res = RETURN_FAIL;
	IUtility = (struct UtilityIFace *)IExec->GetInterface(UtilityBase, "main", 1, NULL);
	if(!IUtility) res = RETURN_FAIL;

	if(res == RETURN_OK) {
		struct RDArgs *rda;
		struct UserArgs
		{
			STRPTR name;
			LONG v_name;
			LONG v_ver;
			LONG v_date;
			LONG resident;
			LONG extver;
			LONG v_nocom;
		} params = {0};

		//IUtility->ClearMem( &params, sizeof(struct UserArgs) );
		rda = IDOS->ReadArgs(TEMPLATE, (LONG *)&params, NULL);
		if(!rda) {
			IDOS->PrintFault(IDOS->IoErr(),"GETVERSION");
		}
		else {
			STRPTR dst_buf;
			int value = 0;

			//IDOS->Printf("\033[1m%s\033[0m:\n", params.name);

			dst_buf = IExec->AllocVecTags(1024, AVT_ClearWithValue,0, TAG_DONE);

			if(params.v_name) value = 1;
			if(params.v_ver)  value += 2;
			if(params.v_date) value += 4;

			if(params.resident) value = RESIDENT;

			if(params.extver) value = EXTVER;

			if(params.v_nocom) value += NO_COMMENT; // 10 + [1..9] = 1x
//IDOS->Printf("[value=%ld']\n",value);
			switch(value) {
				case 4: // only V_DATE
					value = V_DATE; // see enum{} values V_DATE=3
				case 1: // only V_NAME=1
				case 2: // only V_VERSION=2
				case EXTVER:
				case RESIDENT:
				case NO_COMMENT:
				case 18: // EXTVER + NO_COMENT
				case 19: // RESIDENT + NO_COMENT
					if( !showVersion(params.name, dst_buf, 1024, value) ) {
						res = RETURN_FAIL;
					}
				break;
				case 3:
					if( showVersion(params.name, dst_buf, 1024, V_NAME) ) {
						IDOS->Printf("%s ", dst_buf);
						showVersion(params.name, dst_buf, 1024, V_VERSION);
					}
					else { res = RETURN_FAIL; }
				break;
				case 6:
					if( showVersion(params.name, dst_buf, 1024, V_VERSION) ) {
						IDOS->Printf("%s ", dst_buf);
						showVersion(params.name, dst_buf, 1024, V_DATE);
					}
					else { res = RETURN_FAIL; }
				break;
				case 7: // without COMMENT (text behind DATE)
					if( showVersion(params.name, dst_buf, 1024, V_NAME) ) {
						IDOS->Printf("%s ", dst_buf);
						showVersion(params.name, dst_buf, 1024, V_VERSION);
						IDOS->Printf("%s ", dst_buf);
						showVersion(params.name, dst_buf, 1024, V_DATE);
					}
					else { res = RETURN_FAIL; }
				break;
				default: // full version string
//IDOS->Printf("[DEFAULT] '%s'\n",params.name);
					if( !showVersion(params.name, dst_buf, 1024, V_FULLNV) ) {
						res = RETURN_FAIL;
					}
				break;
			}

			if(res) {
				BPTR fh = IDOS->FOpen(params.name, MODE_OLDFILE, 0);
				if(fh == ZERO) {
					IDOS->PrintFault(IDOS->IoErr(),"GETVERSION");
				}
				else {
					IDOS->FClose(fh);
					IDOS->PrintFault(ERROR_SEEK_ERROR, "GETVERSION");
				}
			}
			else IDOS->Printf("%s\n", dst_buf);
/*
				if( showVersion(params.name, dst_buf, 1024, V_FULL) ) {
					IDOS->Printf("\n[full]\n");
					IDOS->Printf("'%s'\n", dst_buf);
					//IDOS->Printf("'%s'\n", dst_buf+6);
				}
*/
			IExec->FreeVec(dst_buf);
		} // END 'if(!rda){}else{..'


		IDOS->FreeArgs(rda);
	} // END 'if(res){..'
	else IDOS->PutErrStr("GETVERSION: Couldn't open \"utility.library\" +V52.\n");

	IExec->DropInterface( (struct Interface *)IUtility );
	IExec->CloseLibrary(UtilityBase);

	return(res);
}


BOOL showVersion(STRPTR name, STRPTR buf, int32 bufsize, int32 type)
{
	BPTR seglist;
	STRPTR str_ver = NULL, str_res = NULL;
	char temp2[512], temp[512] = "????", ver_str[10] = "$VER:";
	int32 n, oldpos, tmp_val;
	int i = 1; // having '$VER: '
	BOOL res = FALSE, no_com = FALSE;
#if USE_FIX_SOFTWARE
	#define SW_FIX 3 // number of software to workaround
	unsigned char soft_md5[SW_FIX][MD5_DIGEST_LENGTH] = { // md5sum of software to workaround
		{0x1b,0xd8,0xe3,0xe4,0xc8,0x49,0xb8,0x3c,0xa3,0xcc,0xa7,0x90,0xab,0x0b,0xb6,0xd0}, // LhA
		{0xb1,0xe9,0xb8,0xaf,0xce,0xd6,0xe2,0x3f,0xa2,0x26,0xa6,0xf2,0xe8,0x07,0xce,0x1a}, // codesets.library
		{0x21,0xef,0xba,0x80,0x28,0x92,0xcb,0xcc,0x5c,0x14,0x9d,0x88,0x01,0x39,0x4b,0xc5}  // openurl.library
	};
	int soft_i[SW_FIX] = {14, 13, 13}; // date string length; LhA, codesets.library,..
#endif

	// Check for NOCOMMENT flag (type+NO_COMMENT -> type+10 = 1x)
	if(type > 9) {
//IDOS->Printf("[debug]NO_COMMENT=%ld [%ld]\n",no_com,type);
		no_com = TRUE;
		if(type == NO_COMMENT) type = V_FULLNV;
		else type -= NO_COMMENT;
//IDOS->Printf("[debug]NO_COMMENT=%ld [%ld]\n",no_com,type);
	}

	// EXTVER: "\0$EXTVER: ProgramName ExtraVersionString (dd.mm.yyyy) Comment\0" 
	if(type == EXTVER) {
//IDOS->Printf("[debug]EXTVER [%ld]\n",type);
		IUtility->Strlcpy(ver_str, "$EXTVER:", 10);
		if( (res=BruteForce(name, temp, sizeof(temp), "$EXTVER:")) )
			type = V_FULLNV; // to remove 'EXT$VER:' if exists
	}

	// RESIDENT
	if(type == RESIDENT) {
		struct Resident *resmod = IExec->FindResident(name);
//IDOS->Printf("[debug]RESIDENT=0x%lx [%ld]\n",resmod,type);
		if(resmod) {
			IUtility->Strlcpy( temp, resmod->rt_IdString, sizeof(temp) );
			type = V_FULLNV; // to remove '$VER:' if exists
			res = TRUE;
		}
		else {
//			struct DosResidentSeg *drseg;
//			struct SignalSemaphore *drsem = IExec->FindSemaphore("DosResident");
//			IExec->ObtainSemaphoreShared(drsem);
//			drseg = IDOS->FindSegment(name, NULL, FALSE);
//IDOS->Printf("[debug]%ld\n",drseg->seg_UC);
//			if(drseg->seg_UC) {
//IDOS->Printf("[debug]0x%lx\n",drseg->seg_Name);
			/*IDOS->GetSegListInfoTags(drseg->seg_Seg,
		                          GSLI_VersionString,&str_ver,
		                          GSLI_ResidentVersionString,&str_res, TAG_DONE);
			if(str_ver) { IUtility->SNPrintf(temp, 512, "%s",str_ver); res = TRUE; }
			else if(str_res) { IUtility->SNPrintf(temp, 512, "%s",str_res); res = TRUE; }
IDOS->Printf("'%s'\n",temp);*/
//			}
//			IExec->ReleaseSemaphore(drsem);
		}
	}

	// ELF FILES
	if(res==FALSE && type!=EXTVER) {
		seglist = IDOS->LoadSeg(name);
//IDOS->Printf("[debug]ELF FILE=0x%lx [%ld]\n",seglist,type);
		if(seglist) {
			IDOS->GetSegListInfoTags(seglist,
		                          GSLI_VersionString,&str_ver,
		                          GSLI_ResidentVersionString,&str_res, TAG_DONE);
			if(str_ver) { IUtility->SNPrintf(temp, sizeof(temp), "%s",str_ver); res = TRUE; }
			else if(str_res) { IUtility->SNPrintf(temp, sizeof(temp), "%s",str_res); res = TRUE; }
//IDOS->Printf("[ELF FILE] '%s' %ld\n",temp,res);
			IDOS->UnLoadSeg(seglist);

			// Incorrect $VER data? -> "brute force" search
			if(IUtility->Strlen(temp) < 10) res = BruteForce(name, temp, sizeof(temp), "$VER:");
//IDOS->Printf("[BruteForce] '%ld'\n",res);
		}
		//else { IDOS->Printf("\nnot an ELF file\n"); }
	}

	// PLAIN TEXT FILES
	if(res==FALSE && type!=EXTVER) {
//IDOS->Printf("[debug]PLAIN FILE [%ld]\n",type);
		BPTR fhPlainFile = IDOS->FOpen(name, MODE_OLDFILE, 0);
		if(fhPlainFile != ZERO) {
			struct FReadLineData *frld = IDOS->AllocDosObjectTags(DOS_FREADLINEDATA, 0);
			while(IDOS->FReadLine(fhPlainFile, frld) > 0) {
				if(frld->frld_LineLength > 1) {
//IDOS->Printf("Line is %ld bytes: %s\n", frld->frld_LineLength, frld->frld_Line);
					oldpos = 0;
					for(i=4; i>0; i--) { // split string to find "$VER:"
						oldpos = IDOS->SplitName( frld->frld_Line, ' ', temp, oldpos, sizeof(temp) );
//IDOS->Printf("\n[debug]'%s'\n",temp);
						if(IUtility->Strnicmp(temp,"$VER:",5) == 0) {
							i = 1; // having '$VER: '
							res = TRUE;
							IUtility->Strlcpy( temp, frld->frld_Line+oldpos-6, sizeof(temp) );
							//IUtility->SNPrintf(temp, 512, "%s",frld->frld_Line+oldpos-6);
							break; // exit FOR
						}
					}
				}
				if(res) break; // exit WHILE
			}
			IDOS->FreeDosObject(DOS_FREADLINEDATA, frld);
			IDOS->FClose(fhPlainFile);
		}
		//else { IDOS->Printf("\n[debug]not plain text\n"); }
	}

	// DEVICES
	if(res==FALSE && type!=EXTVER) {
		i = IUtility->Strlen(name) - 1;
		if( *(name+i) == ':' ) { // "silly" check for semicolon in NAME
			if( IDOS->FileSystemAttrTags(FSA_StringNameInput, name,
			                             FSA_VersionStringR, &temp,
			                             FSA_VersionStringR_BufSize, sizeof(temp),
			                            TAG_DONE) ) {
				res = TRUE;
				i = 0; // not having '$VER: '
			}
		}
		//else { IDOS->Printf("\n[debug]not a DEVICE\n"); }
	}

	// Try BruteForce if everything else failed
	if(res == FALSE) {
		res = BruteForce(name, temp, sizeof(temp), "$VER:");
		i = 1; // having '$VER: '
	}

	*(buf) = '\0'; // "clean" destination string buffer

	// Show part of '$[EXT]VER:' asked/passed in TYPE
	if(res) {
		// "Remove" control chars (usually at the end of $[EXT]VER:...'\r\n')
		tmp_val = IUtility->Strlen(temp) - 1;
		for(n=2; n!=0; n--){
			if(*(temp+tmp_val) < 0x20) *(temp+tmp_val) = '\0';
			tmp_val--;
		}

		if(type > 0) { // skip if V_FULLNV (or V_FULL)
			IUtility->Strlcpy( temp2, temp, sizeof(temp2) );

#if USE_FIX_SOFTWARE
			unsigned char md5_hash[MD5_DIGEST_LENGTH];
			md5sum(name, md5_hash);
//IDOS->Printf("\n");
//for(n=0; n<MD5_DIGEST_LENGTH; n++) IDOS->Printf("%02lX", md5_hash[n]);
//IDOS->Printf(" (%s) md5sum\n",name);
			for(n=0; n<SW_FIX; n++) {
//for(int m=0; m<MD5_DIGEST_LENGTH; m++) IDOS->Printf("%02lX", soft_md5[n][m]);
				tmp_val = IUtility->Strnicmp( (STRPTR)md5_hash, (STRPTR)soft_md5[n], MD5_DIGEST_LENGTH );
				if(tmp_val == 0) {
					break; // exit FOR
				}
			}

//IDOS->Printf("\n%lu (%s) crc32\n",crc32b(name), name);
#endif
			oldpos = 0;
			for(i=i+type; i!=0; i--) { // "cuts" version string "$VER: name ver.rev (date) comment"
#if USE_FIX_SOFTWARE
				if(tmp_val==0 && type==V_DATE && i==1) { // workaround for some $VER (soft_md5)
					oldpos = IDOS->SplitName( temp2, ' ', temp, oldpos, sizeof(temp) );
					IUtility->SNPrintf(temp, soft_i[n], "%s",temp2+oldpos);
				}
				else
#endif
					oldpos = IDOS->SplitName( temp2, ' ', temp, oldpos, sizeof(temp) );
			}
		}

		i = 0; // changed/updated in "Remove '$[EXT]VER: ' if exists" (few lines below)

		// "Clean" version string output
		if(type == V_FULLNV) {
			// Add a Carriage Return '\n' if it has a COMMENT
			oldpos = IDOS->SplitName( temp, ')', temp2, 0, sizeof(temp2) );
			tmp_val = IUtility->Strlen(temp);
			if(oldpos>0 && oldpos!=tmp_val) {
				if(temp[oldpos] == ' ') temp[oldpos] = no_com? '\0' : '\n';  // don't show : add "comment"
				else {
					IUtility->Strlcpy(temp2, temp, oldpos); // copy from START to DATE
					if(no_com) IUtility->SNPrintf(temp2, sizeof(temp2), "%s)",temp2);
					else IUtility->SNPrintf(temp2, sizeof(temp2), "%s)\n%s",temp2,temp+oldpos); // add "comment"

					IUtility->Strlcpy( temp, temp2, sizeof(temp) );
				}
			}

			// Remove '$[EXT]VER:' (ver_str) if exists
			IDOS->SplitName( temp, ' ', temp2, 0, sizeof(temp2) );
			if(IUtility->Strnicmp(temp2,ver_str,IUtility->Strlen(ver_str)) == 0) i = IUtility->Strlen(ver_str)+1;//6;
		}

		//IUtility->SNPrintf(buf, bufsize, "%s",temp+i);
		IUtility->Strlcpy(buf, temp+i, bufsize);
	} // END of 'if(res){..'

	return(res);
}


BOOL BruteForce(STRPTR name, STRPTR temp, int32 bufsize, STRPTR ver_mode)
{
	int32 buf_len, n = 0;
	BOOL res = FALSE;
	uint8 *buf_txt = IExec->AllocVecTags(65535, AVT_ClearWithValue,0, TAG_DONE);

//IDOS->Printf("[debug]temp='%s' (%s) file='%s'\n",temp,ver_mode,name);

	if(buf_txt) {
		BPTR fhBinFile = IDOS->FOpen(name, MODE_OLDFILE, 0);
		if(fhBinFile != ZERO) {
			int ver_len = IUtility->Strlen(ver_mode);

			buf_len = IDOS->FRead(fhBinFile, buf_txt, 1, 65535); // only on first 64K of file
			buf_len = buf_len - ver_len + 1; // no version string on "last" buffer bytes
//IDOS->Printf("[debug]buf_len=%ld\n",buf_len);
			while(buf_len--) {
				if(IUtility->Strnicmp((STRPTR)(buf_txt+n),ver_mode,ver_len) == 0) {
					IUtility->Strlcpy(temp, (STRPTR)(buf_txt+n), bufsize);
					if(IUtility->Strlen(temp) > 10) { // expect at least "$[EXT]VER: a 0.1"
						res = TRUE;
						break; // exit WHILE
					}
				}
				n++;
			} // END 'while(buf_len--){..'
			IDOS->FClose(fhBinFile);
		} // END 'if(!fhBinFile){..'
		IExec->FreeVec(buf_txt);
	} // END 'if(buf_txt){..'

//IDOS->Printf("[debug]0'%s'\n",temp);
	return(res);
}


#if USE_FIX_SOFTWARE
// https://stackoverflow.com/questions/3395690/md5sum-of-file-in-linux-c
BOOL md5sum(STRPTR file, unsigned char *out)
{
	MD5_CTX c;
	char buf[512];
	int32 bytes;
	BPTR fh;

	if( (fh=IDOS->Open(file, MODE_OLDFILE)) == 0 ) return(FALSE);

	MD5_Init(&c);

	bytes = IDOS->Read(fh, buf, 512);
	while(bytes > 0) {
		MD5_Update(&c, buf, bytes);
		bytes = IDOS->Read(fh, buf, 512);
	}

	MD5_Final(out, &c);
	IDOS->Close(fh);

	//for(int n=0; n<MD5_DIGEST_LENGTH; n++) IDOS->Printf("%02lX", out[n]);
	//IDOS->Printf("\n");

	return(TRUE);
}


// https://github.com/yaojingguo/c-code/blob/master/crc.c
// This is the basic CRC-32 calculation with some optimization but no
//table lookup. The the byte reversal is avoided by shifting the crc reg
//right instead of left and by using a reversed 32-bit word to represent
//the polynomial.
//   When compiled to Cyclops with GCC, this function executes in 8 + 72n
//instructions, where n is the number of bytes in the input message. It
//should be doable in 4 + 61n instructions.
//   If the inner loop is strung out (approx. 5*8 = 40 instructions),
//it would take about 6 + 46n instructions.

//unsigned int crc32b(unsigned char *message)
/*unsigned int crc32b(STRPTR message)
{
	int i, j;
	unsigned int byte, crc, mask;

	i = 0;
	crc = 0xFFFFFFFF;

	while (message[i] != 0) {
		byte = message[i]; // get next byte.
		crc = crc ^ byte;

		for(j=7; j>=0; j--) { // do eight times.
			mask = -(crc & 1);
			crc = (crc >> 1) ^ (0xEDB88320 & mask);
		}
		i++;
	}
	
	return ~crc;
}
*/
#endif
