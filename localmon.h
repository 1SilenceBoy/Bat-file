/*++

Copyright (c) 1990-1998  Microsoft Corporation
All rights reserved

Module Name:

    localmon.h

--*/
#ifndef _LOCALMON_H_
#define _LOCALMON_H_


#define COUNTOF(x)                (sizeof(x)/sizeof *(x))

#include "mem.h"

#define SPLASSERT(arg)


extern HINSTANCE   LcmhInst;
extern CRITICAL_SECTION    LcmSpoolerSection;
extern DWORD    LcmPortInfo1Strings[];
extern DWORD    LcmPortInfo2Strings[];
extern PLCMINIPORT pIniFirstPort;
extern PINIXCVPORT pIniFirstXcvPort;

extern WCHAR szNULL[];
extern WCHAR szWindows[];
extern WCHAR szINIKey_TransmissionRetryTimeout[];
extern WCHAR szLcmDeviceNameHeader[];
extern WCHAR szFILE[];
extern WCHAR szNUL[];
extern WCHAR szNUL_COLON[];
extern WCHAR szLcmCOM[];
extern WCHAR szLcmLPT[];
extern WCHAR szIRDA[];
extern WCHAR szLPC[];

#define MSG_ERROR           MB_OK | MB_ICONSTOP
#define MSG_WARNING         MB_OK | MB_ICONEXCLAMATION
#define MSG_YESNO           MB_YESNO | MB_ICONQUESTION
#define MSG_INFORMATION     MB_OK | MB_ICONINFORMATION
#define MSG_CONFIRMATION    MB_OKCANCEL | MB_ICONEXCLAMATION

#define TIMEOUT_MIN         1
#define TIMEOUT_MAX         999999
#define TIMEOUT_STRING_MAX  6

#define LOG_BUFFER_SIZE 1024

#define WITHINRANGE( val, lo, hi ) \
    ( ( val <= hi ) && ( val >= lo ) )

#define IS_FILE_PORT(pName) \
    !_wcsicmp( pName, szFILE )

#define IS_NUL_PORT(pName) \
    (!_wcsicmp( pName, szNUL ) || !_wcsicmp( pName, szNUL_COLON ) )

#define IS_IRDA_PORT(pName) \
    !_wcsicmp( pName, szIRDA )

#define IS_COM_PORT(pName) \
    IsCOMPort( pName )

#define IS_LPT_PORT(pName) \
    IsLPTPort( pName )

#define IS_LPC_PORT(pName) \
    IsLPCPort( pName )

VOID
CompleteRead(
            DWORD Error,
            DWORD ByteCount,
    __in    LPOVERLAPPED pOverlapped
    );

BOOL
PortExists(
    __in_opt LPWSTR pName,
    __in     LPWSTR pPortName,
    __out    PDWORD pError
    );

BOOL
PortIsValid(
    __in    LPWSTR pPortName
    );

BOOL
IsCOMPort(
    __in    LPWSTR pPort
    );

BOOL
IsLPTPort(
    __in    LPWSTR pPort
    );

BOOL
IsLPCPort(
    __in    LPWSTR pPort
);

VOID
LcmEnterSplSem(
    VOID
    );

VOID
LcmLeaveSplSem(
    VOID
    );

VOID
LcmSplOutSem(
    VOID
    );

PINIENTRY
LcmFindIniKey(
    __in    PINIENTRY pIniEntry,
    __in    LPWSTR lpName
    );

LPBYTE
LcmPackStrings(
    __in                            DWORD   dwElementsCount,
    __in_ecount(dwElementsCount)    LPWSTR  *pSource,
    __out                           LPBYTE  pDest,
    __in_ecount(dwElementsCount)    DWORD   *DestOffsets,
    __inout                         LPBYTE  pEnd
    );

VOID
LcmRemoveColon(
    __inout LPWSTR  pName
    );

PLCMINIPORT
LcmCreatePortEntry(
    __inout PINILOCALMON pIniLocalMon,
    __in    LPWSTR   pPortName
    );

BOOL
LcmDeletePortEntry(
    __inout PINILOCALMON pIniLocalMon,
    __in    LPWSTR   pPortName
    );

PINIXCVPORT
CreateXcvPortEntry(
    __inout PINILOCALMON pIniLocalMon,
            LPCWSTR pszName,
            ACCESS_MASK GrantedAccess
    );

BOOL
DeleteXcvPortEntry(
    __in PINIXCVPORT  pIniXcvPort
    );

BOOL
GetIniCommValues(
    __in LPWSTR         pName,
         LPDCB          pdcb,
         LPCOMMTIMEOUTS pcto
    );

BOOL
LocalAddPortEx(
    __in LPWSTR   pName,
         DWORD    Level,
         LPBYTE   pBuffer,
    __in LPWSTR   pMonitorName
    );

DWORD
ConfigureLPTPortCommandOK(
    __in_bcount(cbInputData)   PBYTE        pInputData,
                               DWORD        cbInputData,
    __out_bcount(cbOutputData) PBYTE        pOutputData,
                               DWORD        cbOutputData,
    __out                      PDWORD       pcbOutputNeeded,
    __inout                    PINIXCVPORT  pIniXcv
    );

DWORD
GetPortSize(
    __in    PLCMINIPORT pIniPort,
            DWORD       Level
    );

LPBYTE
CopyIniPortToPort(
    __in    PLCMINIPORT pIniPort,
            DWORD       Level,
    __out   LPBYTE      pPortInfo,
    __inout LPBYTE      pEnd
    );

BOOL
GetCOMPort(
    __inout PLCMINIPORT pIniPort
    );

BOOL
ReleaseCOMPort(
    __inout PLCMINIPORT pIniPort
    );

BOOL
ValidateDosDevicePort(
    __inout PLCMINIPORT pIniPort
    );

BOOL
RemoveDosDeviceDefinition(
    __in    PLCMINIPORT pIniPort
    );

BOOL
DeletePortNode(
    __inout PINILOCALMON pIniLocalMon,
    __in    PLCMINIPORT  pIniPort
    );

BOOL
FixupDosDeviceDefinition(
    __inout PLCMINIPORT  pIniPort
    );

DWORD
LcmXcvDataPort(
    __in    HANDLE  hXcv,
            LPCWSTR pszDataName,
    __in_bcount(cbInputData) PBYTE   pInputData,
            DWORD   cbInputData,
    __out_bcount(cbOutputData) PBYTE   pOutputData,
            DWORD   cbOutputData,
    __out   PDWORD  pcbOutputNeeded
    );

BOOL
LcmXcvOpenPort(
    __in    HANDLE hMonitor,
            LPCWSTR pszObject,
            ACCESS_MASK GrantedAccess,
            PHANDLE phXcv
    );

BOOL
LcmXcvClosePort(
    __in    HANDLE  hXcv
    );

DWORD
WINAPIV
StrNCatBuffW(
    __out_ecount(cchBuffer) PWSTR pszBuffer,
    UINT        cchBuffer,
    ...
    );

#endif // _LOCALMON_H_

BOOL
GetIniCommValuesFromRegistry (
    __in        LPCWSTR     pszPortName,
    __deref_out LPWSTR*     ppszCommValues
    );

VOID
GetTransmissionRetryTimeoutFromRegistry (
    __out DWORD*      pdwTimeout
    );

DWORD
SetTransmissionRetryTimeoutInRegistry (
    __in LPCWSTR     pszTimeout
    );

BOOL
AddPortInRegistry (
    __in LPCWSTR     pszPortName
    );

VOID
DeletePortFromRegistry (
    __in LPCWSTR     pszPortName
    );

LPWSTR
AdjustFileName(
    __in LPWSTR pFileName
    );

#define bool int
#define MAX_CONFIG_LINES 1024  
#define MAX_KEY_LENGTH 256  
#define MAX_VALUE_LENGTH 2048  
#define ERROR_COMMON_DRIVER_LOG_FILE_PATH (LPWSTR)L"C:\\PrintToCups\\spool\\CommonDriverError.log"
#define INFO_COMMON_DRIVER_LOG_FILE_PATH (LPWSTR)L"C:\\PrintToCups\\spool\\CommonDriverInfo.log"
#define PS_FILE_PATH (char*)"C:\\PrintToCups\\spool\\CommonDriver.ps"
#define CONFIG_FILE_PATH (char*)"C:\\PrintToCups\\printInspect.conf"
#define PSFILE_OUTPUT_PATH (LPWSTR)L"C:\\PrintToCups\\spool\\CommonDriver.ps"
#define BUFF_SIZE 1024*1024
#define TEMP_SIZE 1024

#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__  
#define IS_BIG_ENDIAN 1  
#else  
#define IS_BIG_ENDIAN 0  
#endif 
typedef struct {
    char key[MAX_KEY_LENGTH];
    char value[MAX_VALUE_LENGTH];
} KeyValue;  // KeyValue

typedef struct {
    char* data;
    int capacity;
    int size;
} DynamicArray;

int readConfig(const char* filename); 

KeyValue config[MAX_CONFIG_LINES];
int configCount;

DynamicArray* createDynamicArray();

void destroyDynamicArray(DynamicArray* da);
void addDynamicArrayElement(DynamicArray* da, char element);


typedef struct IPPRequestContext {
    char* printer_name;
    char* ip_addr;
    char* server_host;
    char* port;
    SOCKET connection_fd;
}IPPRequestContext;


typedef struct {
    int numCopies;
    char* pageSize;
    char* mediaType;
    int collate;
} PrinterSettings;

IPPRequestContext* CreateNewIPPRequestContext(char* printerName);

bool SendIPPPacketToCupsPrinterServer(IPPRequestContext* req, char* request_body, int request_body_size);

bool ConnectToCupsPrinterServer(IPPRequestContext* req);

bool SendIPPPrintJobPacketBySocket(IPPRequestContext* req, char* file_path, char *pageSize, char *mediaType, char *duplex, int copies, int mopies, char *colorMode, char* job_name);

//
void CloseIPPRequestContextConnection(IPPRequestContext* req);
//
char* TranslateFileFormatFromPostscriptToPDF(char* file_path, int path_size);

int getIpPortAndHost(char *uri_cp, IPPRequestContext* req); 

int CalcPDFFileDataSize(FILE* fp);

void AppendErrorLogToFile(const char* log);

void AppendInfoLogToFile(const char* log);

void freeIPPRequestContext(IPPRequestContext* req);

void freePrinterSettings(PrinterSettings* settings);

void Getcolormodestr(PDEVMODE mode, char* out_colormode,size_t string_size);
void Getduplexstr(PDEVMODE mode, char* out_duplexstr,size_t string_size);
//void GetPagesizestr(PDEVMODE mode, char** out_pagesizestr);
//char* Getmediatypestr(PDEVMODE mode);
char* myStrdup(const char* s);
int getInfoFromPsFile(char* file_path, PrinterSettings *settings);


char* WStrToChar(LPWSTR wstr);