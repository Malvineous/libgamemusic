/**
 * @file   playback.hpp
 * @brief  Helper class for managing song playback.
 *
 * Copyright (C) 2010-2014 Adam Nielsen <malvineous@shikadi.net>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <camoto/gamemusic/playback.hpp>
#include "dbopl.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

#define OPL_BUFFER_SIZE 512

// Clipping function to prevent integer wraparound after amplification
#define SAMPLE_SIZE 2
#define SAMP_BITS (SAMPLE_SIZE << 3)
#define SAMP_MAX ((1 << (SAMP_BITS-1)) - 1)
#define SAMP_MIN -((1 << (SAMP_BITS-1)))
#define CLIP(v) (((v) > SAMP_MAX) ? SAMP_MAX : (((v) < SAMP_MIN) ? SAMP_MIN : (v)))

Playback::Position::Position()
	:	loop(0),
		order(0),
		row(0),
		end(true)
{
}

bool Playback::Position::operator== (const Playback::Position& b)
{
	return
		(this->loop == b.loop)
		&& (this->order == b.order)
		&& (this->row == b.row)
	;
}


Playback::OPLHandler::OPLHandler(Playback *playback, bool midi)
	:	playback(playback),
		midi(midi)
{
}

void Playback::OPLHandler::writeNextPair(const OPLEvent *oplEvent)
{
	if (oplEvent->valid & OPLEvent::Regs) {
		// Ignore the delay and write it immediately
		SynthOPL& o = this->midi ? this->playback->oplMIDI : this->playback->opl;
		o.write(oplEvent->chipIndex, oplEvent->reg, oplEvent->val);
	}
	if (oplEvent->valid & OPLEvent::Tempo) {
		this->playback->tempoChange(oplEvent->tempo);
	}
	return;
}


Playback::Playback(unsigned long sampleRate, unsigned int channels,
	unsigned int bits)
	:	outputSampleRate(sampleRate),
		outputChannels(channels),
		outputBits(bits),
		loopCount(1),
		frameBufferPos(0),
		pcm(sampleRate, this),
		pcmMIDI(sampleRate, this),
		opl(sampleRate),
		oplMIDI(sampleRate),
		oplHandler(this, false),
		oplHandlerMIDI(this, true)
{
}

Playback::~Playback()
{
}

void Playback::setBankMIDI(PatchBankPtr bankMIDI)
{
	this->bankMIDI = bankMIDI;
	return;
}

void Playback::setSong(ConstMusicPtr music)
{
	this->music = music;
	this->end = false;
	this->loop = 0;
	this->order = 0;
	if (music->patternOrder.size() == 0) {
		this->pattern = 0;
		std::cerr << "Warning: Song has no pattern order numbers!" << std::endl;
	} else {
		this->pattern = music->patternOrder.at(this->order);
	}
	if (music->ticksPerTrack == 0) {
		std::cerr << "Warning: Song's ticksPerTrack is zero!" << std::endl;
	}
	this->row = 0;
	this->frame = 0;

	this->tempoChange(music->initialTempo);

	this->opl.reset();
	this->oplConverter.reset(new EventConverter_OPL(&this->oplHandler, this->music,
		OPL_FNUM_DEFAULT, OPLWriteFlags::Default));

	this->oplMIDI.reset();
	this->oplConvMIDI.reset(new EventConverter_OPL(&this->oplHandlerMIDI, this->music,
		OPL_FNUM_DEFAULT, OPLWriteFlags::Default));
	this->oplConvMIDI->setBankMIDI(this->bankMIDI);

	this->pcm.reset(&this->music->trackInfo, this->music->patches);
	this->pcmMIDI.reset(&this->music->trackInfo, this->music->patches);
	this->pcmMIDI.setBankMIDI(this->bankMIDI);

	// Turn rhythm mode on or off depending on the presence of rhythm tracks
	bool rhythm = false;
	for (TrackInfoVector::const_iterator
		i = music->trackInfo.begin(); i != music->trackInfo.end(); i++
	) {
		if (i->channelType == TrackInfo::OPLPercChannel) {
			rhythm = true;
			break;
		}
	}
	ConfigurationEvent rhythmEvent;
	rhythmEvent.configType = ConfigurationEvent::EnableRhythm;
	rhythmEvent.value = rhythm ? 1 : 0;
	this->oplConverter->handleEvent(0, 0, 0, &rhythmEvent);
	return;
}

void Playback::setLoopCount(unsigned int count)
{
	this->loopCount = count;
	return;
}

void Playback::seekByOrder(unsigned int destOrder)
{
	this->row = 0;
	this->frame = 0;
	this->order = destOrder;
	if (this->music->patternOrder.size() <= this->order) {
		// order points past end of patterns
		this->end = true;
		this->frame = 0;
		return;
	}
	this->pattern = this->music->patternOrder[this->order];
	return;
}

void Playback::seekByTime(unsigned long ms)
{
	/// @todo: Implement this
	return;
}

void Playback::generate(int16_t *output, unsigned long samples, Playback::Position *pos)
{
	assert(this->music);

	while (samples > 0) {
		if (this->frameBufferPos >= this->frameBuffer.size()) {
			this->nextFrame();
		}
		unsigned long left = std::min(samples, this->frameBuffer.size() - this->frameBufferPos);
		memcpy(output, &this->frameBuffer[this->frameBufferPos], left * sizeof(int16_t));
		output += left;
		samples -= left;
		this->frameBufferPos += left;
	}

	// Return the current playback position
	pos->end = this->end;
	pos->loop = this->loop;
	pos->order = this->order;
	pos->row = this->row;
	pos->tempo = this->tempo;
	return;
}

void Playback::nextFrame()
{
	// Trigger the next event
	if (!this->end) {
		if (this->frame == 0) {
			const PatternPtr& pattern = this->music->patterns.at(this->pattern);
			unsigned int trackIndex = 0;
			// For each track
			TrackInfoVector::const_iterator ti = this->music->trackInfo.begin();
			for (Pattern::const_iterator
				pt = pattern->begin(); pt != pattern->end(); pt++, trackIndex++, ti++
			) {
				unsigned int trackPos = 0; // current track position in ticks
				// For each event in the track
				for (Track::const_iterator
					ev = (*pt)->begin(); ev != (*pt)->end(); ev++
				) {
					const TrackEvent& te = *ev;
					trackPos += te.delay;
					// Right at this event
					if (trackPos == this->row) {
						// delay is zero here because we want it to sound immediately (not
						// that is really matters as the delay is ignored later anyway)
						if (
							(ti->channelType == TrackInfo::AnyChannel)
							|| (ti->channelType == TrackInfo::OPLChannel)
							|| (ti->channelType == TrackInfo::OPLPercChannel)
						) {
							te.event->processEvent(0, trackIndex, this->pattern, this->oplConverter.get());
						}
						if (
							(ti->channelType == TrackInfo::AnyChannel)
							|| (ti->channelType == TrackInfo::MIDIChannel)
						) {
							te.event->processEvent(0, trackIndex, this->pattern, this->oplConvMIDI.get());
							te.event->processEvent(0, trackIndex, this->pattern, &this->pcmMIDI);
						}
						if (
							(ti->channelType == TrackInfo::AnyChannel)
							|| (ti->channelType == TrackInfo::PCMChannel)
						) {
							te.event->processEvent(0, trackIndex, this->pattern, &this->pcm);
						}
					} else if (trackPos > this->row) {
						// Not up to this event yet, don't keep looking
						break;
					}
				}
			}
		} else {
			// Update any effects currently in progress
		}
	}

	// Silence the framebuffer
	memset(this->frameBuffer.data(), 0, this->frameBuffer.size() * sizeof(int16_t));

	// Mix the PCM source in to the frame buffer
	this->pcm.mix(this->frameBuffer.data(), this->frameBuffer.size());

	// Mix the OPL source in to the frame buffer
	this->opl.mix(this->frameBuffer.data(), this->frameBuffer.size());

	// Mix the MIDI PCM source in to the frame buffer
	this->pcmMIDI.mix(this->frameBuffer.data(), this->frameBuffer.size());

	// Mix the MIDI OPL source in to the frame buffer
	this->oplMIDI.mix(this->frameBuffer.data(), this->frameBuffer.size());

	// Increment the frame, row, order, etc.
	this->frameBufferPos = 0;
	if (!this->end) {
		this->frame++;
		if (this->frame >= this->tempo.framesPerTick) {
			this->frame = 0;
			this->row++;
			if (this->row >= this->music->ticksPerTrack) {
				this->row = 0;
				this->order++;
				if (this->order >= this->music->patternOrder.size()) {
					if (
						(this->music->loopDest >= 0) &&
						((this->loopCount == 0) || ((unsigned int)this->loop < this->loopCount - 1))
					) {
						this->order = this->music->loopDest;
						this->loop++;
					} else {
						this->end = true;
						this->frame = 0;
						return; // don't update this->pattern
					}
				}
				if (this->music->patternOrder.size() <= this->order) {
					// order points past end of patterns
					this->end = true;
					this->frame = 0;
					return;
				}
				this->pattern = this->music->patternOrder[this->order];
			}
		}
	}

	return;
}

void Playback::tempoChange(const Tempo& tempo)
{
	// Make this thread-safe
	this->tempo = tempo;

	unsigned long samplesPerTick = this->outputSampleRate
		* tempo.usPerTick / Tempo::US_PER_SEC;
	this->samplesPerFrame = samplesPerTick / tempo.framesPerTick;
	this->frameBuffer.assign(this->samplesPerFrame * 2, 0); // *2 == stereo
	this->frameBufferPos = this->frameBuffer.size();
	return;
}
