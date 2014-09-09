/**
 * @file   test-music.hpp
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

#ifndef _CAMOTO_GAMEMUSIC_TEST_MUSIC_HPP_
#define _CAMOTO_GAMEMUSIC_TEST_MUSIC_HPP_

#include <map>
#include <boost/test/unit_test.hpp>
#include <boost/bind.hpp>
#include <camoto/stream_string.hpp>
#include <camoto/gamemusic.hpp>
#include "tests.hpp"

// This header will only be used by test implementations.
using namespace camoto;
using namespace camoto::gamemusic;

/// Exception thrown if test_musicmetadata::metadata_*() is called for
/// unsupported fields.
class test_metadata_not_supported {};

class test_music: public test_main
{
	public:
		/// Constructor sets some default values.
		test_music();

		/// Add all the standard tests.
		/**
		 * This can be overridden by descendent classes to add more tests for
		 * particular file formats.  If this is done, remember to call this
		 * function from the overridden one or the standard tests won't get run.
		 */
		virtual void addTests();

		/// Reset file content back to a known state.
		virtual void prepareTest();

		/// Meta test: make sure the tests are initialised correctly.
		void test_test_init();

		void test_isinstance_others();
		void test_read();
		/// Write a completely normal file.
		void test_write();
		void test_new_isinstance();
		/// Write a normal file but check the metadata length limits are enforced.
		void test_metadata_desc();
		void test_metadata_palfile();
		void test_metadata_version();
		void test_metadata_title();
		void test_metadata_author();

	protected:
		/// Standard state.
		/**
		 * This contains all the standard song data, and is also the content
		 * produced by creating a new file in this format.
		 */
		virtual std::string standard() = 0;

		/// Standard state but with metadata description field changed.
		virtual std::string metadata_desc_replaced();

		/// Standard state but with metadata palette filename field changed.
		virtual std::string metadata_palfile_replaced();

		/// Standard state but with metadata version field changed.
		virtual std::string metadata_version_replaced();

		/// Standard state but with metadata title field changed.
		virtual std::string metadata_title_replaced();

		/// Standard state but with metadata author field changed.
		virtual std::string metadata_author_replaced();

		/// Add a test to the suite.  Used by ADD_MUSIC_TEST().
		void addBoundTest(boost::function<void()> fnTest,
			boost::unit_test::const_string name);

		/// Reset the music to the initial state and run the given test.
		/**
		 * @param fnTest
		 *   Function to call once music is back to initial state.
		 */
		void runTest(boost::function<void()> fnTest);

		/// Add an isInstance check to run later.
		/**
		 * @param result
		 *   Expected result when opening the content.
		 *
		 * @param content
		 *   Content to pass as an music to MusicType::isInstance().
		 */
		void isInstance(MusicType::Certainty result, const std::string& content);

		/// Perform an isInstance check now.
		void test_isInstance(MusicType::Certainty result,
			const std::string& content, unsigned int testNumber);

		/// Add an invalidContent check to run later.
		/**
		 * These checks make sure files that are in the correct format
		 * don't cause segfaults or infinite loops if the data is corrupted.
		 *
		 * @param content
		 *   Content to pass as an music to MusicType::isInstance() where
		 *   it will be reported as a valid instance, then passed to
		 *   MusicType::open(), where an exception should be thrown.
		 */
		void invalidContent(const std::string& content);

		/// Perform an invalidContent check now.
		void test_invalidContent(const std::string& content,
			unsigned int testNumber);

		/// Does the music content match the parameter?
		boost::test_tools::predicate_result is_content_equal(const std::string& exp);

		/// Does the given supplementary item content match the parameter?
		boost::test_tools::predicate_result is_supp_equal(
			camoto::SuppItem::Type type, const std::string& strExpected);

	protected:
		/// Underlying data stream containing file content.
		stream::string_sptr base;

		/// Factory class used to open files in this format.
		MusicTypePtr pType;

		/// Supplementary data for the file.
		camoto::SuppData suppData;

		void test_metadata_generic(const std::string& name,
			Metadata::MetadataType item, const std::string& expected);

	private:
		/// Have we allocated pType yet?
		bool init;

		/// Number of isInstance tests, used to number them sequentially.
		unsigned int numIsInstanceTests;

		/// Number of invalidData tests, used to number them sequentially.
		unsigned int numInvalidContentTests;

	public:
		/// File type code for this format.
		std::string type;

		/// Flags to pass to MusicType::write().
		unsigned int writeFlags;

		/// Number of instruments in the file.
		int numInstruments;

		/// Index of the standard OPL instrument.
		/**
		 * Set to -1 if there are no OPL instruments in this format, otherwise set
		 * to the zero-based index of the instrument.
		 *
		 * This is set to -2 by default which will produce an error reminding the
		 * test designer that they forgot to set this to a valid value.
		 */
		int indexInstrumentOPL;

		/// Index of the standard MIDI instrument.
		/**
		 * Set to -1 if there are no MIDI instruments in this format, otherwise set
		 * to the zero-based index of the instrument.
		 *
		 * This is set to -2 by default which will produce an error reminding the
		 * test designer that they forgot to set this to a valid value.
		 */
		int indexInstrumentMIDI;

		/// Index of the standard PCM instrument.
		/**
		 * Set to -1 if there are no PCM instruments in this format, otherwise set
		 * to the zero-based index of the instrument.
		 *
		 * This is set to -2 by default which will produce an error reminding the
		 * test designer that they forgot to set this to a valid value.
		 */
		int indexInstrumentPCM;

		/// Does this format support metadata?
		std::map<camoto::Metadata::MetadataType, bool> hasMetadata;

		/// Default value for metadata fields, if present.
		std::map<camoto::Metadata::MetadataType, std::string> metadataContent;

		/// Link between supplementary items and the class containing the expected
		/// content for each test case.
		std::map<camoto::SuppItem::Type, boost::shared_ptr<test_music> > suppResult;
};

/// Add a test_music member function to the test suite
#define ADD_MUSIC_TEST(fn) {	  \
	boost::function<void()> fnTest = boost::bind(fn, this); \
	this->test_music::addBoundTest(fnTest, BOOST_TEST_STRINGIZE(fn)); \
}

#endif // _CAMOTO_GAMEMUSIC_TEST_MUSIC_HPP_
