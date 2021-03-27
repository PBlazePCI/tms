#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <signal.h>
#include <iostream>

#include "ndds/ndds_cpp.h"
#include <pthread.h>

#include "tmsCommPatterns.h"


ReaderThreadInfo::ReaderThreadInfo(enum TOPICS_E topicEnum, bool echoResponse) 
        {
            myTopicEnum = topicEnum;
            echo_response = echoResponse; // default not to echo a response (rcv'd type not a request) 
            reqRspWriter = NULL;  // initialize to NULL and perform a checks if the user requres an echoResponse
        }

bool    ReaderThreadInfo::echoReqResponse() { return echo_response; }

std::string ReaderThreadInfo::me(){ return topic_name_array[myTopicEnum]; }
enum TOPICS_E ReaderThreadInfo::topic_enum() { return myTopicEnum; };

void*  pthreadToProcReaderEvents(void *reader_thread_info) {
    ReaderThreadInfo * myReaderThreadInfo;
    myReaderThreadInfo = (ReaderThreadInfo *)reader_thread_info;
	DDSStatusCondition *status_condition =  NULL;
	DDSReadCondition * read_condition = NULL;
	DDSWaitSet *waitset = new DDSWaitSet();
    DDS_ReturnCode_t retcode, retcode1, retcode2, retcode3;
    DDSConditionSeq active_conditions_seq;
	DDS_DynamicDataSeq data_seq;
	DDS_SampleInfoSeq info_seq;
    tms_SampleId tms_sample_id; // use microgrid def from model tmsTestExample.h
    DDS_UnsignedLong fingerprint_len = (DDS_UnsignedLong) tms_LEN_Fingerprint; // the get_octet_array requires this non-const
    DDS_DynamicData * request_response_data = NULL;

    std::cout << "Created Reader Pthread: " << myReaderThreadInfo->me() << " Topic" << std::endl;


    if (myReaderThreadInfo->echoReqResponse()) {  // If response enabled, create the writer data
        if (myReaderThreadInfo->reqRspWriter == NULL) {
            std::cerr << "Reader thread: Response enabled, but no writer assigned"  << std::endl;
            goto end_reader_thread;
        }
    
        // create a data sample - do I need to dispose this if I use a different key each time?
        request_response_data = myReaderThreadInfo->reqRspWriter->create_data(DDS_DYNAMIC_DATA_PROPERTY_DEFAULT);
        if (request_response_data == NULL) {
            std::cerr << "Reader thread: request_response_data: create_data error"
            << retcode << std::endl << std::flush;
            goto end_reader_thread;
        }
    }
    
    // Create read condition
    read_condition = myReaderThreadInfo->reader->create_readcondition(
        DDS_NOT_READ_SAMPLE_STATE,
        DDS_ANY_VIEW_STATE,
        DDS_ANY_INSTANCE_STATE);
    if (read_condition == NULL) {
        printf("Reader thread: create_readcondition error\n");
		goto end_reader_thread;
    }

    //  Get status conditions
    status_condition = myReaderThreadInfo->reader->get_statuscondition();
    if (status_condition == NULL) {
        printf("Reader thread: get_statuscondition error\n");
 		goto end_reader_thread;
    }

    // Set enabled statuses
    retcode = status_condition->set_enabled_statuses(DDS_SUBSCRIPTION_MATCHED_STATUS);
    if (retcode != DDS_RETCODE_OK) {
        printf("Reader thread: set_enabled_statuses error\n");
 		goto end_reader_thread;
    }   

    /* Attach Read Conditions */
    retcode = waitset->attach_condition(read_condition);
    if (retcode != DDS_RETCODE_OK) {
        printf("Reader thread: attach_condition error\n");
		goto end_reader_thread;
    }

    /* Attach Status Conditions */
    retcode = waitset->attach_condition(status_condition);
    if (retcode != DDS_RETCODE_OK) {
        printf("Reader thread: attach_condition error\n");
		goto end_reader_thread;
    }

	while (run_flag) {
       	retcode = waitset->wait(active_conditions_seq, DDS_DURATION_INFINITE);
        if (retcode == DDS_RETCODE_TIMEOUT) {
            printf("Reader thread: Wait timed out!! No conditions were triggered.\n");
            continue;
        } else if (retcode != DDS_RETCODE_OK) {
            printf("Reader thread:  wait returned error: %d\n", retcode);
            goto end_reader_thread;
        }

        int active_conditions = active_conditions_seq.length();

        for (int i = 0; i < active_conditions; ++i) {
            if (active_conditions_seq[i] == status_condition) {
                /* Get the status changes so we can check which status
                 * condition triggered. */
                DDS_StatusMask triggeredmask =
                        myReaderThreadInfo->reader->get_status_changes();

                /* Subscription matched */
                if (triggeredmask & DDS_SUBSCRIPTION_MATCHED_STATUS) {
                    DDS_SubscriptionMatchedStatus st;
                    myReaderThreadInfo->reader->get_subscription_matched_status(st);
                    std::cout << myReaderThreadInfo->me() << "Reader Pubs: " 
                    << st.current_count << "  " << st.current_count_change << std::endl;
                }
            } else if (active_conditions_seq[i] == read_condition) { 
                // Get the latest samples
				retcode = myReaderThreadInfo->reader->take(
							data_seq, info_seq, DDS_LENGTH_UNLIMITED,
							DDS_ANY_SAMPLE_STATE, DDS_ANY_VIEW_STATE, DDS_ANY_INSTANCE_STATE);
				if (retcode == DDS_RETCODE_OK) {
                    // we've got some data for what ever topic we recieved, figure that out, make an
                    // internal variable change as a result (if that's the case) and respond accordingly 
                    // (with a RequestResponse not an On Change Topic. On Change topics trigger from the 
                    // main loop as you peruse through internal variables that you see have changed as a
                    // result of a request or other internal event.
					for (int i = 0; i < data_seq.length(); ++i) {
						if (info_seq[i].valid_data) {  
                            if (retcode != DDS_RETCODE_OK) goto end_reader_thread;

                            // what topic did we receive -  i.e. what topic is associated with this thread
                            switch  (myReaderThreadInfo->topic_enum()) {  
                                case  tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST_ENUM: // MSM Sim receives this from Device
                                    if (!myReaderThreadInfo->echoReqResponse()) {   // this topic requires and Response
                                        std::cerr << "tms_TOPIC_MICROGRID_MEMBERSHIP_REQUEST resquires Response" << std::endl << std::flush;
                                        goto end_reader_thread;
                                    }
                                    std::cout << "Received Topic Membership Request (should check for MM_JOIN/LEAVE) " << std::endl;
                                    
                                    // Send the Request Response here while we have context of the request
                                    // Get the SampleID and build and send RequestResponse here
                                    retcode = data_seq[i].get_octet_array(
                                        tms_sample_id.deviceId,
                                         &fingerprint_len,
                                         "requestId.deviceId",
                                         DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED
                                         );
                                    retcode1 = data_seq[i].get_ulonglong(
                                        tms_sample_id.sequenceNumber,
                                        "requestId.sequenceNumber",
                                        DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED);
                                    if (retcode != DDS_RETCODE_OK || retcode1 != DDS_RETCODE_OK) {
                                        std::cout << "Reader Thread: get_data error\n" << std::endl;
                                        goto end_reader_thread;
                                    }

                                    // At this point we've verified a required response and writer is valid
                                    // so send it!
                                    retcode = request_response_data->set_octet_array(
                                        "relatedRequestId.deviceId", 
                                        DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED,
                                        tms_LEN_Fingerprint, 
                                        (const DDS_Octet *)&tms_sample_id.deviceId
                                        );
                                    retcode1 = request_response_data->set_ulonglong(
                                        "relatedRequestId.sequenceNumber",
                                        DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED,
                                        tms_sample_id.sequenceNumber
                                        );
                                    retcode2 = request_response_data->set_ulong(
                                        "status.code",
                                        DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED,
                                        tms_REPLY_OK
                                        );
                                    retcode3 = request_response_data->set_string(
                                        "status.reason",
                                        DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED,
                                        "Hello World"
                                        );
                                    if (retcode != DDS_RETCODE_OK || retcode1 != DDS_RETCODE_OK || retcode2 != DDS_RETCODE_OK || retcode3 != DDS_RETCODE_OK) {
                                        std::cout << "Reader Thread: set_data error\n" << std::endl;
                                        goto end_reader_thread;
                                    }
                                    myReaderThreadInfo->reqRspWriter->write(* request_response_data, DDS_HANDLE_NIL);
                                    if (retcode != DDS_RETCODE_OK) {
                                        std::cerr << "Reader Thread: RequestResponce Membership request write Error " << std::endl << std::flush;
                                        goto end_reader_thread;
                                    }
                                    // if we responded tms_REPLY_OK then we should set the internal variable as MMR_COMPLETE
                                    // the mail_loop of the MSM should now see a difference between the internal state and the tms_state 
                                    // causing an On Change tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME to get triggered
                                    internal_membership_result = MMR_COMPLETE;
                                    std::cout << "Membership Result Set: " << internal_membership_result << " ext: " << external_tms_membership_result << std::endl;
                                    break;
                                case  tms_TOPIC_REQUEST_RESPONSE_ENUM:
                                    std::cout << "Received Request Response Topic" << std::endl;
                                    break;
                                default: 
                                    std::cout << "Received unhandled Topic - default topic fall through" << std::endl;
                                    break;
                            }              
						
						}
					}
				} else if (retcode == DDS_RETCODE_NO_DATA) {
					continue;
				} else {
					fprintf(stderr, "Reader thread: read data error %d\n", retcode);
					goto end_reader_thread;
				}
                retcode = myReaderThreadInfo->reader->return_loan(data_seq, info_seq);
                if (retcode != DDS_RETCODE_OK) {
                    fprintf(stderr, "Reader thread:return_loan error %d\n", retcode);
                    goto end_reader_thread;
                }  
			}
		}
	} // While (run_flag)
	end_reader_thread: // reached by ^C or an error
	std::cout << myReaderThreadInfo->me() << " Reader: Pthread Exiting" << std::endl;
	exit(0);
}

// WriterEventsThreadInfo member functions
WriterEventsThreadInfo::WriterEventsThreadInfo(enum TOPICS_E topicEnum) 
        {
            myTopicEnum = topicEnum;
        }

std::string WriterEventsThreadInfo::me(){ return topic_name_array[myTopicEnum]; }
enum TOPICS_E WriterEventsThreadInfo::topic_enum() {return myTopicEnum; };


void*  pthreadToProcWriterEvents(void  * writerEventsThreadInfo) {
	WriterEventsThreadInfo * myWriterEventsThreadInfo;
    myWriterEventsThreadInfo = (WriterEventsThreadInfo *)writerEventsThreadInfo;
	DDSWaitSet * waitset = waitset = new DDSWaitSet();;
    DDS_ReturnCode_t retcode;
    DDSConditionSeq active_conditions_seq;

    std::cout << "Created Writer Pthread: " << myWriterEventsThreadInfo->me() << " Topic" << std::endl;

    // Configure Waitset for Writer Status ****
    DDSStatusCondition *status_condition = myWriterEventsThreadInfo->writer->get_statuscondition();
    if (status_condition == NULL) {
        printf("Writer thread: get_statuscondition error\n");
        goto end_writer_thread;
    }

    // Set enabled statuses
    retcode = status_condition->set_enabled_statuses(
            DDS_PUBLICATION_MATCHED_STATUS);
    if (retcode != DDS_RETCODE_OK) {
        printf("Writer thread: set_enabled_statuses error\n");
        goto end_writer_thread;
    }

    // Attach Status Conditions to the above waitset
    retcode = waitset->attach_condition(status_condition);
    if (retcode != DDS_RETCODE_OK) {
        printf("Writer thread: attach_condition error\n");
        goto end_writer_thread;
    }

    // wait() blocks execution of the thread until one or more attached condition triggers  
	// thread exits upon ^c or error
    while (run_flag) { 
        retcode = waitset->wait(active_conditions_seq, DDS_DURATION_INFINITE);
        /* We get to timeout if no conditions were triggered */
        if (retcode == DDS_RETCODE_TIMEOUT) {
            printf("Writer thread: Wait timed out!! No conditions were triggered.\n");
            continue;
        } else if (retcode != DDS_RETCODE_OK) {
            printf("Writer thread: wait returned error: %d\n", retcode);
            goto end_writer_thread;
        }

        /* Get the number of active conditions */
        int active_conditions = active_conditions_seq.length();

        for (int i = 0; i < active_conditions; ++i) {
            /* Compare with Status Conditions */
            if (active_conditions_seq[i] == status_condition) {
                DDS_StatusMask triggeredmask =
                        myWriterEventsThreadInfo->writer->get_status_changes();

                if (triggeredmask & DDS_PUBLICATION_MATCHED_STATUS) {
					DDS_PublicationMatchedStatus st;
                	myWriterEventsThreadInfo->writer->get_publication_matched_status(st);
					std::cout << myWriterEventsThreadInfo->me() << " Writer Subs: " 
                    << st.current_count << "  " << st.current_count_change << std::endl;
                }
            } else {
                // writers can only have status condition
                std::cout << myWriterEventsThreadInfo->me() << " Writer: False Writer Event Trigger" << std::endl;
            }
        }
	} // While (run_flag)
	end_writer_thread: // reached by ^C or an error
	std::cout << myWriterEventsThreadInfo->me() << " Writer: Pthread Exiting"<< std::endl;
	exit(0);
}

// PeriodicPublishThreadInfo member functions
PeriodicWriterThreadInfo::PeriodicWriterThreadInfo (enum TOPICS_E topicEnum, DDS_Duration_t ratePeriod) 
        {
            enabled = false; //initialize disabled
            myRatePeriod = ratePeriod;
            myTopicEnum = topicEnum;
        }

DDS_Duration_t PeriodicWriterThreadInfo::pubRatePeriod() { return myRatePeriod; };
enum TOPICS_E PeriodicWriterThreadInfo::topic_enum() {return myTopicEnum; };

std::string PeriodicWriterThreadInfo::me(){ return topic_name_array[myTopicEnum]; }

void*  pthreadPeriodicWriter(void  * periodic_writer_thread_info) {
	PeriodicWriterThreadInfo * myPeriodicPublishThreadInfo;
    myPeriodicPublishThreadInfo = (PeriodicWriterThreadInfo *) periodic_writer_thread_info;
	DDSWaitSet * waitset = waitset = new DDSWaitSet();;
    DDS_ReturnCode_t retcode;
    DDSConditionSeq active_conditions_seq;
    long int seq_count = 0;

    std::cout << "Created Periodic Publisher Pthread: " << myPeriodicPublishThreadInfo->me() << " Topic" << std::endl;

    // Configure Waitset for Writer Status ****
    DDSStatusCondition *status_condition = myPeriodicPublishThreadInfo->writer->get_statuscondition();
    if (status_condition == NULL) {
        printf("Writer thread: get_statuscondition error\n");
        goto end_writer_thread;
    }

    // Set enabled statuses
    retcode = status_condition->set_enabled_statuses(
            DDS_PUBLICATION_MATCHED_STATUS);
    if (retcode != DDS_RETCODE_OK) {
        printf("Writer thread: set_enabled_statuses error\n");
        goto end_writer_thread;
    }

    // Attach Status Conditions to the above waitset
    retcode = waitset->attach_condition(status_condition);
    if (retcode != DDS_RETCODE_OK) {
        printf("Writer thread: attach_condition error\n");
        goto end_writer_thread;
    }

    // wait() blocks execution of the thread until one or more attached condition triggers  
	// thread exits upon ^c or error
    while (run_flag) { 
        retcode = waitset->wait(active_conditions_seq, myPeriodicPublishThreadInfo->pubRatePeriod());
        /* We get to timeout if no conditions were triggered */
        if (retcode == DDS_RETCODE_TIMEOUT) {
            if (myPeriodicPublishThreadInfo->enabled) {
                switch (myPeriodicPublishThreadInfo->topic_enum()) {
                    case  tms_TOPIC_HEARTBEAT_ENUM: 
                        std::cout << "Periodic Writer - Heartbeat " << seq_count << std::endl;
                        retcode = myPeriodicPublishThreadInfo->periodicData->set_ulong("sequenceNumber", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED, seq_count);
                        if (retcode != DDS_RETCODE_OK) {
                            std::cerr << "heartbeat: Dynamic Data Set Error" << std::endl << std::flush;
                            break;
                        }
                        myPeriodicPublishThreadInfo->writer->write(* myPeriodicPublishThreadInfo->periodicData, DDS_HANDLE_NIL);
                        seq_count++; // increment seq_count here so 1) it starts at 0 as prescribed by TMS, 2) changes once per write of heartbeat
                        break;
                    default: 
                        std::cout << "Periodic Writer - default topic fall through" << std::endl;
                        break;
                }
            }
            continue; // no need to process active conditions if timeout
        } else if (retcode != DDS_RETCODE_OK) {
            printf("Writer thread: wait returned error: %d\n", retcode);
            goto end_writer_thread;
        }

        /* Get the number of active conditions */
        int active_conditions = active_conditions_seq.length();

        for (int i = 0; i < active_conditions; ++i) {
            /* Compare with Status Conditions */
            if (active_conditions_seq[i] == status_condition) {
                DDS_StatusMask triggeredmask =
                        myPeriodicPublishThreadInfo->writer->get_status_changes();

                if (triggeredmask & DDS_PUBLICATION_MATCHED_STATUS) {
					DDS_PublicationMatchedStatus st;
                	myPeriodicPublishThreadInfo->writer->get_publication_matched_status(st);
					std::cout << myPeriodicPublishThreadInfo->me() << " Writer Subs: " 
                    << st.current_count << "  " << st.current_count_change << std::endl;
                }
            } else {
                // writers can only have status condition
                std::cout << myPeriodicPublishThreadInfo->me() << " Writer: False Writer Event Trigger" << std::endl;
            }
        }
	} // While (run_flag)
	end_writer_thread: // reached by ^C or an error
	std::cout << myPeriodicPublishThreadInfo->me() << " Writer: Pthread Exiting"<< std::endl;
	exit(0);
}

// On Change State PublishThreadInfo member functions
OnChangeWriterThreadInfo::OnChangeWriterThreadInfo (enum TOPICS_E topicEnum, DDSGuardCondition * guard_condition) 
        {

            myTopicEnum = topicEnum;
            myGuardCondition=guard_condition;
        }

enum TOPICS_E OnChangeWriterThreadInfo::topic_enum() {return myTopicEnum; };
DDSGuardCondition* OnChangeWriterThreadInfo::my_guard_condition() { return myGuardCondition; };

std::string OnChangeWriterThreadInfo::me(){ return topic_name_array[myTopicEnum]; }

void*  pthreadOnChangeWriter(void  * on_change_writer_thread_info) {
	OnChangeWriterThreadInfo * myOnChangeWriterThreadInfo;
    myOnChangeWriterThreadInfo = (OnChangeWriterThreadInfo *) on_change_writer_thread_info;
	DDSWaitSet * waitset = waitset = new DDSWaitSet();;
    DDS_ReturnCode_t retcode;
    DDSConditionSeq active_conditions_seq;

    std::cout << "Created On Change State Writer Pthread: " << myOnChangeWriterThreadInfo->me() << " Topic" << std::endl;

    // Configure Waitset for Writer Status ****
    DDSStatusCondition *status_condition = myOnChangeWriterThreadInfo->writer->get_statuscondition();
    if (status_condition == NULL) {
        printf("On Change writer thread: get_statuscondition error\n");
        goto end_on_change_thread;
    }

    // Set enabled statuses
    retcode = status_condition->set_enabled_statuses(
            DDS_PUBLICATION_MATCHED_STATUS);
    if (retcode != DDS_RETCODE_OK) {
        printf("On Change writer thread: set_enabled_statuses error\n");
        goto end_on_change_thread;
    }
      // Attach Status Conditions to the above waitset
    retcode = waitset->attach_condition(status_condition);
    if (retcode != DDS_RETCODE_OK) {
        printf("On Change writer thread: attach_condition error\n");
        goto end_on_change_thread;
    }

    // Attach Status Conditions to the above waitset
    retcode = waitset->attach_condition(myOnChangeWriterThreadInfo->my_guard_condition());
    if (retcode != DDS_RETCODE_OK) {
        printf("On Change writer thread: attach_guard_condition error\n");
        goto end_on_change_thread;
    }

    // wait() blocks execution of the thread until one or more attached condition triggers  
	// thread exits upon ^c or error
    while (run_flag) { 
        retcode = waitset->wait(active_conditions_seq, DDS_DURATION_INFINITE);
        /* We get to timeout if no conditions were triggered */
        if (retcode == DDS_RETCODE_TIMEOUT) {
            
            continue; // no need to process active conditions if timeout
        } else if (retcode != DDS_RETCODE_OK) {
            printf("On Change writer thread: wait returned error: %d\n", retcode);
            goto end_on_change_thread;
        }

        /* Get the number of active conditions */
        int active_conditions = active_conditions_seq.length();

        for (int i = 0; i < active_conditions; ++i) {
            /* Compare with Status Conditions */
            if (active_conditions_seq[i] == status_condition) {
                DDS_StatusMask triggeredmask =
                        myOnChangeWriterThreadInfo->writer->get_status_changes();

                if (triggeredmask & DDS_PUBLICATION_MATCHED_STATUS) {
					DDS_PublicationMatchedStatus st;
                	myOnChangeWriterThreadInfo->writer->get_publication_matched_status(st);
					std::cout << myOnChangeWriterThreadInfo->me() << " Writer Subs: " 
                    << st.current_count << "  " << st.current_count_change << std::endl;
                }
            } else if (active_conditions_seq[i] == myOnChangeWriterThreadInfo->my_guard_condition()) {
                if (myOnChangeWriterThreadInfo->enabled) {
                    switch (myOnChangeWriterThreadInfo->topic_enum()) {
                    
                        case  tms_TOPIC_HEARTBEAT_ENUM: 
                            // Example code where I make heartbeat an On Change writer vs Periodic - get sequence number for display
                            DDS_UnsignedLong mySeqNum; // sequence is set/incremented in main loop where the condit trigger is set
                            retcode = myOnChangeWriterThreadInfo->changeStateData-> \
                                get_ulong(mySeqNum, "sequenceNumber", DDS_DYNAMIC_DATA_MEMBER_ID_UNSPECIFIED);
                            if (retcode != DDS_RETCODE_OK) {
                                printf("On Change writer thread: get_data error\n");
                                goto end_on_change_thread;
                            }
                            std::cout << "On Change writer thread - Heartbeat " <<  mySeqNum << std::endl;

                            myOnChangeWriterThreadInfo->writer->write(* myOnChangeWriterThreadInfo->changeStateData, DDS_HANDLE_NIL);
                            // Need to set this false after processing - else it just retriggers immediately
                            retcode = myOnChangeWriterThreadInfo->my_guard_condition()->set_trigger_value(DDS_BOOLEAN_FALSE);
                            if (retcode != DDS_RETCODE_OK) {
                                printf("On Change writer thread: set_enabled_guard error\n");
                                goto end_on_change_thread;
                            }
                            break;

                        case  tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME_ENUM:

                            std::cout << "On Change writer thread - tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME " <<  mySeqNum << std::endl;

                            myOnChangeWriterThreadInfo->writer->write(* myOnChangeWriterThreadInfo->changeStateData, DDS_HANDLE_NIL);
                            // Need to set this false after processing - else it just retriggers immediately
                            retcode = myOnChangeWriterThreadInfo->my_guard_condition()->set_trigger_value(DDS_BOOLEAN_FALSE);
                            if (retcode != DDS_RETCODE_OK) {
                                printf("On Change writer thread tms_TOPIC_MICROGRID_MEMBERSHIP_OUTCOME: set_enabled_guard error\n");
                                goto end_on_change_thread;
                            }
                            break;
                                
                        default: 
                            std::cout << "On Change writer thread - default topic fall through" << std::endl;
                            break;
                    }
                }
            } else {
                // writers can only have status condition
                std::cout << myOnChangeWriterThreadInfo->me() << " Writer: False Writer Event Trigger" << std::endl;
            }
        }
	} // While (run_flag)
	end_on_change_thread: // reached by ^C or an error
	std::cout << myOnChangeWriterThreadInfo->me() << "On Change writer thread: Pthread Exiting"<< std::endl;
	exit(0);
}

