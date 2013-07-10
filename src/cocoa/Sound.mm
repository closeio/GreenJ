/****************************************************************************
 **
 ** Copyright (C) 2012 Lorem Ipsum Mediengesellschaft m.b.H.
 **
 ** GNU General Public License
 ** This file may be used under the terms of the GNU General Public License
 ** version 3 as published by the Free Software Foundation and
 ** appearing in the file LICENSE.GPL included in the packaging of this file.
 **
 ****************************************************************************/

#include <Cocoa/Cocoa.h>
#import <CoreAudio/CoreAudio.h>

#include <QFile>

#include "Devices.h"
#include "Config.h"
#include "Sound.h"
#include "LogHandler.h"


//-----------------------------------------------------------------------------
Sound::Sound()
{
    sound_ = nil;
}

//-----------------------------------------------------------------------------
Sound::~Sound()
{    
    stop();

    [sound_ release];
    sound_ = nil;
}

//-----------------------------------------------------------------------------
Sound &Sound::getInstance()
{
    static Sound instance;
    return instance;
}

//-----------------------------------------------------------------------------
void Sound::startRing()
{
    NSString *filename = [NSString stringWithCString:ringFilename.toUtf8().data() encoding:NSUTF8StringEncoding];
    if (sound_) {
        [sound_ stop];
    } else {
        sound_ = [[NSSound alloc] initWithContentsOfFile:filename byReference:YES];
    }
    setSoundDevice(device_);
    [sound_ setLoops:YES];
    [sound_ play];
}

//-----------------------------------------------------------------------------
void Sound::startDial()
{
}

//-----------------------------------------------------------------------------
void Sound::setSoundDevice(const int device)
{
    device_ = device;
    
    if (sound_) {
        [sound_ setPlaybackDeviceIdentifier:nil]; // default

        UInt32 size = 0;
        OSStatus err = noErr;

        // Fetch a pointer to the list of available devices.
        AudioDeviceID *devices = NULL;
        unsigned devicesCount = 0;
        err = GetAudioDevices(&devices, &devicesCount);
        if (err != noErr) {
            return;
        }
        
        if (device < 0 || device >= (int)devicesCount) {
            return;
        }
        
        // PJSIP fetches devices in the same order as GetAudioDevices returns them, so we can look it up directly.
        
        // Get device UID.
        CFStringRef UIDStringRef = NULL;
        size = sizeof(CFStringRef);
        err = AudioDeviceGetProperty(devices[device],
                                     0,
                                     0,
                                     kAudioDevicePropertyDeviceUID,
                                     &size,
                                     &UIDStringRef);
        if ((err == noErr) && (UIDStringRef != NULL)) {
            [sound_ setPlaybackDeviceIdentifier:(NSString *)UIDStringRef];
            CFRelease(UIDStringRef);
        }
        
        free(devices);
    }
}

//-----------------------------------------------------------------------------
void Sound::stop()
{
    [sound_ stop];
}
