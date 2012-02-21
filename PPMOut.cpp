/* ---------------------------------------------------------------------------
** This software is in the public domain, furnished "as is", without technical
** support, and with no warranty, express or implied, as to its usefulness for
** any purpose.
**
** PPMOut.cpp
** Pulse Position Modulation Output functionality
**
** Author: Daniel van den Ouden
** Project: ArduinoRCLib
** Website: http://sourceforge.net/p/arduinorclib/
** -------------------------------------------------------------------------*/

#if defined(ARDUINO) && ARDUINO >= 100
	#include <Arduino.h>
#else
	#include <avr/interrupt.h>
	#include <wiring.h>
#endif

#include <PPMOut.h>


// Interrupt service routine
ISR(TIMER1_COMPA_vect)
{
	rc::PPMOut::handleInterrupt();
}


namespace rc
{

PPMOut* PPMOut::s_instance = 0;


// Public functions

PPMOut::PPMOut(uint8_t         p_channels,
               const uint16_t* p_input,
               uint8_t*        p_work,
               uint8_t         p_maxChannels)
:
m_pulseLength(500),
m_pauseLength(10500),
m_channelCount(p_channels),
m_channels(p_input),
m_channelTimings(reinterpret_cast<uint16_t*>(p_work)),
m_timingCount((p_channels + 1) * 2),
m_timings(const_cast<uint16_t*>(m_channelTimings) + p_maxChannels)
{
	s_instance = this;
}


void PPMOut::start(bool p_a, bool p_invert, bool p_debug)
{
	// stop timer 1
	TCCR1B = 0;
	
	// Fill channelTimings buffer with data from channels buffer
	update();
	
	// Fill timings buffer with data from channelTimings buffer (set up a complete PPM frame)
	updateTimings();
	
	uint8_t pin = p_a ? 9 : 10;
	m_timingPos = p_invert ? 0 : 1;
	
	pinMode(pin, OUTPUT);
	
	// First disable the output compare match A interrupt
	TIMSK1 &= ~(1 << OCIE1A);

	// Configure timer1 Toggle OC1A/OC1B on Compare Match
	TCCR1A = p_a ? (1 << COM1A0) : (1 << COM1B0);
	
	// set compare value
	OCR1A = TCNT1 + m_timings[p_invert ? m_timingCount - 1 : 0];
	
	// enable timer output compare match A interrupts
	TIMSK1 |= (1 << OCIE1A);
	
	// start the timer by setting the speed speed
	if (p_debug == false)
	{
		TCCR1B = (1 << CS11); // set at clk / 8
	}
	else
	{
		TCCR1B = ((1 << CS12) | (1 << CS10)); // set at clk / 1024
	}
}


void PPMOut::setChannelCount(uint8_t p_channels)
{
	m_channelCount = p_channels;
}


uint8_t PPMOut::getChannelCount() const
{
	return m_channelCount;
}


void PPMOut::setPulseLength(uint16_t p_length)
{
	m_pulseLength = p_length << 1;
}


uint16_t PPMOut::getPulseLength() const
{
	return m_pulseLength >> 1;
}


void PPMOut::setPauseLength(uint16_t p_length)
{
	m_pauseLength = p_length << 1;
}


uint16_t PPMOut::getPauseLength() const
{
	return m_pauseLength >> 1;
}


void PPMOut::update()
{
	for (uint8_t i = 0; i < m_channelCount; ++i)
	{
		m_channelTimings[i] = m_channels[i] << 1;
	}
}


void PPMOut::handleInterrupt()
{
	if (s_instance != 0)
	{
		s_instance->isr();
	}
}


// Private functions

void PPMOut::updateTimings()
{
	uint16_t* scratch = m_timings;
	
	// copy all pre-calculated timings
	for (uint8_t i = 0; i < m_channelCount; ++i)
	{
		// set pulse length
		*scratch = m_pulseLength;
		++scratch;
		
		// set timing
		*scratch = m_channelTimings[i] - m_pulseLength;
		++scratch;
	}
	
	// set final pulse length
	*scratch = m_pulseLength;
	++scratch;
	
	// set pause length
	*scratch = m_pauseLength - m_pulseLength;
	
	// update number of timings
	m_timingCount = (m_channelCount + 1) * 2;
}


void PPMOut::isr()
{
	// set the compare register with the next value
	OCR1A += m_timings[m_timingPos];
	
	// update position
	++m_timingPos;
	if (m_timingPos >= m_timingCount)
	{
		m_timingPos = 0;
		
		// we're at the end of frame here, so there's plenty of time to update
		updateTimings();
	}
}

// namespace end
}