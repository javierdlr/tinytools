;/*
gcc -Wall -gstabs lspci.c -o lspci -D__AMIGADATE__=\"`c:date LFORMAT %d.%m.%Y`\"
quit
Or VBCC: remove ';' in line 1 and then 'vc lspci.c -o lspci' from CLI/Shell
*/

//#define DEBUG
#define DESC_BUF_SIZE  512
/* Syntax IDs in pci.ids database */
/* VENDOR:4chars+'  ; DEVICE:\t+4chars+'  '; (+1 byte in case...) */
#define BUF_VEN  7
#define BUF_DEV  8

#define VERSION   1
#define REVISION  6
#ifdef __VBCC__
#define DATE  __AMIGADATE__
#else
#define DATE  "("__AMIGADATE__")"
#endif
#define VERS     "lspci 1.6"
#define VSTRING  VERS" "DATE"\r\n"
#define VERSTAG  "\0$VER: "VERS" "DATE


#include <proto/exec.h>
#include <proto/dos.h>
#include <proto/expansion.h>
#include <proto/utility.h>


BOOL FindDescription(UWORD, UWORD, STRPTR, int32);


struct Library *ExpansionBase = NULL;
struct Library *UtilityBase = NULL;

struct PCIIFace *IPCI = NULL;
//struct ExpansionIFace *IExpansion = NULL;
struct UtilityIFace *IUtility = NULL;

const char *version = VERSTAG;


int main(void)
{
	struct PCIDevice *dev = NULL;
	struct PCIResourceRange *range = NULL;
	//struct ConfigDev *cd = NULL;
	struct RDArgs *rdargs;
	STATIC struct{
	              LONG MoreInfo;
	             } Args;
	uint32 iobase;
	int32 pci_index = 0, res = TRUE, basenum;
	uint16 VendorID, DeviceID;
	uint8 bus, devi, func;
	STRPTR desc_txt = IExec->AllocVecTags(DESC_BUF_SIZE, AVT_ClearWithValue,0, TAG_DONE);
	CONST_STRPTR argTemplate = "V=VERBOSE/S";

	//if( !IExec->OpenLibrary("dos.library", 52) ) res = FALSE;

	UtilityBase = IExec->OpenLibrary("utility.library", 52);
	if(!UtilityBase) res = FALSE;
	IUtility = (struct UtilityIFace *)IExec->GetInterface(UtilityBase, "main", 1, NULL);
	if(!IUtility) res = FALSE;

	ExpansionBase = IExec->OpenLibrary("expansion.library", 52);
	if(!ExpansionBase) res = FALSE;
	//IExpansion = (struct ExpansionIFace *)IExec->GetInterface((struct Library *)ExpansionBase, "main", 1, NULL);
	//if(!IExpansion) res = FALSE;
	IPCI = (struct PCIIFace *)IExec->GetInterface((struct Library *)ExpansionBase, "pci", 1, NULL);
	if(!IPCI) res = FALSE;

	if(res) {
		rdargs = IDOS->ReadArgs(argTemplate, (LONG *)&Args, NULL);
/*
		while( (cd=IExpansion->FindConfigDev(cd, -1, -1)) ) {
			IDOS->Printf("cd=0x%08lx\n",cd);
		}
*/
		IDOS->Printf("\033[1m##  Ven   Dev  Description\033[0m\n");
		while( (dev=IPCI->FindDeviceTags(//FDT_VendorID, PCI_ANY_ID,
		                                 //FDT_DeviceID, PCI_ANY_ID,
		                                 FDT_Index, pci_index++,
		                                TAG_DONE)) != NULL ) {
			IDOS->Printf("%02ld ",pci_index);

			VendorID = dev->ReadConfigWord(PCI_VENDOR_ID);
			DeviceID = dev->ReadConfigWord(PCI_DEVICE_ID);
			IDOS->Printf("%04lx  %04lx  ",VendorID, DeviceID);
			if( FindDescription(VendorID, DeviceID, desc_txt, DESC_BUF_SIZE) ) IDOS->Printf("%s", desc_txt);
			else IDOS->Printf("unknown vendor/device");

			if(Args.MoreInfo) {
				IDOS->Printf("\n  \033[1mIOBase   :\033[0m ");

				basenum = 0; iobase = 0;
				range = dev->GetResourceRange(basenum); // PCI_BASE_ADDRESS_0
				if(range) {
					iobase = range->BaseAddress;
					dev->FreeResourceRange(range);
					IDOS->Printf("%ld:0x%08lX", basenum, iobase);

					// maybe DEV has more IOBase values (PCI_BASE_ADDRESS_1..PCI_BASE_ADDRESS_5)
					for(basenum=1; basenum!=6; basenum++) {
						if( (range=dev->GetResourceRange(basenum)) ) {
							if( (iobase=range->BaseAddress) ) IDOS->Printf("; %ld:0x%08lX", basenum, iobase);
							else basenum = 6; // quit FOR(), but..

							dev->FreeResourceRange(range); // ..free RANGE
						}
					} // END for(...
				} // END if(range..

				dev->GetAddress(&bus,&devi,&func);
				IDOS->Printf("\n  \033[1mSlot     :\033[0m %02lX:%02lX.%lX", bus, devi, func);

				IDOS->Printf("\n  \033[1mClass    :\033[0m Base:0x%02lx  Sub:0x%02lx  IFace:0x%02lx",
				                                 (dev->ReadConfigWord(PCI_CLASS_DEVICE)&0xFF00)>>8,
				                                  dev->ReadConfigWord(PCI_CLASS_DEVICE)&0x00FF,
				                                  dev->ReadConfigByte(PCI_CLASS_PROG) // not sure about this one
				);

				IDOS->Printf("\n  \033[1mRevision :\033[0m 0x%02lx", dev->ReadConfigByte(PCI_CLASS_REVISION) );

				IDOS->Printf("\n  \033[1mInterrupt:\033[0m Line:0x%02lx  Pin:0x%02lx",
				                                  dev->ReadConfigByte(PCI_INTERRUPT_LINE),
				                                  dev->ReadConfigByte(PCI_INTERRUPT_PIN)
				);
			} // END if(Args.MoreInfo)

			IDOS->Printf("\n");

			IPCI->FreeDevice(dev);
			dev = NULL;
		} // END while( (dev=IPCI..

		IDOS->FreeArgs(rdargs);
	}

	IExec->FreeVec(desc_txt);

	//IExec->DropInterface( (struct Interface *)IExpansion );
	IExec->DropInterface( (struct Interface *)IPCI );
	IExec->DropInterface( (struct Interface *)IUtility );

	IExec->CloseLibrary( (struct Library*)ExpansionBase );
	IExec->CloseLibrary(UtilityBase);

	return res;
}


BOOL FindDescription(UWORD vID, UWORD dID, STRPTR tmp, int32 tmp_buf)
{
	BPTR fhFile = IDOS->FOpen("DEVS:pci.ids", MODE_OLDFILE, 0);
	int32 length = 6, counter = 0;
	char text[10];

	if(fhFile != ZERO) {
		struct FReadLineData *frld = IDOS->AllocDosObjectTags(DOS_FREADLINEDATA, 0);

		*(tmp) = '\0';
		//IUtility->SNPrintf(tmp, tmp_buf, "unknown vendor/device");

		IUtility->SNPrintf(text, BUF_VEN, "%04lx  ",vID); // pci.ids syntax
		length = IUtility->Strlen(text);

		while(IDOS->FReadLine(fhFile, frld) > 0) {
			if(frld->frld_LineLength > 1) {
//IDOS->Printf("Line is %ld bytes: %s", frld->frld_LineLength, frld->frld_Line);
				if(frld->frld_Line[0] != '#') { // bypass commented '#' lines
					if(IUtility->Strnicmp(frld->frld_Line, text, length) == 0) {
						counter++;
						// merge VENDOR_ID string with DEVICE_ID string
						length = IUtility->SNPrintf(tmp, tmp_buf, "%s%s",tmp,frld->frld_Line+length);
						*(tmp+IUtility->Strlen(tmp)-1) = ' '; // "replace" '\n' with ' ' (space between VEN and DEV)

						if(counter == 1) { // change search text to DEVICE_ID
							IUtility->SNPrintf(text, BUF_DEV, "\t%04lx  ",dID); // pci.ids syntax
							length = IUtility->Strlen(text);
						}

						if(counter == 2) break; // DEVICE_ID found, end of search
					}
				} // END if(frld->frld_Line[0]..
			} // END if(frld->frld_LineLength..
		} // END while(IDOS->FReadLine(f..

		IDOS->FreeDosObject(DOS_FREADLINEDATA, frld);
		IDOS->FClose(fhFile);
	} // END if(fhFile..
	else {
		IUtility->SNPrintf(tmp, tmp_buf, "<file 'DEVS:pci.ids' not found>");
		length = 0;
	}

//IDOS->Printf("[%ld]",length);
	return(length==6? FALSE : TRUE); // 6 -> VENDOR_ID+'  ' (aka no description available))
}
