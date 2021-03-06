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
#include "precompiled_framework.hxx"

//_________________________________________________________________________________________________________________
//	my own includes
//_________________________________________________________________________________________________________________
#include <uifactory/uielementfactorymanager.hxx>
#include <uifactory/windowcontentfactorymanager.hxx>
#include <threadhelp/resetableguard.hxx>
#include "services.h"

//_________________________________________________________________________________________________________________
//	interface includes
//_________________________________________________________________________________________________________________
#include <com/sun/star/beans/PropertyValue.hpp>
#include <com/sun/star/beans/XPropertySet.hpp>
#include <com/sun/star/container/XNameAccess.hpp>
#include <com/sun/star/container/XNameContainer.hpp>
#include <com/sun/star/container/XContainer.hpp>
#include <com/sun/star/frame/XFrame.hpp>

//_________________________________________________________________________________________________________________
//	includes of other projects
//_________________________________________________________________________________________________________________
#include <rtl/ustrbuf.hxx>
#include <cppuhelper/weak.hxx>
#include <tools/urlobj.hxx>
#include <vcl/svapp.hxx>
#include <rtl/logfile.hxx>
//_________________________________________________________________________________________________________________
//	Defines
//_________________________________________________________________________________________________________________
// 

using namespace com::sun::star::uno;
using namespace com::sun::star::lang;
using namespace com::sun::star::beans;
using namespace com::sun::star::frame;
using namespace com::sun::star::container;
using namespace ::com::sun::star::ui;
using namespace ::com::sun::star::frame;

//_________________________________________________________________________________________________________________
//	Namespace
//_________________________________________________________________________________________________________________
// 

namespace framework
{

// global function needed by both implementations
rtl::OUString getHashKeyFromStrings( const rtl::OUString& aType, const rtl::OUString& aName, const rtl::OUString& aModuleName )
{
    rtl::OUStringBuffer aKey( aType );
    aKey.appendAscii( "^" );
    aKey.append( aName );
    aKey.appendAscii( "^" );
    aKey.append( aModuleName );
    return aKey.makeStringAndClear();
}


//*****************************************************************************************************************
//	Configuration access class for UIElementFactoryManager implementation
//*****************************************************************************************************************


ConfigurationAccess_FactoryManager::ConfigurationAccess_FactoryManager( Reference< XMultiServiceFactory >& rServiceManager,const ::rtl::OUString& _sRoot ) :
    ThreadHelpBase(),
    m_aPropType( RTL_CONSTASCII_USTRINGPARAM( "Type" )),
    m_aPropName( RTL_CONSTASCII_USTRINGPARAM( "Name" )),
    m_aPropModule( RTL_CONSTASCII_USTRINGPARAM( "Module" )),
    m_aPropFactory( RTL_CONSTASCII_USTRINGPARAM( "FactoryImplementation" )),
    m_sRoot(_sRoot),
    m_xServiceManager( rServiceManager ),
    m_bConfigAccessInitialized( sal_False ),
    m_bConfigDirty(true)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::ConfigurationAccess_FactoryManager" );
    m_xConfigProvider = Reference< XMultiServiceFactory >( rServiceManager->createInstance( SERVICENAME_CFGPROVIDER),UNO_QUERY );
}

ConfigurationAccess_FactoryManager::~ConfigurationAccess_FactoryManager()
{
    // SAFE
    ResetableGuard aLock( m_aLock );
    
    Reference< XContainer > xContainer( m_xConfigAccess, UNO_QUERY );
    if ( xContainer.is() )
        xContainer->removeContainerListener( this );
}

rtl::OUString ConfigurationAccess_FactoryManager::getFactorySpecifierFromTypeNameModule( const rtl::OUString& rType, const rtl::OUString& rName, const rtl::OUString& rModule ) const
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::getFactorySpecifierFromTypeNameModule" );
    // SAFE
    ResetableGuard aLock( m_aLock );

    FactoryManagerMap::const_iterator pIter = 
        m_aFactoryManagerMap.find( getHashKeyFromStrings( rType, rName, rModule ));
    if ( pIter != m_aFactoryManagerMap.end() )
        return pIter->second;
    else
    {
        pIter = m_aFactoryManagerMap.find( getHashKeyFromStrings( rType, rName, rtl::OUString() ));
        if ( pIter != m_aFactoryManagerMap.end() )
            return pIter->second;
        else
        {
            // Support factories which uses a defined prefix before the ui name.
            sal_Int32 nIndex = rName.indexOf( '_' );
            if ( nIndex > 0 )
            {
                rtl::OUString aName = rName.copy( 0, nIndex+1 );
                pIter = m_aFactoryManagerMap.find( getHashKeyFromStrings( rType, aName, rtl::OUString() ));
                if ( pIter != m_aFactoryManagerMap.end() )
                    return pIter->second;
            }

            pIter = m_aFactoryManagerMap.find( getHashKeyFromStrings( rType, rtl::OUString(), rtl::OUString() ));
            if ( pIter != m_aFactoryManagerMap.end() )
                return pIter->second;
        }
    }
    
    return rtl::OUString();
}

void ConfigurationAccess_FactoryManager::addFactorySpecifierToTypeNameModule( const rtl::OUString& rType, const rtl::OUString& rName, const rtl::OUString& rModule, const rtl::OUString& rServiceSpecifier )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::addFactorySpecifierToTypeNameModule" );
    // SAFE
    ResetableGuard aLock( m_aLock );

    rtl::OUString aHashKey = getHashKeyFromStrings( rType, rName, rModule );

    FactoryManagerMap::const_iterator pIter = m_aFactoryManagerMap.find( aHashKey );
    
    if ( pIter != m_aFactoryManagerMap.end() )
        throw ElementExistException();
    else
        m_aFactoryManagerMap.insert( FactoryManagerMap::value_type( aHashKey, rServiceSpecifier ));
}


void ConfigurationAccess_FactoryManager::removeFactorySpecifierFromTypeNameModule( const rtl::OUString& rType, const rtl::OUString& rName, const rtl::OUString& rModule )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::removeFactorySpecifierFromTypeNameModule" );
    // SAFE
    ResetableGuard aLock( m_aLock );
    
    rtl::OUString aHashKey = getHashKeyFromStrings( rType, rName, rModule );

    FactoryManagerMap::const_iterator pIter = m_aFactoryManagerMap.find( aHashKey );
    
    if ( pIter == m_aFactoryManagerMap.end() )
        throw NoSuchElementException();
    else
        m_aFactoryManagerMap.erase( aHashKey );
}

Sequence< Sequence< PropertyValue > > ConfigurationAccess_FactoryManager::getFactoriesDescription() const
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::getFactoriesDescription" );
    // SAFE
    ResetableGuard aLock( m_aLock );
    
    Sequence< Sequence< PropertyValue > > aSeqSeq;

    sal_Int32 nIndex( 0 );
    FactoryManagerMap::const_iterator pIter = m_aFactoryManagerMap.begin();
    while ( pIter != m_aFactoryManagerMap.end() )
    {
        rtl::OUString aFactory = pIter->first;
        if ( aFactory.getLength() > 0 )
        {
            sal_Int32                 nToken = 0;
            Sequence< PropertyValue > aSeq( 1 );

            aSeqSeq.realloc( aSeqSeq.getLength() + 1 );
            aSeq[0].Name  = m_aPropType;
            aSeq[0].Value = makeAny( aFactory.getToken( 0, '^', nToken ));
            if ( nToken > 0 )
            {
                aSeq.realloc( 2 );
                aSeq[1].Name  = m_aPropName;
                aSeq[1].Value = makeAny( aFactory.getToken( 0, '^', nToken ));
                if ( nToken > 0 )
                {
                    aSeq.realloc( 3 );
                    aSeq[2].Name  = m_aPropModule;
                    aSeq[2].Value = makeAny( aFactory.getToken( 0, '^', nToken ));
                }
            }
            
            aSeqSeq[nIndex++] = aSeq;
        }
        
        ++pIter;
    }

    return aSeqSeq;
}

// container.XContainerListener
void SAL_CALL ConfigurationAccess_FactoryManager::elementInserted( const ContainerEvent& aEvent ) throw(RuntimeException)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::elementInserted" );
    rtl::OUString   aType;
    rtl::OUString   aName;
    rtl::OUString   aModule;
    rtl::OUString   aService;
    
    // SAFE
    ResetableGuard aLock( m_aLock );

    if ( impl_getElementProps( aEvent.Element, aType, aName, aModule, aService ))
    {
        // Create hash key from type, name and module as they are together a primary key to
        // the UNO service that implements a user interface factory.
        rtl::OUString aHashKey( getHashKeyFromStrings( aType, aName, aModule ));
        m_aFactoryManagerMap.insert( FactoryManagerMap::value_type( aHashKey, aService ));
    }
}

void SAL_CALL ConfigurationAccess_FactoryManager::elementRemoved ( const ContainerEvent& aEvent ) throw(RuntimeException)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::elementRemoved " );
    rtl::OUString   aType;
    rtl::OUString   aName;
    rtl::OUString   aModule;
    rtl::OUString   aService;
    
    // SAFE
    ResetableGuard aLock( m_aLock );
    
    if ( impl_getElementProps( aEvent.Element, aType, aName, aModule, aService ))
    {
        // Create hash key from command and model as they are together a primary key to
        // the UNO service that implements the popup menu controller.
        rtl::OUString aHashKey( getHashKeyFromStrings( aType, aName, aModule ));
        m_aFactoryManagerMap.erase( aHashKey );
    }
}

void SAL_CALL ConfigurationAccess_FactoryManager::elementReplaced( const ContainerEvent& aEvent ) throw(RuntimeException)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::elementReplaced" );
    rtl::OUString   aType;
    rtl::OUString   aName;
    rtl::OUString   aModule;
    rtl::OUString   aService;
    
    // SAFE
    ResetableGuard aLock( m_aLock );

    if ( impl_getElementProps( aEvent.Element, aType, aName, aModule, aService ))
    {
        // Create hash key from command and model as they are together a primary key to
        // the UNO service that implements the popup menu controller.
        rtl::OUString aHashKey( getHashKeyFromStrings( aType, aName, aModule ));
        m_aFactoryManagerMap.erase( aHashKey );
        m_aFactoryManagerMap.insert( FactoryManagerMap::value_type( aHashKey, aService ));
    }
}

// lang.XEventListener
void SAL_CALL ConfigurationAccess_FactoryManager::disposing( const EventObject& ) throw(RuntimeException)
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::disposing" );
    // SAFE
    // remove our reference to the config access
    ResetableGuard aLock( m_aLock );
    m_xConfigAccess.clear();   
}

void ConfigurationAccess_FactoryManager::readConfigurationData()
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::readConfigurationData" );
    // SAFE
    ResetableGuard aLock( m_aLock );
    
    if ( !m_bConfigAccessInitialized )
    {
        Sequence< Any > aArgs( 1 );
        PropertyValue   aPropValue;
        
        aPropValue.Name  = rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( "nodepath" ));
        aPropValue.Value <<= m_sRoot;
        aArgs[0] <<= aPropValue;
        
        try
        {
            m_xConfigAccess.set( m_xConfigProvider->createInstanceWithArguments(SERVICENAME_CFGREADACCESS,aArgs ), UNO_QUERY );
        }
        catch ( WrappedTargetException& )
        {
        }
        
        m_bConfigAccessInitialized = sal_True;
    }
    
    if ( m_xConfigAccess.is() )
    {
        Sequence< rtl::OUString >   aUIElementFactories = m_xConfigAccess->getElementNames();
        
        rtl::OUString             aType;
        rtl::OUString             aName;
        rtl::OUString             aModule;
        rtl::OUString             aService;
        rtl::OUString             aHashKey;
        Reference< XPropertySet > xPropertySet;
        for ( sal_Int32 i = 0; i < aUIElementFactories.getLength(); i++ )
        {
            if ( impl_getElementProps( m_xConfigAccess->getByName( aUIElementFactories[i] ), aType, aName, aModule, aService ))
            {
                // Create hash key from type, name and module as they are together a primary key to
                // the UNO service that implements the user interface element factory.
                aHashKey = getHashKeyFromStrings( aType, aName, aModule );
                m_aFactoryManagerMap.insert( FactoryManagerMap::value_type( aHashKey, aService ));
            }
        }
    
        Reference< XContainer > xContainer( m_xConfigAccess, UNO_QUERY );
        aLock.unlock();
        // UNSAFE
        if ( xContainer.is() )
            xContainer->addContainerListener( this );
    }
}

sal_Bool ConfigurationAccess_FactoryManager::impl_getElementProps( const Any& aElement, rtl::OUString& rType, rtl::OUString& rName, rtl::OUString& rModule, rtl::OUString& rServiceSpecifier ) const
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::impl_getElementProps" );
    Reference< XPropertySet > xPropertySet;
    aElement >>= xPropertySet;

    if ( xPropertySet.is() )
    {
        try
        {
            xPropertySet->getPropertyValue( m_aPropType ) >>= rType;
            xPropertySet->getPropertyValue( m_aPropName ) >>= rName;
            xPropertySet->getPropertyValue( m_aPropModule ) >>= rModule;
            xPropertySet->getPropertyValue( m_aPropFactory ) >>= rServiceSpecifier;
        }
        catch ( com::sun::star::beans::UnknownPropertyException& )
        {
            return sal_False;
        }
        catch ( com::sun::star::lang::WrappedTargetException& )
        {
            return sal_False;
        }
    }

    return sal_True;
}

//*****************************************************************************************************************
//	XInterface, XTypeProvider, XServiceInfo
//*****************************************************************************************************************
DEFINE_XSERVICEINFO_ONEINSTANCESERVICE  (   UIElementFactoryManager				            ,
                                            ::cppu::OWeakObject							    ,
                                            SERVICENAME_UIELEMENTFACTORYMANAGER             ,
											IMPLEMENTATIONNAME_UIELEMENTFACTORYMANAGER
										)

DEFINE_INIT_SERVICE                     (   UIElementFactoryManager, {} )

UIElementFactoryManager::UIElementFactoryManager( const Reference< XMultiServiceFactory >& xServiceManager ) :
    ThreadHelpBase( &Application::GetSolarMutex() ),
    m_bConfigRead( sal_False ),
    m_xServiceManager( xServiceManager )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::UIElementFactoryManager" );
    m_pConfigAccess = new ConfigurationAccess_FactoryManager( m_xServiceManager,rtl::OUString( RTL_CONSTASCII_USTRINGPARAM( "/org.openoffice.Office.UI.Factories/Registered/UIElementFactories" )) );
    m_pConfigAccess->acquire();
    m_xModuleManager = Reference< XModuleManager >( m_xServiceManager->createInstance( SERVICENAME_MODULEMANAGER ), UNO_QUERY );
}

UIElementFactoryManager::~UIElementFactoryManager()
{
    ResetableGuard aLock( m_aLock );
    
    // reduce reference count
    m_pConfigAccess->release();
}

// XUIElementFactory
Reference< XUIElement > SAL_CALL UIElementFactoryManager::createUIElement( 
    const ::rtl::OUString& ResourceURL, 
    const Sequence< PropertyValue >& Args )
throw ( ::com::sun::star::container::NoSuchElementException, ::com::sun::star::lang::IllegalArgumentException, ::com::sun::star::uno::RuntimeException )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::createUIElement" );
    // SAFE
    ResetableGuard aLock( m_aLock );
    
    if ( !m_bConfigRead )
    {
        m_bConfigRead = sal_True;
        m_pConfigAccess->readConfigurationData();
    }
    
    const rtl::OUString aPropFrame( RTL_CONSTASCII_USTRINGPARAM( "Frame" ));
    
    rtl::OUString   aModuleId;
    PropertyValue   aPropValue;
    Reference< XFrame > xFrame;
    
    // Retrieve the frame instance from the arguments to determine the module identifier. This must be provided
    // to the search function. An empty module identifier is provided if the frame is missing or the module id cannot
    // retrieve from it.
    for ( int i = 0; i < Args.getLength(); i++ )
    {
        if ( Args[i].Name.equals( aPropFrame ))
            Args[i].Value >>= xFrame;
    }
    
    Reference< XModuleManager > xManager( m_xModuleManager );
    aLock.unlock();

    // Determine the module identifier
    try
    {
        if ( xFrame.is() && xManager.is() )
            aModuleId = xManager->identify( Reference< XInterface >( xFrame, UNO_QUERY ) );
        
        Reference< XUIElementFactory > xUIElementFactory = getFactory( ResourceURL, aModuleId );
        if ( xUIElementFactory.is() )
            return xUIElementFactory->createUIElement( ResourceURL, Args );
    }
    catch ( UnknownModuleException& )
    {
    }

    throw NoSuchElementException();
}

// XUIElementFactoryRegistration
Sequence< Sequence< PropertyValue > > SAL_CALL UIElementFactoryManager::getRegisteredFactories() 
throw ( RuntimeException )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::getRegisteredFactories" );
    // SAFE
    ResetableGuard aLock( m_aLock );
    
    if ( !m_bConfigRead )
    {
        m_bConfigRead = sal_True;
        m_pConfigAccess->readConfigurationData();
    }
    
    return m_pConfigAccess->getFactoriesDescription();
}

Reference< XUIElementFactory > SAL_CALL UIElementFactoryManager::getFactory( const ::rtl::OUString& aResourceURL, const ::rtl::OUString& aModuleId )
throw ( RuntimeException )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::getFactory" );
    ResetableGuard aLock( m_aLock );

    if ( !m_bConfigRead )
    {
        m_bConfigRead = sal_True;
        m_pConfigAccess->readConfigurationData();
    }
    
    rtl::OUString aType;
    rtl::OUString aName;
    
    WindowContentFactoryManager::RetrieveTypeNameFromResourceURL( aResourceURL, aType, aName );
    
    Reference< XMultiServiceFactory > xSManager( m_xServiceManager );
    
    rtl::OUString aServiceSpecifier = m_pConfigAccess->getFactorySpecifierFromTypeNameModule( aType, aName, aModuleId );
    
    aLock.unlock();
    if ( aServiceSpecifier.getLength() )
        return Reference< XUIElementFactory >( xSManager->createInstance( aServiceSpecifier ), UNO_QUERY );
    else
        return Reference< XUIElementFactory >();
}

void SAL_CALL UIElementFactoryManager::registerFactory( const ::rtl::OUString& aType, const ::rtl::OUString& aName, const ::rtl::OUString& aModuleId, const ::rtl::OUString& aFactoryImplementationName ) 
throw ( ElementExistException, RuntimeException )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::registerFactory" );
    // SAFE
    ResetableGuard aLock( m_aLock );
    
    if ( !m_bConfigRead )
    {
        m_bConfigRead = sal_True;
        m_pConfigAccess->readConfigurationData();
    }

    m_pConfigAccess->addFactorySpecifierToTypeNameModule( aType, aName, aModuleId, aFactoryImplementationName );
    // SAFE
}

void SAL_CALL UIElementFactoryManager::deregisterFactory( const ::rtl::OUString& aType, const ::rtl::OUString& aName, const ::rtl::OUString& aModuleId )
throw ( NoSuchElementException, RuntimeException )
{
    RTL_LOGFILE_CONTEXT_AUTHOR( aLogger, "framework", "Ocke.Janssen@sun.com", "UIElementFactoryManager::deregisterFactory" );
    // SAFE
    ResetableGuard aLock( m_aLock );
    
    if ( !m_bConfigRead )
    {
        m_bConfigRead = sal_True;
        m_pConfigAccess->readConfigurationData();
    }

    m_pConfigAccess->removeFactorySpecifierFromTypeNameModule( aType, aName, aModuleId );
    // SAFE
}

} // namespace framework
