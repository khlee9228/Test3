#pragma once
#include "CQueue.h"

enum FlowControl
{
	NoFlowControl,
	CtsRtsFlowControl,
	CtsDtrFlowControl,
	DsrRtsFlowControl,
	DsrDtrFlowControl,
	XonXoffFlowControl
};

class	CCommThread
{
public:



	CCommThread();
	~CCommThread();
	//--------- 환경 변수 -----------------------------------------//
	HANDLE		m_hComm;				// 통신 포트 파일 핸들
	CString		m_sPortName;			// 포트 이름 (COM1 ..)
	BOOL		m_bConnected;			// 포트가 열렸는지 유무를 나타냄.
	OVERLAPPED	m_osRead, m_osWrite;	// 포트 파일 Overlapped structure
	HANDLE		m_hThreadWatchComm;		// Watch함수 Thread 핸들.
	WORD		m_wPortID;				// WM_COMM_READ와 함께 보내는 인수.
	CQueue      m_QueueRead;			//큐버퍼


	DWORD		m_tReadData;

	void(*m_pCallback_func)(DWORD, char*);
	//--------- 외부 사용 함수 ------------------------------------//
	BOOL	OpenPort(CString strPortName, 
					   DWORD dwBaud, BYTE byData, BYTE byStop, BYTE byParity, FlowControl fc);//포트 열기 
	void	ClosePort();				//포트 닫기
	DWORD	WriteComm(BYTE *pBuff, DWORD nToWrite);//포트에 데이터 쓰기

	//--------- 내부 사용 함수 ------------------------------------//
	DWORD	ReadComm(BYTE *pBuff, DWORD nToRead);//포트에서 데이터 읽어오기

	CString byIndexComPort(int xPort);// 포트이름 받기 
	DWORD byIndexBaud(int xBaud);// 전송률 받기
	BYTE byIndexData(int xData);//데이터 비트 받기
	BYTE byIndexStop(int xStop);// 스톱비트 받기 
	BYTE byIndexParity(int xParity);// 페리티 받기
	HWND hCommWnd;
	void SetCallbackFunction(void(*p_func)(DWORD, char*)) { m_pCallback_func = p_func; }	//콜백함수 콜 방식으로 이벤트를 전달할 경우 사용.
};

// Thread로 사용할 함수 
DWORD	ThreadWatchComm(CCommThread* pComm);

