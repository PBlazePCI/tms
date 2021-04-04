/*
* (c) Copyright, Real-Time Innovations, 2012.  All rights reserved.
* RTI grants Licensee a license to use, modify, compile, and create derivative
* works of the software solely for use with RTI Connext DDS. Licensee may
* redistribute copies of the software provided that all such copies are subject
* to this license. The software is provided "as is", with no warranty of any
* type, including any warranty for fitness for any purpose. RTI is under no
* obligation to maintain or support the software. RTI shall not be liable for
* any incidental or consequential damages arising out of the use or inability
* to use the software.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <sstream>
#include <signal.h>
#include <iostream>

#include "ndds/ndds_cpp.h"
#include <pthread.h>
#include "tmsCommon.h"
#include "tmsCommPatterns.h"

#define ECHO_RQST_RESPONSE true

bool run_flag = true;

// should tuck this var into the RequestSequenceNumber class and make that Class a singlton pattern
unsigned long long sequence_number=0; // ever monotonically increasing for each request sent

// Variable associated with Source Transition Request - note the TMS topic struct holds both present and future state
// so we should be able to leverage the state within the topic
// Also a real MSM would need to keep these in arrays for the maximum number of devices allowed on a Microgrid
enum tms_MicrogridMembershipResult internal_membership_result = MMR_UNINITIALIZED;
enum tms_MicrogridMembershipResult external_tms_membership_result = MMR_UNINITIALIZED;  
enum tms_SourceTransition internal_source_transition_state = ST_UNINITIALIZED; 
enum tms_SourceTransition external_tms_source_transition_state = ST_UNINITIALIZED; 

const DDS_Char * const topic_name_array [] = {
    tms_TOPIC_ACTIVE_DIAGNOSTICS,
    tms_TOPIC_AUTHORIZATION_TO_ENERGIZE_OUTCOME,
    tms_TOPIC_AUTHORIZATION_TO_ENERGIZE_REQUEST,
    tms_TOPIC_AUTHORIZATION_TO_ENERGIZE_RESPONSE,
    tms_TOPIC_CONFIG_RESERVATION_STATE,
    tms_TOPIC_COPY_CONFIG_REQUEST,
    tms_TOPIC_DC_DEVICE_POWER_MEASUREMENT_LIST,
    tms_TOPIC_DC_LOAD_SHARING_REQUEST,
    tms_TOPIC_DC_LOAD_SHARING_STATUS,
    tms_TOPIC_DEVICE_ANNOUNCEMENT,
    tms_TOPIC_DEVICE_CLOCK_STATUS,
    tms_TOPIC_DEVICE_GROUNDING,
    tms_TOPIC_DEVICE_GROUNDING_STATUS,
    tms_TOPIC_DEVICE_PARAMETER_REQUEST,
    tms_TOPIC_DEVICE_PARAMETER_STATUS,
    tms_TOPIC_DEVICE_POWER_MEASUREMENT_LIST,
    tms_TOPIC_DEVICE_POWER_PORT_LIST,
    tms_TOPIC_DEVICE_POWER_STATUS_LIST,
    tms_TOPIC_DISCOVERED_CONNECTION_LIST,
    tms_TOPIC_ENGINE_STATE,
    tms_TOPIC_FINGERPRINT_NICKNAME,
    tms_TOPIC_FINGERPRINT_NICKNAME_REQUEST,
    tms_TOPIC_GET_CONFIG_CONTENTS_REQUEST,
    tms_TOPIC_GET_CONFIG_DC_LOAD_SHARING_RESPONSE,
    tms_TOPIC_GET_CONFIG_DEVICE_PARAMETER_RESPONSE,
    tms_TOPIC_GET_CONFIG_GROUNDING_CIRCUIT_RESPONSE,
    tms_TOPIC_GET_CONFIG_LOAD_SHARING_RESPONSE,
    tms_TOPIC_GET_CONFIG_POWER_SWITCH_RESPONSE,
    tms_TOPIC_GET_CONFIG_SOURCE_TRANSITION_RESPONSE,
    tms_TOPIC_GET_CONFIG_STORAGE_CONTROL_RESPONSE,
    tms_TOPIC_GROUNDING_CIRCUIT_REQUEST,
    tms_TOPIC_HEARTBEAT,
    tms_TOPIC_LOAD_SHARING_REQUEST,
    tms_TOPIC_LOAD_SHARING_STATUS,
    tms_TOPIC_MICROGRID_CONNECTION_LIST,
    tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME,
    tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST,
    tms_TOPIC_OPERATOR_CONNECTION_LIST,
    tms_TOPIC_OPERATOR_INTENT_REQUEST,
    tms_TOPIC_OPERATOR_INTENT_STATE,
    tms_TOPIC_POWER_SWITCH_REQUEST,
    tms_TOPIC_RELEASE_CONFIG_REQUEST,
    tms_TOPIC_REQUEST_RESPONSE,
    tms_TOPIC_RESERVE_CONFIG_REPLY,
    tms_TOPIC_RESERVE_CONFIG_REQUEST,
    tms_TOPIC_SOURCE_TRANSITION_REQUEST,
    tms_TOPIC_SOURCE_TRANSITION_STATE,
    tms_TOPIC_STANDARD_CONFIG_MASTER,
    tms_TOPIC_STORAGE_CONTROL_STATUS,
    tms_TOPIC_STORAGE_INFO,
    tms_TOPIC_STORAGE_STATE
};

// Should probably intialize this_device_id in a loop since setting an array size tms_LEN_Fingerprint
// to a "fixed string 32 chars + null" defeats the purpose of using tms_LEN_Fingerprint - but eventually
// it would be populated via a CAN device ID access 
static char this_device_id [tms_LEN_Fingerprint+1] = "00000000000000000000000000001234";  

// Local prototypes
void handle_SIGINT(int unused);
int tms_app_main (unsigned int);
int main (int argc, char *argv[]);

//-------------------------------------------------------------------
// handle_SIGINT - sets flag for orderly shutdown on Ctrl-C
//-------------------------------------------------------------------

void handle_SIGINT(int unused)
{
  // On CTRL+C - abort! //
  run_flag = false;
}

// class RequestSequenceNumber member function definitions (see class def in tmsTestExampleApp.h)
// Used to manage the static sequence_number across all requests
RequestSequenceNumber::RequestSequenceNumber(ReqCmdQ * req_cmd_q_ptr) {
    myReqCmdQptr =req_cmd_q_ptr;
    mySeqNum = &sequence_number; 
}

unsigned long long RequestSequenceNumber::getNextSeqNo(enum TOPICS_E topic_enum) { 
    // this function increments and returns the next sequenceNo for the requestTopic write
    // and enques the topic_enum and sequence number for later response processing
    ReqQEntry rq_entry;
    rq_entry.requestorEnum = topic_enum;
    (*mySeqNum)++;
    rq_entry.sequenceNum = (*mySeqNum);
    myReqCmdQptr->reqCmdQWrite(rq_entry);
    return (*mySeqNum);
}
// END class RequestSequenceNumber member function definitions


// class ReqCmdQ member function definitions (see class def in tmsTestExampleApp.h)
ReqCmdQ::ReqCmdQ () {
// main thing is to set the start=end indexes to 0, 
// and sequence numbers to 0 to since we start at 1
   memset(&rq, 0, sizeof(rq));  
}

// The code below must be used as follows: reqCmdQwrite is only ever done
// from class RequestSequenceNumber::getNextSeqNo which is only ever used
// when sending a request from the main loop. This guarantees that any
// response we are processing can not be from the sequenced request we are
// about to write. To ensure the requestorEnum is paired with the
// sequenceNum if we write the sequenceNum before the requestorEnum and
// read the sequenceNum after the requestorEnum they MUST necessarily 
// be a pair IFF the read seqenceNum matches the requested seqnceNo.
void ReqCmdQ::reqCmdQWrite(ReqQEntry reqQentry) {
    // CAUTION: Keep Order - Write sequence number first, read sequence number last
    rq.req_Q_entry[rq.end].sequenceNum = reqQentry.sequenceNum;
    rq.req_Q_entry[rq.end].requestorEnum = reqQentry.requestorEnum;
    rq.end = (rq.end + 1) % RQ_SIZE;
}

enum TOPICS_E  ReqCmdQ::reqCmdQRead(unsigned long long sequenceNo){
    // CAUTION: Keep Order - Write squence number first, read sequence number last
    int idx = sequenceNo % RQ_SIZE;
    enum TOPICS_E enumFound = rq.req_Q_entry[idx].requestorEnum;
    if (rq.req_Q_entry[idx].sequenceNum != sequenceNo)
        enumFound = tms_TOPIC_LAST_SENTINEL_ENUM;
    
    return enumFound;
}
// END class ReqQEntry member function definitions


/* Delete all entities */
static int participant_shutdown(
    DDSDomainParticipant *participant)
{
    DDS_ReturnCode_t retcode;
    int status = 0;

    if (participant != NULL) {
        retcode = participant->delete_contained_entities();
        if (retcode != DDS_RETCODE_OK) {
            std::cerr <<  "delete_contained_entities error " << retcode << std::endl << std::flush;
            status = -1;
        }

        retcode = DDSTheParticipantFactory->delete_participant(participant);
        if (retcode != DDS_RETCODE_OK) {
            std::cerr <<  "delete_participant error " << retcode << std::endl << std::flush;
            status = -1;
        }
    }

    /* RTI Connext provides finalize_instance() method on
    domain participant factory for people who want to release memory used
    by the participant factory. Uncomment the following block of code for
    clean destruction of the singleton. */
    /*

    retcode = DDSDomainParticipantFactory::finalize_instance();
    if (retcode != DDS_RETCODE_OK) {
        std::cerr <<  "Finalize_instance error " << retcode << std::endl; 
        status = -1;
    }
    */

    return status;
}

extern "C" int tms_app_main(int sample_count) {
    DDSDomainParticipant * participant = NULL;
    DDS_ReturnCode_t retcode, retcode1, retcode2, retcode3;  // compound retcodes to do one check
 
    // Array of writer enum TOPIC_E - enter the writers defined in System Designer XML file
    TOPICS_E myWritersIndx [] = {
        tms_TOPIC_HEARTBEAT_ENUM, 
        tms_TOPIC_REQUEST_RESPONSE_ENUM,
        tms_TOPIC_SOURCE_TRANSITION_STATE_ENUM,
        tms_TOPIC_DEVICE_ANNOUNCEMENT_ENUM,
        tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM
    }; 
    size_t mwIndx = sizeof(myWritersIndx) / sizeof(TOPICS_E); // actual number of writers

    // Array of Writer Handles - filled out by our Lookup loop below    
    DDSDynamicDataWriter * myWriters[tms_TOPIC_LAST_SENTINEL_ENUM] = { // Maximum # writers possible
        NULL
    };
    // Array of Writer Data Instance (To Do - combine to one struct)
    DDS_DynamicData * myWriterDataInstances[tms_TOPIC_LAST_SENTINEL_ENUM] = { // Maximum # writers possible
        NULL 
    };

    // Array of reader enum TOPIC_E - enter the writers defined in  
    TOPICS_E myReadersIndx [] = {
        tms_TOPIC_REQUEST_RESPONSE_ENUM,
        tms_TOPIC_SOURCE_TRANSITION_REQUEST_ENUM,
        tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM
    };
    size_t mrIndx = sizeof(myReadersIndx) / sizeof(TOPICS_E); // actual number of writers

    // Array of Reader Handles - filled out by our Lookup loop below
    DDSDynamicDataReader * myReaders[tms_TOPIC_LAST_SENTINEL_ENUM] = { // Maximum # writers possible
        NULL
    };

    std::string writerName;
    std::string readerName;

    // DDSGuardCondition heartbeatStateChangeCondit;  // example of publishing a periodic as a change state.
    DDSGuardCondition sourceTransitionStateChangeCondit;

    DDS_Duration_t send_period = {1,0};

    ReqCmdQ  req_cmd_q;
    RequestSequenceNumber * reqSeqNo = new RequestSequenceNumber(&req_cmd_q);

    // Declare Reader and Writer thread Information structs
    PeriodicWriterThreadInfo * myHeartbeatThreadInfo = new PeriodicWriterThreadInfo(tms_TOPIC_HEARTBEAT_ENUM, send_period);
    // OnChangeWriterThreadInfo * myOnChangeWriterHeartbeatThreadInfo = new OnChangeWriterThreadInfo(tms_TOPIC_HEARTBEAT_ENUM, &heartbeatStateChangeCondit);
    WriterEventsThreadInfo * myDeviceAnnouncementEventThreadInfo = new WriterEventsThreadInfo(tms_TOPIC_DEVICE_ANNOUNCEMENT_ENUM); 
	WriterEventsThreadInfo * myMicrogridMembershipRequestEventThreadInfo = new WriterEventsThreadInfo (tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM);
    WriterEventsThreadInfo * myRequestResponseEventThreadInfo = new WriterEventsThreadInfo(tms_TOPIC_REQUEST_RESPONSE_ENUM);
    OnChangeWriterThreadInfo * myOnChangeWriterSourceTransitionStateThreadInfo = new OnChangeWriterThreadInfo(tms_TOPIC_SOURCE_TRANSITION_STATE_ENUM, &sourceTransitionStateChangeCondit);
    ReaderThreadInfo * myMicrogridMembershipOutcomeReaderThreadInfo = new ReaderThreadInfo(tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM);
    ReaderThreadInfo * myRequestResponseReaderThreadInfo = new ReaderThreadInfo(tms_TOPIC_REQUEST_RESPONSE_ENUM);
    ReaderThreadInfo * mySourceTransitionRequestReaderThreadInfo = new ReaderThreadInfo(tms_TOPIC_SOURCE_TRANSITION_REQUEST_ENUM, ECHO_RQST_RESPONSE);

    /* To customize participant QoS, use 
    the configuration file USER_QOS_PROFILES.xml */
    std::cout << "Starting tms application\n" << std::flush;

    participant = DDSTheParticipantFactory->
            create_participant_from_config(
                                "TMS_ParticipantLibrary1::TMS Device Participant1");
    if (participant == NULL) {
        std::cerr << "create_participant_from_config error " << std::endl << std::flush;
        participant_shutdown(participant);
        goto tms_app_main_just_return;
    }
    
    std::cout << "Successfully Created Tactical Microgrid Participant from the System Designer config file"
     << std::endl << std::flush;

    // **** FIND DATA WRITERS AND READERS ****** CREATE DATA w/WRITERS
    //
    // Care must be taken to ensure the System Designer Writer/Reader Names match the names
    // in the topic names defined in tmsTestExamples.h which was directly generated via 
    // rtiddscodegen from the official tms Model. 
    // Writers in the xml are "TMS Device Publisher1::<TopicName>Writer"
    // Readers in the xml are "TMS Device Subscriber1::<TopicName>Reader"
    // If the loop throws an error likey these did not match as expected
    for (int i=0; i<mwIndx; i++) {
        writerName = "TMS Device Publisher1::";
        writerName.append(topic_name_array[myWritersIndx[i]]);
        writerName.append("Writer");
        // Lookup writer handles and put them in myWriters[this_topic_enum]
        myWriters[myWritersIndx[i]] = DDSDynamicDataWriter::narrow(
            participant->lookup_datawriter_by_name(writerName.c_str()));
        if (myWriters[myWritersIndx[i]] == NULL) {
            std::cerr << writerName << ": lookup_datawriter_by_name error "
                << retcode << std::endl << std::flush; 
		    goto tms_app_main_end;
        }
        std::cout << "Successfully Found: " << writerName 
            << std::endl << std::flush;
        myWriterDataInstances[myWritersIndx[i]] = myWriters[myWritersIndx[i]]->create_data(DDS_DYNAMIC_DATA_PROPERTY_DEFAULT);
        if (myWriterDataInstances[myWritersIndx[i]]  == NULL) {
            std::cerr << topic_name_array[myWritersIndx[i]] << ": create_data error"
                << retcode << std::endl << std::flush;
            goto tms_app_main_end;
        } 
        std::cout << "Successfully created Data for: " << topic_name_array[myWritersIndx[i]]
            << std::endl << std::flush;  
    }

    for (int i=0; i<mrIndx; i++) {
        readerName = "TMS Device Subscriber1::";
        readerName.append(topic_name_array[myReadersIndx[i]]);
        readerName.append("Reader");
        // Lookup reader handles and put them in myReaders[this_topic_enum]
        myReaders[myReadersIndx[i]] = DDSDynamicDataReader::narrow(
            participant->lookup_datareader_by_name(readerName.c_str()));
        if (myReaders[myReadersIndx[i]] == NULL) {
            std::cerr << readerName << ": lookup_datawreader_by_name error "
                << retcode << std::endl << std::flush; 
		    goto tms_app_main_end;
        }
        std::cout << "Successfully Found: " << readerName 
            << std::endl << std::flush;
    }

    std::cout << "Successfully created: source_transition_state_data topic w/state_transition_state writer" 
    << std::endl << std::flush;  

	// Turn up threads - the Event threads do nothing but hang on events (no data)
    // Like to put the following in an itterator creating all the pthreads but the 
    // topicThreadInfo's are somewhat different depending upon the communications pattern
    myHeartbeatThreadInfo->writer = myWriters[tms_TOPIC_HEARTBEAT_ENUM];
    // myHeartbeatThreadInfo->writer = heartbeat_writer;
    myHeartbeatThreadInfo->enabled=false; // enable topic when Membership approved
    myHeartbeatThreadInfo->periodicData=myWriterDataInstances[tms_TOPIC_HEARTBEAT_ENUM]; 
    pthread_t whb_tid; // writer device_announcement tid
    pthread_create(&whb_tid, NULL, pthreadPeriodicWriter, (void*) myHeartbeatThreadInfo);
    
    // myOnChangeWriterSourceTransitionStateThreadInfo->writer = source_transition_state_writer;
    myOnChangeWriterSourceTransitionStateThreadInfo->writer = myWriters[tms_TOPIC_SOURCE_TRANSITION_STATE_ENUM];
    myOnChangeWriterSourceTransitionStateThreadInfo->enabled=true; // enable topic to be published
    myOnChangeWriterSourceTransitionStateThreadInfo->changeStateData=myWriterDataInstances[tms_TOPIC_SOURCE_TRANSITION_STATE_ENUM]; 
    pthread_t wstc_tid; // writer device_announcement tid
    pthread_create(&wstc_tid, NULL, pthreadOnChangeWriter, (void*) myOnChangeWriterSourceTransitionStateThreadInfo);

    // myDeviceAnnouncementEventThreadInfo->writer = device_announcement_writer;
    myDeviceAnnouncementEventThreadInfo->writer = myWriters[tms_TOPIC_DEVICE_ANNOUNCEMENT_ENUM];
    pthread_t wda_tid; // writer device_announcement tid
    pthread_create(&wda_tid, NULL, pthreadToProcWriterEvents, (void*) myDeviceAnnouncementEventThreadInfo);

    // myMicrogridMembershipRequestEventThreadInfo->writer = microgrid_membership_request_writer;
    myMicrogridMembershipRequestEventThreadInfo->writer = myWriters[tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM];
    pthread_t wmmr_tid; // writer microgrid_membership_request tid
    pthread_create(&wmmr_tid, NULL, pthreadToProcWriterEvents, (void*) myMicrogridMembershipRequestEventThreadInfo);

    // myRequestResponseEventThreadInfo->writer = request_response_writer;
    myRequestResponseEventThreadInfo->writer = myWriters[tms_TOPIC_REQUEST_RESPONSE_ENUM];
    pthread_t wrr_tid; // Wroter Request Response tid
    pthread_create(&wrr_tid, NULL, pthreadToProcWriterEvents, (void*) myRequestResponseEventThreadInfo);

    // myMicrogridMembershipOutcomeReaderThreadInfo->reader = microgrid_membership_outcome_reader;
    myMicrogridMembershipOutcomeReaderThreadInfo->reader = myReaders[tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM];
    pthread_t rmmo_tid; // Reader microgrid_membership_outcome tid
    pthread_create(&rmmo_tid, NULL, pthreadToProcReaderEvents, (void*) myMicrogridMembershipOutcomeReaderThreadInfo);

    // mySourceTransitionRequestReaderThreadInfo->reader = source_transition_request_reader;
    mySourceTransitionRequestReaderThreadInfo->reader = myReaders[tms_TOPIC_SOURCE_TRANSITION_REQUEST_ENUM];
    mySourceTransitionRequestReaderThreadInfo->reqRspWriter = myWriters[tms_TOPIC_REQUEST_RESPONSE_ENUM];
    pthread_t rstr_tid; // Reader Source Transition Request tid
    pthread_create(&rstr_tid, NULL, pthreadToProcReaderEvents, (void*) mySourceTransitionRequestReaderThreadInfo);

    // myRequestResponseReaderThreadInfo->reader = request_response_reader;
    myRequestResponseReaderThreadInfo->reader = myReaders[tms_TOPIC_REQUEST_RESPONSE_ENUM];
    pthread_t rrr_tid; // Reader Request Response tid - a regular pirate rrr
    pthread_create(&rrr_tid, NULL, pthreadToProcReaderEvents, (void*) myRequestResponseReaderThreadInfo);

    NDDSUtility::sleep(send_period); // wait a second for thread initialization to complete printing (printing is not sychronized)

    /* Publish one-time topics here - QoS is likely keep last, with durability set to transient-local to allow late joiners
       to get these announcements
    */
    std::cout <<  std::endl << tms_TOPIC_DEVICE_ANNOUNCEMENT << ": " << this_device_id << std::endl;

    // Set static data in topics (fill dynamic data in the main_loop or handler) 
    myWriterDataInstances[tms_TOPIC_DEVICE_ANNOUNCEMENT_ENUM]->set_octet_array
        ("deviceId", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, tms_LEN_Fingerprint, (const DDS_Octet *)&this_device_id); 
    retcode = myWriters[tms_TOPIC_DEVICE_ANNOUNCEMENT_ENUM]->write(* myWriterDataInstances[tms_TOPIC_DEVICE_ANNOUNCEMENT_ENUM], DDS_HANDLE_NIL);
    if (retcode != DDS_RETCODE_OK) {
        std::cerr << "product_info: Dynamic Data Set Error " << std::endl << std::flush;
        goto tms_app_main_end;
    }

    retcode = myWriterDataInstances[tms_TOPIC_HEARTBEAT_ENUM]->set_octet_array
        ("deviceId", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, tms_LEN_Fingerprint, (const DDS_Octet *)&this_device_id); 
    if (retcode != DDS_RETCODE_OK) {
        std::cerr << "heartbeat: Dynamic Data Set Error" << std::endl << std::flush;
        goto tms_app_main_end;
    }

    // configure a request to join microgrid  - once configured - update the requestId.sequenceNumber and issue via the write below at will (not durable)
    retcode = myWriterDataInstances[tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM]->set_octet_array
        ("requestId.deviceId", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, tms_LEN_Fingerprint, (const DDS_Octet *)&this_device_id); 
    retcode1 = myWriterDataInstances[tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM]->\
        set_ulonglong("requestId.sequenceNumber", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, (DDS_UnsignedLongLong)\
        reqSeqNo->getNextSeqNo(tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM)); 
    retcode2 = myWriterDataInstances[tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM]->set_octet_array
        ("deviceId", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, tms_LEN_Fingerprint, (const DDS_Octet *)&this_device_id); 
    // note enums are compiler dependent and here seem to be 4 bytes long (the compiler will tell you- and you can always printf sizeof(MM_JOIN))
    retcode3 = myWriterDataInstances[tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM]->set_long
        ("membership", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, (DDS_Long) MM_JOIN);
    if (retcode != DDS_RETCODE_OK || retcode1 != DDS_RETCODE_OK || retcode2 != DDS_RETCODE_OK || retcode3 != DDS_RETCODE_OK) {
        std::cerr << "microgrid_membership_request: Dynamic Data Set Error" << std::endl << std::flush;
        goto tms_app_main_end;
    }
    NDDSUtility::sleep(send_period); // Optional - to let periodic writer go first

    // get approval to enter the grid
    retcode = myWriters[tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM]->write
        (* myWriterDataInstances[tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM], DDS_HANDLE_NIL);
    if (retcode != DDS_RETCODE_OK) {
        std::cerr << "product_info: Dynamic Data Set Error " << std::endl << std::flush;
        goto tms_app_main_end;
    }

    // Upon approval enable all Periodic and Change State Topics
    myHeartbeatThreadInfo->enabled=true;  

    /* Main loop */
    while (run_flag) {

        std::cout << ". "; // background idle
        // Do your stuff here to interact CAN to DDS (i.e. get devices state and
        // load DDS topics, set change triggers etc.)
    
        NDDSUtility::sleep(send_period);  // remove eventually 
    }

    tms_app_main_end:
    /* Delete all entities */
    std::cout << "Stopping - shutting down participant\n" << std::flush;

    pthread_cancel(whb_tid); 
    pthread_cancel(wstc_tid);
    pthread_cancel(wda_tid); 
    pthread_cancel(wmmr_tid); 
    pthread_cancel(rmmo_tid); 
    pthread_cancel(rrr_tid);
    pthread_cancel(wrr_tid);
    pthread_cancel(rstr_tid);

    tms_app_main_just_return:
    return participant_shutdown(participant);
}

int main(int argc, char *argv[])
{
    int sample_count = 0; /* infinite loop */
    signal(SIGINT, handle_SIGINT);

    if (argc >= 2) {
        sample_count = atoi(argv[2]);
    }

    /* Uncomment this to turn on additional logging
    NDDSConfigLogger::get_instance()->
    set_verbosity_by_category(NDDS_CONFIG_LOG_CATEGORY_API, 
    NDDS_CONFIG_LOG_VERBOSITY_STATUS_ALL);
    */

    return tms_app_main(sample_count);
}