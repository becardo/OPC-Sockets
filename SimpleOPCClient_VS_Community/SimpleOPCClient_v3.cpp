// Simple OPC Client
//
// This is a modified version of the "Simple OPC Client" originally
// developed by Philippe Gras (CERN) for demonstrating the basic techniques
// involved in the development of an OPC DA client.
//
// The modifications are the introduction of two C++ classes to allow the
// the client to ask for callback notifications from the OPC server, and
// the corresponding introduction of a message comsumption loop in the
// main program to allow the client to process those notifications. The
// C++ classes implement the OPC DA 1.0 IAdviseSink and the OPC DA 2.0
// IOPCDataCallback client interfaces, and in turn were adapted from the
// KEPWARE´s  OPC client sample code. A few wrapper functions to initiate
// and to cancel the notifications were also developed.
//
// The original Simple OPC Client code can still be found (as of this date)
// in
//        http://pgras.home.cern.ch/pgras/OPCClientTutorial/
//
//
// Luiz T. S. Mendes - DELT/UFMG - 15 Sept 2011
// luizt at cpdee.ufmg.br
//

#include <atlbase.h>    // required for using the "_T" macro
#include <iostream>
#include <ObjIdl.h>

#include "opcda.h"
#include "opcerror.h"
#include "SimpleOPCClient_v3.h"
#include "SOCAdviseSink.h"
#include "SOCDataCallback.h"
#include "SOCWrapperFunctions.h"
#include "DadosProcessos.h"

using namespace std;

#define OPC_SERVER_NAME L"Matrikon.OPC.Simulation.1"
#define NUM_ITEMS 7
#define NUM_LEITURA 4

//#define REMOTE_SERVER_NAME L"your_path"

// Global variables

// The OPC DA Spec requires that some constants be registered in order to use
// them. The one below refers to the OPC DA 1.0 IDataObject interface.
UINT OPC_DATA_TIME = RegisterClipboardFormat (_T("OPCSTMFORMATDATATIME"));

wchar_t* ITEMS_ID[] = {
	L"Saw-toothed Waves.Real4",
	L"Square Waves.Real4",
	L"Triangle Waves.Real4",
	L"Random.Real4",
	L"Bucket Brigade.Int4",
	L"Bucket Brigade.Real4",
	L"Bucket Brigade.Real8"
};
static const VARTYPE ITEMS_VT[NUM_ITEMS] = { VT_R4, VT_R4, VT_R4, VT_R4, VT_I4, VT_R4, VT_R8 };

void WriteSetpoints(IUnknown* pGroup, OPCHANDLE* h, int pCamara, float temp, double pInjecao)
{
	IOPCSyncIO* io = NULL;
	if (FAILED(pGroup->QueryInterface(__uuidof(io), (void**)&io))) return;

	VARIANT v[3];
	for (int i = 0; i < 3; i++) {
		VariantInit(&v[i]);
	}

	v[0].vt = VT_I4; 
	v[0].lVal = pCamara;

	v[1].vt = VT_R4; 
	v[1].fltVal = temp;

	v[2].vt = VT_R8; 
	v[2].dblVal = pInjecao;

	HRESULT* pErr = NULL;
	HRESULT hr = io->Write(3, h, v, &pErr);

	if (FAILED(hr)) {
		printf("[OPC escrita] Falha no Write. Error = %x\n", hr);
	} else {
		printf("[OPC escrita] %ls = %d (%s)\n", ITEMS_ID[4], pCamara, FAILED(pErr[0]) ? "ERRO" : "ok");
		printf("[OPC escrita] %ls = %.1f (%s)\n", ITEMS_ID[5], temp, FAILED(pErr[1]) ? "ERRO" : "ok");
		printf("[OPC escrita] %ls = %.1f (%s)\n", ITEMS_ID[6], pInjecao, FAILED(pErr[2]) ? "ERRO" : "ok");
	}

	CoTaskMemFree(pErr);
	io->Release();
}

void threadClienteOPC(void) 
{
	IOPCServer* pIOPCServer = NULL;   //pointer to IOPServer interface
	IOPCItemMgt* pIOPCItemMgt = NULL; //pointer to IOPCItemMgt interface

	OPCHANDLE hServerGroup; // server handle to the group
	OPCHANDLE hServerItem[NUM_ITEMS];  // server handle to the item

	int i;
	char buf[100];

	// Have to be done before using microsoft COM library:
	printf("Initializing the COM environment...\n");
	CoInitialize(NULL);

	// Let's instantiante the IOPCServer interface and get a pointer of it:
	printf("Instantiating the MATRIKON OPC Server for Simulation...\n");
	pIOPCServer = InstantiateServer(OPC_SERVER_NAME);
	
	// Add the OPC group the OPC server and get an handle to the IOPCItemMgt
	//interface:
	printf("Adding a group in the INACTIVE state for the moment...\n");
	AddTheGroup(pIOPCServer, pIOPCItemMgt, hServerGroup);

	// Add the OPC item. First we have to convert from wchar_t* to char*
	// in order to print the item name in the console.
    size_t m;
	wcstombs_s(&m, buf, 100, *ITEMS_ID, _TRUNCATE);
	printf("Adding the items to the group...\n", buf);
	AddTheItem(pIOPCItemMgt, hServerItem);

	// Callback assincrono (OPC DA 2.0)
	IConnectionPoint* pIConnectionPoint = NULL;
	DWORD dwCookie = 0;
	SOCDataCallback* pSOCDataCallback = new SOCDataCallback();
	pSOCDataCallback->AddRef();

	printf("Setting up the IConnectionPoint callback connection...\n");
	SetDataCallback(pIOPCItemMgt, pSOCDataCallback, pIConnectionPoint, &dwCookie);

	printf("Changing the group state to ACTIVE...\n");
	SetGroupActive(pIOPCItemMgt);
	
	// Escrita sincrona dos set-points
	MSG msg;
	while (!g_encerrar) {
		MsgWaitForMultipleObjects(0, NULL, FALSE, 100, QS_ALLINPUT);
		while (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		bool escrever = false;
		int p = 0; double t = 0, a = 0;
		{
			std::lock_guard<std::mutex> lk(mtx_dados);
			if (dadosGlobais.sp_pendente) {
				p = dadosGlobais.sp_pressao_camara;
				t = dadosGlobais.sp_temp_camara;
				a = dadosGlobais.sp_pressao_argonio;
				dadosGlobais.sp_pendente = false;
				escrever = true;
			}
		}
		if (escrever)
			WriteSetpoints(pIOPCItemMgt, &hServerItem[NUM_LEITURA], p, (float)t, a);
	}

	// Cancel the callback and release its reference
	printf("Cancelling the IOPCDataCallback notifications...\n");
    CancelDataCallback(pIConnectionPoint, dwCookie);
	//pIConnectionPoint->Release();
	pSOCDataCallback->Release();

	// Remove the OPC item:
	printf("Removing the OPC item...\n");
	RemoveItem(pIOPCItemMgt, hServerItem);

	// Remove the OPC group:
	printf("Removing the OPC group object...\n");
    pIOPCItemMgt->Release();
	RemoveGroup(pIOPCServer, hServerGroup);

	// release the interface references:
	printf("Removing the OPC server object...\n");
	pIOPCServer->Release();

	//close the COM library:
	printf ("Releasing the COM environment...\n");
	CoUninitialize();
}

////////////////////////////////////////////////////////////////////
// Instantiate the IOPCServer interface of the OPCServer
// having the name ServerName. Return a pointer to this interface
//
IOPCServer* InstantiateServer(wchar_t ServerName[])
{
	CLSID CLSID_OPCServer;
	HRESULT hr;

	// get the CLSID from the OPC Server Name:
	hr = CLSIDFromString(ServerName, &CLSID_OPCServer);
	_ASSERT(!FAILED(hr));


	//queue of the class instances to create
	LONG cmq = 1; // nbr of class instance to create.
	MULTI_QI queue[1] =
		{{&IID_IOPCServer,
		NULL,
		0}};

	//Server info:
	//COSERVERINFO CoServerInfo =
    //{
	//	/*dwReserved1*/ 0,
	//	/*pwszName*/ REMOTE_SERVER_NAME,
	//	/*COAUTHINFO*/  NULL,
	//	/*dwReserved2*/ 0
    //}; 

	// create an instance of the IOPCServer
	hr = CoCreateInstanceEx(CLSID_OPCServer, NULL, CLSCTX_SERVER,
		/*&CoServerInfo*/NULL, cmq, queue);
	_ASSERT(!hr);

	// return a pointer to the IOPCServer interface:
	return(IOPCServer*) queue[0].pItf;
}


/////////////////////////////////////////////////////////////////////
// Add group "Group1" to the Server whose IOPCServer interface
// is pointed by pIOPCServer. 
// Returns a pointer to the IOPCItemMgt interface of the added group
// and a server opc handle to the added group.
//
void AddTheGroup(IOPCServer* pIOPCServer, IOPCItemMgt* &pIOPCItemMgt, 
				 OPCHANDLE& hServerGroup)
{
	DWORD dwUpdateRate = 0;
	OPCHANDLE hClientGroup = 0;

	// Add an OPC group and get a pointer to the IUnknown I/F:
    HRESULT hr = pIOPCServer->AddGroup(/*szName*/ L"Group1",
		/*bActive*/ FALSE,
		/*dwRequestedUpdateRate*/ 1000,
		/*hClientGroup*/ hClientGroup,
		/*pTimeBias*/ 0,
		/*pPercentDeadband*/ 0,
		/*dwLCID*/0,
		/*phServerGroup*/&hServerGroup,
		&dwUpdateRate,
		/*riid*/ IID_IOPCItemMgt,
		/*ppUnk*/ (IUnknown**) &pIOPCItemMgt);
	_ASSERT(!FAILED(hr));
}



//////////////////////////////////////////////////////////////////
// Add the Item ITEM_ID to the group whose IOPCItemMgt interface
// is pointed by pIOPCItemMgt pointer. Return a server opc handle
// to the item.
 
void AddTheItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE* hServerItem)
{
	HRESULT hr;
	OPCITEMDEF ItemArray[NUM_ITEMS];

	for (int i = 0; i < NUM_ITEMS; i++) {
		ItemArray[i].szAccessPath = L"";
		ItemArray[i].szItemID = ITEMS_ID[i];
		ItemArray[i].bActive = TRUE;
		ItemArray[i].hClient = i + 1;
		ItemArray[i].dwBlobSize = 0;
		ItemArray[i].pBlob = NULL;
		ItemArray[i].vtRequestedDataType = ITEMS_VT[i];
		ItemArray[i].wReserved = 0;
	}

	//Add Result:
	OPCITEMRESULT* pAddResult=NULL;
	HRESULT* pErrors = NULL;

	// Add an Item to the previous Group:
	hr = pIOPCItemMgt->AddItems(NUM_ITEMS, ItemArray, &pAddResult, &pErrors);
	if (hr != S_OK){
		printf("Failed call to AddItems function. Error code = %x\n", hr);
		exit(0);
	}
	for (int i = 0; i < NUM_ITEMS; i++) {
		// Server handle for the added item:
		hServerItem[i] = pAddResult[i].hServer;

		// release memory allocated by the server:
		CoTaskMemFree(pAddResult[i].pBlob);   // libera o blob de cada item

	}

	CoTaskMemFree(pAddResult);
	pAddResult = NULL;

	CoTaskMemFree(pErrors);
	pErrors = NULL;
}

///////////////////////////////////////////////////////////////////////////////
// Read from device the value of the item having the "hServerItem" server 
// handle and belonging to the group whose one interface is pointed by
// pGroupIUnknown. The value is put in varValue. 
//
void ReadItem(IUnknown* pGroupIUnknown, OPCHANDLE* hServerItem, VARIANT* varValue)
{
	// value of the item:
	OPCITEMSTATE* pValue = NULL;

	//get a pointer to the IOPCSyncIOInterface:
	IOPCSyncIO* pIOPCSyncIO;
	pGroupIUnknown->QueryInterface(__uuidof(pIOPCSyncIO), (void**) &pIOPCSyncIO);

	// read the item value from the device:
	HRESULT* pErrors = NULL; //to store error code(s)
	HRESULT hr = pIOPCSyncIO->Read(OPC_DS_DEVICE, NUM_ITEMS, hServerItem, &pValue, &pErrors);
	_ASSERT(!hr);
	_ASSERT(pValue!=NULL);

	for (int i = 0; i < NUM_ITEMS; i++) {
		if (SUCCEEDED(pErrors[i]))
			varValue[i] = pValue[i].vDataValue;
		else
			printf("Error reading item %d: %x\n", i, pErrors[i]);
	}

	//Release memeory allocated by the OPC server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;

	CoTaskMemFree(pValue);
	pValue = NULL;

	// release the reference to the IOPCSyncIO interface:
	pIOPCSyncIO->Release();
}

///////////////////////////////////////////////////////////////////////////
// Remove the item whose server handle is hServerItem from the group
// whose IOPCItemMgt interface is pointed by pIOPCItemMgt
//
void RemoveItem(IOPCItemMgt* pIOPCItemMgt, OPCHANDLE* hServerItem)
{
	// server handle of items to remove:
	/*OPCHANDLE hServerArray[1];
	hServerArray[0] = hServerItem;*/
	
	//Remove the item:
	HRESULT* pErrors; // to store error code(s)
	HRESULT hr = pIOPCItemMgt->RemoveItems(NUM_ITEMS, hServerItem, &pErrors);
	_ASSERT(!hr);

	//release memory allocated by the server:
	CoTaskMemFree(pErrors);
	pErrors = NULL;
}

////////////////////////////////////////////////////////////////////////
// Remove the Group whose server handle is hServerGroup from the server
// whose IOPCServer interface is pointed by pIOPCServer
//
void RemoveGroup (IOPCServer* pIOPCServer, OPCHANDLE hServerGroup)
{
	// Remove the group:
	HRESULT hr = pIOPCServer->RemoveGroup(hServerGroup, FALSE);
	if (hr != S_OK){
		if (hr == OPC_S_INUSE)
			printf ("Failed to remove OPC group: object still has references to it.\n");
		else printf ("Failed to remove OPC group. Error code = %x\n", hr);
		exit(0);
	}
}
