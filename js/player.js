/**
 * @file  libgamemusic/js/player.js
 * @brief Javascript interface to the libgamemusic asm.js player.
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

class LGMPlayer
{
	/// Find all placeholder elements and replace them with player widgets.
	static createPlayers() {
		// Load the JS and CSS we need first
		Promise.all([
			this.addCSS('player.css'),
			this.addJS('libgamemusic-player-asm.js'),
		]).then(this.insertPlayers);
	}

	/// Replace the divs with actual player HTML.
	static insertPlayers() {
		var el = document.getElementsByTagName('span');
		for (var i = 0; i < el.length; i++) {
			if (el[i].className == "lgmPlayer") {
				var p = new LGMPlayer(el[i]);
			}
		}
	}

	/// Add a CSS file at runtime.
	static addCSS(href) {
		href = LGMPlayer.urlPrefix + href;
		return new Promise((resolve, reject) => {
			// avoid duplicates
			for(var i = 0; i < document.styleSheets.length; i++){
				if (document.styleSheets[i].href == href) {
					resolve();
					return;
				}
			}
			var head = document.getElementsByTagName('head')[0];
			var link = document.createElement('link');
			link.onload = resolve;
			link.onerror = reject;
			link.rel = 'stylesheet';
			link.type = 'text/css';
			link.href = href;
			head.appendChild(link);
		});
	}

	/// Include a JS file at runtime.
	static addJS(href) {
		href = LGMPlayer.urlPrefix + href;
		return new Promise((resolve, reject) => {
			var head = document.getElementsByTagName('head')[0];
			var link = document.createElement('script');
			link.onload = resolve;
			link.onerror = reject;
			link.src = href;
			head.appendChild(link);
		});
	}

	/// Create a new player widget inside the given element.
	/**
	 * @param Element elPlayer
	 *   HTML Element to turn into the player.
	 */
	constructor(elPlayer) {
		this.elPlayer = elPlayer;
		elPlayer.player = this;
		this.playing = false;
		this.ready = false;
		this.failed = false;
		var self = this;

		// Play/pause button
		this.ctPlay = document.createElement('i');
		this.ctPlay.className = 'material-icons lgmButton lgmLoading';
		this.ctPlay.appendChild(document.createTextNode('sync'));
		this.ctPlay.addEventListener('click', function() {
			self.onPlayPause();
		});
		elPlayer.appendChild(this.ctPlay);

		// Progress bar
		this.ctProgress = document.createElement('span');
		this.ctProgress.className = 'lgmProgress';
		this.ctProgress.addEventListener('click', function(ev) {
			self.onSeek(ev);
		});
		elPlayer.appendChild(this.ctProgress);

		this.ctProgressBG = document.createElement('span');
		this.ctProgressBG.className = 'lgmProgressBG';
		this.ctProgress.appendChild(this.ctProgressBG);

		this.ctProgressNeedle = document.createElement('span');
		this.ctProgressNeedle.className = 'lgmProgressNeedle';
		this.ctProgressBG.appendChild(this.ctProgressNeedle);

		// Time display
		var ctTimes = document.createElement('span');
		ctTimes.className = 'lgmTimes';
		elPlayer.appendChild(ctTimes);

		this.ctPlayTime = document.createElement('span');
		this.ctPlayTime.className = 'cur';
		this.ctPlayTime.appendChild(document.createTextNode('0:00'));
		ctTimes.appendChild(this.ctPlayTime);

		ctTimes.appendChild(document.createTextNode('/'));

		this.ctTotalTime = document.createElement('span');
		this.ctTotalTime.className = 'total';
		this.ctTotalTime.appendChild(document.createTextNode('?:??'));
		ctTimes.appendChild(this.ctTotalTime);

		this.setProgress(0);
		this.downloadSrcSong();
	}

	/// Download the song given in the HTML element's "data-src" attribute.
	/**
	 * This is used to initially download the song, and also to try the download
	 * again if it fails and the user clicks on the error icon.
	 */
	downloadSrcSong() {
		if (!this.elPlayer.dataset.src) {
			this.showError({
				message: 'No song specified.',
			});
			return;
		}
		this.getSong(this.elPlayer.dataset.src, this.elPlayer.dataset.type);
	}

	/// Download the specified song.
	/**
	 * @param string url
	 *   URL to download.
	 *
	 * @param string typecode
	 *   File format type, omit to autodetect.
	 *
	 * @return Promise, when resolved the song is available to be played.
	 */
	getSong(url, typecode) {
		this.ready = false;
		this.failed = false;
		this.failCount = 0;
		var self = this;

		this.ctPlay.classList.add('lgmLoading');
		this.ctPlay.firstChild.nodeValue = 'sync';

		// We want to download the URL first then init libgamemusic, so that they
		// happen in parallel in case one is slow.
		return this.download(url).then(content => {
			// Make sure everything has loaded first
			return this.initLibgamemusic().then(() => {
				return self.setSong(content, url, typecode);
			});
		}).catch(e => {
			self.showError(e);
			throw e;
		});
	}

	/// Download the given URL and return the content as a resolved Promise.
	download(url) {
		return new Promise((resolve, reject) => {
			var request = new XMLHttpRequest();
			request.open('GET', url, true);
			request.responseType = 'arraybuffer';
			request.onload = function() {
				resolve(request.response);
			};
			request.onerror = function(e) {
				reject({
					url: url,
					message: 'Unable to download ' + url + ' - got HTTP' + e.target.status
						+ ': ' + e.target.statusText
				});
			};
			request.send();
		});
	}

	/// Convert milliseconds to (h:m)m:ss
	msToTime(ms) {
		var s_t = ms / 1000;
		var m_t = Math.floor(s_t / 60);
		var s = Math.floor(s_t) % 60;
		var h = Math.floor(m_t / 60);
		var m = m_t % 60;
		var str = s;
		if (s < 10) str = '0' + str;
		str = m + ':' + str;
		if (h > 0) {
			if (m < 10) str = '0' + str;
			str = h + ':' + str;
		}
		return str;
	}

	/// Song has been downloaded, see if we need any extra files
	/**
	 * @param ArrayBuffer content
	 *   Song content to load.
	 *
	 * @param string url
	 *   URL or just plain filename.  Used for those file formats that need
	 *   additional data files, where those files are named based on the main
	 *   file (e.g. example.mus loads instruments from example.tim).  It can be
	 *   omitted if you know that you will never be playing songs that need
	 *   additional data files based on the main song filename.
	 *
	 * @param string typecode
	 *   libgamemusic file type code.  Omit to autodetect.  Sometimes needed
	 *   e.g. for IMF/WLF files where the tempo is based on this type code,
	 *   and cannot be autodetected.
	 *
	 * @return Promise, song is ready to play when it is resolved.
	 */
	setSong(content, url, typecode) {
		var self = this;
		this.failed = false;
		if (!typecode) typecode = '';
		if (this.playing) {
			this.playbackStop();
		}
		this.updatePlayTime(0);

		this.jspb = new Module.JSPlayback(LGMPlayer.audioCtx.sampleRate, 2, 16);
		if (!this.jspb.identify(content, typecode, url)) {
			var e = {
				message: this.jspb.lastError,
			};
			// Show the error now so a user calling setSong() doesn't have to handle
			// the error.
			this.showError(e);
			return Promise.reject(e);
		}
		var suppList = this.jspb.getSupps();
		var suppReqs = new Array();
		for (var s in suppList) {
			console.log('[LGMPlayer] Loading', s, 'suppdata from', suppList[s]);
			var p = this.download(suppList[s]).then(suppContent => {
				self.jspb.setSupp(s, suppContent);
			});
			suppReqs.push(p);
		};

		// Wait until all the supp files have been downloaded and set
		return Promise.all(suppReqs).then(() => {
			self.onSuppsReady();
		}).catch(e => {
			self.showError(e);
		});
	}

	/// Change the play/pause icon to an error and set the message as a tooltip.
	showError(e) {
		this.failed = true;
		console.warn('[LGMPlayer]', e.message || e);
		this.ctPlay.classList.remove('lgmLoading');
		this.ctPlay.firstChild.nodeValue = 'error';
		this.ctPlay.title = e.message;
	}

	/// Callback for when the supplementary files have finished downloading.
	onSuppsReady() {
		// Read the song data
		if (!this.jspb.open()) {
			this.showError({
				message: this.jspb.lastError,
			});
			return;
		}
		this.ctTotalTime.firstChild.nodeValue = this.msToTime(this.jspb.msLength);
		this.ready = true;
		this.ctPlay.classList.remove('lgmLoading');
		this.ctPlay.firstChild.nodeValue = 'play_arrow';
		this.ctPlay.title = ''; // clear any error tooltip
		this.last_msPlayback = 0;

		// Call the element's onload handler if there is one now the song is ready
		// to play.
		if (this.elPlayer.onload) this.elPlayer.onload(this.elPlayer);
	}

	initLibgamemusic() {
		var self = this;
		return new Promise((resolve, reject) => {
			self.tryInitLibgamemusic(resolve, reject);
		});
	}

	tryInitLibgamemusic(resolve, reject) {
		var self = this;

		// Extra safety check to ensure JS is loaded
		if (!window.Module || !Module.JSPlayback) {
			if (this.failCount < 3) {
				this.failCount++;
				console.log('Player asm.js not loaded?!  Will try again shortly.');
				setTimeout(() => {
					self.tryInitLibgamemusic(resolve, reject);
				}, 500 * this.failCount);
				return;
			}
			// Too many failures
			console.log('Unable to access asm.js code after ' + this.failCount
				+ ' attempts');
			reject({
				message: 'Player asm.js code did not load!',
			});
			return;
		}

		if (!LGMPlayer.setBuffer) {
			LGMPlayer.setBuffer = Module.cwrap(
				'c_setBuffer',
				'void',
				['number', 'number']
			);
		}
		resolve();
	}

	/// Play/pause button clicked.
	onPlayPause() {
		if (this.failed) {
			// Try the download again
			this.downloadSrcSong();
		}
		if (!this.ready) return;
		if (this.playing) {
			this.playbackStop();
		} else {
			this.playbackStart();
		}
	}

	/// Event handler for when the progress bar is clicked.
	onSeek(ev) {
		if (!this.ready) return;

		// Figure out where the click was along the progress bar
		var fraction = ev.offsetX / this.ctProgressBG.offsetWidth;

		// Shrink it a bit so clicking close to the start or end will hit the very
		// start or end.
		fraction = fraction * 1.01 - 0.01;
		fraction = Math.min(Math.max(fraction, 0), 1);

		// Figure out what time this equates to, and seek there
		var msTarget = this.jspb.msLength * fraction;
		this.seekToMs(msTarget);
	}

	/// Seek to the given position, in milliseconds.
	/**
	 * @param int msTarget
	 *   Number of milliseconds into the song to seek to.  0 returns to the start
	 *   of the song.  Note that the seeking is approximate and will usually seek
	 *   to the nearest row.
	 */
	seekToMs(msTarget) {
		this.jspb.seek(msTarget);
		this.last_msPlayback = msTarget;
	}

	/// Get the song length, in milliseconds.
	getSongLength() {
		if (!this.ready) return 0;
		return this.jspb.msLength;
	}

	/// Start playback.
	playbackStart() {
		if (this.playing) return;

		this.playing = true;
		this.ctPlay.firstChild.nodeValue = 'pause';

		var self = this;
		this.source = LGMPlayer.audioCtx.createBufferSource();

		this.scriptNode = LGMPlayer.audioCtx.createScriptProcessor(8192, 2, 2);

		// Allocate a buffer to hold the float data.  This is because we can't pass
		// buffers to the C++ code (so we can't pass the Web Audio's buffer in), so
		// instead we allocate a fixed buffer in the C++ heap (which is also
		// accessible from JS) and when the time comes, we just copy from this
		// buffer into the Web Audio buffers.
		let lenSamples = this.scriptNode.bufferSize;
		let lenChan = lenSamples * Float32Array.BYTES_PER_ELEMENT;
		let lenBuf = lenChan * 2;
		this.ptrBuffer = Module._malloc(lenBuf);

		// Tell the C++ code where this new buffer is.  It's passed in as a raw
		// byte buffer.  Unfortunately because we can't pass raw pointers with
		// embind, we have to pass it to a C function that stores it as a global
		// variable, and then we call a C++ function to pick off that global and
		// store it in the class instance.  Very dodgy, let's hope we're always
		// single-threaded.
		var ptr_u8 = new Uint8Array(Module.HEAPU8.buffer, this.ptrBuffer, lenBuf);
		this.constructor.setBuffer(ptr_u8.byteOffset);
		self.jspb.grabBuffer();

		// Create two views of the same data in float format, one each for the left
		// and right channels, as this is the way the Web Audio API wants it,
		// because for some reason it's different to every other audio API.
		var ptr_f32_l = new Float32Array(ptr_u8.buffer, ptr_u8.byteOffset, lenSamples);
		var ptr_f32_r = new Float32Array(ptr_u8.buffer, ptr_u8.byteOffset+lenChan, lenSamples);

		this.scriptNode.onaudioprocess = function(audioProcessingEvent) {
			var b = audioProcessingEvent.outputBuffer;

			var c0 = b.getChannelData(0);
			var c1 = b.getChannelData(1);
			self.updatePlayTime(self.last_msPlayback);
			self.last_msPlayback = self.jspb.fillBuffer(c0.length);

			if (self.jspb.pos.end) {
				self.jspb.loop();
			}
			c0.set(ptr_f32_l);
			c1.set(ptr_f32_r);
		}

		// When the buffer source stops playing, disconnect everything
		this.source.onended = function() {
			self.source.disconnect(self.scriptNode);
			self.scriptNode.disconnect(LGMPlayer.audioCtx.destination);
		}

		this.source.connect(this.scriptNode);
		this.scriptNode.connect(LGMPlayer.audioCtx.destination);
		this.source.start();
	}

	/// Pause playback.
	playbackStop() {
		if (!this.playing) return;

		this.source.disconnect(this.scriptNode);
		this.scriptNode.disconnect(LGMPlayer.audioCtx.destination);
		this.source = null;
		this.scriptNode = null;
		Module._free(this.ptrBuffer);

		this.playing = false;
		this.ctPlay.firstChild.nodeValue = 'play_arrow';
	}

	updatePlayTime(msPlayback) {
		this.ctPlayTime.firstChild.nodeValue = this.msToTime(msPlayback);
		if (!this.jspb) {
			// Will get called when first manually setting a file
			this.setProgress(0);
		} else {
			this.setProgress(msPlayback / this.jspb.msLength);
		}
	}

	/// Set the position of the progress bar.
	/**
	 * @param float n
	 *   Position, 0 for the start, 0.5 for the middle, 1.0 for the end.
	 */
	setProgress(n) {
		n = Math.min(Math.max(n, 0), 1);
		this.ctProgressNeedle.style.width = (n * 100) + '%';
	}
};

// Get the path of this JS file and use it as the prefix to load the other
// associated files.
LGMPlayer.urlPrefix = document.currentScript.src.substring(0,
	document.currentScript.src.lastIndexOf('/') + 1);

// For local testing, don't use a prefix or the XHR requests for file:// fail.
if (LGMPlayer.urlPrefix.substr(0, 7) == 'file://') LGMPlayer.urlPrefix = '';

// Tell Emscripten to load the .mem data file from the same prefix too.
var Module = {
	memoryInitializerPrefixURL: LGMPlayer.urlPrefix,
};

// Limit of 5 audio contexts in Chrome, so use a single global one.
LGMPlayer.audioCtx = new AudioContext();

document.addEventListener('DOMContentLoaded', function() {
	LGMPlayer.createPlayers();
}, true);
