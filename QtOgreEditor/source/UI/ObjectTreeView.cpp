/*
-----------------------------------------------------------------------------
Copyright (c) 2008-2010 Diversia

This file is part of Diversia.
-----------------------------------------------------------------------------
*/

#include "Platform/StableHeaders.h"

#include "Client/Plugin/ClientPluginManager.h"
#include "Client/Communication/GridManager.h"
#include "Client/Communication/Server.h"
#include "Client/Object/ClientComponent.h"
#include "Client/Object/ClientObject.h"
#include "Client/Object/ClientObjectManager.h"
#include "Client/Object/ClientObjectTemplate.h"
#include "Client/Object/ClientObjectTemplateManager.h"
#include "Client/Undo/ComponentCommand.h"
#include "Client/Undo/ObjectCommand.h"
#include "Client/Undo/UndoStack.h"
#include "Model/ObjectComponentModel.h"
#include "Object/Component.h"
#include "Object/ComponentFactory.h"
#include "Object/ComponentFactoryManager.h"
#include "Object/Object.h"
#include "Object/ObjectTemplateManager.h"
#include "OgreClient/Input/ObjectSelection.h"
#include "Shared/Plugin/Plugin.h"
#include "UI/MainWindow.h"
#include "UI/ObjectTreeView.h"
#include "UI/PropertyBrowser.h"
#include "Util/Camp/CampUtils.h"
#include "qsignalmapper.h"

namespace Diversia
{
namespace QtOgreEditor
{
//------------------------------------------------------------------------------

unsigned int ObjectTreeView::mObjectCounter = 0;

ObjectTreeView::ObjectTreeView( QWidget* pParent ):
    QTreeView( pParent ),
    mModel( new ObjectComponentModel( this ) ),
    mShowRuntimeObjects( false ),
    mShowDefaultComponents( false )
{
    QTreeView::setDragDropMode( QTreeView::InternalMove );
    QTreeView::setSelectionMode( QAbstractItemView::ExtendedSelection );
    QTreeView::setModel( mModel );
    QWidget::setContextMenuPolicy( Qt::CustomContextMenu );
}

ObjectTreeView::~ObjectTreeView()
{

}

void ObjectTreeView::init()
{
    EditorGlobals::mGrid->connectServerChange( sigc::mem_fun( this, &ObjectTreeView::serverChange ) );

    // Create new component actions.
    QSignalMapper* newComponentSignalMapper = new QSignalMapper( this );
    QSignalMapper* componentActionSignalMapper = new QSignalMapper( this );
    const ComponentFactories& factories = ComponentFactoryManager::getComponentFactories();
    QList<QAction*> actions;
    for( ComponentFactories::const_iterator i = factories.begin(); i != factories.end(); ++i )
    {
        try
        {
            // Add create component menu action.
            QString className = i->second->getTypeName().c_str();
            const camp::Class& metaclass = camp::classByName( className.toStdString() );
            QString icon = ":/Icons/Icons/categories/xfce-system.png";
            if( metaclass.hasTag( "QtIcon" ) ) icon = metaclass.tag( "QtIcon" ).to<QString>();

            QAction* newComponentAction = new QAction( QIcon( icon ), className, this );
            QObject::connect( newComponentAction, SIGNAL( triggered() ), newComponentSignalMapper, SLOT( map() ) );
            newComponentSignalMapper->setMapping( newComponentAction, (int)i->second->getType() );
            actions.push_back( newComponentAction );

            // Add component specific actions.
            std::size_t functionCount = metaclass.functionCount();
            for( std::size_t j = 0; j < functionCount; ++j )
            {
                const camp::Function& function = metaclass.function( j );
                if( function.hasTag( "QtPopupMenu" ) )
                {
                    QString componentActionIcon = ":/Icons/Icons/categories/xfce-system.png";
                    QString componentActionName = QString::fromStdString( function.name() );
                    if( function.hasTag( "QtIcon" ) )  componentActionIcon = function.tag( "QtIcon" ).to<QString>();

                    QAction* componentAction = new QAction( QIcon( componentActionIcon ), componentActionName, this );
                    componentAction->setProperty( "TypeName", QVariant( className ) );
                    componentAction->setProperty( "FunctionName", QVariant( componentActionName ) );
                    QObject::connect( componentAction, SIGNAL( triggered() ), componentActionSignalMapper, SLOT( map() ) );
                    componentActionSignalMapper->setMapping( componentAction, componentAction );
                    mComponentActions[ i->first ].push_back( componentAction );
                }
            }
        }
        catch( camp::Error e )
        {
            LOGE << e.what();
        }

    }
    QObject::connect( newComponentSignalMapper, SIGNAL( mapped(int) ), this, SLOT( createComponent(int) ) );
    QObject::connect( componentActionSignalMapper, SIGNAL( mapped(QObject*) ), this, SLOT( componentAction(QObject*) ) );

    mComponentMenu = new QMenu( "Create component", this );
    mComponentMenu->setIcon( QIcon( ":/Icons/Icons/categories/xfce-system.png" ) );
    mComponentMenu->addActions( actions );
    mComponentMenuAction = mComponentMenu->menuAction();

    // Make object dock and toolbar focus propagate to this widget. Doesn't work yet.
    /*connect( EditorGlobals::mMainWindow->mUI.objectsDock, 
        SIGNAL(focusInEvent(QFocusEvent*)), this, SLOT(focusInEvent(QFocusEvent*)) );
    connect( EditorGlobals::mMainWindow->mUI.toolBarObject, 
        SIGNAL(focusInEvent(QFocusEvent*)), this, SLOT(focusInEvent(QFocusEvent*)) );
    QObject::connect( EditorGlobals::mMainWindow->mUI.objectsDockWidgetContents, 
        SIGNAL(focusInEvent(QFocusEvent*)), this, SLOT(focusInEvent(QFocusEvent*)) );*/

    // Connect to object selection changes signal.
    GlobalsBase::mSelection->connectSelected( sigc::mem_fun( this, 
        &ObjectTreeView::objectSelectionChange ) );

    ObjectTreeView::clear();
}

bool ObjectTreeView::isObject( const QModelIndex& rIndex )
{
    return ObjectTreeView::getReplicaType( rIndex ) == REPLICATYPE_OBJECT;
}

bool ObjectTreeView::isComponent( const QModelIndex& rIndex )
{
    return ObjectTreeView::getReplicaType( rIndex ) == REPLICATYPE_COMPONENT;
}

ReplicaType ObjectTreeView::getReplicaType( const QModelIndex& rIndex )
{
    if( rIndex.isValid() ) return (ReplicaType)rIndex.data( Qt::UserRole + 1 ).toInt();
    return 0xFF;
}

ClientObject& ObjectTreeView::getObject( const QModelIndex& rIndex )
{
    if( !ObjectTreeView::isObject( rIndex ) )
        DIVERSIA_EXCEPT( Exception::ERR_INVALIDPARAMS, 
            "Given QModelIndex does not represent an object.", 
            "ObjectTreeView::getObject" );

    String objectName = rIndex.data( Qt::UserRole + 2 ).toString().toStdString();
    Object& object = mObjectManager->getObject( objectName );
    return static_cast<ClientObject&>( object );
}

ClientComponent& ObjectTreeView::getComponent( const QModelIndex& rIndex )
{
    if( !ObjectTreeView::isComponent( rIndex ) )
        DIVERSIA_EXCEPT( Exception::ERR_INVALIDPARAMS, 
            "Given QModelIndex does not represent a component.", 
            "ObjectTreeView::getObject" );

    String objectName = rIndex.data( Qt::UserRole + 2 ).toString().toStdString();
    String componentName = rIndex.data( Qt::UserRole + 3 ).toString().toStdString();
    return static_cast<ClientComponent&>( mObjectManager->getObject( objectName ).getComponent( 
        componentName ) );
}

void ObjectTreeView::load( const QModelIndex& rIndex )
{
    switch( ObjectTreeView::getReplicaType( rIndex ) )
    {
        case REPLICATYPE_OBJECT: 
        {
            // Load property browser with object.
            EditorGlobals::mMainWindow->mUI.propertyBrowser->load( 
                ObjectTreeView::getObject( rIndex ) );
            EditorGlobals::mMainWindow->mUI.propertyBrowser->setEnabled( true );

            // Set actions to toolbar.
            ObjectTreeView::clearActions();
            EditorGlobals::mMainWindow->mUI.toolBarObject->addAction( 
                EditorGlobals::mMainWindow->mUI.actionCreate_child_object );
            EditorGlobals::mMainWindow->mUI.toolBarObject->addAction( 
                EditorGlobals::mMainWindow->mUI.actionDestroy_object );
            EditorGlobals::mMainWindow->mUI.toolBarObject->addAction( mComponentMenuAction );
            EditorGlobals::mMainWindow->mUI.toolBarObject->addAction( 
                EditorGlobals::mMainWindow->mUI.actionDuplicate_object );
            EditorGlobals::mMainWindow->mUI.toolBarObject->addAction( 
                EditorGlobals::mMainWindow->mUI.actionCreate_template_from_object );

            break;
        }
        case REPLICATYPE_COMPONENT:
        {
            // Load property browser with component.
            EditorGlobals::mMainWindow->mUI.propertyBrowser->load( 
                ObjectTreeView::getComponent( rIndex ) );
            EditorGlobals::mMainWindow->mUI.propertyBrowser->setEnabled( true );

            // Set actions to toolbar.
            ObjectTreeView::clearActions();
            EditorGlobals::mMainWindow->mUI.toolBarObject->addAction( 
                EditorGlobals::mMainWindow->mUI.actionDestroy_component );

            break;
        }
    }
}

void ObjectTreeView::selectionChanged( const QItemSelection& rSelected, 
    const QItemSelection& rDeselected )
{
    // Select and unselect items in ObjectSelection.
    QModelIndexList sIndexes = rSelected.indexes();
    for( QModelIndexList::iterator i = sIndexes.begin(); i != sIndexes.end(); ++i )
    {
        if( ObjectTreeView::isObject( *i ) )
            EditorGlobals::mSelection->select( ObjectTreeView::getObject( *i ), true );
    }
    QModelIndexList dIndexes = rDeselected.indexes();
    for( QModelIndexList::iterator i = dIndexes.begin(); i != dIndexes.end(); ++i )
    {
        if( ObjectTreeView::isObject( *i ) )
        {
            try
            {
                EditorGlobals::mSelection->deselect( ObjectTreeView::getObject( *i ), true );
            }
            catch( ... )
            {
                // Ignore error, object will be deselected because it has been destroyed.
            }
        }
    }

    // Let the tree view handle the selection so the updated selection can be used below.
    QTreeView::selectionChanged( rSelected, rDeselected );

    QModelIndexList selectedIndexes = QTreeView::selectionModel()->selectedIndexes();
    if( !selectedIndexes.size() )
    {
        // No items are selected.
        ObjectTreeView::clear();
    }
    else if( selectedIndexes.size() > 1 )
    {
        // More than one item is selected.
        EditorGlobals::mMainWindow->mUI.propertyBrowser->clear();
        EditorGlobals::mMainWindow->mUI.propertyBrowser->setEnabled( false );
        ObjectTreeView::clearActions();
        EditorGlobals::mMainWindow->mUI.toolBarObject->addAction( 
            EditorGlobals::mMainWindow->mUI.actionDestroy_selected );
    }
    else 
    {
        // One item is selected.
        ObjectTreeView::load( selectedIndexes.at( 0 ) );
    }
}

void ObjectTreeView::showContextMenu( const QPoint& rPoint )
{
    QModelIndex index = QTreeView::indexAt( rPoint );
    QMenu contextMenu;
    if( index.isValid() )
    {
        switch( ObjectTreeView::getReplicaType( index ) )
        {
            case REPLICATYPE_OBJECT: 
            {
                contextMenu.addAction( EditorGlobals::mMainWindow->mUI.actionCreate_child_object );
                contextMenu.addAction( EditorGlobals::mMainWindow->mUI.actionDestroy_object );
                contextMenu.addMenu( mComponentMenu );
                contextMenu.addAction( 
                    EditorGlobals::mMainWindow->mUI.actionDuplicate_object );
                contextMenu.addAction( 
                    EditorGlobals::mMainWindow->mUI.actionCreate_template_from_object );   
                break;
            }
            case REPLICATYPE_COMPONENT:
            {
                contextMenu.addAction( EditorGlobals::mMainWindow->mUI.actionDestroy_component );

                // Add component specific actions.
                ComponentType componentType = ObjectTreeView::getComponent( index ).getType();
                bool addSeperator = true;
                for( QList<QAction*>::iterator i = mComponentActions[ componentType ].begin(); 
                    i != mComponentActions[ componentType ].end(); ++i )
                {
                    if( addSeperator )
                    {
                        contextMenu.addSeparator();
                        addSeperator = false;
                    }
                    contextMenu.addAction( *i );
                }

                break;
            }
        }
    }
    else
    {
        contextMenu.addAction( EditorGlobals::mMainWindow->mUI.actionCreate_new_object );
    }

    contextMenu.exec( QTreeView::mapToGlobal( rPoint ) );
}

void ObjectTreeView::search()
{
    QString text = EditorGlobals::mMainWindow->mUI.editObjectSearch->text();
    if( text.isEmpty() )
        ObjectTreeView::clear();
    else
        QTreeView::keyboardSearch( text );
}

void ObjectTreeView::createNewObject()
{
    try
    {
        GlobalsBase::mUndoStack->push( new ObjectCommand( *mObjectManager, 
            String( "EditorObject" ) + mObjectManager->getGUIDString() + 
            boost::lexical_cast<String>( ++mObjectCounter ), LOCAL, "EditorObject" ) );
    }
    catch( Exception e )
    {
        LOGE << "Could not create new object: " << e.what();
    }
}

void ObjectTreeView::createChildObject()
{
    try
    {
        QModelIndex index = QAbstractItemView::currentIndex();
        Object& rootObject = ObjectTreeView::getObject( index );

        GlobalsBase::mUndoStack->push( new ObjectCommand( *mObjectManager, 
            String( "EditorObject" ) + mObjectManager->getGUIDString() + 
            boost::lexical_cast<String>( ++mObjectCounter ), rootObject.getNetworkingType(), 
            "EditorObject", &rootObject ) );
    }
    catch( Exception e )
    {
        LOGE << "Could not create child object: " << e.what();
    }
}

void ObjectTreeView::createComponent( int componentType )
{
    QModelIndex index = QAbstractItemView::currentIndex();
    if( index.isValid() && index.data( Qt::UserRole + 1 ).toInt() == REPLICATYPE_OBJECT )
    {
        try
        {
            Object& object = mObjectManager->getObject( index.data( Qt::UserRole + 2 
                ).toString().toStdString() );
            
            ComponentFactory& componentFactory = ComponentFactoryManager::getComponentFactory( 
                componentType );

            // Create unique component name.
            String componentName = componentFactory.getTypeName();
            if( componentFactory.multiple() )
            {
                std::size_t count = object.componentCount( componentType );
                if( count )
                {
                    componentName += boost::lexical_cast<String>( count + 1 );
                }
            }
            
            GlobalsBase::mUndoStack->push( new ComponentCommand( object, componentType, 
                componentName ) );
        }
        catch( Exception e )
        {
            LOGE << "Could not create component: " << e.what();
        }
    }
}

void ObjectTreeView::componentAction( QObject* pObject )
{
    String className = pObject->property( "TypeName" ).toString().toStdString();
    String functionName = pObject->property( "FunctionName" ).toString().toStdString();

    try
    {
        QModelIndexList selectedIndexes = QTreeView::selectionModel()->selectedIndexes();
        for( QModelIndexList::iterator i = selectedIndexes.begin(); i != selectedIndexes.end(); ++i )
        {
            if( ObjectTreeView::isComponent( *i ) )
            {
                camp::classByName( className ).function( functionName ).call( ObjectTreeView::getComponent( *i ) );
            }
        }
    }
    catch( camp::Error e )
    {
        LOGE << "Cannot execute function " << functionName << " in class " << className << ": " << 
            e.what();
    }
    catch( Exception e )
    {
        LOGE << "Cannot execute function " << functionName << " in class " << className << ": " << 
            e.what();
    }
}

void ObjectTreeView::createTemplate()
{
    static unsigned int counter = 0;

    QModelIndexList selectedIndexes = QTreeView::selectionModel()->selectedIndexes();
    if( selectedIndexes.size() == 1 )
    {
        try
        {
            Object& object = ObjectTreeView::getObject( selectedIndexes.at( 0 ) );
            mObjectTemplateManager->createObjectTemplate( object, 
                String( "ObjectTemplateFromObject" ) + mObjectTemplateManager->getGUIDString() + 
                boost::lexical_cast<String>( ++counter ), object.getNetworkingType() );
        }
        catch( Exception e )
        {
            LOGE << "Could not create object template from object: " << e.what();
        }
    }
}

void ObjectTreeView::duplicate()
{
    QModelIndexList selectedIndexes = QTreeView::selectionModel()->selectedIndexes();
    if( selectedIndexes.size() == 1 )
    {
        try
        {
            Object& object = ObjectTreeView::getObject( selectedIndexes.at( 0 ) );
            object.duplicate();
        }
        catch( const Exception& e )
        {
            LOGE << "Could not duplicate object: " << e.what();
        }
    }
}

void ObjectTreeView::destroyObject()
{
    QModelIndexList selectedIndexes = QTreeView::selectionModel()->selectedIndexes();
    if( selectedIndexes.size() == 1 )
        ObjectTreeView::destroyObject( selectedIndexes.at( 0 ) );
}

void ObjectTreeView::destroyComponent()
{
    QModelIndexList selectedIndexes = QTreeView::selectionModel()->selectedIndexes();
    if( selectedIndexes.size() == 1 )
        ObjectTreeView::destroyComponent( selectedIndexes.at( 0 ) );
}

void ObjectTreeView::destroy( const QModelIndex& rIndex )
{
    switch( ObjectTreeView::getReplicaType( rIndex ) )
    {
        case REPLICATYPE_OBJECT: ObjectTreeView::destroyObject( rIndex ); break;
        case REPLICATYPE_COMPONENT: ObjectTreeView::destroyComponent( rIndex ); break;
    }
}

void ObjectTreeView::destroyObject( const QModelIndex& rIndex )
{
    try
    {
        GlobalsBase::mUndoStack->push( new ObjectCommand( ObjectTreeView::getObject( rIndex ) ) );
    }
    catch( Exception e )
    {
        LOGE << "Could not destroy object(s): " << e.what();
    }
}

void ObjectTreeView::destroyComponent( const QModelIndex& rIndex )
{
    try
    {
        GlobalsBase::mUndoStack->push( new ComponentCommand( 
            ObjectTreeView::getComponent( rIndex ) ) );
    }
    catch( Exception e )
    {
        LOGE << "Could not destroy component: " << e.what();
    }
}

void ObjectTreeView::destroySelected()
{
    QModelIndexList selectedIndexes = QTreeView::selectionModel()->selectedIndexes();
    for( QModelIndexList::iterator i = selectedIndexes.begin(); i != selectedIndexes.end(); ++i )
    {
        ObjectTreeView::destroy( *i );
    }
}

void ObjectTreeView::focusInEvent( QFocusEvent* pEvent )
{
    // Reload one item into property grid when getting focus.
    QModelIndexList selectedIndexes = QTreeView::selectionModel()->selectedIndexes();
    if( selectedIndexes.size() == 1 )
    {
        ObjectTreeView::load( selectedIndexes.at( 0 ) );
        pEvent->accept();
    }

    QTreeView::focusInEvent( pEvent );
}

void ObjectTreeView::showRuntimeObjects( bool show )
{
    mShowRuntimeObjects = show;
    ObjectTreeView::checkHiddenItems();
}

void ObjectTreeView::showDefaultComponents( bool show )
{
    mShowDefaultComponents = show;
    ObjectTreeView::checkHiddenItems();
}

void ObjectTreeView::checkHiddenItems( QModelIndex parent /*= QModelIndex()*/ )
{
    unsigned int rows = mModel->rowCount( parent );
    for( unsigned int i = 0; i < rows; ++i )
    {
        QModelIndex index = mModel->index( i, 0, parent );

        switch( ObjectTreeView::getReplicaType( index ) )
        {
            case REPLICATYPE_OBJECT:
            {
                try
                {
                    if( ObjectTreeView::getObject( index ).isRuntimeObject() ) 
                        QTreeView::setRowHidden( i, parent, !mShowRuntimeObjects );

                    ObjectTreeView::checkHiddenItems( index );
                }
                catch( ... )
                {
                	// Ignore error, object has been destroyed but is still in the tree view.
                }

                break;
            }
            case REPLICATYPE_COMPONENT:
                if( Object::hasAutoCreateComponent( ObjectTreeView::getComponent( 
                    index ).getType() ) ) 
                    QTreeView::setRowHidden( i, parent, !mShowDefaultComponents );
                break;
        }
    }
}

void ObjectTreeView::mousePressEvent( QMouseEvent* pEvent )
{
    QModelIndex index = QTreeView::indexAt( pEvent->pos() );

    if( !index.isValid() )
        QTreeView::clearSelection();

    QTreeView::mousePressEvent( pEvent );
}

void ObjectTreeView::dropEvent( QDropEvent* pEvent )
{
    // Stop the abstract item view from rearranging items, the model will do this!
    try
    {
        QAbstractItemView::dropEvent( pEvent );
    }
    catch( Exception e )
    {
        LOGE << "Could not change parent object: " << e.what();
    }

    pEvent->setDropAction( Qt::IgnoreAction );
    pEvent->accept();
}

void ObjectTreeView::serverChange( ServerAbstract& rServer, bool created )
{
    // TODO: Support multiple servers.
    if( created )
        rServer.getPluginManager().connect( sigc::mem_fun( this, &ObjectTreeView::pluginChange ) );
}

void ObjectTreeView::pluginChange( Plugin& rPlugin, bool created )
{
    switch( rPlugin.getType() ) 
    {
        case PLUGINTYPE_OBJECTMANAGER:
            mObjectManager = &static_cast<ClientObjectManager&>( rPlugin );
            mModel->setObjectManager( *mObjectManager );
            break;
        case PLUGINTYPE_OBJECTTEMPLATEMANAGER:
            mObjectTemplateManager = &static_cast<ClientObjectTemplateManager&>( rPlugin );
            break;
    }
}

void ObjectTreeView::objectSelectionChange( const camp::UserObject& rObject, bool selected )
{
    try
    {
        const camp::Class& objectClass = camp::classByName( "Object" );
        if( camp::hasBase( rObject.getClass(), objectClass ) )
        {
            Object* object = static_cast<Object*>( rObject.getClass().applyOffset( rObject.pointer(), 
                objectClass ) );

            QTreeView::selectionModel()->select( mModel->getObjectIndex( *object ), 
                selected ? QItemSelectionModel::Select : QItemSelectionModel::Deselect );
        }
    }
    catch( Exception e )
    {
        // Ignore error? It could happen that it will try to get a destroyed object but that is not
        // a problem.
    }
}

void ObjectTreeView::clear()
{
    // Clear selection
    QTreeView::clearSelection();

    // Clear property browser
    EditorGlobals::mMainWindow->mUI.propertyBrowser->clear();
    EditorGlobals::mMainWindow->mUI.propertyBrowser->setEnabled( false );

    // Clear actions
    ObjectTreeView::clearActions();

    EditorGlobals::mMainWindow->mUI.toolBarObject->addAction( 
        EditorGlobals::mMainWindow->mUI.actionCreate_new_object );

    ObjectTreeView::checkHiddenItems();
}

void ObjectTreeView::clearActions()
{
    EditorGlobals::mMainWindow->mUI.toolBarObject->removeAction( 
        EditorGlobals::mMainWindow->mUI.actionCreate_child_object );
    EditorGlobals::mMainWindow->mUI.toolBarObject->removeAction( 
        EditorGlobals::mMainWindow->mUI.actionDestroy_object );
    EditorGlobals::mMainWindow->mUI.toolBarObject->removeAction( mComponentMenuAction );
    EditorGlobals::mMainWindow->mUI.toolBarObject->removeAction( 
        EditorGlobals::mMainWindow->mUI.actionDestroy_component );
    EditorGlobals::mMainWindow->mUI.toolBarObject->removeAction( 
        EditorGlobals::mMainWindow->mUI.actionCreate_new_object );
    EditorGlobals::mMainWindow->mUI.toolBarObject->removeAction( 
        EditorGlobals::mMainWindow->mUI.actionCreate_template_from_object );
    EditorGlobals::mMainWindow->mUI.toolBarObject->removeAction( 
        EditorGlobals::mMainWindow->mUI.actionDestroy_selected );
    EditorGlobals::mMainWindow->mUI.toolBarObject->removeAction( 
        EditorGlobals::mMainWindow->mUI.actionDuplicate_object );
}

//------------------------------------------------------------------------------
} // Namespace QtOgreEditor
} // Namespace Diversia