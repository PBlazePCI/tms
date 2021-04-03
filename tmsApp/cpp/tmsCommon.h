/* (c) Copyright, Real-Time Innovations, 2012.  All rights reserved.
* RTI grants Licensee a license to use, modify, compile, and create derivative
* works of the software solely for use with RTI Connext DDS. Licensee may
* redistribute copies of the software provided that all such copies are subject
* to this license. The software is provided "as is", with no warranty of any
* type, including any warranty for fitness for any purpose. RTI is under no
* obligation to maintain or support the software. RTI shall not be liable for
* any incidental or consequential damages arising out of the use or inability
* to use the software.
*/
#ifndef tmsCommon_h
#define tmsCommon_h



enum TOPICS_E { 
    tms_TOPIC_ACTIVE_DIAGNOSTICS_ENUM,
    tms_TOPIC_AUTHORIZATION_TO_ENERGIZE_OUTCOME_ENUM,
    tms_TOPIC_AUTHORIZATION_TO_ENERGIZE_REQUEST_ENUM,
    tms_TOPIC_AUTHORIZATION_TO_ENERGIZE_RESPONSE_ENUM,
    tms_TOPIC_CONFIG_RESERVATION_STATE_ENUM,
    tms_TOPIC_COPY_CONFIG_REQUEST_ENUM,
    tms_TOPIC_DC_DEVICE_POWER_MEASUREMENT_LIST_ENUM,
    tms_TOPIC_DC_LOAD_SHARING_REQUEST_ENUM,
    tms_TOPIC_DC_LOAD_SHARING_STATUS_ENUM,
    tms_TOPIC_DEVICE_ANNOUNCEMENT_ENUM,
    tms_TOPIC_DEVICE_CLOCK_STATUS_ENUM,
    tms_TOPIC_DEVICE_GROUNDING_ENUM,
    tms_TOPIC_DEVICE_GROUNDING_STATUS_ENUM,
    tms_TOPIC_DEVICE_PARAMETER_REQUEST_ENUM,
    tms_TOPIC_DEVICE_PARAMETER_STATUS_ENUM,
    tms_TOPIC_DEVICE_POWER_MEASUREMENT_LIST_ENUM,
    tms_TOPIC_DEVICE_POWER_PORT_LIST_ENUM,
    tms_TOPIC_DEVICE_POWER_STATUS_LIST_ENUM,
    tms_TOPIC_DISCOVERED_CONNECTION_LIST_ENUM,
    tms_TOPIC_ENGINE_STATE_ENUM,
    tms_TOPIC_FINGERPRINT_NICKNAME_ENUM,
    tms_TOPIC_FINGERPRINT_NICKNAME_REQUEST_ENUM,
    tms_TOPIC_GET_CONFIG_CONTENTS_REQUEST_ENUM,
    tms_TOPIC_GET_CONFIG_DC_LOAD_SHARING_RESPONSE_ENUM,
    tms_TOPIC_GET_CONFIG_DEVICE_PARAMETER_RESPONSE_ENUM,
    tms_TOPIC_GET_CONFIG_GROUNDING_CIRCUIT_RESPONSE_ENUM,
    tms_TOPIC_GET_CONFIG_LOAD_SHARING_RESPONSE_ENUM,
    tms_TOPIC_GET_CONFIG_POWER_SWITCH_RESPONSE_ENUM,
    tms_TOPIC_GET_CONFIG_SOURCE_TRANSITION_RESPONSE_ENUM,
    tms_TOPIC_GET_CONFIG_STORAGE_CONTROL_RESPONSE_ENUM,
    tms_TOPIC_GROUNDING_CIRCUIT_REQUEST_ENUM,
    tms_TOPIC_HEARTBEAT_ENUM,
    tms_TOPIC_LOAD_SHARING_REQUEST_ENUM,
    tms_TOPIC_LOAD_SHARING_STATUS_ENUM,
    tms_TOPIC_MICROGRID_CONNECTION_LIST_ENUM,
    tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM,
    tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM,
    tms_TOPIC_OPERATOR_CONNECTION_LIST_ENUM,
    tms_TOPIC_OPERATOR_INTENT_REQUEST_ENUM,
    tms_TOPIC_OPERATOR_INTENT_STATE_ENUM,
    tms_TOPIC_POWER_SWITCH_REQUEST_ENUM,
    tms_TOPIC_RELEASE_CONFIG_REQUEST_ENUM,
    tms_TOPIC_REQUEST_RESPONSE_ENUM,
    tms_TOPIC_RESERVE_CONFIG_REPLY_ENUM,
    tms_TOPIC_RESERVE_CONFIG_REQUEST_ENUM,
    tms_TOPIC_SOURCE_TRANSITION_REQUEST_ENUM,
    tms_TOPIC_SOURCE_TRANSITION_STATE_ENUM,
    tms_TOPIC_STANDARD_CONFIG_MASTER_ENUM,
    tms_TOPIC_STORAGE_CONTROL_REQUEST_ENUM,
    tms_TOPIC_STORAGE_CONTROL_STATUS_ENUM,
    tms_TOPIC_STORAGE_INFO_ENUM,
    tms_TOPIC_STORAGE_STATE_ENUM,
    tms_TOPIC_LAST_SENTINEL_ENUM
};

class ReqCmdQ; // forward reference

class RequestSequenceNumber {
    public:
    // class to manage the static sequence_number used in requests
    RequestSequenceNumber(ReqCmdQ * req_cmd_q_ptr);
    unsigned long long getNextSeqNo(enum TOPICS_E topic_enum);

    private:
    unsigned long long * mySeqNum; // manages the single static sequence_number
    ReqCmdQ * myReqCmdQptr;
};
typedef struct {
    unsigned long long sequenceNum;
    enum TOPICS_E requestorEnum;
} ReqQEntry;

#define RQ_SIZE 10
typedef struct {
    unsigned int start, end;
    ReqQEntry req_Q_entry[RQ_SIZE];
} ReqQ;

class ReqCmdQ {   
    // ReqCmdQ is a circular buffer that holds the last n Request Entries this device has written
    // it is used with a RequestResponse sequence number to determine if we still have the request
    // entry () (i.e. RequestResponse sequence number matches the saved sequence number in the 
    // entry [RequestResponse sequence % qSize]), if so, the RequestResponse can be processed in
    // the context of the request pended to the queue (what ever that means to you). If not the
    // Request Response being process must be ignored since we no longer have context.

    friend class RequestSequenceNumber; // allow reqCmdQWrite only by RequestSequenceNumber::getNextSeqNo()

    public:
    // allocate and intializes the array of entries and structure control vars that track the circular writes
    ReqCmdQ();  

  
    // Reading the reqCmdQ is non distructive, it simply takes the given sequenceNo and calculates
    // and index modulo the queue size returning the enum TOPICS_E stored in the entry ReqQEntry.  
    // HOWEVER, before it blindly returns the enum TOPICS_E it first verifes the stored sequence 
    // number matches the requested sequence number. If it does not match, the SENTINEL_ENUM is
    // returned instead of a correlating ENUM to the response.
    // The reader needs needs to check the returned ENUM for the SENTINEL to determine if the 
    // Response Request they are processing is known (i.e., it has not been overwritten.)
    // Reading is expected to occur from a thread processing the ResponseRequests being returned
    // from the Microgrid Controller to our prior requests (issued from the main loop) 
    enum TOPICS_E reqCmdQRead(unsigned long long sequenceNo);

    private:
    // There MUST be only one writer at a time (i.e. reqCmdQwrite() is not reentrant.
    // reqCmdQWrite is intended to ONLY be used from the main thread on a request. This
    // is enforced by only allowing the reqCmdQWrite() executed from the 
    // RequestSequenceNumber::getNextSeqNo() function which is to only be used from the
    // main loop when you send a request.
    // Writing is never blocked and will wrap, overwriting the oldest entry. 
    void reqCmdQWrite(ReqQEntry reqQentry);

    ReqQ rq;  // The actual request queue w/entries array
};


#endif