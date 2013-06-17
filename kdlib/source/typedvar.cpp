#include "stdafx.h"

#include <boost/regex.hpp>

#include "kdlib/typedvar.h"
#include "kdlib/module.h"

#include "typedvarimp.h"


namespace kdlib {

///////////////////////////////////////////////////////////////////////////////

TypedVarPtr loadTypedVar( const std::wstring &varName )
{
    std::wstring     moduleName;
    std::wstring     symName;

    splitSymName( varName, moduleName, symName );

    ModulePtr  module;

    if ( moduleName.empty() )
    {
        MEMOFFSET_64 moduleOffset = findModuleBySymbol( symName );
        module = loadModule( moduleOffset );
    }
    else
    {
        module = loadModule( moduleName );
    }

    SymbolPtr  symVar = module->getSymbolScope()->getChildByName( symName );

    TypeInfoPtr varType = loadType( symVar );

    if ( LocIsConstant != symVar->getLocType() )
    {
        MEMOFFSET_64 offset = module->getBase() + symVar->getRva();

        return getTypedVar( varType, VarDataProviderPtr( new VarDataMemoryProvider(offset) ) );
    }

    NOT_IMPLEMENTED();
}

///////////////////////////////////////////////////////////////////////////////

TypedVarPtr loadTypedVar( const std::wstring &typeName, MEMOFFSET_64 offset )
{
    TypeInfoPtr varType = loadType( typeName );

    return getTypedVar( varType, VarDataProviderPtr( new VarDataMemoryProvider(offset) ) );
}

///////////////////////////////////////////////////////////////////////////////

TypedVarPtr loadTypedVar( const TypeInfoPtr &varType, MEMOFFSET_64 offset )
{
    if ( !varType )
        throw DbgException( L"type info is null");

    return getTypedVar( varType, VarDataProviderPtr( new VarDataMemoryProvider(offset) ) );
}

///////////////////////////////////////////////////////////////////////////////

TypedVarPtr getTypedVar( const TypeInfoPtr& typeInfo, VarDataProviderPtr &varData )
{
    if ( typeInfo->isBase() )
        return TypedVarPtr( new TypedVarBase( typeInfo, varData ) );

    if ( typeInfo->isUserDefined() )
        return TypedVarPtr( new TypedVarUdt( typeInfo, varData ) );

    if ( typeInfo->isPointer() )
        return TypedVarPtr( new  TypedVarPointer( typeInfo, varData ) );

    if ( typeInfo->isArray() )
        return TypedVarPtr( new TypedVarArray( typeInfo, varData ) );

    if ( typeInfo->isBitField() )
        return TypedVarPtr( new TypedVarBitField( typeInfo, varData ) );

    if ( typeInfo->isEnum() )
        return TypedVarPtr( new TypedVarEnum( typeInfo, varData ) );

    NOT_IMPLEMENTED();
}

///////////////////////////////////////////////////////////////////////////////

TypedVarPtr containingRecord( MEMOFFSET_64 offset, const std::wstring &typeName, const std::wstring &fieldName )
{
    std::wstring     moduleName;
    std::wstring     symName;

    splitSymName( typeName, moduleName, symName );

     ModulePtr  module;

    if ( moduleName.empty() )
    {
        MEMOFFSET_64 moduleOffset = findModuleBySymbol( symName );
        module = loadModule( moduleOffset );
    }
    else
    {
        module = loadModule( moduleName );
    }

    TypeInfoPtr typeInfo = module->getTypeByName( symName );

    return containingRecord( offset, typeInfo, fieldName );
}

///////////////////////////////////////////////////////////////////////////////

TypedVarPtr containingRecord( MEMOFFSET_64 offset, TypeInfoPtr &typeInfo, const std::wstring &fieldName )
{
    if ( !typeInfo )
        throw DbgException( L"type info is null");

    offset = addr64( offset );

    return loadTypedVar( typeInfo, offset - typeInfo->getElementOffset( fieldName ) );
}

///////////////////////////////////////////////////////////////////////////////

TypedVarList loadTypedVarList( MEMOFFSET_64 offset, const std::wstring &typeName, const std::wstring &fieldName )
{
    std::wstring     moduleName;
    std::wstring     symName;

    splitSymName( typeName, moduleName, symName );

     ModulePtr  module;

    if ( moduleName.empty() )
    {
        MEMOFFSET_64 moduleOffset = findModuleBySymbol( symName );
        module = loadModule( moduleOffset );
    }
    else
    {
        module = loadModule( moduleName );
    }

    TypeInfoPtr typeInfo = module->getTypeByName( symName );

    return loadTypedVarList( offset, typeInfo, fieldName );
}

///////////////////////////////////////////////////////////////////////////////

TypedVarList loadTypedVarList( MEMOFFSET_64 offset, TypeInfoPtr &typeInfo, const std::wstring &fieldName )
{
    if ( !typeInfo )
        throw DbgException( L"type info is null" );

    offset = addr64(offset);

    TypedVarList  lst;
    lst.reserve(100);

    MEMOFFSET_64  entryAddress = 0;
    TypeInfoPtr  fieldTypeInfo = typeInfo->getElement( fieldName );
    size_t  psize = fieldTypeInfo->getPtrSize();

    if ( fieldTypeInfo->getName() == ( typeInfo->getName() + L"*" ) )
    {
        for( entryAddress = ptrPtr(offset, psize); addr64(entryAddress) != offset && entryAddress != NULL; entryAddress = ptrPtr( entryAddress + typeInfo->getElementOffset(fieldName), psize ) )
            lst.push_back( loadTypedVar( typeInfo, entryAddress ) );
    }
    else
    {
        for( entryAddress = ptrPtr( offset, psize ); addr64(entryAddress) != offset && entryAddress != NULL; entryAddress = ptrPtr( entryAddress, psize ) )
            lst.push_back( containingRecord( entryAddress, typeInfo, fieldName ) );
    }

    return lst;
}

///////////////////////////////////////////////////////////////////////////////

TypedVarList loadTypedVarArray( MEMOFFSET_64 offset, const std::wstring &typeName, size_t number )
{
    std::wstring     moduleName;
    std::wstring     symName;

    splitSymName( typeName, moduleName, symName );

     ModulePtr  module;

    if ( moduleName.empty() )
    {
        MEMOFFSET_64 moduleOffset = findModuleBySymbol( symName );
        module = loadModule( moduleOffset );
    }
    else
    {
        module = loadModule( moduleName );
    }

    TypeInfoPtr typeInfo = module->getTypeByName( symName );

    return loadTypedVarArray( offset, typeInfo, number );
}

///////////////////////////////////////////////////////////////////////////////

TypedVarList loadTypedVarArray( MEMOFFSET_64 offset, TypeInfoPtr &typeInfo, size_t number )
{
   if ( !typeInfo )
        throw DbgException( L"type info is null" );

    offset = addr64(offset); 

    TypedVarList  lst;
    lst.reserve(100);

    for( size_t i = 0; i < number; ++i )
        lst.push_back( loadTypedVar( typeInfo, offset + i * typeInfo->getSize() ) );
   
    return lst;
}

///////////////////////////////////////////////////////////////////////////////

NumVariant TypedVarBase::getValue() const
{
    if ( m_typeInfo->getName() == L"Char" )
        return NumVariant( m_varData->readSignByte() );

    if ( m_typeInfo->getName() == L"WChar" )
        return NumVariant( m_varData->readSignByte() );

    if ( m_typeInfo->getName() == L"Int1B" )
        return NumVariant( m_varData->readByte() );

    if ( m_typeInfo->getName() == L"UInt1B" )
        return NumVariant( m_varData->readByte() );

    if ( m_typeInfo->getName() == L"Int2B" )
        return NumVariant( m_varData->readSignWord() );

    if ( m_typeInfo->getName() == L"UInt2B" )
        return NumVariant( m_varData->readWord() );

    if ( m_typeInfo->getName() == L"Int4B" )
        return NumVariant( m_varData->readSignDWord() );

    if ( m_typeInfo->getName() == L"UInt4B" )
        return NumVariant( m_varData->readDWord() );

    if ( m_typeInfo->getName() == L"Int8B" )
        return NumVariant( m_varData->readSignQWord() );

    if ( m_typeInfo->getName() == L"UInt8B" )
        return NumVariant( m_varData->readQWord() );

    if ( m_typeInfo->getName() == L"Float" )
        return NumVariant( m_varData->readFloat() );

    if ( m_typeInfo->getName() == L"Double" )
        return NumVariant( m_varData->readDouble() );

    if ( m_typeInfo->getName() == L"Bool" )
        return NumVariant( m_varData->readByte() );

    NOT_IMPLEMENTED();
}

///////////////////////////////////////////////////////////////////////////////

TypedVarPtr TypedVarUdt::getElement( const std::wstring& fieldName )
{
    TypeInfoPtr fieldType = m_typeInfo->getElement( fieldName );

    if ( m_typeInfo->isStaticMember(fieldName) )
    {
        MEMOFFSET_64  staticOffset = m_typeInfo->getElementVa(fieldName);

        if ( staticOffset == 0 )
           NOT_IMPLEMENTED();

        return  loadTypedVar( fieldType, staticOffset );
    }

    MEMOFFSET_32   fieldOffset = m_typeInfo->getElementOffset(fieldName);

    if ( m_typeInfo->isVirtualMember( fieldName ) )
    {
        fieldOffset += getVirtualBaseDisplacement( fieldName );
    }

    return  loadTypedVar( fieldType, m_varData->getAddress() + fieldOffset );
}

///////////////////////////////////////////////////////////////////////////////

TypedVarPtr TypedVarUdt::getElement( size_t index )
{
    TypeInfoPtr fieldType = m_typeInfo->getElement( index );

    if ( m_typeInfo->isStaticMember(index) )
    {
        MEMOFFSET_64  staticOffset = m_typeInfo->getElementVa(index);

        if ( staticOffset == 0 )
           NOT_IMPLEMENTED();

        return  loadTypedVar( fieldType, staticOffset );
    }

    MEMOFFSET_32   fieldOffset = m_typeInfo->getElementOffset(index);

    if ( m_typeInfo->isVirtualMember( index ) )
    {
        fieldOffset += getVirtualBaseDisplacement( index );
    }

    return  loadTypedVar( fieldType, m_varData->getAddress() + fieldOffset );
}

///////////////////////////////////////////////////////////////////////////////

MEMDISPLACEMENT TypedVarUdt::getVirtualBaseDisplacement( const std::wstring &fieldName )
{
    MEMOFFSET_32 virtualBasePtr = 0;
    size_t  virtualDispIndex = 0;
    size_t  virtualDispSize = 0;
    m_typeInfo->getVirtualDisplacement( fieldName, virtualBasePtr, virtualDispIndex, virtualDispSize );

    MEMOFFSET_64 vfnptr = m_varData->getAddress() + virtualBasePtr;
    MEMOFFSET_64 vtbl = m_typeInfo->getPtrSize() == 4 ? ptrDWord( vfnptr ) : ptrQWord(vfnptr);

    MEMDISPLACEMENT     displacement =  ptrSignDWord( vtbl + virtualDispIndex*virtualDispSize );

    return virtualBasePtr + displacement;
}

///////////////////////////////////////////////////////////////////////////////

MEMDISPLACEMENT TypedVarUdt::getVirtualBaseDisplacement( size_t index )
{
    MEMOFFSET_32 virtualBasePtr = 0;
    size_t  virtualDispIndex = 0;
    size_t  virtualDispSize = 0;
    m_typeInfo->getVirtualDisplacement( index, virtualBasePtr, virtualDispIndex, virtualDispSize );

    MEMOFFSET_64 vfnptr = m_varData->getAddress() + virtualBasePtr;
    MEMOFFSET_64 vtbl = m_typeInfo->getPtrSize() == 4 ? ptrDWord( vfnptr ) : ptrQWord(vfnptr);

    MEMDISPLACEMENT     displacement =  ptrSignDWord( vtbl + virtualDispIndex*virtualDispSize );

    return virtualBasePtr + displacement;
}

///////////////////////////////////////////////////////////////////////////////

TypedVarPtr TypedVarPointer::deref()
{
    return loadTypedVar( 
                m_typeInfo->deref(), 
                m_typeInfo->getPtrSize() == 4 ? m_varData->readPtr4() : m_varData->readPtr8() );
}

///////////////////////////////////////////////////////////////////////////////

TypedVarPtr TypedVarArray::getElement( size_t index )
{
    if ( index >= m_typeInfo->getElementCount() )
        throw IndexException( index );

    TypeInfoPtr     elementType = m_typeInfo->getElement(0);

    return loadTypedVar( elementType, m_varData->getAddress() + elementType->getSize()*index );
}

///////////////////////////////////////////////////////////////////////////////

NumVariant TypedVarBitField::getValue() const
{
    NumVariant  var = *loadTypedVar( m_typeInfo->getBitType(), m_varData->getAddress() );

    var >>=  m_typeInfo->getBitOffset();

    if ( var.isSigned() )
    {
        BITOFFSET width = m_typeInfo->getBitWidth();

        if ( ( var & ( 1ULL << ( width -1 ) ) ) != 0 )
        {
            var |= ~( ( 1ULL << width ) - 1 );
        }
        else
        {
            var &= ( 1ULL << width ) - 1;
        }

        var = var.asLongLong();
    }
    else
    {     

        var &= ( 1 << m_typeInfo->getBitWidth() ) - 1;
    }

    return var;
}

///////////////////////////////////////////////////////////////////////////////

} // end kdlib namesapce