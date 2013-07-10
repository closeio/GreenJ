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

#include <QFile>
#include "Config.h"
#include "Sound.h"
#include "LogHandler.h"


//-----------------------------------------------------------------------------
Sound::Sound()
{
    // Don't create the pool here yet, since it will crash on Windows.
}

//-----------------------------------------------------------------------------
Sound::~Sound()
{
    // The following code might crash if the Sip destructor is called before
    // this gets called. Since we're shutting down the app anyway, we'll just
    // not deallocate the resources and have the OS do it for us.
    /*
    stop();
    if (snd_port_) {
        pjmedia_snd_port_destroy(snd_port_);
        snd_port_ = NULL;
    }

    if (pool_)
        pj_pool_release(pool_);
    */
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
    pj_status_t status;

    if (!pool_) {
        pool_ = pjsua_pool_create("wav", 512, 512);
    }

    if (pool_ && ringFilename.length()) {
        status = pjmedia_wav_player_port_create(pool_, /* memory pool */
                                                ringFilename.toUtf8().data(), /* file to play */
                                                20, /* ptime. */
                                                0, /* flags */
                                                0, /* default buffer */
                                                &file_port_ /* returned port */
                                                );
        
        if (status != PJ_SUCCESS) {
            LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "pjsip", status, "Error in pjmedia_wav_player_port_create"));
            return;
        }

        
        
        if (!snd_port_) {
            status = pjmedia_snd_port_create_player(pool_, /* pool */
                                                    device_,
                                                    PJMEDIA_PIA_SRATE(&file_port_->info),/* clock rate. */
                                                    PJMEDIA_PIA_CCNT(&file_port_->info),/* # of channels. */
                                                    PJMEDIA_PIA_SPF(&file_port_->info), /* samples per frame. */
                                                    PJMEDIA_PIA_BITS(&file_port_->info),/* bits per sample. */
                                                    0, /* options */
                                                    &snd_port_ /* returned port */
                                                    );
            if (status != PJ_SUCCESS) {
                LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "pjsip", status, "Failed to create player"));
                return;
            }
        }
        
        status = pjmedia_snd_port_connect(snd_port_, file_port_);
        if (status != PJ_SUCCESS) {
            LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "pjsip", status, "Failed to play"));
            return;
        }
    } else {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "pjsip", status, "No ringtone set"));
    }
}

//-----------------------------------------------------------------------------
void Sound::startDial()
{
}

//-----------------------------------------------------------------------------
void Sound::setSoundDevice(const int device)
{
    if (device != device_) {
        device_ = device;
        if (snd_port_) {
            pjmedia_snd_port_destroy(snd_port_);
            snd_port_ = NULL;
        }
    }
}

//-----------------------------------------------------------------------------
void Sound::stop()
{
    // If we destroy the sound port here, we crash for some reason when accepting a call.
    
    if (file_port_) {
        pjmedia_port_destroy(file_port_);
        file_port_ = NULL;
    }
}
