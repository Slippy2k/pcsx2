/*  PCSX2 - PS2 Emulator for PCs
 *  Copyright (C) 2002-2021  PCSX2 Dev Team
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
#include "Utilities/FixedPointTypes.inl"

#include "Config.h"
#include "GS.h"

const wxChar* const tbl_SpeedhackNames[] =
{
	L"mvuFlag",
	L"InstantVU1" };

const __fi wxChar* EnumToString(SpeedhackId id)
{
	return tbl_SpeedhackNames[id];
}

void Pcsx2Config::SpeedhackOptions::Set(SpeedhackId id, bool enabled)
{
	EnumAssert(id);
	switch (id)
	{
		case Speedhack_mvuFlag:
			vuFlagHack = enabled;
			break;
		case Speedhack_InstantVU1:
			vu1Instant = enabled;
			break;
		default:
			break;
	}
}

Pcsx2Config::SpeedhackOptions::SpeedhackOptions()
{
	DisableAll();
	
	// Set recommended speedhacks to enabled by default. They'll still be off globally on resets.
	WaitLoop = true;
	IntcStat = true;
	vuFlagHack = true;
	vu1Instant = true;
}

Pcsx2Config::SpeedhackOptions& Pcsx2Config::SpeedhackOptions::DisableAll()
{
	bitset			= 0;
	EECycleRate		= 0;
	EECycleSkip		= 0;
	
	return *this;
}


Pcsx2Config::RecompilerOptions::RecompilerOptions()
{
	bitset		= 0;

	// All recs are enabled by default.

	EnableEE	= true;
	EnableIOP	= true;
	EnableVU0	= true;
	EnableVU1	= true;

	// vu and fpu clamping default to standard overflow.
	vuOverflow	= true;
	//vuExtraOverflow = false;
	//vuSignOverflow = false;
	//vuUnderflow = false;

	fpuOverflow	= true;
	//fpuExtraOverflow = false;
	//fpuFullMode = false;
}

void Pcsx2Config::RecompilerOptions::ApplySanityCheck()
{
	bool fpuIsRight = true;

	if( fpuExtraOverflow )
		fpuIsRight = fpuOverflow;

	if( fpuFullMode )
		fpuIsRight = fpuOverflow && fpuExtraOverflow;

	if( !fpuIsRight )
	{
		// Values are wonky; assume the defaults.
		fpuOverflow		= RecompilerOptions().fpuOverflow;
		fpuExtraOverflow= RecompilerOptions().fpuExtraOverflow;
		fpuFullMode		= RecompilerOptions().fpuFullMode;
	}

	bool vuIsOk = true;

	if( vuExtraOverflow ) vuIsOk = vuIsOk && vuOverflow;
	if( vuSignOverflow ) vuIsOk = vuIsOk && vuExtraOverflow;

	if( !vuIsOk )
	{
		// Values are wonky; assume the defaults.
		vuOverflow		= RecompilerOptions().vuOverflow;
		vuExtraOverflow	= RecompilerOptions().vuExtraOverflow;
		vuSignOverflow	= RecompilerOptions().vuSignOverflow;
		vuUnderflow		= RecompilerOptions().vuUnderflow;
	}
}


Pcsx2Config::CpuOptions::CpuOptions()
{
	sseMXCSR.bitmask	= DEFAULT_sseMXCSR;
	sseVUMXCSR.bitmask	= DEFAULT_sseVUMXCSR;
}

void Pcsx2Config::CpuOptions::ApplySanityCheck()
{
	sseMXCSR.ClearExceptionFlags().DisableExceptions();
	sseVUMXCSR.ClearExceptionFlags().DisableExceptions();

	Recompiler.ApplySanityCheck();
}


// Default GSOptions
Pcsx2Config::GSOptions::GSOptions()
{
	FrameSkipEnable			= false;

	VsyncQueueSize			= 2;

	FramesToDraw			= 2;
	FramesToSkip			= 2;

	FramerateNTSC			= 59.94;
	FrameratePAL			= 50.0;
}


const wxChar *const tbl_GamefixNames[] =
{
	L"VuAddSub",
	L"FpuCompare",
	L"FpuMul",
	L"FpuNegDiv",
	L"XGKick",
	L"IPUWait",
	L"EETiming",
	L"SkipMPEG",
	L"OPHFlag",
	L"DMABusy",
	L"VIFFIFO",
	L"VIF1Stall",
	L"GIFFIFO",
	L"FMVinSoftware",
	L"GoemonTlb",
	L"Ibit",
	L"VUKickstart"
};

const __fi wxChar* EnumToString( GamefixId id )
{
	return tbl_GamefixNames[id];
}

// all gamefixes are disabled by default.
Pcsx2Config::GamefixOptions::GamefixOptions()
{
	DisableAll();
}

Pcsx2Config::GamefixOptions& Pcsx2Config::GamefixOptions::DisableAll()
{
	bitset = 0;
	return *this;
}

// Enables a full list of gamefixes.  The list can be either comma or pipe-delimited.
//   Example:  "XGKick,IpuWait"  or  "EEtiming,FpuCompare"
// If an unrecognized tag is encountered, a warning is printed to the console, but no error
// is generated.  This allows the system to function in the event that future versions of
// PCSX2 remove old hacks once they become obsolete.
void Pcsx2Config::GamefixOptions::Set( const wxString& list, bool enabled )
{
	wxStringTokenizer izer( list, L",|", wxTOKEN_STRTOK );
	
	while( izer.HasMoreTokens() )
	{
		wxString token( izer.GetNextToken() );

		GamefixId i;
		for (i=GamefixId_FIRST; i < pxEnumEnd; ++i)
		{
			if( token.CmpNoCase( EnumToString(i) ) == 0 ) break;
		}
		if( i < pxEnumEnd ) Set( i );
	}
}

void Pcsx2Config::GamefixOptions::Set( GamefixId id, bool enabled )
{
	EnumAssert( id );
	switch(id)
	{
		case Fix_VuAddSub:		VuAddSubHack		= enabled;	break;
		case Fix_FpuCompare:	FpuCompareHack		= enabled;	break;
		case Fix_FpuMultiply:	FpuMulHack			= enabled;	break;
		case Fix_FpuNegDiv:		FpuNegDivHack		= enabled;	break;
		case Fix_XGKick:		XgKickHack			= enabled;	break;
		case Fix_IpuWait:		IPUWaitHack			= enabled;	break;
		case Fix_EETiming:		EETimingHack		= enabled;	break;
		case Fix_SkipMpeg:		SkipMPEGHack		= enabled;	break;
		case Fix_OPHFlag:		OPHFlagHack			= enabled;  break;
		case Fix_DMABusy:		DMABusyHack			= enabled;  break;
		case Fix_VIFFIFO:		VIFFIFOHack			= enabled;  break;
		case Fix_VIF1Stall:		VIF1StallHack		= enabled;  break;
		case Fix_GIFFIFO:		GIFFIFOHack			= enabled;  break;
		case Fix_FMVinSoftware:	FMVinSoftwareHack	= enabled;  break;
		case Fix_GoemonTlbMiss: GoemonTlbHack		= enabled;  break;
		case Fix_Ibit:  IbitHack        = enabled;  break;
		case Fix_VUKickstart:	VUKickstartHack	= enabled; break;
		default:
					break;
	}
}

bool Pcsx2Config::GamefixOptions::Get( GamefixId id ) const
{
	EnumAssert( id );
	switch(id)
	{
		case Fix_VuAddSub:		return VuAddSubHack;
		case Fix_FpuCompare:	return FpuCompareHack;
		case Fix_FpuMultiply:	return FpuMulHack;
		case Fix_FpuNegDiv:		return FpuNegDivHack;
		case Fix_XGKick:		return XgKickHack;
		case Fix_IpuWait:		return IPUWaitHack;
		case Fix_EETiming:		return EETimingHack;
		case Fix_SkipMpeg:		return SkipMPEGHack;
		case Fix_OPHFlag:		return OPHFlagHack;
		case Fix_DMABusy:		return DMABusyHack;
		case Fix_VIFFIFO:		return VIFFIFOHack;
		case Fix_VIF1Stall:		return VIF1StallHack;
		case Fix_GIFFIFO:		return GIFFIFOHack;
		case Fix_FMVinSoftware:	return FMVinSoftwareHack;
		case Fix_GoemonTlbMiss: return GoemonTlbHack;
		case Fix_Ibit:  return IbitHack;
		case Fix_VUKickstart:	return VUKickstartHack;
		default:
					break;
	}
	return false;		// unreachable, but we still need to suppress warnings >_<
}


Pcsx2Config::Pcsx2Config()
{
	bitset = 0;
	// Set defaults for fresh installs / reset settings
	McdEnableEjection = true;
	McdFolderAutoManage = true;
	EnablePatches = true;
}


bool Pcsx2Config::MultitapEnabled( uint port ) const
{
	pxAssert( port < 2 );
	return (port==0) ? MultitapPort0_Enabled : MultitapPort1_Enabled;
}

