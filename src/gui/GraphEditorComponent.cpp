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

#include "ElementApp.h"
#include "controllers/GraphController.h"
#include "controllers/GraphManager.h"

#include "gui/GuiCommon.h"
#include "gui/ContentComponent.h"
#include "gui/ContextMenus.h"
#include "gui/Icons.h"
#include "gui/PluginWindow.h"

#include "gui/AudioIOPanelView.h"
#include "gui/SessionTreePanel.h"
#include "gui/views/PluginsPanelView.h"
#include "gui/NavigationConcertinaPanel.h"
#include "gui/NodeIOConfiguration.h"
#include "engine/nodes/SubGraphProcessor.h"
#include "engine/nodes/BaseProcessor.h"

#include "session/PluginManager.h"
#include "session/Presets.h"
#include "session/Node.h"

#include "gui/GraphEditorComponent.h"

#include "ScopedFlag.h"

namespace Element {

static bool elNodeIsAudioMixer (const Node& node)
{
    return node.getFormat().toString() == "Element"
        && node.getIdentifier().toString() == "element.audioMixer";
}

static bool elNodeIsMidiDevice (const Node& node)
{
    return node.getFormat().toString() == "Internal"
        && ( node.getIdentifier().toString() == "element.midiInputDevice" ||
             node.getIdentifier().toString() == "element.midiOutputDevice" );
}

static bool elNodeCanChangeIO (const Node& node)
{
    return !node.isIONode() 
        && !node.isGraph()
        && !elNodeIsAudioMixer (node)
        && !elNodeIsMidiDevice (node);
}

class PigWhipSource : public Component {
public:
    PigWhipSource ()
    {
        int ssize = 10;
        setSize (ssize, ssize);
        path.addEllipse (2.0, 2.0, ssize - 2, ssize - 2);
    }

    ~PigWhipSource() { }

    void paint (Graphics& g) override
    {
        g.setColour (LookAndFeel::widgetBackgroundColor);
        g.fillPath (path);
    }

    void mouseDown (const MouseEvent& e) override
    {
        DBG("down");
    }

    void mouseDrag (const MouseEvent& e) override
    {
        DBG("drag");
    }

    void mouseUp (const MouseEvent& e) override
    {
        DBG("up");
    }

private:
    Path path;
    GraphEditorComponent* getGraphPanel() const {
        return findParentComponentOfClass<GraphEditorComponent>();
    }
};

class PinComponent   : public Component,
                       public SettableTooltipClient
{
public:
    PinComponent (const Node& graph_, const Node& node_,
                  const uint32 filterID_,
                  const uint32 index_, const bool isInput_,
                  const PortType type_, const bool verticle_)
        : filterID (filterID_),
          port (index_),
          type (type_),
          isInput (isInput_),
          graph (graph_), node (node_),
          verticle (verticle_)
    {
        if (const GraphNodePtr obj = node.getGraphNode())
        {
            const Port p (node.getPort ((int) port));
            String tip = p.getName();
            
            if (tip.isEmpty())
            {
                if (node.isAudioInputNode())
                {
                    tip = "Input " + String (index_ + 1);
                }
                else if (node.isAudioOutputNode())
                {
                    tip = "Output " + String (index_ + 1);
                }
                else
                {
                    tip = (isInput ? "Input " : "Output ") + String (index_ + 1);
                }
            }

            setTooltip (tip);
        }

        setSize (16, 16);
    }

    void paint (Graphics& g) override
    {    
        g.setColour (getColor());

       #if 0
        // stick with circle
        const float w = (float) getWidth();
        const float h = (float) getHeight();
        Path p;
        if (verticle)
        {
            p.addEllipse (w * 0.25f, h * 0.25f, w * 0.5f, h * 0.5f);
            p.addRectangle (w * 0.4f,
                            isInput ? (0.5f * h) : 0.0f,
                            w * 0.2f,
                            h * 0.5f);
        }
        else
        {
            p.addEllipse (w * 0.25f, h * 0.25f, w * 0.5f, h * 0.5f);
            p.addRectangle (isInput ? (0.5f * w) : 0.0f,
                            h * 0.4f,
                            w * 0.5f,
                            h * 0.2f);

        }
        g.fillPath (p);
       #endif

       #if 0
        // half circles
        Path p;
        if (isInput)
        {
            p.addPieSegment (getLocalBounds().toFloat(),
                            -juce::float_Pi * 0.5, juce::float_Pi * 0.5f,
                            0);
        }
        else
        {
            p.addPieSegment (getLocalBounds().toFloat(),
                            juce::float_Pi * 0.5f, juce::float_Pi * 1.5f,
                            0);
        }
        
        g.fillPath (p);
       #else
        // full circle
        g.fillEllipse (getLocalBounds().toFloat());
        g.setColour (Colours::black);
        g.drawEllipse (getLocalBounds().toFloat(), 0.5f);
       #endif
    }

    Colour getColor() const
    {
        switch (this->type)
        {
            case PortType::Audio:   return Colours::lightgreen; break;
            case PortType::Control: return Colours::lightblue;  break;
            default: break;
        }
        
        return Colours::orange;
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (! isEnabled())
            return;
        getGraphPanel()->beginConnectorDrag (isInput ? 0 : filterID,
                                             port,
                                             isInput ? filterID : 0,
                                             port,
                                             e);
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (! isEnabled())
            return;
        getGraphPanel()->dragConnector (e);
    }

    void mouseUp (const MouseEvent& e) override
    {
        if (! isEnabled())
            return;
        getGraphPanel()->endDraggingConnector (e);
    }

    const uint32    filterID;
    const uint32    port;
    const PortType  type;
    const bool      isInput;

private:
    Node graph, node;
    bool verticle;
    GraphEditorComponent* getGraphPanel() const noexcept
    {
        return findParentComponentOfClass<GraphEditorComponent>();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PinComponent)
};

class FilterComponent    : public Component,
                           public Button::Listener,
                           public AsyncUpdater,
                           public Value::Listener
{
public:
    FilterComponent (const Node& graph_, const Node& node_, const bool vertical_)
        : filterID (node_.getNodeId()), graph (graph_), node (node_), font (11.0f)
    {
        setBufferedToImage (true);
        nodeEnabled = node.getPropertyAsValue (Tags::enabled);
        nodeEnabled.addListener (this);
        nodeName = node.getPropertyAsValue (Tags::name);
        nodeName.addListener (this);

        shadow.setShadowProperties (DropShadow (Colours::black.withAlpha (0.5f), 3, Point<int> (0, 1)));
        setComponentEffect (&shadow);
        
        addAndMakeVisible (ioButton);
        ioButton.setPath (getIcons().fasCog);
        ioButton.addListener (this);
        ioButton.setVisible (elNodeCanChangeIO (node));

        if (! node.isIONode() && ! node.isRootGraph())
        {
            addAndMakeVisible (powerButton);
            powerButton.setColour (SettingButton::backgroundOnColourId,
                                   findColour (SettingButton::backgroundColourId));
            powerButton.setColour (SettingButton::backgroundColourId, Colors::toggleBlue);
            powerButton.getToggleStateValue().referTo (node.getPropertyAsValue (Tags::bypass));
            powerButton.setClickingTogglesState (true);
            powerButton.addListener (this);

            addAndMakeVisible (muteButton);
            muteButton.setYesNoText ("M", "M");
            muteButton.setColour (SettingButton::backgroundOnColourId, Colors::toggleRed);
            muteButton.getToggleStateValue().referTo (node.getPropertyAsValue (Tags::mute));
            muteButton.setClickingTogglesState (true);
            muteButton.addListener (this);
        }

        setSize (170, 60);
    }

    ~FilterComponent() noexcept
    {
        nodeEnabled.removeListener (this);
        nodeName.removeListener (this);
        deleteAllPins();
    }

    void valueChanged (Value& value) override
    {
        if (nodeEnabled.refersToSameSourceAs (value)) {
            repaint();
        } else if (nodeName.refersToSameSourceAs (value)) {
            setName (node.getName());
            repaint();
        }
    }

    void handleAsyncUpdate() override
    {
        repaint();
    }

    void buttonClicked (Button* b) override 
    {
        if (! isEnabled())
            return;

        GraphNodePtr obj = node.getGraphNode();
        auto* proc = (obj) ? obj->getAudioProcessor() : 0;
        if (! proc) return;

        if (b == &ioButton && ioButton.getToggleState())
        {
            ioButton.setToggleState (false, dontSendNotification);
            ioBox.clear();
        }
        else if (b == &ioButton && !ioButton.getToggleState())
        {
            auto* const component = new NodeAudioBusesComponent (node, proc,
                    ViewHelpers::findContentComponent (this));
            auto& box = CallOutBox::launchAsynchronously (
                component, ioButton.getScreenBounds(), 0);
            ioBox.setNonOwned (&box);
        }
        else if (b == &powerButton)
        {
            if (obj->isSuspended() != node.isBypassed())
                obj->suspendProcessing (node.isBypassed());
        }
        else if (b == &muteButton)
        {
            node.setMuted (muteButton.getToggleState());
        }
    }

    void deleteAllPins()
    {
        for (int i = getNumChildComponents(); --i >= 0;)
            if (auto * c = dynamic_cast<PinComponent*> (getChildComponent(i)))
                delete c;
    }

    void mouseDown (const MouseEvent& e) override
    {
        if (! isEnabled())
            return;

        bool collapsedToggled = false;
        if (! vertical && getOpenCloseBox().contains (e.x, e.y))
        {
            node.setProperty (Tags::collapsed, !collapsed);
            update (false);
            getGraphPanel()->updateConnectorComponents();
            collapsedToggled = true;
            blockDrag = true;
        }

        originalPos = localPointToGlobal (Point<int>());
        toFront (true);
        dragging = false;
        auto* const panel = getGraphPanel();
        
        selectionMouseDownResult = panel->selectedNodes.addToSelectionOnMouseDown (node.getNodeId(), e.mods);
        if (auto* cc = ViewHelpers::findContentComponent (this))
        {
            ScopedFlag block (panel->ignoreNodeSelected, true);
            cc->getAppController().findChild<GuiController>()->selectNode (node);
        }

        if (! collapsedToggled)
        {
            if (e.mods.isPopupMenu())
            {
                auto* const world = ViewHelpers::getGlobals (this);
                auto& plugins (world->getPluginManager());
                NodePopupMenu menu (node);
                menu.addReplaceSubmenu (plugins);
                menu.addSeparator();
                menu.addOptionsSubmenu();
                
                if (world)
                    menu.addPresetsMenu (world->getPresetCollection());
                
                const int result = menu.show();
                if (auto* message = menu.createMessageForResultCode (result))
                {
                    ViewHelpers::postMessageFor (this, message);
                    for (const auto& nodeId : getGraphPanel()->selectedNodes)
                    {
                        if (nodeId == node.getNodeId ())
                            continue;
                        const Node selectedNode = graph.getNodeById (nodeId);
                        if (selectedNode.isValid())
                        {
                            if (nullptr != dynamic_cast<RemoveNodeMessage*> (message))
                            {
                                ViewHelpers::postMessageFor (this, new RemoveNodeMessage (selectedNode));
                            }
                        }
                    }
                }
                else if (plugins.getKnownPlugins().getIndexChosenByMenu(result) >= 0)
                {
                    const auto index = plugins.getKnownPlugins().getIndexChosenByMenu (result);
                    if (const auto* desc = plugins.getKnownPlugins().getType (index))
                        ViewHelpers::postMessageFor (this, new ReplaceNodeMessage (node, *desc));
                }
            }
        }

        repaint();
        getGraphPanel()->updateSelection();
    }

    void setNodePosition (const int x, const int y)
    {
        if (vertical)
        {
            node.setRelativePosition ((x + getWidth() / 2) / (double) getParentWidth(),
                                      (y + getHeight() / 2) / (double) getParentHeight());
        }
        else
        {
            node.setRelativePosition ((y + getHeight() / 2) / (double) getParentHeight(),
                                      (x + getWidth() / 2) / (double) getParentWidth());
        }
    }

    void mouseDrag (const MouseEvent& e) override
    {
        if (! isEnabled())
            return;

        if (e.mods.isPopupMenu() || blockDrag)
            return;
        dragging = true;
        Point<int> pos (originalPos + Point<int> (e.getDistanceFromDragStartX(), e.getDistanceFromDragStartY()));
        
        if (getParentComponent() != nullptr)
            pos = getParentComponent()->getLocalPoint (nullptr, pos);
        
        setNodePosition (pos.getX(), pos.getY());
        updatePosition();
    }

    void mouseUp (const MouseEvent& e) override
    {
        if (! isEnabled())
            return;
        auto* panel = getGraphPanel();
        
        if (panel)
            panel->selectedNodes.addToSelectionOnMouseUp (node.getNodeId(), e.mods,
                                                          dragging, selectionMouseDownResult);

        if (e.mouseWasClicked() && e.getNumberOfClicks() == 2)
            makeEditorActive();

        dragging = selectionMouseDownResult = blockDrag = false;   
    }

    void updatePosition()
    {
        node.getRelativePosition (relativeX, relativeY);
        vertical ? setCentreRelative (relativeX, relativeY)
                 : setCentreRelative (relativeY, relativeX);
        getGraphPanel()->updateConnectorComponents();
    }

    void makeEditorActive()
    {
        if (node.isGraph())
        {
            // TODO: this can cause a crash, do it async
            if (auto* cc = ViewHelpers::findContentComponent (this))
                cc->setCurrentNode (node);
        }
        else if (node.hasProperty (Tags::missing))
        {
            String message = "This node is unavailable and running as a Placeholder.\n";
            message << node.getName() << " (" << node.getFormat().toString() 
                    << ") could not be found for loading.";
            AlertWindow::showMessageBoxAsync (AlertWindow::InfoIcon, 
                node.getName(), message, "Ok");
        }
        else if (node.isValid())
        {
            ViewHelpers::presentPluginWindow (this, node);
        }
    }
    
    bool hitTest (int x, int y) override
    {
        for (int i = getNumChildComponents(); --i >= 0;)
            if (getChildComponent(i)->getBounds().contains (x, y))
                return true;

        return vertical ? x >= 3 && x < getWidth() - 6 && y >= pinSize && y < getHeight() - pinSize
                        : y >= 3 && y < getHeight() - 6 && x >= pinSize && x < getWidth() - pinSize;
    }

    Rectangle<int> getOpenCloseBox() const
    {
        const auto box (getBoxRectangle());
        return { box.getX() + 5, box.getY() + 4, 16, 16 };
    }

    Rectangle<int> getBoxRectangle() const
    {
       #if 0
        // for pins with stems
        if (vertical)
        {
            const int x = 4;
            const int y = pinSize;
            const int w = getWidth() - x * 2;
            const int h = getHeight() - pinSize * 2;
            
            return Rectangle<int> (x, y, w, h);
        }

        const int x = pinSize;
        const int y = 4;
        const int w = getWidth() - pinSize * 2;
        const int h = getHeight() - y * 2;
        
        return { x, y, w, h };
       #else
        if (vertical)
        {
            return Rectangle<int> (
                0,
                pinSize / 2,
                getWidth(),
                getHeight() - pinSize
            ).reduced (2, 0);
        }

        return Rectangle<int> (
            pinSize / 2,
            0,
            getWidth() - pinSize,
            getHeight()
        ).reduced (0, 2);
       #endif
    }
    
    void paintOverChildren (Graphics& g) override
    {
        ignoreUnused (g);
    }

    void paint (Graphics& g) override
    {
        const float cornerSize = 2.4f;
        const auto box (getBoxRectangle());

        g.setColour (isEnabled() && node.isEnabled()
            ? LookAndFeel::widgetBackgroundColor.brighter (0.8) 
            : LookAndFeel::widgetBackgroundColor.brighter (0.2));
        g.fillRoundedRectangle (box.toFloat(), cornerSize);

        if (! vertical)
        {
            getLookAndFeel().drawTreeviewPlusMinusBox (
                g, getOpenCloseBox().toFloat(),
                LookAndFeel::widgetBackgroundColor.brighter (0.7), 
                ! collapsed, false); 
        }

        if (node.getValueTree().hasProperty (Tags::missing))
        {
            g.setColour (Colour (0xff333333));
            g.setFont (9.f);
            auto pr = box; pr.removeFromTop (6);
            g.drawFittedText ("(placeholder)", pr, Justification::centred, 2);
        }

        g.setColour (Colours::black);
        g.setFont (font);
        
        auto displayName = node.getDisplayName();
        auto subName = node.hasModifiedName() ? node.getPluginName() : String();
        
        if (node.getParentGraph().isRootGraph())
        {
            if (node.isAudioIONode())
            {
                // FIXME: uniform way to refresh node names
                //        see https://github.com/kushview/Element/issues/109
                subName = String();
            }
            else if (node.isMidiInputNode())
            {
                auto& midi = ViewHelpers::getGlobals(this)->getMidiEngine();
                if (midi.getNumActiveMidiInputs() <= 0)
                    subName = "(no device)";
            }
        }

        if (vertical)
        {
            g.drawFittedText (displayName, box.getX() + 9, box.getY() + 2, box.getWidth(),
                                         18, Justification::centredLeft, 2);

            if (subName.isNotEmpty())
            {
                g.setFont (Font (8.f));
                g.drawFittedText (subName, box.getX() + 9, box.getY() + 10, box.getWidth(),
                                  18, Justification::centredLeft, 2);
            }
        }
        else
        {
            g.drawFittedText (displayName, box.getX() + 20, box.getY() + 2, box.getWidth(),
                                           18, Justification::centredLeft, 2);
            if (subName.isNotEmpty())
            {
                g.setFont (Font (8.f));
                g.drawFittedText (subName, box.getX() + 20, box.getY() + 10, box.getWidth(),
                                  18, Justification::centredLeft, 2);
            }
        }
        
        bool selected = getGraphPanel()->selectedNodes.isSelected (node.getNodeId());
        g.setColour (selected ? Colors::toggleBlue : Colours::grey);
        g.drawRoundedRectangle (box.toFloat(), cornerSize, 1.4);
    }

    void resized() override
    {
        const auto box (getBoxRectangle());
        auto r = box.reduced(4, 2).removeFromBottom (14);
        ioButton.setBounds (r.removeFromRight (16)); 
        r.removeFromLeft (3);
        muteButton.setBounds (r.removeFromRight (16));       
        r.removeFromLeft (2);
        powerButton.setBounds (r.removeFromRight (16));

        const int halfPinSize = pinSize / 2;
        if (vertical)
        {
            Rectangle<int> pri (box.getX() + 9, 0, getWidth(), pinSize);
            Rectangle<int> pro (box.getX() + 9, getHeight() - pinSize, getWidth(), pinSize);
            for (int i = 0; i < getNumChildComponents(); ++i)
            {
                if (PinComponent* const pc = dynamic_cast <PinComponent*> (getChildComponent(i)))
                {
                    pc->setBounds (pc->isInput ? pri.removeFromLeft (pinSize) 
                                               : pro.removeFromLeft (pinSize));
                    pc->isInput ? pri.removeFromLeft (pinSize * 1.25)
                                : pro.removeFromLeft (pinSize * 1.25);                  
                }
            }
        }
        else
        {
            Rectangle<int> pri (box.getX() - halfPinSize, 
                                box.getY() + 9, 
                                pinSize, 
                                box.getHeight());
            Rectangle<int> pro (box.getWidth(),
                                box.getY() + 9, 
                                pinSize, 
                                box.getHeight());
            float scale = collapsed ? 0.25f : 1.125f;
            int spacing = jmax (2, int (pinSize * scale));
            for (int i = 0; i < getNumChildComponents(); ++i)
            {
                if (PinComponent* const pc = dynamic_cast <PinComponent*> (getChildComponent(i)))
                {
                    pc->setBounds (pc->isInput ? pri.removeFromTop (pinSize) 
                                               : pro.removeFromTop (pinSize));
                    pc->isInput ? pri.removeFromTop (spacing)
                                : pro.removeFromTop (spacing);
                }
            }
        }
    }

    void getPinPos (const int index, const bool isInput, float& x, float& y)
    {
        for (int i = 0; i < getNumChildComponents(); ++i)
        {
            if (PinComponent* const pc = dynamic_cast <PinComponent*> (getChildComponent(i)))
            {
                if (pc->port == index && isInput == pc->isInput)
                {
                    x = getX() + pc->getX() + pc->getWidth() * 0.5f;
                    y = getY() + pc->getY() + pc->getHeight() * 0.5f;
                    break;
                }
            }
        }
    }

    void update (const bool doPosition = true)
    {
        vertical = getGraphPanel()->isLayoutVertical();
    
        if (! node.getValueTree().getParent().hasType (Tags::nodes))
        {
            delete this;
            return;
        }
        collapsed = (bool) node.getProperty (Tags::collapsed, false);
        numIns = numOuts = 0;
        const auto numPorts = node.getPortsValueTree().getNumChildren();
        for (int i = 0; i < numPorts; ++i)
        {
            const Port port (node.getPort (i));
            if (PortType::Control == port.getType())
                continue;
            
            if (port.isInput())
                ++numIns;
            else
                ++numOuts;
        }

        int w = 120;
        int h = 46;

        const int maxPorts = jmax (numIns, numOuts) + 1;
        
        if (vertical)
        {
            w = jmax (w, int(maxPorts * pinSize) + int(maxPorts * pinSize * 1.25f));
        }
        else
        {
            float scale = collapsed ? 0.25f : 1.125f;
            int endcap = collapsed ? 9 : -5;
            h = jmax (h, int(maxPorts * pinSize) + int(maxPorts * jmax(int(pinSize * scale), 2)) + endcap);
        }
        
        int textWidth = font.getStringWidth (node.getDisplayName());
        textWidth += (vertical) ? 20 : 36;
        setSize (jmax (w, textWidth), h);
        setName (node.getDisplayName());

        if (doPosition)
        {
            updatePosition();
        }
        else if (nullptr != getParentComponent())
        {
            // position is relative and parent might be resizing
            const auto b = getBoundsInParent();
            setNodePosition (b.getX(), b.getY());
        }

        if (numIns != numInputs || numOuts != numOutputs)
        {
            numInputs  = numIns;
            numOutputs = numOuts;

            deleteAllPins();

            for (uint32 i = 0; i < (uint32) numPorts; ++i)
            {
                const Port port (node.getPort (i));
                const PortType t (port.getType());
                if (t == PortType::Control)
                    continue;
                
                const bool isInput (port.isInput());
                addAndMakeVisible (new PinComponent (graph, node, filterID, i, isInput, t, vertical));
            }

            resized();
        }
    }
    
    const uint32 filterID;

private:
    Node graph;
    Node node;

    Value nodeEnabled;
    Value nodeName;

    int numInputs = 0, numOutputs = 0;
    int numIns = 0, numOuts = 0;

    double relativeX = 0.5f;
    double relativeY = 0.5f;

    int pinSize = 9;    
    Font font;
    
    Point<int> originalPos;
    bool selectionMouseDownResult = false;
    bool vertical = true;
    bool dragging = false;
    bool blockDrag = false;
    bool collapsed = false;

    SettingButton ioButton;
    PowerButton powerButton;
    SettingButton muteButton;

    OptionalScopedPointer<CallOutBox> ioBox;
    PigWhipSource pigWhip;

    DropShadowEffect shadow;
    ScopedPointer<Component> embedded;

    GraphEditorComponent* getGraphPanel() const noexcept
    {
        return findParentComponentOfClass<GraphEditorComponent>();
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(FilterComponent);
};

class ConnectorComponent   : public Component,
                             public SettableTooltipClient
{
public:
    ConnectorComponent (const Node& graph_)
        : sourceFilterID (0), destFilterID (0),
          sourceFilterChannel (0), destFilterChannel (0),
          graph (graph_),
          lastInputX (0), lastInputY (0),
          lastOutputX (0), lastOutputY (0)
    { }

    ~ConnectorComponent() { }

    bool isDragging() const { return dragging; }
    void setGraph (const Node& g) { graph = g; }

    void setInput (const uint32 sourceFilterID_, const int sourceFilterChannel_)
    {
        if (sourceFilterID != sourceFilterID_ || sourceFilterChannel != sourceFilterChannel_)
        {
            sourceFilterID = sourceFilterID_;
            sourceFilterChannel = sourceFilterChannel_;
            update();
        }
    }

    void setOutput (const uint32 destFilterID_, const int destFilterChannel_)
    {
        if (destFilterID != destFilterID_ || destFilterChannel != destFilterChannel_)
        {
            destFilterID = destFilterID_;
            destFilterChannel = destFilterChannel_;
            update();
        }
    }

    void dragStart (int x, int y)
    {
        lastInputX = (float) x;
        lastInputY = (float) y;
        resizeToFit();
    }

    void dragEnd (int x, int y)
    {
        lastOutputX = (float) x;
        lastOutputY = (float) y;
        resizeToFit();
    }

    void update()
    {
        float x1, y1, x2, y2;
        getPoints (x1, y1, x2, y2);

        if (lastInputX != x1
             || lastInputY != y1
             || lastOutputX != x2
             || lastOutputY != y2)
        {
            resizeToFit();
        }
    }

    void resizeToFit()
    {
        float x1, y1, x2, y2;
        getPoints (x1, y1, x2, y2);

        const Rectangle<int> newBounds ((int) jmin (x1, x2) - 4,
                                        (int) jmin (y1, y2) - 4,
                                        (int) fabsf (x1 - x2) + 8,
                                        (int) fabsf (y1 - y2) + 8);
        setBounds (newBounds);
        repaint();
    }

    void getPoints (float& x1, float& y1, float& x2, float& y2) const
    {
        x1 = lastInputX;
        y1 = lastInputY;
        x2 = lastOutputX;
        y2 = lastOutputY;

        if (GraphEditorComponent* const hostPanel = getGraphPanel())
        {
            if (FilterComponent* srcFilterComp = hostPanel->getComponentForFilter (sourceFilterID))
                srcFilterComp->getPinPos (sourceFilterChannel, false, x1, y1);

            if (FilterComponent* dstFilterComp = hostPanel->getComponentForFilter (destFilterID))
                dstFilterComp->getPinPos (destFilterChannel, true, x2, y2);
        }
    }

    void paint (Graphics& g)
    {
        g.setColour (Colours::black.brighter());
        g.fillPath (linePath);
    }

    bool hitTest (int x, int y)
    {
        if (hitPath.contains ((float) x, (float) y))
        {
            double distanceFromStart, distanceFromEnd;
            getDistancesFromEnds (x, y, distanceFromStart, distanceFromEnd);

            // avoid clicking the connector when over a pin
            return distanceFromStart > 7.0 && distanceFromEnd > 7.0;
        }

        return false;
    }

    void mouseDown (const MouseEvent&)
    {
        if (! isEnabled())
            return;
        dragging = false;
    }

    void mouseDrag (const MouseEvent& e)
    {
        if (! isEnabled())
            return;

        if ((! dragging) && ! e.mouseWasClicked())
        {
            dragging = true;
            
            double distanceFromStart, distanceFromEnd;
            getDistancesFromEnds (e.x, e.y, distanceFromStart, distanceFromEnd);
            const bool isNearerSource = (distanceFromStart < distanceFromEnd);
            ViewHelpers::postMessageFor (this, new RemoveConnectionMessage (
                    sourceFilterID, (uint32)sourceFilterChannel,
                    destFilterID, (uint32)destFilterChannel, graph));
                
            getGraphPanel()->beginConnectorDrag (isNearerSource ? 0 : sourceFilterID, sourceFilterChannel,
                                                    isNearerSource ? destFilterID : 0,
                                                    destFilterChannel,
                                                    e);
        }
        else if (dragging)
        {
            getGraphPanel()->dragConnector (e);
        }
    }

    void mouseUp (const MouseEvent& e)
    {
        if (! isEnabled())
            return;
        if (dragging)
            getGraphPanel()->endDraggingConnector (e);
    }

    void resized()
    {
        float x1, y1, x2, y2;
        getPoints (x1, y1, x2, y2);

        lastInputX = x1;
        lastInputY = y1;
        lastOutputX = x2;
        lastOutputY = y2;

        x1 -= getX();
        y1 -= getY();
        x2 -= getX();
        y2 -= getY();

        linePath.clear();
        linePath.startNewSubPath (x1, y1);
        const bool vertical = getGraphPanel()->isLayoutVertical();
        
        if (vertical)
        {
            linePath.cubicTo (x1, y1 + (y2 - y1) * 0.33f,
                              x2, y1 + (y2 - y1) * 0.66f,
                              x2, y2);
        }
        else
        {
            linePath.cubicTo (x1 + (x2 - x1) * 0.33f, y1,
                              x1 + (x2 - x1) * 0.66f, y2,
                              x2, y2);
        }
        
        PathStrokeType wideStroke (8.0f);
        wideStroke.createStrokedPath (hitPath, linePath);

        PathStrokeType stroke (2.5f);
        stroke.createStrokedPath (linePath, linePath);

        const bool showArrow = false;
        
        if (showArrow)
        {
            const float arrowW = 5.0f;
            const float arrowL = 4.0f;

            Path arrow;
            arrow.addTriangle (-arrowL, arrowW,
                               -arrowL, -arrowW,
                               arrowL, 0.0f);

            arrow.applyTransform (AffineTransform()
                                    .rotated (float_Pi * 0.5f - (float) atan2 (x2 - x1, y2 - y1))
                                    .translated ((x1 + x2) * 0.5f,
                                                 (y1 + y2) * 0.5f));

            linePath.addPath (arrow);
        }
        linePath.setUsingNonZeroWinding (true);
    }

    uint32 sourceFilterID, destFilterID;
    int sourceFilterChannel, destFilterChannel;

private:
    Node graph;
    float lastInputX, lastInputY, lastOutputX, lastOutputY;
    Path linePath, hitPath;
    bool dragging;

    GraphEditorComponent* getGraphPanel() const noexcept
    {
        return findParentComponentOfClass<GraphEditorComponent>();
    }

    void getDistancesFromEnds (int x, int y, double& distanceFromStart, double& distanceFromEnd) const
    {
        float x1, y1, x2, y2;
        getPoints (x1, y1, x2, y2);

        distanceFromStart = juce_hypot (x - (x1 - getX()), y - (y1 - getY()));
        distanceFromEnd = juce_hypot (x - (x2 - getX()), y - (y2 - getY()));
    }

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ConnectorComponent)
};

GraphEditorComponent::GraphEditorComponent()
    : ViewHelperMixin (this)
{
    setOpaque (true);
    data.addListener (this);
}

GraphEditorComponent::~GraphEditorComponent()
{
    data.removeListener (this);
    graph = Node();
    data = ValueTree();
    draggingConnector = nullptr;
    resizePositionsFrozen = false;
    deleteAllChildren();
}

void GraphEditorComponent::setNode (const Node& n)
{
    bool isGraph = n.isGraph();
    bool isValid = n.isValid();
    graph = isValid && isGraph ? n : Node(Tags::graph);
    
    data.removeListener (this);
    data = graph.getValueTree();
    
    verticalLayout = graph.getProperty (Tags::vertical, true);
    resizePositionsFrozen = (bool) graph.getProperty (Tags::staticPos, false);

    if (draggingConnector)
        removeChildComponent (draggingConnector);
    deleteAllChildren();
    updateComponents();
    if (draggingConnector)
        addAndMakeVisible (draggingConnector);
    
    data.addListener (this);
}

void GraphEditorComponent::setVerticalLayout (const bool isVertical)
{
    if (verticalLayout == isVertical)
        return;
    verticalLayout = isVertical;
    
    if (graph.isValid() && graph.isGraph())
        graph.setProperty ("vertical", verticalLayout);
    
    draggingConnector = nullptr;
    deleteAllChildren();
    updateComponents();
}

void GraphEditorComponent::paint (Graphics& g)
{
   g.fillAll (LookAndFeel::widgetBackgroundColor.darker(0.6));
}

void GraphEditorComponent::mouseDown (const MouseEvent& e)
{
    if (! isEnabled())
        return;

    if (selectedNodes.getNumSelected() > 0) {
        selectedNodes.deselectAll();
        updateSelection();
    }

    if (e.mods.isPopupMenu())
    {
        PluginsPopupMenu menu (this);
        if (graph.isGraph())
        {
            menu.addSectionHeader ("Graph I/O");
            menu.addItem (1, "Audio Inputs",    true, graph.hasAudioInputNode());
            menu.addItem (2, "Audio Outputs",   true, graph.hasAudioOutputNode());
            menu.addItem (3, "MIDI Input",      true, graph.hasMidiInputNode());
            menu.addItem (4, "MIDI Output",     true, graph.hasMidiOutputNode());
            menu.addSeparator();
            
           #ifndef EL_FREE
            PopupMenu submenu;
            addMidiDevicesToMenu (submenu, true, 80000);
            menu.addSubMenu ("MIDI Input Device", submenu);
            submenu.clear();
            addMidiDevicesToMenu (submenu, false, 90000);
            menu.addSubMenu ("MIDI Output Device", submenu);
           #endif
        }
        
        menu.addSeparator();
        menu.addItem (5, "Change orientation...");
        menu.addItem (7, "Gather nodes...");
       #if JUCE_DEBUG
        menu.addItem (100, "Misc testing item...");
       #endif
        menu.addSeparator();
        menu.addItem (6, "Fixed node positions", true, areResizePositionsFrozen());
        menu.addSeparator();
        
        menu.addSectionHeader ("Plugins");
        menu.addPluginItems();
        const int result = menu.show();
        
        if (menu.isPluginResultCode (result))
        {
            bool verified = false;
            if (const auto* desc = menu.getPluginDescription (result, verified))
                ViewHelpers::postMessageFor (this, new AddPluginMessage (graph, *desc, verified));
        }
        else if (result >= 80000 && result < 90000)
        {
            ViewHelpers::postMessageFor (this, 
                new AddMidiDeviceMessage (getMidiDeviceForMenuResult (result, true), true));
        }
        else if (result >= 90000 && result < 100000)
        {
            ViewHelpers::postMessageFor (this, 
                new AddMidiDeviceMessage (getMidiDeviceForMenuResult (result, false, 90000), false));
        }
        else
        {
            PluginDescription desc;
            desc.pluginFormatName = "Internal";
            bool hasRequestedType = false;
            bool failure = false;

            switch (result)
            {
                case 1:
                    desc.fileOrIdentifier = "audio.input";
                    hasRequestedType = graph.hasAudioInputNode();
                    break;
                case 2:
                    desc.fileOrIdentifier = "audio.output";
                    hasRequestedType = graph.hasAudioOutputNode();
                    break;
                case 3:
                    desc.fileOrIdentifier = "midi.input";
                    hasRequestedType = graph.hasMidiInputNode();
                    break;
                case 4:
                    desc.fileOrIdentifier = "midi.output";
                    hasRequestedType = graph.hasMidiOutputNode();
                    break;
                case 5:
                    setVerticalLayout (! isLayoutVertical());
                    return;
                    break;
                case 6:
                    setResizePositionsFrozen (! areResizePositionsFrozen());
                    return;
                    break;
                
                case 7:
                {
                    int numChanges = 0;
                    for (int i = 0; i < graph.getNumNodes(); ++i)
                    {
                        Node node = graph.getNode (i);
                        double x = 0, y = 0;
                        node.getRelativePosition (x, y);
                        bool changed = false;
                        if (x < 0.0)
                        {
                            changed = true;
                            x = 0.05;
                        }
                        else if (x > 1.0)
                        {
                            changed = true;
                            x = 0.95;
                        }

                        if (y < 0.0)
                        {
                            changed = true;
                            y = 0.05;
                        }
                        else if (y > 1.0)
                        {
                            changed = true;
                            y = 0.95;
                        }

                        if (changed)
                        {
                            node.setRelativePosition (x, y);
                            ++numChanges;
                        }
                    }

                    if (numChanges > 0)
                    {
                        updateFilterComponents (true);
                        updateConnectorComponents();
                    }

                    return;
                } break;
                
                case 100:
                {
                    updateFilterComponents (true);
                    updateConnectorComponents();
                    return;
                } break;

                default:
                    failure = true;
                    break;
            }
            
            if (failure)
            {
                DBG("[EL] unkown menu result: " << result);
            }
            else if (hasRequestedType)
            {
                const ValueTree requestedNode = graph.getNodesValueTree()
                  .getChildWithProperty (Tags::identifier, desc.fileOrIdentifier);
                const Node model (requestedNode, false);
                ViewHelpers::postMessageFor (this, new RemoveNodeMessage (model));
            }
            else
            {
                ViewHelpers::postMessageFor (this, new AddPluginMessage (graph, desc));
            }
        }
    }
}

void GraphEditorComponent::createNewPlugin (const PluginDescription* desc, int x, int y)
{
    DBG("[EL] GraphEditorComponent::createNewPlugin(...)");
}

FilterComponent* GraphEditorComponent::getComponentForFilter (const uint32 filterID) const
{
    for (int i = getNumChildComponents(); --i >= 0;)
    {
        if (FilterComponent* const fc = dynamic_cast <FilterComponent*> (getChildComponent (i)))
            if (fc->filterID == filterID)
                return fc;
    }

    return nullptr;
}

ConnectorComponent* GraphEditorComponent::getComponentForConnection (const Arc& arc) const
{
    for (int i = getNumChildComponents(); --i >= 0;)
    {
        if (ConnectorComponent* const c = dynamic_cast <ConnectorComponent*> (getChildComponent (i)))
            if (c->sourceFilterID == arc.sourceNode
                 && c->destFilterID == arc.destNode
                 && c->sourceFilterChannel == arc.sourcePort
                 && c->destFilterChannel == arc.destPort)
                return c;
    }

    return nullptr;
}

PinComponent* GraphEditorComponent::findPinAt (const int x, const int y) const
{
    for (int i = getNumChildComponents(); --i >= 0;)
    {
        if (FilterComponent* fc = dynamic_cast <FilterComponent*> (getChildComponent (i)))
        {
            if (PinComponent* pin = dynamic_cast <PinComponent*> (fc->getComponentAt (x - fc->getX(),
                                                                                      y - fc->getY())))
                return pin;
        }
    }

    return nullptr;
}

void GraphEditorComponent::resized()
{
    updateFilterComponents (! areResizePositionsFrozen());
    updateConnectorComponents();
}

void GraphEditorComponent::changeListenerCallback (ChangeBroadcaster*)
{
    updateComponents();
}

void GraphEditorComponent::onGraphChanged()
{
    updateComponents();
}

void GraphEditorComponent::updateConnectorComponents()
{
    const ValueTree arcs = graph.getArcsValueTree();
    for (int i = getNumChildComponents(); --i >= 0;)
    {
        ConnectorComponent* const cc = dynamic_cast <ConnectorComponent*> (getChildComponent (i));
        if (cc != nullptr && cc != draggingConnector)
        {
            if (! Node::connectionExists (arcs, cc->sourceFilterID, (uint32) cc->sourceFilterChannel, 
                                                cc->destFilterID, (uint32) cc->destFilterChannel,
                                                true))
            {
                delete cc;
            }
            else
            {
                cc->update();
            }
        }
    }
}

void GraphEditorComponent::updateFilterComponents (const bool doPosition)
{
    for (int i = getNumChildComponents(); --i >= 0;)
        if (auto* const fc = dynamic_cast<FilterComponent*> (getChildComponent (i)))
            { fc->update (doPosition); }
}

void GraphEditorComponent::stabilizeNodes()
{
    for (int i = getNumChildComponents(); --i >= 0;)
        if (auto* const fc = dynamic_cast<FilterComponent*> (getChildComponent (i)))
            { fc->update (false); fc->repaint(); }
}

void GraphEditorComponent::updateComponents()
{
    for (int i = graph.getNumConnections(); --i >= 0;)
    {
        const ValueTree c = graph.getConnectionValueTree (i);
        const Arc arc (Node::arcFromValueTree (c));
        ConnectorComponent* connector = getComponentForConnection (arc);
        
        if (connector == nullptr)
        {
            connector = new ConnectorComponent (graph);
            addAndMakeVisible (connector, i);
        }
        
        connector->setGraph (this->graph);
        connector->setInput (arc.sourceNode, arc.sourcePort);
        connector->setOutput (arc.destNode, arc.destPort);
    }
    
    for (int i = graph.getNumNodes(); --i >= 0;)
    {
        const Node node (graph.getNode (i));
        FilterComponent* comp = getComponentForFilter (node.getNodeId());
        if (comp == nullptr)
        {
            comp = new FilterComponent (graph, node, verticalLayout);
            addAndMakeVisible (comp, i + 10000);
        }
    }

    updateFilterComponents (true);
    updateConnectorComponents();
}

void GraphEditorComponent::beginConnectorDrag (const uint32 sourceNode, const int sourceFilterChannel,
                                               const uint32 destNode, const int destFilterChannel,
                                               const MouseEvent& e)
{
    draggingConnector = dynamic_cast <ConnectorComponent*> (e.originalComponent);
    if (draggingConnector == nullptr)
        draggingConnector = new ConnectorComponent (graph);

    draggingConnector->setGraph (this->graph);
    draggingConnector->setInput (sourceNode, sourceFilterChannel);
    draggingConnector->setOutput (destNode, destFilterChannel);
    draggingConnector->setAlwaysOnTop (true);
    addAndMakeVisible (draggingConnector);
    draggingConnector->toFront (false);

    dragConnector (e);
}

void GraphEditorComponent::dragConnector (const MouseEvent& e)
{
    const MouseEvent e2 (e.getEventRelativeTo (this));

    if (draggingConnector != nullptr)
    {
        draggingConnector->setTooltip (String());

        int x = e2.x;
        int y = e2.y;

        if (PinComponent* const pin = findPinAt (x, y))
        {
            uint32 srcFilter = draggingConnector->sourceFilterID;
            int srcChannel   = draggingConnector->sourceFilterChannel;
            uint32 dstFilter = draggingConnector->destFilterID;
            int dstChannel   = draggingConnector->destFilterChannel;

            if (srcFilter == 0 && ! pin->isInput)
            {
                srcFilter = pin->filterID;
                srcChannel = pin->port;
            }
            else if (dstFilter == 0 && pin->isInput)
            {
                dstFilter = pin->filterID;
                dstChannel = pin->port;
            }
            
            if (graph.canConnect (srcFilter, srcChannel, dstFilter, dstChannel))
            {
                x = pin->getParentComponent()->getX() + pin->getX() + pin->getWidth() / 2;
                y = pin->getParentComponent()->getY() + pin->getY() + pin->getHeight() / 2;

                draggingConnector->setTooltip (pin->getTooltip());
            }
        }

        if (draggingConnector->sourceFilterID == 0)
            draggingConnector->dragStart (x, y);
        else
            draggingConnector->dragEnd (x, y);
    }
}

Component* GraphEditorComponent::createContainerForNode (GraphNodePtr node, bool useGenericEditor)
{
    if (AudioProcessorEditor* ed = createEditorForNode (node, useGenericEditor))
        if (Component* comp = wrapAudioProcessorEditor (ed, node))
            return comp;
    return nullptr;
}

Component* GraphEditorComponent::wrapAudioProcessorEditor(AudioProcessorEditor* ed, GraphNodePtr) { return ed; }

AudioProcessorEditor* GraphEditorComponent::createEditorForNode (GraphNodePtr node, bool useGenericEditor)
{
    std::unique_ptr<AudioProcessorEditor> ui = nullptr;
    
    if (! useGenericEditor)
    {   
        if (auto* proc = node->getAudioProcessor())
            ui.reset (proc->createEditorIfNeeded());
        if (ui == nullptr)
            useGenericEditor = true;
    }
    
    if (useGenericEditor)
        ui.reset (new GenericAudioProcessorEditor (node->getAudioProcessor()));
    
    return (nullptr != ui) ? ui.release() : nullptr;
}

void GraphEditorComponent::endDraggingConnector (const MouseEvent& e)
{
    if (draggingConnector == nullptr)
        return;

    draggingConnector->setTooltip (String());

    const MouseEvent e2 (e.getEventRelativeTo (this));

    uint32 srcFilter = draggingConnector->sourceFilterID;
    int srcChannel   = draggingConnector->sourceFilterChannel;
    uint32 dstFilter = draggingConnector->destFilterID;
    int dstChannel   = draggingConnector->destFilterChannel;

    draggingConnector = nullptr;

    if (PinComponent* const pin = findPinAt (e2.x, e2.y))
    {
        if (srcFilter == 0)
        {
            if (pin->isInput)
                return;

            srcFilter  = pin->filterID;
            srcChannel = pin->port;
        }
        else
        {
            if (! pin->isInput)
                return;

            dstFilter   = pin->filterID;
            dstChannel  = pin->port;
        }
        
        connectPorts (graph, srcFilter, (uint32)srcChannel,
                             dstFilter, (uint32)dstChannel);
    }
}

bool GraphEditorComponent::isInterestedInDragSource (const SourceDetails& details)
{
    if (details.description.toString() == "ccNavConcertinaPanel")
        return true;
    
    if (! details.description.isArray())
        return false;
    
    if (auto* a = details.description.getArray())
    {
        const var type (a->getFirst());
        return type == var ("plugin");
    }
    
    return false;
}

void GraphEditorComponent::itemDropped (const SourceDetails& details)
{
    lastDropX = (float)details.localPosition.x / (float)getWidth();
    lastDropY = (float)details.localPosition.y / (float)getHeight();
    
    if (const auto* a = details.description.getArray())
    {
        auto& plugs (ViewHelpers::getGlobals(this)->getPluginManager());
        
        if (const auto t = plugs.getKnownPlugins().getTypeForIdentifierString(a->getUnchecked(1).toString()))
        {
            ScopedPointer<AddPluginMessage> message = new AddPluginMessage (graph, *t);
            auto& builder (message->builder);
            
            if (ModifierKeys::getCurrentModifiersRealtime().isAltDown())
            {
                const auto audioInputNode = graph.getIONode (PortType::Audio, true);
                const auto midiInputNode  = graph.getIONode (PortType::Midi, true);
                builder.addChannel (audioInputNode, PortType::Audio, 0, 0, false);
                builder.addChannel (audioInputNode, PortType::Audio, 1, 1, false);
                builder.addChannel (midiInputNode,  PortType::Midi,  0, 0, false);
            }
            
            if (ModifierKeys::getCurrentModifiersRealtime().isCommandDown())
            {
                const auto audioOutputNode = graph.getIONode (PortType::Audio, false);
                const auto midiOutNode     = graph.getIONode (PortType::Midi, false);
                builder.addChannel (audioOutputNode, PortType::Audio, 0, 0, true);
                builder.addChannel (audioOutputNode, PortType::Audio, 1, 1, true);
                builder.addChannel (midiOutNode,     PortType::Midi,  0, 0, true);
            }

            postMessage (message.release());
        }
    }
    else if (details.description.toString() == "ccNavConcertinaPanel")
    {
        auto* nav = ViewHelpers::getNavigationConcertinaPanel (this);
        if (auto* panel = (nav) ? nav->findPanel<DataPathTreeComponent>() : nullptr)
        {
            File file = panel->getSelectedFile();
            if (file.hasFileExtension ("els"))
            {
               #if defined (EL_PRO)
                postMessage (new OpenSessionMessage (file));
               #endif
            }
            else if (file.hasFileExtension ("elg") ||
                     file.hasFileExtension ("elpreset"))
            {
                const Node node (Node::parse (file));
                bool wasHandled = false;

               #if defined (EL_SOLO) || defined (EL_FREE)
                // SE and LT Should just open graphs instead of trying to embed them
                if (file.hasFileExtension (".elg"))
                {
                    if (auto* cc = ViewHelpers::findContentComponent (this))
                        if (auto* gc = cc->getAppController().findChild<GraphController>())
                            gc->openGraph (file);
                    wasHandled = true;
                }
               #endif

                if (! wasHandled && node.isValid())
                {
                    std::unique_ptr<AddNodeMessage> message (new AddNodeMessage (node, graph, file));
                    
                    auto& builder (message->builder);
                    if (ModifierKeys::getCurrentModifiersRealtime().isAltDown())
                    {
                        const auto audioInputNode = graph.getIONode (PortType::Audio, true);
                        const auto midiInputNode  = graph.getIONode (PortType::Midi, true);
                        builder.addChannel (audioInputNode, PortType::Audio, 0, 0, false);
                        builder.addChannel (audioInputNode, PortType::Audio, 1, 1, false);
                        builder.addChannel (midiInputNode,  PortType::Midi,  0, 0, false);
                    }
                    
                    if (ModifierKeys::getCurrentModifiersRealtime().isCommandDown())
                    {
                        const auto audioOutputNode = graph.getIONode (PortType::Audio, false);
                        const auto midiOutNode     = graph.getIONode (PortType::Midi, false);
                        builder.addChannel (audioOutputNode, PortType::Audio, 0, 0, true);
                        builder.addChannel (audioOutputNode, PortType::Audio, 1, 1, true);
                        builder.addChannel (midiOutNode,     PortType::Midi,  0, 0, true);
                    }
                    postMessage (message.release());
                }
            }
        }
    }
}

void GraphEditorComponent::valueTreeChildAdded (ValueTree& parent, ValueTree& child)
{
    if (child.hasType (Tags::node))
    {
        child.setProperty ("relativeX", verticalLayout ? lastDropX : lastDropY, 0);
        child.setProperty ("relativeY", verticalLayout ? lastDropY : lastDropX, 0);
        auto* comp = new FilterComponent (graph, Node (child, false), verticalLayout);
        addAndMakeVisible (comp, 20000);
        comp->update();
    }
    else if (child.hasType (Tags::arc) || child.hasType (Tags::nodes) ||
             child.hasType (Tags::arcs))
    {
        updateComponents();
    }
    else if (child.hasType (Tags::ports))
    {
        const Node node (parent, false);
        for (int i = 0; i < getNumChildComponents(); ++i)
            if (auto* const filter = dynamic_cast<FilterComponent*> (getChildComponent (i)))
                filter->update();
        updateConnectorComponents();
    }
}

void GraphEditorComponent::selectNode (const Node& nodeToSelect)
{
    if (ignoreNodeSelected)
        return;
    
    for (int i = 0; i < graph.getNumNodes(); ++i)
    {
        auto node = graph.getNode (i);
        if (node == nodeToSelect)
        {
            selectedNodes.selectOnly (nodeToSelect.getNodeId());
            updateSelection();
            if (auto* cc = ViewHelpers::findContentComponent (this))
            {
                auto* gui = cc->getAppController().findChild<GuiController>();
                if (gui->getSelectedNode() != nodeToSelect)
                    gui->selectNode (nodeToSelect);
            }

            break;
        }
    }
}

void GraphEditorComponent::deleteSelectedNodes()
{
    NodeArray toRemove;
    for (const auto& nodeId : selectedNodes)
    {
        const auto node = graph.getNodeById (nodeId);
        toRemove.add (node);
    }

    ViewHelpers::postMessageFor (this, new RemoveNodeMessage (toRemove));
    selectedNodes.deselectAll();
}

void GraphEditorComponent::updateSelection()
{
    for (int i = getNumChildComponents(); --i >= 0;)
    {
        if (FilterComponent* const fc = dynamic_cast <FilterComponent*> (getChildComponent (i)))
        { 
            fc->repaint(); 
            MessageManager::getInstance()->runDispatchLoopUntil (20);
        }
    }
}

}
