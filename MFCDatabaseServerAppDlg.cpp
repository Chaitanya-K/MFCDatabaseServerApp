
// MFCDatabaseServerAppDlg.cpp : implementation file
//

#include "stdafx.h"
#include "MFCDatabaseServerApp.h"
#include "MFCDatabaseServerAppDlg.h"
#include "afxdialogex.h"
#include <afxdb.h>
#include <fstream>

#ifdef _DEBUG
#define new DEBUG_NEW
#endif



#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <iostream>
#include <string>


// Need to link with Ws2_32.lib, Mswsock.lib, and Advapi32.lib
#pragma comment (lib, "Ws2_32.lib")
#pragma comment (lib, "Mswsock.lib")
#pragma comment (lib, "AdvApi32.lib")


#define DEFAULT_BUFLEN 512
#define DEFAULT_PORT "27015"
using namespace std;

// CAboutDlg dialog used for App About

class CAboutDlg : public CDialogEx
{
public:
	CAboutDlg();

// Dialog Data
	enum { IDD = IDD_ABOUTBOX };

	protected:
	virtual void DoDataExchange(CDataExchange* pDX);    // DDX/DDV support

// Implementation
protected:
	DECLARE_MESSAGE_MAP()
};

CAboutDlg::CAboutDlg() : CDialogEx(CAboutDlg::IDD)
{
}

void CAboutDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CAboutDlg, CDialogEx)
END_MESSAGE_MAP()


// CMFCDatabaseServerAppDlg dialog

void Log(std::string str,int num)
{
	ofstream logFile;
	logFile.open("D:\\serverlog.txt",ios::app);
	logFile<<str<<" : "<<num<<"\n";
	logFile.close();
}

void Log(std::string str,std::string str2="")
{
	ofstream logFile;
	logFile.open("D:\\serverlog.txt",ios::app);
	logFile<<str<<" : "<<str2<<"\n";
	logFile.close();
}

void InitServer()
{
	Log("\nInside InitServer");
	WSADATA wsaData;
    int iResult;

    SOCKET ListenSocket = INVALID_SOCKET;
    SOCKET ClientSocket = INVALID_SOCKET;

    struct addrinfo *result = NULL;
    struct addrinfo hints;

    int iSendResult;
    char recvbuf[DEFAULT_BUFLEN];
    int recvbuflen = DEFAULT_BUFLEN;
    Log("Before Initializing Winsock");
    // Initialize Winsock
    iResult = WSAStartup(MAKEWORD(2,2), &wsaData);
    if (iResult != 0) 
	{
        Log("WSAStartup failed with error",iResult);
        return;
    }

    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    // Resolve the server address and port
    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if ( iResult != 0 ) 
	{
        Log("getaddrinfo failed with error",iResult);
        WSACleanup();
        return;
    }

    // Create a SOCKET for connecting to server
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) 
	{
        Log("socket failed with error");
        freeaddrinfo(result);
        WSACleanup();
        return;
    }
	Log("Before Setup");
    // Setup the TCP listening socket
    iResult = bind( ListenSocket, result->ai_addr, (int)result->ai_addrlen);
    if (iResult == SOCKET_ERROR) 
	{
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) 
	{
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return;
    }
	Log("Before accept");
    // Accept a client socket
    ClientSocket = accept(ListenSocket, NULL, NULL);
    if (ClientSocket == INVALID_SOCKET) 
	{
        Log("accept failed with error");
        closesocket(ListenSocket);
        WSACleanup();
        return;
    }

	// No longer need server socket
	closesocket(ListenSocket);
	

	Log("After Accept");
	CString Key;
	iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
	if (iResult>0)
		Key=recvbuf;	
	Log("Key is",recvbuf);
	
	CDatabase database;
	CString SqlString= L"SELECT ID,ActivationStatus FROM KeyStatus WHERE ID='" + Key + "';";
	CString sID, sActStatus, sUMail, sUName, sActDate;
	CString sDriver = L"MICROSOFT ACCESS DRIVER (*.mdb)";
	CString sDsn;
	CString sFile = L"C:\\Users\\CK\\Documents\\KeyDB.mdb";
	// You must change above path if it's different
	int iRec = 0; 	
	
	// Build ODBC connection string
	sDsn.Format(L"ODBC;DRIVER={%s};DSN='KeyCheckDSN';DBQ=%s",sDriver,sFile);
	TRY
	{
		// Open the database
		database.Open(NULL,false,false,sDsn);
		
		// Allocate the recordset
		CRecordset recset( &database );

		// Build the SQL statement
		CT2CA SSQL (SqlString);
		std::string sSQL(SSQL);
	
		Log("Query is",sSQL);
		// Execute the query
		recset.Open(CRecordset::forwardOnly,SqlString,CRecordset::readOnly);

		// Loop through each record
		if (recset.IsEOF())
		{
			char errorMSG[]="INVALID KEY";
			iSendResult = send( ClientSocket, errorMSG ,sizeof(errorMSG), 0 );
			if (iSendResult == SOCKET_ERROR) 
			{
				printf("send failed with error: %d\n", WSAGetLastError());
				closesocket(ClientSocket);
				WSACleanup();
				return;
			}
		
		}
		while( !recset.IsEOF() )
		{
			// Copy each column into a variable
			recset.GetFieldValue(L"ID",sID);
			recset.GetFieldValue(L"ActivationStatus",sActStatus);

			if (_wtoi(sActStatus)==0)
			{	
				char errorMSG[]="UNACTIVATED KEY";
				iSendResult = send( ClientSocket, errorMSG ,sizeof(errorMSG), 0 );
				if (iSendResult == SOCKET_ERROR) 
				{
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ClientSocket);
					WSACleanup();
					return;
				}
				break;
			}
			else
			{
				char errorMSG[]="READYACK";
				iSendResult = send( ClientSocket, errorMSG ,sizeof(errorMSG), 0 );
				if (iSendResult == SOCKET_ERROR) 
				{
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ClientSocket);
					WSACleanup();
					return;
				}

				CString Query2=L"SELECT * FROM UserDetails WHERE ID='"+Key+"';";
				CRecordset recset2( &database );

				recset2.Open(CRecordset::forwardOnly,Query2,CRecordset::readOnly);
				recset2.GetFieldValue(L"UserName",sUName);
				recset2.GetFieldValue(L"UserMail",sUMail);
				recset2.GetFieldValue(L"ActivationDate",sActDate);
				
				//Send the data back
				
				
				
				iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
				if (iResult>0)
					{
						if (strcmp(recvbuf,"READY?")!=0)
							cout<<"ERROR OCCURED";
					}
				else
					printf("recv failed with error: %d\n", WSAGetLastError());
		
				CT2CA SUName (sUName);
				std::string stUName(SUName);
				const char *tempUName=stUName.c_str();
				iSendResult = send( ClientSocket,tempUName,sUName.GetLength()+1, 0 );
				if (iSendResult == SOCKET_ERROR) 
				{
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ClientSocket);
					WSACleanup();
					return;
				}

				iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
				if (iResult>0)
					{
						if (strcmp(recvbuf,"READY?")!=0)
							cout<<"ERROR OCCURED";
					}
				else
					printf("recv failed with error: %d\n", WSAGetLastError());
		
				CT2CA SUMail (sUMail);
				std::string stUMail(SUMail);
				const char *tempUMail=stUMail.c_str();
				
				iSendResult = send( ClientSocket,tempUMail ,sUMail.GetLength()+1, 0 );
				if (iSendResult == SOCKET_ERROR) 
				{
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ClientSocket);
					WSACleanup();
					return;
				}

				iResult = recv(ClientSocket, recvbuf, recvbuflen, 0);
				if (iResult>0)
					{
						if (strcmp(recvbuf,"READY?")!=0)
							cout<<"ERROR OCCURED";
					}
				else
					printf("recv failed with error: %d\n", WSAGetLastError());

				
				CT2CA SActDate (sActDate);
				std::string stActDate(SActDate);
				const char *tempActDate=stActDate.c_str();
				iSendResult = send( ClientSocket,tempActDate,sActDate.GetLength()+1, 0 );
				if (iSendResult == SOCKET_ERROR) 
				{
					printf("send failed with error: %d\n", WSAGetLastError());
					closesocket(ClientSocket);
					WSACleanup();
					return;
				}
				// goto next record
				recset.MoveNext();
			
			}
		}
		// Close the database
		database.Close();
	}
	CATCH(CDBException, e)
	{
		// If a database exception occured, show error msg
		AfxMessageBox(L"Database error: "+e->m_strError);
	}
	END_CATCH;
	
	    iResult = shutdown(ClientSocket, SD_SEND);
    if (iResult == SOCKET_ERROR) 
	{
        printf("Shutdown failed with error: %d\n", WSAGetLastError());
        closesocket(ClientSocket);
        WSACleanup();
        return;
    }

    // cleanup
    closesocket(ClientSocket);
    WSACleanup();


}


CMFCDatabaseServerAppDlg::CMFCDatabaseServerAppDlg(CWnd* pParent /*=NULL*/)
	: CDialogEx(CMFCDatabaseServerAppDlg::IDD, pParent)
{
	m_hIcon = AfxGetApp()->LoadIcon(IDR_MAINFRAME);
}

void CMFCDatabaseServerAppDlg::DoDataExchange(CDataExchange* pDX)
{
	CDialogEx::DoDataExchange(pDX);
}

BEGIN_MESSAGE_MAP(CMFCDatabaseServerAppDlg, CDialogEx)	
	ON_WM_SYSCOMMAND()
	ON_WM_PAINT()
	ON_WM_QUERYDRAGICON()
END_MESSAGE_MAP()


// CMFCDatabaseServerAppDlg message handlers

BOOL CMFCDatabaseServerAppDlg::OnInitDialog()
{
	CDialogEx::OnInitDialog();

	// Add "About..." menu item to system menu.

	// IDM_ABOUTBOX must be in the system command range.
	ASSERT((IDM_ABOUTBOX & 0xFFF0) == IDM_ABOUTBOX);
	ASSERT(IDM_ABOUTBOX < 0xF000);

	CMenu* pSysMenu = GetSystemMenu(FALSE);
	if (pSysMenu != NULL)
	{
		BOOL bNameValid;
		CString strAboutMenu;
		bNameValid = strAboutMenu.LoadString(IDS_ABOUTBOX);
		ASSERT(bNameValid);
		if (!strAboutMenu.IsEmpty())
		{
			pSysMenu->AppendMenu(MF_SEPARATOR);
			pSysMenu->AppendMenu(MF_STRING, IDM_ABOUTBOX, strAboutMenu);
		}
	}

	// Set the icon for this dialog.  The framework does this automatically
	//  when the application's main window is not a dialog
	SetIcon(m_hIcon, TRUE);			// Set big icon
	SetIcon(m_hIcon, FALSE);		// Set small icon

	// TODO: Add extra initialization here

	return TRUE;  // return TRUE  unless you set the focus to a control
}

void CMFCDatabaseServerAppDlg::OnSysCommand(UINT nID, LPARAM lParam)
{
	if ((nID & 0xFFF0) == IDM_ABOUTBOX)
	{
		CAboutDlg dlgAbout;
		dlgAbout.DoModal();
	}
	else
	{
		CDialogEx::OnSysCommand(nID, lParam);
	}
}

// If you add a minimize button to your dialog, you will need the code below
//  to draw the icon.  For MFC applications using the document/view model,
//  this is automatically done for you by the framework.

void CMFCDatabaseServerAppDlg::OnPaint()
{
	Log("In Onpaint");
	if (IsIconic())
	{
		CPaintDC dc(this); // device context for painting

		SendMessage(WM_ICONERASEBKGND, reinterpret_cast<WPARAM>(dc.GetSafeHdc()), 0);

		// Center icon in client rectangle
		int cxIcon = GetSystemMetrics(SM_CXICON);
		int cyIcon = GetSystemMetrics(SM_CYICON);
		CRect rect;
		GetClientRect(&rect);
		int x = (rect.Width() - cxIcon + 1) / 2;
		int y = (rect.Height() - cyIcon + 1) / 2;

		// Draw the icon
		dc.DrawIcon(x, y, m_hIcon);
		
	}
	else
	{
		CDialogEx::OnPaint();
	}
	while(1)
		InitServer();
}

// The system calls this function to obtain the cursor to display while the user drags
//  the minimized window.
HCURSOR CMFCDatabaseServerAppDlg::OnQueryDragIcon()
{
	return static_cast<HCURSOR>(m_hIcon);
}

