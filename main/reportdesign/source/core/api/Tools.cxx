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


#include "precompiled_reportdesign.hxx"
#include "Tools.hxx"
#include "core_resource.hrc"
#include "core_resource.hxx"
#include <comphelper/property.hxx>

// =============================================================================
namespace reportdesign
{
// =============================================================================
using namespace com::sun::star;

uno::Reference< report::XSection> lcl_getSection(const uno::Reference< uno::XInterface>& _xReportComponent)
{
    uno::Reference< container::XChild> xChild(_xReportComponent,uno::UNO_QUERY);
    uno::Reference< report::XSection> xRet(_xReportComponent,uno::UNO_QUERY);
    while( !xRet.is() && xChild.is() )
    {
        uno::Reference< uno::XInterface> xTemp = xChild->getParent();
        xChild.set(xTemp,uno::UNO_QUERY);
        xRet.set(xTemp,uno::UNO_QUERY);
    }
    return xRet;
}
// -----------------------------------------------------------------------------
void throwIllegallArgumentException( const ::rtl::OUString& _sTypeName
                                    ,const uno::Reference< uno::XInterface >& ExceptionContext_
                                    ,const ::sal_Int16& ArgumentPosition_
                                    ,const ::com::sun::star::uno::Reference< ::com::sun::star::uno::XComponentContext >& Context_
                                    )
{
    ::rtl::OUString sErrorMessage(RPT_RESSTRING(RID_STR_ERROR_WRONG_ARGUMENT,Context_->getServiceManager()));
    sErrorMessage = sErrorMessage.replaceAt(sErrorMessage.indexOf('#'),2,_sTypeName);
	throw lang::IllegalArgumentException(sErrorMessage,ExceptionContext_,ArgumentPosition_);
}
// -----------------------------------------------------------------------------
uno::Reference< util::XCloneable > cloneObject(const uno::Reference< report::XReportComponent>& _xReportComponent
    ,const uno::Reference< lang::XMultiServiceFactory>& _xFactory
    ,const ::rtl::OUString& _sServiceName)
{
    OSL_ENSURE(_xReportComponent.is() && _xFactory.is() ,"reportcomponent is null -> GPF");
    uno::Reference< report::XReportComponent> xClone(_xFactory->createInstance(_sServiceName),uno::UNO_QUERY_THROW);
    ::comphelper::copyProperties(_xReportComponent.get(),xClone.get());
    return xClone.get();
}
// -----------------------------------------------------------------------------

// =============================================================================
} // namespace reportdesign
// =============================================================================
