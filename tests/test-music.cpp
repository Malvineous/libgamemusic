/**
 * @file   test-music.cpp
 * @brief  Generic test code for MusicType class descendents.
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

#include <iomanip>
#include <iostream>
#include <boost/bind.hpp>
#include <camoto/util.hpp>
#include "test-music.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

void listEvents(const Music& music)
{
	std::cout << "--- Song dump begin ---\nloop_return=";
	if (music.loopDest == -1) std::cout << "none\n";
	else std::cout << "order:" << music.loopDest << "\n";
	unsigned int j = 0;
	for (auto& i : music.trackInfo) {
		std::cout << "track" << j << "=";
		switch (i.channelType) {
			case TrackInfo::ChannelType::Unused:
				std::cout << "unused";
				break;
			case TrackInfo::ChannelType::Any:
				std::cout << "any";
				break;
			case TrackInfo::ChannelType::OPL:
				std::cout << "opl-" << i.channelIndex;
				break;
			case TrackInfo::ChannelType::OPLPerc:
				std::cout << "oplperc-";
				switch (i.channelIndex) {
					case 4: std::cout << "bassdrum"; break;
					case 3: std::cout << "snare"; break;
					case 2: std::cout << "tomtom"; break;
					case 1: std::cout << "topcymbal"; break;
					case 0: std::cout << "hihat"; break;
					default:
						std::cout << "error [channelIndex " << i.channelIndex
							<< " out of range!]";
						break;
				}
				break;
			case TrackInfo::ChannelType::MIDI:
				std::cout << "midi-" << i.channelIndex;
				break;
			case TrackInfo::ChannelType::PCM:
				std::cout << "pcm-" << i.channelIndex;
				break;
		}
		std::cout << "\n";
		j++;
	}

	j = 0;
	for (auto& i : *music.patches) {
		std::cout << "inst" << j << "=";
		auto oplNext = dynamic_cast<OPLPatch*>(i.get());
		if (oplNext) {
			std::cout << "opl:" << oplNext;
		} else {
			auto midiNext = dynamic_cast<MIDIPatch*>(i.get());
			if (midiNext) {
				std::cout << "midi";
				if (midiNext->percussion) {
					std::cout << "perc:" << (int)midiNext->midiPatch;
				} else {
					std::cout << ":" << (int)midiNext->midiPatch;
				}
			} else {
				auto pcmNext = dynamic_cast<PCMPatch*>(i.get());
				if (pcmNext) {
					std::cout << "pcm:" << pcmNext->sampleRate << "/"
						<< (int)pcmNext->bitDepth << "/" << (int)pcmNext->numChannels << " "
						<< (pcmNext->loopEnd ? '+' : '-') << "Loop ";
					if (pcmNext->data.size() < 1024) std::cout << pcmNext->data.size() << "B ";
					else if (pcmNext->data.size() < 1024*1024) std::cout << pcmNext->data.size() / 1024 << "kB ";
					else std::cout << pcmNext->data.size() / 1048576 << "MB ";
				} else {
					std::cout << "empty"; // empty patch
				}
			}
		}
		if (!i->name.empty()) {
			std::cout << ",\"" << i->name << '"';
		}
		std::cout << "\n";
		j++;
	}

	std::cout << "num_patterns=" << music.patterns.size() << "\n";

	unsigned int totalEvents = 0;
	unsigned int patternIndex = 0;
	for (auto& pp : music.patterns) {
		unsigned int trackIndex = 0;
		for (auto& pt : pp) {
			unsigned int eventIndex = 0;
			for (auto& ev : pt) {
				std::cout << "pattern=" << patternIndex << ";track="
					<< trackIndex << ";index=" << eventIndex << ";";
				std::cout << "delay=" << ev.delay << ";"
					<< ev.event->getContent() << "\n";
				eventIndex++;
				totalEvents++;
			}
			trackIndex++;
		}
		patternIndex++;
	}
	std::cout << "total_events=" << totalEvents << "\n--- Song dump complete ---"
		<< std::endl;
	return;
}

/// Check whether a supp item is present and if so that the content is correct.
#define CHECK_SUPP_ITEM(item, check_func, msg) \
	if (this->suppResult[camoto::SuppItem::item]) { \
		BOOST_CHECK_MESSAGE( \
			this->is_supp_equal(camoto::SuppItem::item, \
				this->suppResult[camoto::SuppItem::item]->check_func()), \
			"[SuppItem::" TOSTRING(item) "] " msg \
		); \
	}

test_music::test_music()
	:	init(false),
		numIsInstanceTests(0),
		numInvalidContentTests(1),
		numChangeAttributeTests(1),
		numRewriteTests(1)
{
	this->writeFlags = MusicType::WriteFlags::Default;
	this->numInstruments = -1;
	this->indexInstrumentOPL = -2;
	this->indexInstrumentMIDI = -2;
	this->indexInstrumentPCM = -2;

	this->oplPatch.m.enableTremolo = true;
	this->oplPatch.m.enableVibrato = true;
	this->oplPatch.m.enableSustain = true;
	this->oplPatch.m.enableKSR = true;
	this->oplPatch.m.freqMult = 15;
	this->oplPatch.m.scaleLevel = 3;
	this->oplPatch.m.outputLevel = 63;
	this->oplPatch.m.attackRate = 15;
	this->oplPatch.m.decayRate = 15;
	this->oplPatch.m.sustainRate = 15;
	this->oplPatch.m.releaseRate = 15;
	this->oplPatch.m.waveSelect = 7;

	this->oplPatch.c.enableTremolo = false;
	this->oplPatch.c.enableVibrato = false;
	this->oplPatch.c.enableSustain = false;
	this->oplPatch.c.enableKSR = false;
	this->oplPatch.c.freqMult = 14;
	this->oplPatch.c.scaleLevel = 2;
	this->oplPatch.c.outputLevel = 62;
	this->oplPatch.c.attackRate = 14;
	this->oplPatch.c.decayRate = 14;
	this->oplPatch.c.sustainRate = 14;
	this->oplPatch.c.releaseRate = 14;
	this->oplPatch.c.waveSelect = 6;

	this->oplPatch.feedback = 7;
	this->oplPatch.connection = true;
	this->oplPatch.rhythm = OPLPatch::Rhythm::Melodic;

	this->dumpEvents = false;

	this->writingSupported = true;
}

void test_music::addTests()
{
	ADD_MUSIC_TEST(&test_music::test_test_init);

	ADD_MUSIC_TEST(&test_music::test_isinstance_others);
	ADD_MUSIC_TEST(&test_music::test_read);

	if (this->writingSupported) {
		ADD_MUSIC_TEST(&test_music::test_write);
	} else {
		std::cerr << "WARNING: Format " << this->type
			<< " does not support writing, skipping write test." << std::endl;
	}

	return;
}

void test_music::addBoundTest(boost::function<void()> fnTest,
	boost::unit_test::const_string file, std::size_t line,
	boost::unit_test::const_string name)
{
	this->ts->add(
		boost::unit_test::make_test_case(
			std::bind(&test_music::runTest, this, fnTest),
			createString(name << '[' << this->basename << ']'),
			file, line
		)
	);
	return;
}

void test_music::runTest(boost::function<void()> fnTest)
{
	this->prepareTest();
	fnTest();
	return;
}

void test_music::prepareTest()
{
	this->pType = MusicManager::byCode(this->type);
	BOOST_REQUIRE_MESSAGE(this->pType, "Could not find music type " + this->type);

	// Make this->suppData valid
	this->resetSuppData(false);
	this->populateSuppData();

	this->base.seekp(0, stream::start);
	this->base.data.clear();
	this->base << this->standard();

	// Don't seek back to the start of the data to ensure all format handlers
	// have an initial seek.
	return;
}

void test_music::resetSuppData(bool empty)
{
	this->suppBase.clear();
	for (auto& i : this->suppResult) {
		auto& item = i.first;
		if (!i.second) {
			std::cout << "Warning: " << this->basename << " sets empty "
				<< suppToString(item) << " suppitem, ignoring.\n";
			continue;
		}
		auto suppSS = std::make_shared<stream::string>();
		if (!empty) {
			// Populate the suppitem with its initial state
			*suppSS << i.second->standard();
		}
		this->suppBase[item] = suppSS;
	}
	return;
}

void test_music::populateSuppData()
{
	this->suppData.clear();
	for (auto& i : this->suppBase) {
		auto& item = i.first;
		auto& suppSS = i.second;
		// Wrap this in a substream to get a unique pointer, with an independent
		// seek position.
		this->suppData[item] = stream_wrap(suppSS);
	}
	return;
}

void test_music::isInstance(MusicType::Certainty result,
	const std::string& content)
{
	this->ts->add(
		boost::unit_test::make_test_case(
			std::bind(&test_music::test_isInstance, this, result, content,
				this->numIsInstanceTests),
			createString("test_music[" << this->basename << "]::isinstance_c"
				<< std::setfill('0') << std::setw(2) << this->numIsInstanceTests),
			__FILE__, __LINE__
		)
	);
	this->numIsInstanceTests++;
	return;
}

void test_music::test_isInstance(MusicType::Certainty result,
	const std::string& content, unsigned int testNumber)
{
	BOOST_TEST_MESSAGE(createString("isInstance check (" << this->basename
		<< "; " << std::setfill('0') << std::setw(2) << testNumber << ")"));

	auto pTestType = MusicManager::byCode(this->type);
	BOOST_REQUIRE_MESSAGE(pTestType,
		createString("Could not find music type " << this->type));

	stream::string ss;
	ss << content;

	BOOST_CHECK_EQUAL(pTestType->isInstance(ss), result);
	return;
}

void test_music::invalidContent(const std::string& content)
{
	this->ts->add(
		boost::unit_test::make_test_case(
			std::bind(&test_music::test_invalidContent, this, content,
				this->numInvalidContentTests),
			createString("test_music[" << this->basename << "]::invalidcontent_i"
				<< std::setfill('0') << std::setw(2) << this->numInvalidContentTests),
			__FILE__, __LINE__
		)
	);
	this->numInvalidContentTests++;
	return;
}

void test_music::test_invalidContent(const std::string& content,
	unsigned int testNumber)
{
	BOOST_TEST_MESSAGE(createString("invalidContent check (" << this->basename
		<< "; " << std::setfill('0') << std::setw(2) << testNumber << ")"));

	auto pTestType = MusicManager::byCode(this->type);
	BOOST_REQUIRE_MESSAGE(pTestType,
		createString("Could not find music type " << this->type));

	stream::string ss;
	ss << content;

	// Make sure isInstance reports this is valid
	BOOST_CHECK_EQUAL(pTestType->isInstance(ss), MusicType::Certainty::DefinitelyYes);

	// But that we get an error when trying to open the file
	BOOST_CHECK_THROW(
		auto music = pTestType->read(ss, this->suppData),
		stream::error
	);

	return;
}

void test_music::changeAttribute(unsigned int attributeIndex,
	const std::string& newValue, const std::string& content)
{
	this->ts->add(
		boost::unit_test::make_test_case(
			std::bind(
				(void(test_music::*)(unsigned int, const std::string&,
					const std::string&, unsigned int))&test_music::test_changeAttribute,
				this, attributeIndex,
				newValue, content, this->numChangeAttributeTests),
			createString("test_music[" << this->basename << "]::changeAttribute_a"
				<< std::setfill('0') << std::setw(2) << this->numChangeAttributeTests),
			__FILE__, __LINE__
		)
	);
	this->numChangeAttributeTests++;
	return;
}

void test_music::changeAttribute(unsigned int attributeIndex,
	unsigned int newValue, const std::string& content)
{
	this->ts->add(
		boost::unit_test::make_test_case(
			std::bind(
				(void(test_music::*)(unsigned int, int,
					const std::string&, unsigned int))&test_music::test_changeAttribute,
				this, attributeIndex,
				newValue, content, this->numChangeAttributeTests),
			createString("test_music[" << this->basename << "]::changeAttribute_a"
				<< std::setfill('0') << std::setw(2) << this->numChangeAttributeTests),
			__FILE__, __LINE__
		)
	);
	this->numChangeAttributeTests++;
	return;
}

void test_music::test_changeAttribute(unsigned int attributeIndex,
	const std::string& newValue, const std::string& content,
	unsigned int testNumber)
{
	BOOST_TEST_MESSAGE(this->basename << ": changeAttribute_a"
		<< std::setfill('0') << std::setw(2) << testNumber);

	this->prepareTest();
	auto music = this->pType->read(this->base, this->suppData);
	music->attribute(attributeIndex, newValue);

	// Write modified map
	this->base.seekp(0, stream::start);
	this->base.data.clear();

	this->pType->write(this->base, this->suppData, *music, this->writeFlags);

	// Can't use checkData() here as we don't have parameter for target suppData
	BOOST_CHECK_MESSAGE(
		this->is_content_equal(content),
		"Error setting string attribute"
	);
	return;
}

void test_music::test_changeAttribute(unsigned int attributeIndex,
	int newValue, const std::string& content,
	unsigned int testNumber)
{
	BOOST_TEST_MESSAGE(this->basename << ": changeAttribute_a"
		<< std::setfill('0') << std::setw(2) << testNumber);

	this->prepareTest();
	auto music = this->pType->read(this->base, this->suppData);
	music->attribute(attributeIndex, newValue);

	// Write modified map
	this->base.seekp(0, stream::start);
	this->base.data.clear();

	this->pType->write(this->base, this->suppData, *music, this->writeFlags);

	// Can't use checkData() here as we don't have parameter for target suppData
	BOOST_CHECK_MESSAGE(
		this->is_content_equal(content),
		"Error setting int attribute"
	);
	return;
}

void test_music::rewrite(const std::string& before, const std::string& after)
{
#warning TODO: Make other tests also use addBoundTest?  Otherwise it will never be used
	this->addBoundTest(
		std::bind(
			&test_music::test_rewrite, this, before, after,
			this->numRewriteTests
		),
		__FILE__, __LINE__,
		createString("test_music[" << this->basename << "]::rewrite_"
			<< std::setfill('0') << std::setw(2) << this->numRewriteTests)
	);
	this->numRewriteTests++;
	return;
}

void test_music::test_rewrite(const std::string& before,
	const std::string& after, unsigned int testNumber)
{
	BOOST_TEST_MESSAGE(createString("rewrite check (" << this->basename
		<< "; " << std::setfill('0') << std::setw(2) << testNumber << ")"));

	this->prepareTest();

	// Change initial data to the 'before' parameter
	this->base.seekp(0, stream::start);
	this->base.data.clear();
	this->base << before;

	auto music = this->pType->read(this->base, this->suppData);

	// Write modified map
	this->base.seekp(0, stream::start);
	this->base.data.clear();

	this->pType->write(this->base, this->suppData, *music, this->writeFlags);

	BOOST_CHECK_MESSAGE(
		this->is_content_equal(after),
		"Error rewriting song - data is different to expected"
	);

	return;
}

boost::test_tools::predicate_result test_music::is_content_equal(
	const std::string& exp)
{
	return this->is_equal(exp, this->base.data);
}

boost::test_tools::predicate_result test_music::is_supp_equal(
	camoto::SuppItem type, const std::string& strExpected)
{
	// Use the supp's test-class' own comparison function, as this will use its
	// preferred outputWidth value, which might be different to the main file's.
	return this->suppResult[type]->is_equal(strExpected,
		this->suppBase[type]->data);
}

void test_music::test_test_init()
{
	BOOST_REQUIRE_GT(this->indexInstrumentOPL, -2);
	BOOST_REQUIRE_GT(this->indexInstrumentMIDI, -2);
	BOOST_REQUIRE_GT(this->indexInstrumentPCM, -2);
}

void test_music::test_isinstance_others()
{
	// Check all file formats except this one to avoid any false positives
	BOOST_TEST_MESSAGE(this->basename << ": isInstance check against other formats");

	for (const auto& pTestType : MusicManager::formats()) {
		// Don't check our own type, that's done by the other isinstance_* tests
		std::string otherType = pTestType->code();
		if (otherType.compare(this->type) == 0) continue;

		// Skip any formats known to produce false detections unavoidably
		if (
			std::find(
				this->skipInstDetect.begin(), this->skipInstDetect.end(), otherType
			) != this->skipInstDetect.end()) continue;

		BOOST_TEST_MESSAGE("Checking " << this->type
			<< " content against isInstance() for " << otherType);

		// Put this outside the BOOST_CHECK_MESSAGE macro so if an exception is
		// thrown we can see the above BOOST_TEST_CHECKPOINT message telling us which
		// handler is to blame.
		auto isInstanceResult = pTestType->isInstance(this->base);

		BOOST_CHECK_MESSAGE(isInstanceResult < MusicType::Certainty::DefinitelyYes,
			"isInstance() for " << otherType << " incorrectly recognises content for "
			<< this->type);
	}
	return;
}

void test_music::test_read()
{
	BOOST_TEST_MESSAGE(this->basename << ": Read music file");

	auto music = this->pType->read(this->base, this->suppData);

	if (this->dumpEvents) listEvents(*music);

	if (this->indexInstrumentOPL >= 0) {
		BOOST_REQUIRE_GT(music->patches->size(), this->indexInstrumentOPL);

		auto oplPatch = dynamic_cast<OPLPatch*>(
			music->patches->at(this->indexInstrumentOPL).get());
		BOOST_REQUIRE(oplPatch);

		BOOST_CHECK_EQUAL(oplPatch->m.enableTremolo, this->oplPatch.m.enableTremolo);
		BOOST_CHECK_EQUAL(oplPatch->m.enableVibrato, this->oplPatch.m.enableVibrato);
		BOOST_CHECK_EQUAL(oplPatch->m.enableSustain, this->oplPatch.m.enableSustain);
		BOOST_CHECK_EQUAL(oplPatch->m.enableKSR, this->oplPatch.m.enableKSR);
		BOOST_CHECK_EQUAL(oplPatch->m.freqMult, this->oplPatch.m.freqMult);
		BOOST_CHECK_EQUAL(oplPatch->m.scaleLevel, this->oplPatch.m.scaleLevel);
		BOOST_CHECK_EQUAL(oplPatch->m.outputLevel, this->oplPatch.m.outputLevel);
		BOOST_CHECK_EQUAL(oplPatch->m.attackRate, this->oplPatch.m.attackRate);
		BOOST_CHECK_EQUAL(oplPatch->m.decayRate, this->oplPatch.m.decayRate);
		BOOST_CHECK_EQUAL(oplPatch->m.sustainRate, this->oplPatch.m.sustainRate);
		BOOST_CHECK_EQUAL(oplPatch->m.releaseRate, this->oplPatch.m.releaseRate);
		BOOST_CHECK_EQUAL(oplPatch->m.waveSelect, this->oplPatch.m.waveSelect);

		BOOST_CHECK_EQUAL(oplPatch->c.enableTremolo, this->oplPatch.c.enableTremolo);
		BOOST_CHECK_EQUAL(oplPatch->c.enableVibrato, this->oplPatch.c.enableVibrato);
		BOOST_CHECK_EQUAL(oplPatch->c.enableSustain, this->oplPatch.c.enableSustain);
		BOOST_CHECK_EQUAL(oplPatch->c.enableKSR, this->oplPatch.c.enableKSR);
		BOOST_CHECK_EQUAL(oplPatch->c.freqMult, this->oplPatch.c.freqMult);
		BOOST_CHECK_EQUAL(oplPatch->c.scaleLevel, this->oplPatch.c.scaleLevel);
		BOOST_CHECK_EQUAL(oplPatch->c.outputLevel, this->oplPatch.c.outputLevel);
		BOOST_CHECK_EQUAL(oplPatch->c.attackRate, this->oplPatch.c.attackRate);
		BOOST_CHECK_EQUAL(oplPatch->c.decayRate, this->oplPatch.c.decayRate);
		BOOST_CHECK_EQUAL(oplPatch->c.sustainRate, this->oplPatch.c.sustainRate);
		BOOST_CHECK_EQUAL(oplPatch->c.releaseRate, this->oplPatch.c.releaseRate);
		BOOST_CHECK_EQUAL(oplPatch->c.waveSelect, this->oplPatch.c.waveSelect);

		BOOST_CHECK_EQUAL(oplPatch->feedback, this->oplPatch.feedback);
		BOOST_CHECK_EQUAL(oplPatch->connection, this->oplPatch.connection);
		BOOST_CHECK_EQUAL(oplPatch->rhythm, this->oplPatch.rhythm);
	}

	if (this->indexInstrumentMIDI >= 0) {
		BOOST_REQUIRE_GT(music->patches->size(), this->indexInstrumentMIDI);

		auto midiPatch = dynamic_cast<MIDIPatch*>(
			music->patches->at(this->indexInstrumentMIDI).get());
		BOOST_REQUIRE(midiPatch);
	}

	if (this->indexInstrumentPCM >= 0) {
		BOOST_REQUIRE_GT(music->patches->size(), this->indexInstrumentPCM);

		auto pcmPatch = dynamic_cast<PCMPatch*>(
			music->patches->at(this->indexInstrumentPCM).get());
		BOOST_REQUIRE(pcmPatch);
	}

#warning TODO: Put a delay at the start and end and ensure it is read and saved correctly.

	BOOST_REQUIRE_EQUAL(music->patches->size(), this->numInstruments);
}

void test_music::test_write()
{
	BOOST_TEST_MESSAGE("Write music file");

	// Read in the standard format
	auto music = this->pType->read(this->base, this->suppData);

	// Write it out again
	this->base.seekp(0, stream::start);
	this->base.data.clear();

	this->pType->write(this->base, this->suppData, *music, this->writeFlags);

	// Make sure it matches what we read
	BOOST_REQUIRE(this->is_content_equal(this->standard()));
}

void test_music::test_attributes()
{
	BOOST_TEST_MESSAGE(this->basename << ": Test attributes");

	auto attrAll = this->pType->supportedAttributes();
	int i = 0;
	for (auto& attrExpected : this->attributes) {
		BOOST_REQUIRE_MESSAGE(i < attrAll.size(),
			"Cannot find attribute #" << i);
		auto& attrMusic = attrAll[i];

		BOOST_REQUIRE_EQUAL((int)attrExpected.type, (int)attrMusic.type);

		switch (attrExpected.type) {
			case Attribute::Type::Integer:
				BOOST_REQUIRE_EQUAL(attrExpected.integerValue, attrMusic.integerValue);
				break;
			case Attribute::Type::Enum:
				BOOST_REQUIRE_EQUAL(attrExpected.enumValue, attrMusic.enumValue);
				break;
			case Attribute::Type::Filename:
				BOOST_REQUIRE_MESSAGE(
					this->is_equal(attrExpected.filenameValue, attrMusic.filenameValue),
					"Error getting filename attribute"
				);
				break;
			case Attribute::Type::Text:
				BOOST_REQUIRE_MESSAGE(
					this->is_equal(attrExpected.textValue, attrMusic.textValue),
					"Error getting text attribute"
				);
				break;
			case Attribute::Type::Image:
				BOOST_REQUIRE_EQUAL(attrExpected.imageIndex, attrMusic.imageIndex);
		}
		i++;
	}
}
