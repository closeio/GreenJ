#include "Devices.h"

#import <CoreFoundation/CoreFoundation.h>
#import <AudioToolbox/AudioToolbox.h>


// Code based on coreaudio_dev.c in PJSIP
// MUST free(dev_ids) on successful return
OSStatus GetAudioDevices(AudioDeviceID **dev_ids_ptr, unsigned *dev_count_ptr) {
    AudioDeviceID *dev_ids;
    unsigned dev_count;
    OSStatus ostatus;
    UInt32 dev_size, size = sizeof(AudioDeviceID);
    AudioObjectPropertyAddress addr;
    
    addr.mSelector = kAudioHardwarePropertyDevices;
    addr.mScope = kAudioObjectPropertyScopeGlobal;
    addr.mElement = kAudioObjectPropertyElementMaster;
    
    ostatus = AudioObjectGetPropertyDataSize(kAudioObjectSystemObject, &addr, 0, NULL, &dev_size);
    if (ostatus != noErr) {
        return ostatus;
    }
    
    dev_count = dev_size / size;
    
    dev_ids = (AudioDeviceID *)calloc(dev_size, size);
    if (!dev_ids) {
        return -1; // error
    }
    
    bzero(dev_ids, dev_count);
    ostatus = AudioObjectGetPropertyData(kAudioObjectSystemObject, &addr, 0, NULL, &dev_size, (void *)dev_ids);
    if (ostatus != noErr) {
        free(dev_ids);
        return ostatus;
    }
    
    // To be consistent with PJSIP's backend we must put the default audio input/output devices first in the list
    if (dev_size > 1) {
        unsigned i;
        AudioDeviceID dev_id = kAudioObjectUnknown;
        unsigned idx = 0;
        
        /* Find default audio input device */
        addr.mSelector = kAudioHardwarePropertyDefaultInputDevice;
        addr.mScope = kAudioObjectPropertyScopeGlobal;
        addr.mElement = kAudioObjectPropertyElementMaster;
        size = sizeof(dev_id);
        
        ostatus = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                             &addr, 0, NULL,
                                             &size, (void *)&dev_id);
        if (ostatus == noErr && dev_id != dev_ids[idx]) {
            AudioDeviceID temp_id = dev_ids[idx];
            
            for (i = idx + 1; i < dev_size; i++) {
                if (dev_ids[i] == dev_id) {
                    dev_ids[idx++] = dev_id;
                    dev_ids[i] = temp_id;
                    break;
                }
            }
        }
        
        /* Find default audio output device */
        addr.mSelector = kAudioHardwarePropertyDefaultOutputDevice;
        ostatus = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                             &addr, 0, NULL,
                                             &size, (void *)&dev_id);
        if (ostatus == noErr && dev_id != dev_ids[idx]) {
            AudioDeviceID temp_id = dev_ids[idx];
            
            for (i = idx + 1; i < dev_size; i++) {
                if (dev_ids[i] == dev_id) {
                    dev_ids[idx] = dev_id;
                    dev_ids[i] = temp_id;
                    break;
                }
            }
        }
    }

    *dev_ids_ptr = dev_ids;
    *dev_count_ptr = dev_count;
    
    return noErr;
}

// Useful to get the UID of the default input/output device.
AudioDeviceID GetAudioDeviceIDBySelector(AudioObjectPropertySelector selector) {
    AudioDeviceID answer = 0;
    UInt32 size = sizeof(AudioDeviceID);
    AudioObjectPropertyAddress address = {
        selector,
        kAudioObjectPropertyScopeGlobal,
        kAudioObjectPropertyElementMaster
    };

    OSStatus err = AudioObjectGetPropertyData(kAudioObjectSystemObject,
                                                   &address,
                                                   0,
                                                   NULL,
                                                   &size,
                                                   &answer);

    if (err == noErr)
        return answer;
    else
        return kAudioObjectUnknown;
}

// Returns the Apple AudioDeviceID for a given device index to the device ID pointer.
// Returns kAudioObjectUnknown if unsuccessful.
AudioDeviceID GetAudioDeviceID(int device) {
    AudioDeviceID *devices = NULL;
    AudioDeviceID deviceID;
    unsigned devicesCount = 0;
    OSStatus err = noErr;

    err = GetAudioDevices(&devices, &devicesCount);
    if (err != noErr) {
        return kAudioObjectUnknown;
    }
    if (device < 0 || device >= (int)devicesCount) {
        return kAudioObjectUnknown;
    }
    
    // PJSIP fetches devices in the same order as GetAudioDevices returns them, so we can look it up directly.
    deviceID = devices[device];
    free(devices);
    
    return deviceID;
}
