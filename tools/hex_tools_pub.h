////////////////////////////////////////////////////////////
//
// hex_tools
// Functions for manipulating AVR Intel Hex Files.
//
// Date:    11-21-2018
// Author:  12oClocker
// License: GNU GPL (see License.txt)
//
// REFERENCE...
// https://en.wikipedia.org/wiki/Intel_HEX
// Checksum calc... [all preceeding bytes are added using a U8, then ^ 0xFF and +1] CalcSum = (CalcSum ^ 0xFF) + 0x01;//2's compliment (final stage of calculating checksum)
// Intel Hex Files are in ASCII format
//   ,--S=Start
//   | ,--BB=ByteCount
//   | |   ,--AAAA=Address
//   | |   | ,--RR=RecordType
//   | |   | |   ,--DDDD=Data
//   | |   | |   | ,--CC=Checksum
//   | |   | |   | |
//   SBBAAAARRDDDDCC
//   :020000027F007D
//   :00000001FF
//
////////////////////////////////////////////////////////////


#include <errno.h>
#include <stdint.h>
#include <inttypes.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
//#include <signal.h>
#include <sys/types.h>
//#include <stdarg.h>         //vsnprintf vprintf
//#include <sys/stat.h>

#define MAX_FILE_SIZE  1048576         //max file read size is one MiB	
#define MAX_HEX_SIZE   65536           //max size of hex buffer

#define I8  int8_t
#define I16 int16_t
#define I32 int32_t
#define I64 int64_t
#define U8  uint8_t
#define U16 uint16_t
#define U32 uint32_t
#define U64 uint64_t

//text colors ... https://stackoverflow.com/questions/3585846/color-text-in-terminal-applications-in-unix
//only work in linux ... https://sourceforge.net/p/predef/wiki/OperatingSystems/
#if defined(__linux__)
#define RED   "\x1B[31m"
#define GRN   "\x1B[32m"
#define YEL   "\x1B[33m"
#define BLU   "\x1B[34m"
#define MAG   "\x1B[35m"
#define CYN   "\x1B[36m"
#define WHT   "\x1B[37m"
#define CEND  "\x1B[0m"
#else //windows or others
#define RED   ""
#define GRN   ""
#define YEL   ""
#define BLU   ""
#define MAG   ""
#define CYN   ""
#define WHT   ""
#define CEND  ""
#endif

//trace messages strerror(errno)
#ifdef  HEX_TOOLS_NO_LOG_TRACE
#define IHF_LOG(...)     while(0)
#define IHF_ERR(...)     while(0)
#else
#define IHF_LOG(...)     printf(__VA_ARGS__); printf("\n"); fflush(stdout)
#define IHF_ERR(...)     printf(RED "ERROR: LINE:%d FUNC:%s FILE:%s\nerrno=%d %s\n" CEND,__LINE__,__func__,__FILE__,errno,strerror(errno)); printf(RED); printf(__VA_ARGS__); printf(CEND "\n"); fflush(stdout)
#endif
//#define TRACE(...)     printf(__VA_ARGS__); fflush(stdout)
//#define ERRMSG(...)    printf(RED "\nFILE: %s\nLINE: %d\nFUNC: %s\nERROR: " CEND,__FILE__,__LINE__,__func__); printf(__VA_ARGS__); printf("\n"); fflush(stdout)
//#define ERREXIT(...)   printf(RED "\nFILE: %s\nLINE: %d\nFUNC: %s\nERROR: " CEND,__FILE__,__LINE__,__func__); printf(__VA_ARGS__); printf("\n"); fflush(stdout); exit(0)

//macros
#define ihf_get_hex_data_len(AdrLo, AdrHi)   (AdrHi - AdrLo + 1)  //get the length of hex data only
#define ihf_get_hex_full_len(AdrHi)          (AdrHi + 1)          //get full length with any begining bootloader bytes we skipped over

static U8 HexToNum(char c)
{
	if(c >= '0' && c <= '9'){return c - '0';}
	if(c >= 'a' && c <= 'z'){return c - 'a' + 10;}
	if(c >= 'A' && c <= 'Z'){return c - 'A' + 10;}
	return 0;
}
static U8 StrToHexU8(char* s)//for 2 byte hex strings, such as '0F' (642 bytes less than strtol(buf,0,16); )
{
	U8 n;
	n  = HexToNum(s[0]) << 4; //same as blah * 16
	n += HexToNum(s[1]);
	return n;
}
static U16 StrToHexU16(char* s)//for 2 byte hex strings, such as '0F' (642 bytes less than strtol(buf,0,16); )
{
	U16 n;
	n  = HexToNum(s[0]) << 12;
	n += HexToNum(s[1]) << 8;
	n += HexToNum(s[2]) << 4;
	n += HexToNum(s[3]);
	return n;
}
static U32 StrToHexU32(char* s)//for 2 byte hex strings, such as '0F' (642 bytes less than strtol(buf,0,16); )
{
	U32 n = 0;
	U8 x = 8;//convert 8 nibbles from 7 to 0
	U8 i = 0;
	while(x)
	{
		x--;
		n += HexToNum(s[i]) << (x*4);
		i++;
	}
	return n;
}
//REFERENCE...
//https://en.wikipedia.org/wiki/Intel_HEX
//Checksum calc... [all preceeding bytes are added using a U8, then ^ 0xFF and +1] CalcSum = (CalcSum ^ 0xFF) + 0x01;//2's compliment (final stage of calculating checksum)
//Intel Hex Files are in ASCII format
//  ,--S=Start
//  | ,--BB=ByteCount
//  | |   ,--AAAA=Address
//  | |   | ,--RR=RecordType
//  | |   | |   ,--DDDD=Data
//  | |   | |   | ,--CC=Checksum
//  | |   | |   | |
//  SBBAAAARRDDDDCC
//  :020000027F007D
//  :00000001FF
//read intel hex file into variable
#define ihf_read(sPath, pData, uDataSize)  ihf_read_ex(sPath, pData, uDataSize, 0, 0)
static U32 ihf_read_ex(char* sfile, U8* pData, U32 uDataSize, U32* pLowAdr, U32* pHighAdr)
{
	//open file
    FILE* f = fopen(sfile,"rb");
    if(f == NULL){IHF_ERR("Unable to open file %s",sfile); return 0;}
    IHF_LOG("opened file %s",sfile);
    
    //get file size
    fseek(f,0,SEEK_END);
    const int len = (int)ftell(f);
    fseek(f,0,SEEK_SET);//seek back to begining
    
    //validate buf len
    const int min_len = 11;
    const int max_len = MAX_FILE_SIZE; //1 MiB is max file size we support, got to draw the line somewhere
    //const int max_len = (1024 * 48 * 3);//hex file should not be larger than m48k * 3
    if(len > max_len || len < min_len)//hex file should not be larger than m48k * 3
    {
		IHF_ERR("Size of %d is not valid, max allowed is %d, min allowed is %d",len,max_len,min_len);
		fclose(f);
		return 0;
	}
    
    //read into buffer	
	char buf[len+1];//hex file should not be larger than m48k * 3
    int rd = fread(buf,1,len,f);
    fclose(f);
    if(rd < len)
    {
		IHF_ERR("Only read %d of %d bytes",rd,len); 
		return 0;
	}
    buf[len] = 0;//null last char
    
    //parse hex file...
    U32 Address_ELA_ESA = 0;
    char* s = buf; //search starts at "s" pointer
	int line=0;
	int total_bytes=0; //how many bytes in the flash file
	while(1)
	{
		line++;
		//find :
		while(*s!=':' && *s!=0){s++;}
		//are we at the end of the file?
		if(*s==0){IHF_ERR("Unexpected end of file on line %d",line); return 0;}		
		//get length of current line... "hello\r\n"
		char* e = s;
		while(*e!='\r' && *e!='\n' && *e!=0){e++;}
		int line_len = (e-s); //11 is a terminating line, 13 is minimum data line, nothing will be less than 11
		if(line_len < 11){IHF_ERR("Line length %d is too short on line %d",line_len,line); return 0;}
		if(!(line_len & 1)){IHF_ERR("Line is even, should be odd, on line %d",line); return 0;}
		int data_len = line_len - 11; //anything over 11 is ascii data bytes
		//grab the data
		U8  CalcSum    = 0;	
		U8  ByteCount  = StrToHexU8(s+1);           CalcSum+=ByteCount; //ascii hex chrs will be twice this count, since takes 2 chars per ascii hex byte
		U16 Address    = StrToHexU16(s+3);          CalcSum+=(Address >> 8);    CalcSum+=(Address & 0xFF);
		U16 RecordType = StrToHexU8(s+7);           CalcSum+=(RecordType >> 8); CalcSum+=(RecordType & 0xFF);
		U8  Checksum   = StrToHexU8(s+9+data_len);
		//printf("ByteCount=%03d  Address=%04X  RecordType=%02X  data_len=%03d  line=%03d\n",ByteCount,Address,RecordType,data_len,line);
		//validate byte length
		if((ByteCount*2) != data_len){IHF_ERR("Byte count * 2 is [%d] != data on line of [%d], on line %d",ByteCount,data_len,line); return 0;}
		//validate checksum
		for(int i=0; i<data_len; i+=2)
		{
			CalcSum += StrToHexU8(s+9+i);
		}		
		CalcSum = (CalcSum ^ 0xFF) + 0x01;
		if(CalcSum != Checksum){IHF_ERR("Calced checksum 0x%02X does not match 0x%02X on line %d",CalcSum,Checksum,line); return 0;}
		//Next Action Depends on Record Type...
		if(RecordType==0x02)//Extended Segment Address, ByteCount always 2, Address field Ignored, DataField multiplied by 16 and added to each following Address value, Remains in effect until next 02 record overwrites it
		{
			if(ByteCount != 2){IHF_ERR("Extended Segment Address byte count is not 2, on line %d",line); return 0;}
			if(data_len  != 4){IHF_ERR("Extended Segment Address data count is not 4, on line %d",line); return 0;}
			U16 data = StrToHexU16(s+9);
			Address_ELA_ESA = ((U32)data * 16);
		}
		if(RecordType==0x04)//Extended Linear Address, ByteCount always 2, Address field Ignored, DataField << by 16 and added to each following Address value, Remains in effect until next 04 record overwrites it
		{
			if(ByteCount != 2){IHF_ERR("Extended Linear Address byte count is not 2, on line %d",line); return 0;}
			if(data_len  != 4){IHF_ERR("Extended Linear Address data count is not 4, on line %d",line); return 0;}
			U16 data = StrToHexU16(s+9);
			Address_ELA_ESA = ((U32)data << 16);
		}
		if(RecordType==0x03)//AVRStudio4 uses this record for bootloaders, start address of where the image writes to on the Chip
		{
			if(ByteCount != 4){IHF_ERR("Bootloader Address data byte count is not 4, on line %d",line); return 0;}
			#ifndef  HEX_TOOLS_NO_LOG_TRACE
			U32 data = StrToHexU32(s+9);
			IHF_LOG("Ignoring record 0x03  0x%04X  %d",data,data);
			#endif
			//data value is the address and should match first address line, suspect it is a type of "double checking" for the hex file
		}	
        if(RecordType==0x00)//Data record
        {
            U32 CurAddress = (U32)Address + Address_ELA_ESA;//get current starting address for this line of data
            if((CurAddress + ByteCount) > (U32)uDataSize){IHF_ERR("Data Memory will overrun, on line %d",line); return 0;}
			char byte_buf[ByteCount];
			for(int i=0; i<data_len; i+=2)//spin through ascii hex chars
			{
				byte_buf[i/2] = StrToHexU8(s+9+i);//conver ascii hex char to single char byte
			}            
			memcpy(pData+CurAddress,byte_buf,ByteCount);
			if(pLowAdr && total_bytes==0){ *pLowAdr = CurAddress; } //address of first byte we read
			if(pHighAdr){ *pHighAdr = (CurAddress + ByteCount - 1); } //address of last byte we read
			total_bytes += ByteCount;
		}
		if(RecordType==0x01)//End of File
		{
			IHF_LOG("Read %d bytes",total_bytes);
			break;
		}
		//advance past :
		s=e;			
	}
	
	//make AdrHi and AdrLo word aligned, since AVR hex files are word aligned...
	#define IS_ODD(x)   ((x) & 1)         //is odd number?
	#define IS_EVEN(x)  (((x) & 1)==0)    //is even number?
	if(pLowAdr && IS_ODD(*pLowAdr) )//low byte should be on even address
	{
		(*pLowAdr)--;
	}
	if(pHighAdr && IS_EVEN(*pHighAdr) )//high byte should be on odd address
	{
		(*pHighAdr)++;
	}
	
	return total_bytes;	
}

//write intel hex file, len is the size of buffer we are spinning through
static U8 ihf_write(char* sfile, U8* pData, U32 len)
{
	//open file
    FILE* f = fopen(sfile,"wb");
    if(f == NULL){IHF_ERR("Unable to open file %s",sfile); return 0;}
    IHF_LOG("created file %s",sfile);
	
	//setup pointer for increment
	U8* data = pData;
	U32 byte_cnt = len;
	
	//    ,--S=Start
	//    | ,--BB=ByteCount
	//    | |   ,--AAAA=Address
	//    | |   | ,--RR=RecordType
	//    | |   | |   ,--DDDD=Data
	//    | |   | |   | ,--CC=Checksum
	//    | |   | |   | |
	//    SBBAAAARRDDDDCC
	//    :020000027F007D
	//    :00000001FF
	
	//write the pages to intel hex file
	U16 address = 0; //for intel hex files, device flash addresses ALWAYS start at 0x0000 and NOT the pNvm->start address	
	//write the bytes to intel hex file
	U32 written = 0;
	while(byte_cnt)
	{
		//write 16 bytes per line until we are out of data to write
		U32 cnt = 0;
		if(byte_cnt >= 16){cnt=16;}else{cnt=byte_cnt;} byte_cnt-=cnt;
		//read ahead and see if this line has any non 0xFF data (spin backwards through the data line)
		U32 l = cnt;
		while(l){ if(data[l-1]!=0xFF){break;} l--; }
		if(l & 1){l++;}//if l ended up being an odd number, make it even again, since AVR instructions are 2 bytes each.
		//l is now the length of data bytes to write for this 16 byte line
		if(l)
		{
			written+=l;//keep track of number of bytes written to file
			//write 16 bytes per line until we are out of data to write
			U8 RecordType = 0x00;
			U8 cs = l; cs+=((address>>8) & 0xFF); cs+=(address&0xFF); cs+=RecordType;
			fprintf(f,":%02X%04X%02X",l,address,RecordType);
			for(U32 x=0; x<l; x++){ fprintf(f,"%02X",data[x]); cs+=data[x]; }
			cs = (cs ^ 0xFF) + 0x01;
			fprintf(f,"%02X\r\n",cs);//print checksum
		}
		//increment
		address += cnt;
		data    += cnt;
	}				
	fprintf(f,":00000001FF\r\n");//write the terminating line for intel hex file
	fclose(f);	
	IHF_LOG("Write done, %d bytes of data written",written);
	return 1;
}

static U8 ihf_merge(char* sIn1, char* sIn2, char* sOut)
{
	//create empty buffer for saving data
	const U32 len = MAX_HEX_SIZE;
	U8 buf[len];
	memset(buf,0xFF,sizeof(buf));//use size of in case of compiler padding bytes
	
	//read the intel hex file data into buffer
	U32 lo1=0;	U32 hi1=0;
	U32 lo2=0;	U32 hi2=0;
	U32 rlen1 = ihf_read_ex(sIn1,buf,len,&lo1,&hi1);
	U32 rlen2 = ihf_read_ex(sIn2,buf,len,&lo1,&hi1);
	
	//do some checks
	if(rlen1 < 1){ IHF_ERR("Hex file has no data %s",sIn1); return 0;}
	if(rlen2 < 1){ IHF_ERR("Hex file has no data %s",sIn2); return 0;}
	
	//make sure data does not overlap
	if( (lo1 >= lo2 && lo1 <= hi2) || //lo1 is between lo2 and hi2
	    (hi1 >= lo2 && hi1 <= hi2) || //hi1 is between lo2 and hi2
	    (lo2 >= lo1 && lo2 <= hi1) || //lo2 is between lo1 and hi1
	    (hi2 >= lo1 && hi2 <= hi1) )  //hi2 is between lo1 and hi1
	{ IHF_ERR("The two hex files have overlapping data");return 0; }
	
	//write the combined intel hex file
	return ihf_write(sOut,buf,len);
}

static U8 ihf_hex2bin(char* sIn, char* sOut)
{
	//printf("\n---hex2c---\nconverting intel hex to C variable\n");
	
	//create buffer to work with
	const U32 len = MAX_HEX_SIZE;
	U8 data[len];
	memset(data,0xFF,sizeof(data));
	
	//read intel hex file into buffer
	U32 AdrStart = 0;
	U32 AdrEnd   = 0; //address of last byte we read
	if(!ihf_read_ex(sIn,data,len,&AdrStart,&AdrEnd)){return 0;}
	
	//get start adr and end adr of data...
	if(AdrStart == -1 || AdrEnd == -1){ IHF_ERR("No data found in hex file"); return 0;}
	
	//fix any odd starting or ending addresses, since AVR instructions are 16bit and everything is word aligned in avr hex
	if(AdrStart & 1){AdrStart--;}  //if odd, then -- 1  (start address should always land on the even byte)
	if((AdrEnd & 1)==0){AdrEnd++;} //if even, then ++ 1 (end address should always land on the odd byte)
	int data_len = AdrEnd - AdrStart + 1; //length of data we have
	
	//revalidate
	if(AdrStart == -1 || AdrEnd == -1){ IHF_ERR("No data found in hex file"); return 0;}
	if(data_len < 1 || data_len > MAX_HEX_SIZE){ IHF_ERR("Data len %d is not valid",data_len); return 0;}	
	
	//open file
    FILE* f = fopen(sOut,"wb");
    if(f == NULL){IHF_ERR("Unable to open file for writing %s",sOut); return 0;}	
    
	//write the data
	for(U32 i=0; i<=AdrEnd; i++)
	{
		fputc(data[i],f);
	}
	fclose(f);
	IHF_LOG("done, wrote %d bytes to raw hex binary",AdrEnd+1);
	return 1;  	
}

//convert plain hex file, such as "00,FF,AB" to intel hex file
static U8 ihf_txt2hex(char* sIn, char* sOut)
{
	//open file
    FILE* f = fopen(sIn,"rb");
    if(f == NULL)
    {IHF_ERR("Unable to open file %s",sIn); return 0;}
    IHF_LOG("opened file %s",sIn);
    
    //get file size
    fseek(f,0,SEEK_END);
    const int len = (int)ftell(f);
    fseek(f,0,SEEK_SET);//seek back to begining
    
    //validate size
    int max = MAX_FILE_SIZE; //1 MiB is max file size we support, got to draw the line somewhere
    if(len > max || len < 1)//hex file should not be larger than m48k * 3
    {
		IHF_ERR("Size of %d is not valid, max allowed is %d, min allowed is %d",len,max,1);
		fclose(f);
		return 0;
	}
    
    //read into buffer	
	char buf[len+1];
    int rd = fread(buf,1,len,f);
    fclose(f);
    if(rd < len)
    {
		IHF_ERR("Only read %d of %d bytes",rd,len); 
		return 0;
	}
    buf[len] = 0;//null last char 
    
    //create buffer for converted hex values
    U8 hex[len+1]; 
    hex[len] = 0;//null last char      
    
    //parse file
    int i=0;
    #define IsHex(s)       ( (*(s)>='0' && *(s)<='9') || (*(s)>='a' && *(s)<='f') || (*(s)>='A' && *(s)<='F') )
    char* s = buf;
    while(1)
    {
		while( *s && !IsHex(s) ){s++;}//spin until we hit a valid hex char or end of file
		if(!*s){break;}//end of file
		if(s[1]=='x'){goto next;}//got "0x" so skip it and get to the hex data
		if(IsHex(s+1)){ hex[i]=StrToHexU8(s); i++;}//got a hex character such as 00
		else{ IHF_ERR("Hex nibbles not supported at offset %d",(int)(s-buf+1)); return 0;}//nibble not supported, must have full 2 character hex data
		next:
		s+=2;//advance past ascii hex we just extracted
	}
	IHF_LOG("Extracted %d hex bytes from txt file",i);
	
	//validate size, we dont support anything of 0xFFFF
	int hex_limit = MAX_HEX_SIZE;
	if(i > hex_limit)
	{
		IHF_ERR("Hex data over 0x%04X is not supported",hex_limit); 
		return 0;		
	}
	
	//now write the intel hex file, i is our hex file size
	return ihf_write(sOut,hex,i);    
}

/*
//these functions work but are not needed for this program

//returns address of starting data byte, which is not 0xFF
//returns -1 if not found
U32 GetDataStartAdr(U8* data, U32 len)
{
	for(int i=0; i<len; i++)
	{
		if(data[i] != 0xFF){return i;}
	}
	return -1;//no data found
}

//returns address of last data byte, which is not 0xFF
//returns -1 if not found
U32 GetDataEndAdr(U8* data, U32 len)
{
	while(len)
	{
		len--;
		if(data[len] != 0xFF){return len;}
	}

	return -1;//no data found
}
*/

//convert intel hex file to C variable
#define ihf_hex2c(sIn, sOut)   ihf_hex2c_ex(sIn,sOut,0,16)
static U8 ihf_hex2c_ex(char* sIn, char* sOut, char* sPreTxt, int ascii_nl_width)
{
	//printf("\n---hex2c---\nconverting intel hex to C variable\n");
	
	//create buffer to work with
	const U32 len = MAX_HEX_SIZE;
	U8 data[len];
	memset(data,0xFF,sizeof(data));
	
	//read intel hex file into buffer
	//if(!ReadIntelHexFile(sIn,data,len)){return 0;}
	U32 AdrStart = 0;
	U32 AdrEnd   = 0;
	if(!ihf_read_ex(sIn,data,len,&AdrStart,&AdrEnd)){return 0;}
	
	//get start adr and end adr of data...
	//U32 AdrStart = GetDataStartAdr(data,len);
	//U32 AdrEnd   = GetDataEndAdr(data,len);
	if(AdrStart == -1 || AdrEnd == -1){ IHF_ERR("No data found in hex file"); return 0;}
	
	//fix any odd starting or ending addresses, since AVR instructions are 16bit and everything is word aligned in avr hex
	if(AdrStart & 1){AdrStart--;}  //if odd, then -- 1  (start address should always land on the even byte)
	if((AdrEnd & 1)==0){AdrEnd++;} //if even, then ++ 1 (end address should always land on the odd byte)
	int data_len = AdrEnd - AdrStart + 1; //length of data we have
	
	//revalidate
	if(AdrStart == -1 || AdrEnd == -1){ IHF_ERR("No data found in hex file"); return 0;}
	if(data_len < 1 || data_len > MAX_HEX_SIZE){ IHF_ERR("Data len %d is not valid",data_len); return 0;}	
	
	//open file
    FILE* f = fopen(sOut,"wb");
    if(f == NULL){IHF_ERR("Unable to open file for writing %s",sOut); return 0;}	
    
	//write the data
	if(sPreTxt){fprintf(f,"%s\r\n",sPreTxt);}//append pre-text first
	fprintf(f,"#define FIRM_ADR_START %d\r\n",AdrStart);
	fprintf(f,"#define FIRM_ADR_END   %d\r\n",AdrEnd);
	fprintf(f,"#define FIRM_LEN       %d\r\n",data_len);
	fprintf(f,"static const uint8_t g_HexVariable[] PROGMEM = { \r\n");
	int nl = 0;
	for(U32 i=AdrStart; i<=AdrEnd; i++)
	{
		nl++;
		fprintf(f,"0x%02X",data[i]); 
		if(i!=AdrEnd){ fprintf(f,","); if(nl==ascii_nl_width){nl=0; fprintf(f,"\r\n");} }
	}
	fprintf(f,"\r\n};\r\n");
	fclose(f);
	IHF_LOG("done, wrote %d bytes to C variable file",data_len);
	return 1;  
}



















