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

#include "../../LogHandler.h"
#include "../Phone.h"
#include "Sip.h"
#include "Sound.h"

// From QTextDocument
QString escape(const QString& plain)
{
    QString rich;
    rich.reserve(int(plain.length() * 1.1));
    for (int i = 0; i < plain.length(); ++i) {
        if (plain.at(i) == QLatin1Char('<'))
            rich += QLatin1String("&lt;");
        else if (plain.at(i) == QLatin1Char('>'))
            rich += QLatin1String("&gt;");
        else if (plain.at(i) == QLatin1Char('&'))
            rich += QLatin1String("&amp;");
        else
            rich += plain.at(i);
    }
    return rich;
}


namespace phone
{
    namespace api
    {

Sip *Sip::self_;

//-----------------------------------------------------------------------------
Sip::Sip()
{
    self_ = this;
    defaultSoundInput_ = -1;
    defaultSoundOutput_ = -1;
    started_ = false;
    setupLogging_ = false;
    sipLogPath_ = "";
    account_id_ = -1;
}

//-----------------------------------------------------------------------------
Sip::~Sip()
{
    pjsua_destroy();
}

//-----------------------------------------------------------------------------
void Sip::setLogging(QString path) {
    sipLogPath_ = path;
    if (started_ && !path.isEmpty()) {
        QByteArray pathBytes = path.toUtf8();
        pjsua_logging_config log_cfg;
        pjsua_logging_config_default(&log_cfg);
        if (setupLogging_) {
            // Only overwrite the log when we set up logging the first time.
            log_cfg.log_file_flags = PJ_O_APPEND;
        }
        log_cfg.log_filename = pj_str(pathBytes.data());
        log_cfg.decor |= PJ_LOG_HAS_CR;
        pjsua_reconfigure_logging(&log_cfg);
        setupLogging_ = true;
    }
}

//-----------------------------------------------------------------------------
bool Sip::isInitialized() const {
    return started_;
}

//-----------------------------------------------------------------------------
bool Sip::init(const Settings &settings)
{
    if (started_) {
        return true;
    }

    // Create pjsua first
    pj_status_t status = pjsua_create();
    if (status != PJ_SUCCESS) {
        signalLog(LogInfo(LogInfo::STATUS_FATAL, "pjsip", status, "Creating pjsua application failed"));
        return false;
    }

    // Init pjsua
    if (!_initPjsua(settings.stun_server_, settings.use_ice_)) {
        return false;
    }

    // Add transport
    if (!_addTransport(settings.transport_, settings.port_)) {
        return false;
    }
    
    // Initialization is done, now start pjsua
    status = pjsua_start();
    if (status != PJ_SUCCESS) {
        signalLog(LogInfo(LogInfo::STATUS_FATAL, "pjsip", status, "Couldn't start pjsua"));
        return false;
    }

    pjsua_conf_adjust_rx_level(0, settings.sound_level_);
    pjsua_conf_adjust_tx_level(0, settings.micro_level_);
    
    started_ = true;

    // Easiest
    setLogging(sipLogPath_);
    
    return true;
}

//-----------------------------------------------------------------------------
bool Sip::_initPjsua(const QString &stun, bool ice)
{
    pjsua_config cfg;
    pjsua_logging_config log_cfg;
    pjsua_media_config media_cfg;
    pjsua_config_default(&cfg);
    pjsua_media_config_default(&media_cfg);
    
    media_cfg.ec_options = PJMEDIA_ECHO_SPEEX;
    
    media_cfg.enable_ice = ice;

    // TODO: additional configurations
    // * max_calls
    // * nameserver_count, nameserver (instead of default pj_gethostbyname)
    // * outbound_proxy_cnt, outbound_proxy
    // * add/remove codecs

    if (stun.size()) {
        if (stun.size() > 99) {
            signalLog(LogInfo(LogInfo::STATUS_ERROR, "pjsip", 0, "Couldn't initialize pjsip: Stun server string too long"));
            return false;
        }
        char ch_stun[100];
        strcpy(ch_stun, stun.toLocal8Bit().data());
        cfg.stun_srv[cfg.stun_srv_cnt++] = pj_str(ch_stun);
    }
    cfg.enable_unsolicited_mwi = PJ_FALSE;
    cfg.cb.on_incoming_call = &incomingCallCb;
    cfg.cb.on_call_state = &callStateCb;
    cfg.cb.on_call_media_state = &callMediaStateCb;
    cfg.cb.on_reg_state = &registerStateCb;

    pjsua_logging_config_default(&log_cfg);
    log_cfg.console_level = 4;

    pj_status_t status = pjsua_init(&cfg, &log_cfg, &media_cfg);
    if (status != PJ_SUCCESS) {
        signalLog(LogInfo(LogInfo::STATUS_FATAL, "pjsip", status, "pjsua initialization failed"));
        return false;
    }

    return true;
}
        
Transport Sip::getTransport() const {
    return transport_;
}

bool Sip::deinit()
{
    unregister();
    started_ = false;
    return pjsua_destroy();
}
        
//-----------------------------------------------------------------------------
// TODO: make it nicer to switch between different transport types (UDP, TCP, TLS)
bool Sip::_addTransport(Transport transport, unsigned int port)
{
    pjsua_transport_config cfg;
    pjsua_transport_id transport_id = -1;
    pjsua_transport_config tcp_cfg;

    pjsua_transport_config_default(&cfg);
    cfg.port = port;

    // TODO: TLS transport
    
    pj_status_t status;
    
    if (transport == TRANSPORT_TCP || transport == TRANSPORT_AUTO) {
        // Add TCP transport.
        status = pjsua_transport_create(PJSIP_TRANSPORT_TCP, &cfg, NULL);
        if (status == PJ_SUCCESS) {
            transport_ = TRANSPORT_TCP;
        } else {
            signalLog(LogInfo(LogInfo::STATUS_WARNING, "pjsip", status, "TCP transport creation failed"));
            // Don't return, try UDP.
        }
    }
    if (status != PJ_SUCCESS || transport == TRANSPORT_UDP) {
        status = pjsua_transport_create(PJSIP_TRANSPORT_UDP, &cfg, &transport_id);
        if (status == PJ_SUCCESS) {
            transport_ = TRANSPORT_UDP;
        } else {
            signalLog(LogInfo(LogInfo::STATUS_FATAL, "pjsip", status, "UDP Transport creation failed"));
            return false;
        }
    }

    /*if (cfg.port == 0) {
        pjsua_transport_info ti;
        pj_sockaddr_in *a;

        pjsua_transport_get_info(transport_id, &ti);
        a = (pj_sockaddr_in*)&ti.local_addr;

        tcp_cfg.port = pj_ntohs(a->sin_port);
    }*/


    return true;
}

//-----------------------------------------------------------------------------
int Sip::registerUser(const QString &user, const QString &password, const QString &domain)
{
    if (!started_) {
        signalLog(LogInfo(LogInfo::STATUS_ERROR, "pjsip", 0, "SIP is not initialized"));
        return -1;
    }
    
    if (pjsua_acc_is_valid(account_id_)) {
        signalLog(LogInfo(LogInfo::STATUS_WARNING, "pjsip", 0, "Account already exists"));
        return -2;
    }

    QString id = "sip:" + user + "@" + domain;
    QString uri = "sip:" + domain;
    
    if (transport_ == TRANSPORT_TCP) {
        uri += ";transport=tcp";
    }

    if (id.size() > 149
        || uri.size() > 99
        || user.size() > 99
        || password.size() > 99
        || domain.size() > 99)
    {
        signalLog(LogInfo(LogInfo::STATUS_ERROR, "pjsip", 0, "Error adding account: Invalid data"));
        return -3;
    }

    char cid[150], curi[100], cuser[100], cpassword[100], cdomain[100];
    strcpy(cid, id.toLocal8Bit().constData());
    strcpy(curi, uri.toLocal8Bit().constData());
    strcpy(cuser, user.toLocal8Bit().constData());
    strcpy(cpassword, password.toLocal8Bit().constData());
    strcpy(cdomain, domain.toLocal8Bit().constData());

    // Register to SIP server by creating SIP account.
    pjsua_acc_config cfg;
    pjsua_acc_config_default(&cfg);

    cfg.id = pj_str(cid);
    cfg.reg_uri = pj_str(curi);
    cfg.cred_count = 1;
    cfg.cred_info[0].realm = pj_str((char*)"*");
    //cfg.cred_info[0].realm = pj_str(cdomain);
    cfg.cred_info[0].scheme = pj_str("digest");
    cfg.cred_info[0].username = pj_str(cuser);
    cfg.cred_info[0].data_type = PJSIP_CRED_DATA_PLAIN_PASSWD;
    cfg.cred_info[0].data = pj_str(cpassword);
    cfg.allow_contact_rewrite = PJ_FALSE;

    pj_status_t status = pjsua_acc_add(&cfg, PJ_TRUE, &account_id_);
    if (status != PJ_SUCCESS) {
        signalLog(LogInfo(LogInfo::STATUS_ERROR, "pjsip", status, "Error adding account"));
        return -4;
    }
    signalLog(LogInfo(LogInfo::STATUS_MESSAGE, "pjsip", 0, 
                      "Registering user with account-id " + QString::number(account_id_)));
    
    signalLog(LogInfo(LogInfo::STATUS_DEBUG, "pjsip", 0, 
                      "Registration details: user:'" + QString(cuser)
                                        + "' domain:'" + QString(cdomain)
                                        + "' realm:'*"
                                        + "' uri:'" + QString(curi)
                                        + "' id:'" + QString(cid) + "'"));

    return account_id_;
}

//-----------------------------------------------------------------------------
bool Sip::checkAccountStatus()
{
    return started_ && pjsua_acc_is_valid(account_id_);
}

//-----------------------------------------------------------------------------
void Sip::unregister()
{
    if (pjsua_acc_is_valid(account_id_)) {
        hangUpAll();
        pjsua_acc_del(account_id_);
        signalLog(LogInfo(LogInfo::STATUS_MESSAGE, "pjsip", 0, "Account unregistered"));
    }
}

//-----------------------------------------------------------------------------
void Sip::getAccountInfo(QVariantMap &account_info)
{
    if (!pjsua_acc_is_valid(account_id_)) {
        signalLog(LogInfo(LogInfo::STATUS_WARNING, "pjsip", 0, "Account is not active"));
        return;
    }
    pjsua_acc_info ai;
    pjsua_acc_get_info(account_id_, &ai);

    account_info.insert("address", escape(ai.acc_uri.ptr));
    account_info.insert("status", escape(ai.status_text.ptr));
    account_info.insert("online_status", escape(ai.online_status_text.ptr));
}

//-----------------------------------------------------------------------------
void Sip::incomingCallCb(pjsua_acc_id acc_id, pjsua_call_id call_id,
                         pjsip_rx_data *rdata)
{
    pjsua_call_info ci;

    PJ_UNUSED_ARG(acc_id);

    pjsua_call_get_info(call_id, &ci);
    
    const pjsip_msg *msg = rdata->msg_info.msg;
    const pjsip_hdr *hdr = msg->hdr.next, *end = &msg->hdr;
    
    QVariantMap header_map;
    
    for (; hdr!=end; hdr = hdr->next) {
        if (hdr->name.slen > 2 && pj_strnicmp2(&hdr->name, "x-", 2) == 0) {
            pjsip_generic_string_hdr *string_hdr = (pjsip_generic_string_hdr *)hdr;
            
            QByteArray key_bytes(hdr->name.ptr, hdr->name.slen);
            QByteArray value_bytes(string_hdr->hvalue.ptr, string_hdr->hvalue.slen);
            
            QString key(key_bytes);
            QString value(value_bytes);
            
            header_map.insert(key, value);
        }
    }

    self_->signalRingSound();

    self_->signalLog(LogInfo(LogInfo::STATUS_MESSAGE, "pjsip", 0, "Incoming call from " +
                             QString(ci.remote_contact.ptr)));

    self_->signalIncomingCall(call_id, QString(ci.remote_contact.ptr), QString(ci.remote_info.ptr), header_map);
}

//-----------------------------------------------------------------------------
void Sip::callStateCb(pjsua_call_id call_id, pjsip_event *e)
{
    pjsua_call_info ci;

    PJ_UNUSED_ARG(e);

    pjsua_call_get_info(call_id, &ci);

    if (ci.state == PJSIP_INV_STATE_CONFIRMED
        || ci.state == PJSIP_INV_STATE_DISCONNECTED) 
    {
        self_->signalStopSound();
    }
    if (ci.state == PJSIP_INV_STATE_DISCONNECTED) {
        // Get a call dump now before we hang up.
        const QString dump = self_->getCallDump(call_id);
        self_->signalCallDump(call_id, dump);

        self_->hangUp(call_id);
    }

    self_->signalLog(LogInfo(LogInfo::STATUS_DEBUG, "pjsip", 0,
                             "State of call " + QString::number(call_id)
                             + " changed to " + QString::number(ci.state)));

    self_->signalCallState(call_id, ci.state, ci.last_status);
}

//-----------------------------------------------------------------------------
void Sip::callMediaStateCb(pjsua_call_id call_id)
{
    pjsua_call_info ci;

    pjsua_call_get_info(call_id, &ci);
    
    if (ci.media_status == PJSUA_CALL_MEDIA_ACTIVE) {
        // When media is active, connect call to sound device
        pjsua_conf_connect(ci.conf_slot, 0);
        pjsua_conf_connect(0, ci.conf_slot);
    }
    self_->signalLog(LogInfo(LogInfo::STATUS_DEBUG, "pjsip", 0,
                             "Media state of call " + QString::number(call_id)
                             + " changed to " + QString::number(ci.state)));
}

//-----------------------------------------------------------------------------
void Sip::registerStateCb(pjsua_acc_id acc_id)
{
    PJ_UNUSED_ARG(acc_id);
    pjsua_acc_info acc_info;

    pjsua_acc_get_info(self_->account_id_, &acc_info);

    QString msg("\t");
    msg.append(acc_info.status_text.ptr);
    if (acc_info.status < 300) {
        self_->signalLog(LogInfo(LogInfo::STATUS_MESSAGE, "pjsip-account", acc_info.status, msg));
    } else {
        self_->signalLog(LogInfo(LogInfo::STATUS_ERROR, "pjsip-account", acc_info.status, msg));
    }
    self_->signalAccountState(acc_info.status);
}
                
//-----------------------------------------------------------------------------
int Sip::makeCall(const QString &url)
{
    QVariantMap header_map;
    return makeCall(url, header_map);
}


//-----------------------------------------------------------------------------
int Sip::makeCall(const QString &url, const QVariantMap &header_map)
{
    if (!started_) { return -1; }

    if (url.size() > 149) {
        signalLog(LogInfo(LogInfo::STATUS_ERROR, "pjsip", 0, "Error making call: url too long"));
        return -1;
    }

    signalLog(LogInfo(LogInfo::STATUS_MESSAGE, "pjsip", 0, "Make call"));

    char ch_url[150];
    strcpy(ch_url, url.toLocal8Bit().constData());
    pj_str_t uri = pj_str(ch_url);
    pjsua_call_id call_id;
    pjsua_msg_data msg_data;
    pjsua_msg_data_init(&msg_data);
    
    pj_pool_t *pool = NULL;
    
    if (header_map.size()) {
        QMapIterator<QString, QVariant> it(header_map);
        
        pool = pjsua_pool_create("tmp", 512, 512);
        
        pj_list_init(&msg_data.hdr_list);
        
        while (it.hasNext()) {
            it.next();
            
            pjsip_generic_string_hdr *hdr;
            
            pj_str_t hname, hvalue;
            
            QByteArray name = it.key().toUtf8(), value = it.value().toString().toUtf8();
            hname.ptr = name.data();
            hname.slen = name.size();
            
            hvalue.ptr = value.data();
            hvalue.slen = value.size();
            
            hdr = pjsip_generic_string_hdr_create(pool, &hname, &hvalue);
            
            pj_list_push_back(&msg_data.hdr_list, hdr);
        }        
    }

    pj_status_t status = pjsua_call_make_call(account_id_, &uri, 0, NULL, &msg_data, &call_id);
    if (status != PJ_SUCCESS) {
        signalLog(LogInfo(LogInfo::STATUS_ERROR, "pjsip", status, "Error making call"));
        return -1;
    }
    
    if (pool) {
        pj_pool_release(pool);
    }
    
    return (int)call_id;
}

//-----------------------------------------------------------------------------
void Sip::answerCall(int call_id, int code)
{
    pjsua_call_info ci;
    pjsua_call_get_info(call_id, &ci);

    if (ci.state == PJSIP_INV_STATE_INCOMING || ci.state == PJSIP_INV_STATE_EARLY) {
        pjsua_call_answer((pjsua_call_id)call_id, code, NULL, NULL);
        signalLog(LogInfo(LogInfo::STATUS_DEBUG, "pjsip", ci.state, 
                          "Call " + QString::number(call_id) + " answered"));
    } else {
        signalLog(LogInfo(LogInfo::STATUS_ERROR, "pjsip", ci.state, 
                          "Call " + QString::number(call_id) + " is not an incoming call"));
    }

    if (code >= 200) {
        signalStopSound();
    }
}

//-----------------------------------------------------------------------------
void Sip::hangUp(const int call_id)
{
    signalLog(LogInfo(LogInfo::STATUS_DEBUG, "psjip", 0, "Hangup call " + QString::number(call_id)));

    pjsua_call_info ci;
    pjsua_call_get_info(call_id, &ci);

    pjsua_call_hangup(call_id, 0, 0, 0);

    signalStopSound();
}

//-----------------------------------------------------------------------------
void Sip::hangUpAll()
{
    pjsua_call_hangup_all();
    
    signalStopSound();
}

//-----------------------------------------------------------------------------
bool Sip::addCallToConference(const int call_src, const int call_dest)
{
    if (call_src == -1 || call_dest == -1) {
        signalLog(LogInfo(LogInfo::STATUS_ERROR, "pjsip", 0, "Error: Conference calls are not valid"));
        return false;
    }

    pjsua_call_info src_ci, dest_ci;

    pjsua_call_get_info(call_src, &src_ci);
    pjsua_call_get_info(call_dest, &dest_ci);

    pj_status_t status =  pjsua_conf_connect(src_ci.conf_slot, dest_ci.conf_slot);
    if (status != PJ_SUCCESS) {
        signalLog(LogInfo(LogInfo::STATUS_ERROR, "pjsip", status, "Error connecting conference"));
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
bool Sip::removeCallFromConference(const int call_src, const int call_dest)
{
    if (call_src == -1 || call_dest == -1) {
        signalLog(LogInfo(LogInfo::STATUS_ERROR, "pjsip", 0, "Error: Conference calls are not valid"));
        return false;
    }

    pjsua_call_info src_ci, dest_ci;

    pjsua_call_get_info(call_src, &src_ci);
    pjsua_call_get_info(call_dest, &dest_ci);

    pj_status_t status =  pjsua_conf_disconnect(src_ci.conf_slot, dest_ci.conf_slot);
    if (status != PJ_SUCCESS) {
        signalLog(LogInfo(LogInfo::STATUS_ERROR, "pjsip", status, "Error disconnecting conference"));
        return false;
    }
    return true;
}

//-----------------------------------------------------------------------------
int Sip::redirectCall(const int call_id, const QString &dest_uri)
{
    pjsua_msg_data msg_data;
    pjsua_msg_data_init(&msg_data);
    pj_str_t str = pj_str((char*)dest_uri.toLocal8Bit().data());

    return pjsua_call_xfer(call_id, &str, NULL);
}

//-----------------------------------------------------------------------------
QString Sip::getCallUrl(const int call_id)
{
    pjsua_call_info ci;
    pjsua_call_get_info(call_id, &ci);
    return QString(ci.remote_contact.ptr);
}

//-----------------------------------------------------------------------------
void Sip::getCallInfo(const int call_id, QVariantMap &call_info)
{
    pjsua_call_info ci;
    pjsua_call_get_info(call_id, &ci);
    
    call_info.insert("id", call_id);
    call_info.insert("address", ci.remote_contact.ptr);
    call_info.insert("number", ci.remote_info.ptr);
    call_info.insert("stateText", ci.state_text.ptr);
    call_info.insert("state", (int)ci.state);
    call_info.insert("lastStatus", ci.last_status_text.ptr);
    call_info.insert("duration", (int)ci.connect_duration.sec);
}

//-----------------------------------------------------------------------------
QString Sip::getCallDump(const int call_id)
{
    char buf[8192];
    if (pjsua_call_dump(call_id, TRUE, buf, sizeof(buf), "  ") == PJ_SUCCESS)
        return QString(buf);
    else
        return "";
}

//-----------------------------------------------------------------------------
void Sip::setSoundSignal(const float soundLevel, const int call_id)
{
    QString call;
    pjsua_conf_port_id slot = 0;
    if (call_id >= 0) {
        pjsua_call_info ci;
        pjsua_call_get_info(call_id, &ci);
        slot = ci.conf_slot;
        call = "call " + QString::number(call_id) + " ";
    }
    signalLog(LogInfo(LogInfo::STATUS_DEBUG, "pjsip", 0, call + "sound level: " + QString::number(soundLevel)));
    pjsua_conf_adjust_rx_level(slot, soundLevel);
    signalSoundLevel(int(soundLevel * 255));
}

//-----------------------------------------------------------------------------
void Sip::setMicroSignal(const float microLevel, const int call_id)
{
    QString call;
    pjsua_conf_port_id slot = 0;
    if (call_id >= 0) {
        pjsua_call_info ci;
        pjsua_call_get_info(call_id, &ci);
        slot = ci.conf_slot;
        call = "call " + QString::number(call_id) + " ";
    }
    signalLog(LogInfo(LogInfo::STATUS_DEBUG, "pjsip", 0, call + "micro level: " + QString::number(microLevel)));
    pjsua_conf_adjust_tx_level(slot, microLevel);
    signalMicroLevel(int(microLevel * 255));
}

//-----------------------------------------------------------------------------
void Sip::getSignalLevels(QVariantMap &levels, const int call_id)
{
    unsigned int tx_level, rx_level;
    pjsua_conf_port_id slot = 0;
    if (call_id >= 0) {
        pjsua_call_info ci;
        pjsua_call_get_info(call_id, &ci);
        slot = ci.conf_slot;
    }
    pjsua_conf_get_signal_level(slot, &tx_level, &rx_level);
    levels.insert("sound", rx_level);
    levels.insert("micro", tx_level);
}

//-----------------------------------------------------------------------------
void Sip::setCodecPriority(const QString &codec, int new_priority)
{
    pj_str_t id;
    pj_status_t status;
    
    if (!started_) { return; }

    if (new_priority < 0) {
        new_priority = 0;
    } else if (new_priority > PJMEDIA_CODEC_PRIO_HIGHEST) {
        new_priority = PJMEDIA_CODEC_PRIO_HIGHEST;
    }

    status = pjsua_codec_set_priority(pj_cstr(&id, codec.toLocal8Bit().data()), (pj_uint8_t)new_priority);
    if (status != PJ_SUCCESS) {
        signalLog(LogInfo(LogInfo::STATUS_DEBUG, "pjsip", 0, "Error " + QString::number(status) + " setting codec priority"));
    }
}

//-----------------------------------------------------------------------------
void Sip::getCodecPriorities(QVariantMap &codecs)
{
    pjsua_codec_info codec[32];
    unsigned i, codec_count = PJ_ARRAY_SIZE(codec);
    
    if (!started_) { return; }

    pjsua_enum_codecs(codec, &codec_count);
    for (i=0; i < codec_count; i++) {
        codecs.insert(QString(codec[i].codec_id.ptr), codec[i].priority);
    }
}

//-----------------------------------------------------------------------------
void Sip::setDefaultSoundDevice(const int input, const int output) {
    defaultSoundInput_ = input;
    defaultSoundOutput_ = output;
}

bool Sip::setSoundDeviceStrings(const QString input, const QString output, const QString ring) {
    soundInputString_ = input;
    soundOutputString_ = output;
    ringOutputString_ = ring;
    return selectSoundDevices();
}
        
//-----------------------------------------------------------------------------
bool Sip::setSoundDevice(const int input, const int output) {
    if (!started_) { return false; }
    
    pj_status_t status = pjsua_set_snd_dev(
                                           input == -1 ? defaultSoundInput_ : input,
                                           output == -1 ? defaultSoundOutput_ : output);
    
    signalSoundDeviceChanged();
    return (status == PJ_SUCCESS);
}


//-----------------------------------------------------------------------------
bool Sip::setSoundDevice(const int input, const int output, const int ring) {
    if (!started_) { return false; }
    
    bool ret = setSoundDevice(input, output);

    Sound::getInstance().setSoundDevice(ring == -1 ? defaultSoundOutput_ : ring);
    
    return ret;
}

//-----------------------------------------------------------------------------
bool Sip::selectSoundDevices() {
    if (!started_) { return false; }

    unsigned dev_count = pjmedia_aud_dev_count();
    pj_status_t status;
    int input = -1,
        output = -1,
        ring = -1;
    
    for (unsigned i=0; i<dev_count; ++i) {
        pjmedia_aud_dev_info info;
        
        status = pjmedia_aud_dev_get_info(i, &info);
        
        if (status != PJ_SUCCESS)
            continue;
        
        if (info.input_count > 0 && soundInputString_ == info.name) {
            input = i;
        }
        if (info.output_count > 0 && soundOutputString_ == info.name) {
            output = i;
        }
        if (info.output_count > 0 && ringOutputString_ == info.name) {
            ring = i;
        }
    }

    return setSoundDevice(input, output, ring);
}

//-----------------------------------------------------------------------------
void Sip::getSoundDevices(QVariantList &device_list)
{
    unsigned dev_count = pjmedia_aud_dev_count();
    pj_status_t status;
    
#ifdef Q_WS_MAC
    QVariantMap device_info;

    device_info.insert("index", -1);
    device_info.insert("name", "System default");
    device_info.insert("input_count", 1);
    device_info.insert("output_count", 1);
    device_info.insert("caps", 0);

    device_list.append(device_info);
#endif
    
    for (unsigned i=0; i<dev_count; ++i) {
        QVariantMap device_info;
        
        pjmedia_aud_dev_info info;
        
        status = pjmedia_aud_dev_get_info(i, &info);
        
        if (status != PJ_SUCCESS)
            continue;

        device_info.insert("index", i);
#ifdef Q_WS_WIN
        if (QString(info.name) == "Wave mapper") {
            device_info.insert("name", "System default");
        } else {
            device_info.insert("name", info.name);
        }
#else
        device_info.insert("name", info.name);
#endif
        device_info.insert("input_count", info.input_count);
        device_info.insert("output_count", info.output_count);
        device_info.insert("caps", info.caps);
        
        device_list.append(device_info);
    }
}
   
//-----------------------------------------------------------------------------
bool Sip::sendDTMFDigits(int call_id, const QString &digits) {
    pj_status_t status;
    QByteArray arr = digits.toUtf8();
    pj_str_t pjDigits = pj_str(arr.data());
    
    // Try to send RFC2833 DTMF first.
    status = pjsua_call_dial_dtmf(call_id, &pjDigits);
    
    if (status != PJ_SUCCESS) {
        const pj_str_t kSIPINFO = pj_str((char *)"INFO");
        
        for (int i = 0; i < digits.length(); ++i)
        {
            pjsua_msg_data messageData;
            pjsua_msg_data_init(&messageData);
            messageData.content_type = pj_str((char *)"application/dtmf-relay");
            QByteArray bodyArr = QString("Signal=%1\r\nDuration=300").arg(digits[i]).toUtf8();
            messageData.msg_body = pj_str((char *)bodyArr.data());
            
            status = pjsua_call_send_request(call_id,
                                             &kSIPINFO,
                                             &messageData);
        }
    }
    
    return (status == PJ_SUCCESS);
}


//-----------------------------------------------------------------------------
// Adapted from Telephone.
void Sip::updateSoundDevices() {
    // Stop sound device and disconnect it from the conference.
    pjsua_set_null_snd_dev();
    
    // Reinit sound device.
    pjmedia_snd_deinit();
    pjmedia_snd_init(pjsua_get_pool_factory());
    
    signalSoundDevicesUpdated();
}

}} // phone::api::
