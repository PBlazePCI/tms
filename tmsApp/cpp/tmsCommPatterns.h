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
#ifndef tmsCommPatterns_h
#define tmsCommPatterns_h

#include "tmsTestExample.h"
#include "tmsCommon.h"

extern bool run_flag;

// should tuck this var into the RequestSequenceNumber class and make that Class a singlton pattern
extern unsigned long long sequence_number; // ever monotonically increasing for each request sent

// Variable associated with Source Transition Request - note the TMS topic struct holds both present and future state
// so we should be able to leverage the state within the topic
// Also a real MSM would need to keep these in arrays for the maximum number of devices allowed on a Microgrid
extern enum tms_MicrogridMembershipResult internal_membership_result;
extern enum tms_MicrogridMembershipResult external_tms_membership_result;  
extern enum tms_SourceTransition internal_source_transition_state; 
extern enum tms_SourceTransition external_tms_source_transition_state; 

extern const char topic_name_array[tms_TOPIC_LAST_SENTINEL_ENUM][tms_MAXLEN_TopicName];

extern HandlerPtr reader_handler_ptrs[]; // list of Reader topic handlers
extern HandlerPtr periodic_handler_ptrs[]; // list of Reader topic handlers

/* This Interface provides threads for tms Communications Patterns
   (tms Microgrid Standard section 4.9.2)

   All Patterns monitor for Status conditions (refer to enum DDS_StatusKind). 
   The ChangeState pattern also monitors Guard conditions to trigger a write.
   The Request/reply and read will monitor On Data Available (read) and then write. 

   NOTE: The classes here provide control information for the different threads that
   support each communications pattern. There should be NO TOPIC SPECIFIC information
   in these control blocks.  The closest we come is to provide a echoResponse() and 
   tms_REPLY_code in the reader to handle the group of tms_<>_REQUESTs that we know
   require a response.
*/

class ReaderThreadInfo {
    // holds waitset info needed for the Reader waitset processing thread
    // C'tor has Echo Response flag to handle Rcv'd Command Requests and all
    // Response 
    public:
        ReaderThreadInfo(enum TOPICS_E topicEnum, bool echoResponse = false); 
        enum TOPICS_E topic_enum();
        bool echoReqResponse();

        DDSDynamicDataReader * reader;

        // if echoResponse set true this writer will be used to echo the response
        DDSDynamicDataWriter * reqRspWriter;
        DDS_DynamicDataSeq * dataSeq; // pass in dataSeq for handler to process
        DDS_UnsignedLong tms_REPLY_code;
        char reason[tms_MAXLEN_reason];

        
    private:
        enum TOPICS_E myTopicEnum;
        bool echo_response; // used for received request topics

};
void*  pthreadToProcReaderEvents(void  * readerThreadInfo);


class WriterEventsThreadInfo {
    // Optional - for topics you wish to monitor writer status events 
    // Only triggers on writer event - refer to enum DDS_StatusKind
    // related to Writer/Reader entities
    // This struct holds info needed for the pthread for
    // writer waitset events (no data) processing 
    public:
        WriterEventsThreadInfo(enum TOPICS_E topicEnum);
        enum TOPICS_E topic_enum();

        DDSDynamicDataWriter * writer;
    private:
        enum TOPICS_E myTopicEnum;
};
void*  pthreadToProcWriterEvents(void  * writerEventsThreadInfo);

class PeriodicWriterThreadInfo {
    // Info struct for PeriodicPublishThread (Heartbeat or other status data flow pattern)
    // (tms Microgrid Standard section 4.9.2.1 - Status Flow Pattern)
    // After enabled will send topic at a fixed rate
    public:
        PeriodicWriterThreadInfo(enum TOPICS_E topicEnum, DDS_Duration_t ratePeriod);
        DDS_Duration_t pubRatePeriod();
        enum TOPICS_E topic_enum();

        DDSDynamicDataWriter * writer;
        DDS_DynamicData * periodicData;
        bool enabled;     

    private:
        DDS_Duration_t myRatePeriod;
        enum TOPICS_E myTopicEnum;
};
void*  pthreadPeriodicWriter(void  * periodic_writer_thread_info);

class OnChangeWriterThreadInfo {
    // Info struct for Change of State PublishThread 
    // (tms Microgrid Standard section 4.9.2.1 Status Flow)
    // This pattern is a broader version of the periodic publish pattern
    // Here a trigger or DDS guard condition is triggered externally which 
    // unblocks the thread
    // After enabled will send topic at a fixed rate
    public:
        OnChangeWriterThreadInfo(enum TOPICS_E topicEnum, DDSGuardCondition *guard_condition);  // pass in trigger event
        enum TOPICS_E topic_enum();
        DDSGuardCondition* my_guard_condition();

        DDSDynamicDataWriter * writer;
        DDS_DynamicData * changeStateData;
        bool enabled;
    private:
        enum TOPICS_E myTopicEnum;
        DDSGuardCondition *myGuardCondition;
};
void*  pthreadOnChangeWriter(void  * on_change_writer_thread_info);


#endif