// Integration of read-only RAMSES-II OpenTherm discovery, NVS profile and
// conservative RF request scheduling for ESP32 + CC1101. All SPI/UART radio
// operations must execute on the existing dedicated radioTask.
// This is an experimental implementation; RF TX needs bench verification.
#pragma once
#include <Arduino.h>
#include <HardwareSerial.h>
#include <Preferences.h>
#include <WebServer.h>
#include "OpenThermScan.h"
#include "OpenThermOneShot.h"
#include "RamsesReadRequest.h"
#include "Cc1101Radio.h"
#include "GatewayRuntimeConfig.h"
#include "RamsesCodec.h"
#include "RamsesBinding.h"
#include "RamsesPeerDetection.h"
#include "RamsesControllerIds.h"

class OpenThermGateway {
 public:
  void configure(const gw_config::Settings* settings,uint16_t band){config_=settings;moduleBand_=band;}
  bool transmitConfigured() const {
    return config_ && config_->otTxEnabled == 1 &&
        config_->otBridge > 0 && config_->otBridge <= 262143 &&
        config_->otSource > 0 && config_->otSource < 262143 &&
        config_->otSource != 730 &&
        config_->rfExperimentalRx == 0 &&
        radio_frequency::evohomeTxFrequency(config_->rfFrequencyKhz,
                                              moduleBand_);
  }
  bool transmitEnabled() const { return transmitConfigured() && bridgeSeen_; }
  // One-shot exception NEVER arms the existing scan/poll TX path.
  bool experimentalOneShotReady() const {
    if(!config_ || config_->otTxEnabled!=0 || config_->otAutoPoll!=0 ||
       config_->otScanAll!=0 || config_->otBridge==0 ||
       config_->otBridge>262143 || !bridgeSeen_)return false;
    portENTER_CRITICAL(&mutex_);
    const bool verifiedBinding=binding_.state==ramses_bind::VERIFIED &&
                               !binding_.virtualIdCollision;
    portEXIT_CRITICAL(&mutex_);
    // A verified 01: controller does not require the separate 18: source ID.
    // Only the unpaired cross-band probe uses otSource/sourceConflict.
    return radio_frequency::manualReadOneShotWithIdentity(
       config_->rfFrequencyKhz,config_->rfModuleBand,
       config_->rfExperimentalRx==1,verifiedBinding,
       config_->otSource,sourceConflict_);
  }
  bool bindingReady() const {
    // Read the bounded identity-coverage flag under the same mutex as note().
    // requestBinding also checks eligible() under that lock, so no stale
    // readiness snapshot can permit a collision-prone transmission.
    if(!config_)return false;
    portENTER_CRITICAL(&mutex_);
    const bool coverageExhausted=seenControllers_.saturated();
    portEXIT_CRITICAL(&mutex_);
    if(!bridgeSeen_ || !observedController_ || coverageExhausted ||
       config_->otTxEnabled || config_->otAutoPoll || config_->otScanAll ||
       !config_->otBridge || sourceConflict_)return false;
    return radio_frequency::evohomeTxFrequency(
       config_->rfFrequencyKhz,config_->rfModuleBand) ||
       radio_frequency::experimentalOneShotFrequency(
       config_->rfFrequencyKhz,config_->rfModuleBand,
       config_->rfExperimentalRx==1);
  }
  // Human-readable, fail-closed preflight reason for the manual binding UI.
  // This is observational only and NEVER authorizes RF by itself.
  // The radio task still rechecks bindingReady() before transmitting.
  const char* bindingBlockReason() const {
    if(!config_)return "configuration_unavailable";
    if(!config_->otBridge || config_->otBridge>262143)
      return "configure_bridge_id";
    if(!bridgeSeen_)return "bridge_not_observed";
    if(!observedController_)return "evohome_controller_not_observed";
    if(config_->otTxEnabled)return "automatic_tx_enabled";
    if(config_->otAutoPoll)return "automatic_polling_enabled";
    if(config_->otScanAll)return "full_scan_enabled";
    if(sourceConflict_)return "gateway_source_collision";
    const bool standard=radio_frequency::evohomeTxFrequency(
      config_->rfFrequencyKhz,config_->rfModuleBand);
    const bool experimental=radio_frequency::experimentalOneShotFrequency(
      config_->rfFrequencyKhz,config_->rfModuleBand,
      config_->rfExperimentalRx==1);
    if(!standard && !experimental)return "radio_configuration_not_supported";
    portENTER_CRITICAL(&mutex_);
    const bool saturated=seenControllers_.saturated();
    const bool bindingBusy=binding_.busy();
    const bool probeBusy=singleProbe_.state==ot_single::QUEUED ||
                         singleProbe_.state==ot_single::WAITING;
    const bool cooling=binding_.cooldown(millis())>0;
    portEXIT_CRITICAL(&mutex_);
    if(saturated)return "controller_id_coverage_exhausted";
    if(work_.active)return "opentherm_scan_active";
    if(requested_)return "opentherm_scan_requested";
    if(probeBusy)return "opentherm_one_shot_active";
    if(bindingBusy)return "pairing_already_running";
    if(cooling)return "pairing_cooldown";
    return nullptr;
  }
  bool requestBinding(uint32_t virtualControllerId){
    if(bindingBlockReason()!=nullptr || !bindingReady())return false;
    portENTER_CRITICAL(&mutex_);
    const bool blocked=singleProbe_.state==ot_single::QUEUED ||
       singleProbe_.state==ot_single::WAITING;
    const bool ok=!blocked && !binding_.busy() &&
       seenControllers_.eligible(virtualControllerId) &&
       virtualControllerId!=observedController_ &&
       binding_.start(millis(),virtualControllerId,config_->otBridge,
                      observedController_);
    portEXIT_CRITICAL(&mutex_);
    return ok;
  }
  void sendBindingStatus(WebServer& server,bool radioReady) {
    ramses_bind::Session snap;ot_tx::Report detail;
    portENTER_CRITICAL(&mutex_);
    snap=binding_;detail=bindingTx_;
    const bool exhausted=seenControllers_.saturated();
    const unsigned tracked=seenControllers_.count;
    portEXIT_CRITICAL(&mutex_);
    const char* reason=radioReady?bindingBlockReason():"radio_not_ready";
    const bool mayStart=(reason==nullptr);
    char body[832];
    snprintf(body,sizeof(body),
      "{\"state\":\"%s\",\"ready\":%s,\"block_reason\":\"%s\",\"radio_ready\":%s,\"bridge_seen\":%s,"
      "\"bridge_id\":%lu,\"existing_controller_id\":%lu,"
      "\"virtual_controller_id\":%lu,\"cooldown_ms\":%lu,"
      "\"offer_accepted\":%s,\"confirmation_sent\":%s,"
      "\"verified_reply\":%s,\"read_ack\":%s,"
      "\"id_conflict\":%s,\"raw\":%u,"
      "\"controller_id_coverage_exhausted\":%s,\"controller_ids_seen\":%u,"
      "\"tx_phase\":\"%s\",\"tx_bytes\":%u}",
      ramses_bind::name(snap.state),mayStart?"true":"false",
      reason?reason:"",radioReady?"true":"false",
      bridgeSeen_?"true":"false",
      static_cast<unsigned long>(config_->otBridge),
      static_cast<unsigned long>(observedController_),
      static_cast<unsigned long>(snap.virtualId),
      static_cast<unsigned long>(snap.cooldown(millis())),
      snap.acceptedOffer?"true":"false",
      snap.confirmationSent?"true":"false",
      snap.state==ramses_bind::VERIFIED?"true":"false",
      snap.verifiedReadAck?"true":"false",
      snap.virtualIdCollision?"true":"false",
      static_cast<unsigned>(snap.lastRaw),
      exhausted?"true":"false",tracked,
      ot_tx::phaseName(detail.phase),
      static_cast<unsigned>(detail.written_bytes));
    server.sendHeader("Cache-Control","no-store");
    server.send(200,"application/json",body);
  }
  bool requestOneShot(ot_tx::Method method) {
    if(!experimentalOneShotReady() || work_.active || requested_ || binding_.busy())return false;
    portENTER_CRITICAL(&mutex_);
    const bool ok=singleProbe_.request(millis(),method);
    portEXIT_CRITICAL(&mutex_);
    return ok;
  }
  void sendOneShotStatus(WebServer& server) {
    ot_single::Probe snapshot;
    ot_tx::Report details[2];
    ot_tx::Comparison stats[2];
    portENTER_CRITICAL(&mutex_);
    snapshot=singleProbe_;
    for(unsigned i=0;i<2;i++) {
      details[i]=lastReports_[i];
      stats[i]=comparisons_[i];
    }
    portEXIT_CRITICAL(&mutex_);
    const ot_tx::Report& d=details[snapshot.method];
    float degrees=0;
    const bool hasValue=snapshot.state==ot_single::READ_ACK &&
      ot_readonly::fixedPoint(ot_single::DATA_ID,snapshot.raw,degrees);
    char response[2100];
    snprintf(response,sizeof(response),
      "{\"data_id\":25,\"name\":\"boiler_flow_c\",\"status\":\"%s\","
      "\"ready\":%s,\"bridge_seen\":%s,\"source_conflict\":%s,"
      "\"bridge_id\":%lu,\"source_id\":%lu,\"cooldown_ms\":%lu,"
      "\"attempted\":%s,\"raw\":%u,\"value_c\":%.3f,\"has_value\":%s,"
      "\"tx_method\":\"%s\",\"tx_phase\":\"%s\",\"tx_local_complete\":%s,"
      "\"tx_expected_bytes\":%u,\"tx_written_bytes\":%u,\"tx_elapsed_us\":%lu,"
      "\"tx_pa_programmed\":%u,\"tx_pa_readback\":%u,"
      "\"tx_marc_before\":%u,\"tx_marc_during\":%u,\"tx_marc_after\":%u,"
      "\"tx_fifo_before\":%u,\"tx_fifo_after\":%u,"
      "\"tx_rx_restored\":%s,\"tx_saw_tx\":%s,"
      "\"comparison\":["
      "{\"method\":\"async_uart\",\"attempts\":%lu,\"local_completed\":%lu,"
      "\"read_acks\":%lu,\"timeouts\":%lu,\"other_replies\":%lu,\"tx_failures\":%lu,"
      "\"last_phase\":\"%s\",\"last_elapsed_us\":%lu},"
      "{\"method\":\"fifo_reference_primed_60\",\"attempts\":%lu,\"local_completed\":%lu,"
      "\"read_acks\":%lu,\"timeouts\":%lu,\"other_replies\":%lu,\"tx_failures\":%lu,"
      "\"last_phase\":\"%s\",\"last_elapsed_us\":%lu}],"
      "\"caution\":\"Local TX success does not prove over-the-air delivery\"}",
      snapshot.label(),experimentalOneShotReady()?"true":"false",
      bridgeSeen_?"true":"false",sourceConflict_?"true":"false",
      static_cast<unsigned long>(config_->otBridge),
      static_cast<unsigned long>(config_->otSource),
      static_cast<unsigned long>(snapshot.cooldown(millis())),
      snapshot.attempted?"true":"false",static_cast<unsigned>(snapshot.raw),
      static_cast<double>(hasValue?degrees:0),hasValue?"true":"false",
      ot_tx::methodName(snapshot.method),ot_tx::phaseName(d.phase),
      d.phase==ot_tx::LOCAL_COMPLETE?"true":"false",
      static_cast<unsigned>(d.expected_bytes),static_cast<unsigned>(d.written_bytes),
      static_cast<unsigned long>(d.elapsed_us),
      static_cast<unsigned>(d.pa_configured),static_cast<unsigned>(d.pa_readback),
      static_cast<unsigned>(d.marc_before),static_cast<unsigned>(d.marc_during),
      static_cast<unsigned>(d.marc_after),
      static_cast<unsigned>(d.txbytes_before),static_cast<unsigned>(d.txbytes_after),
      d.rx_restored?"true":"false",d.saw_tx?"true":"false",
      static_cast<unsigned long>(stats[0].attempts),
      static_cast<unsigned long>(stats[0].local_completed),
      static_cast<unsigned long>(stats[0].read_acks),
      static_cast<unsigned long>(stats[0].timeouts),
      static_cast<unsigned long>(stats[0].rejected),
      static_cast<unsigned long>(stats[0].tx_failures),
      ot_tx::phaseName(stats[0].last_phase),
      static_cast<unsigned long>(stats[0].last_elapsed_us),
      static_cast<unsigned long>(stats[1].attempts),
      static_cast<unsigned long>(stats[1].local_completed),
      static_cast<unsigned long>(stats[1].read_acks),
      static_cast<unsigned long>(stats[1].timeouts),
      static_cast<unsigned long>(stats[1].rejected),
      static_cast<unsigned long>(stats[1].tx_failures),
      ot_tx::phaseName(stats[1].last_phase),
      static_cast<unsigned long>(stats[1].last_elapsed_us));
    server.sendHeader("Cache-Control","no-store");
    server.send(200,"application/json",response);
  }
  // Only RX-verified packets can establish the bridge identity. A configured
  // RF address alone is not permission to transmit into a heating system.
  void noteDevice(const ramses::Message& msg) {
    if(config_->otBridge && (msg.addrMask&1) &&
       rfType(msg.addresses[0])==10 &&
       rfId(msg.addresses[0])==config_->otBridge)
      bridgeSeen_=true;
    const uint32_t linkedController=
        ramses_peer::controllerLinkedToBridge(msg,config_->otBridge);
    if((msg.addrMask&1u) && rfType(msg.addresses[0])==1) {
      const uint32_t id=rfId(msg.addresses[0]);
      portENTER_CRITICAL(&mutex_);
      seenControllers_.note(id);
      portEXIT_CRITICAL(&mutex_);
    }
    if(linkedController) {
      portENTER_CRITICAL(&mutex_);
      seenControllers_.note(linkedController);
      // An RP-only 3EF0 address is provisional: if an old virtual 01: was
      // once bound and still replies after reboot, a real controller RQ must
      // be allowed to replace that weaker evidence.
      controllerEvidence_.note(msg,config_->otBridge);
      observedController_=controllerEvidence_.id;
      portEXIT_CRITICAL(&mutex_);
    }
    // Check RF *sender* only. Replies addressed to our virtual ID are
    // expected and must never be misclassified as an address collision.
    if((msg.addrMask&1u) && config_->otSource &&
       rfType(msg.addresses[0])==18 &&
       rfId(msg.addresses[0])==config_->otSource)
      sourceConflict_=true;
  }
  void begin() {
    Preferences p;
    if (p.begin("ot-probe",true)) {
      Persist stored = {};
      if (p.getBytesLength("profile")==sizeof(stored) &&
          p.getBytes("profile",&stored,sizeof(stored))==sizeof(stored) &&
          stored.magic==PROFILE_MAGIC &&
          stored.bridgeId==config_->otBridge &&
          stored.frequency==config_->rfFrequencyKhz) {
        work_.bridgeId=stored.bridgeId;
        work_.completed=true;
        for (unsigned i=0;i<256;i++) {
          const uint8_t state=stored.states[i];
          if(state>=ot_readonly::READ_ACK && state<=ot_readonly::TIMEOUT) {
            work_.entries[i].result=static_cast<ot_readonly::Result>(state);
            work_.entries[i].raw=stored.raw[i];
            work_.attempted++;
            if(state==ot_readonly::READ_ACK)work_.acknowledged++;
          }
        }
      }
      p.end();
    }
    publishSnapshot();
  }
  bool requestScan() {
    if(!transmitEnabled() || binding_.busy())return false;
    portENTER_CRITICAL(&mutex_);
    const bool ok=!published_.active && !requested_ && !binding_.busy();
    if(ok)requested_=true;
    portEXIT_CRITICAL(&mutex_);
    return ok;
  }
  // Must be called from the radioTask after a checksum-validated frame.
  bool observe(const ramses::Message& msg,uint8_t& id,
               ot_readonly::Result& r,uint16_t& raw) {
    portENTER_CRITICAL(&mutex_);
    binding_.observe(msg,millis());
    portEXIT_CRITICAL(&mutex_);
    if(msg.opcode!=0x3220 || msg.messageType!=3 ||
       !(msg.addrMask&1) || rfType(msg.addresses[0])!=10 ||
       (config_->otBridge!=0 && rfId(msg.addresses[0])!=config_->otBridge) ||
       !ot_readonly::response(msg.payload,msg.length,id,r,raw))return false;
    // Only attribute scan results to our transaction when the reply
    // explicitly addresses the configured gateway RF identity.
    const bool directedGateway=(msg.addrMask&2) &&
      rfType(msg.addresses[1])==18 &&
      rfId(msg.addresses[1])==config_->otSource;
    const bool directedPairedController=(msg.addrMask&2) &&
      binding_.state==ramses_bind::VERIFIED &&
      rfType(msg.addresses[1])==1 &&
      rfId(msg.addresses[1])==binding_.virtualId;
    if(directedGateway || directedPairedController) {
      portENTER_CRITICAL(&mutex_);
      if(singleProbe_.accept(id,r,raw,millis())) {
        ot_tx::Comparison& summary=comparisons_[singleProbe_.method];
        if(r==ot_readonly::READ_ACK)summary.read_acks++;
        else summary.rejected++;
      }
      portEXIT_CRITICAL(&mutex_);
      if(work_.accept(id,r,raw,millis()))publishSnapshot();
    }
    return true; // Still valid passive RF telemetry from configured bridge.
  }
  // Called from radioTask, never from HTTP/MQTT task.
  void tick(Cc1101Radio& radio,HardwareSerial& port,ramses::FrameDecoder& decoder) {
    bool start=false;
    portENTER_CRITICAL(&mutex_);
    if(requested_){start=true;requested_=false;}
    portEXIT_CRITICAL(&mutex_);
    const uint32_t now=millis();
    bool oneShot=false;
    ot_tx::Method method=ot_tx::ASYNC_UART;
    portENTER_CRITICAL(&mutex_);
    oneShot=singleProbe_.begin(now);
    method=singleProbe_.method;
    const auto previous=singleProbe_.state;
    singleProbe_.tick(now);
    if(previous==ot_single::WAITING && singleProbe_.state==ot_single::TIMEOUT)
      comparisons_[method].timeouts++;
    portEXIT_CRITICAL(&mutex_);
    if(oneShot) {
      ot_tx::Report details;
      details.method=method;
      const bool permitted=experimentalOneShotReady() && !work_.active && !requested_;
      const bool ok=permitted &&
        sendReadRequest(radio,port,decoder,ot_single::DATA_ID,true,method,details);
      if(!permitted)details.phase=ot_tx::FREQUENCY_REJECTED;
      portENTER_CRITICAL(&mutex_);
      const unsigned index=static_cast<unsigned>(method);
      lastReports_[index]=details;
      ot_tx::Comparison& summary=comparisons_[index];
      summary.attempts++;
      summary.last_phase=details.phase;
      summary.last_elapsed_us=details.elapsed_us;
      if(ok)summary.local_completed++;
      else summary.tx_failures++;
      singleProbe_.sent(ok);
      portEXIT_CRITICAL(&mutex_);
    }
    // Only the radio task sends the manually queued 1FC9 handshake.
    ramses_bind::State bindingAction;
    portENTER_CRITICAL(&mutex_);
    binding_.tick(now);
    bindingAction=binding_.state;
    portEXIT_CRITICAL(&mutex_);
    if(bindingAction==ramses_bind::OFFER_QUEUED ||
       bindingAction==ramses_bind::CONFIRM_QUEUED ||
       bindingAction==ramses_bind::VERIFY_QUEUED) {
      uint8_t frame[64]={}; size_t length=0;
      ramses_bind::Session snap;
      portENTER_CRITICAL(&mutex_);
      snap=binding_;
      portEXIT_CRITICAL(&mutex_);
      bool built=false;
      if(bindingAction==ramses_bind::OFFER_QUEUED)
        built=ramses_bind::makeOffer(snap.virtualId,snap.bridgeId,frame,length);
      else if(bindingAction==ramses_bind::CONFIRM_QUEUED)
        built=ramses_bind::makeConfirmation(snap.virtualId,snap.bridgeId,frame,length);
      else {
        uint8_t verify[16]={};
        built=ramses_bind::makeVerifyRead(snap.virtualId,snap.bridgeId,verify,length);
        if(built)memcpy(frame,verify,length);
      }
      ot_tx::Report tx;
      const bool sent=built && bindingReady() &&
         sendBindingFrame(radio,port,decoder,frame,length,tx);
      portENTER_CRITICAL(&mutex_);
      bindingTx_=tx;
      if(bindingAction==ramses_bind::OFFER_QUEUED)
        binding_.offered(sent,millis());
      else if(bindingAction==ramses_bind::CONFIRM_QUEUED)
        binding_.confirmed(sent);
      else binding_.verifiedRequestSent(sent,millis());
      portEXIT_CRITICAL(&mutex_);
    }
    if(start) {
      work_.reset(config_->otBridge,config_ && config_->otScanAll==1,now);
      wasCompleted_=false;
      publishSnapshot();
    }
    if(work_.active) {
      const int id=work_.nextRequest(now);
      if(id>=0){
        if(!transmit(radio,port,decoder,static_cast<uint8_t>(id)))
          txFailures_++; // on failure, ID eventually becomes timeout, never "unsupported".
        publishSnapshot();
      }
    } else if(work_.completed && !wasCompleted_) {
      wasCompleted_=true;
      save();
      publishSnapshot();
    }
    // Normal polling is opt-in, derived ONLY from observed READ_ACK IDs.
    if(config_ && config_->otAutoPoll==1 && !work_.active && work_.completed &&
       static_cast<int32_t>(now-nextPolling_)>=0 && transmitEnabled()) {
      nextPolling_=now+60000; // at most one request per minute
      for(unsigned attempts=0;attempts<256;attempts++){
        const uint8_t id=pollCursor_++;
        if(work_.autoPoll(id)){
          if(!transmit(radio,port,decoder,id))txFailures_++;
          break;
        }
      }
    }
  }
  bool copyProfile(ot_scan::Scanner& copy) {
    portENTER_CRITICAL(&mutex_);
    copy=published_;
    portEXIT_CRITICAL(&mutex_);
    return copy.completed;
  }
  // Stream JSON instead of buffering all tested IDs in ESP32 heap.
  void sendProfile(WebServer& server) {
    ot_scan::Scanner copy;
    portENTER_CRITICAL(&mutex_);
    copy=published_;
    const bool requestPending=requested_;
    portEXIT_CRITICAL(&mutex_);
    server.sendHeader("Cache-Control","no-store");
    server.setContentLength(CONTENT_LENGTH_UNKNOWN);
    server.send(200,"application/json","");
    char header[240];
    snprintf(header,sizeof(header),
      "{\"device\":\"R8810A\",\"bridge_id\":%lu,\"frequency_khz\":%lu,"
      "\"module_band\":%u,\"rf_tx_enabled\":%s,\"scan_active\":%s,"
      "\"scan_pending\":%s,\"scan_completed\":%s,\"full_scan\":%s,"
      "\"attempted\":%u,\"read_ack\":%u,\"tx_failures\":%lu,\"results\":[",
      static_cast<unsigned long>(config_->otBridge),
      static_cast<unsigned long>(config_->rfFrequencyKhz),
      static_cast<unsigned>(moduleBand_),
      transmitEnabled()?"true":"false",copy.active?"true":"false",
      requestPending?"true":"false",copy.completed?"true":"false",
      copy.full?"true":"false",copy.attempted,copy.acknowledged,
      static_cast<unsigned long>(txFailures_));
    server.sendContent(header);
    bool first=true;
    for (unsigned i=0;i<256;i++) {
      if(copy.entries[i].result==ot_readonly::NOT_TESTED)continue;
      if(!first)server.sendContent(",");
      first=false;
      const ot_readonly::Entry& e=copy.entries[i];
      char line[224];
      const bool enabled=copy.autoPoll(static_cast<uint8_t>(i));
      float value=0;
      const bool hasValue=e.result==ot_readonly::READ_ACK &&
           ot_readonly::fixedPoint(static_cast<uint8_t>(i),e.raw,value);
      if(hasValue)
        snprintf(line,sizeof(line),
          "{\"id\":%u,\"name\":\"%s\",\"result\":\"%s\","
          "\"raw\":%u,\"value\":%.3f,\"auto_poll\":%s}",
          i,ot_readonly::label(static_cast<uint8_t>(i)),
          ot_readonly::state(e.result),e.raw,
          static_cast<double>(value),enabled?"true":"false");
      else
        snprintf(line,sizeof(line),
          "{\"id\":%u,\"name\":\"%s\",\"result\":\"%s\","
          "\"raw\":%u,\"value\":null,\"auto_poll\":%s}",
          i,ot_readonly::label(static_cast<uint8_t>(i)),
          ot_readonly::state(e.result),e.raw,enabled?"true":"false");
      server.sendContent(line);
    }
    // Machine-readable configuration derived exclusively from observed
    // Read-Ack replies. This is a recommendation, not blanket authorization
    // to transmit: periodic polling is separately opt-in.
    server.sendContent("],\"recommended_poll_ids\":[");
    bool firstEligible=true;
    for(unsigned i=0;i<256;i++) {
      if(!copy.autoPoll(static_cast<uint8_t>(i)))continue;
      if(!firstEligible)server.sendContent(",");
      firstEligible=false;
      server.sendContent(String(i));
    }
    server.sendContent("],\"auto_poll_enabled\":");
    server.sendContent(config_ && config_->otAutoPoll==1?"true}":"false}");
    server.sendContent("");
  }
 private:
  struct Persist {
    uint32_t magic,bridgeId,frequency;
    uint8_t states[256];
    uint16_t raw[256];
  };
  static constexpr uint32_t PROFILE_MAGIC=0x4f543034;
  const gw_config::Settings* config_=nullptr;
  uint16_t moduleBand_=868;
  mutable portMUX_TYPE mutex_=portMUX_INITIALIZER_UNLOCKED;
  ot_scan::Scanner work_,published_;
  bool requested_=false;
  volatile bool bridgeSeen_=false;
  volatile bool sourceConflict_=false;
  ot_single::Probe singleProbe_;
  ot_tx::Report lastReports_[2];
  ot_tx::Comparison comparisons_[2];
  ramses_bind::Session binding_;
  ot_tx::Report bindingTx_;
  volatile uint32_t observedController_=0;
  ramses_peer::ExistingController controllerEvidence_;
  ramses_ids::SeenControllers seenControllers_;
  bool wasCompleted_=false;
  uint32_t nextPolling_=0;
  uint32_t txFailures_=0;
  uint8_t pollCursor_=0;
  static uint8_t rfType(const uint8_t* a){return a[0]>>2;}
  static uint32_t rfId(const uint8_t* a){
    return (static_cast<uint32_t>(a[0]&3u)<<16)|
           (static_cast<uint32_t>(a[1])<<8)|a[2];
  }
  void publishSnapshot() {
    portENTER_CRITICAL(&mutex_);
    published_=work_;
    portEXIT_CRITICAL(&mutex_);
  }
  void save() {
    Persist data={};
    data.magic=PROFILE_MAGIC;
    data.bridgeId=config_->otBridge;
    data.frequency=config_->rfFrequencyKhz;
    for(unsigned i=0;i<256;i++){
      data.states[i]=work_.entries[i].result;
      data.raw[i]=work_.entries[i].raw;
    }
    Preferences p;
    if(p.begin("ot-probe",false)){
      p.putBytes("profile",&data,sizeof(data));
      p.end();
    }
  }
  // Fixed permitted RF frames: offer, confirmation, one Read-Data ID25.
  bool sendBindingFrame(Cc1101Radio& radio,HardwareSerial& port,
                        ramses::FrameDecoder& decoder,
                        const uint8_t* frame,size_t length,
                        ot_tx::Report& diag) {
    uint8_t wire[128]={};
    const size_t wireLen=ramses_read::encodeUART(frame,length,wire,sizeof(wire));
    if(!wireLen || wireLen>sizeof(wire)) {
      diag.phase=ot_tx::TX_CONFIG_FAILED;return false;
    }
    diag.expected_bytes=static_cast<uint16_t>(wireLen);
    const uint32_t begun=micros();
    if(!radio.beginReadRequestTx(true,&diag))return false;
    const size_t written=port.write(wire,wireLen);
    port.flush();
    delayMicroseconds(300);
    const bool restored=radio.endReadRequestTx(&diag);
    while(port.available())port.read();
    decoder.reset();
    diag.written_bytes=static_cast<uint16_t>(written);
    diag.elapsed_us=micros()-begun;
    diag.phase=!restored?ot_tx::RX_RESTORE_FAILED:
      written!=wireLen?ot_tx::UART_WRITE_FAILED:ot_tx::LOCAL_COMPLETE;
    return diag.phase==ot_tx::LOCAL_COMPLETE;
  }
  bool transmit(Cc1101Radio& radio,HardwareSerial& port,
                ramses::FrameDecoder& decoder,uint8_t id){
    if(!transmitEnabled())return false;
    ot_tx::Report diagnostics;
    return sendReadRequest(radio,port,decoder,id,false,ot_tx::ASYNC_UART,diagnostics);
  }
  bool sendReadRequest(Cc1101Radio& radio,HardwareSerial& port,
                ramses::FrameDecoder& decoder,uint8_t id,bool experimental,
                ot_tx::Method method,ot_tx::Report& diag){
    diag=ot_tx::Report{};
    diag.method=method;
    if(experimental) {
      if(id!=ot_single::DATA_ID || !experimentalOneShotReady()) {
        diag.phase=ot_tx::FREQUENCY_REJECTED;return false;
      }
    } else if(!transmitEnabled()) {
      diag.phase=ot_tx::FREQUENCY_REJECTED;return false;
    }
    uint8_t frame[16],wire[44];size_t used=0;
    uint8_t sourceType=18;
    uint32_t sourceId=config_->otSource;
    portENTER_CRITICAL(&mutex_);
    if(binding_.state==ramses_bind::VERIFIED){
      sourceType=1;
      sourceId=binding_.virtualId;
    }
    portEXIT_CRITICAL(&mutex_);
    if(!ramses_read::makeRequestTyped(sourceType,sourceId,
                                     config_->otBridge,id,frame,used)){
      diag.phase=ot_tx::TX_CONFIG_FAILED;return false;
    }
    const size_t wireLen=ramses_read::encodeUART(frame,used,wire,sizeof(wire));
    if(wireLen!=44){diag.phase=ot_tx::TX_CONFIG_FAILED;return false;}
    if(method==ot_tx::REFERENCE_FIFO) {
      if(!experimental) {
        diag.phase=ot_tx::FREQUENCY_REJECTED;return false;
      }
      const bool ok=radio.sendOneShotFifo(wire,wireLen,true,diag);
      while(port.available())port.read();
      decoder.reset();
      return ok;
    }
    diag.expected_bytes=static_cast<uint16_t>(wireLen);
    const uint32_t begun=micros();
    if(!radio.beginReadRequestTx(experimental,&diag))return false;
    const size_t written=port.write(wire,wireLen);
    port.flush();
    delayMicroseconds(300);
    const bool rxOk=radio.endReadRequestTx(&diag);
    while(port.available())port.read();
    decoder.reset();
    diag.written_bytes=static_cast<uint16_t>(written);
    diag.elapsed_us=micros()-begun;
    if(!rxOk)diag.phase=ot_tx::RX_RESTORE_FAILED;
    else if(written!=wireLen)diag.phase=ot_tx::UART_WRITE_FAILED;
    else diag.phase=ot_tx::LOCAL_COMPLETE;
    return diag.phase==ot_tx::LOCAL_COMPLETE;
  }
};
