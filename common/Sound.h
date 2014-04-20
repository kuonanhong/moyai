#pragma once

#include "../fmod/api/inc/fmod.h"

class SoundSystem;

class Sound {
public:
    int id;
	FMOD_SOUND *sound;
	SoundSystem *parent;
	FMOD_CHANNEL *ch;

	float default_volume;    
	Sound( SoundSystem *s);
	void setLoop( bool flag );
	void play();
	void play(float vol);
	void playDistance(float mindist, float maxdist, float dist, float relvol );
	void stop();
	bool isPlaying();
    void setVolume(float v);
};
