/*BEGIN_LEGAL 
Intel Open Source License 

Copyright (c) 2002-2012 Intel Corporation. All rights reserved.
 
Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are
met:

Redistributions of source code must retain the above copyright notice,
this list of conditions and the following disclaimer.  Redistributions
in binary form must reproduce the above copyright notice, this list of
conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.  Neither the name of
the Intel Corporation nor the names of its contributors may be used to
endorse or promote products derived from this software without
specific prior written permission.
 
THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE INTEL OR
ITS CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
END_LEGAL */
#include "pin.H"
#include <iostream>
#include <cstdlib>
#include <signal.h>
#include <list>
#include <string>

using std::cerr;
using std::endl;
using std::list;
using std::string;

extern "C" void ToolRaiseAccessInvalidAddressException(ADDRINT **addr, ADDRINT val);
extern "C" ADDRINT ToolCatchAccessInvalidAddressException;
extern "C" ADDRINT ToolIpAccessInvalidAddressException;

extern "C" ADDRINT ToolRaiseIntDivideByZeroException(ADDRINT catchAddr, ADDRINT code);
extern "C" ADDRINT ToolCatchIntDivideByZeroException;
extern "C" ADDRINT ToolIpIntDivideByZeroException;

enum ToolExceptionEnumeration { TEE_ACCESS_INVALID_ADDRESS, TEE_INT_DIV_ZERO };

struct ToolException {
    ADDRINT ip;
    ADDRINT fixIp;
    string name;
    bool* handled;
};

bool handledPinException = false;
bool handledToolAccessInvalidAddress = false;
bool handledToolIntDivZero = false;
bool handledApplicationDivZero = false;

ToolException toolExceptions[2];
list<ADDRINT> inslist;

// This is the tool's generic handler. It handles the excpetion defined in toolException.
static EXCEPT_HANDLING_RESULT toolHandler(ToolException* toolException, EXCEPTION_INFO* exptInfo,
                                          PHYSICAL_CONTEXT* pctxt)
{
    ADDRINT exptAddr = PIN_GetExceptionAddress(exptInfo);
    ADDRINT ip = PIN_GetPhysicalContextReg(pctxt, REG_INST_PTR);
    if (ip == toolException->ip) { // Only handle this exception.
        cerr << "TOOL: Identified " << toolException->name << " exception in the tool, fixing the problem..." << endl;
        PIN_SetPhysicalContextReg(pctxt, REG_INST_PTR, toolException->fixIp);
        if (exptAddr != ip) {
            cerr << hex << "TOOL ERROR: Exception address 0x" << exptAddr << " does not match ip 0x" << ip << endl;
        }
        else {
            *(toolException->handled) = true;
        }
        return EHR_HANDLED;
    }
    return EHR_UNHANDLED;
}

// Global handler for SIGFPE exceptions.
// This handler only deals with a specific int-div-zero exception, the one raised in "generateToolException()".
// All other exceptions are not handled here and are forwarded to the next level.
static EXCEPT_HANDLING_RESULT onException(THREADID tid, EXCEPTION_INFO* exptInfo, PHYSICAL_CONTEXT* pctxt, VOID* v) {
    return toolHandler(&toolExceptions[TEE_INT_DIV_ZERO], exptInfo, pctxt);
}

// Thread specific handler for PIN_Try exceptions.
// This handler only deals with a specific access-invalid-address exception, the one raised in "generateToolException()".
// All other exceptions are not handled here and are forwarded to the next level.
static EXCEPT_HANDLING_RESULT onTryException(THREADID tid, EXCEPTION_INFO* exptInfo, PHYSICAL_CONTEXT* pctxt, VOID* v) {
    return toolHandler(&toolExceptions[TEE_ACCESS_INVALID_ADDRESS], exptInfo, pctxt);
}

// Pin generates an access violation in SafeCopy (due to the given arguments).
// This should return an exception with a null exception address
static void generatePinException(ADDRINT address) {
    cerr << "TOOL: Generate pin exception" << endl;
    VOID* to = (VOID*)address;
    ADDRINT* from = 0;
    EXCEPTION_INFO exptInfo;
    int toCopy = 1024;
    if (PIN_SafeCopyEx(to, from, toCopy, &exptInfo) != toCopy) {
        if (PIN_GetExceptionCode(&exptInfo) != EXCEPTCODE_ACCESS_INVALID_ADDRESS) {
            cerr << "TOOL ERROR: PIN_SafeCopyEx returned with an unexpected exception code" << endl;
            cerr << PIN_ExceptionToString(&exptInfo) << endl;
            return;
        }
        ADDRINT exptAddr = PIN_GetExceptionAddress(&exptInfo);
        if (exptAddr != 0) {
            cerr << "TOOL ERROR: Exception from PIN_SafeCopyEx returned with non NULL address: 0x"
                 << hex << exptAddr << endl;
            return;
        }
        else {
            cerr << "TOOL: PIN_SafeCopyEx failed as expected, continue test..." << endl;
            handledPinException = true;
        }
    }
}

// Tool generates a div zero exception.
// This should return an exception address which equals the IP value.
// The address is in the analysis routine placed in the codecache.
static void generateToolException() {
    ToolRaiseIntDivideByZeroException(ToolCatchIntDivideByZeroException, 0);
    PIN_TryStart(PIN_ThreadId(), onTryException, 0);
    ADDRINT *addressPair[2];
    addressPair[0] = 0;
    addressPair[1] = (ADDRINT *)malloc(sizeof(ADDRINT));
    ToolRaiseAccessInvalidAddressException(addressPair, 0x12345);
    PIN_TryEnd(PIN_ThreadId());
}

// Catch a SIGFPE signal generated by a div zero in the application.
// This should return an exception address in the application's native code, not the instrumented code in the codecache.
// Fix the problem by advancing the IP to the next instruction - advance the IP in the context (native code) and let Pin
// translate to the actual address in the codecache.
static bool onSIGFPE(THREADID tid, INT32 sig, CONTEXT* ctxt, BOOL hasHandler, const EXCEPTION_INFO* exptInfo, VOID* v) {
    ADDRINT exptAddr = PIN_GetExceptionAddress(exptInfo);
    ADDRINT ip = PIN_GetContextReg(ctxt, REG_INST_PTR);
    list<ADDRINT>::const_iterator it = inslist.begin();
    list<ADDRINT>::const_iterator end = inslist.end();
    for (; it != end; ++it) {
        if (*it == ip) {
            cerr << "TOOL: Identified div zero instruction in the application, fixing the problem..." << endl;
            ++it; // Advance the iterator to the next instruction.
            PIN_SetContextReg(ctxt, REG_INST_PTR, *it); // Load the new IP.
            if (exptAddr == ip) {                
                handledApplicationDivZero = true;
            }
            else {
                cerr << hex << "TOOL ERROR: Exception address 0x" << exptAddr << " does not match ip 0x" << ip << endl;
            }
            return false;
        }
    }
    return true;
}

// Instrument the application functions - intentionally generate exceptions in the proper places and build the instList.
static VOID onRoutine(RTN rtn, VOID *) {
    if (RTN_Name(rtn).find("pinException") != string::npos) {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(generatePinException), IARG_ADDRINT, RTN_Address(rtn), IARG_END);
        RTN_Close(rtn);
    }
    
    else if (RTN_Name(rtn).find("toolException") != string::npos) {
        RTN_Open(rtn);
        RTN_InsertCall(rtn, IPOINT_AFTER, AFUNPTR(generateToolException), IARG_END);
        RTN_Close(rtn);
    }
    
    else if (RTN_Name(rtn).find("appException") != string::npos) {
        RTN_Open(rtn);
        
        // Build a list of all the instrucions in the routine.
        // The tool will use this to fix the div zero exception by advancing the IP to the next instruction.
        for (INS ins = RTN_InsHead(rtn); INS_Valid(ins); ins = INS_Next(ins)) {
            inslist.push_back(INS_Address(ins));
        }
        
        RTN_Close(rtn);
    }
}

// Check that all four scenarios were handled.
VOID Fini(INT32 code, VOID* v) {
    bool failed = false;
    if (!handledPinException) {
        cerr << "TOOL ERROR: PIN_SafeCopy exception was not handled properly in the test." << endl;
        failed = true;
    }
    if (!handledToolAccessInvalidAddress) {
        cerr << "TOOL ERROR: Tool access invalid address exception was not handled properly in the test." << endl;
        failed = true;
    }
    if (!handledToolIntDivZero) {
        cerr << "TOOL ERROR: Tool int div zero exception was not handled properly in the test." << endl;
        failed = true;
    }
    if (!handledApplicationDivZero) {
        cerr << "TOOL ERROR: Application div zero exception was not handled properly in the test." << endl;
        failed = true;
    }
    if (failed) {
        exit(-1);
    }
    else {
    	   cerr << "TOOL: Test completed successfully!" << endl;
    }
}

// Tool's main function
int main(int argc, char *argv[]) {
    PIN_InitSymbols();
    PIN_Init(argc, argv);
    
    toolExceptions[TEE_ACCESS_INVALID_ADDRESS].ip = reinterpret_cast < ADDRINT > (&ToolIpAccessInvalidAddressException);
    toolExceptions[TEE_ACCESS_INVALID_ADDRESS].fixIp = reinterpret_cast < ADDRINT > (&ToolCatchAccessInvalidAddressException);
    toolExceptions[TEE_ACCESS_INVALID_ADDRESS].name = "access invalid address";
    toolExceptions[TEE_ACCESS_INVALID_ADDRESS].handled = &handledToolAccessInvalidAddress;

    toolExceptions[TEE_INT_DIV_ZERO].ip = reinterpret_cast < ADDRINT > (&ToolIpIntDivideByZeroException);
    toolExceptions[TEE_INT_DIV_ZERO].fixIp = reinterpret_cast < ADDRINT > (&ToolCatchIntDivideByZeroException);
    toolExceptions[TEE_INT_DIV_ZERO].name = "int div zero";
    toolExceptions[TEE_INT_DIV_ZERO].handled = &handledToolIntDivZero;

    PIN_AddInternalExceptionHandler(onException, 0);
    RTN_AddInstrumentFunction(onRoutine, 0);
    PIN_InterceptSignal(SIGFPE, onSIGFPE, 0);
    PIN_AddFiniFunction(Fini, 0);

    PIN_StartProgram();
    return 0;
}