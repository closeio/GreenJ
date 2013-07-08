#ifndef DEVICES_INCLUDE_H
#define DEVICES_INCLUDE_H

#import <CoreAudio/CoreAudio.h>

#ifdef __cplusplus
extern "C" {
#endif

OSStatus GetAudioDevices(AudioDeviceID **dev_ids_ptr, unsigned *dev_count_ptr);
AudioDeviceID GetAudioDeviceIDBySelector(AudioObjectPropertySelector selector);
AudioDeviceID GetAudioDeviceID(int device);

#ifdef __cplusplus
}
#endif

#endif
