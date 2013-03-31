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

//#include <QWebFrame>
#include <QString>
#include "phone/Account.h"
#include "phone/Call.h"
#include "phone/Phone.h"
#include "LogHandler.h"
#include "Config.h"
#include "Sound.h"
#include "JavascriptHandler.h"
#include "json.h"

using phone::Phone;
using phone::Call;
using phone::Account;
namespace Json=QtJson;

const QString JavascriptHandler::OBJECT_NAME = "qt_handler";

//-----------------------------------------------------------------------------
JavascriptHandler::JavascriptHandler(Phone &phone) :
    phone_(phone), js_callback_handler_("")
{
    connect(&phone_, SIGNAL(signalIncomingCall(Call &)),
            this, SLOT(incomingCall(Call &)));
    connect(&phone_, SIGNAL(signalCallState(const int, const int, const int)),
            this, SLOT(callState(const int, const int, const int)));
    connect(&phone_, SIGNAL(signalSoundLevel(int)),
            this, SLOT(soundLevel(int)));
    connect(&phone_, SIGNAL(signalMicrophoneLevel(int)),
            this, SLOT(microphoneLevel(int)));
    connect(&phone_, SIGNAL(signalAccountStateChanged(const int, const int)),
            this, SLOT(accountStateChanged(const int, const int)));
    connect(&phone_, SIGNAL(signalSoundDevicesUpdated()),
            this, SLOT(soundDevicesUpdated()));
}

//-----------------------------------------------------------------------------
void JavascriptHandler::accountStateChanged(const int state, const int event_id) const
{
    evaluateJavaScript("accountStateChanged(" + QString::number(state) + ", " + QString::number(event_id) + ")");
}

//-----------------------------------------------------------------------------
void JavascriptHandler::soundDevicesUpdated() const
{
    evaluateJavaScript("soundDevicesUpdated()");
}

//-----------------------------------------------------------------------------
void JavascriptHandler::callState(const int call_id, const int code, 
                                  const int last_status) const
{
    evaluateJavaScript("callStateChanged(" + QString::number(call_id) + "," 
                                           + QString::number(code) + "," 
                                           + QString::number(last_status) + ")");
}

//-----------------------------------------------------------------------------
void JavascriptHandler::incomingCall(Call &call) const
{
    evaluateJavaScript("incomingCall(" + QString::number(call.getId()) + ",'" 
                                       + call.getUrl() + "','" 
                                       + call.getName() + "',"
                                       + Json::serialize(call.getHeaders()) + ")");
}

//-----------------------------------------------------------------------------
void JavascriptHandler::soundLevel(int level) const
{
    evaluateJavaScript("soundLevel(" + QString::number(level) + ")");
}

//-----------------------------------------------------------------------------
void JavascriptHandler::microphoneLevel(int level) const
{
    evaluateJavaScript("microphoneLevel(" + QString::number(level) + ")");
}

//-----------------------------------------------------------------------------
// Public slots
// These methods will be exposed as JavaScript methods.

//-----------------------------------------------------------------------------
int JavascriptHandler::registerJsCallbackHandler(const QString &handler_name)
{
    js_callback_handler_ = handler_name;
    return 0;
}

//-----------------------------------------------------------------------------
bool JavascriptHandler::checkAccountStatus() const
{
    return phone_.checkAccountStatus();
}

//-----------------------------------------------------------------------------
QVariantMap JavascriptHandler::getAccountInformation() const
{
    return phone_.getAccountInfo();
}

bool JavascriptHandler::initialize(const QVariantMap &settings, int event_id) const
{
    phone::Settings phoneSettings;
    
    QString transport = settings["transport"].toString();
    
    if (transport == "udp") {
        phoneSettings.transport_ = phone::TRANSPORT_UDP;
    } else if (transport == "tcp") {
        phoneSettings.transport_ = phone::TRANSPORT_TCP;
    } else {
        phoneSettings.transport_ = phone::TRANSPORT_AUTO;
    }
    
    phoneSettings.port_ = 0;
    phoneSettings.stun_server_ = settings["stun_host"].toString(); // default: ""
    phoneSettings.use_ice_ = settings["use_ice"].toBool(); // default: false
    phoneSettings.sound_level_ = 1.0f;
    phoneSettings.micro_level_ = 1.0f;
    
    if (event_id) {
        // This initialize is in response to an event ID. It will only be executed once.
        return phone_.reinit(phoneSettings, event_id);
    } else {
        return phone_.init(phoneSettings);
    }
}

bool JavascriptHandler::initialize(const QVariantMap &settings) const {
    return initialize(settings, false);
}

QString JavascriptHandler::getTransport() const {
    phone::Transport t = phone_.getTransport();
    if (t == phone::TRANSPORT_AUTO) {
        return "auto";
    } else if (t == phone::TRANSPORT_TCP) {
        return "tcp";
    } else if (t == phone::TRANSPORT_UDP) {
        return "udp";
    }
    return "";
}

//-----------------------------------------------------------------------------
bool JavascriptHandler::registerToServer(const QString &host, const QString &user_name,
                                         const QString &password) const
{
    LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_DEBUG, "js_handler", 0, "register"));

    Account acc;
    acc.setUsername(user_name);
    acc.setPassword(password);
    acc.setHost(host);

    return phone_.registerUser(acc);
}

//-----------------------------------------------------------------------------
void JavascriptHandler::unregisterFromServer() const
{
    LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_DEBUG, "js_handler", 0, "unregister"));

    phone_.unregister();
}

//-----------------------------------------------------------------------------
int JavascriptHandler::makeCall(const QString &number, const QVariantMap &header_map) const
{
    Call *call = phone_.makeCall(number, header_map);
    if (!call) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "makeCall: failed"));
        return -1;
    }
    LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_DEBUG, "js_handler", 0, "calling " + number));
    return call->getId();
}

//-----------------------------------------------------------------------------
int JavascriptHandler::makeCall(const QString &number) const
{
    Call *call = phone_.makeCall(number);
    if (!call) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "makeCall: failed"));
        return -1;
    }
    LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_DEBUG, "js_handler", 0, "calling " + number));
    return call->getId();
}

//-----------------------------------------------------------------------------
void JavascriptHandler::callAccept(const int call_id, const int code) const
{
    Call *call = phone_.getCall(call_id);
    if (call) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_DEBUG, "js_handler", 0, "accepting call " + QString::number(call_id)));
        call->answerCall(code);
    } else {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "callAccept: Call doesn't exist!"));
    }
}

//-----------------------------------------------------------------------------
void JavascriptHandler::hangup(const int call_id) const
{    
    Call *call = phone_.getCall(call_id);
    if (call) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_DEBUG, "js_handler", 0, "hangup call " + QString::number(call_id)));
        call->hangUp();
    } else {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "Hangup: Call doesn't exist!"));
    }
}

//-----------------------------------------------------------------------------
void JavascriptHandler::hangupAll() const
{
    LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_DEBUG, "js_handler", 0, "hangup all calls"));
    phone_.hangUpAll();
}

//-----------------------------------------------------------------------------
void JavascriptHandler::setLogLevel(const unsigned int log_level) const
{
    LogHandler::getInstance().setLevel(log_level);
}

//-----------------------------------------------------------------------------
QString JavascriptHandler::getCallDump(const int call_id) const
{
    Call *call = phone_.getCall(call_id);
    if (call) {
        return call->getDump();
    }
    return "";
}

//-----------------------------------------------------------------------------
QString JavascriptHandler::getCallUserData(const int call_id) const
{
    Call *call = phone_.getCall(call_id);
    if (call) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_DEBUG, "js_handler", 0, "Get userdata for call " + QString::number(call_id)));
        return call->getUserData();
    }
    LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "getCallUserData: Call doesn't exist!"));
    return "";
}

//-----------------------------------------------------------------------------
void JavascriptHandler::setCallUserData(const int call_id, const QString &data) const
{
    Call *call = phone_.getCall(call_id);
    if (call) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_DEBUG, "js_handler", 0, "Set userdata for call " + QString::number(call_id)));
        return call->setUserData(data);
    } else {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "setCallUserData: Call doesn't exist!"));
    }
}

//-----------------------------------------------------------------------------
QVariantList JavascriptHandler::getErrorLogData() const
{
    LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_DEBUG, "js_handler", 0, "Read error log data"));

    QVariantList log_data;
    QFile file(Phone::ERROR_FILE);
    file.open(QIODevice::ReadOnly);
    QDataStream in(&file);
    while (!in.atEnd()) {
        Call call;
        in >> call;
        log_data << call.getInfo();
    }

    return log_data;
}

//-----------------------------------------------------------------------------
void JavascriptHandler::deleteErrorLogFile() const
{
    LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_DEBUG, "js_handler", 0, "Delete error log file"));

    QFile::remove(Phone::ERROR_FILE);
}

//-----------------------------------------------------------------------------
bool JavascriptHandler::addToConference(const int src_id, const int dst_id) const
{
    Call *call = phone_.getCall(src_id);
    Call *dest_call = phone_.getCall(dst_id);
    if (!call || !dest_call) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "Error: one of the selected calls doesn't exist!"));
        return false;
    }
    if (!call->isActive() || !dest_call->isActive()) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "Error: one of the selected calls isn't active!"));
        return false;
    }
    if (!call->addToConference(*dest_call)) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "Error: failed to connect to source!"));
        return false;
    }
    if (!dest_call->addToConference(*call)) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "Error: failed to connect to destination!"));
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool JavascriptHandler::removeFromConference(const int src_id, const int dst_id) const
{
    Call *call = phone_.getCall(src_id);
    Call *dest_call = phone_.getCall(dst_id);
    if (!call || !dest_call) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "Error: one of the selected calls doesn't exist!"));
        return false;
    }
    if (!call->isActive() || !dest_call->isActive()) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "Error: one of the selected calls isn't active!"));
        return false;
    }
    if (call->removeFromConference(*dest_call)) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "Error: failed to remove from source!"));
        return false;
    }
    if (dest_call->removeFromConference(*call)) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "Error: failed to remove from destination!"));
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
int JavascriptHandler::redirectCall(const int call_id, const QString &dst_uri) const
{
    Call *call = phone_.getCall(call_id);
    if (call) {
        LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_DEBUG, "js_handler", 0, "Redirected call " + QString::number(call_id) + " to " + dst_uri));
        return call->redirect(dst_uri);
    }
    LogHandler::getInstance().log(LogInfo(LogInfo::STATUS_ERROR, "js_handler", 0, "redirectCall: Call doesn't exist!"));
    return -1;
}

//-----------------------------------------------------------------------------
QVariantList JavascriptHandler::getCallList() const
{
    return phone_.getCallList();
}

//-----------------------------------------------------------------------------
QVariantList JavascriptHandler::getActiveCallList() const
{
    return phone_.getActiveCallList();
}

//-----------------------------------------------------------------------------
void JavascriptHandler::muteSound(const bool mute, const int call_id) const
{
    if (call_id < 0) {
        phone_.setSoundSignal(mute ? 0.0f : 1.0f);
    } else {
        Call *call = phone_.getCall(call_id);
        if (call) {
            call->setSoundSignal(mute ? 0.0f : 1.0f);
        }
    }
}

//-----------------------------------------------------------------------------
void JavascriptHandler::muteMicrophone(const bool mute, const int call_id) const
{
    if (call_id < 0) {
        phone_.setMicroSignal(mute ? 0.0f : 1.0f);
    } else {
        Call *call = phone_.getCall(call_id);
        if (call) {
            call->setMicroSignal(mute ? 0.0f : 1.0f);
        }
    }
}

//-----------------------------------------------------------------------------
void JavascriptHandler::setSoundLevel(const int level, const int call_id) const
{
    float flevel = static_cast<float>(level) / 255.f;
    if (flevel > 1.0f) {
        flevel = 1.0f;
    } else if (flevel < 0.0f) {
        flevel = 0.0f;
    }
    
    if (call_id < 0) {
        phone_.setSoundSignal(flevel);
    } else {
        Call *call = phone_.getCall(call_id);
        if (call) {
            call->setSoundSignal(flevel);
        }
    }
}

//-----------------------------------------------------------------------------
void JavascriptHandler::setMicrophoneLevel(const int level, const int call_id) const
{
    float flevel = static_cast<float>(level) / 255.f;
    if (flevel > 1.0f) {
        flevel = 1.0f;
    } else if (flevel < 0.0f) {
        flevel = 0.0f;
    }
    
    if (call_id < 0) {
        phone_.setMicroSignal(flevel);
    } else {
        Call *call = phone_.getCall(call_id);
        if (call) {
            call->setMicroSignal(flevel);
        }
    }
}

//-----------------------------------------------------------------------------
QVariantMap JavascriptHandler::getSignalInformation() const
{
    return phone_.getSignalLevels();
}

//-----------------------------------------------------------------------------
void JavascriptHandler::setCodecPriority(const QString &codec, int new_priority) const
{
    phone_.setCodecPriority(codec, new_priority);
}

//-----------------------------------------------------------------------------
QVariantMap JavascriptHandler::getCodecPriorities() const
{
    return phone_.getCodecPriorities();
}

//-----------------------------------------------------------------------------
void JavascriptHandler::setSoundDevice(const int input, const int output) const
{
    phone_.setSoundDevice(input, output);
}

//-----------------------------------------------------------------------------
void JavascriptHandler::setSoundDevice(const int input, const int output, const int ring) const
{
    phone_.setSoundDevice(input, output);
    Sound::getInstance().setSoundDevice(ring);
}

//-----------------------------------------------------------------------------
QVariantList JavascriptHandler::getSoundDevices() const
{
    return phone_.getSoundDevices();
}

//-----------------------------------------------------------------------------
QVariant JavascriptHandler::getOption(const QString &name) const
{
    return Config::getInstance().getOption(name);
}

//-----------------------------------------------------------------------------
void JavascriptHandler::setOption(const QString &name, const QVariant &option)
{
    Config::getInstance().setOption(name, option);
    if (name == "url") {
        signalWebPageChanged();
    }
}

//-----------------------------------------------------------------------------
bool JavascriptHandler::sendLogMessage(const QVariantMap &log) const
{
    QVariant time   = log["time"];
    QVariant status = log["status"];
    QVariant domain = log["domain"];
    QVariant code   = log["code"];
    QVariant msg    = log["message"];

    if (!time.convert(QVariant::String) || !status.convert(QVariant::UInt)
        || !domain.convert(QVariant::String) || !code.convert(QVariant::Int)
        || !msg.convert(QVariant::String))
    {
        return false;
    }

    LogInfo info((LogInfo::Status)status.toUInt(), domain.toString(), code.toInt(), msg.toString());
    info.time_.fromString(time.toString(), "dd.MM.yyyy hh:mm:ss");

    LogHandler::getInstance().log(info, false);
    return true;
}

//-----------------------------------------------------------------------------
QStringList JavascriptHandler::getLogFileList() const
{
    return LogHandler::getInstance().getFileList();
}   

//-----------------------------------------------------------------------------
QString JavascriptHandler::getLogFileContent(const QString &file_name) const
{
    return LogHandler::getInstance().getFileContent(file_name);
}

//-----------------------------------------------------------------------------
void JavascriptHandler::deleteLogFile(const QString &file_name) const
{
    LogHandler::getInstance().deleteFile(file_name);
}

//-----------------------------------------------------------------------------
void JavascriptHandler::sendDTMFDigits(const int call_id, const QString &digit) const {
    Call *call = phone_.getCall(call_id);
    if (call) {
        call->sendDTMFDigits(digit);
    }
}

//-----------------------------------------------------------------------------
// Private slots
// Will be excluded from JavaScript.

//-----------------------------------------------------------------------------
void JavascriptHandler::slotLogMessage(const LogInfo &info) const
{
    QVariantMap map;
    map["time"] = info.time_.toString("dd.MM.yyyy hh:mm:ss");
    map["status"] = QString::number(info.status_);
    map["domain"] = info.domain_;
    map["code"] = QString::number(info.code_);
    map["message"] = info.msg_;
    evaluateJavaScript("logMessage(" + Json::serialize(map) + ")");
}
