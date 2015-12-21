/*
    Main.cpp - This file is part of Element
    Copyright (C) 2014  Kushview, LLC.  All rights reserved.

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

#include "element/Juce.h"
#include "controllers/AppController.h"
#include "engine/AudioEngine.h"
#include "engine/InternalFormat.h"
#include "gui/Alerts.h"
#include "gui/GuiApp.h"
#include "session/Session.h"
#include "Globals.h"
#include "Settings.h"

namespace Element {

class StartupThread :  public Thread
{
public:
    StartupThread (Globals& w)
        : Thread ("element_startup"),
          world (w)
    { }

    ~StartupThread() { }

    void run() override
    {
        Settings& settings (world.settings());
        if (ScopedXml dxml = settings.getUserSettings()->getXmlValue ("devices"))
             world.devices().initialise (16, 16, dxml.get(), true, "default", nullptr);

        AudioEnginePtr engine = new AudioEngine (world);
        world.setEngine (engine); // this will also instantiate the session

        // global data is ready, so now we can start using it;
        PluginManager& plugins (world.plugins());
        plugins.addDefaultFormats();
        plugins.addFormat (new InternalFormat (*engine));
        plugins.restoreUserPlugins (settings);

        engine->activate();

        world.loadModule ("test");
        controller = new AppController (world);
    }

    void launchApplication()
    {
        if (world.cli.fullScreen)
        {
            Desktop::getInstance().setKioskModeComponent(&screen, false);
            screen.setVisible (true);
        }

        startThread();

        while (isThreadRunning()) {
            MessageManager::getInstance()->runDispatchLoopUntil (30);
        }

        screen.removeFromDesktop();
    }



    ScopedPointer<AppController> controller;

private:
  class StartupScreen :  public TopLevelWindow
  {
  public:
    StartupScreen()
        : TopLevelWindow ("startup", true)
    {
        text.setText ("Loading Application", dontSendNotification);
        text.setSize (100, 100);
        text.setFont (Font (24.0f));
        text.setJustificationType (Justification::centred);
        text.setColour (Label::textColourId, Colours::white);
        addAndMakeVisible (text);
        centreWithSize (text.getWidth(), text.getHeight());
    }

    void resized() override {
        text.setBounds (getLocalBounds());
    }

    void paint (Graphics& g) override {
        g.fillAll (Colours::transparentBlack);
    }

  private:
      Label text;
  } screen;

  Globals& world;
};

class Application  : public JUCEApplication
{
public:
   Application() { }
   virtual ~Application() { }

   const String getApplicationName()       { return "Element"; }
   const String getApplicationVersion()    { return ELEMENT_VERSION_STRING; }
   bool moreThanOneInstanceAllowed()       { return true; }

   void initialise (const String&  commandLine )
   {
#if JUCE_DEBUG
       const File path (File::getSpecialLocation (File::invokedExecutableFile));
       const String ep = path.getParentDirectory().getChildFile("modules").getFullPathName();
       DBG("path: " << ep);
       setenv("ELEMENT_MODULE_PATH", ep.toRawUTF8(), 1);
       DBG("module_path: " << getenv("ELEMENT_MODULE_PATH"));
#endif

       world = new Globals (commandLine);
       {
           StartupThread startup (*world);
           startup.launchApplication();
           controller = startup.controller.release();
           engine = world->engine();
       }

       gui = Gui::GuiApp::create (*world);
       gui->run();
   }

   void shutdown()
   {
       if (gui != nullptr)
           gui = nullptr;

       PluginManager& plugins (world->plugins());
       Settings& settings (world->settings());
       plugins.saveUserPlugins (settings);

       if (ScopedXml el = world->devices().createStateXml())
           settings.getUserSettings()->setValue ("devices", el);

       engine->deactivate();
       world->setEngine (nullptr);
       engine = nullptr;

       world->unloadModules();
       world = nullptr;
   }

   void systemRequestedQuit()
   {
       if (gui->shutdownApp())
       {
           gui = nullptr;
           this->quit();
       }
   }

   void anotherInstanceStarted (const String& /*commandLine*/)
   {

   }

   bool perform (const InvocationInfo& info) override
   {
       switch (info.commandID) {
           case Commands::quit: {
               this->systemRequestedQuit();
           } break;
       }

       return true;
   }

private:
    ScopedPointer<Globals>  world;
    AudioEnginePtr          engine;
    Scoped<Gui::GuiApp>     gui;
    ScopedPointer<AppController> controller;
};

}

START_JUCE_APPLICATION (Element::Application)
