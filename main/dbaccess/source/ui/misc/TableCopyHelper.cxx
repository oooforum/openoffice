/**************************************************************
 * 
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 * 
 *   http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 * 
 *************************************************************/



// MARKER(update_precomp.py): autogen include statement, do not remove
#include "precompiled_dbui.hxx"
#ifndef DBUI_TABLECOPYHELPER_HXX
#include "TableCopyHelper.hxx"
#endif
#ifndef DBACCESS_SHARED_DBUSTRINGS_HRC
#include "dbustrings.hrc"
#endif
#ifndef _DBAUI_SQLMESSAGE_HXX_
#include "sqlmessage.hxx"
#endif
#ifndef _SV_MSGBOX_HXX
#include <vcl/msgbox.hxx>
#endif
#ifndef DBAUI_WIZ_COPYTABLEDIALOG_HXX
#include "WCopyTable.hxx"
#endif
#ifndef DBAUI_GENERICCONTROLLER_HXX
#include "genericcontroller.hxx"
#endif
#ifndef DBAUI_WIZARD_CPAGE_HXX
#include "WCPage.hxx"
#endif
#ifndef _COM_SUN_STAR_TASK_XINTERACTIONHANDLER_HPP_
#include <com/sun/star/task/XInteractionHandler.hpp>
#endif
#ifndef _COM_SUN_STAR_SDB_XSINGLESELECTQUERYCOMPOSER_HPP_
#include <com/sun/star/sdb/XSingleSelectQueryComposer.hpp>
#endif
#ifndef _COM_SUN_STAR_SDB_APPLICATION_COPYTABLEOPERATION_HPP_
#include <com/sun/star/sdb/application/CopyTableOperation.hpp>
#endif
#ifndef _COM_SUN_STAR_SDB_APPLICATION_COPYTABLEWIZARD_HPP_
#include <com/sun/star/sdb/application/CopyTableWizard.hpp>
#endif
#ifndef _COM_SUN_STAR_SDB_DATAACCESSDESCRIPTORFACTORY_HPP_
#include <com/sun/star/sdb/DataAccessDescriptorFactory.hpp>
#endif

#ifndef DBAUI_RTFREADER_HXX
#include "RtfReader.hxx"
#endif
#ifndef DBAUI_HTMLREADER_HXX
#include "HtmlReader.hxx"
#endif
#ifndef DBAUI_TOKENWRITER_HXX
#include "TokenWriter.hxx"
#endif
#ifndef DBAUI_TOOLS_HXX
#include "UITools.hxx"
#endif
#ifndef DBAUI_DATAVIEW_HXX
#include "dataview.hxx"
#endif
#ifndef _DBU_RESOURCE_HRC_
#include "dbu_resource.hrc"
#endif
#ifndef _UNOTOOLS_UCBHELPER_HXX
#include <unotools/ucbhelper.hxx>
#endif
#ifndef _URLOBJ_HXX
#include <tools/urlobj.hxx>
#endif
#ifndef TOOLS_DIAGNOSE_EX_H
#include <tools/diagnose_ex.h>
#endif
#ifndef COMPHELPER_COMPONENTCONTEXT_HXX
#include <comphelper/componentcontext.hxx>
#endif
#ifndef _COM_SUN_STAR_SDBCX_XTABLESSUPPLIER_HPP_
#include <com/sun/star/sdbcx/XTablesSupplier.hpp>
#endif
#ifndef _COM_SUN_STAR_SDBCX_XVIEWSSUPPLIER_HPP_
#include <com/sun/star/sdbcx/XViewsSupplier.hpp>
#endif
#ifndef _COM_SUN_STAR_SDB_XQUERYDEFINITIONSSUPPLIER_HPP_
#include <com/sun/star/sdb/XQueryDefinitionsSupplier.hpp>
#endif
#ifndef _COM_SUN_STAR_SDB_SQLCONTEXT_HPP_
#include <com/sun/star/sdb/SQLContext.hpp>
#endif
#ifndef _COM_SUN_STAR_SDBC_XPARAMETERS_HPP_
#include <com/sun/star/sdbc/XParameters.hpp>
#endif
#ifndef _COM_SUN_STAR_SDBC_XRESULTSETMETADATASUPPLIER_HPP_
#include <com/sun/star/sdbc/XResultSetMetaDataSupplier.hpp>
#endif
#ifndef _COM_SUN_STAR_SDB_XQUERIESSUPPLIER_HPP_
#include <com/sun/star/sdb/XQueriesSupplier.hpp>
#endif
#ifndef _COM_SUN_STAR_SDBC_XCOLUMNLOCATE_HPP_
#include <com/sun/star/sdbc/XColumnLocate.hpp>
#endif
#ifndef _COM_SUN_STAR_SDBCX_XROWLOCATE_HPP_
#include <com/sun/star/sdbcx/XRowLocate.hpp>
#endif
#ifndef _SV_WAITOBJ_HXX
#include <vcl/waitobj.hxx>
#endif
#ifndef _COM_SUN_STAR_SDB_XSQLQUERYCOMPOSERFACTORY_HPP_
#include <com/sun/star/sdb/XSQLQueryComposerFactory.hpp>
#endif
#ifndef _UNOTOOLS_TEMPFILE_HXX
#include <unotools/tempfile.hxx>
#endif
#ifndef _CPPUHELPER_EXC_HLP_HXX_
#include <cppuhelper/exc_hlp.hxx>
#endif

#ifndef DBAUI_DBEXCHANGE_HXX
#include "dbexchange.hxx"
#endif
#include <rtl/logfile.hxx>
//........................................................................
namespace dbaui
{
//........................................................................
using namespace ::dbtools;
using namespace ::svx;
using namespace ::com::sun::star::uno;
using namespace ::com::sun::star::task;
using namespace ::com::sun::star::beans;
using namespace ::com::sun::star::lang;
using namespace ::com::sun::star::container;
using namespace ::com::sun::star::sdb;
using namespace ::com::sun::star::sdb::application;
using namespace ::com::sun::star::sdbc;
using namespace ::com::sun::star::sdbcx;
using namespace ::com::sun::star::frame;
using namespace ::com::sun::star::ucb;

// -----------------------------------------------------------------------------
OTableCopyHelper::OTableCopyHelper(OGenericUnoController* _pControler)
    :m_pController(_pControler)
{
}

// -----------------------------------------------------------------------------
void OTableCopyHelper::insertTable( const ::rtl::OUString& i_rSourceDataSource, const Reference<XConnection>& i_rSourceConnection,
        const ::rtl::OUString& i_rCommand, const sal_Int32 i_nCommandType,
        const Reference< XResultSet >& i_rSourceRows, const Sequence< Any >& i_rSelection, const sal_Bool i_bBookmarkSelection,
        const ::rtl::OUString& i_rDestDataSource, const Reference<XConnection>& i_rDestConnection)
{
    if ( CommandType::QUERY != i_nCommandType && CommandType::TABLE != i_nCommandType )
    {
		DBG_ERROR( "OTableCopyHelper::insertTable: invalid call (no supported format found)!" );
        return;
    }

	try
	{
        Reference<XConnection> xSrcConnection( i_rSourceConnection );
        if ( i_rSourceDataSource == i_rDestDataSource )
            xSrcConnection = i_rDestConnection;

        if ( !xSrcConnection.is() || !i_rDestConnection.is() )
        {
            OSL_ENSURE( false, "OTableCopyHelper::insertTable: no connection/s!" );
            return;
        }

        ::comphelper::ComponentContext aContext( m_pController->getORB() );

        Reference< XDataAccessDescriptorFactory > xFactory( DataAccessDescriptorFactory::get( aContext.getUNOContext() ) );

        Reference< XPropertySet > xSource( xFactory->createDataAccessDescriptor(), UNO_SET_THROW );
        xSource->setPropertyValue( PROPERTY_COMMAND_TYPE, makeAny( i_nCommandType ) );
        xSource->setPropertyValue( PROPERTY_COMMAND, makeAny( i_rCommand ) );
        xSource->setPropertyValue( PROPERTY_ACTIVE_CONNECTION, makeAny( xSrcConnection ) );
        xSource->setPropertyValue( PROPERTY_RESULT_SET, makeAny( i_rSourceRows ) );
        xSource->setPropertyValue( PROPERTY_SELECTION, makeAny( i_rSelection ) );
        xSource->setPropertyValue( PROPERTY_BOOKMARK_SELECTION, makeAny( i_bBookmarkSelection ) );

        Reference< XPropertySet > xDest( xFactory->createDataAccessDescriptor(), UNO_SET_THROW );
        xDest->setPropertyValue( PROPERTY_ACTIVE_CONNECTION, makeAny( i_rDestConnection ) );

        Reference< XCopyTableWizard > xWizard( CopyTableWizard::create( aContext.getUNOContext(), xSource, xDest ), UNO_SET_THROW );

        ::rtl::OUString sTableNameForAppend( GetTableNameForAppend() );
        xWizard->setDestinationTableName( GetTableNameForAppend() );

        bool bAppendToExisting = ( sTableNameForAppend.getLength() != 0 );
        xWizard->setOperation( bAppendToExisting ? CopyTableOperation::AppendData : CopyTableOperation::CopyDefinitionAndData );

        xWizard->execute();
	}
	catch( const SQLException& )
    {
        m_pController->showError( SQLExceptionInfo( ::cppu::getCaughtException() ) );
    }
	catch( const Exception& )
	{
        DBG_UNHANDLED_EXCEPTION();
	}
}

// -----------------------------------------------------------------------------
void OTableCopyHelper::pasteTable( const ::svx::ODataAccessDescriptor& _rPasteData, const ::rtl::OUString& i_rDestDataSourceName,
                                  const SharedConnection& i_rDestConnection )
{
    ::rtl::OUString sSrcDataSourceName = _rPasteData.getDataSource();

    ::rtl::OUString sCommand;
    _rPasteData[ daCommand ] >>= sCommand;

    Reference<XConnection> xSrcConnection;
    if ( _rPasteData.has(daConnection) )
    {
        OSL_VERIFY( _rPasteData[daConnection] >>= xSrcConnection );
    }

    Reference< XResultSet > xResultSet;
    if ( _rPasteData.has(daCursor) )
    {
        OSL_VERIFY( _rPasteData[ daCursor ] >>= xResultSet );
    }

    Sequence< Any > aSelection;
    if ( _rPasteData.has( daSelection ) )
    {
        OSL_VERIFY( _rPasteData[ daSelection ] >>= aSelection );
        OSL_ENSURE( _rPasteData.has( daBookmarkSelection ), "OTableCopyHelper::pasteTable: you should specify BookmarkSelection, too, to be on the safe side!" );
    }


    sal_Bool bBookmarkSelection( sal_True );
    if ( _rPasteData.has( daBookmarkSelection ) )
    {
        OSL_VERIFY( _rPasteData[ daBookmarkSelection ] >>= bBookmarkSelection );
    }
    OSL_ENSURE( bBookmarkSelection, "OTableCopyHelper::pasteTable: working with selection-indicies (instead of bookmarks) is error-prone, and thus deprecated!" );

    sal_Int32 nCommandType = CommandType::COMMAND;
    if ( _rPasteData.has(daCommandType) )
        _rPasteData[daCommandType] >>= nCommandType;

    insertTable( sSrcDataSourceName, xSrcConnection, sCommand, nCommandType,
                 xResultSet, aSelection, bBookmarkSelection,
                 i_rDestDataSourceName, i_rDestConnection );
}

// -----------------------------------------------------------------------------
void OTableCopyHelper::pasteTable( SotFormatStringId _nFormatId
                                  ,const TransferableDataHelper& _rTransData 
                                  ,const ::rtl::OUString& i_rDestDataSource
                                  ,const SharedConnection& _xConnection)
{
    if ( _nFormatId == SOT_FORMATSTR_ID_DBACCESS_TABLE || _nFormatId == SOT_FORMATSTR_ID_DBACCESS_QUERY )
	{
		if ( ODataAccessObjectTransferable::canExtractObjectDescriptor(_rTransData.GetDataFlavorExVector()) )
		{
			::svx::ODataAccessDescriptor aPasteData = ODataAccessObjectTransferable::extractObjectDescriptor(_rTransData);
			pasteTable( aPasteData,i_rDestDataSource,_xConnection);
		}
	}
	else if ( _rTransData.HasFormat(_nFormatId) )
	{
		try
		{
			DropDescriptor aTrans;
			if ( _nFormatId != SOT_FORMAT_RTF )
				const_cast<TransferableDataHelper&>(_rTransData).GetSotStorageStream(SOT_FORMATSTR_ID_HTML ,aTrans.aHtmlRtfStorage);
			else
				const_cast<TransferableDataHelper&>(_rTransData).GetSotStorageStream(SOT_FORMAT_RTF,aTrans.aHtmlRtfStorage);

			aTrans.nType			= E_TABLE;
			aTrans.bHtml			= SOT_FORMATSTR_ID_HTML == _nFormatId;
			aTrans.sDefaultTableName = GetTableNameForAppend();
			if ( !copyTagTable(aTrans,sal_False,_xConnection) )
				m_pController->showError(SQLException(String(ModuleRes(STR_NO_TABLE_FORMAT_INSIDE)),*m_pController,::rtl::OUString::createFromAscii("S1000") ,0,Any()));
		}
        catch(const SQLException&)
        {
            m_pController->showError( SQLExceptionInfo( ::cppu::getCaughtException() ) );
        }
        catch( const Exception& )
        {
            DBG_UNHANDLED_EXCEPTION();
        }
	}
	else
		m_pController->showError(SQLException(String(ModuleRes(STR_NO_TABLE_FORMAT_INSIDE)),*m_pController,::rtl::OUString::createFromAscii("S1000") ,0,Any()));
}

// -----------------------------------------------------------------------------
void OTableCopyHelper::pasteTable( const TransferableDataHelper& _rTransData 
                                  ,const ::rtl::OUString& i_rDestDataSource
                                  ,const SharedConnection& _xConnection)
{
	if ( _rTransData.HasFormat(SOT_FORMATSTR_ID_DBACCESS_TABLE) || _rTransData.HasFormat(SOT_FORMATSTR_ID_DBACCESS_QUERY) )
		pasteTable( SOT_FORMATSTR_ID_DBACCESS_TABLE,_rTransData,i_rDestDataSource,_xConnection);
	else if ( _rTransData.HasFormat(SOT_FORMATSTR_ID_HTML) )
		pasteTable( SOT_FORMATSTR_ID_HTML,_rTransData,i_rDestDataSource,_xConnection);
	else if ( _rTransData.HasFormat(SOT_FORMAT_RTF) )
		pasteTable( SOT_FORMAT_RTF,_rTransData,i_rDestDataSource,_xConnection);
}

// -----------------------------------------------------------------------------
sal_Bool OTableCopyHelper::copyTagTable(OTableCopyHelper::DropDescriptor& _rDesc, sal_Bool _bCheck,const SharedConnection& _xConnection)
{
    Reference<XEventListener> xEvt;
	ODatabaseImportExport* pImport = NULL;
	if ( _rDesc.bHtml )
		pImport = new OHTMLImportExport(_xConnection,getNumberFormatter(_xConnection,m_pController->getORB()),m_pController->getORB());
	else
		pImport = new ORTFImportExport(_xConnection,getNumberFormatter(_xConnection,m_pController->getORB()),m_pController->getORB());

	xEvt = pImport;
	SvStream* pStream = (SvStream*)(SotStorageStream*)_rDesc.aHtmlRtfStorage;
	if ( _bCheck )
		pImport->enableCheckOnly();

	//dyf add 20070601
	//set the selected tablename
	pImport->setSTableName(_rDesc.sDefaultTableName);
	//dyf add end 
	pImport->setStream(pStream);
	return pImport->Read();
}
// -----------------------------------------------------------------------------
sal_Bool OTableCopyHelper::isTableFormat(const TransferableDataHelper& _rClipboard)  const
{
	sal_Bool bTableFormat	=	_rClipboard.HasFormat(SOT_FORMATSTR_ID_DBACCESS_TABLE)
				||	_rClipboard.HasFormat(SOT_FORMATSTR_ID_DBACCESS_QUERY)
				||	_rClipboard.HasFormat(SOT_FORMAT_RTF)
				||	_rClipboard.HasFormat(SOT_FORMATSTR_ID_HTML);

	return bTableFormat;
}
// -----------------------------------------------------------------------------
sal_Bool OTableCopyHelper::copyTagTable(const TransferableDataHelper& _aDroppedData
                                        ,DropDescriptor& _rAsyncDrop
                                        ,const SharedConnection& _xConnection)
{
    sal_Bool bRet = sal_False;
    sal_Bool bHtml = _aDroppedData.HasFormat(SOT_FORMATSTR_ID_HTML);
	if ( bHtml || _aDroppedData.HasFormat(SOT_FORMAT_RTF) )
	{
		if ( bHtml )
			const_cast<TransferableDataHelper&>(_aDroppedData).GetSotStorageStream(SOT_FORMATSTR_ID_HTML ,_rAsyncDrop.aHtmlRtfStorage);
		else
			const_cast<TransferableDataHelper&>(_aDroppedData).GetSotStorageStream(SOT_FORMAT_RTF,_rAsyncDrop.aHtmlRtfStorage);

		_rAsyncDrop.bHtml			= bHtml;
		_rAsyncDrop.bError			= !copyTagTable(_rAsyncDrop,sal_True,_xConnection);

        bRet = ( !_rAsyncDrop.bError && _rAsyncDrop.aHtmlRtfStorage.Is() );
		if ( bRet )
		{
			// now we need to copy the stream
			::utl::TempFile aTmp;
			aTmp.EnableKillingFile(sal_False);
			_rAsyncDrop.aUrl = aTmp.GetURL();
			SotStorageStreamRef aNew = new SotStorageStream( aTmp.GetFileName() );
			_rAsyncDrop.aHtmlRtfStorage->Seek(STREAM_SEEK_TO_BEGIN);
			_rAsyncDrop.aHtmlRtfStorage->CopyTo( aNew );
			aNew->Commit();
			_rAsyncDrop.aHtmlRtfStorage = aNew;
		}
		else
			_rAsyncDrop.aHtmlRtfStorage = NULL;
	}
    return bRet;
}
// -----------------------------------------------------------------------------
void OTableCopyHelper::asyncCopyTagTable(  DropDescriptor& _rDesc
                                ,const ::rtl::OUString& i_rDestDataSource
                                ,const SharedConnection& _xConnection)
{
    if ( _rDesc.aHtmlRtfStorage.Is() )
	{
		copyTagTable(_rDesc,sal_False,_xConnection);
		_rDesc.aHtmlRtfStorage = NULL;
		// we now have to delete the temp file created in executeDrop
		INetURLObject aURL;
		aURL.SetURL(_rDesc.aUrl);
		::utl::UCBContentHelper::Kill(aURL.GetMainURL(INetURLObject::NO_DECODE));
	}
	else if ( !_rDesc.bError )
		pasteTable(_rDesc.aDroppedData,i_rDestDataSource,_xConnection);
	else
		m_pController->showError(SQLException(String(ModuleRes(STR_NO_TABLE_FORMAT_INSIDE)),*m_pController,::rtl::OUString::createFromAscii("S1000") ,0,Any()));
}
// -----------------------------------------------------------------------------
//........................................................................
}	// namespace dbaui
//........................................................................

