#include "stdafx.h"

#include "dbgmgr.h"

#include "win/exceptions.h"

namespace kdlib {

DebugManager*  g_dbgMgr = NULL;

///////////////////////////////////////////////////////////////////////////////

DebugManager::DebugManager()
{
    CoInitialize(NULL);

    HRESULT  hres = DebugCreate( __uuidof(IDebugClient4), (void **)&client );
    if ( FAILED( hres ) )
        throw DbgEngException(L"DebugCreate", hres);

    control = CComQIPtr<IDebugControl4>(client);
    system = CComQIPtr<IDebugSystemObjects2>(client);
    dataspace = CComQIPtr<IDebugDataSpaces4>(client);
    symbols = CComQIPtr<IDebugSymbols3>(client);
    advanced = CComQIPtr<IDebugAdvanced2>(client);

    client->SetEventCallbacks( this );
}

///////////////////////////////////////////////////////////////////////////////

DebugManager::~DebugManager()
{
    CoUninitialize();
}

///////////////////////////////////////////////////////////////////////////////

void DebugManager::registerEventsCallback( DebugEventsCallback *callback )
{
     boost::recursive_mutex::scoped_lock l(m_callbacksLock);
     m_callbacks.push_back( callback );
}

///////////////////////////////////////////////////////////////////////////////

void DebugManager::removeEventsCallback( DebugEventsCallback *callback )
{
    boost::recursive_mutex::scoped_lock l(m_callbacksLock);
    m_callbacks.remove( callback );
}

///////////////////////////////////////////////////////////////////////////////

inline
ULONG ConvertCallbackResult( DebugCallbackResult result )
{
    switch( result )
    {
    case DebugCallbackBreak:
        return DEBUG_STATUS_BREAK;

    case DebugCallbackProceed:
        return DEBUG_STATUS_GO_HANDLED;

    default:
        assert( 0 );

    case DebugCallbackNoChange:
        return DEBUG_STATUS_NO_CHANGE;
    }
}

HRESULT STDMETHODCALLTYPE DebugManager::Breakpoint( IDebugBreakpoint *bp ) 
{
    DebugCallbackResult  result = DebugCallbackNoChange;

    HRESULT hres;
    ULONG id;

    hres = bp->GetId( &id );
    if ( FAILED( hres ) )
        return DEBUG_STATUS_NO_CHANGE;

    boost::recursive_mutex::scoped_lock l(m_callbacksLock);

    EventsCallbackList::iterator  it;
    for ( it = m_callbacks.begin(); it != m_callbacks.end(); ++it )
    {
        DebugCallbackResult  ret = (*it)->onBreakpoint(id);
        result = ret != DebugCallbackNoChange ? ret : result;
    }

    return ConvertCallbackResult( result );
}

///////////////////////////////////////////////////////////////////////////////

} //kdlib namespace end