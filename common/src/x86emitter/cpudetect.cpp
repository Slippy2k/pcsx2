/*  Cpudetection lib
 *  Copyright (C) 2002-2010  PCSX2 Dev Team
 *
 *  PCSX2 is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU Lesser General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  PCSX2 is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with PCSX2.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "PrecompiledHeader.h"
#include "Utilities/RedtapeWindows.h"
#include "x86emitter/tools.h"
#include "internal.h"
#include "x86_intrin.h"

// CPU information support
#if defined(_WIN32)

#define cpuid __cpuid
#define cpuidex __cpuidex

#else

#include <cpuid.h>

static __inline__ __attribute__((always_inline)) void cpuidex(int CPUInfo[], const int InfoType, const int count)
{
    __cpuid_count(InfoType, count, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
}

static __inline__ __attribute__((always_inline)) void cpuid(int CPUInfo[], const int InfoType)
{
    __cpuid(InfoType, CPUInfo[0], CPUInfo[1], CPUInfo[2], CPUInfo[3]);
}

#endif

using namespace x86Emitter;

__aligned16 x86capabilities x86caps;

x86capabilities::x86capabilities()
    : isIdentified(false)
    , VendorID(x86Vendor_Unknown)
    , FamilyID(0)
    , Model(0)
    , TypeID(0)
    , StepID(0)
    , Flags(0)
    , Flags2(0)
    , EFlags(0)
    , EFlags2(0)
    , SEFlag(0)
    , AllCapabilities(0)
{
    memzero(VendorName);
    memzero(FamilyName);
}

// Warning!  We've had problems with the MXCSR detection code causing stack corruption in
// MSVC PGO builds.  The problem was fixed when I moved the MXCSR code to this function, and
// moved the recSSE[] array to a global static (it was local to cpudetectInit).  Commented
// here in case the nutty crash ever re-surfaces. >_<
// Note: recSSE was deleted
void x86capabilities::SIMD_EstablishMXCSRmask()
{
    if (!hasStreamingSIMDExtensions)
        return;

    MXCSR_Mask.bitmask = 0xFFBF; // MMX/SSE default

    if (hasStreamingSIMD2Extensions) {
        // This is generally safe assumption, but FXSAVE is the "correct" way to
        // detect MXCSR masking features of the cpu, so we use it's result below
        // and override this.

        MXCSR_Mask.bitmask = 0xFFFF; // SSE2 features added
    }

    __aligned16 u8 targetFXSAVE[512];

    // Work for recent enough GCC/CLANG/MSVC 2012
    _fxsave(&targetFXSAVE);

    u32 result;
    memcpy(&result, &targetFXSAVE[28], 4); // bytes 28->32 are the MXCSR_Mask.
    if (result != 0)
        MXCSR_Mask.bitmask = result;
}

static const char *tbl_x86vendors[] =
    {
        "GenuineIntel",
        "AuthenticAMD",
        "Unknown     ",
};

// Performs all _cpuid-related activity.  This fills *most* of the x86caps structure, except for
// the cpuSpeed and the mxcsr masks.  Those must be completed manually.
void x86capabilities::Identify()
{
    if (isIdentified)
        return;
    isIdentified = true;

    s32 regs[4];
    u32 cmds;

//AMD 64 STUFF
#ifdef __M_X86_64
    u32 x86_64_8BITBRANDID;
    u32 x86_64_12BITBRANDID;
#endif

    memzero(VendorName);
    cpuid(regs, 0);

    cmds = regs[0];
    memcpy(&VendorName[0], &regs[1], 4);
    memcpy(&VendorName[4], &regs[3], 4);
    memcpy(&VendorName[8], &regs[2], 4);

    // Determine Vendor Specifics!
    // It's really not recommended that we base much (if anything) on CPU vendor names,
    // however it's currently necessary in order to gain a (pseudo)reliable count of cores
    // and threads used by the CPU (AMD and Intel can't agree on how to make this info available).

    int vid;
    for (vid = 0; vid < x86Vendor_Unknown; ++vid) {
        if (memcmp(VendorName, tbl_x86vendors[vid], 12) == 0)
            break;
    }
    VendorID = static_cast<x86VendorType>(vid);

    if (cmds >= 0x00000001) {
        cpuid(regs, 0x00000001);

        StepID = regs[0] & 0xf;
        Model = (regs[0] >> 4) & 0xf;
        FamilyID = (regs[0] >> 8) & 0xf;
        TypeID = (regs[0] >> 12) & 0x3;
#ifdef __M_X86_64
        x86_64_8BITBRANDID = regs[1] & 0xff;
#endif
        Flags = regs[3];
        Flags2 = regs[2];
    }

    if (cmds >= 0x00000007) {
        // Note: ECX must be 0 for AVX2 detection.
        cpuidex(regs, 0x00000007, 0);

        SEFlag = regs[1];
    }

    cpuid(regs, 0x80000000);
    cmds = regs[0];
    if (cmds >= 0x80000001) {
        cpuid(regs, 0x80000001);

#ifdef __M_X86_64
        x86_64_12BITBRANDID = regs[1] & 0xfff;
#endif
        EFlags2 = regs[2];
        EFlags = regs[3];
    }

    memzero(FamilyName);
    cpuid((int *)FamilyName, 0x80000002);
    cpuid((int *)(FamilyName + 16), 0x80000003);
    cpuid((int *)(FamilyName + 32), 0x80000004);

    hasFloatingPointUnit = (Flags >> 0) & 1;
    hasVirtual8086ModeEnhancements = (Flags >> 1) & 1;
    hasDebuggingExtensions = (Flags >> 2) & 1;
    hasPageSizeExtensions = (Flags >> 3) & 1;
    hasTimeStampCounter = (Flags >> 4) & 1;
    hasModelSpecificRegisters = (Flags >> 5) & 1;
    hasPhysicalAddressExtension = (Flags >> 6) & 1;
    hasMachineCheckArchitecture = (Flags >> 7) & 1;
    hasCOMPXCHG8BInstruction = (Flags >> 8) & 1;
    hasAdvancedProgrammableInterruptController = (Flags >> 9) & 1;
    hasSEPFastSystemCall = (Flags >> 11) & 1;
    hasMemoryTypeRangeRegisters = (Flags >> 12) & 1;
    hasPTEGlobalFlag = (Flags >> 13) & 1;
    hasMachineCheckArchitecture = (Flags >> 14) & 1;
    hasConditionalMoveAndCompareInstructions = (Flags >> 15) & 1;
    hasFGPageAttributeTable = (Flags >> 16) & 1;
    has36bitPageSizeExtension = (Flags >> 17) & 1;
    hasProcessorSerialNumber = (Flags >> 18) & 1;
    hasCFLUSHInstruction = (Flags >> 19) & 1;
    hasDebugStore = (Flags >> 21) & 1;
    hasACPIThermalMonitorAndClockControl = (Flags >> 22) & 1;
    hasFastStreamingSIMDExtensionsSaveRestore = (Flags >> 24) & 1;
    hasStreamingSIMDExtensions = (Flags >> 25) & 1;  //sse
    hasStreamingSIMD2Extensions = (Flags >> 26) & 1; //sse2
    hasSelfSnoop = (Flags >> 27) & 1;
    hasThermalMonitor = (Flags >> 29) & 1;
    hasIntel64BitArchitecture = (Flags >> 30) & 1;

    // -------------------------------------------------
    // --> SSE3 / SSSE3 / SSE4.1 / SSE 4.2 detection <--
    // -------------------------------------------------

    hasStreamingSIMD3Extensions = (Flags2 >> 0) & 1;             //sse3
    hasSupplementalStreamingSIMD3Extensions = (Flags2 >> 9) & 1; //ssse3
    hasStreamingSIMD4Extensions = (Flags2 >> 19) & 1;            //sse4.1
    hasStreamingSIMD4Extensions2 = (Flags2 >> 20) & 1;           //sse4.2

    if ((Flags2 >> 27) & 1) // OSXSAVE
    {
        // Note: In theory, we should use xgetbv to check OS support
        // but all OSes we officially run under support it
        // and its intrinsic requires extra compiler flags
        hasAVX = (Flags2 >> 28) & 1; //avx
        hasFMA = (Flags2 >> 12) & 1; //fma
        hasAVX2 = (SEFlag >> 5) & 1; //avx2
    }

    hasBMI1 = (SEFlag >> 3) & 1;
    hasBMI2 = (SEFlag >> 8) & 1;

    // Ones only for AMDs:
    hasAMD64BitArchitecture = (EFlags >> 29) & 1;      //64bit cpu
    hasStreamingSIMD4ExtensionsA = (EFlags2 >> 6) & 1; //INSERTQ / EXTRQ / MOVNT

    isIdentified = true;
}
