/*++

Copyright (c) 1990-2003  Microsoft Corporation
All rights reserved

Module Name:

	localmon.c

--*/

#include "precomp.h"


#pragma hdrstop

#include <DriverSpecs.h>
__user_driver

#include <lmon.h>
#include "irda.h"
#include <time.h>
HANDLE              LcmhMonitor;
HINSTANCE           LcmhInst;
CRITICAL_SECTION    LcmSpoolerSection;

PMONITORINIT        g_pMonitorInit;

DWORD LcmPortInfo1Strings[] = { FIELD_OFFSET(PORT_INFO_1, pName),
						  (DWORD)-1 };

DWORD LcmPortInfo2Strings[] = { FIELD_OFFSET(PORT_INFO_2, pPortName),
						  FIELD_OFFSET(PORT_INFO_2, pMonitorName),
						  FIELD_OFFSET(PORT_INFO_2, pDescription),
						  (DWORD)-1 };

WCHAR g_szPortsKey[] = L"Ports";
WCHAR gszWindows[] = L"Software\\Microsoft\\Windows NT\\CurrentVersion\\Windows";
WCHAR szPortsEx[] = L"portsex"; /* Extra ports values */
WCHAR szNUL[] = L"NUL";
WCHAR szNUL_COLON[] = L"NUL:";
WCHAR szFILE[] = L"FILE:";
WCHAR szLcmCOM[] = L"COM";
WCHAR szLcmLPT[] = L"LPT";
WCHAR szIRDA[] = L"IR";
WCHAR szLPC[] = L"LPC:";

extern DWORD g_COMWriteTimeoutConstant_ms;

LPWSTR CurrentPrintName = NULL;
int CurrentJobIDGlobal = 0;
IPPRequestContext* GlobalIPPRequestContext = NULL;


BOOL
LocalMonInit(HINSTANCE hModule)
{

	LcmhInst = hModule;

	return InitializeCriticalSectionAndSpinCount(&LcmSpoolerSection, 0x80000000);
}


VOID
LocalMonCleanUp(
	VOID
)
{

	DeleteCriticalSection(&LcmSpoolerSection);
}

BOOL
LcmEnumPorts(
	__in                        HANDLE  hMonitor,
	__in_opt                    LPWSTR  pName,
	DWORD   Level,
	__out_bcount_opt(cbBuf)     LPBYTE  pPorts,
	DWORD   cbBuf,
	__out                       LPDWORD pcbNeeded,
	__out                       LPDWORD pcReturned
)
{
	PINILOCALMON    pIniLocalMon = (PINILOCALMON)hMonitor;
	PLCMINIPORT     pIniPort = NULL;
	DWORD           cb = 0;
	LPBYTE          pEnd = NULL;
	DWORD           LastError = 0;

	UNREFERENCED_PARAMETER(pName);


	LcmEnterSplSem();

	cb = 0;

	pIniPort = pIniLocalMon->pIniPort;

	CheckAndAddIrdaPort(pIniLocalMon);

	while (pIniPort) {

		if (!(pIniPort->Status & PP_FILEPORT)) {

			cb += GetPortSize(pIniPort, Level);
		}
		pIniPort = pIniPort->pNext;
	}

	*pcbNeeded = cb;

	if (cb <= cbBuf) {
		pEnd = pPorts + cbBuf;
		*pcReturned = 0;

		pIniPort = pIniLocalMon->pIniPort;
		while (pIniPort) {

			if (!(pIniPort->Status & PP_FILEPORT)) {

				pEnd = CopyIniPortToPort(pIniPort, Level, pPorts, pEnd);

				if (!pEnd) {
					LastError = GetLastError();
					break;
				}

				switch (Level) {
				case 1:
					pPorts += sizeof(PORT_INFO_1);
					break;
				case 2:
					pPorts += sizeof(PORT_INFO_2);
					break;
				default:
					LastError = ERROR_INVALID_LEVEL;
					goto Cleanup;
				}
				(*pcReturned)++;
			}
			pIniPort = pIniPort->pNext;
		}

	}
	else

		LastError = ERROR_INSUFFICIENT_BUFFER;

Cleanup:
	LcmLeaveSplSem();

	if (LastError) {

		SetLastError(LastError);
		return FALSE;

	}
	else

		return TRUE;
}

BOOL
LcmOpenPort(
	__in    HANDLE  hMonitor,
	__in    LPWSTR  pName,
	__out   PHANDLE pHandle
)
{
	PINILOCALMON    pIniLocalMon = (PINILOCALMON)hMonitor;
	PLCMINIPORT     pIniPort = NULL;
	BOOL            bRet = FALSE;


	LcmEnterSplSem();

	if (!pName)
	{
		SetLastError(ERROR_INVALID_PARAMETER);
		goto Done;
	}

	if (IS_FILE_PORT(pName)) {
		pIniPort = LcmCreatePortEntry(pIniLocalMon, pName);
		if (!pIniPort)
			goto Done;

		pIniPort->Status |= PP_FILEPORT;
		*pHandle = pIniPort;
		bRet = TRUE;
		goto Done;
	}

	bRet = TRUE;
	pIniPort = FindPort(pIniLocalMon, pName);

	if (!pIniPort)
		goto Done;

Done:
	if (!bRet && pIniPort && (pIniPort->Status & PP_FILEPORT))
		DeletePortNode(pIniLocalMon, pIniPort);

	if (bRet)
		*pHandle = pIniPort;

	LcmLeaveSplSem();
	return bRet;
}

BOOL
LcmStartDocPort(
	__in    HANDLE  hPort,
	__in    LPWSTR  pPrinterName,
	DWORD   JobId,
	DWORD   Level,
	__in    LPBYTE  pDocInfo)
{
	PLCMINIPORT pIniPort = (PLCMINIPORT)hPort;
	PDOC_INFO_1 pDocInfo1 = (PDOC_INFO_1)pDocInfo;
	DWORD       Error = 0;
	LPWSTR      pAdjustedName = NULL;

	UNREFERENCED_PARAMETER(Level);

	if (pIniPort->Status & PP_STARTDOC)
	{
		return TRUE;
	}

	AppendInfoLogToFile("LcmStartDocPort():LcmStartDocPort START");

	LcmEnterSplSem();
	{
		int nBufSize = wcslen(pPrinterName) * sizeof(WCHAR) + sizeof(WCHAR);
		CurrentPrintName = (LPWSTR)malloc(nBufSize);
		if (CurrentPrintName == NULL) {
			AppendErrorLogToFile("LcmStartDocPort():CurrentPrintName Malloc Error");
			return FALSE;
		}

		memset(CurrentPrintName, 0, nBufSize);

		memcpy(CurrentPrintName, pPrinterName, nBufSize - sizeof(WCHAR));
	}
	CurrentJobIDGlobal = JobId;
	pIniPort->Status |= PP_STARTDOC;
	LcmLeaveSplSem();

	pIniPort->hPrinter = NULL;
	pIniPort->pPrinterName = AllocSplStr(pPrinterName);

	if (pIniPort->pPrinterName)
	{

		if (OpenPrinter(pPrinterName, &pIniPort->hPrinter, NULL))
		{

			pIniPort->JobId = JobId;

			if (IS_FILE_PORT(pIniPort->pName))
			{

				HANDLE hFile = INVALID_HANDLE_VALUE;

				if (pDocInfo1 &&
					pDocInfo1->pOutputFile &&
					pDocInfo1->pOutputFile[0])
				{

					hFile = CreateFile(pDocInfo1->pOutputFile,
						GENERIC_WRITE,
						FILE_SHARE_READ | FILE_SHARE_WRITE,
						NULL,
						OPEN_ALWAYS,
						FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
						NULL);
				}
				else
				{
					Error = ERROR_INVALID_PARAMETER;
				}

				if (hFile != INVALID_HANDLE_VALUE)
				{
					SetEndOfFile(hFile);
				}
				pIniPort->hFile = hFile;
			}
			else if (IS_LPC_PORT(pIniPort->pName)) {
				HANDLE hFile = INVALID_HANDLE_VALUE;
				if (pIniPort->hFile != INVALID_HANDLE_VALUE) {
					CloseHandle(pIniPort->hFile);
				}

				DeleteFileW(PSFILE_OUTPUT_PATH);

				hFile = CreateFileW(PSFILE_OUTPUT_PATH,
					GENERIC_WRITE,
					FILE_SHARE_READ | FILE_SHARE_WRITE,
					NULL,
					OPEN_ALWAYS,
					FILE_ATTRIBUTE_NORMAL | FILE_FLAG_SEQUENTIAL_SCAN,
					NULL);

				pIniPort->hFile = hFile;
			}
		} else {
			AppendErrorLogToFile("LcmStartDocPort():OpenPrinter Faild");
			goto Fail;
		}
	} else {
		AppendErrorLogToFile("LcmStartDocPort():pIniPort->pPrinterName Malloc Error");
		goto Fail;
		
	}// end of if (pIniPort->pPrinterName)

	if (pIniPort->hFile == INVALID_HANDLE_VALUE)
	{
		AppendErrorLogToFile("LcmStartDocPort():Start Doc Create File Error");
		goto Fail;
	}

	return TRUE;


Fail:
	AppendErrorLogToFile("LcmStartDocPort():Start Doc Error");
	SPLASSERT(pIniPort->hFile == INVALID_HANDLE_VALUE);

	LcmEnterSplSem();
	pIniPort->Status &= ~PP_STARTDOC;
	CurrentJobIDGlobal = 0;
	LcmLeaveSplSem();

	if (pIniPort->hPrinter)
	{
		ClosePrinter(pIniPort->hPrinter);
	}

	if (pIniPort->pPrinterName)
	{
		FreeSplStr(pIniPort->pPrinterName);
	}

	if (Error)
	{
		SetLastError(Error);
	}
	return FALSE;
}


BOOL
LcmWritePort(
	__in                HANDLE  hPort,
	__in_bcount(cbBuf)  LPBYTE  pBuffer,
	DWORD   cbBuf,
	__out               LPDWORD pcbWritten)
{
	PLCMINIPORT pIniPort = (PLCMINIPORT)hPort;
	BOOL        rc = FALSE;


	if (!pIniPort->hFile || pIniPort->hFile == INVALID_HANDLE_VALUE)
	{
		AppendErrorLogToFile("Write Doc Data Error By INVALID_HANDLE_VALUE");
		SetLastError(ERROR_INVALID_HANDLE);
		return FALSE;
	}
	else
	{
		rc = WriteFile(pIniPort->hFile, pBuffer, cbBuf, pcbWritten, NULL);
		if (rc && *pcbWritten == 0)
		{
			AppendErrorLogToFile("Write Doc Data Error By pcbWritten == 0");
			SetLastError(ERROR_TIMEOUT);
			rc = FALSE;
		}
	}


	return rc;
}


BOOL
LcmReadPort(
	__in                HANDLE      hPort,
	__out_bcount(cbBuf) LPBYTE      pBuffer,
	DWORD       cbBuf,
	__out               LPDWORD     pcbRead)
{
	PLCMINIPORT pIniPort = (PLCMINIPORT)hPort;
	BOOL        rc = FALSE;

	SetLastError(ERROR_INVALID_HANDLE);
	AppendInfoLogToFile("Client Read Port");
	return rc;
}

BOOL
LcmEndDocPort(
	__in    HANDLE   hPort
)
{
	PLCMINIPORT    pIniPort = (PLCMINIPORT)hPort;
	IPPRequestContext* req_ctx = NULL;
	if (!(pIniPort->Status & PP_STARTDOC))
	{
		char buf[TEMP_SIZE];
		memset(buf, 0, sizeof(buf));
		sprintf_s(buf, sizeof(buf), "PIniPort->status = %d", pIniPort->Status);
		AppendErrorLogToFile("LcmEndDocPort status Error");
		AppendErrorLogToFile((const char*)buf);

		if (CurrentPrintName != NULL) {
			free(CurrentPrintName);
			CurrentPrintName = NULL;
		}
		return TRUE;
	}

	// The flush here is done to make sure any cached IO's get written
	// before the handle is closed.   This is particularly a problem
	// for Intelligent buffered serial devices

	FlushFileBuffers(pIniPort->hFile);

	CloseHandle(pIniPort->hFile);
	pIniPort->hFile = INVALID_HANDLE_VALUE;

	SetJob(pIniPort->hPrinter, pIniPort->JobId, 0, NULL, JOB_CONTROL_SENT_TO_PRINTER);

	ClosePrinter(pIniPort->hPrinter);

	FreeSplStr(pIniPort->pPrinterName);



	//
	// Startdoc no longer active.
	//
	pIniPort->Status &= ~PP_STARTDOC;

	{
		HANDLE hPrinter;
		PRINTER_DEFAULTS printerDefaults = { NULL, NULL, PRINTER_ACCESS_ADMINISTER };
		DWORD cbNeeded;
		PRINTER_INFO_2* pPrinterInfo = NULL;
		DEVMODE* pDevMode = NULL;
		int copies = 1;
		int mopies = 0;
		char* pagesizestr = NULL;
		char duplexstr[MAX_PATH + 1] = { 0 };
		char colormodestr[MAX_PATH + 1] = { 0 };
		char* mediatypestr = NULL;
		JOB_INFO_2W* jobInfo = NULL;
		DWORD bytesNeeded;
		PrinterSettings *settings = NULL;
		char* cszJobName = NULL;//打印作业的名字
		char* printer_name = NULL;

		memset(duplexstr, 0, sizeof(duplexstr));
		memset(colormodestr, 0, sizeof(colormodestr));

		if (!OpenPrinter(CurrentPrintName, &hPrinter, NULL)) {
			if (CurrentPrintName != NULL) {
				free(CurrentPrintName);
				CurrentPrintName = NULL;
			}
			AppendErrorLogToFile("OpenPrinter CommonDriver PS Error\n");
			return TRUE;
		}

		if (CurrentPrintName != NULL) {
			free(CurrentPrintName);
			CurrentPrintName = NULL;
		}

		// 获取所需的缓冲区大小
		if (!GetJob(hPrinter, CurrentJobIDGlobal, 2, NULL, 0, &bytesNeeded) && GetLastError() != ERROR_INSUFFICIENT_BUFFER)
		{
			AppendErrorLogToFile("GetJob Info Error");
			ClosePrinter(hPrinter);
			return TRUE;
		}

		// 分配缓冲区并获取打印作业信息
		jobInfo = (JOB_INFO_2W*)malloc(bytesNeeded);
		if (jobInfo == NULL) {
			ClosePrinter(hPrinter);
			AppendErrorLogToFile("Malloc JOB_INFO_2W Error");
			return TRUE;
		}

		if (!GetJob(hPrinter, CurrentJobIDGlobal, 2, (LPBYTE)jobInfo, bytesNeeded, &bytesNeeded))
		{
			if (jobInfo != NULL) {
				free(jobInfo);
				jobInfo = NULL;
			}
			ClosePrinter(hPrinter);
			return TRUE;
		}

		if (jobInfo == NULL) {
			AppendErrorLogToFile("Get JOB_INFO_2W Error");
			ClosePrinter(hPrinter);
			return TRUE;
		}

		if (jobInfo->pDocument != NULL) {
			int bufferSize = WideCharToMultiByte(CP_UTF8, 0, jobInfo->pDocument, -1, NULL, 0, NULL, NULL);

			cszJobName = (char*)malloc(bufferSize + 1);
			if (cszJobName != NULL) {
				memset(cszJobName, 0, bufferSize + 1);
				// 进行转换
				WideCharToMultiByte(CP_UTF8, 0, jobInfo->pDocument, -1, cszJobName, bufferSize, NULL, NULL);
			}
		}

		// 获取DEVMODE结构体
		pDevMode = jobInfo->pDevMode;
		if (pDevMode == NULL) {
			if (jobInfo != NULL) {
				free(jobInfo);
				jobInfo = NULL;
			}

			if (cszJobName != NULL) {
				free(cszJobName);
				cszJobName = NULL;
			}

			ClosePrinter(hPrinter);
			return TRUE;
		}
		settings = (PrinterSettings*)malloc(sizeof(PrinterSettings));
		if (getInfoFromPsFile(PS_FILE_PATH, settings) == 1) {
			copies = settings->numCopies; // 拷贝数
			mopies = settings->collate; // 逐步印刷
			//GetPagesizestr(pDevMode, &pagesizestr); 
			pagesizestr = settings->pageSize;// 纸型
			mediatypestr = settings->mediaType; // 媒体种类
		}
		else {
			freePrinterSettings(settings);
			copies = 1; 
			mopies = TRUE;
			pagesizestr = NULL; // 打印机默认纸型
			mediatypestr = NULL;  // 打印机默认媒体种类
		}

		Getduplexstr(pDevMode,duplexstr,sizeof(duplexstr)); // 单面印刷/两面印刷
		Getcolormodestr(pDevMode,colormodestr,sizeof(colormodestr));
		printer_name= WStrToChar(jobInfo->pPrinterName);
		AppendInfoLogToFile("Start::Create New IPP Request Context\n");
		req_ctx = CreateNewIPPRequestContext(printer_name);
		if (printer_name != NULL) {
			free(printer_name);
			printer_name = NULL;
		}
		if (req_ctx == NULL || req_ctx->printer_name == NULL || req_ctx->ip_addr == NULL || req_ctx->port == NULL || req_ctx->server_host == NULL) {
			AppendErrorLogToFile("Create New IPP Request Context Error\n");
			freeIPPRequestContext(req_ctx);
			remove(PS_FILE_PATH);
			return TRUE;
		}

		AppendInfoLogToFile("SUCCESS::Starting Connect Cups Server\n");
		if (ConnectToCupsPrinterServer(req_ctx) == FALSE) {
			AppendErrorLogToFile("Connect Cups Server Error\n");
			freeIPPRequestContext(req_ctx);
			remove(PS_FILE_PATH);
			return TRUE;
		}
		AppendInfoLogToFile("SUCCESS::Starting Connect Cups Server Success\n");
		

		AppendInfoLogToFile("SUCCESS::Starting Send PDF Data To CupsServer \n");
		if (SendIPPPrintJobPacketBySocket(req_ctx, PS_FILE_PATH, pagesizestr, NULL, duplexstr, copies, mopies, colormodestr, cszJobName) == FALSE) {
			AppendErrorLogToFile("Starting Send PDF Data To CupsServer Error\n");
			CloseIPPRequestContextConnection(req_ctx);
			freePrinterSettings(settings);
			if (jobInfo != NULL) {
				free(jobInfo);
				jobInfo = NULL;
			}
			ClosePrinter(hPrinter);

			hPrinter = INVALID_HANDLE_VALUE;

			if (cszJobName != NULL) {
				free(cszJobName);
				cszJobName = NULL;
			}
			return TRUE;
		}
		AppendInfoLogToFile("SUCCESS::Starting Send PDF Data To CupsServer Success\n");
		CloseIPPRequestContextConnection(req_ctx);
		freePrinterSettings(settings);

		if (jobInfo != NULL) {
			free(jobInfo);
			jobInfo = NULL;
		}
		if (cszJobName != NULL) {
			free(cszJobName);
			cszJobName = NULL;
		}
		ClosePrinter(hPrinter);
		hPrinter = INVALID_HANDLE_VALUE;
	}

	return TRUE;
}

BOOL
LcmClosePort(
	__in    HANDLE  hPort
)
{
	PLCMINIPORT pIniPort = (PLCMINIPORT)hPort;

	AppendInfoLogToFile("Close Monitor Port");
	FreeSplStr(pIniPort->pDeviceName);
	pIniPort->pDeviceName = NULL;

	if (pIniPort->Status & PP_FILEPORT) {

		LcmEnterSplSem();
		DeletePortNode(pIniPort->pIniLocalMon, pIniPort);
		LcmLeaveSplSem();
	}
	else if (pIniPort->Status & PP_COMM_PORT) {

		(VOID)RemoveDosDeviceDefinition(pIniPort);
		if (pIniPort->hFile != INVALID_HANDLE_VALUE) {

			// no COM ports should hit this path
			SPLASSERT(!IS_COM_PORT(pIniPort->pName));


			CloseHandle(pIniPort->hFile);
			pIniPort->hFile = INVALID_HANDLE_VALUE;
		}
		pIniPort->Status &= ~(PP_COMM_PORT | PP_DOSDEVPORT);
	}

	return TRUE;
}


BOOL
LcmAddPortEx(
	__in        HANDLE   hMonitor,
	__in_opt    LPWSTR   pName,
	DWORD    Level,
	__in        LPBYTE   pBuffer,
	__in_opt    LPWSTR   pMonitorName
)
{
	PINILOCALMON    pIniLocalMon = (PINILOCALMON)hMonitor;
	LPWSTR          pPortName = NULL;
	DWORD           Error = NO_ERROR;
	LPPORT_INFO_1   pPortInfo1 = NULL;
	LPPORT_INFO_FF  pPortInfoFF = NULL;

	UNREFERENCED_PARAMETER(pMonitorName);

	AppendInfoLogToFile("Start Add Monitor Port");
	switch (Level) {
	case (DWORD)-1:
		pPortInfoFF = (LPPORT_INFO_FF)pBuffer;
		pPortName = pPortInfoFF->pName;
		break;

	case 1:
		pPortInfo1 = (LPPORT_INFO_1)pBuffer;
		pPortName = pPortInfo1->pName;
		break;

	default:
		AppendErrorLogToFile("Add Monitor Port Error By Level");
		SetLastError(ERROR_INVALID_LEVEL);
		return(FALSE);
	}
	if (!pPortName) {
		AppendErrorLogToFile("Add Monitor Port Error By Invalid PortName");
		SetLastError(ERROR_INVALID_PARAMETER);
		return(FALSE);
	}
	if (PortExists(pName, pPortName, &Error)) {
		AppendErrorLogToFile("Add Monitor Port Error By Invalid PortName");
		SetLastError(ERROR_INVALID_PARAMETER);
		return(FALSE);
	}
	if (Error != NO_ERROR) {
		SetLastError(Error);
		return(FALSE);
	}
	if (!LcmCreatePortEntry(pIniLocalMon, pPortName)) {
		AppendErrorLogToFile("Add Monitor Port Error By Create Port Entry");
		return(FALSE);
	}

	if (!AddPortInRegistry(pPortName)) {
		AppendErrorLogToFile("Add Monitor Port Error By Create Port Entry In Registry");
		LcmDeletePortEntry(pIniLocalMon, pPortName);
		return(FALSE);
	}

	AppendInfoLogToFile("Add Monitor Port Success");
	return TRUE;
}

BOOL
LcmGetPrinterDataFromPort(
	__in                            HANDLE  hPort,
	DWORD   ControlID,
	__in_opt                        LPWSTR  pValueName,
	__in_bcount_opt(cbInBuffer)     LPWSTR  lpInBuffer,
	DWORD   cbInBuffer,
	__out_bcount_opt(cbOutBuffer)   LPWSTR  lpOutBuffer,
	DWORD   cbOutBuffer,
	__out                           LPDWORD lpcbReturned)
{
	PLCMINIPORT  pIniPort = (PLCMINIPORT)hPort;
	BOOL         rc = FALSE;

	UNREFERENCED_PARAMETER(pValueName);

	AppendInfoLogToFile("Start Get PrinterData From Port");
	if (ControlID &&
		(pIniPort->Status & PP_DOSDEVPORT) &&
		IS_COM_PORT(pIniPort->pName))
	{
		if (GetCOMPort(pIniPort))
		{
			rc = DeviceIoControl(pIniPort->hFile,
				ControlID,
				lpInBuffer,
				cbInBuffer,
				lpOutBuffer,
				cbOutBuffer,
				lpcbReturned,
				NULL);

			ReleaseCOMPort(pIniPort);
		}
	}
	else if (ControlID &&
		pIniPort->hFile &&
		(pIniPort->hFile != INVALID_HANDLE_VALUE) &&
		(pIniPort->Status & PP_DOSDEVPORT))
	{
		rc = DeviceIoControl(pIniPort->hFile,
			ControlID,
			lpInBuffer,
			cbInBuffer,
			lpOutBuffer,
			cbOutBuffer,
			lpcbReturned,
			NULL);
	}
	else
	{
		AppendErrorLogToFile("Get PrinterData From Port Error");
		SetLastError(ERROR_INVALID_PARAMETER);
	}

	return rc;
}

BOOL
LcmSetPortTimeOuts(
	__in        HANDLE          hPort,
	__in        LPCOMMTIMEOUTS  lpCTO,
	__reserved  DWORD           reserved)    // must be set to 0
{
	PLCMINIPORT     pIniPort = (PLCMINIPORT)hPort;
	COMMTIMEOUTS    cto = { 0 };
	BOOL            rc = FALSE;

	AppendInfoLogToFile("Start Set Port TimeOuts");
	if (reserved != 0)
	{
		AppendErrorLogToFile("Set Port TimeOuts Error By reserved != 0");
		SetLastError(ERROR_INVALID_PARAMETER);
		goto done;
	}

	if (!(pIniPort->Status & PP_DOSDEVPORT))
	{
		AppendErrorLogToFile("Set Port TimeOuts Error By Status & PP_DOSDEVPORT");
		SetLastError(ERROR_INVALID_PARAMETER);
		goto done;
	}

	if (IS_COM_PORT(pIniPort->pName))
	{
		GetCOMPort(pIniPort);
	}

	if (GetCommTimeouts(pIniPort->hFile, &cto))
	{
		cto.ReadTotalTimeoutConstant = lpCTO->ReadTotalTimeoutConstant;
		cto.ReadIntervalTimeout = lpCTO->ReadIntervalTimeout;
		rc = SetCommTimeouts(pIniPort->hFile, &cto);
	}

	if (IS_COM_PORT(pIniPort->pName))
	{
		ReleaseCOMPort(pIniPort);
	}

done:
	return rc;
}

VOID
LcmShutdown(
	__in    HANDLE hMonitor
)
{
	PLCMINIPORT     pIniPort = NULL;
	PLCMINIPORT     pIniPortNext = NULL;
	PINILOCALMON    pIniLocalMon = (PINILOCALMON)hMonitor;

	AppendInfoLogToFile("Port Monitor Shutdown");
	//
	// Delete the ports, then delete the LOCALMONITOR.
	//
	for (pIniPort = pIniLocalMon->pIniPort; pIniPort; pIniPort = pIniPortNext) {
		pIniPortNext = pIniPort->pNext;
		FreeSplMem(pIniPort);
	}

	FreeSplMem(pIniLocalMon);

	pIniLocalMon = NULL;
}

DWORD
GetPortStrings(
	__out   PWSTR* ppPorts
)
{
	DWORD   sRetval = ERROR_INVALID_PARAMETER;


	if (ppPorts)
	{
		DWORD dwcValues = 0;
		DWORD dwMaxValueName = 0;
		HKEY  hk = NULL;

		//
		// open ports key
		//
		sRetval = g_pMonitorInit->pMonitorReg->fpCreateKey(g_pMonitorInit->hckRegistryRoot,
			g_szPortsKey,
			REG_OPTION_NON_VOLATILE,
			KEY_READ,
			NULL,
			&hk,
			NULL,
			g_pMonitorInit->hSpooler);
		if (sRetval == ERROR_SUCCESS && hk)
		{
			sRetval = g_pMonitorInit->pMonitorReg->fpQueryInfoKey(hk,
				NULL,
				NULL,
				&dwcValues,
				&dwMaxValueName,
				NULL,
				NULL,
				NULL,
				g_pMonitorInit->hSpooler);
			if ((sRetval == ERROR_SUCCESS) && (dwcValues > 0))
			{
				PWSTR pPorts = NULL;
				DWORD cbMaxMemNeeded = ((dwcValues * (dwMaxValueName + 1) + 1) * sizeof(WCHAR));

				pPorts = (LPWSTR)AllocSplMem(cbMaxMemNeeded);

				if (pPorts)
				{
					DWORD sTempRetval = ERROR_SUCCESS;
					DWORD CharsAvail = cbMaxMemNeeded / sizeof(WCHAR);
					INT   cIndex = 0;
					PWSTR pPort = NULL;
					DWORD dwCurLen = 0;

					for (pPort = pPorts; sTempRetval == ERROR_SUCCESS; cIndex++)
					{
						dwCurLen = CharsAvail;
						sTempRetval = g_pMonitorInit->pMonitorReg->fpEnumValue(hk,
							cIndex,
							pPort,
							&dwCurLen,
							NULL,
							NULL,
							NULL,
							g_pMonitorInit->hSpooler);
						// based on the results of current length,
						// move pointers/counters for the next iteration.
						if (sTempRetval == ERROR_SUCCESS)
						{
							// RegEnumValue only returns the char count.
							// Add 1 for NULL.
							dwCurLen++;

							// decrement the count of available chars.
							CharsAvail -= dwCurLen;

							// prepare pPort for next string.
							pPort += dwCurLen;
						}

					}

					if (sTempRetval == ERROR_NO_MORE_ITEMS)
					{
						*pPort = L'\0';
						*ppPorts = pPorts;
					}
					else
					{
						// set return value in error case.
						sRetval = sTempRetval;
					}
				}
				else
				{
					sRetval = GetLastError();
				}
			}

			// close Reg key.
			g_pMonitorInit->pMonitorReg->fpCloseKey(hk,
				g_pMonitorInit->hSpooler);
		}
	}

	return sRetval;
}


MONITOR2 Monitor2 = {
	sizeof(MONITOR2),
	LcmEnumPorts,
	LcmOpenPort,
	NULL,           // OpenPortEx is not supported
	LcmStartDocPort,
	LcmWritePort,
	LcmReadPort,
	LcmEndDocPort,
	LcmClosePort,
	NULL,           // AddPort is not supported
	LcmAddPortEx,
	NULL,           // ConfigurePort is not supported
	NULL,           // DeletePort is not supported
	LcmGetPrinterDataFromPort,
	LcmSetPortTimeOuts,
	LcmXcvOpenPort,
	LcmXcvDataPort,
	LcmXcvClosePort,
	LcmShutdown
};


LPMONITOR2
InitializePrintMonitor2(
	__in    PMONITORINIT pMonitorInit,
	__out   PHANDLE phMonitor
)
{
	LPWSTR   pPortTmp = NULL;
	DWORD    rc = 0, i = 0;
	PINILOCALMON pIniLocalMon = NULL;
	LPWSTR   pPorts = NULL;
	DWORD    sRetval = ERROR_SUCCESS;

	AppendInfoLogToFile("InitializePrintMonitor");
	// cache pointer
	pIniLocalMon = (PINILOCALMON)AllocSplMem(sizeof(INILOCALMON));
	if (!pIniLocalMon)
	{
		AppendErrorLogToFile("AllocSplMem Error");
		goto Fail;
	}

	pIniLocalMon->signature = ILM_SIGNATURE;
	pIniLocalMon->pMonitorInit = pMonitorInit;
	g_pMonitorInit = pMonitorInit;

	// get ports
	sRetval = GetPortStrings(&pPorts);
	if (sRetval != ERROR_SUCCESS)
	{
		AppendErrorLogToFile("GetPortStrings Error");
		SetLastError(sRetval);
		goto Fail;
	}


	LcmEnterSplSem();

	//
	// We now have all the ports
	//
	for (pPortTmp = pPorts; pPortTmp && *pPortTmp; pPortTmp += rc + 1) {

		rc = (DWORD)wcslen(pPortTmp);

		if (!_wcsnicmp(pPortTmp, L"Ne", 2)) {

			i = 2;

			//
			// For Ne- ports
			//
			if (rc > 2 && pPortTmp[2] == L'-')
				++i;
			for (; i < rc - 1 && iswdigit(pPortTmp[i]); ++i)
				;

			if (i == rc - 1 && pPortTmp[rc - 1] == L':') {
				continue;
			}
		}

		LcmCreatePortEntry(pIniLocalMon, pPortTmp);
	}

	FreeSplMem(pPorts);

	LcmLeaveSplSem();

	CheckAndAddIrdaPort(pIniLocalMon);

	*phMonitor = (HANDLE)pIniLocalMon;


	return &Monitor2;

Fail:

	FreeSplMem(pPorts);
	FreeSplMem(pIniLocalMon);

	return NULL;
}



BOOL
DllMain(
	HINSTANCE hModule,
	DWORD dwReason,
	LPVOID lpRes)
{
	static BOOL bLocalMonInit = FALSE;

	switch (dwReason)
	{
	case DLL_PROCESS_ATTACH:
		AppendInfoLogToFile("DllMain():DLL_PROCESS_ATTACH");
		bLocalMonInit = LocalMonInit(hModule);
		if(bLocalMonInit) {
		    DisableThreadLibraryCalls(hModule);
			AppendInfoLogToFile("DllMain():DLL_PROCESS_ATTACH LocalMonInit OK");
		    return TRUE; 
		} else {
			AppendInfoLogToFile("DllMain():DLL_PROCESS_ATTACH LocalMonInit NG");
			return FALSE;
		}


	case DLL_PROCESS_DETACH:
		AppendInfoLogToFile("DllMain():DLL_PROCESS_DETACH");
		if (bLocalMonInit)
		{
			LocalMonCleanUp();
			bLocalMonInit = FALSE;
		}

		return TRUE;
	}

	UNREFERENCED_PARAMETER(lpRes);

	return TRUE;
}

int readConfig(const char* filename) {
	FILE* fp = NULL;
	char* line = NULL;
	configCount = 0;
	fp = fopen(filename, "r");
	if (!fp) {
		AppendErrorLogToFile("ReadConfig():File Open Failed");
		return -1;
	}
	
	line = (char*)malloc(MAX_VALUE_LENGTH);
	if (line == NULL) {
		AppendErrorLogToFile("readConfig():Memory Allocation Failed");
		fclose(fp);
		return -1;
	}

	while (fgets(line, MAX_VALUE_LENGTH, fp) && (configCount < MAX_CONFIG_LINES)) {
		if (sscanf(line, "%[^=]=%[^\r\n]", config[configCount].key, config[configCount].value) == 2) {
			config[configCount].key[strcspn(config[configCount].key, "\n")] = 0;
			config[configCount].value[strcspn(config[configCount].value, "\n")] = 0;
			configCount++;
		}
	}
	free(line);
	line = NULL;
	fclose(fp);
	return 1;
}

IPPRequestContext* CreateNewIPPRequestContext(char* printerName) {
	
	int i = 0;
	int idex = 0;
	char log_message[512];
	IPPRequestContext* req = NULL;
	char* uri_cp = NULL;
	memset(log_message,0x00,sizeof(log_message));
	
	
	req = (IPPRequestContext*)malloc(sizeof(IPPRequestContext));
	uri_cp = (char*)malloc(MAX_VALUE_LENGTH);

    //内存申请失败，返回NULL
	if((req == NULL) || (uri_cp == NULL)){
		free(req);
		free(uri_cp);
		AppendErrorLogToFile("CreateNewIPPRequestContext(): req or uri_cp malloc Failed\n");
		return NULL;
	}

	//内存申请失败，返回NULL	
	req->printer_name = (char*)malloc(MAX_VALUE_LENGTH);
	if (req->printer_name == NULL) {
			free(req);
			free(uri_cp);
			return NULL;
	}


	if (readConfig(CONFIG_FILE_PATH) == 1) {
		AppendInfoLogToFile("CreateNewIPPRequestContext():Start:Find Target Printer Uri\n");
		for (; i < configCount; i++) {
			// 查看打印机目标的uri
			if (printerName != NULL && strncmp(printerName,config[i].key,strlen(printerName)) == 0) {
				idex = i;
				break;
			}
		}
			
		// 如果未找到uri，默认使用配置文件中第一个uri
		if (config[idex].value != NULL && strlen(config[idex].value) < MAX_VALUE_LENGTH - 1) {
			// copy : config[0].value -> req->printer_name
			strncpy(req->printer_name, config[idex].value, strlen(config[idex].value));
			req->printer_name[strlen(config[idex].value)] = '\0';
			// split: uri_cp is use by get ipaddress,port and host
			strncpy(uri_cp, req->printer_name, strlen(req->printer_name));
			uri_cp[strlen(req->printer_name)] = '\0';

			AppendInfoLogToFile("CreateNewIPPRequestContext():Split ipAddress,Port,host form uri start\n");
			if (getIpPortAndHost(uri_cp, req) == 1) {
				AppendInfoLogToFile("CreateNewIPPRequestContext():Split ipAddress/Port/host form uri Success\n");
			} else {
				//读取配置文件，没有找到IP地址或者端口，释放req,赋值为NULL		
				free(req->printer_name);
				req->printer_name = NULL;
				free(req);
				req = NULL;
				AppendErrorLogToFile("CreateNewIPPRequestContext():Split ipAddress/Port/host form uri Failed\n");
			}
			sprintf_s(log_message,sizeof(log_message),"CreateNewIPPRequestContext():PrinterName=%s,ServerHost=%s, ip_addr=%s,port=%s\n",
																	req->printer_name,req->server_host,req->ip_addr,req->port);
			AppendInfoLogToFile(log_message);
		} else {
			//读取配置文件，没有找到URI，释放req,赋值为NULL
			free(req->printer_name);
			req->printer_name = NULL;
			free(req);
			req = NULL;
			AppendErrorLogToFile("CreateNewIPPRequestContext():Printer Config Info is't Correct\n");

		}
	//读取配置文件失败，释放req,赋值为NULL
	} else {
		
		free(req->printer_name);
		req->printer_name = NULL;
		free(req);
		req = NULL;
		AppendErrorLogToFile("CreateNewIPPRequestContext():Read Config File Failed\n");
	}
	
	free(uri_cp);
	uri_cp = NULL;

	return req;
}

DynamicArray* createDynamicArray() {

    DynamicArray* da = (DynamicArray*)malloc(sizeof(DynamicArray));
    if (da == NULL) {
        AppendErrorLogToFile("createDynamicArray(): da malloc Failed\n");
        return NULL;
    }
    da->capacity = 16;
    da->data = (char*)malloc(da->capacity * sizeof(char));
    if (da->data == NULL) {
        AppendErrorLogToFile("createDynamicArray(): da->data malloc Failed\n");
        free(da);
        return NULL;
    }

    da->size = 0;
    return da;
}

void destroyDynamicArray(DynamicArray* da) {
    if (da != NULL) {
        free(da->data);
		da->data = NULL;
        free(da);
		da = NULL;
    }
}

void addDynamicArrayElement(DynamicArray* da, char element) {
	if (da == NULL) {
		AppendErrorLogToFile("addDynamicArrayElement():Received NULL DynamicArray");
		return;
	}

	if (da->size == da->capacity) {
		size_t new_capacity = da->capacity * 2;
		char* new_data  = (char*)realloc(da->data, new_capacity * sizeof(char));
		if (new_data == NULL) {
			AppendErrorLogToFile("addDynamicArrayElement():Realloc Memory Error");
			return;
		} else {
			da->data = new_data;
			da->capacity = new_capacity;
		}
	}

	da->data[da->size] = element;
	da->size++;
}

bool ConnectToCupsPrinterServer(IPPRequestContext* req) {
	WSADATA wsaData;
	int i_result = 1;
	struct sockaddr_in server;

	if (req == NULL) {
		return FALSE;
	}

	i_result = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (i_result != 0) {
		AppendErrorLogToFile("ConnectToCupsPrinterServer():Client WSAStartup Error");
		WSACleanup();
		return FALSE;
	}

	req->connection_fd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (req->connection_fd == INVALID_SOCKET) {
		AppendErrorLogToFile("ConnectToCupsPrinterServer():Client Create Socket Error");
		WSACleanup();
		return FALSE;
	}

	memset(&server, 0, sizeof(server));
	server.sin_family = AF_INET;
	server.sin_port = htons((USHORT)atoi(req->port));
	if (inet_pton(AF_INET, (const char*)req->ip_addr, &(server.sin_addr)) <= 0) {
		closesocket(req->connection_fd);
		req->connection_fd = INVALID_SOCKET;
		WSACleanup();
		AppendErrorLogToFile("ConnectToCupsPrinterServer():Cups IPPort Translate Error");
		return FALSE;
	}

	i_result = connect(req->connection_fd, (SOCKADDR*)&server, sizeof(server));
	if (i_result == SOCKET_ERROR) {
		AppendErrorLogToFile("ConnectToCupsPrinterServer():Cups Server Connect Error");
		closesocket(req->connection_fd);
		req->connection_fd = INVALID_SOCKET;
		WSACleanup();
		return FALSE;
	}

	return TRUE;
}


void MyItoA(int num, char* buff, int buff_size) {
	int i = 0, j = 0;
	char* temp_buf = buff;

	if (buff == NULL || buff_size <= 0 || num < 0 ) {
		return;
	}

	if (num == 0) {
        if (buff_size > 1) {
            buff[i++] = '0';
            buff[i] = '\0';
        }
        return;
    }

	while (num > 0 && buff_size > 0) {
		char n = (char)(num % 10);
		*buff++ = n + '0';
		i++;
		num /= 10;
		buff_size--;
	}

	*buff = '\0';

	for (j = 0; j < (i / 2); j++) {
		char temp = temp_buf[j];
		temp_buf[j] = temp_buf[i - j - 1];
		temp_buf[i - j - 1] = temp;
	}
}



bool SendIPPPacketToCupsPrinterServer(IPPRequestContext* req, char* request_body, int request_body_size) {
	int i_result = 0;
	char* request_body_size_str = (char*)malloc(TEMP_SIZE);
	char* request_header = (char*)malloc(BUFF_SIZE);
	int requestLength = 0;
	char buffer[1024] = { 0 };
	int bytesReceived = 0;
	if ((request_body_size_str == NULL) ||(request_header == NULL)) {
		free(request_body_size_str);
		request_body_size_str = NULL;
		free(request_header);
		request_header = NULL;
		AppendErrorLogToFile("SendIPPPacketToCupsPrinterServer():malloc memery Faild");
		return FALSE;
	}
	memset(request_body_size_str, 0, sizeof(request_body_size_str));
	memset(request_header, 0, sizeof(request_header));
	
	MyItoA(request_body_size, (char*)request_body_size_str, TEMP_SIZE);

	strncat(request_header, (const char*)"POST ", TEMP_SIZE);
	strncat(request_header, req->printer_name, TEMP_SIZE);
	strncat(request_header, " HTTP/1.1\r\n", TEMP_SIZE);
	strncat(request_header, "Content-Type: application/ipp\r\n", TEMP_SIZE);
	strncat(request_header, "Host: ", TEMP_SIZE);
	strncat(request_header, req->server_host, TEMP_SIZE);
	strncat(request_header, "\r\n", TEMP_SIZE);
	strncat(request_header, "User-Agent: Internet Print Provider\r\n", TEMP_SIZE);
	strncat(request_header, "Content-Length: ", TEMP_SIZE);
	strncat(request_header, request_body_size_str, TEMP_SIZE);
	strncat(request_header, "\r\n", TEMP_SIZE);
	strncat(request_header, "Connection: Keep-Alive\r\n\r\n", TEMP_SIZE);

	requestLength = strlen(request_header);

	i_result = send(req->connection_fd, request_header, requestLength, 0);
	if (i_result == SOCKET_ERROR) {
		AppendErrorLogToFile("SendIPPPacketToCupsPrinterServer():Send IPP Header Data To CupsPrinter Server By Socket Error");
		free(request_body_size_str);
		request_body_size_str = NULL;
		free(request_header);
		request_header = NULL;
		return FALSE;
	}

	i_result = send(req->connection_fd, request_body, request_body_size, 0);
	if (i_result == SOCKET_ERROR) {
		AppendErrorLogToFile("SendIPPPacketToCupsPrinterServer():Send IPP Body Data To CupsPrinter Server By Socket Error");
		free(request_body_size_str);
		request_body_size_str = NULL;
		free(request_header);
		request_header = NULL;
		return FALSE;
	}

	bytesReceived = recv(req->connection_fd, buffer, sizeof(buffer) - 1, 0);
	if (bytesReceived > 0) {
		AppendInfoLogToFile("SendIPPPacketToCupsPrinterServer():Recv Response Data From CupsPrinter Server Success");
		free(request_body_size_str);
		request_body_size_str = NULL;
		free(request_header);
		request_header = NULL;
		return TRUE;
	}
	AppendErrorLogToFile("SendIPPPacketToCupsPrinterServer():Recv Response Data From CupsPrinter Server Error");
	free(request_body_size_str);
	request_body_size_str = NULL;
	free(request_header);
	request_header = NULL;
	return FALSE;
}

bool SendIPPPrintJobPacketBySocket(IPPRequestContext* req_context, char* file_path, char* pageSize, char* mediaType, char* duplex, int copies,
	int mopies, char* colorMode,char* job_name) {
	DynamicArray* request_body_packet = createDynamicArray();
	int i = 0;
	char request_body[] = {
		0x01, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00, 0x02,
		0x01, 0x47, 0x00, 0x12, 'a', 't', 't', 'r', 'i', 'b', 'u', 't', 'e', 's', '-',
		'c', 'h', 'a', 'r', 's', 'e', 't', 0x00, 0x05, 'u', 't', 'f', '-', '8', 'H',
		0x00, 0x1b, 'a', 't', 't', 'r', 'i', 'b', 'u', 't', 'e', 's', '-', 'n', 'a',
		't', 'u', 'r', 'a', 'l', '-', 'l', 'a', 'n', 'g', 'u', 'a', 'g', 'e', 0x00,
		0x05, 'e', 'n', '-', 'u', 's'
	};
	char temp[] = { 0x0b, 'p', 'r', 'i', 'n', 't', 'e', 'r', '-','u','r','i', 0x00 };
	char temp2[] = { 0x08, 'j', 'o', 'b', '-', 'n', 'a', 'm', 'e', 0x00 };
	// job name   
	// requesting-user-name
	DWORD name_size = MAX_CONFIG_LINES;
	WCHAR username[MAX_CONFIG_LINES + 1] = { 0 };
	char* pdf_path = NULL;
	FILE* pdf_fp = NULL;
	int size = 0;
	char* pdf_data = NULL;
	char temp3[] = { 0x14, 'r', 'e', 'q', 'u', 'e', 's', 't', 'i','n','g','-','u','s','e','r','-','n','a','m','e',0x00 };
	char copy[] = { 0x21,0x00,0x06,'c', 'o', 'p', 'i', 'e', 's', 0x00 };
	char collate[] = { 0x22, 0x00, 0x07,'C', 'o', 'l', 'l', 'a', 't', 'e', 0x00, 0x01 }; // Mopies
	char page_size[] = { 0x42,0x00,0x08,'P','a','g','e','S','i','z','e',0x00 };
	char name_duplex[] = { 0x42,0x00,0x06,'D','u','p','l','e','x',0x00 };
	//char color_mode[] = { 0x44,0x00,0x10,'p','r','i','n','t','-','c','o','l','o','r','-','m','o','d','e',0x00 };
	char color_mode[] = { 0x42,0x00,0x0a,'C','o','l','o','r','M','o','d','e','l',0x00 };
	char media_type[] = { 0x44,0x00,0x0a,'m','e','d','i','a','-','t','y','p','e',0x00 };
	unsigned char* p = NULL;
	size_t int_size = sizeof(int);
	size_t y = 0;
	if (request_body_packet == NULL) {
		AppendErrorLogToFile("SendIPPPrintJobPacketBySocket():request_body_packet malloc Failed");
		return FALSE;
	}


	// 初始化username缓冲区,默认值为Unknown
	wcscpy_s(username, sizeof(username) / sizeof(WCHAR), L"Unknown");
	GetUserNameW(username, &name_size); // 获取登录的用户名

	for (; i < sizeof(request_body); i++) {
		addDynamicArrayElement(request_body_packet, request_body[i]);
	}

	// printer-uri  
	addDynamicArrayElement(request_body_packet, 0x45);
	addDynamicArrayElement(request_body_packet, 0x00);


	for (i = 0; i < sizeof(temp); i++) {
		addDynamicArrayElement(request_body_packet, temp[i]);
	}

	addDynamicArrayElement(request_body_packet, (char)strlen(req_context->printer_name));

	for (i = 0; i < strlen(req_context->printer_name); i++) {
		addDynamicArrayElement(request_body_packet, req_context->printer_name[i]);
	}

	addDynamicArrayElement(request_body_packet, 0x42);
	addDynamicArrayElement(request_body_packet, 0x00);

	for (i = 0; i < sizeof(temp2); i++) {
		addDynamicArrayElement(request_body_packet, temp2[i]);
	}

	addDynamicArrayElement(request_body_packet, (char)strlen(job_name));
	for (i = 0; i < strlen(job_name); i++) {
		addDynamicArrayElement(request_body_packet, job_name[i]);
	}


	addDynamicArrayElement(request_body_packet, 0x42);
	addDynamicArrayElement(request_body_packet, 0x00);


	for (i = 0; i < sizeof(temp3); i++) {
		addDynamicArrayElement(request_body_packet, temp3[i]);
	}

	addDynamicArrayElement(request_body_packet, (char)wcslen(username));

	for (i = 0; i < wcslen(username); i++) {
		addDynamicArrayElement(request_body_packet, (char)username[i]);
	}

	addDynamicArrayElement(request_body_packet, 0x02);// Operation Attributes Group

	if (copies > 1) {
		for (i = 0; i < sizeof(copy); i++) {
			addDynamicArrayElement(request_body_packet, copy[i]);
		}
		addDynamicArrayElement(request_body_packet, 0x04); // Integer Type
		p = (unsigned char*)&copies;

		for (y = 0; y < int_size; ++y) {
			if (IS_BIG_ENDIAN) {
				// 如果是大端字节序，则直接添加字节  
				addDynamicArrayElement(request_body_packet, p[y]);
			}
			else {
				// 如果是小端字节序，则交换字节并添加  
				addDynamicArrayElement(request_body_packet, p[int_size - 1 - y]);
			}
		}
	}
	else {
		// Continue
	}

	if (pageSize != NULL) {

		for (i = 0; i < sizeof(page_size); i++) {
			addDynamicArrayElement(request_body_packet, page_size[i]);
		}
		addDynamicArrayElement(request_body_packet, (char)strlen(pageSize));
		for (i = 0; i < strlen(pageSize); i++) {
			addDynamicArrayElement(request_body_packet, pageSize[i]);
		}
	}
	else {
		// Continue
	}

	if (mediaType != NULL && strncmp(mediaType, "Auto", 4) != 0) {

		for (i = 0; i < sizeof(media_type); i++) {
			addDynamicArrayElement(request_body_packet, media_type[i]);
		}
		addDynamicArrayElement(request_body_packet, (char)strlen(mediaType));
		for (i = 0; i < strlen(mediaType); i++) {
			addDynamicArrayElement(request_body_packet, mediaType[i]);
		}
	}
	else {
		// Continue
	}

	if (duplex != NULL) {
		
		for (i = 0; i < sizeof(name_duplex); i++) {
			addDynamicArrayElement(request_body_packet, name_duplex[i]);
		}
		addDynamicArrayElement(request_body_packet, (char)strlen(duplex));
		for (i = 0; i < strlen(duplex); i++) {
			addDynamicArrayElement(request_body_packet, duplex[i]);
		}

	}
	else {
		// Continue
	}

	if (mopies == TRUE ) {
		for (i = 0; i < sizeof(collate); i++) {
			addDynamicArrayElement(request_body_packet, collate[i]);
		}
		addDynamicArrayElement(request_body_packet, 0x01);
		
	}
	else if (mopies == FALSE) {
		for (i = 0; i < sizeof(collate); i++) {
			addDynamicArrayElement(request_body_packet, collate[i]);
		}
		addDynamicArrayElement(request_body_packet, 0x00);
	}
	else {
		// Continue
	}

	if (colorMode != NULL) {
		for (i = 0; i < sizeof(color_mode); i++) {
			addDynamicArrayElement(request_body_packet, color_mode[i]);
		}
		addDynamicArrayElement(request_body_packet, (char)strlen(colorMode));
		for (i = 0; i < strlen(colorMode); i++) {
			addDynamicArrayElement(request_body_packet, colorMode[i]);
		}

	}
	else {
		// Continue
	}

	addDynamicArrayElement(request_body_packet, 0x03); // End Of Attributes (since no copies attribute)  

	AppendInfoLogToFile("SendIPPPrintJobPacketBySocket():TranslateFileFormatFromPostscriptToPDF");
	pdf_path = TranslateFileFormatFromPostscriptToPDF(file_path, strlen(file_path));
	if (pdf_path == NULL) {
		destroyDynamicArray(request_body_packet);
		remove(file_path);
		AppendErrorLogToFile("SendIPPPrintJobPacketBySocket(): pdf_path is null");
		return FALSE;
	}
	AppendInfoLogToFile("SendIPPPrintJobPacketBySocket(): TranslateFileFormatFromPostscriptToPDF Success");
	//remove(file_path);

	pdf_fp = fopen(pdf_path, "rb");
	if (pdf_fp == NULL) {
		free(pdf_path);
		pdf_path = NULL;
		destroyDynamicArray(request_body_packet);
		AppendErrorLogToFile("SendIPPPrintJobPacketBySocket():Open PDF File Error");
		return FALSE;
	}

	size = CalcPDFFileDataSize(pdf_fp);
	pdf_data = (char*)malloc(size);
	if (pdf_data == NULL) {
		fclose(pdf_fp);
		//remove(pdf_path);
		free(pdf_path);
		pdf_path = NULL;
		destroyDynamicArray(request_body_packet);
		AppendErrorLogToFile("SendIPPPrintJobPacketBySocket():Malloc pdf_data Memory Error");
		return FALSE;
	}


	fread(pdf_data, 1, size, pdf_fp);
	AppendInfoLogToFile("SendIPPPrintJobPacketBySocket():Read PDF File Success");
	for (i = 0; i < size; i++) {
		addDynamicArrayElement(request_body_packet, pdf_data[i]);
	}

	free(pdf_data);
	pdf_data = NULL;

	AppendInfoLogToFile("SendIPPPrintJobPacketBySocket():Start Send IPPPacket To CupsPrinter Server");
	if (SendIPPPacketToCupsPrinterServer(req_context, request_body_packet->data, request_body_packet->size) == TRUE) {
		fclose(pdf_fp);
		//remove(pdf_path);
		free(pdf_path);
		pdf_path = NULL;
		destroyDynamicArray(request_body_packet);
		AppendInfoLogToFile("SendIPPPrintJobPacketBySocket():Send IPPPacket To CupsPrinter Server Success");
		return TRUE;
	}

	AppendErrorLogToFile("SendIPPPrintJobPacketBySocket():Send IPPPacket To CupsPrinter Server Error");

	fclose(pdf_fp);
	remove(pdf_path);
	free(pdf_path);
	pdf_path = NULL;
	destroyDynamicArray(request_body_packet);
	return FALSE;
}


int CalcPDFFileDataSize(FILE* fp)
{
	int end = 0;
	int beg = 0;

	if (fp != NULL) {
		beg = ftell(fp);
		fseek(fp, 0, SEEK_END);
		end = ftell(fp);
		fseek(fp, 0, SEEK_SET);
	}

	return end - beg;
}


char* TranslateFileFormatFromPostscriptToPDF(char* file_path, int file_path_length) {
	int pdf_file_path_len = file_path_length + 2;
	char* cmd = NULL;
	int tmp_len = 0;
	char* dest = NULL;
	char* src = NULL;
	char log_buf[512];
	char* pdf_file_path = (char*)malloc(pdf_file_path_len);
	memset(log_buf, 0, sizeof(log_buf));

	AppendInfoLogToFile("TranslateFileFormatFromPostscriptToPDF():Start");

	if (pdf_file_path == NULL) {
		AppendErrorLogToFile("TranslateFileFormatFromPostscriptToPDF(): PDF File Path Is NULL");
		return NULL;
	}

	memset(pdf_file_path, 0, pdf_file_path_len);

	dest = pdf_file_path;
	src = file_path;

	while (*src != '.') {
		*dest++ = *src++;
	}

	*dest++ = '.';
	*dest++ = 'p';
	*dest++ = 'd';
	*dest++ = 'f';

	tmp_len = strlen("gswin64c -sDEVICE=pdfwrite -o ");

	tmp_len += 1;//"
	tmp_len += strlen(pdf_file_path);
	tmp_len += 1;//"

	tmp_len += 1;//' '

	tmp_len += 1;//"
	tmp_len += file_path_length;
	tmp_len += 1;//"

	tmp_len += 1;//'\0'

	cmd = (char*)malloc(tmp_len);
	if (cmd == NULL) {
		free(pdf_file_path);
		pdf_file_path = NULL;
		AppendErrorLogToFile("TranslateFileFormatFromPostscriptToPDF(): cmd malloc Faild");
		return NULL;
	}

	memset(cmd, 0, tmp_len);

	strncat(cmd, "gswin64c -sDEVICE=pdfwrite -o ", tmp_len);
	strncat(cmd, "\"", tmp_len);
	strncat(cmd, pdf_file_path, tmp_len);
	strncat(cmd, "\"", tmp_len);
	strncat(cmd, " ", tmp_len);

	strncat(cmd, "\"", tmp_len);
	strncat(cmd, file_path, tmp_len);
	strncat(cmd, "\"", tmp_len);
    snprintf(log_buf,sizeof(log_buf),"TranslateFileFormatFromPostscriptToPDF(): cmd=%s",cmd);
	system(cmd);
	free(cmd);
	cmd = NULL;
    
	snprintf(log_buf,sizeof(log_buf),"TranslateFileFormatFromPostscriptToPDF():End pdffilepath=%s",pdf_file_path);
	AppendInfoLogToFile(log_buf);
	return pdf_file_path;
}

void CloseIPPRequestContextConnection(IPPRequestContext* req) {
	WSACleanup();
	if (req == NULL) {
		return;
	}
	if (req->connection_fd > 0) {
		closesocket(req->connection_fd);
	}
	req->connection_fd = -1;

	freeIPPRequestContext(req);
}

void AppendInfoLogToFile(const char* log)
{
	HANDLE hFile = NULL;
	time_t currentTime;
	struct tm* timeInfo;
	DWORD write_size = 0;
	char log_buffer[LOG_BUFFER_SIZE];
	char time_buffer[128];
	memset(log_buffer, 0x00, sizeof(log_buffer));
	memset(time_buffer, 0x00, sizeof(time_buffer));

    //打开日志文件
	hFile = CreateFileW(ERROR_COMMON_DRIVER_LOG_FILE_PATH, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		    //日志文件打开失败则创建文件
			hFile = CreateFileW(ERROR_COMMON_DRIVER_LOG_FILE_PATH, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			//创建失败直接返回，本函数结束
			if (hFile == INVALID_HANDLE_VALUE) {
				return;
			}
	}
    //将文件的指针指向文件尾，用来追加内容
	SetFilePointer(hFile, (LONG)NULL, (PLONG)NULL, FILE_END);
    //取得当前时间
	currentTime = time(NULL);
	timeInfo = localtime(&currentTime);
	strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", timeInfo);

	snprintf(log_buffer,sizeof(log_buffer),"[%s]INFO::%s\r\n",time_buffer,log);

	WriteFile(hFile, log_buffer, strlen(log_buffer), &write_size, NULL);
	CloseHandle(hFile);
}

void AppendErrorLogToFile(const char* log)
{
	HANDLE hFile = NULL;
	time_t currentTime;
	struct tm* timeInfo;
	DWORD write_size = 0;
	char log_buffer[LOG_BUFFER_SIZE];
	char time_buffer[128];
	memset(log_buffer, 0x00, sizeof(log_buffer));
	memset(time_buffer, 0x00, sizeof(time_buffer));

    //打开日志文件
	hFile = CreateFileW(ERROR_COMMON_DRIVER_LOG_FILE_PATH, GENERIC_WRITE, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		    //日志文件打开失败则创建文件
			hFile = CreateFileW(ERROR_COMMON_DRIVER_LOG_FILE_PATH, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
			//创建失败直接返回，本函数结束
			if (hFile == INVALID_HANDLE_VALUE) {
				return;
			}
	}
    //将文件的指针指向文件尾，用来追加内容
	SetFilePointer(hFile, (LONG)NULL, (PLONG)NULL, FILE_END);
    //取得当前时间
	currentTime = time(NULL);
	timeInfo = localtime(&currentTime);
	strftime(time_buffer, sizeof(time_buffer), "%Y-%m-%d %H:%M:%S", timeInfo);

	snprintf(log_buffer,sizeof(log_buffer),"[%s]ERROR::%s\r\n",time_buffer,log);

	WriteFile(hFile, log_buffer, strlen(log_buffer), &write_size, NULL);
	CloseHandle(hFile);

}

int getIpPortAndHost(char* uri_cp, IPPRequestContext* req) {
	char* temp = NULL;
	int ret = 1;
	char* unuse = NULL;
	char* host_cp = NULL;
	if((uri_cp == NULL) || (req == NULL)) {
		AppendErrorLogToFile("getIpPortAndHost(): req or uri_cp is NULL");
		ret = -1;
		return ret;
	}
	unuse = (char*)malloc(MAX_KEY_LENGTH);
	host_cp = (char*)malloc(MAX_KEY_LENGTH);
	req->server_host = (char*)malloc(MAX_KEY_LENGTH);
	req->ip_addr = (char*)malloc(MAX_KEY_LENGTH);
	req->port = (char*)malloc(MAX_KEY_LENGTH);

	 if (unuse == NULL || host_cp == NULL || req->server_host == NULL || req->ip_addr == NULL || req->port == NULL) {
        free(unuse);
		unuse = NULL;
        free(host_cp);
		host_cp = NULL;
        free(req->server_host);
		req->server_host = NULL;
        free(req->ip_addr);
		req->ip_addr = NULL;
        free(req->port);
		req->port = NULL;
        AppendErrorLogToFile("getIpPortAndHost(): malloc memery Failed");
		ret = -1;
        return ret;
    }

	if (sscanf(uri_cp, "%[^//]//%[^/]", unuse, host_cp) == 2) {
		// copy: host_cp -> req->server_host
		strncpy(req->server_host, host_cp, strlen(host_cp));
		req->server_host[strlen(host_cp)] = '\0';

		// get ipaddress
		temp = strtok(host_cp, ":");
		if (temp != NULL) {
			// copy:toke- > req->ip_addr
			strncpy(req->ip_addr, temp, strlen(temp));
			req->ip_addr[strlen(temp)] = '\0';

			// get port 
			temp = strtok(NULL, ":");
			if (temp != NULL) {
				// copy : toke- > req->port
				strncpy(req->port, temp, strlen(temp));
				req->port[strlen(temp)] = '\0';
			}
		}
	} else {
		AppendErrorLogToFile("getIpPortAndHost():Error By Scanf IPADDR And PPORT");
		free(req->server_host);
		req->server_host = NULL;
    	free(req->ip_addr);
		req->ip_addr = NULL;
    	free(req->port);
		req->port = NULL;
		ret = -1;
	}
    free(unuse);
	unuse = NULL;
    free(host_cp);
	host_cp = NULL;
	return ret;
}

void freeIPPRequestContext(IPPRequestContext* req) {
	if (req != NULL) {

		free(req->server_host);
		req->server_host = NULL;
		
		free(req->ip_addr);
		req->ip_addr = NULL;
		
		free(req->port);
		req->port = NULL;
	
		free(req->printer_name);
		req->printer_name = NULL;

		free(req);
		req = NULL;
	}
	else {
		// Do nothing
	}

}

//void GetPagesizestr(PDEVMODE mode, char** out_pagesizestr) {
//	char* pagesizestr = NULL;
//
//	if (wcscmp(mode->dmFormName, (LPWSTR)L"A3") == 0) {
//		pagesizestr = "A3";
//	}
//	else if (wcscmp(mode->dmFormName, (LPWSTR)L"A4") == 0) {
//		pagesizestr = "A4";
//	}
//	else if (wcscmp(mode->dmFormName, (LPWSTR)L"A5") == 0) {
//		pagesizestr = "A5";
//	}
//	else if (wcscmp(mode->dmFormName, (LPWSTR)L"A6") == 0) {
//		pagesizestr = "A6";
//	}
//	else if (wcscmp(mode->dmFormName, (LPWSTR)L"B5") == 0) {
//		pagesizestr = "B5";
//	}
//	else if (wcscmp(mode->dmFormName, (LPWSTR)L"B6") == 0) {
//		pagesizestr = "B6";
//	}
//	else {
//		pagesizestr = "A4";
//	}
//
//	*out_pagesizestr = pagesizestr;
//}

void Getduplexstr(PDEVMODE mode, char* out_duplexstr, size_t string_size) {

	if((mode == NULL) || (out_duplexstr == NULL) || (string_size == 0)){
		return;
	}

	switch (mode->dmDuplex) {
	case 1:
		strncpy(out_duplexstr, "None", string_size-1);
		out_duplexstr[string_size-1] = '\0';
		break;
	case 2:
		strncpy(out_duplexstr, "DuplexNoTumble", string_size-1);
		out_duplexstr[string_size-1] = '\0';
		break;
	case 3:
		strncpy(out_duplexstr, "DuplexTumble", string_size-1);
		out_duplexstr[string_size-1] = '\0';
		break;
	default:
		strncpy(out_duplexstr, "None", string_size-1);
		out_duplexstr[string_size-1] = '\0';
		AppendErrorLogToFile("Getduplexstr():unKnow IPP Duplex argument is Found");
		break;
	}
}

void Getcolormodestr(PDEVMODE mode, char* out_colormode,size_t string_size) {
	
	if((mode == NULL) || (out_colormode == NULL) || (string_size == 0)){
		return;
	}

	switch (mode->dmColor)
	{
	case 1:
		strncpy(out_colormode, "monochrome", string_size-1);
		out_colormode[string_size-1] = '\0';
		break;
	case 2:
		strncpy(out_colormode, "color", string_size-1);
		out_colormode[string_size-1] = '\0';
		break;
	default:
		strncpy(out_colormode, "color", string_size-1);
		out_colormode[string_size-1] = '\0';
		AppendErrorLogToFile("Getcolormodestr():unKnow IPP color argument is Found");
	    break;
	}
}

//char* Getmediatypestr(PDEVMODE mode) {
//	char* mediatypestr = NULL;
//	switch (mode->dmMediaType)
//	{
//	case 256:
//		mediatypestr = "plain";
//		break;
//	case 257:
//		mediatypestr = "recycled";
//		break;
//	case 258:
//		mediatypestr = "letterhead";
//		break;
//	case 259:
//		mediatypestr = "bond";
//		break;
//	case 260:
//		mediatypestr = "cardstock";
//	case 261:
//		mediatypestr = "rough";
//		break;
//	default:
//		mediatypestr = "plain";
//		break;
//	}
//
//	return mediatypestr;
//}

int getInfoFromPsFile(char* file_path, PrinterSettings *settings) {
	FILE* fp = NULL;
	long start_line = 152L;
	long end_line = 261L;
	char buffer[1024] = { 0 };
	long current_line = 0;
	char* value_start = NULL;
	char* value_end = NULL;
	char* keyword = NULL;
	int mopy_time = 0;

	if (settings == NULL) {
		AppendErrorLogToFile("getInfoFromPsFile():settings ss NULL");
		return -1;
	}
	else {
		settings->numCopies = 1;
		settings->mediaType = NULL;
		settings->pageSize = NULL;
		settings->collate = TRUE;
	}
	fp = fopen(file_path, "r");
	if (fp == NULL) {
		AppendErrorLogToFile("getInfoFromPsFile():Open Ps File Failed");
		return -1;
	}

	while (fgets(buffer, sizeof(buffer), fp) != NULL) {
		current_line++;
		if (current_line >= start_line && current_line <= end_line) {
			keyword = strstr(buffer, "NumCopies"); // %%BeginFeature: *NumCopies X\n
			if (keyword && settings->numCopies == 1) {
				value_start = keyword + strlen("NumCopies") + 1; // NumCopies X\n
				value_end = strchr(value_start, '\n'); // X
				if (value_end) {
					*value_end = (char)0;
					settings->numCopies = atoi(value_start);
				}
			}
			// %%BeginFeature: *PageRegion XX\n
			keyword = strstr(buffer, "PageRegion");
			if (keyword && settings->pageSize == NULL) {
				value_start = keyword + strlen("PageRegion") + 1; // PageRegion XX\n
				value_end = strchr(value_start, '\n'); // XX
				if (value_end) {
					*value_end = (char)0;
					settings->pageSize = myStrdup(value_start);
				}
			}
			// %%BeginFeature: *pageSize XX\n
			keyword = strstr(buffer, "PageSize");
			if (keyword && settings->pageSize == NULL) {
				value_start = keyword + strlen("PageSize") + 1; // PageSize XX\n
				value_end = strchr(value_start, '\n'); // XX
				if (value_end) {
					*value_end = (char)0;
					settings->pageSize = myStrdup(value_start);
				}
			}
			// %%BeginFeature: *MediaType XX\n
			keyword = strstr(buffer, "MediaType");
			if (keyword && settings->mediaType == NULL) {
				value_start = keyword + strlen("MediaType") + 1; // MediaType XX\n
				value_end = strchr(value_start, '\n'); // XX
				if (value_end) {
					*value_end = (char)0;
					settings->mediaType = myStrdup(value_start);
				}
			}

			keyword = strstr(buffer, "Collate"); // %%BeginFeature: *Collate X\n
			if (keyword && mopy_time == 0) {
				value_start = keyword + strlen("Collate") + 1; // Collate X\n
				value_end = strchr(value_start, '\n'); // X
				if (value_end) {
					*value_end = (char)0;
					if (strncmp(value_start, "True", 4) == 0) {
						settings->collate = TRUE;
						mopy_time += 1;
					}
					else {
						settings->collate = FALSE;
						mopy_time += 1;
					}
				}
			}
		}
	}
	fclose(fp);
	return 1;
}

char* myStrdup(const char* s) {
	char* newString = NULL;
	size_t len = 0;
	if(s == NULL) {
		return NULL;
	}

	len = strlen(s) + 1;
	newString = malloc(len);

	if (newString != NULL) {
		memset(newString,0x00,len);
		strncpy(newString,s,len-1);
        newString[len-1] = '\0';
	} else{
		AppendErrorLogToFile("myStrdup():malloc Failed");
	}
	return newString;
}

void freePrinterSettings(PrinterSettings* settings) {
	if (settings != NULL) {
		
		free(settings->pageSize);
		settings->pageSize = NULL;
		
		free(settings->mediaType);
		settings->mediaType = NULL;
		
		free(settings);
		settings = NULL;
	}
	else {
		// Do nothing
	}
}


char* WStrToChar(LPWSTR wstr) {
	char* str = NULL;
	int len = 0;
	if(wstr == NULL) {
		AppendErrorLogToFile("WStrToChar():wstr is NULL");
		return NULL;
	}
	//确定将宽字符字符串 wstr 转换为多字节字符串所需的缓冲区大小
	len = WideCharToMultiByte(CP_ACP, 0, wstr, -1, NULL, 0, NULL, NULL);
	if (len > 0) {

	    str = (char*)malloc(len);
	    if (str == NULL) { 
	        AppendErrorLogToFile("WStrToChar():malloc Failed"); 
			return NULL;
	    } else {
	        // 将宽字符字符串 wstr 转换为 UTF-8 编码的多字节字符串，并将结果存储在str
	        WideCharToMultiByte(CP_ACP, 0, wstr, -1, str, len, NULL, NULL);
	    }
	
	} else {
		AppendErrorLogToFile("WStrToChar():WideCharToMultiByte 's len ==0");
	}

    return str;
}
