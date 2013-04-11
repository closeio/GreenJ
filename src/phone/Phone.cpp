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
#include <QDataStream>
#include "../LogHandler.h"
#include "../JavascriptHandler.h"
#include "../Sound.h"
#include "api/Interface.h"
#include "Call.h"
#include "Account.h"
#include "Phone.h"

namespace phone
{

const QString Phone::ERROR_FILE = "error.log";

//-----------------------------------------------------------------------------
Phone::Phone(api::Interface *api) : api_(api), event_id_(0)
{
    connect(api_, SIGNAL(signalAccountState(const int)),
            this, SLOT(slotAccountState(const int)));
    connect(api_, SIGNAL(signalIncomingCall(int, QString, QString, QVariantMap)),
            this, SLOT(slotIncomingCall(int, QString, QString, QVariantMap)));
    connect(api_, SIGNAL(signalCallState(int,int,int)),
            this, SLOT(slotCallState(int,int,int)));
    connect(api_, SIGNAL(signalCallDump(int,QString)),
            this, SLOT(slotCallDump(int,QString)));
    connect(api_, SIGNAL(signalSoundLevel(int)),
            this, SLOT(slotSoundLevel(int)));
    connect(api_, SIGNAL(signalMicroLevel(int)),
            this, SLOT(slotMicroLevel(int)));
    connect(api_, SIGNAL(signalLog(const LogInfo&)),
            this, SLOT(slotLogData(const LogInfo&)));
    connect(api_, SIGNAL(signalRingSound()),
            this, SLOT(slotRingSound()));
    connect(api_, SIGNAL(signalStopSound()),
            this, SLOT(slotStopSound()));
    connect(api_, SIGNAL(signalSoundDevicesUpdated()),
            this, SLOT(slotSoundDevicesUpdated()));
}

//-----------------------------------------------------------------------------
Phone::~Phone()
{
//    QFile file(ERROR_FILE);
//    file.open(QIODevice::WriteOnly | QIODevice::Append);
//    QDataStream out(&file);
    for (int i = 0; i < calls_.size(); i++) {
        Call *call = calls_[i];
        if (call && call->isActive()) {
//            out << *call;
        }
        delete call;
    }
    calls_.clear();
}

//-----------------------------------------------------------------------------
bool Phone::init(const Settings &settings)
{
    if (api_->init(settings)) {
        api_->updateSoundDevices();
        api_->selectSoundDevices();
        return true;
    }
    return false;
}

Transport Phone::getTransport() {
    return api_->getTransport();
}

bool Phone::reinit(const Settings &settings, int event_id) {
    if (event_id_ == event_id) {
        event_id_++;
        api_->deinit();
        return init(settings);
    }
    return false;
}

//-----------------------------------------------------------------------------
api::Interface *Phone::getApi()
{
    return api_;
}

//-----------------------------------------------------------------------------
const QString &Phone::getErrorMessage() const
{
    return error_msg_;
}

//-----------------------------------------------------------------------------
bool Phone::checkAccountStatus() const
{
    return api_->checkAccountStatus();
}

//-----------------------------------------------------------------------------
bool Phone::registerUser(const Account &acc)
{
    if (api_->registerUser(acc.getUsername(), acc.getPassword(), acc.getHost()) == -1) {
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
QVariantMap Phone::getAccountInfo() const
{
    QVariantMap info;
    api_->getAccountInfo(info);
    return info;
}

//-----------------------------------------------------------------------------
bool Phone::addToCallList(Call *call)
{
    for (int i = 0; i < calls_.size(); i++) {
        if (calls_[i] == call) {
            return true;
        }
        if (calls_[i]->getId() == call->getId()) {
            if (calls_[i]->isActive()) {
                return false;
            } else {
                delete calls_[i];
                calls_[i] = call;
                return true;
            }
        }
    }

    calls_.push_back(call);
    return true;
}

//-----------------------------------------------------------------------------
Call *Phone::makeCall(const QString &url)
{
    Call *call = new Call(this, Call::TYPE_OUTGOING);
    if (call->makeCall(url) < 0 || !addToCallList(call)) {
        delete call;
        call = NULL;
    }
    return call;
}
    
//-----------------------------------------------------------------------------
Call *Phone::makeCall(const QString &url, const QVariantMap &header_map)
{
    Call *call = new Call(this, Call::TYPE_OUTGOING);
    if (call->makeCall(url, header_map) < 0 || !addToCallList(call)) {
        delete call;
        call = NULL;
    }
    return call;
}

//-----------------------------------------------------------------------------
void Phone::hangUpAll()
{
    api_->hangUpAll();
    for (int i = 0; i < calls_.size(); i++) {
        calls_[i]->setInactive();
    }
}

//-----------------------------------------------------------------------------
Call *Phone::getCall(const int call_id)
{
    for (int i = 0; i < calls_.size(); i++) {
        if (calls_[i]->getId() == call_id) {
            return calls_[i];
        }
    }
    return NULL;
}

//-----------------------------------------------------------------------------
QVariantList Phone::getCallList() const
{
    QVariantList list;
    for (int i = 0; i < calls_.size(); ++i) {
        Call *call = calls_[i];
        int id = call->getId();
        if (call->getStatus() != Call::STATUS_CLOSED) {
            QVariantMap current;
            current.insert("id", id);
            current = call->getInfo();
            list << current;
        }
    }
    return list;
}

//-----------------------------------------------------------------------------
QVariantList Phone::getActiveCallList() const
{
    QVariantList list;
    for (int i = 0; i < calls_.size(); ++i) {
        Call *call = calls_[i];
        int id = call->getId();
        if (call->isActive()) {
            QVariantMap current;
            current.insert("id", id);
            current = call->getInfo();
            list << current;
        }
    }
    return list;
}

//-----------------------------------------------------------------------------
void Phone::setSoundSignal(const float soundLevel)
{
    api_->setSoundSignal(soundLevel);
}

//-----------------------------------------------------------------------------
void Phone::setMicroSignal(const float microLevel)
{
    api_->setMicroSignal(microLevel);
}

//-----------------------------------------------------------------------------
QVariantMap Phone::getSignalLevels() const
{
    QVariantMap info;
    api_->getSignalLevels(info);
    return info;
}

//-----------------------------------------------------------------------------
void Phone::setCodecPriority(const QString &codec, int new_priority)
{
    api_->setCodecPriority(codec, new_priority);
}

//-----------------------------------------------------------------------------
void Phone::setSoundDevice(const int input, const int output)
{
    api_->setSoundDevice(input, output);
}
    
//-----------------------------------------------------------------------------
void Phone::setSoundDeviceStrings(const QString input, const QString output, const QString ring)
{
    api_->setSoundDeviceStrings(input, output, ring);
}

//-----------------------------------------------------------------------------
QVariantList Phone::getSoundDevices() const
{
    QVariantList device_list;
    api_->getSoundDevices(device_list);
    return device_list;
}

//-----------------------------------------------------------------------------
QVariantMap Phone::getCodecPriorities() const
{
    QVariantMap codecs;
    api_->getCodecPriorities(codecs);
    return codecs;
}

//-----------------------------------------------------------------------------
void Phone::unregister()
{
    api_->unregister();
}

//-----------------------------------------------------------------------------
void Phone::slotIncomingCall(int call_id, const QString &url, const QString &name, const QVariantMap &header_map)
{
    Call *call = new Call(this, Call::TYPE_INCOMING);
    call->setId(call_id);
    call->setUrl(url);
    call->setName(name);
    call->setHeaders(header_map);

    if (!addToCallList(call)) {
        delete call;
        return;
    }

    call->answerCall(180); /* Ringing */

    signalIncomingCall(*call);
}

//-----------------------------------------------------------------------------
void Phone::slotCallState(int call_id, int call_state, int last_status)
{
    Call *call = getCall(call_id);
    if (call) {
        call->setState(call_state);
    }
    
    signalCallState(call_id, call_state, last_status);
}
    
//-----------------------------------------------------------------------------
void Phone::slotCallDump(int call_id, QString dump)
{
    Call *call = getCall(call_id);
    if (call) {
        call->setDump(dump);
    }
}

//-----------------------------------------------------------------------------
void Phone::slotSoundLevel(int level)
{
    signalSoundLevel(level);
}

//-----------------------------------------------------------------------------
void Phone::slotMicroLevel(int level)
{
    signalMicrophoneLevel(level);
}

//-----------------------------------------------------------------------------
void Phone::slotAccountState(const int state)
{
    signalAccountStateChanged(state, ++event_id_);
}

//-----------------------------------------------------------------------------
void Phone::slotLogData(const LogInfo &info)
{
    if (info.status_ >= LogInfo::STATUS_ERROR) {
        error_msg_ = info.msg_;
    }
    LogHandler::getInstance().log(info);
}

//-----------------------------------------------------------------------------
void Phone::slotRingSound()
{
    Sound::getInstance().startRing();
}

//-----------------------------------------------------------------------------
void Phone::slotStopSound()
{
    Sound::getInstance().stop();
}
    
//-----------------------------------------------------------------------------
void Phone::slotSoundDevicesUpdated()
{
    signalSoundDevicesUpdated();
}

//-----------------------------------------------------------------------------
void Phone::sendDTMFDigits(int call_id, const QString &digits)
{
    Call *call = getCall(call_id);
    if (call) {
        call->sendDTMFDigits(digits);
    }
}

    
} // phone::
