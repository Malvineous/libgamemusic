/**
 * @file   test-music.cpp
 * @brief  Generic test code for MusicType class descendents.
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

#include <iomanip>
#include <boost/bind.hpp>
#include <camoto/util.hpp>
#include "test-music.hpp"

using namespace camoto;
using namespace camoto::gamemusic;

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
		numInvalidContentTests(1)
{
	this->writeFlags = 0;
	this->numInstruments = -1;
	this->indexInstrumentOPL = -2;
	this->indexInstrumentMIDI = -2;
	this->indexInstrumentPCM = -2;

	this->hasMetadata[Metadata::Description] = false;
	this->hasMetadata[Metadata::PaletteFilename] = false;
	this->hasMetadata[Metadata::Version] = false;
	this->hasMetadata[Metadata::Title] = false;
	this->hasMetadata[Metadata::Author] = false;

	this->metadataContent[Metadata::Description] = "Test description";
	this->metadataContent[Metadata::PaletteFilename] = "Test palette";
	this->metadataContent[Metadata::Version] = "123";
	this->metadataContent[Metadata::Title] = "Test title";
	this->metadataContent[Metadata::Author] = "Test author";
}

void test_music::addTests()
{
	ADD_MUSIC_TEST(&test_music::test_test_init);

	ADD_MUSIC_TEST(&test_music::test_isinstance_others);
	ADD_MUSIC_TEST(&test_music::test_read);
	ADD_MUSIC_TEST(&test_music::test_write);

	// Only perform the metadata tests if supported by the music format
	if (this->hasMetadata[Metadata::Description]) {
		ADD_MUSIC_TEST(&test_music::test_metadata_desc);
	}
	if (this->hasMetadata[Metadata::PaletteFilename]) {
		ADD_MUSIC_TEST(&test_music::test_metadata_palfile);
	}
	if (this->hasMetadata[Metadata::Version]) {
		ADD_MUSIC_TEST(&test_music::test_metadata_version);
	}
	if (this->hasMetadata[Metadata::Title]) {
		ADD_MUSIC_TEST(&test_music::test_metadata_title);
	}
	if (this->hasMetadata[Metadata::Author]) {
		ADD_MUSIC_TEST(&test_music::test_metadata_author);
	}

	return;
}

void test_music::addBoundTest(boost::function<void()> fnTest,
	boost::unit_test::const_string name)
{
	boost::function<void()> fnTestWrapper = boost::bind(&test_music::runTest,
		this, fnTest);
	this->ts->add(boost::unit_test::make_test_case(
		boost::unit_test::callback0<>(fnTestWrapper),
		createString(name << '[' << this->basename << ']')
	));
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
	if (!this->init) {
		ManagerPtr pManager;
		BOOST_REQUIRE_NO_THROW(
			pManager = getManager();
			this->pType = pManager->getMusicTypeByCode(this->type);
		);
		BOOST_REQUIRE_MESSAGE(this->pType, "Could not find music type " + this->type);
		this->init = true;
	}

	if (this->suppResult[SuppItem::FAT]) {
		stream::string_sptr suppSS(new stream::string());
		// Populate the suppitem with its initial state
		suppSS << this->suppResult[SuppItem::FAT]->standard();
		this->suppData[SuppItem::FAT] = suppSS;
	}

	this->base.reset(new stream::string());

	this->base << this->standard();
	return;
}

void test_music::isInstance(MusicType::Certainty result,
	const std::string& content)
{
	boost::function<void()> fnTest = boost::bind(&test_music::test_isInstance,
		this, result, content, this->numIsInstanceTests);
	this->ts->add(boost::unit_test::make_test_case(
			boost::unit_test::callback0<>(fnTest),
			createString("test_music[" << this->basename << "]::isinstance_c"
				<< std::setfill('0') << std::setw(2) << this->numIsInstanceTests)
		));
	this->numIsInstanceTests++;
	return;
}

void test_music::test_isInstance(MusicType::Certainty result,
	const std::string& content, unsigned int testNumber)
{
	BOOST_TEST_MESSAGE(createString("isInstance check (" << this->basename
		<< "; " << std::setfill('0') << std::setw(2) << testNumber << ")"));

	ManagerPtr pManager(getManager());
	MusicTypePtr pTestType(pManager->getMusicTypeByCode(this->type));
	BOOST_REQUIRE_MESSAGE(pTestType,
		createString("Could not find music type " << this->type));

	stream::string_sptr ss(new stream::string());
	ss << content;

	BOOST_CHECK_EQUAL(pTestType->isInstance(ss), result);
	return;
}

void test_music::invalidContent(const std::string& content)
{
	boost::function<void()> fnTest = boost::bind(&test_music::test_invalidContent,
		this, content, this->numInvalidContentTests);
	this->ts->add(boost::unit_test::make_test_case(
			boost::unit_test::callback0<>(fnTest),
			createString("test_music[" << this->basename << "]::invalidcontent_i"
				<< std::setfill('0') << std::setw(2) << this->numInvalidContentTests)
		));
	this->numInvalidContentTests++;
	return;
}

void test_music::test_invalidContent(const std::string& content,
	unsigned int testNumber)
{
	BOOST_TEST_MESSAGE(createString("invalidContent check (" << this->basename
		<< "; " << std::setfill('0') << std::setw(2) << testNumber << ")"));

	ManagerPtr pManager(getManager());
	MusicTypePtr pTestType(pManager->getMusicTypeByCode(this->type));
	BOOST_REQUIRE_MESSAGE(pTestType,
		createString("Could not find music type " << this->type));

	stream::string_sptr ss(new stream::string());
	ss << content;

	// Make sure isInstance reports this is valid
	BOOST_CHECK_EQUAL(pTestType->isInstance(ss), MusicType::DefinitelyYes);

	// But that we get an error when trying to open the file
	BOOST_CHECK_THROW(
		MusicPtr pMusic(pTestType->read(ss, this->suppData)),
		stream::error
	);

	return;
}

boost::test_tools::predicate_result test_music::is_content_equal(
	const std::string& exp)
{
	return this->is_equal(exp, this->base->str());
}

boost::test_tools::predicate_result test_music::is_supp_equal(
	camoto::SuppItem::Type type, const std::string& strExpected)
{
	stream::string_sptr suppBase =
		boost::dynamic_pointer_cast<stream::string>(this->suppData[type]);
	return this->is_equal(strExpected, suppBase->str());
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
	BOOST_TEST_MESSAGE("isInstance check for other formats (not " << this->type
		<< ")");
	ManagerPtr pManager(camoto::gamemusic::getManager());
	int i = 0;
	MusicTypePtr pTestType;
	for (int i = 0; (pTestType = pManager->getMusicType(i)); i++) {
		// Don't check our own type, that's done by the other isinstance_* tests
		std::string otherType = pTestType->getCode();
		if (otherType.compare(this->type) == 0) continue;

		BOOST_TEST_MESSAGE("Checking " << this->type
			<< " content against isInstance() for " << otherType);

		BOOST_CHECK_MESSAGE(pTestType->isInstance(base) < MusicType::DefinitelyYes,
			"isInstance() for " << otherType << " incorrectly recognises content for "
			<< this->type);
	}
	return;
}

void test_music::test_read()
{
	BOOST_TEST_MESSAGE("Read music file");

	MusicPtr music(this->pType->read(this->base, this->suppData));

	BOOST_REQUIRE_EQUAL(music->patches->size(), this->numInstruments);
}

void test_music::test_write()
{
	BOOST_TEST_MESSAGE("Write music file");

	// Read in the standard format
	MusicPtr music(this->pType->read(this->base, this->suppData));

	// Write it out again
	this->base.reset(new stream::string());
	this->pType->write(this->base, this->suppData, music, this->writeFlags);

	// Make sure it matches what we read
	BOOST_REQUIRE(this->is_content_equal(this->standard()));
}

void test_music::test_metadata_desc()
{
	this->test_metadata_generic("Description", Metadata::Description,
		this->metadata_desc_replaced());
}

void test_music::test_metadata_palfile()
{
	this->test_metadata_generic("PaletteFilename", Metadata::PaletteFilename,
		this->metadata_palfile_replaced());
}

void test_music::test_metadata_version()
{
	this->test_metadata_generic("Version", Metadata::Version,
		this->metadata_version_replaced());
}

void test_music::test_metadata_title()
{
	this->test_metadata_generic("Title", Metadata::Title,
		this->metadata_title_replaced());
}

void test_music::test_metadata_author()
{
	this->test_metadata_generic("Author", Metadata::Author,
		this->metadata_author_replaced());
}

void test_music::test_metadata_generic(const std::string& name,
	Metadata::MetadataType item, const std::string& expected)
{
	BOOST_TEST_MESSAGE("Metadata " + name);

	MusicPtr music(this->pType->read(this->base, this->suppData));

	BOOST_REQUIRE_EQUAL(music->metadata[item], this->metadataContent[item]);

	music->metadata[item] = "Replaced";

	this->base.reset(new stream::string());
	this->pType->write(this->base, this->suppData, music, this->writeFlags);

	BOOST_REQUIRE(this->is_content_equal(expected));
}

std::string test_music::metadata_desc_replaced()
{
	throw new test_metadata_not_supported;
}

std::string test_music::metadata_palfile_replaced()
{
	throw new test_metadata_not_supported;
}

std::string test_music::metadata_version_replaced()
{
	throw new test_metadata_not_supported;
}

std::string test_music::metadata_title_replaced()
{
	throw new test_metadata_not_supported;
}

std::string test_music::metadata_author_replaced()
{
	throw new test_metadata_not_supported;
}
