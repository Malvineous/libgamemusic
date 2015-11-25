/**
 * @file  camoto/gamemusic/eventconverter-opl.hpp
 * @brief EventConverter implementation that produces OPL data from Events.
 *
 * Copyright (C) 2010-2015 Adam Nielsen <malvineous@shikadi.net>
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

#ifndef _CAMOTO_GAMEMUSIC_EVENTCONVERTER_OPL_HPP_
#define _CAMOTO_GAMEMUSIC_EVENTCONVERTER_OPL_HPP_

#include <camoto/error.hpp>
#include <camoto/enum-ops.hpp>
#include <camoto/gamemusic/events.hpp>
#include <camoto/gamemusic/eventhandler.hpp>
#include <camoto/gamemusic/musictype.hpp>
#include <camoto/gamemusic/patch-opl.hpp>
#include <camoto/gamemusic/tempo.hpp>

namespace camoto {
namespace gamemusic {

/// Flag for delay type
enum class DelayType {
	DelayIsPreData,  ///< The delay should occur before using the register value
	DelayIsPostData, ///< The delay should happen after the reg val is used
};

/// Flags that control the conversion to OPL data.
enum class OPLWriteFlags
{
	/// No special treatment
	Default          = 0x00,

	/// Disable pitchbends
	IntegerNotesOnly = 0x01,

	/// Don't use the first channel (reserve it for e.g. Adlib SFX in a game)
	ReserveFirstChan = 0x02,

	/// Set: OPL2 chip only, unset: supports OPL3/dual OPL2
	OPL2Only         = 0x04,
};

IMPLEMENT_ENUM_OPERATORS(OPLWriteFlags);

CAMOTO_GAMEMUSIC_API OPLWriteFlags toOPLFlags(MusicType::WriteFlags wf);

/// Information about a single OPL reg/val pair.
struct OPLEvent
{
	static const unsigned int Delay = 1; ///< Set if the delay field is valid
	static const unsigned int Regs  = 2; ///< Set if the reg, val and chipIndex fields are valid
	static const unsigned int Tempo = 4; ///< Set if the tempo field has been modified

	/// Which fields are valid from the constants above?
	unsigned int valid;

	/// Number of ticks to delay before the data bytes are actioned.  (Ignored
	/// unless OPLEvent::Delay is set)
	unsigned long delay;

	/// Which OPL chip to use (0 or 1, ignored unless OPLEvent::Regs is set)
	uint8_t chipIndex;

	/// OPL register (ignored unless OPLEvent::Regs is set)
	uint8_t reg;

	/// Value to write to OPL register (ignored unless OPLEvent::Regs is set)
	uint8_t val;

	/// Current/new song tempo
	/**
	 * This value is used when reading and writing OPL data.
	 *
	 * When reading (decoding) OPL data, set the tempo field (and include the
	 * OPLEvent::Tempo flag in \ref valid) if there is a change in tempo,
	 * otherwise ignore this field.
	 *
	 * When writing (encoding) OPL data, this field will always contain the
	 * current song tempo and can be used at any time, even when \ref valid does
	 * not contain the OPLEvent::Tempo flag.  However if the OPLEvent::Tempo flag
	 * is set, then the tempo has changed and this value is of the new tempo.
	 *
	 * The tempo value given always applies to the delay field (if present) in the
	 * same structure instance.
	 *
	 * In other words, if a new tempo is supplied, it is of the highest priority
	 * and is actioned first, before any delay value should be considered.  Delays
	 * will always run at the new tempo.
	 *
	 * This means that special care must be taken when decoding OPL data where a
	 * delay and a tempo change can happen in close proximity.  The new tempo
	 * value may have to be postponed by one event, so that a delay at the old
	 * tempo can be actioned before the new tempo takes effect for subsequent
	 * delays.
	 *
	 * This allows songs that do not support tempo changes to calculate the
	 * correct delay based on the given delay and tempo values combined, without
	 * needing to keep track of the previous tempo value.
	 *
	 * This has no effect on the DelayType for the song.  For both DelayIsPreData
	 * and DelayIsPostData, the given tempo always applies to the delay value in
	 * the same structure.
	 */
	camoto::gamemusic::Tempo tempo;
};

const float OPL_FNUM_DEFAULT = 49716.0;  ///< Most common conversion value
const float OPL_FNUM_ROUND = 50000.0;    ///< Alternate value used occasionally

/// Number of OPL chips/register sets
const unsigned int OPL_NUM_CHIPS = 2;

/// Value returned by getOPLChannel() if there aren't enough channels available
const unsigned int OPL_INVALID_CHIP = OPL_NUM_CHIPS;

/// Maximum number of OPL channels that will ever be used
const unsigned int OPL_MAX_CHANNELS = 18;

/// Maximum number of tracks (melodic + percussive mode)
const unsigned int OPL_TRACK_COUNT = 9 * OPL_NUM_CHIPS + 5;

/// Callback used to do something with the OPL data supplied by oplEncode().
class CAMOTO_GAMEMUSIC_API OPLWriterCallback
{
	public:
		/// Handle the next OPL register/value pair.
		/**
		 * @param oplEvent
		 *   The reg/val pair and associated data to do something with.
		 *
		 * @throw stream:error
		 *   The data could not be processed for some reason.
		 *
		 * @note The delay value in oplEvent is always to occur before the data
		 *   bytes are actioned, i.e. as if DelayIsPreData is always set.
		 */
		virtual void writeNextPair(const OPLEvent *oplEvent) = 0;
};

/// Immediate conversion between incoming events and OPL data.
/**
 * This class is used to convert Event instances into raw OPL data.  It is used
 * for both real time playback of songs and to write them into formats that
 * store raw OPL data.
 *
 * @note Unsupported instruments (i.e. non-OPL instruments, like MIDI) won't
 *   generate any errors.  Instead all notes for those instruments will be
 *   ignored.  This is to facilitate real time playing of a song with
 *   multiple instrument types, without requiring any special code to split
 *   the song up by instrument type.
 *
 * @note This class does no optimisation of the OPL data.  Multiple redundant
 *   writes will occur.  The OPLEncoder class does however perform optimisation.
 */
class CAMOTO_GAMEMUSIC_API EventConverter_OPL: virtual public EventHandler
{
	public:
		/// Set encoding parameters.
		/**
		 * @param cb
		 *   Callback to do something with the OPL data bytes.
		 *
		 * @param music
		 *   Song to convert.  The track info values (channel assignments) and
		 *   patches can be changed during event processing and the changes will be
		 *   reflected in subsequent events.
		 *
		 * @param flags
		 *   One or more OPLWriteFlags to use to control the conversion.
		 *
		 * @param fnumConversion
		 *   Conversion constant to use when converting Hertz into OPL frequency
		 *   numbers.  Can be one of OPL_FNUM_* or a raw value.
		 */
		EventConverter_OPL(OPLWriterCallback *cb,
			std::shared_ptr<const Music> music, double fnumConversion,
			OPLWriteFlags flags);

		/// Destructor.
		virtual ~EventConverter_OPL();

		/// Set the samples to use for playing MIDI instruments.
		/**
		 * @param bankMIDI
		 *   Patch bank to use.  An empty patch bank will mute any MIDI events.
		 *   A supplied patch bank will mute any OPL events.
		 *   The patch bank can contain different instrument types - only PCM
		 *   instruments will be played.
		 *   Entries 0 to 127 inclusive are for GM instruments, entries 128 to 255
		 *   are for percussion (128=note 0, 129=note 1, etc.)
		 */
		void setBankMIDI(std::shared_ptr<const PatchBank> bankMIDI);

		// EventHandler overrides
		virtual void endOfTrack(unsigned long delay);
		virtual void endOfPattern(unsigned long delay);
		virtual void handleAllEvents(EventHandler::EventOrder eventOrder);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const TempoEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOnEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const NoteOffEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const EffectEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const GotoEvent *ev);
		virtual void handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const ConfigurationEvent *ev);

	private:
		OPLWriterCallback *cb;      ///< Callback to handle the generated OPL data
		std::shared_ptr<const Music> music; ///< Song to convert
		double fnumConversion;      ///< Conversion value to use in Hz -> fnum calc
		OPLWriteFlags flags;        ///< One or more OPLWriteFlags
		std::shared_ptr<const PatchBank> bankMIDI; ///< Optional patch bank for MIDI notes

		unsigned long cachedDelay;  ///< Delay to add on to next reg write
		bool oplSet[2][256];        ///< Has this register been set yet?
		uint8_t oplState[2][256];   ///< Current register values
		bool modeOPL3;              ///< Is OPL3/dual OPL2 mode on?
		bool modeRhythm;            ///< Is rhythm mode enabled?

		/// Mapping between track indices and OPL channels
		typedef std::map<unsigned int, int> MIDIChannelMap;
		/// Mapping between track indices and OPL channels
		MIDIChannelMap midiChannelMap;
		/// Update oplState then call handleNextPair()
		/**
		 * @param chipIndex
		 *   0 or 1 for which OPL chip to use.
		 *
		 * @param reg
		 *   OPL register to write to.
		 *
		 * @param val
		 *   Value to write to the OPL register.
		 *
		 * @throw camoto::error
		 *   The data could not be processed.
		 *
		 * @post A delay of value this->cachedDelay is inserted before the event,
		 *   and cachedDelay is set to zero on return;
		 */
		void processNextPair(uint8_t chipIndex, uint8_t reg, uint8_t val);

		/// Write one operator's patch settings (modulator or carrier)
		/**
		 * @param chipIndex
		 *   0 or 1 for which OPL chip to use.
		 *
		 * @param oplChannel
		 *   0-8 inclusive for OPL hardware channel to use.
		 *
		 * @param opNum
		 *   0 for modulator, 1 for carrier.
		 *
		 * @param i
		 *   Patch to write to OPL chip.
		 *
		 * @param velocity
		 *   Velocity value to set.
		 *
		 * @throw camoto::error
		 *   The data could not be processed.
		 */
		void writeOpSettings(int chipIndex, int oplChannel, int opNum,
			const OPLPatch& i, int velocity);

		/// Get the OPL channel to use for the given track.
		/**
		 * @param ti
		 *   Track being used.
		 *
		 * @param trackIndex
		 *   Zero-based index of the track.
		 *
		 * @param oplChannel
		 *   On return, set to the OPL channel to use.
		 *
		 * @param chipIndex
		 *   On return, set to the index of the OPL chip to use.  Can be
		 *   OPL_INVALID_CHIP if there are not enough channels available.
		 *
		 * @param mod
		 *   On return, set to true if this instrument uses the modulator operator.
		 *
		 * @param car
		 *   On return, set to true if this instrument uses the carrier operator.
		 */
		void getOPLChannel(const TrackInfo& ti, unsigned int trackIndex,
			unsigned int *oplChannel, unsigned int *chipIndex, bool *mod, bool *car);

		/// Unmap a channel when no more events are active, freeing it up for later
		/// use.
		void clearOPLChannel(unsigned int trackIndex);
};

} // namespace gamemusic
} // namespace camoto

#endif // _CAMOTO_GAMEMUSIC_EVENTCONVERTER_OPL_HPP_
