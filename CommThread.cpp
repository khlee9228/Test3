#include "stdafx.h"
#include "CommThread.h"

//--- 클래스 생성자
CCommThread::CCommThread()
{

	//--> 초기는 당연히..포트가 열리지 않은 상태여야겠죠?
	m_bConnected = FALSE;

}

CCommThread::~CCommThread()
{

}


// 포트 sPortName을 dwBaud 속도로 연다.
// ThreadWatchComm 함수에서 포트에 무언가 읽혔을 때 MainWnd에 알리기
// 위해 WM_COMM_READ메시지를 보낼때 같이 보낼 wPortID값을 전달 받는다.
BOOL CCommThread::OpenPort(CString strPortName, 
					   DWORD dwBaud, BYTE byData, BYTE byStop, BYTE byParity, FlowControl fc)
{

	// Local 변수.
		COMMTIMEOUTS	timeouts;
		DCB				dcb;
		DWORD			dwThreadID;
		
		// overlapped structure 변수 초기화.
		m_osRead.Offset = 0;
		m_osRead.OffsetHigh = 0;
		//--> Read 이벤트 생성에 실패..
		if ( !(m_osRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)) ) 	
		{
			return FALSE;
		}
	

		m_osWrite.Offset = 0;
		m_osWrite.OffsetHigh = 0;
		//--> Write 이벤트 생성에 실패..
		if (! (m_osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		{
			return FALSE;
		}
		
		//--> 포트명 저장..
		m_sPortName = strPortName;
	
		//--> 실제적인...RS 232 포트 열기..
		m_hComm = CreateFile( m_sPortName, 
							  GENERIC_READ | GENERIC_WRITE, 0, NULL,
							  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 
							  NULL);

	
		//--> 포트 열기에 실해하면..
		if (m_hComm == (HANDLE) -1)
		{
			//AfxMessageBox("fail Port ofen");
			return FALSE;
		}
	

	//===== 포트 상태 설정. =====

	// EV_RXCHAR event 설정...데이터가 들어오면.. 수신 이벤트가 발생하게끔..
	SetCommMask( m_hComm, EV_RXCHAR);	

	// InQueue, OutQueue 크기 설정.
	SetupComm( m_hComm, BUFF_SIZE, BUFF_SIZE);	

	// 포트 비우기.
	PurgeComm( m_hComm,					
			   PURGE_TXABORT | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_RXCLEAR);

	// timeout 설정.
	timeouts.ReadIntervalTimeout = 0xFFFFFFFF;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 0;
	
	timeouts.WriteTotalTimeoutMultiplier = 2*CBR_9600 / dwBaud;
	timeouts.WriteTotalTimeoutConstant = 0;
	
	SetCommTimeouts( m_hComm, &timeouts);

	// dcb 설정.... 포트의 실제적인..제어를 담당하는 DCB 구조체값 셋팅..
	dcb.DCBlength = sizeof(DCB);

	//--> 현재 설정된 값 중에서..
	GetCommState( m_hComm, &dcb);	
	
	//--> 보드레이트를 바꾸고..
	dcb.BaudRate = dwBaud;
	

	//--> Data 8 Bit
	dcb.ByteSize = byData;

	//--> Noparity
	dcb.Parity = byParity;

	//--> 1 Stop Bit
	dcb.StopBits = byStop;

	dcb.fDsrSensitivity = FALSE;

	switch (fc)
	{
	case NoFlowControl:
		{
			dcb.fOutxCtsFlow = FALSE;
			dcb.fOutxDsrFlow = FALSE;
			dcb.fOutX = FALSE;
			dcb.fInX = FALSE;
			break;
		}
	case CtsRtsFlowControl:
		{
			dcb.fOutxCtsFlow = TRUE;
			dcb.fOutxDsrFlow = FALSE;
			dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
			dcb.fOutX = FALSE;
			dcb.fInX = FALSE;
			break;
		}
	case CtsDtrFlowControl:
		{
			dcb.fOutxCtsFlow = TRUE;
			dcb.fOutxDsrFlow = FALSE;
			dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
			dcb.fOutX = FALSE;
			dcb.fInX = FALSE;
			break;
		}
	case DsrRtsFlowControl:
		{
			dcb.fOutxCtsFlow = FALSE;
			dcb.fOutxDsrFlow = TRUE;
			dcb.fRtsControl = RTS_CONTROL_HANDSHAKE;
			dcb.fOutX = FALSE;
			dcb.fInX = FALSE;
			break;
		}
	case DsrDtrFlowControl:
		{
			dcb.fOutxCtsFlow = FALSE;
			dcb.fOutxDsrFlow = TRUE;
			dcb.fDtrControl = DTR_CONTROL_HANDSHAKE;
			dcb.fOutX = FALSE;
			dcb.fInX = FALSE;
			break;
		}
	case XonXoffFlowControl:
		{
			dcb.fOutxCtsFlow = FALSE;
			dcb.fOutxDsrFlow = FALSE;
			dcb.fOutX = TRUE;
			dcb.fInX = TRUE;
			dcb.XonChar = 0x11;
			dcb.XoffChar = 0x13;
			dcb.XoffLim = 6045;
			dcb.XonLim = 6045;
			break;
		}
	default:
		{
			ASSERT(FALSE);
			break;
		}
	}

	//--> 포트를 재..설정값으로.. 설정해보고..
	if( !SetCommState( m_hComm, &dcb) )	
	{
		return FALSE;
	}


	// 포트 감시 쓰레드 생성.
	m_bConnected = TRUE;

	//--> 포트 감시 쓰레드 생성.
	m_hThreadWatchComm = CreateThread( NULL, 0, 
									   (LPTHREAD_START_ROUTINE)ThreadWatchComm, 
									   this, 0, &dwThreadID);

	//--> 쓰레드 생성에 실패하면..
	if (! m_hThreadWatchComm)
	{
		//--> 열린 포트를 닫고..
		ClosePort();
		return FALSE;
	}
	return TRUE;
}
	
// 포트를 닫는다.
void CCommThread::ClosePort()
{
	//--> 연결되지 않았음.
	if(!m_bConnected) return;

	m_bConnected = FALSE;
	

	//--> 마스크 해제..
	SetCommMask( m_hComm, 0);
	
	//--> 포트 비우기.
	PurgeComm( m_hComm,	PURGE_TXABORT | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_RXCLEAR);
	
	CloseHandle(m_hComm);
	//--> 핸들 닫기


	DWORD dwTime=GetTickCount();

	for(;;){
		if( GetTickCount() - dwTime > 2000){
			break;
		}

		if(m_hThreadWatchComm == NULL) break;
	}
}

// 포트에 pBuff의 내용을 nToWrite만큼 쓴다.
// 실제로 쓰여진 Byte수를 리턴한다.
DWORD CCommThread::WriteComm(BYTE *pBuff, DWORD nToWrite)
{
	DWORD	dwWritten, dwError, dwErrorFlags;
	COMSTAT	comstat;

	//--> 포트가 연결되지 않은 상태이면..
	if( !m_bConnected )		
	{
		return 0;
	}


	//--> 인자로 들어온 버퍼의 내용을 nToWrite 만큼 쓰고.. 쓴 갯수를.,dwWrite 에 넘김.
	if( !WriteFile( m_hComm, pBuff, nToWrite, &dwWritten, &m_osWrite))
	{
		//--> 아직 전송할 문자가 남았을 경우..
		if (GetLastError() == ERROR_IO_PENDING)
		{
			// 읽을 문자가 남아 있거나 전송할 문자가 남아 있을 경우 Overapped IO의
			// 특성에 따라 ERROR_IO_PENDING 에러 메시지가 전달된다.
			//timeouts에 정해준 시간만큼 기다려준다.
			while (! GetOverlappedResult( m_hComm, &m_osWrite, &dwWritten, TRUE))
			{
				dwError = GetLastError();
				if (dwError != ERROR_IO_INCOMPLETE)
				{
					ClearCommError( m_hComm, &dwErrorFlags, &comstat);
					break;
				}
			}
		}
		else
		{
			dwWritten = 0;
			ClearCommError( m_hComm, &dwErrorFlags, &comstat);
		}
	}

	//--> 실제 포트로 쓰여진 갯수를 리턴..
	return dwWritten;
}


// 포트로부터 pBuff에 nToWrite만큼 읽는다.
// 실제로 읽혀진 Byte수를 리턴한다.
DWORD CCommThread::ReadComm(BYTE *pBuff, DWORD nToRead)
{
	DWORD	dwRead,dwError, dwErrorFlags;
	COMSTAT comstat;

	//--- system queue에 도착한 byte수만 미리 읽는다.
	ClearCommError( m_hComm, &dwErrorFlags, &comstat);

	//--> 시스템 큐에서 읽을 거리가 있으면..
	dwRead = comstat.cbInQue;
	
	if(dwRead > 0)
	{

		//--> 버퍼에 일단 읽어들이는데.. 만일..읽어들인값이 없다면..

		if( !ReadFile( m_hComm, pBuff, nToRead, &dwRead, &m_osRead) )
		{
			//--> 읽을 거리가 남았으면..
			if (GetLastError() == ERROR_IO_PENDING)
			{
				//--------- timeouts에 정해준 시간만큼 기다려준다.
				while (! GetOverlappedResult( m_hComm, &m_osRead, &dwRead, TRUE))
				{
					dwError = GetLastError();
					if (dwError != ERROR_IO_INCOMPLETE)
					{
						ClearCommError( m_hComm, &dwErrorFlags, &comstat);
						break;
					}
				}
			}
			else
			{
				dwRead = 0;
				ClearCommError( m_hComm, &dwErrorFlags, &comstat);
			}
		}
	}


	//--> 실제 읽어들인 갯수를 리턴.
	return dwRead;

}


// 포트를 감시하고, 읽힌 내용이 있으면
// m_ReadData에 저장한 뒤에 MainWnd에 메시지를 보내어 Buffer의 내용을
// 읽어가라고 신고한다.

DWORD	ThreadWatchComm(CCommThread* pComm)
{
   DWORD           dwEvent;
   OVERLAPPED      os;
   BOOL            bOk = TRUE;
   BYTE            buff[2048];      // 읽기 버퍼
   DWORD           dwRead;  // 읽은 바이트수.
 

   // Event, OS 설정.
   memset( &os, 0, sizeof(OVERLAPPED));
   
   //--> 이벤트 설정..
   if( !(os.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL)) )
   {
		bOk = FALSE;
   }

   //--> 이벤트 마스크..
   if( !SetCommMask( pComm->m_hComm, EV_RXCHAR) )
   {
	   bOk = FALSE;
   }

   //--> 이벤트나..마스크 설정에 실패함..
   if( !bOk )
   {
		AfxMessageBox("Error while creating ThreadWatchComm, " + pComm->m_sPortName);
		return FALSE;
   }
  
   while (pComm ->m_bConnected)//포트가 연결되면 무한 루프에 들어감
   {
 		dwEvent = 0;
	
        // 포트에 읽을 거리가 올때까지 기다린다.
        WaitCommEvent( pComm->m_hComm, &dwEvent, NULL);
	
	
		//--> 데이터가 수신되었다는 메세지가 발생하면..
        if ((dwEvent & EV_RXCHAR) == EV_RXCHAR)
        {
            // 포트에서 읽을 수 있는 만큼 읽는다.
				//--> buff 에 받아놓고..
			do
			{
				dwRead = pComm->ReadComm( buff, 2048); //들어온 데이터 읽어 오기 
				if(BUFF_SIZE - pComm->m_QueueRead.GetSize() > (int)dwRead)
				{
					for( WORD i = 0; i < dwRead; i++ )
					{
						pComm->m_QueueRead.PutByte(buff[i]);//큐 버퍼에  들어온 데이터 넣기
					}
				}
				else{
					pComm->m_QueueRead.Clear();
					//AfxMessageBox("buff full"); //큐버퍼의 크기를 초과하면 경고 메시지 보냄
				}
			}while(dwRead);
			pComm->m_tReadData = GetTickCount();

			//MainDlg에게 데이터가 들어왔다는 메시지를 보내고 실제 데이터는 메인에서 읽어낸다.
			//제어보드로부터 오는 신호는 특히 트리거 신호가 딜레이가 있어서는 안되는 실시간 자료이므로 직접 함수콜 방식으로 사용한다.
			if (pComm->m_pCallback_func != NULL)
			{
				(*(pComm->m_pCallback_func))(pComm->m_tReadData, "serial data received.");
			}
		//	else if (pComm->m_hParentWnd != NULL)
		//	{
		//		::SendMessage(pComm->m_hParentWnd, WM_COMM_READ, pComm->m_nCommID, pComm->m_tReadData);
		//	}
	//		::PostMessage(hCommWnd, WM_COMM_READ, 0, 0 );//데이터가 들어왔다는 메시지를 보냄
		}
		Sleep(1);	// 받은 데이터를 화면에 보여줄 시간을 벌기 위해.
					// 데이터를 연속으로 받으면 cpu점유율이 100%가 되어 화면에 뿌려주는 작업이 잘 안되고. 결과적으로 
					// 큐 버퍼에 데이터가 쌓이게 됨
   }
   
  CloseHandle( os.hEvent);

   //--> 쓰레드 종료가 되겠죠?
   pComm->m_hThreadWatchComm = NULL;

   return TRUE;

}


CString CCommThread::byIndexComPort(int xPort)
{
	CString PortName;
	switch(xPort)
	{
		case 1:		PortName = "COM1"; 			break;
		case 2:		PortName = "COM2";			break;
		case 3:		PortName = "COM3"; 			break;
		case 4:		PortName = "COM4";			break;
		case 5:		PortName = "COM5";			break;
		case 6:		PortName = "COM6";			break;
		case 7:		PortName = "COM7";			break;
		case 8:		PortName = "COM8";			break;
		case 9:		PortName = "COM9";			break;
		case 10:	PortName = "\\\\.\\COM10";		break;
		case 11:	PortName = "\\\\.\\COM11";		break;
		case 12:	PortName = "\\\\.\\COM12";		break;
		case 13:	PortName = "\\\\.\\COM13";		break;
		case 14:	PortName = "\\\\.\\COM14";		break;
		case 15:	PortName = "\\\\.\\COM15";		break;
		case 16:	PortName = "\\\\.\\COM16";		break;
		case 17:	PortName = "\\\\.\\COM17";		break;
		case 18:	PortName = "\\\\.\\COM18";		break;
		case 19:	PortName = "\\\\.\\COM19";		break;
		case 20:	PortName = "\\\\.\\COM20";		break;
		case 21:	PortName = "\\\\.\\COM21";		break;
		case 22:	PortName = "\\\\.\\COM22";		break;
		case 23:	PortName = "\\\\.\\COM23";		break;
		case 24:	PortName = "\\\\.\\COM24";		break;
		case 25:	PortName = "\\\\.\\COM25";		break;
		case 26:	PortName = "\\\\.\\COM26";		break;
		case 27:	PortName = "\\\\.\\COM27";		break;
		case 28:	PortName = "\\\\.\\COM28";		break;
		case 29:	PortName = "\\\\.\\COM29";		break;
	}


	
	return PortName;
}

DWORD CCommThread::byIndexBaud(int xBaud)
{
	DWORD dwBaud;
	switch(xBaud)
	{
		case 0:		dwBaud = CBR_4800;		break;
			
		case 1:		dwBaud = CBR_9600;		break;
	
		case 2:		dwBaud = CBR_14400;		break;
	
		case 3:		dwBaud = CBR_19200;		break;
	
		case 4:		dwBaud = CBR_38400;		break;
	
		case 5:		dwBaud = CBR_56000;		break;
	
		case 6:		dwBaud = CBR_57600;		break;
	
		case 7:		dwBaud = CBR_115200;	break;
	}

	return dwBaud;
}

BYTE CCommThread::byIndexData(int xData)
{
	BYTE byData;
	switch(xData)
	{
		case 0 :	byData = 5;			break;
	
		case 1 :	byData = 6;			break;
		
		case 2 :	byData = 7;			break;
		
		case 3 :	byData = 8;			break;
	}
	return byData;
}

BYTE CCommThread::byIndexStop(int xStop)
{
	BYTE byStop;
	if(xStop == 0)
	{
		byStop = ONESTOPBIT;
	}
	else
	{
		byStop = TWOSTOPBITS;
	}
	return byStop;
}
BYTE CCommThread::byIndexParity(int xParity)
{
	BYTE byParity;
	switch(xParity)
	{
	case 0 :	byParity = NOPARITY;	break;
	
	case 1 :	byParity = ODDPARITY;	break;
	
	case 2 :	byParity = EVENPARITY;	break;
	}

	return byParity;
}
