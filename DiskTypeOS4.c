;/*
gcc -Wall -gstabs -o DiskTypeOS4 DiskTypeOS4.c
quit
*/
/* DiskType.c - program to display disk types for volumes currently in
 *              system.
 *
 * Doesn't check all allocations!
 *
 * ToDo: ReadArgs() interface to specify a particular device
 *       Pooled memory allocation? nah, no real point.
 *
 * Modified by Scott A. Cabit 04/14/07 to include new OS4 filetypes.
 * More OS4 modifications by Javier de las Rivas 3/07/2020
 */

#include <proto/dos.h>
#include <proto/exec.h>
#include <proto/utility.h>

struct Library *UtilityBase = NULL;
struct UtilityIFace *IUtility = NULL;

void sort_list(int32 n,struct Hook *);
int CompareNameNodes(struct Hook *, struct Node *, struct Node *);

struct List *list;
const char *version = "\0$VER: DiskTypeOS4 43.3 (03.07.2020)";


int main(void)
{
	struct Node *nd = NULL;
	struct InfoData id;
	struct DevProc *dp = NULL;
	struct Hook *prhook = NULL;
	struct FileSystemData *fsd = NULL;
	struct ClockData cd;
	struct DosList *dl = NULL;
	uint64 disksize, diskusedsize;
	int32 sizeunit, disktype, count = 0;
	STRPTR sizeunit_str[] = {"B","K","M","G","T","P","E", NULL};
	char name_buf[128], temp_buf[128], temp_date[22];

	if( !(DOSBase=IExec->OpenLibrary("dos.library", 52)) ) goto clean_exit;

	if( !(UtilityBase=IExec->OpenLibrary("utility.library", 52)) ) goto clean_exit;
	IUtility = (struct UtilityIFace *)IExec->GetInterface(UtilityBase, "main", 1, NULL);

	list = IDOS->AllocDosObjectTags(DOS_VOLUMELIST,
	                                //ADO_Type,LDF_VOLUMES, ADO_AddColon,TRUE,
	                                ADO_Type,LDF_DEVICES, ADO_AddColon,TRUE,
	                               TAG_END);
	if(list) {
		IDOS->Printf("Volume          Block   Size    Used  Name                           FileSystem/Handler\n");

		for( nd=IExec->GetHead(list); nd; nd=IExec->GetSucc(nd) ) count++;
		// Sort list by volume/device name
		prhook = (struct Hook *)IExec->AllocSysObjectTags(ASOT_HOOK,
		                                                  ASOHOOK_Entry,CompareNameNodes,
		                                                 TAG_DONE);
		sort_list(count, prhook);
		IExec->FreeSysObject(ASOT_HOOK, prhook);
//for( nd=IExec->GetHead(list); nd; nd=IExec->GetSucc(nd) ) { IDOS->Printf("%-20s\n",nd->ln_Name); }

		for( nd=IExec->GetHead(list); nd; nd=IExec->GetSucc(nd) ) {
			//if( (dp=IDOS->GetDeviceProc(nd->ln_Name, NULL)) ) { // get DevProc
			if( (dp=IDOS->GetDeviceProcFlags(nd->ln_Name, NULL, LDF_DEVICES)) ) { // get DevProc
				*temp_date = '\0';

				if( IDOS->IsFileSystemPort(dp->dvp_Port) ) { // check for "valid" FSPort
					IDOS->Printf("%-11s",nd->ln_Name); // device name
					// When using LDF_VOLUMES -> Get volume name
					//IDOS->DevNameFromPort(dp->dvp_Port, temp_buf, sizeof(temp_buf), TRUE);
					//IDOS->Printf("%-11s",temp_buf);

					IDOS->GetDiskInfoTags(GDI_MsgPortInput,dp->dvp_Port, GDI_InfoData,&id, TAG_END);

					disktype = id.id_DiskType;
					if(disktype == ID_NO_DISK_PRESENT) IDOS->Printf("      [no disk in drive]  %31s","");
					else {
						IDOS->Printf(" %9lD ",id.id_BytesPerBlock);

						disksize = diskusedsize = 0;

						sizeunit = 0;
						disksize = id.id_NumBlocks>0? (uint64)id.id_NumBlocks*id.id_BytesPerBlock : 0;
						while(disksize > 10000) {
							disksize /= 1024;
							sizeunit++;
						}
						IDOS->Printf("%5llD%s%s",disksize,"",sizeunit_str[sizeunit]);

						sizeunit = 0;
						diskusedsize = id.id_NumBlocksUsed>0? (int64)id.id_NumBlocksUsed*id.id_BytesPerBlock : 0;
						while(diskusedsize > 10000) {
							diskusedsize /= 1024;
							sizeunit++;
						}
						IDOS->Printf("  %5llD%s%s",diskusedsize,"",sizeunit_str[sizeunit]);

						// Volume name
						IDOS->NameFromPort(dp->dvp_Port, name_buf, sizeof(name_buf), FALSE);
						IDOS->Printf("  %-30s",name_buf);
//IExec->DebugPrintF("1)0x%08lx\n",disktype);
						// "Fix" disktype (CrossDOSFS)
						if( (fsd=IDOS->GetDiskFileSystemData(nd->ln_Name)) )
							disktype = fsd->fsd_Environment->de_DosType;
//IExec->DebugPrintF("2)0x%08lx\n",disktype);
						IDOS->Printf(" (%lc%lc%lc",disktype>>24,disktype>>16,disktype>>8);
						if( (disktype&0xFFFFFF00)!=0x46415400 && (disktype&0xFF)>0x29 ) IDOS->Printf("%lc)  ",disktype&0xFF); // 0x46415400 -> FAT\xx
						else IDOS->Printf("\\%02lx)",disktype&0xFF);
//						if( (disktype&0xFFFF0000) == 0x43440000 ) IDOS->Printf("\\0%lc)",disktype&0xFF); // 0x43440000 -> CD0\xx
//						else IDOS->Printf("\\%02lx)",disktype&0xFF);

						// Get creation date
						uint32 flags = LDF_VOLUMES|LDF_READ;
						dl = IDOS->LockDosList(flags);
						while( (dl=IDOS->FindDosEntry(dl, name_buf, flags)) ) {
							IUtility->Amiga2Date(IDOS->DateStampToSeconds(&dl->dol_misc.dol_volume.dol_VolumeDate), &cd);
//IDOS->Printf(" %lu-%02lu-%02lu %02lu:%02lu:%02lu",cd.year,cd.month,cd.mday,cd.hour,cd.min,cd.sec);
							IUtility->SNPrintf(temp_date, 22, " %lu-%02lu-%02lu %02lu:%02lu:%02lu",cd.year,cd.month,cd.mday,cd.hour,cd.min,cd.sec);
							dl = IDOS->NextDosEntry(dl, flags);
						}
						IDOS->UnLockDosList(flags);

//IDOS->AllocDosObjectTags(DOS_VOLUMELIST, TAG_END);

					}

					// Get handler and version
					if( IDOS->FileSystemAttrTags(FSA_MsgPortInput, dp->dvp_Port,
					                             //FSA_StringNameInput, nd->ln_Name,
					                             FSA_VersionStringR, &temp_buf,
					                             FSA_VersionStringR_BufSize, sizeof(temp_buf),
					                            TAG_DONE) ) {
//IDOS->Printf(" '%s'\n", temp_buf);
						count = IDOS->SplitName( temp_buf, ' ', name_buf, 0, sizeof(name_buf) );
							IDOS->Printf(" %s ", name_buf);
							count = IDOS->SplitName( temp_buf, ' ', name_buf, count, sizeof(name_buf) );
							IDOS->Printf("V%s ", name_buf);
					}
					else { IDOS->Printf(" <unknown filesystem/handler> "); }

					// Show device:unit
					if(fsd) IDOS->Printf("[%s:%ld] ",fsd->fsd_DeviceName,fsd->fsd_DeviceUnit);
					fsd = NULL;

					IDOS->Printf("%s\n",temp_date);
					//IDOS->Printf("\n");

				} // END if( IDOS->IsFileSystemPort(...
			} // END if( (dp=IDOS->GetDeviceProc(...
			IDOS->FreeDeviceProc(dp);

		} // END for( nd=IExec->GetHead(...
		IDOS->FreeDosObject(DOS_VOLUMELIST, list);

	} // END if(list)

clean_exit:
	IExec->DropInterface( (struct Interface *)IUtility );
	IExec->CloseLibrary(UtilityBase);
	IExec->CloseLibrary(DOSBase);
	return(0);
}

/* sort_list() by Fredrik 'salas00' Wikstrom */
void sort_list(int32 n, struct Hook *cmphook) {
	struct Node *curr, *next;
	int32 i, j;
//IDOS->Printf("SORTING list:\n");
	for(i=0; i<n; i++) {
		curr = IExec->GetHead(list);
		for(j=0; j<n-1-i; j++) {
			next = IExec->GetSucc(curr);
			if( (int)IUtility->CallHookPkt(cmphook,curr,next) > 0 ) {
				IExec->Remove(curr);
				IExec->Insert(list, curr, next);
			}
			else { curr = next; }
		}
	}
}
/* sort_list() comparation hook */
int CompareNameNodes(struct Hook *hook, struct Node *node1, struct Node *node2)
{
//	dp=IDOS->GetDeviceProc(nd->ln_Name, NULL)) ) { // get DevProc
//	IDOS->DevNameFromPort(dp->dvp_Port, temp_buf, sizeof(temp_buf), TRUE);

//IDOS->Printf("[0x%08lx]'%s' <-?-> [0x%08lx]'%s'\n",node1,node1->ln_Name,node2,node2->ln_Name);
	return( IUtility->Stricmp(node1->ln_Name,node2->ln_Name) );
}

