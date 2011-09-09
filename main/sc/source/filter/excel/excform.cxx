/*************************************************************************
 *
 * DO NOT ALTER OR REMOVE COPYRIGHT NOTICES OR THIS FILE HEADER.
 * 
 * Copyright 2000, 2010 Oracle and/or its affiliates.
 *
 * OpenOffice.org - a multi-platform office productivity suite
 *
 * This file is part of OpenOffice.org.
 *
 * OpenOffice.org is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License version 3
 * only, as published by the Free Software Foundation.
 *
 * OpenOffice.org is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License version 3 for more details
 * (a copy is included in the LICENSE file that accompanied this code).
 *
 * You should have received a copy of the GNU Lesser General Public License
 * version 3 along with OpenOffice.org.  If not, see
 * <http://www.openoffice.org/license.html>
 * for a copy of the LGPLv3 License.
 *
 ************************************************************************/

// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_sc.hxx"
#include "excform.hxx"
#include <osl/endian.h>

#include "cell.hxx"
#include "document.hxx"
#include "rangenam.hxx"
#include "global.hxx"
#include "formula/errorcodes.hxx"

#include "imp_op.hxx"
#include "root.hxx"
#include "xltracer.hxx"
#include "xihelper.hxx"
#include "xilink.hxx"
#include "xiname.hxx"

using ::std::vector;

const sal_uInt16 ExcelToSc::nRowMask = 0x3FFF;
const sal_uInt16 ExcelToSc::nLastInd = 399;




void ImportExcel::Formula25()
{
    XclAddress aXclPos;
    sal_uInt16  nXF = 0, nFormLen;
	double	fCurVal;
	sal_uInt8	nFlag0;
	sal_Bool	bShrFmla;

    aIn >> aXclPos;

    if( GetBiff() == EXC_BIFF2 )
	{//						BIFF2
		sal_uInt8 nDummy;

        aIn.Ignore( 3 );

		aIn >> fCurVal;
        aIn.Ignore( 1 );
		aIn >> nDummy;
		nFormLen = nDummy;
		bShrFmla = sal_False;
	}
	else
	{//						BIFF5
		aIn >> nXF >> fCurVal >> nFlag0;
        aIn.Ignore( 5 );

		aIn >> nFormLen;

		bShrFmla = nFlag0 & 0x08;	// shared or not shared
	}

    Formula( aXclPos, nXF, nFormLen, fCurVal, bShrFmla );
}


void ImportExcel::Formula3()
{
	Formula4();
}


void ImportExcel::Formula4()
{
    XclAddress aXclPos;
    sal_uInt16  nXF, nFormLen;
	double	fCurVal;
	sal_uInt8	nFlag0;

    aIn >> aXclPos >> nXF >> fCurVal >> nFlag0;
    aIn.Ignore( 1 );
	aIn >> nFormLen;

    Formula( aXclPos, nXF, nFormLen, fCurVal, sal_False );
}


void ImportExcel::Formula( const XclAddress& rXclPos,
    sal_uInt16 nXF, sal_uInt16 nFormLen, double& rCurVal, sal_Bool bShrFmla )
{
	ConvErr eErr = ConvOK;

    ScAddress aScPos( ScAddress::UNINITIALIZED );
    if( GetAddressConverter().ConvertAddress( aScPos, rXclPos, GetCurrScTab(), true ) )
	{
		// jetzt steht Lesemarke auf Formel, Laenge in nFormLen
        const ScTokenArray* pErgebnis = 0;
		sal_Bool				bConvert;

        pFormConv->Reset( aScPos );

		if( bShrFmla )
            bConvert = !pFormConv->GetShrFmla( pErgebnis, maStrm, nFormLen );
		else
            bConvert = sal_True;

        if( bConvert )
            eErr = pFormConv->Convert( pErgebnis, maStrm, nFormLen, true, FT_CellFormula);

        ScFormulaCell*		pZelle = NULL;

		if( pErgebnis )
		{
            pZelle = new ScFormulaCell( pD, aScPos, pErgebnis );
            pD->PutCell( aScPos.Col(), aScPos.Row(), aScPos.Tab(), pZelle, (sal_Bool)sal_True );
		}
		else
		{
			CellType		eCellType;
			ScBaseCell*		pBaseCell;
            pD->GetCellType( aScPos.Col(), aScPos.Row(), aScPos.Tab(), eCellType );
			if( eCellType == CELLTYPE_FORMULA )
			{
                pD->GetCell( aScPos.Col(), aScPos.Row(), aScPos.Tab(), pBaseCell );
				pZelle = ( ScFormulaCell* ) pBaseCell;
				if( pZelle )
					pZelle->AddRecalcMode( RECALCMODE_ONLOAD_ONCE );
			}
		}

		if( pZelle )
		{
			if( eErr != ConvOK )
				ExcelToSc::SetError( *pZelle, eErr );
#if 0
            else
                ExcelToSc::SetCurVal( *pZelle, rCurVal );
#else
            (void)rCurVal;
#endif
		}

        GetXFRangeBuffer().SetXF( aScPos, nXF );
	}
}




ExcelToSc::ExcelToSc( const XclImpRoot& rRoot ) :
    ExcelConverterBase( 512 ),
    XclImpRoot( rRoot ),
    maFuncProv( rRoot ),
    meBiff( rRoot.GetBiff() )
{
}

ExcelToSc::~ExcelToSc()
{
}

void ExcelToSc::GetDummy( const ScTokenArray*& pErgebnis )
{
    aPool.Store( CREATE_STRING( "Dummy()" ) );
	aPool >> aStack;
	pErgebnis = aPool[ aStack.Get() ];
}


// if bAllowArrays is false stream seeks to first byte after <nFormulaLen>
// otherwise it will seek to the first byte after the additional content (eg
// inline arrays) following <nFormulaLen>
ConvErr ExcelToSc::Convert( const ScTokenArray*& pErgebnis, XclImpStream& aIn, sal_Size nFormulaLen, bool bAllowArrays, const FORMULA_TYPE eFT )
{
    RootData&       rR = GetOldRoot();
	sal_uInt8			nOp, nLen, nByte;
    sal_uInt16          nUINT16;
	sal_Int16			nINT16;
	double			fDouble;
	String			aString;
	sal_Bool			bError = sal_False;
	sal_Bool			bArrayFormula = sal_False;
	TokenId			nMerk0;
	const sal_Bool		bRangeName = eFT == FT_RangeName;
	const sal_Bool		bSharedFormula = eFT == FT_SharedFormula;
	const sal_Bool		bRNorSF = bRangeName || bSharedFormula;

	ScSingleRefData		aSRD;
	ScComplexRefData		aCRD;
    ExtensionTypeVec    aExtensions;

	bExternName = sal_False;

	if( eStatus != ConvOK )
	{
        aIn.Ignore( nFormulaLen );
		return eStatus;
	}

	if( nFormulaLen == 0 )
	{
        aPool.Store( CREATE_STRING( "-/-" ) );
		aPool >> aStack;
		pErgebnis = aPool[ aStack.Get() ];
		return ConvOK;
	}

    sal_Size nEndPos = aIn.GetRecPos() + nFormulaLen;

    while( (aIn.GetRecPos() < nEndPos) && !bError )
	{
		aIn >> nOp;

        // #98524# always reset flags
        aSRD.InitFlags();
        aCRD.InitFlags();

		switch( nOp )	//								Buch Seite:
		{			//										SDK4 SDK5
			case 0x01: // Array Formula							[325    ]
					   // Array Formula or Shared Formula		[    277]
			case 0x02: // Data Table							[325 277]
				nUINT16 = 3;

                if( meBiff != EXC_BIFF2 )
					nUINT16++;

                aIn.Ignore( nUINT16 );

				bArrayFormula = sal_True;
				break;
			case 0x03: // Addition								[312 264]
				aStack >> nMerk0;
				aPool <<  aStack << ocAdd << nMerk0;
				aPool >> aStack;
				break;
			case 0x04: // Subtraction							[313 264]
				// SECOMD-TOP minus TOP
				aStack >> nMerk0;
				aPool << aStack << ocSub << nMerk0;
				aPool >> aStack;
				break;
			case 0x05: // Multiplication						[313 264]
				aStack >> nMerk0;
				aPool << aStack << ocMul << nMerk0;
				aPool >> aStack;
				break;
			case 0x06: // Division								[313 264]
				// divide TOP by SECOND-TOP
				aStack >> nMerk0;
				aPool << aStack << ocDiv << nMerk0;
				aPool >> aStack;
				break;
			case 0x07: // Exponetiation							[313 265]
				// raise SECOND-TOP to power of TOP
				aStack >> nMerk0;
				aPool << aStack << ocPow << nMerk0;
				aPool >> aStack;
				break;
			case 0x08: // Concatenation							[313 265]
				// append TOP to SECOND-TOP
				aStack >> nMerk0;
				aPool << aStack << ocAmpersand << nMerk0;
				aPool >> aStack;
				break;
			case 0x09: // Less Than								[313 265]
				// SECOND-TOP < TOP
				aStack >> nMerk0;
				aPool << aStack << ocLess << nMerk0;
				aPool >> aStack;
				break;
			case 0x0A: // Less Than or Equal					[313 265]
				// SECOND-TOP <= TOP
				aStack >> nMerk0;
				aPool << aStack << ocLessEqual << nMerk0;
				aPool >> aStack;
				break;
			case 0x0B: // Equal									[313 265]
				// SECOND-TOP == TOP
				aStack >> nMerk0;
				aPool << aStack << ocEqual << nMerk0;
				aPool >> aStack;
				break;
			case 0x0C: // Greater Than or Equal					[313 265]
				// SECOND-TOP == TOP
				aStack >> nMerk0;
				aPool << aStack << ocGreaterEqual << nMerk0;
				aPool >> aStack;
				break;
			case 0x0D: // Greater Than							[313 265]
				// SECOND-TOP == TOP
				aStack >> nMerk0;
				aPool << aStack << ocGreater << nMerk0;
				aPool >> aStack;
				break;
			case 0x0E: // Not Equal								[313 265]
				// SECOND-TOP == TOP
				aStack >> nMerk0;
				aPool << aStack << ocNotEqual << nMerk0;
				aPool >> aStack;
				break;
			case 0x0F: // Intersection							[314 265]
				aStack >> nMerk0;
				aPool << aStack << ocIntersect << nMerk0;
				aPool >> aStack;
				break;
			case 0x10: // Union									[314 265]
				// ocSep behelfsweise statt 'ocUnion'
				aStack >> nMerk0;
//#100928#      aPool << ocOpen << aStack << ocSep << nMerk0 << ocClose;
                aPool << aStack << ocSep << nMerk0;
					// doesn't fit exactly, but is more Excel-like
				aPool >> aStack;
				break;
			case 0x11: // Range									[314 265]
				aStack >> nMerk0;
				aPool << aStack << ocRange << nMerk0;
				aPool >> aStack;
				break;
			case 0x12: // Unary Plus							[312 264]
                aPool << ocAdd << aStack;
                aPool >> aStack;
				break;
			case 0x13: // Unary Minus							[312 264]
				aPool << ocNegSub << aStack;
				aPool >> aStack;
				break;
			case 0x14: // Percent Sign							[312 264]
                aPool << aStack << ocPercentSign;
				aPool >> aStack;
				break;
			case 0x15: // Parenthesis							[326 278]
				aPool << ocOpen << aStack << ocClose;
				aPool >> aStack;
				break;
			case 0x16: // Missing Argument						[314 266]
				aPool << ocMissing;
				aPool >> aStack;
                GetTracer().TraceFormulaMissingArg();
				break;
			case 0x17: // String Constant						[314 266]
				aIn >> nLen;
                aString = aIn.ReadRawByteString( nLen );

				aStack << aPool.Store( aString );
				break;
			case 0x19: // Special Attribute						[327 279]
			{
				sal_uInt16	nData, nFakt;
				sal_uInt8	nOpt;

				aIn >> nOpt;

                if( meBiff == EXC_BIFF2 )
				{
					nData = aIn.ReaduInt8();
					nFakt = 1;
				}
				else
				{
					aIn >> nData;
					nFakt = 2;
				}

				if( nOpt & 0x04 )
				{// nFakt -> Bytes oder Words ueberlesen	AttrChoose
					nData++;
                    aIn.Ignore( nData * nFakt );
				}
				else if( nOpt & 0x10 )						// AttrSum
					DoMulArgs( ocSum, 1 );
			}
				break;
			case 0x1A: // External Reference					[330    ]
                switch( meBiff )
				{
                    case EXC_BIFF2: aIn.Ignore( 7 );    break;
                    case EXC_BIFF3:
                    case EXC_BIFF4: aIn.Ignore( 10 );   break;
                    case EXC_BIFF5:
						DBG_WARNING( "-ExcelToSc::Convert(): 0x1A gibt's nicht in Biff5!" );
					default:
						DBG_WARNING( "-ExcelToSc::Convert(): Ein wenig vergesslich, was?" );
				}
				break;
			case 0x1B: // End External Reference				[330    ]
                switch( meBiff )
				{
                    case EXC_BIFF2: aIn.Ignore( 3 );    break;
                    case EXC_BIFF3:
                    case EXC_BIFF4: aIn.Ignore( 4 );    break;
                    case EXC_BIFF5:
						DBG_WARNING( "-ExcelToSc::Convert(): 0x1B gibt's nicht in Biff5!" );
					default:
						DBG_WARNING( "-ExcelToSc::Convert(): Ein wenig vergesslich, was?" );
				}
				break;
			case 0x1C: // Error Value							[314 266]
			{
				aIn >> nByte;
#if 0   // erAck
                aPool.StoreError( XclTools::GetScErrorCode( nByte ) );
#else
				DefTokenId			eOc;
				switch( nByte )
				{
                    case EXC_ERR_NULL:
                    case EXC_ERR_DIV0:
                    case EXC_ERR_VALUE:
                    case EXC_ERR_REF:
                    case EXC_ERR_NAME:
                    case EXC_ERR_NUM:   eOc = ocStop;       break;
                    case EXC_ERR_NA:    eOc = ocNotAvail;   break;
                    default:            eOc = ocNoName;
				}
				aPool << eOc;
				if( eOc != ocStop )
					aPool << ocOpen << ocClose;
#endif
				aPool >> aStack;
			}
				break;
			case 0x1D: // Boolean								[315 266]
				aIn >> nByte;
				if( nByte == 0 )
					aPool << ocFalse << ocOpen << ocClose;
				else
					aPool << ocTrue << ocOpen << ocClose;
				aPool >> aStack;
				break;
			case 0x1E: // Integer								[315 266]
				aIn >> nUINT16;
				aStack << aPool.Store( ( double ) nUINT16 );
				break;
			case 0x1F: // Number								[315 266]
				aIn >> fDouble;
				aStack << aPool.Store( fDouble );
				break;
			case 0x40:
			case 0x60:
            case 0x20: // Array Constant						[317 268]
                aIn >> nByte >> nUINT16;
                aIn.Ignore( (meBiff == EXC_BIFF2) ? 3 : 4 );
                if( bAllowArrays )
                {
                    aStack << aPool.StoreMatrix();
                    aExtensions.push_back( EXTENSION_ARRAY );
                }
                else
                {
                    aPool << ocBad;
                    aPool >> aStack;
                }
                break;
			case 0x41:
			case 0x61:
			case 0x21: // Function, Fixed Number of Arguments	[333 282]
            {
                sal_uInt16 nXclFunc;
                if( meBiff <= EXC_BIFF3 )
                    nXclFunc = aIn.ReaduInt8();
                else
                    aIn >> nXclFunc;
                if( const XclFunctionInfo* pFuncInfo = maFuncProv.GetFuncInfoFromXclFunc( nXclFunc ) )
                    DoMulArgs( pFuncInfo->meOpCode, pFuncInfo->mnMaxParamCount, pFuncInfo->mnMinParamCount );
                else
                    DoMulArgs( ocNoName, 0 );
            }
            break;
			case 0x42:
			case 0x62:
			case 0x22: // Function, Variable Number of Arg.		[333 283]
            {
                sal_uInt16 nXclFunc;
                sal_uInt8 nParamCount;
                aIn >> nParamCount;
                nParamCount &= 0x7F;
                if( meBiff <= EXC_BIFF3 )
                    nXclFunc = aIn.ReaduInt8();
                else
                    aIn >> nXclFunc;
                if( const XclFunctionInfo* pFuncInfo = maFuncProv.GetFuncInfoFromXclFunc( nXclFunc ) )
                    DoMulArgs( pFuncInfo->meOpCode, nParamCount, pFuncInfo->mnMinParamCount );
                else
                    DoMulArgs( ocNoName, 0 );
            }
            break;
			case 0x43:
			case 0x63:
			case 0x23: // Name									[318 269]
            {
				aIn >> nUINT16;
                switch( meBiff )
				{
                    case EXC_BIFF2: aIn.Ignore( 5 );    break;
                    case EXC_BIFF3:
                    case EXC_BIFF4: aIn.Ignore( 8 );    break;
                    case EXC_BIFF5: aIn.Ignore( 12 );   break;
					default:
						DBG_ERROR(
						"-ExcelToSc::Convert(): Ein wenig vergesslich, was?" );
				}
                const XclImpName* pName = GetNameManager().GetName( nUINT16 );
				if(pName && !pName->GetScRangeData())
				    aStack << aPool.Store( ocMacro, pName->GetXclName() );
				else
				    aStack << aPool.Store( nUINT16 );
			}
				break;
			case 0x44:
			case 0x64:
			case 0x24: // Cell Reference						[319 270]
			case 0x4A:
			case 0x6A:
			case 0x2A: // Deleted Cell Reference				[323 273]
				aIn >> nUINT16 >> nByte;
				aSRD.nCol = static_cast<SCsCOL>(nByte);
				aSRD.nRow = nUINT16 & 0x3FFF;
				aSRD.nRelTab = 0;
				aSRD.SetTabRel( sal_True );
				aSRD.SetFlag3D( bRangeName );

				ExcRelToScRel( nUINT16, nByte, aSRD, bRangeName );

				switch ( nOp )
				{
					case 0x4A:
					case 0x6A:
					case 0x2A: // Deleted Cell Reference	 	[323 273]
						// no information which part is deleted, set both
						aSRD.SetColDeleted( sal_True );
						aSRD.SetRowDeleted( sal_True );
				}

				aStack << aPool.Store( aSRD );
				break;
			case 0x45:
			case 0x65:
			case 0x25: // Area Reference						[320 270]
			case 0x4B:
			case 0x6B:
			case 0x2B: // Deleted Area Refernce					[323 273]
			{
				sal_uInt16			nRowFirst, nRowLast;
				sal_uInt8			nColFirst, nColLast;
				ScSingleRefData&	rSRef1 = aCRD.Ref1;
				ScSingleRefData&	rSRef2 = aCRD.Ref2;

				aIn >> nRowFirst >> nRowLast >> nColFirst >> nColLast;

				rSRef1.nRelTab = rSRef2.nRelTab = 0;
				rSRef1.SetTabRel( sal_True );
				rSRef2.SetTabRel( sal_True );
				rSRef1.SetFlag3D( bRangeName );
				rSRef2.SetFlag3D( bRangeName );

				ExcRelToScRel( nRowFirst, nColFirst, aCRD.Ref1, bRangeName );
				ExcRelToScRel( nRowLast, nColLast, aCRD.Ref2, bRangeName );

				if( IsComplColRange( nColFirst, nColLast ) )
					SetComplCol( aCRD );
				else if( IsComplRowRange( nRowFirst, nRowLast ) )
					SetComplRow( aCRD );

				switch ( nOp )
				{
					case 0x4B:
					case 0x6B:
					case 0x2B: // Deleted Area Refernce			[323 273]
						// no information which part is deleted, set all
						rSRef1.SetColDeleted( sal_True );
						rSRef1.SetRowDeleted( sal_True );
						rSRef2.SetColDeleted( sal_True );
						rSRef2.SetRowDeleted( sal_True );
				}

				aStack << aPool.Store( aCRD );
			}
				break;
			case 0x46:
			case 0x66:
			case 0x26: // Constant Reference Subexpression		[321 271]
                aExtensions.push_back( EXTENSION_MEMAREA );
                // fall through

			case 0x47:
			case 0x67:
			case 0x27: // Erroneous Constant Reference Subexpr.	[322 272]
			case 0x48:
			case 0x68:
			case 0x28: // Incomplete Constant Reference Subexpr.[331 281]
                aIn.Ignore( (meBiff == EXC_BIFF2) ? 4 : 6 );
				break;
			case 0x4C:
			case 0x6C:
			case 0x2C: // Cell Reference Within a Name			[323    ]
					   // Cell Reference Within a Shared Formula[    273]
			{
				aIn >> nUINT16 >> nByte;	// >> Attribute, Row >> Col

				aSRD.nRelTab = 0;
				aSRD.SetTabRel( sal_True );
				aSRD.SetFlag3D( bRangeName );

				ExcRelToScRel( nUINT16, nByte, aSRD, bRNorSF );

				aStack << aPool.Store( aSRD );
			}
				break;
			case 0x4D:
			case 0x6D:
			case 0x2D: // Area Reference Within a Name			[324    ]
			{	   // Area Reference Within a Shared Formula[    274]
				sal_uInt16					nRowFirst, nRowLast;
				sal_uInt8					nColFirst, nColLast;

				aCRD.Ref1.nRelTab = aCRD.Ref2.nRelTab = 0;
				aCRD.Ref1.SetTabRel( sal_True );
				aCRD.Ref2.SetTabRel( sal_True );
				aCRD.Ref1.SetFlag3D( bRangeName );
				aCRD.Ref2.SetFlag3D( bRangeName );

				aIn >> nRowFirst >> nRowLast >> nColFirst >> nColLast;

				ExcRelToScRel( nRowFirst, nColFirst, aCRD.Ref1, bRNorSF );
				ExcRelToScRel( nRowLast, nColLast, aCRD.Ref2, bRNorSF );

				if( IsComplColRange( nColFirst, nColLast ) )
					SetComplCol( aCRD );
				else if( IsComplRowRange( nRowFirst, nRowLast ) )
					SetComplRow( aCRD );

				aStack << aPool.Store( aCRD );
			}
				break;
            case 0x49:
            case 0x69:
            case 0x29: // Variable Reference Subexpression      [331 281]
			case 0x4E:
			case 0x6E:
			case 0x2E: // Reference Subexpression Within a Name	[332 282]
			case 0x4F:
			case 0x6F:
			case 0x2F: // Incomplete Reference Subexpression...	[332 282]
                aIn.Ignore( (meBiff == EXC_BIFF2) ? 1 : 2 );
				break;
			case 0x58:
			case 0x78:
			case 0x38: // Command-Equivalent Function			[333    ]
				aString.AssignAscii( "COMM_EQU_FUNC" );
				aIn >> nByte;
				aString += String::CreateFromInt32( nByte );
				aIn >> nByte;
				aStack << aPool.Store( aString );
				DoMulArgs( ocPush, nByte + 1 );
				break;
			case 0x59:
			case 0x79:
			case 0x39: // Name or External Name					[    275]
				aIn >> nINT16;
                aIn.Ignore( 8 );
				aIn >> nUINT16;
				if( nINT16 >= 0 )
				{
                    const ExtName* pExtName = rR.pExtNameBuff->GetNameByIndex( nINT16, nUINT16 );
					if( pExtName && pExtName->IsDDE() &&
                        rR.pExtSheetBuff->IsLink( ( sal_uInt16 ) nINT16 ) )
					{
						String			aAppl, aExtDoc;
						TokenId			nPar1, nPar2;

                        rR.pExtSheetBuff->GetLink( ( sal_uInt16 ) nINT16 , aAppl, aExtDoc );
						nPar1 = aPool.Store( aAppl );
						nPar2 = aPool.Store( aExtDoc );
						nMerk0 = aPool.Store( pExtName->aName );
						aPool	<< ocDde << ocOpen << nPar1 << ocSep << nPar2 << ocSep
								<< nMerk0 << ocClose;

                        GetDoc().CreateDdeLink( aAppl, aExtDoc, pExtName->aName, SC_DDE_DEFAULT );
					}
					else
						aPool << ocBad;

					aPool >> aStack;
				}
				else
                    aStack << aPool.Store( nUINT16 );
                aIn.Ignore( 12 );
				break;
			case 0x5A:
			case 0x7A:
			case 0x3A: // 3-D Cell Reference					[    275]
			case 0x5C:
			case 0x7C:
			case 0x3C: // Deleted 3-D Cell Reference			[    277]
			{
				sal_uInt16			nTabFirst, nTabLast, nRow;
				sal_Int16			nExtSheet;
				sal_uInt8			nCol;

				aIn >> nExtSheet;
                aIn.Ignore( 8 );
				aIn >> nTabFirst >> nTabLast >> nRow >> nCol;

				if( nExtSheet >= 0 )
				{	// von extern
                    if( rR.pExtSheetBuff->GetScTabIndex( nExtSheet, nTabLast ) )
					{
						nTabFirst = nTabLast;
						nExtSheet = 0;		// gefunden
					}
					else
					{
						aPool << ocBad;
						aPool >> aStack;
						nExtSheet = 1;		// verhindert Erzeugung einer SingleRef
					}
				}

				if( nExtSheet <= 0 )
				{	// in aktuellem Workbook
					aSRD.nTab = static_cast<SCTAB>(nTabFirst);
                    aSRD.SetFlag3D( sal_True );
					aSRD.SetTabRel( sal_False );

					ExcRelToScRel( nRow, nCol, aSRD, bRangeName );

					switch ( nOp )
					{
						case 0x5C:
						case 0x7C:
						case 0x3C: // Deleted 3-D Cell Reference	[    277]
							// no information which part is deleted, set both
							aSRD.SetColDeleted( sal_True );
							aSRD.SetRowDeleted( sal_True );
					}
					if ( !ValidTab(static_cast<SCTAB>(nTabFirst)) )
						aSRD.SetTabDeleted( sal_True );

					if( nTabLast != nTabFirst )
					{
                        aCRD.Ref1 = aCRD.Ref2 = aSRD;
						aCRD.Ref2.nTab = static_cast<SCTAB>(nTabLast);
						aCRD.Ref2.SetTabDeleted( !ValidTab(static_cast<SCTAB>(nTabLast)) );
						aStack << aPool.Store( aCRD );
					}
					else
						aStack << aPool.Store( aSRD );
				}
			}

				break;
			case 0x5B:
			case 0x7B:
			case 0x3B: // 3-D Area Reference					[    276]
			case 0x5D:
			case 0x7D:
			case 0x3D: // Deleted 3-D Area Reference			[    277]
			{
				sal_uInt16		nTabFirst, nTabLast, nRowFirst, nRowLast;
				sal_Int16		nExtSheet;
				sal_uInt8		nColFirst, nColLast;

				aIn >> nExtSheet;
                aIn.Ignore( 8 );
				aIn >> nTabFirst >> nTabLast >> nRowFirst >> nRowLast
					>> nColFirst >> nColLast;

				if( nExtSheet >= 0 )
					// von extern
				{
                    if( rR.pExtSheetBuff->GetScTabIndex( nExtSheet, nTabLast ) )
					{
						nTabFirst = nTabLast;
						nExtSheet = 0;		// gefunden
					}
					else
					{
						aPool << ocBad;
						aPool >> aStack;
						nExtSheet = 1;		// verhindert Erzeugung einer CompleteRef
					}
				}

				if( nExtSheet <= 0 )
				{// in aktuellem Workbook
					// erster Teil des Bereichs
					ScSingleRefData&	rR1 = aCRD.Ref1;
					ScSingleRefData&	rR2 = aCRD.Ref2;

					rR1.nTab = static_cast<SCTAB>(nTabFirst);
					rR2.nTab = static_cast<SCTAB>(nTabLast);
                    rR1.SetFlag3D( sal_True );
					rR1.SetTabRel( sal_False );
                    rR2.SetFlag3D( nTabFirst != nTabLast );
					rR2.SetTabRel( sal_False );

					ExcRelToScRel( nRowFirst, nColFirst, aCRD.Ref1, bRangeName );
					ExcRelToScRel( nRowLast, nColLast, aCRD.Ref2, bRangeName );

					if( IsComplColRange( nColFirst, nColLast ) )
						SetComplCol( aCRD );
					else if( IsComplRowRange( nRowFirst, nRowLast ) )
						SetComplRow( aCRD );

					switch ( nOp )
					{
						case 0x5D:
						case 0x7D:
						case 0x3D: // Deleted 3-D Area Reference	[    277]
						   	// no information which part is deleted, set all
							rR1.SetColDeleted( sal_True );
							rR1.SetRowDeleted( sal_True );
							rR2.SetColDeleted( sal_True );
							rR2.SetRowDeleted( sal_True );
					}
					if ( !ValidTab(static_cast<SCTAB>(nTabFirst)) )
						rR1.SetTabDeleted( sal_True );
					if ( !ValidTab(static_cast<SCTAB>(nTabLast)) )
						rR2.SetTabDeleted( sal_True );

					aStack << aPool.Store( aCRD );
				}//ENDE in aktuellem Workbook
			}
				break;
			default: bError = sal_True;
		}
		bError |= !aIn.IsValid();
	}

	ConvErr eRet;

	if( bError )
	{
		aPool << ocBad;
		aPool >> aStack;
		pErgebnis = aPool[ aStack.Get() ];
		eRet = ConvErrNi;
	}
    else if( aIn.GetRecPos() != nEndPos )
	{
		aPool << ocBad;
		aPool >> aStack;
		pErgebnis = aPool[ aStack.Get() ];
		eRet = ConvErrCount;
	}
	else if( bExternName )
	{
		pErgebnis = aPool[ aStack.Get() ];
		eRet = ConvErrExternal;
	}
	else if( bArrayFormula )
	{
		pErgebnis = NULL;
		eRet = ConvOK;
	}
	else
	{
		pErgebnis = aPool[ aStack.Get() ];
		eRet = ConvOK;
	}

    aIn.Seek( nEndPos );

    if( eRet == ConvOK )
        ReadExtensions( aExtensions, aIn );

	return eRet;
}


// stream seeks to first byte after <nFormulaLen>
ConvErr ExcelToSc::Convert( _ScRangeListTabs& rRangeList, XclImpStream& aIn, sal_Size nFormulaLen, const FORMULA_TYPE eFT )
{
    RootData&       rR = GetOldRoot();
	sal_uInt8			nOp, nLen;
    sal_Size        nIgnore;
	sal_uInt16			nUINT16;
	sal_uInt8			nByte;
	sal_Bool			bError = sal_False;
	sal_Bool			bArrayFormula = sal_False;
	const sal_Bool		bRangeName = eFT == FT_RangeName;
	const sal_Bool		bSharedFormula = eFT == FT_SharedFormula;
	const sal_Bool		bRNorSF = bRangeName || bSharedFormula;

	ScSingleRefData	aSRD;
	ScComplexRefData	aCRD;
	aCRD.Ref1.nTab = aCRD.Ref2.nTab = aEingPos.Tab();

	bExternName = sal_False;

	if( eStatus != ConvOK )
	{
        aIn.Ignore( nFormulaLen );
		return eStatus;
	}

	if( nFormulaLen == 0 )
		return ConvOK;

    sal_Size nEndPos = aIn.GetRecPos() + nFormulaLen;

    while( (aIn.GetRecPos() < nEndPos) && !bError )
	{
		aIn >> nOp;
		nIgnore = 0;

        // #98524# always reset flags
        aSRD.InitFlags();
        aCRD.InitFlags();

		switch( nOp )	//								Buch Seite:
		{			//										SDK4 SDK5
			case 0x01: // Array Formula							[325    ]
					   // Array Formula or Shared Formula		[    277]
                nIgnore = (meBiff == EXC_BIFF2) ? 3 : 4;
				bArrayFormula = sal_True;
				break;
			case 0x02: // Data Table							[325 277]
                nIgnore = (meBiff == EXC_BIFF2) ? 3 : 4;
				break;
			case 0x03: // Addition								[312 264]
			case 0x04: // Subtraction							[313 264]
			case 0x05: // Multiplication						[313 264]
			case 0x06: // Division								[313 264]
			case 0x07: // Exponetiation							[313 265]
			case 0x08: // Concatenation							[313 265]
			case 0x09: // Less Than								[313 265]
			case 0x0A: // Less Than or Equal					[313 265]
			case 0x0B: // Equal									[313 265]
			case 0x0C: // Greater Than or Equal					[313 265]
			case 0x0D: // Greater Than							[313 265]
			case 0x0E: // Not Equal								[313 265]
			case 0x0F: // Intersection							[314 265]
			case 0x10: // Union									[314 265]
			case 0x11: // Range									[314 265]
			case 0x12: // Unary Plus							[312 264]
			case 0x13: // Unary Minus							[312 264]
			case 0x14: // Percent Sign							[312 264]
			case 0x15: // Parenthesis							[326 278]
			case 0x16: // Missing Argument						[314 266]
				break;
			case 0x17: // String Constant						[314 266]
				aIn >> nLen;
				nIgnore = nLen;
				break;
			case 0x19: // Special Attribute						[327 279]
			{
				sal_uInt16 nData, nFakt;
				sal_uInt8 nOpt;

				aIn >> nOpt;

                if( meBiff == EXC_BIFF2 )
				{
					nData = aIn.ReaduInt8();
					nFakt = 1;
				}
				else
				{
					aIn >> nData;
					nFakt = 2;
				}

				if( nOpt & 0x04 )
				{// nFakt -> Bytes oder Words ueberlesen	AttrChoose
					nData++;
                    aIn.Ignore( nData * nFakt );
				}
			}
				break;
			case 0x1A: // External Reference					[330    ]
                switch( meBiff )
				{
                    case EXC_BIFF2: nIgnore = 7;    break;
                    case EXC_BIFF3:
                    case EXC_BIFF4: nIgnore = 10;   break;
                    case EXC_BIFF5: DBG_WARNING( "-ExcelToSc::Convert(): 0x1A gibt's nicht in Biff5!" );
                    default:        DBG_WARNING( "-ExcelToSc::Convert(): Ein wenig vergesslich, was?" );
				}
				break;
			case 0x1B: // End External Reference				[330    ]
                switch( meBiff )
				{
                    case EXC_BIFF2: nIgnore = 3;        break;
                    case EXC_BIFF3:
                    case EXC_BIFF4: nIgnore = 4;        break;
                    case EXC_BIFF5: DBG_WARNING( "-ExcelToSc::Convert(): 0x1B gibt's nicht in Biff5!" );
                    default:        DBG_WARNING( "-ExcelToSc::Convert(): Ein wenig vergesslich, was?" );
				}
				break;
			case 0x1C: // Error Value							[314 266]
			case 0x1D: // Boolean								[315 266]
				nIgnore = 1;
				break;
			case 0x1E: // Integer								[315 266]
				nIgnore = 2;
				break;
			case 0x1F: // Number								[315 266]
                nIgnore = 8;
				break;
			case 0x40:
			case 0x60:
			case 0x20: // Array Constant						[317 268]
                nIgnore = (meBiff == EXC_BIFF2) ? 6 : 7;
				break;
			case 0x41:
			case 0x61:
			case 0x21: // Function, Fixed Number of Arguments	[333 282]
                nIgnore = (meBiff <= EXC_BIFF3) ? 1 : 2;
				break;
			case 0x42:
			case 0x62:
			case 0x22: // Function, Variable Number of Arg.		[333 283]
                nIgnore = (meBiff <= EXC_BIFF3) ? 2 : 3;
				break;
			case 0x43:
			case 0x63:
			case 0x23: // Name									[318 269]
                switch( meBiff )
				{
                    case EXC_BIFF2: nIgnore = 7;    break;
                    case EXC_BIFF3:
                    case EXC_BIFF4: nIgnore = 10;   break;
                    case EXC_BIFF5: nIgnore = 14;   break;
                    default:        DBG_ERROR( "-ExcelToSc::Convert(): Ein wenig vergesslich, was?" );
				}
				break;
			case 0x44:
			case 0x64:
			case 0x24: // Cell Reference						[319 270]
				aIn >> nUINT16 >> nByte;
				aSRD.nCol = static_cast<SCsCOL>(nByte);
				aSRD.nRow = nUINT16 & 0x3FFF;
				aSRD.nRelTab = 0;
				aSRD.SetTabRel( sal_True );
				aSRD.SetFlag3D( bRangeName );

				ExcRelToScRel( nUINT16, nByte, aSRD, bRangeName );

				rRangeList.Append( aSRD );
				break;
			case 0x45:
			case 0x65:
			case 0x25: // Area Reference						[320 270]
			{
				sal_uInt16			nRowFirst, nRowLast;
				sal_uInt8			nColFirst, nColLast;
				ScSingleRefData	&rSRef1 = aCRD.Ref1;
				ScSingleRefData	&rSRef2 = aCRD.Ref2;

				aIn >> nRowFirst >> nRowLast >> nColFirst >> nColLast;

				rSRef1.nRelTab = rSRef2.nRelTab = 0;
				rSRef1.SetTabRel( sal_True );
				rSRef2.SetTabRel( sal_True );
				rSRef1.SetFlag3D( bRangeName );
				rSRef2.SetFlag3D( bRangeName );

				ExcRelToScRel( nRowFirst, nColFirst, aCRD.Ref1, bRangeName );
				ExcRelToScRel( nRowLast, nColLast, aCRD.Ref2, bRangeName );

				if( IsComplColRange( nColFirst, nColLast ) )
					SetComplCol( aCRD );
				else if( IsComplRowRange( nRowFirst, nRowLast ) )
					SetComplRow( aCRD );

				rRangeList.Append( aCRD );
			}
				break;
			case 0x46:
			case 0x66:
			case 0x26: // Constant Reference Subexpression		[321 271]
			case 0x47:
			case 0x67:
			case 0x27: // Erroneous Constant Reference Subexpr.	[322 272]
			case 0x48:
			case 0x68:
			case 0x28: // Incomplete Constant Reference Subexpr.[331 281]
                nIgnore = (meBiff == EXC_BIFF2) ? 4 : 6;
				break;
			case 0x4A:
			case 0x6A:
			case 0x2A: // Deleted Cell Reference				[323 273]
				nIgnore = 3;
				break;
			case 0x4B:
			case 0x6B:
			case 0x2B: // Deleted Area Refernce					[323 273]
				nIgnore = 6;
				break;
			case 0x4C:
			case 0x6C:
			case 0x2C: // Cell Reference Within a Name			[323    ]
					   // Cell Reference Within a Shared Formula[    273]
			{
				aIn >> nUINT16 >> nByte;	// >> Attribute, Row >> Col

				aSRD.nRelTab = 0;
				aSRD.SetTabRel( sal_True );
				aSRD.SetFlag3D( bRangeName );

				ExcRelToScRel( nUINT16, nByte, aSRD, bRNorSF );

				rRangeList.Append( aSRD );
			}
				break;
			case 0x4D:
			case 0x6D:
			case 0x2D: // Area Reference Within a Name			[324    ]
			{	   // Area Reference Within a Shared Formula[    274]
				sal_uInt16					nRowFirst, nRowLast;
				sal_uInt8					nColFirst, nColLast;

				aCRD.Ref1.nRelTab = aCRD.Ref2.nRelTab = 0;
				aCRD.Ref1.SetTabRel( sal_True );
				aCRD.Ref2.SetTabRel( sal_True );
				aCRD.Ref1.SetFlag3D( bRangeName );
				aCRD.Ref2.SetFlag3D( bRangeName );

				aIn >> nRowFirst >> nRowLast >> nColFirst >> nColLast;

				ExcRelToScRel( nRowFirst, nColFirst, aCRD.Ref1, bRNorSF );
				ExcRelToScRel( nRowLast, nColLast, aCRD.Ref2, bRNorSF );

				if( IsComplColRange( nColFirst, nColLast ) )
					SetComplCol( aCRD );
				else if( IsComplRowRange( nRowFirst, nRowLast ) )
					SetComplRow( aCRD );

				rRangeList.Append( aCRD );
			}
				break;
            case 0x49:
            case 0x69:
            case 0x29: // Variable Reference Subexpression      [331 281]
			case 0x4E:
			case 0x6E:
			case 0x2E: // Reference Subexpression Within a Name	[332 282]
			case 0x4F:
			case 0x6F:
			case 0x2F: // Incomplete Reference Subexpression...	[332 282]
                nIgnore = (meBiff == EXC_BIFF2) ? 1 : 2;
				break;
			case 0x58:
			case 0x78:
			case 0x38: // Command-Equivalent Function			[333    ]
				nIgnore = 2;
				break;
			case 0x59:
			case 0x79:
			case 0x39: // Name or External Name					[    275]
				nIgnore = 24;
				break;
			case 0x5A:
			case 0x7A:
			case 0x3A: // 3-D Cell Reference					[    275]
			{
				sal_uInt16			nTabFirst, nTabLast, nRow;
				sal_Int16			nExtSheet;
				sal_uInt8			nCol;

				aIn >> nExtSheet;
                aIn.Ignore( 8 );
				aIn >> nTabFirst >> nTabLast >> nRow >> nCol;

				if( nExtSheet >= 0 )
					// von extern
				{
                    if( rR.pExtSheetBuff->GetScTabIndex( nExtSheet, nTabLast ) )
					{
						nTabFirst = nTabLast;
						nExtSheet = 0;		// gefunden
					}
					else
					{
						aPool << ocBad;
						aPool >> aStack;
						nExtSheet = 1;		// verhindert Erzeugung einer SingleRef
					}
				}

				if( nExtSheet <= 0 )
				{// in aktuellem Workbook
					sal_Bool b3D = ( static_cast<SCTAB>(nTabFirst) != aEingPos.Tab() ) || bRangeName;
					aSRD.nTab = static_cast<SCTAB>(nTabFirst);
					aSRD.SetFlag3D( b3D );
					aSRD.SetTabRel( sal_False );

					ExcRelToScRel( nRow, nCol, aSRD, bRangeName );

					if( nTabLast != nTabFirst )
					{
						aCRD.Ref1 = aSRD;
						aCRD.Ref2.nCol = aSRD.nCol;
						aCRD.Ref2.nRow = aSRD.nRow;
						aCRD.Ref2.nTab = static_cast<SCTAB>(nTabLast);
						b3D = ( static_cast<SCTAB>(nTabLast) != aEingPos.Tab() );
						aCRD.Ref2.SetFlag3D( b3D );
						aCRD.Ref2.SetTabRel( sal_False );
						rRangeList.Append( aCRD );
					}
					else
						rRangeList.Append( aSRD );
				}
			}

				break;
			case 0x5B:
			case 0x7B:
			case 0x3B: // 3-D Area Reference					[    276]
			{
				sal_uInt16		nTabFirst, nTabLast, nRowFirst, nRowLast;
				sal_Int16		nExtSheet;
				sal_uInt8		nColFirst, nColLast;

				aIn >> nExtSheet;
                aIn.Ignore( 8 );
				aIn >> nTabFirst >> nTabLast >> nRowFirst >> nRowLast
					>> nColFirst >> nColLast;

				if( nExtSheet >= 0 )
					// von extern
				{
                    if( rR.pExtSheetBuff->GetScTabIndex( nExtSheet, nTabLast ) )
					{
						nTabFirst = nTabLast;
						nExtSheet = 0;		// gefunden
					}
					else
					{
						aPool << ocBad;
						aPool >> aStack;
						nExtSheet = 1;		// verhindert Erzeugung einer CompleteRef
					}
				}

				if( nExtSheet <= 0 )
				{// in aktuellem Workbook
					// erster Teil des Bereichs
					ScSingleRefData	&rR1 = aCRD.Ref1;
					ScSingleRefData	&rR2 = aCRD.Ref2;

					rR1.nTab = static_cast<SCTAB>(nTabFirst);
					rR2.nTab = static_cast<SCTAB>(nTabLast);
					rR1.SetFlag3D( ( static_cast<SCTAB>(nTabFirst) != aEingPos.Tab() ) || bRangeName );
					rR1.SetTabRel( sal_False );
					rR2.SetFlag3D( ( static_cast<SCTAB>(nTabLast) != aEingPos.Tab() ) || bRangeName );
					rR2.SetTabRel( sal_False );

					ExcRelToScRel( nRowFirst, nColFirst, aCRD.Ref1, bRangeName );
					ExcRelToScRel( nRowLast, nColLast, aCRD.Ref2, bRangeName );

					if( IsComplColRange( nColFirst, nColLast ) )
						SetComplCol( aCRD );
					else if( IsComplRowRange( nRowFirst, nRowLast ) )
						SetComplRow( aCRD );

					rRangeList.Append( aCRD );
				}//ENDE in aktuellem Workbook
			}
				break;
			case 0x5C:
			case 0x7C:
			case 0x3C: // Deleted 3-D Cell Reference			[    277]
				nIgnore = 17;
				break;
			case 0x5D:
			case 0x7D:
			case 0x3D: // Deleted 3-D Area Reference			[    277]
				nIgnore = 20;
				break;
			default: bError = sal_True;
		}
		bError |= !aIn.IsValid();

        aIn.Ignore( nIgnore );
	}

	ConvErr eRet;

	if( bError )
		eRet = ConvErrNi;
    else if( aIn.GetRecPos() != nEndPos )
		eRet = ConvErrCount;
	else if( bExternName )
		eRet = ConvErrExternal;
	else if( bArrayFormula )
		eRet = ConvOK;
	else
		eRet = ConvOK;

    aIn.Seek( nEndPos );
	return eRet;
}

ConvErr ExcelToSc::ConvertExternName( const ScTokenArray*& /*rpArray*/, XclImpStream& /*rStrm*/, sal_Size /*nFormulaLen*/,
                                      const String& /*rUrl*/, const vector<String>& /*rTabNames*/ )
{
    // not implemented ...
    return ConvErrNi;
}

sal_Bool ExcelToSc::GetAbsRefs( ScRangeList& rRangeList, XclImpStream& rStrm, sal_Size nLen )
{
    DBG_ASSERT_BIFF( GetBiff() == EXC_BIFF5 );
    if( GetBiff() != EXC_BIFF5 )
        return sal_False;

    sal_uInt8 nOp;
    sal_uInt16 nRow1, nRow2;
    sal_uInt8 nCol1, nCol2;
    SCTAB nTab1, nTab2;
    sal_uInt16 nTabFirst, nTabLast;
    sal_Int16 nRefIdx;

    sal_Size nSeek;
    sal_Size nEndPos = rStrm.GetRecPos() + nLen;

    while( rStrm.IsValid() && (rStrm.GetRecPos() < nEndPos) )
    {
        rStrm >> nOp;
        nSeek = 0;

        switch( nOp )
        {
            case 0x44:
            case 0x64:
            case 0x24: // Cell Reference                        [319 270]
            case 0x4C:
            case 0x6C:
            case 0x2C: // Cell Reference Within a Name          [323    ]
                       // Cell Reference Within a Shared Formula[    273]
                rStrm >> nRow1 >> nCol1;

                nRow2 = nRow1;
                nCol2 = nCol1;
                nTab1 = nTab2 = GetCurrScTab();
                goto _common;
            case 0x45:
            case 0x65:
            case 0x25: // Area Reference                        [320 270]
            case 0x4D:
            case 0x6D:
            case 0x2D: // Area Reference Within a Name          [324    ]
                       // Area Reference Within a Shared Formula[    274]
                rStrm >> nRow1 >> nRow2 >> nCol1 >> nCol2;

                nTab1 = nTab2 = GetCurrScTab();
                goto _common;
            case 0x5A:
            case 0x7A:
            case 0x3A: // 3-D Cell Reference                    [    275]
                rStrm >> nRefIdx;
                rStrm.Ignore( 8 );
                rStrm >> nTabFirst >> nTabLast >> nRow1 >> nCol1;

                nRow2 = nRow1;
                nCol2 = nCol1;

                goto _3d_common;
            case 0x5B:
            case 0x7B:
            case 0x3B: // 3-D Area Reference                    [    276]
                rStrm >> nRefIdx;
                rStrm.Ignore( 8 );
                rStrm >> nTabFirst >> nTabLast >> nRow1 >> nRow2 >> nCol1 >> nCol2;

    _3d_common:
                nTab1 = static_cast< SCTAB >( nTabFirst );
                nTab2 = static_cast< SCTAB >( nTabLast );

                // #122885# skip references to deleted sheets
                if( (nRefIdx >= 0) || !ValidTab( nTab1 ) || (nTab1 != nTab2) )
                    break;

                goto _common;
    _common:
                // do not check abs/rel flags, linked controls have set them!
//               if( !(( nCol1 & 0xC000 ) || ( nCol2 & 0xC000 )) )
                {
                    ScRange aScRange;
                    nRow1 &= 0x3FFF;
                    nRow2 &= 0x3FFF;
                    if( GetAddressConverter().ConvertRange( aScRange, XclRange( nCol1, nRow1, nCol2, nRow2 ), nTab1, nTab2, true ) )
                        rRangeList.Append( aScRange );
                }
                break;

            case 0x03: // Addition                              [312 264]
            case 0x04: // Subtraction                           [313 264]
            case 0x05: // Multiplication                        [313 264]
            case 0x06: // Division                              [313 264]
            case 0x07: // Exponetiation                         [313 265]
            case 0x08: // Concatenation                         [313 265]
            case 0x09: // Less Than                             [313 265]
            case 0x0A: // Less Than or Equal                    [313 265]
            case 0x0B: // Equal                                 [313 265]
            case 0x0C: // Greater Than or Equal                 [313 265]
            case 0x0D: // Greater Than                          [313 265]
            case 0x0E: // Not Equal                             [313 265]
            case 0x0F: // Intersection                          [314 265]
            case 0x10: // Union                                 [314 265]
            case 0x11: // Range                                 [314 265]
            case 0x12: // Unary Plus                            [312 264]
            case 0x13: // Unary Minus                           [312 264]
            case 0x14: // Percent Sign                          [312 264]
            case 0x15: // Parenthesis                           [326 278]
            case 0x16: // Missing Argument                      [314 266]
                break;
            case 0x1C: // Error Value                           [314 266]
            case 0x1D: // Boolean                               [315 266]
                nSeek = 1;
                break;
            case 0x1E: // Integer                               [315 266]
            case 0x41:
            case 0x61:
            case 0x21: // Function, Fixed Number of Arguments   [333 282]
            case 0x49:
            case 0x69:
            case 0x29: // Variable Reference Subexpression      [331 281]
            case 0x4E:
            case 0x6E:
            case 0x2E: // Reference Subexpression Within a Name [332 282]
            case 0x4F:
            case 0x6F:
            case 0x2F: // Incomplete Reference Subexpression... [332 282]
            case 0x58:
            case 0x78:
            case 0x38: // Command-Equivalent Function           [333    ]
                nSeek = 2;
                break;
            case 0x42:
            case 0x62:
            case 0x22: // Function, Variable Number of Arg.     [333 283]
            case 0x4A:
            case 0x6A:
            case 0x2A: // Deleted Cell Reference                [323 273]
                nSeek = 3;
                break;
            case 0x01: // Array Formula                         [325    ]
                       // Array Formula or Shared Formula       [    277]
            case 0x02: // Data Table                            [325 277]
                nSeek = 4;
                break;
            case 0x46:
            case 0x66:
            case 0x26: // Constant Reference Subexpression      [321 271]
            case 0x47:
            case 0x67:
            case 0x27: // Erroneous Constant Reference Subexpr. [322 272]
            case 0x48:
            case 0x68:
            case 0x28: // Incomplete Constant Reference Subexpr.[331 281]
            case 0x4B:
            case 0x6B:
            case 0x2B: // Deleted Area Refernce                 [323 273]
                nSeek = 6;
                break;
            case 0x40:
            case 0x60:
            case 0x20: // Array Constant                        [317 268]
                nSeek = 7;
                break;
            case 0x1F: // Number                                [315 266]
                nSeek = 8;
                break;
            case 0x43:
            case 0x63:
            case 0x23: // Name                                  [318 269]
                nSeek = 14;
                break;
            case 0x5C:
            case 0x7C:
            case 0x3C: // Deleted 3-D Cell Reference            [    277]
                nSeek = 17;
                break;
            case 0x5D:
            case 0x7D:
            case 0x3D: // Deleted 3-D Area Reference            [    277]
                nSeek = 20;
                break;
            case 0x59:
            case 0x79:
            case 0x39: // Name or External Name                 [    275]
                nSeek = 24;
                break;
            case 0x17: // String Constant                       [314 266]
                nSeek = rStrm.ReaduInt8();
                break;
            case 0x19: // Special Attribute                     [327 279]
            {
                sal_uInt8 nOpt;
                sal_uInt16 nData;
                rStrm >> nOpt >> nData;
                if( nOpt & 0x04 )
                    nSeek = nData * 2 + 2;
            }
                break;
        }

        rStrm.Ignore( nSeek );
    }
    rStrm.Seek( nEndPos );

    return rRangeList.Count() != 0;
}

void ExcelToSc::DoMulArgs( DefTokenId eId, sal_uInt8 nAnz, sal_uInt8 nMinParamCount )
{
	TokenId					eParam[ 256 ];
	sal_Int32					nLauf;

	if( eId == ocCeil || eId == ocFloor )
	{
		aStack << aPool.Store( 1.0 );	// default, da in Excel nicht vorhanden
		nAnz++;
	}

    for( nLauf = 0; aStack.HasMoreTokens() && (nLauf < nAnz); nLauf++ )
		aStack >> eParam[ nLauf ];
    // #i70925# reduce parameter count, if no more tokens available on token stack
    if( nLauf < nAnz )
        nAnz = static_cast< sal_uInt8 >( nLauf );

	if( nAnz > 0 && eId == ocExternal )
	{
		TokenId				n = eParam[ nAnz - 1 ];
//##### GRUETZE FUER BASIC-FUNCS RICHTEN!
        if( const String* pExt = aPool.GetExternal( n ) )
		{
            if( const XclFunctionInfo* pFuncInfo = maFuncProv.GetFuncInfoFromXclMacroName( *pExt ) )
                aPool << pFuncInfo->meOpCode;
            else
                aPool << n;
            nAnz--;
		}
		else
			aPool << eId;
	}
	else
		aPool << eId;

	aPool << ocOpen;

	if( nAnz > 0 )
	{
		// attention: 0 = last parameter, nAnz-1 = first parameter
		sal_Int16 nNull = -1;		// skip this parameter
		sal_Int16 nSkipEnd = -1;	// skip all parameters <= nSkipEnd

		sal_Int16 nLast = nAnz - 1;

		// Funktionen, bei denen Parameter wegfallen muessen
		if( eId == ocPercentrank && nAnz == 3 )
			nSkipEnd = 0;		// letzten Parameter bei Bedarf weglassen

		// Joost-Spezialfaelle
		else if( eId == ocIf )
		{
			sal_uInt16			nNullParam = 0;
			for( nLauf = 0 ; nLauf < nAnz ; nLauf++ )
			{
				if( aPool.IsSingleOp( eParam[ nLauf ], ocMissing ) )
				{
					if( !nNullParam )
						nNullParam = (sal_uInt16) aPool.Store( ( double ) 0.0 );
					eParam[ nLauf ] = nNullParam;
				}
			}
		}

		// FIXME: ideally we'd want to import all missing args, but this
		// conflicts with lots of fn's understanding of nParams - we need
		// a function table, and pre-call argument normalisation 1st.
		sal_Int16 nLastRemovable = nLast - nMinParamCount;

		// #84453# skip missing parameters at end of parameter list
		while( nSkipEnd < nLastRemovable &&
			   aPool.IsSingleOp( eParam[ nSkipEnd + 1 ], ocMissing ) )
			nSkipEnd++;

//		fprintf (stderr, "Fn %d nSkipEnd %d nLast %d nMinParamCnt %d %d\n",
//				 eId, nSkipEnd, nLast, nMinParamCount, nLastRemovable);

		// [Parameter{;Parameter}]
		if( nLast > nSkipEnd )
		{
			aPool << eParam[ nLast ];
			for( nLauf = nLast - 1 ; nLauf > nSkipEnd ; nLauf-- )
			{
				if( nLauf != nNull )
					aPool << ocSep << eParam[ nLauf ];
			}
		}
	}
	aPool << ocClose;

	aPool >> aStack;
}


void ExcelToSc::ExcRelToScRel( sal_uInt16 nRow, sal_uInt8 nCol, ScSingleRefData &rSRD, const sal_Bool bName )
{
	if( bName )
	{
		// C O L
		if( nRow & 0x4000 )
		{//															rel Col
			rSRD.SetColRel( sal_True );
			rSRD.nRelCol = static_cast<SCsCOL>(static_cast<sal_Int8>(nCol));
		}
		else
		{//															abs Col
			rSRD.SetColRel( sal_False );
			rSRD.nCol = static_cast<SCCOL>(nCol);
		}

		// R O W
		if( nRow & 0x8000 )
		{//															rel Row
			rSRD.SetRowRel( sal_True );
			if( nRow & 0x2000 )	// Bit 13 gesetzt?
				//												-> Row negativ
				rSRD.nRelRow = static_cast<SCsROW>(static_cast<sal_Int16>(nRow | 0xC000));
			else
				//												-> Row positiv
				rSRD.nRelRow = static_cast<SCsROW>(nRow & nRowMask);
		}
		else
		{//															abs Row
			rSRD.SetRowRel( sal_False );
			rSRD.nRow = static_cast<SCROW>(nRow & nRowMask);
		}

		// T A B
		// #67965# abs needed if rel in shared formula for ScCompiler UpdateNameReference
		if ( rSRD.IsTabRel() && !rSRD.IsFlag3D() )
            rSRD.nTab = GetCurrScTab();
	}
	else
	{
		// C O L
		rSRD.SetColRel( ( nRow & 0x4000 ) > 0 );
		rSRD.nCol = static_cast<SCsCOL>(nCol);

		// R O W
		rSRD.SetRowRel( ( nRow & 0x8000 ) > 0 );
		rSRD.nRow = static_cast<SCsROW>(nRow & nRowMask);

		if ( rSRD.IsColRel() )
			rSRD.nRelCol = rSRD.nCol - aEingPos.Col();
		if ( rSRD.IsRowRel() )
			rSRD.nRelRow = rSRD.nRow - aEingPos.Row();

        // T A B
        // #i10184# abs needed if rel in shared formula for ScCompiler UpdateNameReference
        if ( rSRD.IsTabRel() && !rSRD.IsFlag3D() )
            rSRD.nTab = GetCurrScTab() + rSRD.nRelTab;
	}
}


const ScTokenArray* ExcelToSc::GetBoolErr( XclBoolError eType )
{
	sal_uInt16					nError;
	aPool.Reset();
	aStack.Reset();

	DefTokenId				eOc;

	switch( eType )
	{
        case xlErrNull:     eOc = ocStop;       nError = errNoCode;             break;
        case xlErrDiv0:     eOc = ocStop;       nError = errDivisionByZero;     break;
        case xlErrValue:    eOc = ocStop;       nError = errNoValue;            break;
        case xlErrRef:      eOc = ocStop;       nError = errNoRef;              break;
        case xlErrName:     eOc = ocStop;       nError = errNoName;             break;
        case xlErrNum:      eOc = ocStop;       nError = errIllegalFPOperation; break;
        case xlErrNA:       eOc = ocNotAvail;   nError = NOTAVAILABLE;          break;
        case xlErrTrue:     eOc = ocTrue;       nError = 0;                     break;
        case xlErrFalse:    eOc = ocFalse;      nError = 0;                     break;
        case xlErrUnknown:  eOc = ocStop;       nError = errUnknownState;       break;
		default:
            DBG_ERROR( "ExcelToSc::GetBoolErr - wrong enum!" );
			eOc = ocNoName;
			nError = errUnknownState;
	}

	aPool << eOc;
	if( eOc != ocStop )
		aPool << ocOpen << ocClose;

	aPool >> aStack;

	const ScTokenArray*		pErgebnis = aPool[ aStack.Get() ];
	if( nError )
		( ( ScTokenArray* ) pErgebnis )->SetCodeError( nError );

	( ( ScTokenArray* ) pErgebnis )->SetRecalcModeNormal();

	return pErgebnis;
}


// if a shared formula was found, stream seeks to first byte after <nFormulaLen>,
// else stream pointer stays unchanged
sal_Bool ExcelToSc::GetShrFmla( const ScTokenArray*& rpErgebnis, XclImpStream& aIn, sal_Size nFormulaLen )
{
	sal_uInt8			nOp;
	sal_Bool			bRet = sal_True;

	if( nFormulaLen == 0 )
		bRet = sal_False;
	else
	{
		aIn.PushPosition();

		aIn >> nOp;

		if( nOp == 0x01 )	// Shared Formula		[    277]
		{
			sal_uInt16 nCol, nRow;

			aIn >> nRow >> nCol;

            aStack << aPool.Store( GetOldRoot().pShrfmlaBuff->Find(
                ScAddress( static_cast<SCCOL>(nCol), static_cast<SCROW>(nRow), GetCurrScTab() ) ) );

			bRet = sal_True;
		}
		else
			bRet = sal_False;

		aIn.PopPosition();
	}

	if( bRet )
	{
        aIn.Ignore( nFormulaLen );
		rpErgebnis = aPool[ aStack.Get() ];
	}
	else
		rpErgebnis = NULL;

	return bRet;
}


#if 0
sal_Bool ExcelToSc::SetCurVal( ScFormulaCell &rCell, double &rfCurVal )
{
	sal_uInt16	nInd;
	sal_uInt8	nType;
	sal_uInt8	nVal;
	sal_Bool	bString = sal_False;

#ifdef OSL_BIGENDIAN
	// Code fuer alle anstaendigen Prozessoren
	nType = *( ( ( sal_uInt8 * ) &rfCurVal ) + 7 );
	nVal = *( ( ( sal_uInt8 * ) &rfCurVal ) + 5 );
	nInd = *( ( sal_uInt16 * ) &rfCurVal );
#else
	// fuer Schund-Prozessoren
	nType = *( ( sal_uInt8 * ) &rfCurVal );
	nVal = *( ( ( sal_uInt8 * ) &rfCurVal ) + 2 );
	nInd = *( ( ( sal_uInt16 * ) &rfCurVal ) + 3 );
#endif

	if( ( sal_uInt16 ) ~nInd )
		// Wert ist Float
		rCell.SetHybridDouble( rfCurVal );
	else
	{
		switch( nType )
		{
			case 0:		// String
				bString = sal_True;
				break;
			case 1:		// Bool
				if( nVal )
					rfCurVal = 1.0;
				else
					rfCurVal = 0.0;
				rCell.SetHybridDouble( rfCurVal );
				break;
			case 2:		// Error
                rCell.SetErrCode( XclTools::GetScErrorCode( nVal ) );
				break;
		}
	}

	return bString;
}
#endif


void ExcelToSc::SetError( ScFormulaCell &rCell, const ConvErr eErr )
{
	sal_uInt16	nInd;

	switch( eErr )
	{
		case ConvErrNi:			nInd = errUnknownToken; break;
		case ConvErrNoMem:		nInd = errCodeOverflow; break;
		case ConvErrExternal:	nInd = errNoName; break;
		case ConvErrCount:		nInd = errCodeOverflow; break;
		default:				nInd = errNoCode;	// hier fiel mir nichts
													//  Besseres ein...
	}

	rCell.SetErrCode( nInd );
}


void ExcelToSc::SetComplCol( ScComplexRefData &rCRD )
{
	ScSingleRefData	&rSRD = rCRD.Ref2;
	if( rSRD.IsColRel() )
		rSRD.nRelCol = MAXCOL - aEingPos.Col();
	else
		rSRD.nCol = MAXCOL;
}


void ExcelToSc::SetComplRow( ScComplexRefData &rCRD )
{
	ScSingleRefData	&rSRD = rCRD.Ref2;
	if( rSRD.IsRowRel() )
		rSRD.nRelRow = MAXROW - aEingPos.Row();
	else
		rSRD.nRow = MAXROW;
}

void ExcelToSc::ReadExtensionArray( unsigned int n, XclImpStream& aIn )
{
    // printf( "inline array;\n" );

    sal_uInt8        nByte;
    sal_uInt16      nUINT16;
    double      fDouble;
    String      aString;
    ScMatrix*   pMatrix;

    aIn >> nByte >> nUINT16;

    SCSIZE nC, nCols;
    SCSIZE nR, nRows;
    if( GetBiff() == EXC_BIFF8 )
    {
        nCols = nByte + 1;
        nRows = nUINT16 + 1;
    }
    else
    {
        nCols = nByte ? nByte : 256;
        nRows = nUINT16;
    }

    pMatrix = aPool.GetMatrix( n );

    if( NULL != pMatrix )
    {
        pMatrix->Resize(nCols, nRows);
        pMatrix->GetDimensions( nC, nR);
        if( nC != nCols || nR != nRows )
        {
            DBG_ERRORFILE( "ExcelToSc::ReadExtensionArray - matrix size mismatch" );
            pMatrix = NULL;
        }
    }
    else
    {
        DBG_ERRORFILE( "ExcelToSc::ReadExtensionArray - missing matrix" );
    }

    for( nR = 0 ; nR < nRows; nR++ )
    {
        for( nC = 0 ; nC < nCols; nC++ )
        {
            aIn >> nByte;
            switch( nByte )
            {
                case EXC_CACHEDVAL_EMPTY:
                    aIn.Ignore( 8 );
                    if( NULL != pMatrix )
                    {
                        pMatrix->PutEmpty( nC, nR );
                    }
                    break;

                case EXC_CACHEDVAL_DOUBLE:
                    aIn >> fDouble;
                    if( NULL != pMatrix )
                    {
                        pMatrix->PutDouble( fDouble, nC, nR );
                    }
                    break;

                case EXC_CACHEDVAL_STRING:
                    if( GetBiff() == EXC_BIFF8 )
                    {
                        aIn >> nUINT16;
                        aString = aIn.ReadUniString( nUINT16 );
                    }
                    else
                    {
                        aIn >> nByte;
                        aString = aIn.ReadRawByteString( nByte );
                    }
                    if( NULL != pMatrix )
                    {
                        pMatrix->PutString( aString, nC, nR );
                    }
                    break;

                case EXC_CACHEDVAL_BOOL:
                    aIn >> nByte;
                    aIn.Ignore( 7 );
                    if( NULL != pMatrix )
                    {
                        pMatrix->PutBoolean( nByte != 0, nC, nR );
                    }
                    break;

                case EXC_CACHEDVAL_ERROR:
                    aIn >> nByte;
                    aIn.Ignore( 7 );
                    if( NULL != pMatrix )
                    {
                        pMatrix->PutError( XclTools::GetScErrorCode( nByte ), nC, nR );
                    }
                    break;
            }
        }
    }
}

void ExcelToSc::ReadExtensionNlr( XclImpStream& aIn )
{
    // printf( "natural lang fmla;\n" );

    sal_uInt32 nFlags;
    aIn >> nFlags;

    sal_uInt32 nCount = nFlags & EXC_TOK_NLR_ADDMASK;
    aIn.Ignore( nCount * 4 ); // Drop the cell positions
}

void ExcelToSc::ReadExtensionMemArea( XclImpStream& aIn )
{
    // printf( "mem area;\n" );

    sal_uInt16 nCount;
    aIn >> nCount;

    aIn.Ignore( nCount * ((GetBiff() == EXC_BIFF8) ? 8 : 6) ); // drop the ranges
}

void ExcelToSc::ReadExtensions( const ExtensionTypeVec& rExtensions,
                                XclImpStream& aIn )
{
    unsigned int nArray = 0;

    for( unsigned int i = 0 ; i < rExtensions.size() ; i++ )
    {
        ExtensionType eType = rExtensions[i];

        switch( eType )
        {
            case EXTENSION_ARRAY:
                ReadExtensionArray( nArray++, aIn );
                break;

            case EXTENSION_NLR:
                ReadExtensionNlr( aIn );
                break;

            case EXTENSION_MEMAREA:
                ReadExtensionMemArea( aIn );
                break;
        }
    }
}

