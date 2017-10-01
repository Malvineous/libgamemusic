/**
 * @file  gamemus.cpp
 * @brief Command-line interface to libgamemusic.
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

#include <thread>
#include <mutex>
#include <condition_variable>
#include <boost/program_options.hpp>
#include <camoto/gamemusic.hpp>
#include <camoto/util.hpp>
#include <camoto/stream_file.hpp>
#include <camoto/iostream_helpers.hpp>
#include <stdlib.h>
#include <iostream>
#include <fstream>
#include <queue>
#ifndef _MSC_VER
#include <config.h>
#endif
#ifdef USE_PORTAUDIO
// Must be after config.h
#include <portaudio.h>
#endif
#include "common-attributes.hpp"

namespace po = boost::program_options;
namespace gm = camoto::gamemusic;
using namespace camoto;

#define PROGNAME "gamemus"

/// Number of audio frames to generate at one time
#define FRAMES_TO_BUFFER 512

/// Number of channels in audio output
#define NUM_CHANNELS 2

/*** Return values ***/
// All is good
#define RET_OK                 0
// Bad arguments (missing/invalid parameters)
#define RET_BADARGS            1
// Major error (couldn't open file, etc.)
#define RET_SHOWSTOPPER        2
// More info needed (-t auto didn't work, specify a type)
#define RET_BE_MORE_SPECIFIC   3
// One or more files failed, probably user error (file not found, etc.)
#define RET_NONCRITICAL_FAILURE 4
// Some files failed, but not in a common way (cut off write, disk full, etc.)
#define RET_UNCOMMON_FAILURE   5

/// @see libgamearchive/examples/gamearch.cpp
/// find_last_of() changed to find_first_of() here so equal signs can be put
/// into instrument names, e.g. for URLs
bool split(const std::string& in, char delim, std::string *out1, std::string *out2)
{
	std::string::size_type iEqualPos = in.find_first_of(delim);
	*out1 = in.substr(0, iEqualPos);
	// Does the destination have a different filename?
	bool bAltDest = iEqualPos != std::string::npos;
	*out2 = bAltDest ? in.substr(iEqualPos + 1) : *out1;
	return bAltDest;
}

/// Open a music file.
/**
 * @param strFilename
 *   Filename of music or instrument file to open.
 *
 * @param typeArg
 *   Name of command line argument used to specify strType.  Shown to user
 *   when strType is invalid to indicate which option was at fault.
 *
 * @param strType
 *   File type, empty string for autodetect.
 *
 * @param pManager
 *   Manager instance.
 *
 * @param bForceOpen
 *   True to force files to be opened, even if they are not of the indicated
 *   file format.
 *
 * @return Shared pointer to MusicReader instance.
 *
 * @throws int on failure (one of the RET_* values)
 */
int openMusicFile(const std::string& strFilename,
	const char *typeArg, const std::string& strType, bool bForceOpen,
	std::shared_ptr<gm::Music>* pMusicOut,
	gm::MusicManager::handler_t* pMusicTypeOut)
{
	stream::file content(strFilename, false);
	gm::MusicManager::handler_t pMusicType;
	if (strType.empty()) {
		// Need to autodetect the file format.
		for (const auto& i : gm::MusicManager::formats()) {
			gm::MusicType::Certainty cert = i->isInstance(content);
			switch (cert) {

				case gm::MusicType::Certainty::DefinitelyNo:
					// Don't print anything (TODO: Maybe unless verbose?)
					break;

				case gm::MusicType::Certainty::Unsure:
					std::cout << "File could be: " << i->friendlyName()
						<< " [" << i->code() << "]" << std::endl;
					// If we haven't found a match already, use this one
					if (!pMusicType) pMusicType = i;
					break;

				case gm::MusicType::Certainty::PossiblyYes:
					std::cout << "File is likely to be: " << i->friendlyName()
						<< " [" << i->code() << "]" << std::endl;
					// Take this one as it's better than an uncertain match
					pMusicType = i;
					break;

				case gm::MusicType::Certainty::DefinitelyYes:
					std::cout << "File is definitely: " << i->friendlyName()
						<< " [" << i->code() << "]" << std::endl;
					pMusicType = i;
					// Don't bother checking any other formats if we got a 100% match
					goto finishTesting;
			}
			if (cert != gm::MusicType::Certainty::DefinitelyNo) {
				// We got a possible match, see if it requires any suppdata
				auto suppList = i->getRequiredSupps(content, strFilename);
				if (suppList.size() > 0) {
					// It has suppdata, see if it's present
					std::cout << "  * This format requires supplemental files..."
						<< std::endl;
					bool bSuppOK = true;
					for (const auto& s : suppList) {
						try {
							stream::file test_presence(s.second, false);
						} catch (const stream::open_error&) {
							bSuppOK = false;
							std::cout << "  * Could not find/open " << s.second
								<< ", format is probably not " << i->code() << std::endl;
							break;
						}
					}
					if (bSuppOK) {
						// All supp files opened ok
						std::cout << "  * All supp files present, archive is likely "
							<< i->code() << std::endl;
						// Set this as the most likely format
						pMusicType = i;
					}
				}
			}
		}
finishTesting:
		if (!pMusicType) {
			std::cerr << "Unable to automatically determine the file type.  Use "
				"the " << typeArg << " option to manually specify the file format."
				<< std::endl;
			return RET_BE_MORE_SPECIFIC;
		}
	} else {
		auto pTestType = gm::MusicManager::byCode(strType);
		if (!pTestType) {
			std::cerr << "Unknown file type given to " << typeArg << ": " << strType
				<< std::endl;
			return RET_BADARGS;
		}
		pMusicType = pTestType;
	}

	assert(pMusicType != NULL);

	// Check to see if the file is actually in this format
	if (pMusicType->isInstance(content) == gm::MusicType::Certainty::DefinitelyNo) {
		if (bForceOpen) {
			std::cerr << "Warning: " << strFilename << " is not a "
				<< pMusicType->friendlyName() << ", open forced." << std::endl;
		} else {
			std::cerr << "Invalid format: " << strFilename << " is not a "
				<< pMusicType->friendlyName() << "\n"
				<< "Use the -f option to try anyway." << std::endl;
			return RET_BE_MORE_SPECIFIC;
		}
	}

	// See if the format requires any supplemental files
	auto suppList = pMusicType->getRequiredSupps(content, strFilename);
	camoto::SuppData suppData;
	for (const auto& s : suppList) {
		try {
			std::cout << "Opening supplemental file " << s.second << std::endl;
			auto suppStream = std::make_unique<stream::file>(s.second, false);
			suppData[s.first] = std::move(suppStream);
		} catch (const stream::open_error& e) {
			std::cerr << "Error opening supplemental file " << s.second
				<< ": " << e.what() << std::endl;
			return RET_SHOWSTOPPER;
		}
	}

	// Open the file
	try {
		*pMusicOut = pMusicType->read(content, suppData);
		assert(*pMusicOut);
	} catch (const camoto::error& e) {
		std::cerr << "Error opening music file: " << e.what() << std::endl;
		return RET_SHOWSTOPPER;
	}

	*pMusicTypeOut = pMusicType;
	return RET_OK;
}

// Event handler used to replace tempo events
class ReplaceTempo: virtual public gm::EventHandler
{
	public:
		ReplaceTempo(const gm::Tempo& newTempo)
			: newTempo(newTempo)
		{
		}

		virtual void endOfTrack(unsigned long delay) { };
		virtual void endOfPattern(unsigned long delay) { };

		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const gm::TempoEvent *ev)
		{
			// Dodgy hack
			gm::TempoEvent *ev2 = const_cast<gm::TempoEvent *>(ev);
			ev2->tempo = this->newTempo;
			std::cout << "Replaced tempo event" << std::endl;
			return true;
		}

		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const gm::NoteOnEvent *ev) { return true; };

		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const gm::NoteOffEvent *ev) { return true; };

		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const gm::EffectEvent *ev) { return true; };

		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const gm::GotoEvent *ev) { return true; };

		virtual bool handleEvent(unsigned long delay, unsigned int trackIndex,
			unsigned int patternIndex, const gm::ConfigurationEvent *ev) { return true; };

	private:
		gm::Tempo newTempo;
};


#ifdef USE_PORTAUDIO
struct PositionHistory {
	PaTime time;
	gm::Playback::Position pos;
};

struct PBCallback
{
	gm::Playback *playback;
	PositionHistory position;
	gm::Playback::Position lastPos;
	PaTime waitUntil; // stream time the main thread is waiting for
	std::mutex mut;
	std::condition_variable wait;
};

static int paCallback(const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer, const PaStreamCallbackTimeInfo *timeInfo,
	PaStreamCallbackFlags statusFlags, void *userData)
{
	PBCallback *pbcb = (PBCallback *)userData;
	memset(outputBuffer, 0, framesPerBuffer * NUM_CHANNELS * 2);
	{
		std::lock_guard<std::mutex> lock(pbcb->mut);
		pbcb->playback->mix((int16_t *)outputBuffer, framesPerBuffer * 2, &pbcb->position.pos);
		pbcb->position.time = timeInfo->outputBufferDacTime;
	}
	if (
		(pbcb->position.pos != pbcb->lastPos) ||
		(pbcb->waitUntil <= timeInfo->currentTime) ||
		(pbcb->position.pos.end)
	) {
		pbcb->lastPos = pbcb->position.pos;

		// Notify anyone waiting that the position has updated
		pbcb->wait.notify_all();
	}
	return paContinue;
}
#endif // USE_PORTAUDIO

/// Play the given song.
/**
 * @param music
 *   Song to play.
 *
 * @param bankMIDI
 *   Patch bank to use for MIDI notes.
 *
 * @param loopCount
 *   Number of times to play the song.  1=once, 2=twice (loop once), 0=loop
 *   forever.  If 0, this function never returns.  Note these values are
 *   different to those given to the --loop parameter.
 *
 * @param extraTime
 *   Number of seconds to linger after song finishes, to let notes fade out.
 */
int play(std::shared_ptr<gm::Music> music,
	std::shared_ptr<const gm::PatchBank> bankMIDI, unsigned int loopCount,
	unsigned int extraTime)
{
#ifdef USE_PORTAUDIO
	// Init PA
	PaError err = Pa_Initialize();
	if (err != paNoError) {
		std::cerr << "Unable to initialise PortAudio.  Pa_Initialize() failed: "
			<< Pa_GetErrorText(err) << std::endl;
		return RET_SHOWSTOPPER;
	}

	// Open audio device
	PaStreamParameters op;
	op.device = Pa_GetDefaultOutputDevice();
	if (op.device == paNoDevice) {
		std::cerr << "No available audio devices.  Pa_GetDefaultOutputDevice() "
			"returned paNoDevice." << std::endl;
		Pa_Terminate();
		return RET_SHOWSTOPPER;
	}
	op.channelCount = NUM_CHANNELS;
	op.sampleFormat = paInt16; // paFloat32
	op.suggestedLatency = 1;//Pa_GetDeviceInfo(op.device)->defaultLowOutputLatency;
	op.hostApiSpecificStreamInfo = NULL;

	unsigned int sampleRate = 48000;
	gm::Playback playback(sampleRate, NUM_CHANNELS, 16);
	playback.setBankMIDI(bankMIDI);
	playback.setSong(music);
	playback.setLoopCount(loopCount);
	PBCallback pbcb;
	pbcb.playback = &playback;
	pbcb.position.pos.end = false;

	PaStream *stream;
	err = Pa_OpenStream(&stream, NULL, &op, sampleRate,
		paFramesPerBufferUnspecified, paClipOff, paCallback, &pbcb);
	if (err != paNoError) {
		std::cerr << "Unable to open audio stream.  Pa_OpenStream() failed: "
			<< Pa_GetErrorText(err) << std::endl;
		Pa_Terminate();
		return RET_SHOWSTOPPER;
	}

	err = Pa_StartStream(stream);
	pbcb.waitUntil = 0;
	if (err != paNoError) {
		std::cerr << "Unable to start audio stream.  Pa_StartStream() failed: "
			<< Pa_GetErrorText(err) << std::endl;
		Pa_CloseStream(stream);
		Pa_Terminate();
		return RET_SHOWSTOPPER;
	}
	pbcb.waitUntil = Pa_GetStreamTime(stream);

	const PaStreamInfo *info = Pa_GetStreamInfo(stream);
	if (!info) {
		std::cerr << "Unable to get stream parameters.  Pa_GetStreamInfo() failed."
			<< std::endl;
		Pa_CloseStream(stream);
		Pa_Terminate();
		return RET_SHOWSTOPPER;
	}
	std::cout << "Adjusting for output latency: " << info->outputLatency << " sec\n";

	auto msTotal = playback.getLength();
	auto min = msTotal / 60000;
	auto sec = (msTotal % 60000) / 1000;
	auto ms = (msTotal % 1000) / 100;
	std::cout << "Calculated song length: " << min << ":" << std::setw(2)
		<< std::setfill('0') << sec << "." << ms << " (" << msTotal << " ms)"
		<< std::endl;

	std::queue<PositionHistory> queuePos;
	gm::Playback::Position lastPos;
	gm::Playback::Position audiblePos, lastAudiblePos;
	audiblePos.end = false;
	PaTime lastTime = 0;
	{
		std::unique_lock<std::mutex> lock(pbcb.mut);
		while (!audiblePos.end) {
			//if (!pbcb.position.pos.end)
			pbcb.wait.wait(lock);

			// The values returned by PortAudio don't include the output latency
			// so we have to add that now to make this position change slightly
			// further in the future.
			// 2015-11-26: Apparently this is no longer needed?
			//pbcb.position.time += info->outputLatency;

			lastTime = pbcb.position.time;

			// See if the position has changed
			if (pbcb.position.pos != lastPos) {
				// It has, so store it and the time
				queuePos.push(pbcb.position);
				lastPos = pbcb.position.pos;
			}

			// Check to see whether the oldest pos has played yet
			while (!queuePos.empty()) {
				PositionHistory& nextH = queuePos.front();
				if (nextH.time <= Pa_GetStreamTime(stream)) {
					// This pos has been played now
					audiblePos = nextH.pos;
					queuePos.pop();
				} else {
					// The next event hasn't happened yet, leave it for later
					pbcb.waitUntil = nextH.time;
					break;
				}
			}

			if (audiblePos != lastAudiblePos) {
				// The position being played out of the speakers has just changed
				long pattern = -1;
				if (audiblePos.order < music->patternOrder.size()) {
					pattern = music->patternOrder[audiblePos.order];
				}
				unsigned int beat = (audiblePos.row / audiblePos.tempo.ticksPerBeat)
					% audiblePos.tempo.beatsPerBar;

				std::cout
					<< std::setfill(' ')
					<< "Loop: " << audiblePos.loop
					<< " Order: " << std::setw(2) << audiblePos.order
					<< " Pattern: " << std::setw(2) << pattern
					<< " Row: " << std::setw(2) << audiblePos.row
					<< " Beat: " << beat
					<< "    \r" << std::flush;
				lastAudiblePos = audiblePos;
			}
		}

		if (extraTime) {
			PaTime endTime = lastTime + extraTime;
			do {
				pbcb.wait.wait(lock);
			} while (pbcb.position.time < endTime);
		}
	}
	std::cout << "\n";

	Pa_StopStream(stream);
	Pa_CloseStream(stream);
	Pa_Terminate();
	return RET_OK;
#else
	std::cerr << "PortAudio was not available at compile time, playback is "
		"unavailable." << std::endl;
	return RET_BADARGS;
#endif
}

/// Render the given song to a .wav file.
/**
 * @param wavFilename
 *   Filename and optional path of the output file.
 *
 * @param music
 *   Song to play.
 *
 * @param bankMIDI
 *   Patch bank to use for MIDI notes.
 *
 * @param loopCount
 *   Number of times to play the song.  1=once, 2=twice (loop once), 0=loop
 *   forever.  If 0, this function never returns.  Note these values are
 *   different to those given to the --loop parameter.
 *
 * @param extraTime
 *   Number of seconds to linger after song finishes, to let notes fade out.
 */
int render(stream::output& wav, std::shared_ptr<gm::Music> music,
	std::shared_ptr<gm::PatchBank> bankMIDI, unsigned int loopCount,
	unsigned int extraTime)
{
	if (loopCount == 0) {
		std::cerr << "Can't loop forever when writing to .wav or you will run out "
			"of disk space!" << std::endl;
		return RET_BADARGS;
	}

	unsigned int numChannels = NUM_CHANNELS;
	unsigned int bitDepth = 16;
	unsigned int sampleRate = 48000;
	gm::Playback playback(sampleRate, numChannels, bitDepth);
	playback.setBankMIDI(bankMIDI);
	playback.setSong(music);
	playback.setLoopCount(loopCount);

	// Make room for the header, will rewrite later
#define WAVE_FMT_SIZE (2+2+4+4+2+2)
#define WAVE_HEADER_SIZE (4+4+4+4+4+WAVE_FMT_SIZE+4+4)
	wav
		<< "RIFF"
		<< u32le(0) // overwritten later
		<< "WAVEfmt "
		<< u32le(WAVE_FMT_SIZE)
		<< u16le(1) // PCM
		<< u16le(numChannels)
		<< u32le(sampleRate)
		<< u32le(sampleRate * numChannels * bitDepth / 8)
		<< u16le(numChannels * bitDepth / 8)
		<< u16le(bitDepth)
		<< "data"
		<< u32le(0) // overwritten later
	;

	const unsigned long lenBuffer = FRAMES_TO_BUFFER * NUM_CHANNELS;
	std::vector<int16_t> output(lenBuffer);
	const unsigned long lenBufferBytes = lenBuffer * 2;

	std::cout << "Writing WAV at " << sampleRate << "Hz, " << bitDepth << "-bit, "
		<< (numChannels == 1 ? "mono" : "stereo")
		<< "\nExtra time: " << extraTime << " seconds, loop: ";
	if (loopCount == 1) std::cout << "off"; else std::cout << loopCount - 1;
	std::cout << std::endl;

	gm::Playback::Position pos, lastPos;
	lastPos.end = true;
	pos.end = false;
	unsigned int numOrders = music->patternOrder.size();
	while (!pos.end) {
		memset(output.data(), 0, lenBufferBytes);
		playback.mix(output.data(), lenBuffer, &pos);

		// Make sure samples are little-endian
		for (unsigned int i = 0; i < lenBuffer; i++) output[i] = htole16(output[i]);

		wav.write((uint8_t *)output.data(), lenBuffer * sizeof(int16_t));

		if (pos != lastPos) {
			long pattern = -1;
			if (pos.order < numOrders) {
				pattern = music->patternOrder[pos.order];
			}

			unsigned int loopLength = numOrders - (music->loopDest == -1 ? 0 : music->loopDest);
			unsigned int progress = (
					(pos.loop * loopLength + pos.order)
					* music->ticksPerTrack + pos.row
				) * 100
				/ (((loopCount - 1) * loopLength + numOrders) * music->ticksPerTrack);

			std::cout
				<< std::setfill(' ')
				<< "Loop: " << pos.loop
				<< " Order: " << std::setw(2) << pos.order
				<< " Pattern: " << std::setw(2) << pattern
				<< " Row: " << std::setw(2) << pos.row
				<< " Progress: " << progress
				<< "%    \r" << std::flush;
			lastPos = pos;
		}
	}

	if (extraTime) {
		unsigned long extraSamples = extraTime * sampleRate * numChannels;
		while (extraSamples >= lenBuffer) {
			memset(output.data(), 0, lenBufferBytes);
			playback.mix(output.data(), lenBuffer, &pos);
			extraSamples -= lenBuffer;

			// Make sure samples are little-endian
			for (unsigned int i = 0; i < lenBuffer; i++) output[i] = htole16(output[i]);

			wav.write((uint8_t *)output.data(), lenBuffer * sizeof(int16_t));
		}
	}
	std::cout << "\n";

	stream::pos lenTotal = wav.tellp();

	wav.seekp(4, stream::start);
	wav << u32le(lenTotal - 8);
	wav.seekp(WAVE_HEADER_SIZE - 4, stream::start);
	wav << u32le(lenTotal - WAVE_HEADER_SIZE);
	wav.flush();

	return RET_OK;
}

/// Convert the track info struct into human-readable text
std::string getTrackChannelText(const gm::TrackInfo& ti)
{
	std::string text;
	switch (ti.channelType) {
		case gm::TrackInfo::ChannelType::Unused:
			text = "Unused";
			break;
		case gm::TrackInfo::ChannelType::Any:
			text = "Any";
			break;
		case gm::TrackInfo::ChannelType::OPL:
			text = createString("OPL " << ti.channelIndex << " [chan " << ti.channelIndex % 9 << " @ chip " << ti.channelIndex / 9 << "]");
			break;
		case gm::TrackInfo::ChannelType::OPLPerc:
			text = "OPL percussive ";
			switch (ti.channelIndex) {
				case 4: text += "bass drum"; break;
				case 3: text += "snare"; break;
				case 2: text += "tomtom"; break;
				case 1: text += "top cymbal"; break;
				case 0: text += "hi-hat"; break;
				default:
					text += createString("- channelIndex " << ti.channelIndex
						<< " out of range!");
					break;
			}
			break;
		case gm::TrackInfo::ChannelType::MIDI:
			text = createString("MIDI " << ti.channelIndex);
			break;
		case gm::TrackInfo::ChannelType::PCM:
			text = createString("PCM " << ti.channelIndex);
			break;
	}
	return text;
}

int main(int iArgC, char *cArgV[])
{
#ifdef __GLIBCXX__
	// Set a better exception handler
	std::set_terminate(__gnu_cxx::__verbose_terminate_handler);
#endif

	// Disable stdin/printf/etc. sync for a speed boost
	std::ios_base::sync_with_stdio(false);

	// Declare the supported options.
	po::options_description poActions("Actions");
	poActions.add_options()
		("list-events,v",
			"list all events in the song")

		("list-instruments,i",
			"list channel map and all instruments in the song")

		("metadata,m",
			"list metadata tags (title, etc.)")

		("remap-tracks,k", po::value<std::string>(),
			"change the target channel for each track")

		("convert,c", po::value<std::string>(),
			"convert the song to another file format")

		("start-at", po::value<int>(),
			"drop notes from the start until this number of ticks")

		("stop-at", po::value<int>(),
			"drop all events after this number of ticks")

		("newinst,n", po::value<std::string>(),
			"override the instrument bank used by subsequent conversions (-c)")

		("rename-instrument,e", po::value<std::string>(),
			"rename the given instrument (-e index=name)")

		("play,p",
			"play the song on the default audio device")

		("tempo", po::value<std::string>(),
			"change the speed of the song")

		("wav,w", po::value<std::string>(),
			"render the song to a .wav file with the given filename")

		("repeat-instruments,r", po::value<int>(),
			"repeat the instrument bank until there are this many valid instruments")

		("tag-title", po::value<std::string>(),
			"set the title tag for the next output file, blank to remove tag")

		("tag-artist", po::value<std::string>(),
			"set the artist tag for the next output file, blank to remove tag")

		("tag-comment", po::value<std::string>(),
			"set the comment tag for the next output file, blank to remove tag")
	;

	po::options_description poOptions("Options");
	poOptions.add_options()
		("type,t", po::value<std::string>(),
			"specify the music file format (default is autodetect)")
		("script,s",
			"format output suitable for script parsing")
		("force,f",
			"force open even if the file is not in the given format")
		("list-types",
			"list available input/output file formats")
		("no-pitchbends,o",
			"don't use pitchbends with -c")
		("force-opl3,3",
			"force OPL3 mode (18 channels) with -c")
		("force-opl2,2",
			"force OPL2 mode (11 channels) with -c")
		("loop,l", po::value<int>(),
			"repeat the song (-1=loop forever, 0=no loop, 1=loop once) [default=1]")
		("extra-time,x", po::value<int>(),
			"number of seconds to linger after song finishes to allow notes to fade "
			"out [default=2]")
		("midibank,b", po::value<std::string>(),
			"patch bank to use for MIDI instruments with --play and --wav "
			"[default=none, MIDI is silent]")
	;

	po::options_description poHidden("Hidden parameters");
	poHidden.add_options()
		("music", "music file to manipulate")
		("help", "produce help message")
	;

	po::options_description poVisible("");
	poVisible.add(poActions).add(poOptions);

	po::options_description poComplete("Parameters");
	poComplete.add(poActions).add(poOptions).add(poHidden);
	po::variables_map mpArgs;

	std::string strFilename;
	std::string strType;

	bool bScript = false; // show output suitable for script parsing?
	bool bForceOpen = false; // open anyway even if not in given format?
	bool bUsePitchbends = true; // pass pitchbend events with -c
	bool bForceOPL2 = false; // force OPL2 mode?
	bool bForceOPL3 = false; // force OPL3 mode?
	int userLoop = 1; // repeat once by default
	int extraTime = 2; // two seconds extra by default
	std::shared_ptr<gm::PatchBank> bankMIDI; // instruments to use for MIDI notes
	try {
		po::parsed_options pa = po::parse_command_line(iArgC, cArgV, poComplete);

		// Parse the global command line options
		for (std::vector<po::option>::iterator i = pa.options.begin(); i != pa.options.end(); i++) {
			if (i->string_key.empty()) {
				// If we've already got a filename, complain that a second one
				// was given (probably a typo.)
				if (!strFilename.empty()) {
					std::cerr << "Error: unexpected extra parameter (multiple "
						"filenames given?!)" << std::endl;
					return RET_BADARGS;
				}
				assert(i->value.size() > 0);  // can't have no values with no name!
				strFilename = i->value[0];
			} else if (i->string_key.compare("help") == 0) {
				std::cout <<
					"Copyright (C) 2010-2015 Adam Nielsen <malvineous@shikadi.net>\n"
					"This program comes with ABSOLUTELY NO WARRANTY.  This is free software,\n"
					"and you are welcome to change and redistribute it under certain conditions;\n"
					"see <http://www.gnu.org/licenses/> for details.\n"
					"\n"
					"Utility to manipulate music files used by games.\n"
					"Build date " __DATE__ " " __TIME__ << "\n"
					"\n"
					"Usage: gamemus [options] <infile> [actions...]\n" << poVisible << "\n"
					"--convert requires a filetype prefix, e.g. raw-rdos:out.raw.  Also works\n"
					"with --newinst if the file type cannot be autodetected."
					<< std::endl;
				return RET_OK;
			} else if (i->string_key.compare("list-types") == 0) {
				for (const auto& i : gm::MusicManager::formats()) {
					std::string code = i->code();
					std::cout << code;
					int len = code.length();
					if (len < 20) std::cout << std::string(20 - len, ' ');
					std::string desc = i->friendlyName();
					std::cout << ' ' << desc;
					std::vector<std::string> exts = i->fileExtensions();
					if (exts.size() > 0) {
						len = desc.length();
						if (len < 40) std::cout << std::string(40-len, ' ');
						std::cout << " [";
						std::string sep;
						for (std::vector<std::string>::iterator i = exts.begin();
							i != exts.end();
							i++
						) {
							std::cout << sep << "*." << *i;
							if (sep.empty()) sep = "; ";
						}
						std::cout << ']';
					}
					std::cout << '\n';
				}
				return RET_OK;
			} else if (
				(i->string_key.compare("t") == 0) ||
				(i->string_key.compare("type") == 0)
			) {
				if (i->value.size() == 0) {
					std::cerr << PROGNAME ": --type (-t) requires a parameter."
						<< std::endl;
					return RET_BADARGS;
				}
				strType = i->value[0];
			} else if (
				(i->string_key.compare("s") == 0) ||
				(i->string_key.compare("script") == 0)
			) {
				bScript = true;
			} else if (
				(i->string_key.compare("f") == 0) ||
				(i->string_key.compare("force") == 0)
			) {
				bForceOpen = true;
			} else if (
				(i->string_key.compare("o") == 0) ||
				(i->string_key.compare("no-pitchbends") == 0)
			) {
				bUsePitchbends = false;
			} else if (
				(i->string_key.compare("3") == 0) ||
				(i->string_key.compare("force-opl3") == 0)
			) {
				bForceOPL3 = true;
			} else if (
				(i->string_key.compare("2") == 0) ||
				(i->string_key.compare("force-opl2") == 0)
			) {
				bForceOPL2 = true;
			} else if (
				(i->string_key.compare("l") == 0) ||
				(i->string_key.compare("loop") == 0)
			) {
				userLoop = strtod(i->value[0].c_str(), NULL);
			} else if (
				(i->string_key.compare("x") == 0) ||
				(i->string_key.compare("extra-time") == 0)
			) {
				extraTime = strtod(i->value[0].c_str(), NULL);
			} else if (
				(i->string_key.compare("b") == 0) ||
				(i->string_key.compare("midibank") == 0)
			) {
				std::string strInstFile;
				std::string strInstType;
				if (!split(i->value[0], ':', &strInstType, &strInstFile)) {
					strInstType.clear(); // no type given, autodetect
				}

				gm::MusicManager::handler_t pInstType;
				std::shared_ptr<gm::Music> pInst;
				try {
					auto ret = openMusicFile(strInstFile, "-b/--midibank", strInstType,
						bForceOpen, &pInst, &pInstType);
					if (ret != RET_OK) return ret;
				} catch (stream::open_error& e) {
					std::cerr << "Error opening instrument bank " << strFilename
						<< ": " << e.what() << std::endl;
					return RET_SHOWSTOPPER;
				}

				if (!pInst->patches) {
					std::cerr << "MIDI bank given with -b/--midibank has no instruments!"
						<< std::endl;
					return RET_BADARGS;
				}
				bankMIDI = pInst->patches;
			}
		}

		if ((bForceOPL2 == true) && (bForceOPL3 == true)) {
			std::cerr << "Error: can't force OPL2 and OPL3 at the same time!" << std::endl;
			return RET_BADARGS;
		}

		if (strFilename.empty()) {
			std::cerr << "Error: no filename given.  Use --help for help." << std::endl;
			return RET_BADARGS;
		}
		if (!bScript) std::cout << "Opening " << strFilename << " as type "
			<< (strType.empty() ? "<autodetect>" : strType) << std::endl;

		gm::MusicManager::handler_t pMusicType;
		std::shared_ptr<gm::Music> pMusic;
		try {
			int ret = openMusicFile(strFilename, "-t/--type", strType, bForceOpen,
				&pMusic, &pMusicType);
			if (ret != RET_OK) return ret;
		} catch (stream::open_error& e) {
			std::cerr << "Error opening " << strFilename << ": " << e.what()
				<< std::endl;
			return RET_SHOWSTOPPER;
		}

		if (bForceOPL2 || bForceOPL3) {

			unsigned int patternIndex = 0;
			// For each pattern
			for (auto& pp : pMusic->patterns) {
				unsigned int trackIndex = 0;
				// For each track
				for (auto& pt : pp) {
					// For each event in the track
					for (auto ev = pt.begin(); ev != pt.end(); ev++) {
						auto& te = *ev;
						gm::ConfigurationEvent *cev = dynamic_cast<gm::ConfigurationEvent *>(te.event.get());
						if (cev) {
							switch (cev->configType) {
								case gm::ConfigurationEvent::Type::EmptyEvent:
									// Nothing to do
									break;
								case gm::ConfigurationEvent::Type::EnableOPL3:
									// Got an OPL3 mode change event, nullify it.  This is easier
									// than deleting it because we don't have to handle merging
									// the delay in with a following event (or creating a None
									// event anyway if there is no event following this one.)
									cev->value = bForceOPL3 ? 1 : 0;
									break;
								case gm::ConfigurationEvent::Type::EnableDeepTremolo: {
									if (bForceOPL3) {
										// Duplicate this event for the other chip
										gm::TrackEvent te2;
										te2.delay = 0;
										auto ev2 = std::make_shared<gm::ConfigurationEvent>(*cev);
										te2.event = ev2;
										ev2->value |= 2; // "|=2" == chip index 1
										ev = pt.insert(ev+1, te2);
									}
									break;
								}
								case gm::ConfigurationEvent::Type::EnableDeepVibrato: {
									if (bForceOPL3) {
										// Duplicate this event for the other chip
										gm::TrackEvent te2;
										te2.delay = 0;
										auto ev2 = std::make_shared<gm::ConfigurationEvent>(*cev);
										te2.event = ev2;
										ev2->value |= 2; // "|=2" == chip index 1
										ev = pt.insert(ev+1, te2);
									}
									break;
								}
								case gm::ConfigurationEvent::Type::EnableRhythm:
									// Not sure how to deal with this yet
									break;
								case gm::ConfigurationEvent::Type::EnableWaveSel:
									// Always enabled on OPL3, no need to do anything
									break;
							}
						}
					} // end of track
					trackIndex++;
				} // end of pattern
				patternIndex++;
			} // end of song

			// Insert an OPL 2/3 switch event at the start of the first track
			// This isn't perfect (the first pattern may not play first) but it'll
			// do for the moment.
			// @todo: Find first order number and put it in that pattern instead.
			// @todo: But only if that pattern doesn't already have an OPL event at the start
			if (pMusic->patterns.size() > 0) {
				auto& pattern = pMusic->patterns.at(0);
				if (pattern.size() > 0) {
					auto& track = pattern.at(0);

					gm::TrackEvent te2;
					te2.delay = 0;
					auto ev2 = std::make_shared<gm::ConfigurationEvent>();
					te2.event = ev2;
					ev2->configType = gm::ConfigurationEvent::Type::EnableOPL3;
					ev2->value = bForceOPL3 ? 1 : 0;
					track.insert(track.begin(), te2);
				}
			}
		} // if force opl2/3

		// Run through the actions on the command line
		for (auto& i : pa.options) {

			if (i.string_key.compare("list-events") == 0) {
				if (!bScript) {
					std::cout << "Song has " << pMusic->patterns.size() << " patterns\n";
				}
				unsigned int totalEvents = 0;
				unsigned int patternIndex = 0;
				for (auto& pp : pMusic->patterns) {
					unsigned int trackIndex = 0;
					for (auto& pt : pp) {
						if (!bScript) {
							std::cout << ">> Pattern " << patternIndex << ", track "
								<< trackIndex << "\n";
						}
						unsigned int eventIndex = 0;
						unsigned long ticks = 0;
						for (auto ev = pt.begin(); ev != pt.end(); ev++, eventIndex++, totalEvents++) {
							ticks += ev->delay;
							if (bScript) {
								std::cout << "pattern=" << patternIndex << ";track="
									<< trackIndex << ";index=" << eventIndex << ";tick="
									<< ticks << ";";
							} else {
								std::cout << eventIndex << "/" << ticks << ": ";
							}
							std::cout << "delay=" << ev->delay << ";"
								<< ev->event->getContent() << "\n";
						}
						trackIndex++;
					}
					patternIndex++;
				}
				if (!bScript) std::cout << totalEvents << " events listed." << std::endl;

				gm::Playback playback(48000, 2, 16);
				playback.setSong(pMusic);
				playback.setLoopCount(1);
				auto msTotal = playback.getLength();
				auto min = msTotal / 60000;
				auto sec = (msTotal % 60000) / 1000;
				auto ms = (msTotal % 1000) / 100;
				if (bScript) {
					std::cout << "len_min=" << min
						<< "\nlen_sec=" << sec
						<< "\nlen_ms=" << ms
						<< "\nlen_total_ms=" << msTotal
						<< std::endl;
				} else {
					std::cout << "Calculated song length: " << min << ":" << std::setw(2)
						<< std::setfill('0') << sec << "." << ms << " (" << msTotal << " ms)"
						<< std::endl;
				}

			} else if (i.string_key.compare("metadata") == 0) {
				listAttributes(pMusic.get(), bScript);

			} else if (i.string_key.compare("set-metadata") == 0) {
				std::string strIndex, strValue;
				if (!split(i.value[0], '=', &strIndex, &strValue)) {
					std::cerr << PROGNAME ": -e/--set-metadata requires an index and "
						"a value (e.g. --set-metadata 0=example)" << std::endl;
					return RET_BADARGS;
				}
				unsigned int index = strtoul(strIndex.c_str(), nullptr, 0);
				setAttribute(pMusic.get(), bScript, index, strValue);

			} else if (i.string_key.compare("convert") == 0) {
				std::string strOutFile;
				std::string strOutType;
				if (!split(i.value[0], ':', &strOutType, &strOutFile)) {
					std::cerr << "-c/--convert requires a type and a filename, "
						"e.g. -c raw-rdos:out.raw" << std::endl;
					return RET_BADARGS;
				}

				if (!pMusic->patches) {
					std::cerr << "Unable to convert, the source file had no "
						"instruments!  Please supply some with -n." << std::endl;
					return RET_SHOWSTOPPER;
				}

				try {
					stream::output_file contentOut(strOutFile, true);

					auto pMusicOutType = gm::MusicManager::byCode(strOutType);
					if (!pMusicOutType) {
						std::cerr << "Unknown file type given to -c/--convert: " << strOutType
							<< std::endl;
						return RET_BADARGS;
					}

					assert(pMusicOutType != NULL);

					// TODO: figure out whether we need to open any supplemental files
					// (e.g. Vinyl will need to create an external instrument file, whereas
					// Kenslab has one but won't need to use it for this.)
					camoto::SuppData suppData;
					gm::MusicType::WriteFlags flags = gm::MusicType::WriteFlags::Default;

					if (!bUsePitchbends) flags |= gm::MusicType::WriteFlags::IntegerNotesOnly;

					try {
						pMusicOutType->write(contentOut, suppData, *pMusic, flags);
						std::cout << "Wrote " << strOutFile << " as " << strOutType << std::endl;
					} catch (const gm::format_limitation& e) {
						std::cerr << PROGNAME ": Unable to write this song in format "
							<< strOutType << " - " << e.what() << std::endl;
						// Delete empty output file
						contentOut.remove();
						return RET_UNCOMMON_FAILURE;
					}
				} catch (stream::open_error& e) {
					std::cerr << "Error creating " << strOutFile << ": " << e.what()
						<< std::endl;
					return RET_SHOWSTOPPER;
				}

			} else if (i.string_key.compare("list-instruments") == 0) {
				std::cout << "Loop return: ";
				if (pMusic->loopDest == -1) std::cout << "[no loop]\n";
				else std::cout << "Order " << pMusic->loopDest << "\n";
				std::cout << "Channel map:\n";
				unsigned int j = 0;
				for (auto& ti : pMusic->trackInfo) {
					std::cout << "Track " << j << ": " << getTrackChannelText(ti)
						<< " (inst:";

					// Figure out which instruments play on this channel
					std::map<unsigned int, bool> printed;
					for (auto& pp : pMusic->patterns) {
						auto& pt = pp.at(j);
						for (auto& te : pt) {
							const gm::NoteOnEvent *ev = dynamic_cast<const gm::NoteOnEvent*>(te.event.get());
							if (!ev) continue;

							// If we are here, this is a note-on event
							if (printed.find(ev->instrument) == printed.end()) {
								std::cout << ' ' << ev->instrument;
								printed[ev->instrument] = true;
							}
						}
					}
					if (printed.size() == 0) std::cout << " none";
					std::cout << ")\n";
					j++;
				}
				std::cout << std::endl;

				std::cout << "Listing " << pMusic->patches->size() << " instruments:\n";
				j = 0;
				for (auto& i : *pMusic->patches) {
					std::cout << " #" << j << ": ";
					auto oplNext = dynamic_cast<gm::OPLPatch*>(i.get());
					if (oplNext) {
						std::cout << "OPL " << *oplNext;
					} else {
						auto midiNext = dynamic_cast<gm::MIDIPatch*>(i.get());
						if (midiNext) {
							std::cout << "MIDI ";
							if (midiNext->percussion) {
								std::cout << "percussion note " << (int)midiNext->midiPatch;
							} else {
								std::cout << "patch " << (int)midiNext->midiPatch;
							}
						} else {
							auto pcmNext = dynamic_cast<gm::PCMPatch*>(i.get());
							if (pcmNext) {
								std::cout << "PCM " << pcmNext->sampleRate << "/"
									<< (int)pcmNext->bitDepth << "/" << (int)pcmNext->numChannels << " "
									<< (pcmNext->loopEnd ? '+' : '-') << "Loop ";
								if (pcmNext->data.size() < 1024) std::cout << pcmNext->data.size() << "B ";
								else if (pcmNext->data.size() < 1024*1024) std::cout << pcmNext->data.size() / 1024 << "kB ";
								else std::cout << pcmNext->data.size() / 1048576 << "MB ";
							} else {
								std::cout << "--- "; // empty patch
							}
						}
					}
					if (!i->name.empty()) {
						std::cout << " \"" << i->name << '"';
					}
					std::cout << "\n";
					j++;
				}

			} else if (i.string_key.compare("start-at") == 0) {
				if (i.value[0].empty()) {
					std::cerr << "--start-at requires a positive numeric parameter" << std::endl;
					return RET_BADARGS;
				}
				unsigned int target = strtod(i.value[0].c_str(), NULL);
				if (!bScript) {
					std::cout << "Dropping notes before t=" << target << "\n";
				}
				unsigned int count = 0;
				for (auto& pp : pMusic->patterns) {
					for (auto& pt : pp) {
						unsigned long ticks = 0;
						unsigned long ticksCut = 0;
						for (auto ev = pt.begin(); ev != pt.end(); ) {
							ticks += ev->delay;
							if (ticks < target) {
								// This event is in the timeframe we are looking to remove, but
								// we only want to remove note events.
								const gm::NoteOnEvent *ev2 = dynamic_cast<const gm::NoteOnEvent*>(ev->event.get());
								if (!ev2) {
									const gm::NoteOffEvent *ev3 = dynamic_cast<const gm::NoteOffEvent*>(ev->event.get());
									if (!ev3) {
										ev++;
										continue;
									}
								}
								// If we're here, it's either a NoteOnEvent or a NoteOffEvent
								ticksCut += ev->delay;
								ev = pt.erase(ev);
								count++;
							} else {
								// We've removed the last event, set the delay for the next
								// event to match the time we cut.  Without this, each track
								// will begin at a slightly different time, depending on the
								// number of events that were cut.
								unsigned long ticksUncut = target - ticksCut;
								ev->delay -= ticksUncut;
								break;
							}
						}
					}
				}
				pMusic->ticksPerTrack -= target;
				if (bScript) {
					std::cout << "start_at_erased_count=" << count << "\n";
				} else {
					std::cout << "Erased first " << count << " note events\n";
				}

			} else if (i.string_key.compare("stop-at") == 0) {
				if (i.value[0].empty()) {
					std::cerr << "--stop-at requires a positive numeric parameter" << std::endl;
					return RET_BADARGS;
				}
				unsigned int target = strtod(i.value[0].c_str(), NULL);
				if (!bScript) {
					std::cout << "Dropping notes at and after t=" << target << "\n";
				}
				unsigned int count = 0;
				for (auto& pp : pMusic->patterns) {
					for (auto& pt : pp) {
						unsigned long ticks = 0;
						for (auto ev = pt.begin(); ev != pt.end(); ev++) {
							ticks += ev->delay;
							if (ticks >= target) {
								count += pt.end() - ev;
								ev = pt.erase(ev, pt.end());
								break;
							}
						}
					}
				}
				pMusic->ticksPerTrack = target;
				if (bScript) {
					std::cout << "stop_at_erased_count=" << count << "\n";
				} else {
					std::cout << "Erased final " << count << " note events\n";
				}

			} else if (i.string_key.compare("remap-tracks") == 0) {
				if (i.value[0].empty()) {
					std::cerr << "-m/--remap-tracks requires a parameter" << std::endl;
					return RET_BADARGS;
				}
				std::string strTrack, strChan;
				if (!split(i.value[0], '=', &strTrack, &strChan)) {
					std::cerr << "-m/--remap-tracks must be of the form track=channel, "
						"e.g. 4=m0 (to map track 4 to MIDI channel 0)" << std::endl;
					return RET_BADARGS;
				}
				unsigned int track = strtod(strTrack.c_str(), NULL);
				if (pMusic->trackInfo.size() <= track) {
					std::cerr << "-m/--remap-tracks parameter out of range: cannot "
						"change track " << track << " as the last track in the song is track "
						<< pMusic->trackInfo.size() - 1 << std::endl;
					return RET_BADARGS;
				}
				auto& ti = pMusic->trackInfo[track];
				switch (strChan[0]) {
					case 'm':
					case 'M':
						ti.channelType = gm::TrackInfo::ChannelType::MIDI;
						break;
					case 'p':
					case 'P':
						ti.channelType = gm::TrackInfo::ChannelType::PCM;
						break;
					case 'o':
					case 'O':
						ti.channelType = gm::TrackInfo::ChannelType::OPL;
						break;
					case 'r':
					case 'R':
						ti.channelType = gm::TrackInfo::ChannelType::OPLPerc;
						break;
					case '-':
						ti.channelType = gm::TrackInfo::ChannelType::Unused;
						break;
					case '*':
						ti.channelType = gm::TrackInfo::ChannelType::Any;
						break;
					default:
						std::cerr << "Unknown channel type \"" << strChan[0]
							<< "\" passed to -m/--remap-tracks.  Must be one of M, P, O, R, -, *"
							<< std::endl;
						return RET_BADARGS;
				}
				ti.channelIndex = strtod(strChan.c_str() + 1, NULL);
				std::cout << "Mapping track " << track << " to "
					<< getTrackChannelText(ti) << "\n";

			} else if (i.string_key.compare("rename-instrument") == 0) {
				if (i.value[0].empty()) {
					std::cerr << "-e/--rename-instrument requires a parameter" << std::endl;
					return RET_BADARGS;
				}
				std::string strIndex, strName;
				if (!split(i.value[0], '=', &strIndex, &strName)) {
					std::cerr << "-e/--rename-instrument must be of the form index=name, "
						"e.g. 0=test (to rename the first instrument to 'test')"
						<< std::endl;
					return RET_BADARGS;
				}
				unsigned int index = strtod(strIndex.c_str(), NULL);
				std::shared_ptr<gm::Patch> inst;
				if (pMusic->patches->size() < index) {
					std::cerr << "-e/--rename-instrument parameter out of range: cannot "
						"change instrument " << index << " as the last instrument in the "
						"song is number " << pMusic->patches->size() - 1 << std::endl;
					return RET_BADARGS;
				} else if (pMusic->patches->size() == index) {
					// Trying to add one past the end, append a blank
					inst = std::make_shared<gm::Patch>();
					pMusic->patches->push_back(inst);
					std::cout << "Added empty";
				} else {
					inst = pMusic->patches->at(index);
					std::cout << "Renamed";
				}
				inst->name = strName;
				std::cout << " instrument " << index << " as " << inst->name << "\n";

			} else if (i.string_key.compare("newinst") == 0) {
				if (i.value[0].empty()) {
					std::cerr << "-n/--newinst requires filename" << std::endl;
					return RET_BADARGS;
				}
				std::string strInstFile, strInstType;
				if (!split(i.value[0], ':', &strInstType, &strInstFile)) {
					strInstFile = i.value[0];
					strInstType.clear();
				}

				gm::MusicManager::handler_t pInstType;
				std::shared_ptr<gm::Music> pInst;
				try {
					int ret = openMusicFile(strInstFile, "-n/--newinst", strInstType,
						bForceOpen, &pInst, &pInstType);
					if (ret != RET_OK) return ret;
				} catch (stream::open_error& e) {
					std::cerr << "Error opening new instrument file " << strFilename
						<< ": " << e.what() << std::endl;
					return RET_SHOWSTOPPER;
				}

				if (!pInst->patches) {
					std::cerr << "Replacement instrument file given with -n/--newinst "
						"has no instruments" << std::endl;
					return RET_BADARGS;
				}

				unsigned int newCount = pInst->patches->size();
				unsigned int oldCount;
				if (pMusic->patches) {
					oldCount = pMusic->patches->size();
				} else {
					oldCount = 0;
				}
				if (newCount < oldCount) {
					std::cout << "Warning: " << strInstFile << " has less instruments "
						"than the original song! (" << newCount << " vs " << oldCount
						<< ")" << std::endl;
					// Recycle the new instruments until there are the same as there were
					// originally.
					pInst->patches->reserve(oldCount);
					for (unsigned int i = newCount; i < oldCount; i++) {
						unsigned int srcInst = (i - newCount) % newCount;
						std::cout << " > Reusing new instrument #"
							<< srcInst + 1 << " as #" << i + 1 << "/"
							<< oldCount << std::endl;
						// NOTE: This will store the same instrument twice, instead of
						// making a copy - i.e. modifying one will change the other.
						// It's OK here since we won't be futher changing instruments.
						pInst->patches->push_back(pInst->patches->at(srcInst));
					}
				}

				// Repeat the instruments to make sure there are at least the value
				// given to --repeat-instruments.
				pMusic->patches = pInst->patches;

				std::cout << "Loaded replacement instruments from " << strInstFile
					<< std::endl;

			} else if (i.string_key.compare("repeat-instruments") == 0) {
				if (!pMusic->patches) {
					std::cerr << "No instruments available to repeat with "
						"-r/--repeat-instruments" << std::endl;
					return RET_BADARGS;
				}

				unsigned int instrumentRepeat = strtod(i.value[0].c_str(), NULL);

				unsigned int oldCount = pMusic->patches->size();
				pMusic->patches->reserve(instrumentRepeat);
				for (unsigned int i = oldCount; i < instrumentRepeat; i++) {
					unsigned int srcInst = (i - oldCount) % oldCount;
					std::cout << " > Repeating instrument #"
						<< srcInst + 1 << " as #" << i + 1 << "/"
						<< instrumentRepeat << std::endl;
					// NOTE: This will store the same instrument twice, instead of
					// making a copy - i.e. modifying one will change the other.
					// It's OK here since we won't be futher changing instruments.
					pMusic->patches->push_back(pMusic->patches->at(srcInst));
				}

			} else if (i.string_key.compare("play") == 0) {
				if (!pMusic->patches) {
					std::cerr << "This song has no instruments - please specify an "
						"external instrument bank with -n/--newinst before -p/--play"
						<< std::endl;
					return RET_BADARGS;
				}

				int ret = play(pMusic, bankMIDI, userLoop+1, extraTime);
				if (ret != RET_OK) return ret;

			} else if (i.string_key.compare("wav") == 0) {
				if (!pMusic->patches) {
					std::cerr << "This song has no instruments - please specify an "
						"external instrument bank with -n/--newinst before -w/--wav"
						<< std::endl;
					return RET_BADARGS;
				}
				if (i.value[0].empty()) {
					std::cerr << "-w/--wav requires filename" << std::endl;
					return RET_BADARGS;
				}
				std::string wavFilename = i.value[0];

				try {
					stream::output_file wav(wavFilename, true);
					std::cout << "Creating " << wavFilename << "\n";
					int ret = render(wav, pMusic, bankMIDI, userLoop+1, extraTime);
					if (ret != RET_OK) return ret;
				} catch (stream::open_error& e) {
					std::cerr << "Error opening " << wavFilename << ": " << e.what()
						<< std::endl;
					return RET_SHOWSTOPPER;
				}

			} else if (i.string_key.compare("tempo") == 0) {
				bool error = false;
				std::string strUsPerTick, strTime;
				std::string strTicksPerBeat, strTime2;
				std::string strFramesPerTick, strTimeSig;
				std::string strTimeHigh, strTimeLow;
				if (i.value[0].empty()) {
					error = true;
				} else if (!split(i.value[0], ':', &strUsPerTick, &strTime)) {
					error = true;
				} else if (!split(strTime, ',', &strTicksPerBeat, &strTime2)) {
					error = true;
				} else if (!split(strTime2, ',', &strFramesPerTick, &strTimeSig)) {
					error = true;
				} else if (!split(strTimeSig, '/', &strTimeHigh, &strTimeLow)) {
					std::cerr << "--tempo time signature is invalid, expecting a value "
						"of the form 3/4 or similar." << std::endl;
					return RET_BADARGS;
				}
				if (error) {
					std::cerr << "--tempo must be of the form us_per_tick:ticks_per_beat"
						",frames_per_tick,time_signature.  Default is 250000:2,6,4/4 for "
						"120 BPM, two ticks per beat, 6 frames per tick and 4/4 time."
						<< std::endl;
					return RET_BADARGS;
				}

				gm::Tempo newTempo;
				newTempo.beatsPerBar = strtoul(strTimeHigh.c_str(), NULL, 10);
				newTempo.beatLength = strtoul(strTimeLow.c_str(), NULL, 10);
				newTempo.ticksPerBeat = strtoul(strTicksPerBeat.c_str(), NULL, 10);
				newTempo.usPerTick = strtod(strUsPerTick.c_str(), NULL);
				newTempo.framesPerTick = strtoul(strFramesPerTick.c_str(), NULL, 10);

				ReplaceTempo r(newTempo);
				r.handleAllEvents(gm::EventHandler::Pattern_Track_Row, *pMusic, 1);

				pMusic->initialTempo = newTempo;

			// Ignore --type/-t
			} else if (i.string_key.compare("type") == 0) {
			} else if (i.string_key.compare("t") == 0) {
			// Ignore --script/-s
			} else if (i.string_key.compare("script") == 0) {
			} else if (i.string_key.compare("s") == 0) {
			// Ignore --force/-f
			} else if (i.string_key.compare("force") == 0) {
			} else if (i.string_key.compare("f") == 0) {
			}

			// Make sure output doesn't get mixed in with PortAudio messages
			std::cout << std::flush;

		} // for (all command line elements)

	} catch (const po::unknown_option& e) {
		std::cerr << PROGNAME ": " << e.what()
			<< ".  Use --help for help." << std::endl;
		return RET_BADARGS;
	} catch (const po::invalid_command_line_syntax& e) {
		std::cerr << PROGNAME ": " << e.what()
			<< ".  Use --help for help." << std::endl;
		return RET_BADARGS;
	} catch (const stream::error& e) {
		std::cerr << PROGNAME ": I/O error - " << e.what() << std::endl;
		return RET_UNCOMMON_FAILURE;
	} catch (const std::exception& e) {
		std::cerr << PROGNAME ": Unexpected error - " << e.what() << std::endl;
		return RET_UNCOMMON_FAILURE;
	}

	return RET_OK;
}
