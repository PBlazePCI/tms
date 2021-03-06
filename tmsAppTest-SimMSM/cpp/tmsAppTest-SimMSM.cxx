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
#include <signal.h>
#include <iostream>

#include "ndds/ndds_cpp.h"
#include <pthread.h>
#include "tmsTestExample.h"
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
char this_device_id [tms_LEN_Fingerprint+1] = "10000000100000001000000010000000";

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
        fprintf(stderr, "finalize_instance error %d\n", retcode);
        status = -1;
    }
    */

    return status;
}

extern "C" int tms_app_test_msm_main(int sample_count) {
    DDSDomainParticipant * participant = NULL;
    DDS_ReturnCode_t retcode, retcode1, retcode2, retcode3, retcode4;  // compound retcodes to do one check

    // array of writer enum TOPIC_E - enter the writers defined in 
    // System Designer XML file
    TOPICS_E myWritersIndx [] = {
        tms_TOPIC_REQUEST_RESPONSE_ENUM,
        tms_TOPIC_SOURCE_TRANSITION_REQUEST_ENUM,
        tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM
    }; 
    size_t mwIndx = sizeof(myWritersIndx) / sizeof(TOPICS_E); // actual number of writers

    DDSDynamicDataWriter * myWriters[tms_TOPIC_LAST_SENTINEL_ENUM] = { // Maximum # writers possible
        NULL
    };

    // array of Reader handles by enum TOPIC_E 
    TOPICS_E myReadersIndx [] = {
        tms_TOPIC_REQUEST_RESPONSE_ENUM,
        tms_TOPIC_SOURCE_TRANSITION_STATE_ENUM,
        tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM
    };
    size_t mrIndx = sizeof(myReadersIndx) / sizeof(TOPICS_E); // actual number of writers

    DDSDynamicDataReader * myReaders[tms_TOPIC_LAST_SENTINEL_ENUM] = { // Maximum # writers possible
        NULL
    };

    DDS_DynamicData * myWriterDataInstances [tms_TOPIC_LAST_SENTINEL_ENUM] = { // Maximum # writers possible
        NULL 
    };

    std::string writerName;
    std::string readerName;

    // DDSGuardCondition heartbeatStateChangeCondit;  // example of publishing a periodic as a change state.
    DDSGuardCondition microgridMembershipOutcomeCondit;

    DDS_Duration_t send_period = {1,0};

    ReqCmdQ  req_cmd_q;
    RequestSequenceNumber * reqSeqNo = new RequestSequenceNumber(&req_cmd_q);

    // Declare Reader and Writer thread Information structs
	OnChangeWriterThreadInfo * myOnChangeMicrogridMembershipOutcomeThreadInfo = \
        new OnChangeWriterThreadInfo (tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM, &microgridMembershipOutcomeCondit);
    WriterEventsThreadInfo * myRequestResponseEventThreadInfo = new WriterEventsThreadInfo(tms_TOPIC_REQUEST_RESPONSE_ENUM);
    WriterEventsThreadInfo * mySourceTransitionRequestEventThreadInfo = new WriterEventsThreadInfo(tms_TOPIC_SOURCE_TRANSITION_REQUEST_ENUM);
    ReaderThreadInfo * myMicrogridMembershipRequestReaderThreadInfo = new ReaderThreadInfo(tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM, ECHO_RQST_RESPONSE);
    ReaderThreadInfo * myRequestResponseReaderThreadInfo = new ReaderThreadInfo(tms_TOPIC_REQUEST_RESPONSE_ENUM);
    ReaderThreadInfo * mySourceTransitionStateReaderThreadInfo = new ReaderThreadInfo(tms_TOPIC_SOURCE_TRANSITION_STATE_ENUM);

    /* To customize participant QoS, use 
    the configuration file USER_QOS_PROFILES.xml */
    std::cout << "Starting tms SIM MSM application\n" << std::flush;

    participant = DDSTheParticipantFactory->
            create_participant_from_config(
                                "TMS_ParticipantLibrary1::TMS MSM-Simulation Participant1");
    if (participant == NULL) {
        std::cerr << "create_participant_from_config error " << std::endl << std::flush;
        participant_shutdown(participant);
        goto tms_app__test_MSM_main_just_return;
    }
    
    std::cout << "Successfully Created Tactical Microgrid TMS Simulation Participant from the System Designer config file"
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
        writerName = "TMS MSM-Simulation Publisher1::";
        writerName.append(topic_name_array[myWritersIndx[i]]);
        writerName.append("Writer");
        // Lookup writer handles and put them in myWriters[this_topic_enum]
        myWriters[myWritersIndx[i]] = DDSDynamicDataWriter::narrow(
            participant->lookup_datawriter_by_name(writerName.c_str()));
        if (myWriters[myWritersIndx[i]] == NULL) {
            std::cerr << writerName << ": lookup_datawriter_by_name error "
                << retcode << std::endl << std::flush; 
		    goto tms_app_test_MSM_main_end;
        }
        std::cout << "Successfully Found: " << writerName 
            << std::endl << std::flush;
        myWriterDataInstances[myWritersIndx[i]] = myWriters[myWritersIndx[i]]->create_data(DDS_DYNAMIC_DATA_PROPERTY_DEFAULT);
        if (myWriterDataInstances[myWritersIndx[i]]  == NULL) {
            std::cerr << topic_name_array[myWritersIndx[i]] << ": create_data error"
                << retcode << std::endl << std::flush;
            goto tms_app_test_MSM_main_end;
        } 
        std::cout << "Successfully created Data for: " << topic_name_array[myWritersIndx[i]]
            << std::endl << std::flush;  
    }

   for (int i=0; i<mrIndx; i++) {
        readerName = "TMS MSM-Simulation Subscriber1::";
        readerName.append(topic_name_array[myReadersIndx[i]]);
        readerName.append("Reader");
        // Lookup reader handles and put them in myReaders[this_topic_enum]
        myReaders[myReadersIndx[i]] = DDSDynamicDataReader::narrow(
            participant->lookup_datareader_by_name(readerName.c_str()));
        if (myReaders[myReadersIndx[i]] == NULL) {
            std::cerr << readerName << ": lookup_datawreader_by_name error "
                << retcode << std::endl << std::flush; 
		    goto tms_app_test_MSM_main_end;
        }
        std::cout << "Successfully Found: " << readerName 
            << std::endl << std::flush;
    } 

	// Turn up threads - the Event threads do nothing but hang on events (no data)
    // Like to put the following in an itterator creating all the pthreads but the 
    // topicThreadInfo's are somewhat different depending upon the communications pattern
    myRequestResponseEventThreadInfo->writer = myWriters[tms_TOPIC_REQUEST_RESPONSE_ENUM];
    pthread_t wrr_tid; // Wroter Request Response tid
    pthread_create(&wrr_tid, NULL, pthreadToProcWriterEvents, (void*) myRequestResponseEventThreadInfo);

    myOnChangeMicrogridMembershipOutcomeThreadInfo->writer = myWriters[tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM];
    myOnChangeMicrogridMembershipOutcomeThreadInfo->enabled=true; // enable topic to be published
    myOnChangeMicrogridMembershipOutcomeThreadInfo->changeStateData=myWriterDataInstances[tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM]; 
    pthread_t wmmo_tid; // writer microgrid membership outcome tid
    pthread_create(&wmmo_tid, NULL, pthreadOnChangeWriter, (void*) myOnChangeMicrogridMembershipOutcomeThreadInfo);

    mySourceTransitionRequestEventThreadInfo->writer = myWriters[tms_TOPIC_SOURCE_TRANSITION_REQUEST_ENUM];
    pthread_t wstr_tid; // writer source transiton request tid
    pthread_create(&wstr_tid, NULL, pthreadToProcWriterEvents, (void*) mySourceTransitionRequestEventThreadInfo);

    myMicrogridMembershipRequestReaderThreadInfo->reader = myReaders[tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM];
    // this topic when received, requires a RequestResponse - so pass in the writer.
    myMicrogridMembershipRequestReaderThreadInfo->reqRspWriter = myWriters[tms_TOPIC_REQUEST_RESPONSE_ENUM];
    pthread_t rmmo_tid; // Reader microgrid_membership_outcome tid
    pthread_create(&rmmo_tid, NULL, pthreadToProcReaderEvents, (void*) myMicrogridMembershipRequestReaderThreadInfo);

    mySourceTransitionStateReaderThreadInfo->reader = myReaders[tms_TOPIC_SOURCE_TRANSITION_STATE_ENUM];
    pthread_t rsts_tid; // Reader Source Transition state tid
    pthread_create(&rsts_tid, NULL, pthreadToProcReaderEvents, (void*) mySourceTransitionStateReaderThreadInfo);

    myRequestResponseReaderThreadInfo->reader = myReaders[tms_TOPIC_REQUEST_RESPONSE_ENUM];
    pthread_t rrr_tid; // Reader Request Response tid - a regular pirate rrr
    pthread_create(&rrr_tid, NULL, pthreadToProcReaderEvents, (void*) myRequestResponseReaderThreadInfo);

    NDDSUtility::sleep(send_period); // wait a second for thread initialization to complete printing (printing is not sychronized)

    // Preconfigure outcome topic (MicrogridMembershipApproval), in a real MSM you'd keep a database (array) of requesting devices and state
    // Putting the MSM id in the requestId and the approved device relatedRequestId in the deviceId
    retcode = myWriterDataInstances[tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM]->set_octet_array
        ("requestId.deviceId", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, tms_LEN_Fingerprint, (const DDS_Octet *)&this_device_id); 
    retcode1 = myWriterDataInstances[tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM]->\
        set_ulonglong("requestId.sequenceNumber", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, (DDS_UnsignedLongLong)\
        reqSeqNo->getNextSeqNo(tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM)); 
    retcode2 = myWriterDataInstances[tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM]->set_octet_array
        ("deviceId", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, tms_LEN_Fingerprint, (const DDS_Octet *)&this_device_id); 
    // note enums are compiler dependent and here seem to be 4 bytes long (the compiler will tell you- and you can always printf sizeof(MMR_COMPLETE))
    retcode3 = myWriterDataInstances[tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM]->set_long
        ("membership", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, (DDS_Long) MM_JOIN);
    retcode4 = myWriterDataInstances[tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM]->set_long
        ("result", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, (DDS_Long) MMR_COMPLETE);
    if (retcode != DDS_RETCODE_OK || retcode1 != DDS_RETCODE_OK || retcode2 != DDS_RETCODE_OK || retcode2 != DDS_RETCODE_OK) {
        std::cerr << "microgrid_membership_outcome: Dynamic Data Set Error" << std::endl << std::flush;
        goto tms_app_test_MSM_main_end;
    }
 
    NDDSUtility::sleep(send_period); // Optional - to let periodic writer go first

    /* Main loop */
    while (run_flag) {

        std::cout << ". " << std::flush; // background idle
        // Do your stuff here to interact CAN to DDS (i.e. get devices state and
        // load DDS topics, set change triggers etc.)
    
        // check for any internal variables that have changed that are associated
        // with the On Change Topic triggers
        if (internal_membership_result != external_tms_membership_result) {
            external_tms_membership_result=internal_membership_result;
            retcode = microgridMembershipOutcomeCondit.set_trigger_value(DDS_BOOLEAN_TRUE);
            if (retcode != DDS_RETCODE_OK) {
                std::cerr << "Main membership outcome: set_trigger condition error\n" << std::endl << std::flush;
                break;
            } 
        }

        NDDSUtility::sleep(send_period);  // remove eventually 
    }

    tms_app_test_MSM_main_end:
    /* Delete all entities */
    std::cout << "Stopping - shutting down participant\n" << std::flush;

    pthread_cancel(wrr_tid); 
    pthread_cancel(wmmo_tid); 
    pthread_cancel(wstr_tid);
    pthread_cancel(rmmo_tid);
    pthread_cancel(rsts_tid);
    pthread_cancel(rrr_tid);

    tms_app__test_MSM_main_just_return:
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

    return tms_app_test_msm_main(sample_count);
}