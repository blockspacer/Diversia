/*
-----------------------------------------------------------------------------
Copyright (c) 2008-2010 Diversia

This file is part of Diversia.

Diversia is free software: you can redistribute it and/or modify it under the 
terms of the GNU General Public License as published by the Free Software 
Foundation, either version 3 of the License, or (at your option) any later 
version.

Diversia is distributed in the hope that it will be useful, but WITHOUT ANY 
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with 
Diversia. If not, see <http://www.gnu.org/licenses/>.

You may contact the author of Diversia by e-mail at: equabyte@sonologic.nl
-----------------------------------------------------------------------------
*/

#ifndef DIVERSIA_CLIENT_CLIENTCOMPONENT_H
#define DIVERSIA_CLIENT_CLIENTCOMPONENT_H

#include "Client/Platform/Prerequisites.h"

#include "Client/Plugin/ClientPluginManager.h"
#include "Client/Object/ClientObject.h"
#include "Object/Component.h"
#include "Shared/Camp/PropertySynchronization.h"
#include "Shared/Plugin/PluginManager.h"
#include "Shared/Resource/ResourceInfo.h"

namespace Diversia
{
namespace Client
{
//------------------------------------------------------------------------------

class DIVERSIA_CLIENT_API ClientComponent : public Component, public PropertySynchronization
{
public:
    /**
    Gets the object that owns this component.
    **/
    inline ClientObject& getClientObject() const
    {
        return static_cast<ClientObject&>( Component::getObject() );
    }
    /**
    Gets the server that this components belongs to.
    **/
    inline ServerAbstract& getServer() const
    {
        return ClientComponent::getClientObject().getClientObjectManager().
            getClientPluginManager().getServer();
    }
    /**
    Gets the plugin state. 
    **/
    inline PluginState getPluginState() const 
    { 
        return ClientComponent::getClientObject().getClientObjectManager().getState();
    }
    /**
    Connects a slot to the plugin state changed signal. 
    
    @param [in,out] rSlot   The slot (signature: void func(PluginState [new state], PluginState 
    [previous state])) to connect.
    
    @return Connection object to block or disconnect the connection.
    **/
    inline sigc::connection connectPluginStateChange( const sigc::slot<void, PluginState, PluginState>& rSlot ) 
    {
        return ClientComponent::getClientObject().getClientObjectManager().getPluginManager().
            connectPluginStateChange( rSlot ); 
    }

protected:
    friend class ClientObject;

    ClientComponent( const String& rName, Mode mode, NetworkingType networkingType, 
        ComponentType type, RakNet::RakNetGUID source, bool localOverride, ClientObject& rObject );
    virtual ~ClientComponent();

    /**
    Called when parent object is hovered in or out.
    
    @param  hoverIn True when hovered in, false when hovered out.
    **/
    inline virtual void hovered( bool hoverIn ) {}
    /**
    Called when parent object is selected or deselected.
    
    @param  selected    True if selected, false if deselected.
    **/
    inline virtual void selected( bool selected ) {}
    /**
    Called when parent object is clicked.
    **/
    inline virtual void clicked() {}
    /**
    Called when parent object is dragged or dropped.
    
    @param  dragStart   True if dragged, false if dropped.
    @param  rPosition   The drag or drop position. Must be copied if used outside of this
                        function call.
    **/
    inline virtual void dragged( bool dragStart, const Vector3& rPosition ) {}

    /**
    Implemented in ClientComponent, but can be overridden.
    **/
    virtual RakNet::RM3ConstructionState QueryConstruction( 
        RakNet::Connection_RM3* pDestinationConnection, 
        RakNet::ReplicaManager3* pReplicaManager3 );
    virtual bool QueryRemoteConstruction( RakNet::Connection_RM3* pSourceConnection );
    virtual RakNet::RM3QuerySerializationResult QuerySerialization( 
        RakNet::Connection_RM3* pDestinationConnection );
    inline virtual void SerializeDestruction( RakNet::BitStream* pDestructionBitstream, 
        RakNet::Connection_RM3* pDestinationConnection ) {}
    inline virtual bool DeserializeDestruction( RakNet::BitStream* pDestructionBitstream, 
        RakNet::Connection_RM3* pSourceConnection ) { return true; }

    /**
    Implemented in ClientComponent, but can be overridden. Be sure to call the function in this 
    class first in your overridden function before doing your own processing!
    **/
    virtual void SerializeConstruction( RakNet::BitStream* pConstructionBitstream, 
        RakNet::Connection_RM3* pDestinationConnection );
    virtual bool DeserializeConstruction( RakNet::BitStream* pConstructionBitstream, 
        RakNet::Connection_RM3* pSourceConnection );
    virtual RakNet::RM3SerializationResult Serialize( 
        RakNet::SerializeParameters* pSerializeParameters );
    virtual void Deserialize( RakNet::DeserializeParameters* pDeserializeParameters );

    ResourceList mResourceList;

private:
    void setNetworkingTypeImpl( NetworkingType type );
    void querySetNetworkingTypeImpl( NetworkingType type );
    void cleanupQuerySetNetworkingType( NetworkingType type );
    void querySetProperty( const String& rQuery, camp::Value& rValue );
    void queryInsertProperty( const String& rQuery, camp::Value& rValue );

    PermissionManager&  mPermissionManager;

    CAMP_CLASS(ClientComponent)

};

//------------------------------------------------------------------------------
} // Namespace Client
} // Namespace Diversia

CAMP_AUTO_TYPE_NONCOPYABLE( Diversia::Client::ClientComponent, 
    &Diversia::Client::Bindings::CampBindings::bindClientComponent );

#endif // DIVERSIA_CLIENT_CLIENTCOMPONENT_H