/*============================================================================*/
/*
  Copyright (C) 2008 by Vinnie Falco, this file is part of VFLib.
  See the file GNU_GPL_v2.txt for full licensing terms.

  This program is free software; you can redistribute it and/or modify it
  under the terms of the GNU General Public License as published by the Free
  Software Foundation; either version 2 of the License, or (at your option)
  any later version.

  This program is distributed in the hope that it will be useful, but WITHOUT
  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
  FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
  details.

  You should have received a copy of the GNU General Public License along with
  this program; if not, write to the Free Software Foundation, Inc., 51
  Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/
/*============================================================================*/

namespace Model {

Button::Button ()
  : m_checked (false)
{
}

bool Button::isChecked ()
{
  return m_checked;
}

void Button::setChecked (bool checked)
{
  if (m_checked != checked)
  {
    m_checked = checked;
    getListeners().call (&Listener::onModelChecked, this);
  }
}

bool Button::isMomentary ()
{
  return false;
}

void Button::clicked (ModifierKeys const& modifiers)
{
}

void Button::pressed ()
{
}

void Button::released ()
{
}

//------------------------------------------------------------------------------

DummyButton::DummyButton (bool enabled, bool checkButton)
  : m_checkButton (checkButton)
{
  setEnabled (enabled);
}

void DummyButton::clicked (ModifierKeys const& modifiers)
{
  if (m_checkButton)
    setChecked (!isChecked ());
}

}

//------------------------------------------------------------------------------

namespace Facade {

Button::Button ()
  : m_isMouseOverButton (false)
  , m_isButtonDown (false)
  , m_isEnabled (true)
  , m_isChecked (false)
{
}

bool Button::isMouseOverButton () const
{
  return m_isMouseOverButton;
}

bool Button::isButtonDown () const
{
  return m_isButtonDown;
}

bool Button::isEnabled () const
{
  return m_isEnabled;
}

bool Button::isChecked () const
{
  return m_isChecked;
}

void Button::setMouseOverButton (bool isMouseOverButton)
{
  m_isMouseOverButton = isMouseOverButton;
}

void Button::setButtonDown (bool isButtonDown)
{
  m_isButtonDown = isButtonDown;
}

void Button::setEnabled (bool isEnabled)
{
  m_isEnabled = isEnabled;
}

void Button::setChecked (bool isChecked)
{
  m_isChecked = isChecked;
}

}

//------------------------------------------------------------------------------

namespace Control {

Button::StateDetector::StateDetector (Button* owner)
: m_owner (*owner)
, m_wasDown (false)
{
}

void Button::StateDetector::buttonClicked (juce::Button* button)
{
  // ignore
}

void Button::StateDetector::buttonStateChanged (juce::Button* button)
{
  if (button->isDown ())
  {
    if (!m_wasDown)
    {
      m_wasDown = true;
      m_owner.pressed ();
    }
  }
  else if (m_wasDown)
  {
    m_wasDown = false;
    m_owner.released ();
  }
}

//------------------------------------------------------------------------------

Button::Button (Model::Button* model)
: Base (model)
, juce::Button (String::empty)
, m_listener (this)
, m_bEnabledUnboundedMouseMovement (false)
{
  addListener (&m_listener);

  if (model != nullptr)
    setToggleState (model->isChecked (), false);
}

Button::~Button ()
{
}

bool Button::isMomentary ()
{
  bool momentary;

  vf::Model::Button* model = dynamic_cast <vf::Model::Button*> (getModel ());
  
  if (model != nullptr)
    momentary = model->isMomentary ();
  else
    momentary = false;

  return momentary;
}

void Button::clicked (ModifierKeys const& modifiers)
{
  vf::Model::Button* model = dynamic_cast <vf::Model::Button*> (getModel ());

  if (model != nullptr)
  {
    model->clicked (modifiers);
  }
}

void Button::pressed ()
{
  vf::Model::Button* model = dynamic_cast <vf::Model::Button*> (getModel ());

  if (model != nullptr)
  {
    model->pressed ();
  }
}

void Button::released ()
{
  vf::Model::Button* model = dynamic_cast <vf::Model::Button*> (getModel ());

  if (model != nullptr)
  {
    model->released ();
  }
}

void Button::paintButton (
  Graphics& g,
  bool isMouseOverButton,
  bool isButtonDown)
{
  vf::Facade::Button* facade = dynamic_cast <vf::Facade::Button*> (this);

  if (facade != nullptr)
  {
    updateFacade ();

    facade->paint (g, getLocalBounds ());
  }
}

void Button::paintOverChildren (Graphics& g)
{
  vf::Facade::Button* facade = dynamic_cast <vf::Facade::Button*> (this);

  if (facade != nullptr)
  {
    facade->paintOverChildren (g, getLocalBounds ());
  }
}

void Button::updateFacade ()
{
  vf::Facade::Button* facade = dynamic_cast <vf::Facade::Button*> (this);

  if (facade != nullptr)
  {
    facade->setMouseOverButton (isOver ());
    facade->setButtonDown (isDown ());
    facade->setEnabled (isEnabled ());
    facade->setChecked (getToggleState ());
    facade->setConnectedEdgeFlags (getConnectedEdgeFlags ());
  }
}

void Button::enablementChanged ()
{
  if (isEnabled ())
  {
    setAlpha (1.f);
  }
  else
  {
    setAlpha (.5f);
  }
}

void Button::mouseDrag (MouseEvent const& e)
{
  if (Button::isEnabled ())
  {
    if (!m_bEnabledUnboundedMouseMovement && isMomentary ())
    {
      e.source.enableUnboundedMouseMovement (true);

      m_bEnabledUnboundedMouseMovement = true;
    }

    juce::Button::mouseDrag (e);
  }
}

void Button::mouseUp (MouseEvent const& e)
{
  if (Button::isEnabled ())
  {
    m_bEnabledUnboundedMouseMovement = false;

    juce::Button::mouseUp (e);
  }
}

void Button::onModelChecked (Model::Button*)
{
  vf::Model::Button& model = dynamic_cast <vf::Model::Button&> (*getModel ());

  setToggleState (model.isChecked (), false);
}

}
