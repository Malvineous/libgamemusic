/**
 * @file  js/main.cpp
 * @brief Main interface for the libgamemusic asm.js player.
 *
 * Copyright (C) 2010-2017 Adam Nielsen <malvineous@shikadi.net>
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

#include <iostream>
#include <functional>
#include <camoto/stream_string.hpp>
#include <camoto/util.hpp> // make_unique
#include <camoto/gamemusic.hpp>
#include <emscripten.h>
#include <emscripten/bind.h>

using namespace emscripten;
namespace gm = camoto::gamemusic;

/// Dodgy hack to pass a buffer from JS to C++ via the C binding.
/**
 * This is because embind doesn't let us pass pointers to buffers, so we need
 * to use the C bindings to do so.
 */
float *globalBuffer = NULL;
extern "C" {
	void EMSCRIPTEN_KEEPALIVE c_setBuffer(float *out)
	{
		::globalBuffer = out;
	}
}

//typedef std::function<std::string(std::string)> CB_OpenFile;

class JSPlayback
{
	public:
		int16_t buf[8192*2]; ///< Internal buffer for synthesis
		float *outBuffer;    ///< Final buffer accessible from JS

		gm::Playback::Position pos;
		unsigned long msLength; ///< Length of song in ms, available after readfile()
		unsigned long msCurrent; ///< Current playback position, in milliseconds
		std::string lastError; ///< Last error message for UI display

		/// Create a new player.
		/**
		 * @param sampleRate
		 *   Sampling rate, e.g. 44100.
		 *
		 * @param channels
		 *   Channel count, 1 for mono and 2 for stereo.
		 *
		 * @param bitDepth
		 *   Bit depth, probably only 16 works.
		 */
		JSPlayback(unsigned long sampleRate, unsigned int channels,
			unsigned int bitDepth);

		/// Parse the given song content.
		/**
		 * @param data
		 *   Binary data containing the song to play.
		 *
		 * @param type
		 *   File type, or empty to autodetect.  Needed for some formats like IMF
		 *   where the tempo can't be autodetected.
		 *
		 * @param url
		 *   URL to file being played.  Used to generate filenames for suppdata.
		 *
		 *
		 * @return true on success, false if the file is not supported.
		 */
		bool identify(const std::string& data, const std::string& type,
			const std::string& url);

		/// Get a list of supp filenames.
		/**
		 * Returns an associative array of string -> string, with the keys being the
		 * internal type code, and the values being URLs pointing to the required
		 * data (based off the URL supplied to identify().
		 *
		 * The URLs should be downloaded and the data passed to setSupp() using the
		 * same key.
		 *
		 * @return object
		 */
		emscripten::val getSupps();

		/// Set a supplementary data file.
		/**
		 * @param s
		 *   Type of file to set, string version of SuppItem::*.  Use the value
		 *   returned by getSupps() in the array key.
		 *
		 * @param data
		 *   File content for this supplementary file.
		 */
		void setSupp(const std::string& s, const std::string& data);

		/// Continue opening the file.
		/**
		 * This finishes the process started off by identify(), once the suppdata
		 * has been loaded.
		 *
		 * @return true on success, false on failure.  Failure should be rare if
		 *   identify() succeeded.
		 */
		bool open();

		/// Seek to the given playback time.
		void seek(unsigned long msTarget);

		/// Grab the global buffer and set it as the buffer used by this instance.
		void grabBuffer();

		/// Synthesize some samples and fill the buffer
		/**
		 * @param len
		 *   Number of samples to put in the buffer.
		 *
		 * @return Current song time in milliseconds.
		 */
		unsigned long fillBuffer(int len);

		/// Seek to the song's loop point.
		void loop();

	protected:
		gm::Playback playback;
		float sampleRateDivisor; ///< Used to convert samples into milliseconds
		std::shared_ptr<camoto::stream::string> content;
		gm::MusicManager::handler_t musicType;
		std::shared_ptr<gm::Music> music;
		camoto::SuppFilenames suppList;
		camoto::SuppData suppData;
};

JSPlayback::JSPlayback(unsigned long sampleRate, unsigned int channels,
	unsigned int bitDepth)
	: playback(sampleRate, channels, bitDepth),
	  sampleRateDivisor(sampleRate / 1000)
{
}

bool JSPlayback::identify(const std::string& data, const std::string& type,
	const std::string& url)
{
	this->content = std::make_shared<camoto::stream::string>(data);
	if (type.empty()) {
		// Need to autodetect the file format.
		for (const auto& i : gm::MusicManager::formats()) {
			gm::MusicType::Certainty cert = i->isInstance(*content);
			switch (cert) {

				case gm::MusicType::Certainty::DefinitelyNo:
					// Don't print anything (TODO: Maybe unless verbose?)
					break;

				case gm::MusicType::Certainty::Unsure:
					std::cout << "File could be: " << i->friendlyName()
						<< " [" << i->code() << "]" << std::endl;
					// If we haven't found a match already, use this one
					if (!this->musicType) this->musicType = i;
					break;

				case gm::MusicType::Certainty::PossiblyYes:
					std::cout << "File is likely to be: " << i->friendlyName()
						<< " [" << i->code() << "]" << std::endl;
					// Take this one as it's better than an uncertain match
					this->musicType = i;
					break;

				case gm::MusicType::Certainty::DefinitelyYes:
					std::cout << "File is definitely: " << i->friendlyName()
						<< " [" << i->code() << "]" << std::endl;
					this->musicType = i;
					// Don't bother checking any other formats if we got a 100% match
					goto finishTesting;
			}
/*
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
						this->musicType = i;
					}
				}
			}
*/
		}
finishTesting:
		if (!this->musicType) {
			this->lastError = "Unable to automatically determine the file type.";
			std::cerr << this->lastError << std::endl;
			return false;
		}
	} else {
		this->musicType = gm::MusicManager::byCode(type);
		if (!this->musicType) {
			this->lastError = "File type supplied is invalid code: " + type;
			std::cerr << this->lastError << std::endl;
			return false;
		}
	}

	assert(this->musicType != NULL);

	// See if the format requires any supplemental files
	this->suppList = this->musicType->getRequiredSupps(*content, url);

	this->lastError.clear();
	return true;
}

emscripten::val JSPlayback::getSupps()
{
	emscripten::val o = emscripten::val::object();
	for (auto& i : this->suppList) {
		o.set(camoto::suppToString(i.first), i.second);
	}
	return o;
}

void JSPlayback::setSupp(const std::string& s, const std::string& data)
{
	for (int i = 0; i < (int)camoto::SuppItem::MaxValue; i++) {
		camoto::SuppItem si = (camoto::SuppItem)i;
		if (s.compare(camoto::suppToString(si)) == 0) {
			this->suppData[si] = std::make_unique<camoto::stream::string>(data);
			break;
		}
	}
}

bool JSPlayback::open()
{
	try {
		this->music = this->musicType->read(*this->content, this->suppData);
	} catch (const camoto::error& e) {
		this->lastError = std::string("Error opening music file: ") + e.what();
		std::cerr << this->lastError << std::endl;
		return false;
	}

	this->playback.setSong(this->music);

	this->msLength = this->playback.getLength();
	this->msCurrent = 0;

	this->lastError.clear();
	return true;
}

void JSPlayback::seek(unsigned long msTarget)
{
	this->msCurrent = this->playback.seekByTime(msTarget);
}

void JSPlayback::grabBuffer()
{
	this->outBuffer = ::globalBuffer;
}

unsigned long JSPlayback::fillBuffer(int len)
{
	memset(this->buf, 0, len*2*sizeof(int16_t));
	this->playback.mix(this->buf, len*2, &this->pos);
	for (int i = 0, j = 0; i < len; i++, j+= 2) {
		this->outBuffer[i] = this->buf[j] / 32767.0;
		this->outBuffer[len+i] = this->buf[j+1] / 32767.0;
	}
	this->msCurrent += len / this->sampleRateDivisor;
	return this->msCurrent;
}

void JSPlayback::loop()
{
	this->msCurrent = 0; // TODO: Proper loop time
	this->playback.seekByOrder(this->music->loopDest < 0 ? 0 : this->music->loopDest);
	return;
}

EMSCRIPTEN_BINDINGS(main) {
	class_<JSPlayback>("JSPlayback")
		.constructor<unsigned long, unsigned int, unsigned int>()
		.function("identify", &JSPlayback::identify)
		.function("getSupps", &JSPlayback::getSupps)
		.function("setSupp", &JSPlayback::setSupp)
		.function("open", &JSPlayback::open)
		.function("seek", &JSPlayback::seek)
		.function("grabBuffer", &JSPlayback::grabBuffer)
		.function("fillBuffer", &JSPlayback::fillBuffer)
		.function("loop", &JSPlayback::loop)
		.property("pos", &JSPlayback::pos)
		.property("msLength", &JSPlayback::msLength)
		.property("lastError", &JSPlayback::lastError)
	;
	value_object<gm::Playback::Position>("Position")
		.field("loop", &gm::Playback::Position::loop)
		.field("order", &gm::Playback::Position::order)
		.field("row", &gm::Playback::Position::row)
		.field("end", &gm::Playback::Position::end)
	;
}
