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
#include <stdio.h>
#include <stdlib.h>
#include "pin.H"

/*
This tool is used in conjunction with  set_fp_context_ymm_regs_app.cpp applictaion
- the test verifies that the ymm registers can be set for the application by
  PIN_CallApplicationFunction,  PIN_ExecuteAt and the function registered by PIN_AddContextChangeFunction
  that is invoked when an exception or non-fatal (i.e. handled) signal oocurs.
  Note that the thre callback function registered by the PIN_AddThreadStartFunction, can also change the ymm
  registers, however since there is much code executed before the actual application Thread main function is
  invoked, they will likely change before reaching there
*/


#ifdef TARGET_IA32E
#define NUM_YMM_REGS 16
#else
#define NUM_YMM_REGS 8
#endif

KNOB<BOOL> KnobUseIargConstContext(KNOB_MODE_WRITEONCE, "pintool",
                                   "const_context", "0", "use IARG_CONST_CONTEXT");

ADDRINT executeAtAddr = 0;
ADDRINT dumpYmmRegsAtExceptionAddr = 0;
union /*<POD>*/ XMMREG
{
    UINT8 _vec8[16];      ///< Vector of 16 8-bit elements.
    UINT16 _vec16[8];     ///< Vector of 8 16-bit elements.
    UINT32 _vec32[4];     ///< Vector of 4 32-bit elements.
    UINT64 _vec64[2];     ///< Vector of 2 64-bit elements.
};

VOID REPLACE_ReplacedYmmRegs(CONTEXT *context, THREADID tid, AFUNPTR originalFunction)
{
    printf ("TOOL in REPLACE_ReplacedYmmRegs\n");
    fflush (stdout);

    CONTEXT writableContext, *ctxt;
    if (KnobUseIargConstContext)
    { // need to copy the ctxt into a writable context
        PIN_SaveContext(context, &writableContext);
        ctxt = &writableContext;
    }
    else
    {
        ctxt = context;
    }

    /* set the ymm regs in the ctxt which is used to execute the 
       originalFunction (via PIN_CallApplicationFunction) */
    CHAR fpContextSpaceForFpConextFromPin[FPSTATE_SIZE];
    FPSTATE *fpContextFromPin = reinterpret_cast<FPSTATE *>(fpContextSpaceForFpConextFromPin);
        
    PIN_GetContextFPState(ctxt, fpContextFromPin);
    for (int i=0; i<NUM_YMM_REGS; i++)
    {
        fpContextFromPin->fxsave_legacy._xmms[i]._vec32[0] = 0xacdcacdc;
        fpContextFromPin->fxsave_legacy._xmms[i]._vec32[1] = 0xacdcacdc;
        fpContextFromPin->fxsave_legacy._xmms[i]._vec32[2] = 0xacdcacdc;
        fpContextFromPin->fxsave_legacy._xmms[i]._vec32[3] = 0xacdcacdc;
        
    }
	int j =0;
	for (int i=0; i<NUM_YMM_REGS; i++)
	{
		(reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[j])))->_vec32[0] = 0xacdcacdc;
		(reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[j])))->_vec32[1] = 0xacdcacdc;
		(reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[j])))->_vec32[2] = 0xacdcacdc;
		(reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[j])))->_vec32[3] = 0xacdcacdc;
		j+=16;
	}

    PIN_SetContextFPState(ctxt, fpContextFromPin);

    // verify the ymm regs were set in the ctxt
    CHAR fpContextSpaceForFpConextFromPin2[FPSTATE_SIZE];
    FPSTATE *fpContextFromPin2 = reinterpret_cast<FPSTATE *>(fpContextSpaceForFpConextFromPin2);
    PIN_GetContextFPState(ctxt, fpContextFromPin2);
	j = 0;
    for (int i=0; i<NUM_YMM_REGS; i++)
    {
        if ((fpContextFromPin->fxsave_legacy._xmms[i]._vec64[0] !=fpContextFromPin2->fxsave_legacy._xmms[i]._vec64[0]) ||
            (fpContextFromPin->fxsave_legacy._xmms[i]._vec64[1] !=fpContextFromPin2->fxsave_legacy._xmms[i]._vec64[1]) ||
            ((reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[i])))->_vec64[0] !=
             (reinterpret_cast<XMMREG*>(&(fpContextFromPin2->_xstate._ymmUpper[i])))->_vec64[0]) ||
            ((reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[i])))->_vec64[1] !=
             (reinterpret_cast<XMMREG*>(&(fpContextFromPin2->_xstate._ymmUpper[i])))->_vec64[1])
        )
        {
            printf("TOOL ERROR at ymm[%d]  (%lx %lx %lx %lx) (%lx %lx %lx %lx) (%lx %lx %lx %lx) (%lx %lx %lx %lx)\n", i,
                (unsigned long)fpContextFromPin->fxsave_legacy._xmms[i]._vec32[0],
                (unsigned long)fpContextFromPin->fxsave_legacy._xmms[i]._vec32[1],
                (unsigned long)fpContextFromPin->fxsave_legacy._xmms[i]._vec32[2],
                (unsigned long)fpContextFromPin->fxsave_legacy._xmms[i]._vec32[3],
                (unsigned long)fpContextFromPin2->fxsave_legacy._xmms[i]._vec32[0],
                (unsigned long)fpContextFromPin2->fxsave_legacy._xmms[i]._vec32[1],
                (unsigned long)fpContextFromPin2->fxsave_legacy._xmms[i]._vec32[2],
                (unsigned long)fpContextFromPin2->fxsave_legacy._xmms[i]._vec32[3],
                (reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[j])))->_vec32[0],
                (reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[j])))->_vec32[1],
                (reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[j])))->_vec32[2],
                (reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[j])))->_vec32[3],
                (reinterpret_cast<XMMREG*>(&(fpContextFromPin2->_xstate._ymmUpper[j])))->_vec32[0],
                (reinterpret_cast<XMMREG*>(&(fpContextFromPin2->_xstate._ymmUpper[j])))->_vec32[1],
                (reinterpret_cast<XMMREG*>(&(fpContextFromPin2->_xstate._ymmUpper[j])))->_vec32[2],
                (reinterpret_cast<XMMREG*>(&(fpContextFromPin2->_xstate._ymmUpper[j])))->_vec32[3]
                );
            exit (-1);
			
        }
		j+=16;
    }

    // call the originalFunction function with the ymm regs set from above
    printf("TOOL Calling replaced ReplacedYmmRegs()\n");
    fflush (stdout);
    PIN_CallApplicationFunction(ctxt, tid, CALLINGSTD_DEFAULT, 
                                originalFunction, PIN_PARG_END());
    printf("TOOL Returned from replaced ReplacedYmmRegs()\n");
    fflush (stdout);

    if (executeAtAddr != 0)
    {
        // set ymm regs to other values
        for (int i=0; i<NUM_YMM_REGS; i++)
        {
            fpContextFromPin->fxsave_legacy._xmms[i]._vec32[0] = 0xdeadbeef;
            fpContextFromPin->fxsave_legacy._xmms[i]._vec32[1] = 0xdeadbeef;
            fpContextFromPin->fxsave_legacy._xmms[i]._vec32[2] = 0xdeadbeef;
            fpContextFromPin->fxsave_legacy._xmms[i]._vec32[3] = 0xdeadbeef;
            
        }
		int j =0;
		for (int i=0; i<NUM_YMM_REGS; i++)
		{
			(reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[j])))->_vec32[0] = 0xdeadbeef;
			(reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[j])))->_vec32[1] = 0xdeadbeef;
			(reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[j])))->_vec32[2] = 0xdeadbeef;
			(reinterpret_cast<XMMREG*>(&(fpContextFromPin->_xstate._ymmUpper[j])))->_vec32[3] = 0xdeadbeef;
			j+=16;
		}

        PIN_SetContextFPState(ctxt, fpContextFromPin);
        // execute the application function ExecuteAtFunc with the ymm regs set
        PIN_SetContextReg(ctxt, REG_INST_PTR, executeAtAddr);
        printf("TOOL Calling ExecutedAtFunc\n");
        fflush (stdout);
        PIN_ExecuteAt (ctxt);
        printf("TOOL returned from ExecutedAtFunc\n");
        fflush (stdout);
    }
}



VOID Image(IMG img, void *v)
{
    RTN rtn = RTN_FindByName(img, "ReplacedYmmRegs");
    if (RTN_Valid(rtn))
    {
        PROTO proto = PROTO_Allocate(PIN_PARG(int), CALLINGSTD_DEFAULT, "ReplacedYmmRegs", PIN_PARG_END());
        RTN_ReplaceSignature(rtn, AFUNPTR(REPLACE_ReplacedYmmRegs),
            IARG_PROTOTYPE, proto,
            (KnobUseIargConstContext)?IARG_CONST_CONTEXT:IARG_CONTEXT,
            IARG_THREAD_ID,
            IARG_ORIG_FUNCPTR,
            IARG_END);
        PROTO_Free(proto);
        printf ("TOOL found and replaced ReplacedYmmRegs\n");
        fflush (stdout);

        RTN rtn = RTN_FindByName(img, "ExecutedAtFunc");
        if (RTN_Valid(rtn))
        {
            executeAtAddr = RTN_Address(rtn);
            printf ("TOOL found ExecutedAtFunc for later PIN_ExecuteAt\n");
            fflush (stdout);
        }

        rtn = RTN_FindByName(img, "DumpYmmRegsAtException");
        if (RTN_Valid(rtn))
        {
            dumpYmmRegsAtExceptionAddr = RTN_Address(rtn);
            printf ("TOOL found DumpYmmRegsAtException for later Exception\n");
            fflush (stdout);
        }
    }
}




void CheckAndSetFpContextYmmRegs (const CONTEXT *ctxtFrom, CONTEXT *ctxtTo)
{
    fprintf (stdout, "TOOL CheckAndSetFpContextYmmRegs\n");
    fflush (stdout);
    CHAR fpContextSpaceForFpConextFromPin[FPSTATE_SIZE];
    FPSTATE *fpContextFromPin = reinterpret_cast<FPSTATE *>(fpContextSpaceForFpConextFromPin);
    
    // the application set the each byte in the ymm regs in the state to be 0xa5 before the exception was caused
    PIN_GetContextFPState(ctxtFrom, fpContextFromPin);
    for (int i=0; i<NUM_YMM_REGS; i++)
    {
        for (int j=0; j<16; j++)
        {
            if (fpContextFromPin->fxsave_legacy._xmms[i]._vec8[j] != 0xa5)
            {
                fprintf (stdout, "TOOL unexpected _ymm[%d]._vec8[%d] value %x\n", i, j,
                    (unsigned int)fpContextFromPin->fxsave_legacy._xmms[i]._vec8[j]);
                fflush (stdout);
                exit (-1);
            }
        }
    }
    fprintf (stdout, "TOOL Checked ctxtFrom OK\n");
    fflush (stdout);


    // the tool now sets the each byte of the ymm regs in the ctxtTo to be 0x5a 
    for (int i=0; i<NUM_YMM_REGS; i++)
    {
        for (int j=0; j<16; j++)
        {
            fpContextFromPin->fxsave_legacy._xmms[i]._vec8[j] = 0x5a;
        }
    }
    PIN_SetContextFPState(ctxtTo, fpContextFromPin);
    
    // verify the setting worked
    for (int i=0; i<NUM_YMM_REGS; i++)
    {
        fpContextFromPin->fxsave_legacy._xmms[i]._vec64[0] = 0x0;
        fpContextFromPin->fxsave_legacy._xmms[i]._vec64[1] = 0x0;
    }
    PIN_GetContextFPState(ctxtTo, fpContextFromPin);
    for (int i=0; i<NUM_YMM_REGS; i++)
    {
        for (int j=0; j<16; j++)
        {
            if (fpContextFromPin->fxsave_legacy._xmms[i]._vec8[j] != 0x5a)
            {
                printf ("TOOL ERROR\n");
                fflush (stdout);
                exit (-1);
            }
        }
    }
    printf ("TOOL Checked ctxtTo OK\n");
    fflush (stdout);

    // application will verify that actual ymm registers contain 0x5a in each byte
}


// Special stack alignment ( n mod 16 ) at callee entry point, after return address has been pushed on the stack.
// n == 0 means no special alignment, e.g. regular void* alignment
// n == 16 means alignment on 16
// (reference document http://www.agner.org/optimize/calling_conventions.pdf)
#if defined(TARGET_IA32E)
LOCALVAR const INT32 StackEntryAlignment = 8;
#elif defined(TARGET_LINUX) || defined(TARGET_MAC) || defined(TARGET_BSD)
LOCALVAR const INT32 StackEntryAlignment = 12;
#else
LOCALVAR const INT32 StackEntryAlignment = 0;
#endif


INT32 GetStackAdjustment(INT32 currentAlignment, INT32 frameSize)
{
    if ((StackEntryAlignment == 0) || (currentAlignment == 0))
    {
        // No special alignment requirements, so stack adjustment is not required
        return 0;
    }

    // Compute adjustment after frame is allocated
    
    INT32 adjustment = (currentAlignment - frameSize - StackEntryAlignment) % 16;

    if (adjustment < 0)
    {
        // adjustment > -16
        adjustment = 16 + adjustment;
    }
    return adjustment;
}

#define NUM_XMM_REGS 8
VOID GetFpContext(const CONTEXT * context)
{
    CHAR fpContext[FPSTATE_SIZE];
    PIN_GetContextFPState(context, reinterpret_cast<VOID *>(fpContext));
    

    FPSTATE *fpVerboseContext;
    fpVerboseContext = reinterpret_cast <FPSTATE *>(fpContext);

    printf ("tool: xmm regs in context\n");
    for (int i=0; i<NUM_XMM_REGS; i++)
    {
        printf ("tool: xmm[%d] %x %x %x %x\n", i, fpVerboseContext->fxsave_legacy._xmms[i]._vec32[3],
                fpVerboseContext->fxsave_legacy._xmms[i]._vec32[2], fpVerboseContext->fxsave_legacy._xmms[i]._vec32[1],
                fpVerboseContext->fxsave_legacy._xmms[i]._vec32[0]);
    }
    fflush(stdout);
}

// this function verifies that the ymm regs in the ctxtFrom are as they were set in the app just before the
// exception occurs. Then it sets the ymm regs in the ctxtTo to a different value, finally it causes the
// execution to continue in the application function DumpYmmRegsAtException
static void OnException(THREADID threadIndex, 
                  CONTEXT_CHANGE_REASON reason, 
                  const CONTEXT *ctxtFrom,
                  CONTEXT *ctxtTo,
                  INT32 info, 
                  VOID *v)
{
    if (CONTEXT_CHANGE_REASON_SIGRETURN == reason || CONTEXT_CHANGE_REASON_APC == reason
        || CONTEXT_CHANGE_REASON_CALLBACK == reason || CONTEXT_CHANGE_REASON_FATALSIGNAL == reason
        || ctxtTo == NULL)
    { // don't want to handle these
        return;
    }
    fprintf (stdout, "TOOL OnException callback\n");
    fflush (stdout);
    

    printf ("\ntool: ctxtFrom\n");
    GetFpContext (ctxtFrom);
    printf ("\ntool: ctxtTo\n");
    GetFpContext (ctxtTo);


    //PIN_SaveContext(ctxtFrom, ctxtTo);
    CheckAndSetFpContextYmmRegs(ctxtFrom, ctxtTo);
    printf ("\ntool: ctxtTo\n");
    GetFpContext (ctxtTo);
    
    
    
    // call the application function with the ctxtTo context
#ifdef TARGET_IA32E
    PIN_SetContextReg(ctxtTo, REG_RIP, dumpYmmRegsAtExceptionAddr);
    // take care of stack alignment
    ADDRINT curSp = PIN_GetContextReg(ctxtTo, REG_RSP);
    INT32 currentAlignment = curSp % 16;
    PIN_SetContextReg(ctxtTo, REG_RSP, curSp - GetStackAdjustment(currentAlignment, sizeof (ADDRINT)));
#else
    PIN_SetContextReg(ctxtTo, REG_EIP, dumpYmmRegsAtExceptionAddr);
#endif
}

extern "C" int SetXmmScratchesFun();

VOID Trace (TRACE trace, VOID *v)
{
    for (BBL bbl = TRACE_BblHead(trace); BBL_Valid(bbl); bbl = BBL_Next(bbl))
    {
        for (INS ins = BBL_InsHead(bbl); INS_Valid(ins); ins = INS_Next(ins))
        {
            xed_iclass_enum_t iclass1 = static_cast<xed_iclass_enum_t>(INS_Opcode(ins));
            if (iclass1 == XED_ICLASS_FLD1 && INS_Valid(INS_Next(ins)))
            {
                xed_iclass_enum_t iclass2 = static_cast<xed_iclass_enum_t>(INS_Opcode(INS_Next(ins)));
                if (iclass2 == XED_ICLASS_FLD1 && INS_Valid(INS_Next(INS_Next(ins))))
                {
                    xed_iclass_enum_t iclass3 = static_cast<xed_iclass_enum_t>(INS_Opcode(INS_Next(INS_Next(ins))));
                    if (iclass3 == XED_ICLASS_FLD1)
                    {
                        printf ("tool: found fld1 sequence at %p\n", (void *)INS_Address(INS_Next(INS_Next(ins))));
                        fflush (stdout);
                        // Insert an analysis call that will cause the xmm scratch registers to be spilled
                        INS_InsertCall(INS_Next(INS_Next(ins)), IPOINT_AFTER, (AFUNPTR)SetXmmScratchesFun, IARG_END);
                        return;
                    }
                }
            }
        }
    }
}

extern "C" unsigned int xmmInitVals[];
unsigned int xmmInitVals[64];

int main(int argc, char **argv)
{
    // initialize memory area used to set values in ymm regs
    for (int i =0; i<64; i++)
    {
        xmmInitVals[i] = 0x12345678;
    }

    PIN_Init(argc, argv);
    PIN_InitSymbols();

    IMG_AddInstrumentFunction(Image, 0);
    PIN_AddContextChangeFunction(OnException, 0);
    TRACE_AddInstrumentFunction(Trace, 0);

    PIN_StartProgram();
    return 0;
}
