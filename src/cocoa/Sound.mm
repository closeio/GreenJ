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
#include "Config.h"
#include "Sound.h"
#include "LogHandler.h"


// Gets audio devices data.
static OSStatus GetAudioDevices(Ptr *devices, UInt16 *devicesCount) {
    OSStatus err = noErr;
    UInt32 size;
    Boolean isWritable;
    
    // Get sound devices count.
    err = AudioHardwareGetPropertyInfo(kAudioHardwarePropertyDevices,
                                       &size,
                                       &isWritable);
    if (err != noErr) {
        return err;
    }
    
    *devicesCount = size / sizeof(AudioDeviceID);
    if (*devicesCount < 1) {
        return err;
    }
    
    // Allocate space for devices.
    *devices = (Ptr)malloc(size);
    memset(*devices, 0, size);
    
    // Get the data.
    err = AudioHardwareGetProperty(kAudioHardwarePropertyDevices,
                                   &size,
                                   (void *)*devices);
    if (err != noErr) {
        return err;
    }
    
    return err;
}



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
        UInt16 devicesCount = 0;
        err = GetAudioDevices((Ptr *)&devices, &devicesCount);
        if (err != noErr) {
            return;
        }
        
        if (device < 0 || device >= devicesCount) {
            return;
        }
        
        // Portaudio fetches devices in the same order as OS X returns them, so we can look it up directly.
        
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
    }
}

//-----------------------------------------------------------------------------
void Sound::stop()
{
    [sound_ stop];
}
