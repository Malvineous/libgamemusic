/**
 * @file   test-music.hpp
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

#ifndef _CAMOTO_GAMEMUSIC_TEST_MUSIC_HPP_
#define _CAMOTO_GAMEMUSIC_TEST_MUSIC_HPP_

#include <map>
#include <functional>
#include <boost/test/unit_test.hpp>
#include <camoto/stream_string.hpp>
#include <camoto/gamemusic.hpp>
#include "tests.hpp"

// This header will only be used by test implementations.
using namespace camoto;
using namespace camoto::gamemusic;

/// Print a list of all the events in the song to stdout.
void listEvents(const Music& music);

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
		void test_isinstance_empty();
		void test_read();
		/// Write a completely normal file.
		void test_write();
		void test_attributes();

	protected:
		/// Standard state.
		/**
		 * This contains all the standard song data, and is also the content
		 * produced by creating a new file in this format.
		 */
		virtual std::string standard() = 0;

		/// Add a test to the suite.  Used by ADD_MUSIC_TEST().
		void addBoundTest(std::function<void()> fnTest,
			boost::unit_test::const_string file, std::size_t line,
			boost::unit_test::const_string name);

		/// Reset the music to the initial state and run the given test.
		/**
		 * @param fnTest
		 *   Function to call once music is back to initial state.
		 */
		void runTest(std::function<void()> fnTest);

		/// Populate suppBase with default content.
		/**
		 * This may be called mid-test if the suppBase content should be reset to
		 * the initial state.
		 *
		 * @param empty
		 *   If true, the supp items are blanked out and not repopulated.
		 */
		void resetSuppData(bool empty);

		/// Populate suppData with data loaded from suppBase.
		/**
		 * This may be called mid-test if new suppData structures are needed to
		 * create a new Music instance.
		 *
		 * This repopulates suppData from the existing suppBase content, so it is
		 * possible to access modified data this way.  If you don't want suppData
		 * that may have been modified by a previous Music instance, call
		 * resetSuppData() first to return everything to the initialstate.
		 */
		void populateSuppData();

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

		/// Add a changeAttribute check to run later.
		/**
		 * These checks make sure attribute alterations work correctly.
		 *
		 * @param attributeIndex
		 *   Zero-based index of the attribute to change.
		 *
		 * @param newValue
		 *   New content for the attribute.
		 *
		 * @param content
		 *   Expected result after taking the initialstate() and changing the
		 *   given attribute as specified.
		 */
		void changeAttribute(unsigned int attributeIndex,
			const std::string& newValue, const std::string& content);

		void changeAttribute(unsigned int attributeIndex,
			unsigned int newValue, const std::string& content);

		/// Perform a changeAttribute<std::string> check now.
		void test_changeAttribute(unsigned int attributeIndex,
			const std::string& newValue, const std::string& content,
			unsigned int testNumber);

		/// Perform a changeAttribute<int> check now.
		void test_changeAttribute(unsigned int attributeIndex,
			int newValue, const std::string& content,
			unsigned int testNumber);

		/// Add a rewrite check to run later.
		/**
		 * These checks make sure certain types of input data get written out
		 * differently upon saving.
		 *
		 * @param before
		 *   Content to be read.
		 *
		 * @param after
		 *   Expected content to be written out again.
		 */
		void rewrite(const std::string& before, const std::string& after);

		/// Perform a rewrite check now.
		void test_rewrite(const std::string& before, const std::string& after,
			unsigned int testNumber);

		/// Does the music content match the parameter?
		boost::test_tools::predicate_result is_content_equal(const std::string& exp);

		/// Does the given supplementary item content match the parameter?
		boost::test_tools::predicate_result is_supp_equal(
			camoto::SuppItem type, const std::string& strExpected);

	protected:
		/// Underlying data stream containing file content.
		stream::string base;

		/// Factory class used to open files in this format.
		MusicManager::handler_t pType;

		/// Pointers to the underlying storage used for suppitems.
		std::map<SuppItem, std::shared_ptr<stream::string>> suppBase;

		/// Supplementary data for the file.
		camoto::SuppData suppData;

	private:
		/// Have we allocated pType yet?
		bool init;

		/// Number of isInstance tests, used to number them sequentially.
		unsigned int numIsInstanceTests;

		/// Number of invalidData tests, used to number them sequentially.
		unsigned int numInvalidContentTests;

		unsigned int numChangeAttributeTests;

		unsigned int numRewriteTests;

	public:
		/// File type code for this format.
		std::string type;

		/// Any formats here identify us as an instance of that type, and it
		/// cannot be avoided.
		/**
		 * If "otherformat" is listed here then we will not pass our initialstate
		 * to otherformat's isInstance function.  This is kind of backwards but is
		 * is the way the test functions are designed.
		 */
		std::vector<std::string> skipInstDetect;

		/// Flags to pass to MusicType::write().
		MusicType::WriteFlags writeFlags;

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

		/// Patch to expect at patchbank position indexInstrumentOPL.
		OPLPatch oplPatch;

		/// If true, list music events in read() test on stdout.  Defaults to false.
		bool dumpEvents;

		/// List of attributes this format supports, can be empty.
		std::vector<camoto::Attribute> attributes;

		/// Link between supplementary items and the class containing the expected
		/// content for each test case.
		std::map<camoto::SuppItem, std::unique_ptr<test_music>> suppResult;

		/// Set to false if the format cannot be written yet (development use only)
		bool writingSupported;
};

/// Add a test_music member function to the test suite
#define ADD_MUSIC_TEST(fn) \
	this->test_music::addBoundTest( \
		std::bind(fn, this), \
		__FILE__, __LINE__, \
		BOOST_TEST_STRINGIZE(fn) \
	);

#endif // _CAMOTO_GAMEMUSIC_TEST_MUSIC_HPP_
