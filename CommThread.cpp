#include "stdafx.h"
#include "CommThread.h"

//--- Ŭ���� ������
CCommThread::CCommThread()
{

	//--> �ʱ�� �翬��..��Ʈ�� ������ ���� ���¿��߰���?
	m_bConnected = FALSE;

}

CCommThread::~CCommThread()
{

}


// ��Ʈ sPortName�� dwBaud �ӵ��� ����.
// ThreadWatchComm �Լ����� ��Ʈ�� ���� ������ �� MainWnd�� �˸���
// ���� WM_COMM_READ�޽����� ������ ���� ���� wPortID���� ���� �޴´�.
BOOL CCommThread::OpenPort(CString strPortName, 
					   DWORD dwBaud, BYTE byData, BYTE byStop, BYTE byParity, FlowControl fc)
{

	// Local ����.
		COMMTIMEOUTS	timeouts;
		DCB				dcb;
		DWORD			dwThreadID;
		
		// overlapped structure ���� �ʱ�ȭ.
		m_osRead.Offset = 0;
		m_osRead.OffsetHigh = 0;
		//--> Read �̺�Ʈ ������ ����..
		if ( !(m_osRead.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)) ) 	
		{
			return FALSE;
		}
	

		m_osWrite.Offset = 0;
		m_osWrite.OffsetHigh = 0;
		//--> Write �̺�Ʈ ������ ����..
		if (! (m_osWrite.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL)))
		{
			return FALSE;
		}
		
		//--> ��Ʈ�� ����..
		m_sPortName = strPortName;
	
		//--> ��������...RS 232 ��Ʈ ����..
		m_hComm = CreateFile( m_sPortName, 
							  GENERIC_READ | GENERIC_WRITE, 0, NULL,
							  OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, 
							  NULL);

	
		//--> ��Ʈ ���⿡ �����ϸ�..
		if (m_hComm == (HANDLE) -1)
		{
			//AfxMessageBox("fail Port ofen");
			return FALSE;
		}
	

	//===== ��Ʈ ���� ����. =====

	// EV_RXCHAR event ����...�����Ͱ� ������.. ���� �̺�Ʈ�� �߻��ϰԲ�..
	SetCommMask( m_hComm, EV_RXCHAR);	

	// InQueue, OutQueue ũ�� ����.
	SetupComm( m_hComm, BUFF_SIZE, BUFF_SIZE);	

	// ��Ʈ ����.
	PurgeComm( m_hComm,					
			   PURGE_TXABORT | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_RXCLEAR);

	// timeout ����.
	timeouts.ReadIntervalTimeout = 0xFFFFFFFF;
	timeouts.ReadTotalTimeoutMultiplier = 0;
	timeouts.ReadTotalTimeoutConstant = 0;
	
	timeouts.WriteTotalTimeoutMultiplier = 2*CBR_9600 / dwBaud;
	timeouts.WriteTotalTimeoutConstant = 0;
	
	SetCommTimeouts( m_hComm, &timeouts);

	// dcb ����.... ��Ʈ�� ��������..��� ����ϴ� DCB ����ü�� ����..
	dcb.DCBlength = sizeof(DCB);

	//--> ���� ������ �� �߿���..
	GetCommState( m_hComm, &dcb);	
	
	//--> ���巹��Ʈ�� �ٲٰ�..
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

	//--> ��Ʈ�� ��..����������.. �����غ���..
	if( !SetCommState( m_hComm, &dcb) )	
	{
		return FALSE;
	}


	// ��Ʈ ���� ������ ����.
	m_bConnected = TRUE;

	//--> ��Ʈ ���� ������ ����.
	m_hThreadWatchComm = CreateThread( NULL, 0, 
									   (LPTHREAD_START_ROUTINE)ThreadWatchComm, 
									   this, 0, &dwThreadID);

	//--> ������ ������ �����ϸ�..
	if (! m_hThreadWatchComm)
	{
		//--> ���� ��Ʈ�� �ݰ�..
		ClosePort();
		return FALSE;
	}
	return TRUE;
}
	
// ��Ʈ�� �ݴ´�.
void CCommThread::ClosePort()
{
	//--> ������� �ʾ���.
	if(!m_bConnected) return;

	m_bConnected = FALSE;
	

	//--> ����ũ ����..
	SetCommMask( m_hComm, 0);
	
	//--> ��Ʈ ����.
	PurgeComm( m_hComm,	PURGE_TXABORT | PURGE_TXCLEAR | PURGE_RXABORT | PURGE_RXCLEAR);
	
	CloseHandle(m_hComm);
	//--> �ڵ� �ݱ�


	DWORD dwTime=GetTickCount();

	for(;;){
		if( GetTickCount() - dwTime > 2000){
			break;
		}

		if(m_hThreadWatchComm == NULL) break;
	}
}

// ��Ʈ�� pBuff�� ������ nToWrite��ŭ ����.
// ������ ������ Byte���� �����Ѵ�.
DWORD CCommThread::WriteComm(BYTE *pBuff, DWORD nToWrite)
{
	DWORD	dwWritten, dwError, dwErrorFlags;
	COMSTAT	comstat;

	//--> ��Ʈ�� ������� ���� �����̸�..
	if( !m_bConnected )		
	{
		return 0;
	}


	//--> ���ڷ� ���� ������ ������ nToWrite ��ŭ ����.. �� ������.,dwWrite �� �ѱ�.
	if( !WriteFile( m_hComm, pBuff, nToWrite, &dwWritten, &m_osWrite))
	{
		//--> ���� ������ ���ڰ� ������ ���..
		if (GetLastError() == ERROR_IO_PENDING)
		{
			// ���� ���ڰ� ���� �ְų� ������ ���ڰ� ���� ���� ��� Overapped IO��
			// Ư���� ���� ERROR_IO_PENDING ���� �޽����� ���޵ȴ�.
			//timeouts�� ������ �ð���ŭ ��ٷ��ش�.
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

	//--> ���� ��Ʈ�� ������ ������ ����..
	return dwWritten;
}


// ��Ʈ�κ��� pBuff�� nToWrite��ŭ �д´�.
// ������ ������ Byte���� �����Ѵ�.
DWORD CCommThread::ReadComm(BYTE *pBuff, DWORD nToRead)
{
	DWORD	dwRead,dwError, dwErrorFlags;
	COMSTAT comstat;

	//--- system queue�� ������ byte���� �̸� �д´�.
	ClearCommError( m_hComm, &dwErrorFlags, &comstat);

	//--> �ý��� ť���� ���� �Ÿ��� ������..
	dwRead = comstat.cbInQue;
	
	if(dwRead > 0)
	{

		//--> ���ۿ� �ϴ� �о���̴µ�.. ����..�о���ΰ��� ���ٸ�..

		if( !ReadFile( m_hComm, pBuff, nToRead, &dwRead, &m_osRead) )
		{
			//--> ���� �Ÿ��� ��������..
			if (GetLastError() == ERROR_IO_PENDING)
			{
				//--------- timeouts�� ������ �ð���ŭ ��ٷ��ش�.
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


	//--> ���� �о���� ������ ����.
	return dwRead;

}


// ��Ʈ�� �����ϰ�, ���� ������ ������
// m_ReadData�� ������ �ڿ� MainWnd�� �޽����� ������ Buffer�� ������
// �о��� �Ű��Ѵ�.

DWORD	ThreadWatchComm(CCommThread* pComm)
{
   DWORD           dwEvent;
   OVERLAPPED      os;
   BOOL            bOk = TRUE;
   BYTE            buff[2048];      // �б� ����
   DWORD           dwRead;  // ���� ����Ʈ��.
 

   // Event, OS ����.
   memset( &os, 0, sizeof(OVERLAPPED));
   
   //--> �̺�Ʈ ����..
   if( !(os.hEvent = CreateEvent( NULL, TRUE, FALSE, NULL)) )
   {
		bOk = FALSE;
   }

   //--> �̺�Ʈ ����ũ..
   if( !SetCommMask( pComm->m_hComm, EV_RXCHAR) )
   {
	   bOk = FALSE;
   }

   //--> �̺�Ʈ��..����ũ ������ ������..
   if( !bOk )
   {
		AfxMessageBox("Error while creating ThreadWatchComm, " + pComm->m_sPortName);
		return FALSE;
   }
  
   while (pComm ->m_bConnected)//��Ʈ�� ����Ǹ� ���� ������ ��
   {
 		dwEvent = 0;
	
        // ��Ʈ�� ���� �Ÿ��� �ö����� ��ٸ���.
        WaitCommEvent( pComm->m_hComm, &dwEvent, NULL);
	
	
		//--> �����Ͱ� ���ŵǾ��ٴ� �޼����� �߻��ϸ�..
        if ((dwEvent & EV_RXCHAR) == EV_RXCHAR)
        {
            // ��Ʈ���� ���� �� �ִ� ��ŭ �д´�.
				//--> buff �� �޾Ƴ���..
			do
			{
				dwRead = pComm->ReadComm( buff, 2048); //���� ������ �о� ���� 
				if(BUFF_SIZE - pComm->m_QueueRead.GetSize() > (int)dwRead)
				{
					for( WORD i = 0; i < dwRead; i++ )
					{
						pComm->m_QueueRead.PutByte(buff[i]);//ť ���ۿ�  ���� ������ �ֱ�
					}
				}
				else{
					pComm->m_QueueRead.Clear();
					//AfxMessageBox("buff full"); //ť������ ũ�⸦ �ʰ��ϸ� ��� �޽��� ����
				}
			}while(dwRead);
			pComm->m_tReadData = GetTickCount();

			//MainDlg���� �����Ͱ� ���Դٴ� �޽����� ������ ���� �����ʹ� ���ο��� �о��.
			//�����κ��� ���� ��ȣ�� Ư�� Ʈ���� ��ȣ�� �����̰� �־�� �ȵǴ� �ǽð� �ڷ��̹Ƿ� ���� �Լ��� ������� ����Ѵ�.
			if (pComm->m_pCallback_func != NULL)
			{
				(*(pComm->m_pCallback_func))(pComm->m_tReadData, "serial data received.");
			}
		//	else if (pComm->m_hParentWnd != NULL)
		//	{
		//		::SendMessage(pComm->m_hParentWnd, WM_COMM_READ, pComm->m_nCommID, pComm->m_tReadData);
		//	}
	//		::PostMessage(hCommWnd, WM_COMM_READ, 0, 0 );//�����Ͱ� ���Դٴ� �޽����� ����
		}
		Sleep(1);	// ���� �����͸� ȭ�鿡 ������ �ð��� ���� ����.
					// �����͸� �������� ������ cpu�������� 100%�� �Ǿ� ȭ�鿡 �ѷ��ִ� �۾��� �� �ȵǰ�. ��������� 
					// ť ���ۿ� �����Ͱ� ���̰� ��
   }
   
  CloseHandle( os.hEvent);

   //--> ������ ���ᰡ �ǰ���?
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
