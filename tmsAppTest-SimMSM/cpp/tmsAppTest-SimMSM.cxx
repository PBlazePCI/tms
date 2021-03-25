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
#include "tmsTestExample.h" // This file was created by rticodegen from the official TMS datamodel
#include "tmsTestExampleApp.h" // Use the common app header vs. "tmsAppTest-SimMSM.h"
#include "tmsCommPatterns.h"

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
    DDSDynamicDataReader * request_response_reader = NULL;
    DDSDynamicDataReader * source_transition_state_reader = NULL;
	DDSDynamicDataReader * microgrid_membership_request_reader = NULL;
	DDSDynamicDataWriter * microgrid_membership_outcome_writer = NULL;
    DDSDynamicDataWriter * request_response_writer = NULL;
    DDSDynamicDataWriter * source_transition_request_writer = NULL;
    DDS_DynamicData * microgrid_membership_outcome_data = NULL;
    DDS_DynamicData * source_transition_request_data = NULL;
    DDS_DynamicData * request_response_data = NULL;
    DDS_ReturnCode_t retcode, retcode1, retcode2;  // compound retcodes to do one check

     
    // DDSGuardCondition heartbeatStateChangeCondit;  // example of publishing a periodic as a change state.
    DDSGuardCondition sourceTransitionRequestChangeCondit;

    unsigned long long count = 0;  
    DDS_Duration_t send_period = {1,0};

    ReqCmdQ  req_cmd_q;
    RequestSequenceNumber * reqSeqNo = new RequestSequenceNumber(&req_cmd_q);

    // Declare Reader and Writer thread Information structs
	OnChangeWriterThreadInfo * myOnChangeMicrogridMembershipOutcomeThreadInfo = \
        new OnChangeWriterThreadInfo (tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM, &sourceTransitionRequestChangeCondit);
    WriterEventsThreadInfo * myRequestResponseEventThreadInfo = new WriterEventsThreadInfo(tms_TOPIC_REQUEST_RESPONSE_ENUM);
    WriterEventsThreadInfo * mySourceTransitionRequestEventThreadInfo = new WriterEventsThreadInfo(tms_TOPIC_SOURCE_TRANSITION_REQUEST_ENUM);
    ReaderThreadInfo * myMicrogridMembershipRequestReaderThreadInfo = new ReaderThreadInfo(tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM);
    ReaderThreadInfo * myRequestResponseReaderThreadInfo = new ReaderThreadInfo(tms_TOPIC_REQUEST_RESPONSE_ENUM);
    ReaderThreadInfo * mySourceTransitionStateReaderThreadInfo = new ReaderThreadInfo(tms_TOPIC_SOURCE_TRANSITION_STATE_ENUM);

    /* To customize participant QoS, use 
    the configuration file USER_QOS_PROFILES.xml */
    std::cout << "Starting tms SIM MSM application\n" << std::flush;

    participant = DDSTheParticipantFactory->
            create_participant_from_config(
                                "TMS_ParticipantLibrary1::TMS_Participant1");
    if (participant == NULL) {
        std::cerr << "create_participant_from_config error " << std::endl << std::flush;
        participant_shutdown(participant);
        goto tms_app__test_MSM_main_just_return;
    }
    
    std::cout << "Successfully Created Tactical Microgrid Participant from the System Designer config file"
     << std::endl << std::flush;


    // To Do: create an array of writers w/data handles indexed by enum topic
    // and itterate getting writer handles - same with reader handles 
    microgrid_membership_outcome_writer = DDSDynamicDataWriter::narrow(
		// Defined only in domain_participant_library. PUblisher name not defined QoS file
        participant->lookup_datawriter_by_name("TMS_Publisher1::MicrogridMembershipOutcomeWriter"));
    if (microgrid_membership_outcome_writer  == NULL) {
        std::cerr << "TMS_Publisher1::MicrogridMembershipOutcomeWriter lookup_datawriter_by_name error " 
        << retcode << std::endl << std::flush; 
		goto tms_app_test_MSM_main_end;
    }
    std::cout << "Successfully Found: TMS_Publisher1::MicrogridMembershipOutcomeWriter" 
    << std::endl << std::flush;

    source_transition_request_writer = DDSDynamicDataWriter::narrow(
        participant->lookup_datawriter_by_name("TMS_Publisher1::SourceTransitionRequestWriter"));
    if (source_transition_request_writer  == NULL) {
        std::cerr << "TMS_Publisher1::SourceTransitionRequestWriter: lookup_datawriter_by_name error "
        << retcode << std::endl << std::flush; 
		goto tms_app_test_MSM_main_end;
    }
    std::cout << "Successfully Found: TMS_Publisher1::SourceTransitionRequestWriter" 
    << std::endl << std::flush;

    request_response_writer = DDSDynamicDataWriter::narrow(
        participant->lookup_datawriter_by_name("TMS_Publisher1::RequestResponseWriter"));
    if (request_response_writer   == NULL) {
        std::cerr << "TMS_Publisher1::RequestResponseWriter: lookup_datawriter_by_name error "
        << retcode << std::endl << std::flush; 
		goto tms_app_test_MSM_main_end;
    }
    std::cout << "Successfully Found: TMS_Publisher1::RequestResponseWriter" 
    << std::endl << std::flush;

 	microgrid_membership_request_reader = DDSDynamicDataReader::narrow(
		// Defined only in domain_participant_library. PUblisher name not defined QoS file
		participant->lookup_datareader_by_name("TMS_Subscriber1::MicrogridMembershipRequestReader")); 
    if (microgrid_membership_request_reader == NULL) {
        std::cerr << "TMS_Subscriber1::MicrogridMembershipRequestReader"
        << retcode << std::endl << std::flush;
		goto tms_app_test_MSM_main_end;
    } 
    std::cout << "Successfully Found: TMS_Subscriber1::MicrogridMembershipOutcomeTopicReader" 
    << std::endl << std::flush;   

    request_response_reader = DDSDynamicDataReader::narrow(
		// Defined only in domain_participant_library. PUblisher name not defined QoS file
		participant->lookup_datareader_by_name("TMS_Subscriber1::RequestResponseReader")); 
    if (request_response_reader == NULL) {
        std::cerr << "TMS_Subscriber1::RequestResponseReader"
        << retcode << std::endl << std::flush;
		goto tms_app_test_MSM_main_end;
    } 
    std::cout << "Successfully Found: TMS_Subscriber1::RequestResponseReader" 
    << std::endl << std::flush; 

    source_transition_state_reader = DDSDynamicDataReader::narrow(
		// Defined only in domain_participant_library. PUblisher name not defined QoS file
		participant->lookup_datareader_by_name("TMS_Subscriber1::SourceTransitionStateReader")); 
    if (source_transition_state_reader == NULL) {
        std::cerr << "TMS_Subscriber1::SourceTransitionStateReader"
        << retcode << std::endl << std::flush;
		goto tms_app_test_MSM_main_end;
    } 
    std::cout << "Successfully Found: TMS_Subscriber1::SourceTransitionStateReader" 
    << std::endl << std::flush; 


    microgrid_membership_outcome_data = microgrid_membership_outcome_writer->create_data(DDS_DYNAMIC_DATA_PROPERTY_DEFAULT);
    if (microgrid_membership_outcome_data == NULL) {
        std::cerr << "microgrid_membership_outcome_data: create_data error"
        << retcode << std::endl << std::flush;
		goto tms_app_test_MSM_main_end;
    } 

    std::cout << "Successfully created: microgrid_membership_outcome_data topic w/microgrid_membership_outcome writer" 
    << std::endl << std::flush;  

    source_transition_request_data = source_transition_request_writer->create_data(DDS_DYNAMIC_DATA_PROPERTY_DEFAULT);
    if (source_transition_request_data == NULL) {
        std::cerr << "source_transition_request_data: create_data error"
        << retcode << std::endl << std::flush;
		goto tms_app_test_MSM_main_end;
    } 

    std::cout << "Successfully created: source_transition_request_data topic w/source_transition_request writer" 
    << std::endl << std::flush;  

    request_response_data = request_response_writer->create_data(DDS_DYNAMIC_DATA_PROPERTY_DEFAULT);
    if (request_response_data == NULL) {
        std::cerr << "request_response_data: create_data error"
        << retcode << std::endl << std::flush;
		goto tms_app_test_MSM_main_end;
    } 

    std::cout << "Successfully created: srequest_response_data topic w/request_response_writer" 
    << std::endl << std::flush;  

	// Turn up threads - the Event threads do nothing but hang on events (no data)
    // Like to put the following in an itterator creating all the pthreads but the 
    // topicThreadInfo's are somewhat different depending upon the communications pattern
    myRequestResponseEventThreadInfo->writer = request_response_writer;
    pthread_t wrr_tid; // Wroter Request Response tid
    pthread_create(&wrr_tid, NULL, pthreadToProcWriterEvents, (void*) myRequestResponseEventThreadInfo);

    myOnChangeMicrogridMembershipOutcomeThreadInfo->writer = microgrid_membership_outcome_writer;
    myOnChangeMicrogridMembershipOutcomeThreadInfo->enabled=true; // enable topic to be published
    myOnChangeMicrogridMembershipOutcomeThreadInfo->changeStateData=microgrid_membership_outcome_data; 
    pthread_t wmmo_tid; // writer microgrid membership outcome tid
    pthread_create(&wmmo_tid, NULL, pthreadOnChangeWriter, (void*) myOnChangeMicrogridMembershipOutcomeThreadInfo);

    mySourceTransitionRequestEventThreadInfo->writer = source_transition_request_writer;
    pthread_t wstr_tid; // writer source transiton request tid
    pthread_create(&wstr_tid, NULL, pthreadToProcWriterEvents, (void*) mySourceTransitionRequestEventThreadInfo);

    myMicrogridMembershipRequestReaderThreadInfo->reader = microgrid_membership_request_reader;
    pthread_t rmmo_tid; // Reader microgrid_membership_outcome tid
    pthread_create(&rmmo_tid, NULL, pthreadToProcReaderEvents, (void*) myMicrogridMembershipRequestReaderThreadInfo);

    mySourceTransitionStateReaderThreadInfo->reader = source_transition_state_reader;
    pthread_t rsts_tid; // Reader Source Transition state tid
    pthread_create(&rsts_tid, NULL, pthreadToProcReaderEvents, (void*) mySourceTransitionStateReaderThreadInfo);

    myRequestResponseReaderThreadInfo->reader = request_response_reader;
    pthread_t rrr_tid; // Reader Request Response tid - a regular pirate rrr
    pthread_create(&rrr_tid, NULL, pthreadToProcReaderEvents, (void*) myRequestResponseReaderThreadInfo);


    NDDSUtility::sleep(send_period); // wait a second for thread initialization to complete printing (printing is not sychronized)


    // configure a request to join microgrid  - once configured - update the requestId.sequenceNumber and issue via the write below at will (not durable)
    retcode = microgrid_membership_outcome_data->set_octet_array("requestId.deviceId", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, tms_LEN_Fingerprint, (const DDS_Octet *)&this_device_id); 
    retcode = microgrid_membership_outcome_data->\
        set_ulonglong("requestId.sequenceNumber", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, (DDS_UnsignedLongLong)\
        reqSeqNo->getNextSeqNo(tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM)); 
    retcode1 = microgrid_membership_outcome_data->set_octet_array("deviceId", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, tms_LEN_Fingerprint, (const DDS_Octet *)&this_device_id); 
    // note enums are compiler dependent and here seem to be 4 bytes long (the compiler will tell you- and you can always printf sizeof(MM_JOIN))
    retcode2 = microgrid_membership_outcome_data->set_long("membership", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, (DDS_Long) MM_JOIN);
    if (retcode != DDS_RETCODE_OK || retcode1 != DDS_RETCODE_OK || retcode2 != DDS_RETCODE_OK) {
        std::cerr << "microgrid_membership_request: Dynamic Data Set Error" << std::endl << std::flush;
        goto tms_app_test_MSM_main_end;
    }
 
    NDDSUtility::sleep(send_period); // Optional - to let periodic writer go first

    // get approval to enter the grid
    retcode = microgrid_membership_outcome_writer->write(* microgrid_membership_outcome_data, DDS_HANDLE_NIL);
    if (retcode != DDS_RETCODE_OK) {
        std::cerr << "product_info: Dynamic Data Set Error " << std::endl << std::flush;
        goto tms_app_test_MSM_main_end;
    }

    /* Main loop */
    while (run_flag) {

        std::cout << ". " << std::flush; // background idle
        // Do your stuff here to interact CAN to DDS (i.e. get devices state and
        // load DDS topics, set change triggers etc.)
    
        /*
        // Demo only - normally change value in statement prior to trigger - but heartbeat is also running periodically
        retcode = heartbeatStateChangeCondit.set_trigger_value(DDS_BOOLEAN_TRUE);
        if (retcode != DDS_RETCODE_OK) {
            std::cerr << "Main Heartbeat: set_trigger condition error\n" << std::endl << std::flush;
            break;
        }
        */

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