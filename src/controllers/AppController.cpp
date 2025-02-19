/*
    This file is part of Element
    Copyright (C) 2019  Kushview, LLC.  All rights reserved.

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
*/

#include "controllers/AppController.h"
#include "controllers/DevicesController.h"
#include "controllers/EngineController.h"
#include "controllers/GuiController.h"
#include "controllers/GraphManager.h"
#include "controllers/GraphController.h"
#include "controllers/MappingController.h"
#include "controllers/SessionController.h"
#include "controllers/PresetsController.h"
#include "controllers/ScriptingController.h"
#include "controllers/WorkspacesController.h"

#include "gui/MainWindow.h"
#include "gui/GuiCommon.h"

#include "session/Presets.h"
#include "Commands.h"
#include "Globals.h"
#include "Messages.h"
#include "Settings.h"
#include "Version.h"

namespace Element {

Globals& AppController::Child::getWorld()               { return getAppController().getWorld(); }
Settings& AppController::Child::getSettings()           { return getWorld().getSettings(); }

AppController::AppController (Globals& g)
    : world (g)
{
    addChild (new GuiController (g, *this));
    addChild (new DevicesController());
    addChild (new EngineController());
    addChild (new MappingController());
    addChild (new PresetsController());
    addChild (new SessionController());
    addChild (new GraphController());
    addChild (new ScriptingController());
    addChild (new WorkspacesController());

    lastExportedGraph = DataPath::defaultGraphDir();

    auto& commands = getWorld().getCommandManager();
    commands.registerAllCommandsForTarget (this);
   #if 1
    commands.registerAllCommandsForTarget (findChild<GuiController>());
    commands.registerAllCommandsForTarget (findChild<WorkspacesController>());
   #else
    // can't do this yet until all controllers have a reliable way to
    // return the next command target
    for (auto* ctl : getChildren())
        if (auto* child = dynamic_cast<AppController::Child*> (ctl))
            commands.registerAllCommandsForTarget (child);
   #endif

    commands.setFirstCommandTarget (this);
}

AppController::~AppController() { }

void AppController::activate()
{
    const auto recentList = DataPath::applicationDataDir().getChildFile ("RecentFiles.txt");
    if (recentList.existsAsFile())
    {
        FileInputStream stream (recentList);
        recentFiles.restoreFromString (stream.readEntireStreamAsString());
    }

    Controller::activate();
}

void AppController::deactivate()
{
    licenseRefreshedConnection.disconnect();
    
    const auto recentList = DataPath::applicationDataDir().getChildFile ("RecentFiles.txt");
    if (! recentList.existsAsFile())
        recentList.create();
    if (recentList.exists())
        recentList.replaceWithText (recentFiles.toString(), false, false);
    
    Controller::deactivate();
}

void AppController::run()
{
    activate();
    
    // need content component parented for the following init routines
    // TODO: better controlled startup procedure
    if (auto* gui = findChild<GuiController>())
        gui->run();

    auto session = getWorld().getSession();
    Session::ScopedFrozenLock freeze (*session);
    
   #if EL_PRO
    if (auto* sc = findChild<SessionController>())
    {
        bool loadDefault = true;

        if (world.getSettings().openLastUsedSession())
        {
            const auto lastSession = getWorld().getSettings().getUserSettings()->getValue ("lastSession");
            if (File::isAbsolutePath(lastSession) && File(lastSession).existsAsFile())
            {
                sc->openFile (File (lastSession));
                loadDefault = false;
            }
        }

        if (loadDefault)
            sc->openDefaultSession();
    }
   #else
    if (auto* gc = findChild<GraphController>())
    {
        bool loadDefaultGraph = true;
        if (world.getSettings().openLastUsedSession())
        {
            const auto lastGraph = getWorld().getSettings().getUserSettings()->getValue (Settings::lastGraphKey);
            if (File::isAbsolutePath(lastGraph) && File(lastGraph).existsAsFile())
            {
                gc->openGraph (File (lastGraph));
                loadDefaultGraph = false;
            }
        }
        if (loadDefaultGraph)
            gc->openDefaultGraph();
    }
   #endif

    if (auto* gui = findChild<GuiController>())
    {
        gui->stabilizeContent();
        const Node graph (session->getCurrentGraph());
        auto* const props = getGlobals().getSettings().getUserSettings();

        if (graph.isValid())
        {
            // don't show plugin windows on load if the UI was hidden
            // TODO: cleanup the boot up process for UI visibility
            if (props->getBoolValue ("mainWindowVisible", true))
                gui->showPluginWindowsFor (graph);
        }
    }
}

void AppController::handleMessage (const Message& msg)
{
	auto* ec        = findChild<EngineController>();
    auto* gui       = findChild<GuiController>();
    auto* sess      = findChild<SessionController>();
    auto* devs      = findChild<DevicesController>();
    auto* maps      = findChild<MappingController>();
    auto* presets   = findChild<PresetsController>();
	jassert(ec && gui && sess && devs && maps && presets);

    bool handled = false; // final else condition will set false
    
    if (const auto* message = dynamic_cast<const AppMessage*> (&msg))
    {
        OwnedArray<UndoableAction> actions;
        message->createActions (*this, actions);
        if (! actions.isEmpty())
        {
            undo.beginNewTransaction();
            for (auto* action : actions)
                undo.perform (action);
            actions.clearQuick (false);
            gui->stabilizeViews();
            return;
        }

        for (auto* const child : getChildren())
        {
            if (auto* const acc = dynamic_cast<AppController::Child*> (child))
                handled = acc->handleMessage (*message);
            if (handled)
                break;
        }

        if (handled)
            return;
    }

    handled = true; // final else condition will set false
    if (const auto* lpm = dynamic_cast<const LoadPluginMessage*> (&msg))
    {
        ec->addPlugin (lpm->description, lpm->verified, lpm->relativeX, lpm->relativeY);
    }
    else if (const auto* dnm = dynamic_cast<const DuplicateNodeMessage*> (&msg))
    {
        Node node = dnm->node;
        ValueTree parent (node.getValueTree().getParent());
        if (parent.hasType (Tags::nodes))
            parent = parent.getParent();
        jassert (parent.hasType (Tags::node));

        const Node graph (parent, false);
        node.savePluginState();
        Node newNode (node.getValueTree().createCopy(), false);
        
        if (newNode.isValid() && graph.isValid())
        {
            newNode = Node (Node::resetIds (newNode.getValueTree()), false);
            ConnectionBuilder dummy;
            ec->addNode (newNode, graph, dummy);
        }
    }
    else if (const auto* dnm2 = dynamic_cast<const DisconnectNodeMessage*> (&msg))
    {
        ec->disconnectNode (dnm2->node, dnm2->inputs, dnm2->outputs,
                                        dnm2->audio, dnm2->midi);
    }
    else if (const auto* aps = dynamic_cast<const AddPresetMessage*> (&msg))
    {
        String name = aps->name;
        Node node = aps->node;
        bool canceled = false;

        if (name.isEmpty ())
        {
            AlertWindow alert ("Add Preset", "Enter preset name", AlertWindow::NoIcon, 0);
            alert.addTextEditor ("name", aps->node.getName());
            alert.addButton ("Save", 1, KeyPress (KeyPress::returnKey));
            alert.addButton ("Cancel", 0, KeyPress (KeyPress::escapeKey));
            canceled = 0 == alert.runModalLoop();
            name = alert.getTextEditorContents ("name");
        }

        if (! canceled)
        {
            presets->add (node, name);
            node.setProperty (Tags::name, name);
        }
    }
    else if (const auto* anm = dynamic_cast<const AddNodeMessage*> (&msg))
    {
        if (anm->target.isValid ())
            ec->addNode (anm->node, anm->target, anm->builder);
        else
            ec->addNode (anm->node);

        if (anm->sourceFile.existsAsFile() && anm->sourceFile.hasFileExtension(".elg"))
            recentFiles.addFile (anm->sourceFile);
    }
    else if (const auto* cbm = dynamic_cast<const ChangeBusesLayout*> (&msg))
    {
        ec->changeBusesLayout (cbm->node, cbm->layout);
    }
    else if (const auto* osm = dynamic_cast<const OpenSessionMessage*> (&msg))
    {
       #if defined (EL_PRO)
        sess->openFile (osm->file);
       #else
        findChild<GraphController>()->openGraph (osm->file);
       #endif
        recentFiles.addFile (osm->file);
    }
    else if (const auto* mdm = dynamic_cast<const AddMidiDeviceMessage*> (&msg))
    {
        ec->addMidiDeviceNode (mdm->device, mdm->inputDevice);
    }
    else if (const auto* removeControllerDeviceMessage = dynamic_cast<const RemoveControllerDeviceMessage*> (&msg))
    {
        const auto device = removeControllerDeviceMessage->device;
        devs->remove (device);
    }
    else if (const auto* addControllerDeviceMessage = dynamic_cast<const AddControllerDeviceMessage*> (&msg))
    {
        const auto device = addControllerDeviceMessage->device;
        const auto file   = addControllerDeviceMessage->file;
        if (file.existsAsFile())
        {
            devs->add (file);
        }
        else if (device.getValueTree().isValid())
        {
            devs->add (device);
        }
        else
        {
            DBG("[EL] add controller device not valid");
        }
    }
    else if (const auto* removeControlMessage = dynamic_cast<const RemoveControlMessage*> (&msg))
    {
        const auto device = removeControlMessage->device;
        const auto control = removeControlMessage->control;
        devs->remove (device, control);
    }
    else if (const auto* addControlMessage = dynamic_cast<const AddControlMessage*> (&msg))
    {
        const auto device (addControlMessage->device);
        const auto control (addControlMessage->control);
        devs->add (device, control);
    }
    else if (const auto* refreshControllerDevice = dynamic_cast<const RefreshControllerDeviceMessage*> (&msg))
    {
        const auto device = refreshControllerDevice->device;
        devs->refresh (device);
    }
    else if (const auto* removeMapMessage = dynamic_cast<const RemoveControllerMapMessage*> (&msg))
    {
        const auto controllerMap = removeMapMessage->controllerMap;
        maps->remove (controllerMap);
        gui->stabilizeViews();
    }
    else if (const auto* replaceNodeMessage = dynamic_cast<const ReplaceNodeMessage*> (&msg))
    {
        const auto graph = replaceNodeMessage->graph;
        const auto node  = replaceNodeMessage->node;
        const auto desc (replaceNodeMessage->description);
        if (graph.isValid() && node.isValid() && 
            graph.getNodesValueTree() == node.getValueTree().getParent())
        {
            ec->replace (node, desc);
        }
    }
    else
    {
        handled = false;
    }
    
    if (! handled)
    {
        DBG("[EL] unhandled Message received");
    }
}

ApplicationCommandTarget* AppController::getNextCommandTarget()
{
    return findChild<GuiController>();
}

void AppController::getAllCommands (Array<CommandID>& cids)
{
    cids.addArray ({
        Commands::mediaNew,
        Commands::mediaOpen,
        Commands::mediaSave,
        Commands::mediaSaveAs,
        
        Commands::signIn,
        Commands::signOut,
       #ifdef EL_PRO
        Commands::sessionNew,
        Commands::sessionSave,
        Commands::sessionSaveAs,
        Commands::sessionOpen,
        Commands::sessionAddGraph,
        Commands::sessionDuplicateGraph,
        Commands::sessionDeleteGraph,
        Commands::sessionInsertPlugin,
       #endif
        Commands::importGraph,
        Commands::exportGraph,
        Commands::panic,
        
        Commands::checkNewerVersion,
        
        Commands::transportPlay,

       #ifndef EL_PRO
        Commands::graphNew,
        Commands::graphOpen,
        Commands::graphSave,
        Commands::graphSaveAs,
        Commands::importSession
       #endif
    });
    cids.addArray({ Commands::copy, Commands::paste, Commands::undo, Commands::redo });
}

void AppController::getCommandInfo (CommandID commandID, ApplicationCommandInfo& result)
{
    findChild<GuiController>()->getCommandInfo (commandID, result);
    // for (auto* const child : getChildren())
    //     if (auto* const appChild = dynamic_cast<AppController::Child*> (child))
    //         appChild->getCommandInfo (commandID, result);
}

bool AppController::perform (const InvocationInfo& info)
{
    bool res = true;
    switch (info.commandID)
    {
        case Commands::undo: {
            if (undo.canUndo())
                undo.undo();
            if (auto* cc = findChild<GuiController>()->getContentComponent())
                cc->stabilizeViews();
            findChild<GuiController>()->refreshMainMenu();
        } break;
        
        case Commands::redo: {
            if (undo.canRedo())
                undo.redo();
            if (auto* cc = findChild<GuiController>()->getContentComponent())
                cc->stabilizeViews();
            findChild<GuiController>()->refreshMainMenu();
        } break;

        case Commands::sessionOpen:
        {
            FileChooser chooser ("Open Session", lastSavedFile, "*.els", true, false);
            if (chooser.browseForFileToOpen())
            {
                findChild<SessionController>()->openFile (chooser.getResult());
                recentFiles.addFile (chooser.getResult());
            }

        } break;

        case Commands::sessionNew:
            findChild<SessionController>()->newSession();
            break;
        case Commands::sessionSave:
            findChild<SessionController>()->saveSession (false);
            break;
        case Commands::sessionSaveAs:
            findChild<SessionController>()->saveSession (true);
            break;
        case Commands::sessionClose:
            findChild<SessionController>()->closeSession();
            break;
        case Commands::sessionAddGraph:
            findChild<EngineController>()->addGraph();
            break;
        case Commands::sessionDuplicateGraph:
            findChild<EngineController>()->duplicateGraph();
            break;
        case Commands::sessionDeleteGraph:
            findChild<EngineController>()->removeGraph();
            break;
        
        case Commands::transportPlay:
            getWorld().getAudioEngine()->togglePlayPause();
            break;
            
        case Commands::importGraph:
        {
            FileChooser chooser ("Import Graph", lastExportedGraph, "*.elg");
            if (chooser.browseForFileToOpen())
                findChild<SessionController>()->importGraph (chooser.getResult());
            
        } break;
            
        case Commands::exportGraph:
        {
            auto session = getWorld().getSession();
            auto node = session->getCurrentGraph();
            node.savePluginState();
            
            if (!lastExportedGraph.isDirectory())
                lastExportedGraph = lastExportedGraph.getParentDirectory();
            if (lastExportedGraph.isDirectory())
            {
                lastExportedGraph = lastExportedGraph.getChildFile(node.getName()).withFileExtension ("elg");
                lastExportedGraph = lastExportedGraph.getNonexistentSibling();
            }

            {
                FileChooser chooser ("Export Graph", lastExportedGraph, "*.elg");
                if (chooser.browseForFileToSave (true))
                    findChild<SessionController>()->exportGraph (node, chooser.getResult());
                if (auto* gui = findChild<GuiController>())
                    gui->stabilizeContent();
            }
        } break;

        case Commands::panic:
        {
            auto e = getWorld().getAudioEngine();
            for (int c = 1; c <= 16; ++c)
            {
                auto msg = MidiMessage::allNotesOff (c);
                msg.setTimeStamp (Time::getMillisecondCounterHiRes());
                e->addMidiMessage (msg);
                msg = MidiMessage::allSoundOff(c);
                msg.setTimeStamp (Time::getMillisecondCounterHiRes());
                e->addMidiMessage (msg);
            }
        }  break;
            
        case Commands::mediaNew:
        case Commands::mediaSave:
        case Commands::mediaSaveAs:
            break;
        
        case Commands::signIn:
        {
            
        } break;
        
        case Commands::signOut:
        {
            // noop
        } break;
        
        case Commands::checkNewerVersion:
            CurrentVersion::checkAfterDelay (20, true);
            break;
        
        case Commands::graphNew:
            findChild<GraphController>()->newGraph();
            break;
        case Commands::graphOpen:
        {
            FileChooser chooser ("Open Graph", lastSavedFile, "*.elg", true, false);
            if (chooser.browseForFileToOpen())
            {
                findChild<GraphController>()->openGraph (chooser.getResult());
                recentFiles.addFile (chooser.getResult());
            }
        } break;
        case Commands::graphSave:
            findChild<GraphController>()->saveGraph (false);
            break;
        case Commands::graphSaveAs: 
            findChild<GraphController>()->saveGraph (true);
            break;
        case Commands::importSession:
        {
            FileChooser chooser ("Import Session Graph", lastSavedFile, "*.els", true, false);
            if (chooser.browseForFileToOpen())
            {
                findChild<GraphController>()->openGraph (chooser.getResult());
                recentFiles.addFile (chooser.getResult());
            }
        } break;

        default: 
            res = false; 
            break;
    }

    return res;
}

void AppController::checkForegroundStatus()
{
   #if ! EL_RUNNING_AS_PLUGIN
    class CheckForeground : public CallbackMessage
    {
    public:
        CheckForeground (AppController& a) : app (a) { }
        void messageCallback() override
        {
            static bool sIsForeground = true;
            const auto foreground = Process::isForegroundProcess();
            if (sIsForeground == foreground)
                return;
            
            if (! app.getWorld().getSettings().hidePluginWindowsWhenFocusLost())
                return;

            auto session  = app.getWorld().getSession();
            auto& gui     = *app.findChild<GuiController>();
            const Node graph (session->getCurrentGraph());
            jassert (session);
            if (foreground)
            {
                gui.showPluginWindowsFor (graph, true, false);
                gui.getMainWindow()->toFront (true);
            }
            else if (! foreground)
            {
                gui.closeAllPluginWindows();
            }
            
            sIsForeground = foreground;
        }

    private:
        AppController& app;
    };

    (new CheckForeground(*this))->post();
   #endif
}

void AppController::licenseRefreshed()
{
   #if 0
    findChild<Element::DevicesController>()->refresh();
    findChild<Element::MappingController>()->learn (false);
    if (auto engine = getWorld().getAudioEngine())
        engine->updateUnlockStatus();

   #if EL_RUNNING_AS_PLUGIN
    // FIXME: this came from UnlockForm.cpp
    // typedef ElementPluginAudioProcessorEditor EdType;
    // if (EdType* editor = form.findParentComponentOfClass<EdType>())
    //     editor->triggerAsyncUpdate();
   #else
    getWorld().getDeviceManager().restartLastAudioDevice();
   #endif

    findChild<GuiController>()->stabilizeContent();
    findChild<GuiController>()->stabilizeViews();
   #endif
}

}
