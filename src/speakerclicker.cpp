/*
    epple2
    Copyright (C) 2008 by Christopher A. Mosher <cmosher01@gmail.com>

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY, without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program. If not, see <http://www.gnu.org/licenses/>.
*/
#include "speakerclicker.h"
#include <SDL2/SDL.h>
#include <iostream>
#include <ostream>
#include <deque>

std::deque<char> locbuf;
volatile bool done;
SDL_cond* bufchg;
SDL_mutex* buflck;
volatile int silence;

// TODO figure out why sound is so choppy, and fix it
void fillbuf(void *userdata, Uint8 *stream, int len)
{
	int tot(0);
	while (len && !done)
	{
		// wait for locbuf to have some data
		// (it gets filled up by tick() method)
		SDL_LockMutex(buflck);
		while (locbuf.empty() && !done)
		{
			SDL_CondWait(bufchg,buflck);
		}
		if (done)
		{
			SDL_UnlockMutex(buflck);
			return;
		}
		while (locbuf.size() > 0 && len > 0)
		{
			int v = locbuf.front();
			*stream++ = v;

			if (v < 0) v = -v;
			tot += v;

			--len;
			locbuf.pop_front();
		}
		if (tot <= 0)
		{
			if (locbuf.size() >= 1024)
				locbuf.clear();
		}
		else
		{
			if (locbuf.size() >= 65536)
				locbuf.clear();
		}
		SDL_UnlockMutex(buflck);
	}
}

#define TICKS_PER_SAMPLE 21

SpeakerClicker::SpeakerClicker():
	clicked(false),
	t(TICKS_PER_SAMPLE)
{
	done = false;

	SDL_AudioSpec audio;
	audio.freq = 48000; // samples per second
	audio.format = AUDIO_U8; // 8 bits (1 byte) per sample
	audio.channels = 1; // mono
	audio.silence = 0;
	audio.samples = 1024;
	audio.callback = fillbuf;
	audio.userdata = 0;
	SDL_AudioSpec obtained;
	obtained.callback = 0;
	obtained.userdata = 0;

 	if (SDL_OpenAudio(&audio,&obtained) != 0)
	{
		done = true;
		std::cerr << "Unable to initialize audio: " << SDL_GetError() << std::endl;
	}
	else
	{
		bufchg = SDL_CreateCond();
		buflck = SDL_CreateMutex();
		silence = obtained.silence;

//		std::cout << "initialized audio: " << std::endl;
//		std::cout << "freq: " << obtained.freq << std::endl;
//		std::cout << "format: " << obtained.format << std::endl;
//		std::cout << "channels: " << (int)obtained.channels << std::endl;
//		std::cout << "silence: " << (int)obtained.silence << std::endl;
//		std::cout << "samples: " << obtained.samples << std::endl;
//		std::cout << "size: " << obtained.size << std::endl;

		SDL_PauseAudio(0);
	}
}


SpeakerClicker::~SpeakerClicker()
{
	if (done)
	{
		return;
	}
	done = true;
	SDL_LockMutex(buflck);
	SDL_CondSignal(bufchg);
	SDL_UnlockMutex(buflck);
	SDL_CloseAudio();
	SDL_DestroyCond(bufchg);
	SDL_DestroyMutex(buflck);
}

void SpeakerClicker::tick()
{
	if (done)
	{
		return;
	}

	--this->t;
	if (this->t <= 0)
	{
		this->t = TICKS_PER_SAMPLE;
		int amp;
		if (this->clicked)
		{
			amp = this->positive ? silence+100 : silence-100;
			this->positive = !this->positive;
			this->clicked = false;
		}
		else
		{
			amp = silence;
		}
		SDL_LockMutex(buflck);
		locbuf.push_back(amp);
		SDL_CondSignal(bufchg);
		SDL_UnlockMutex(buflck);
	}
}

void SpeakerClicker::click()
{
	this->clicked = true;
}
