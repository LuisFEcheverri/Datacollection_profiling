/* ------------------------------------------------------------------------------*
 * "THE BEER-WARE LICENSE" (Revision 42):                                        *
 * <luis.echeverri6408@gmail.com>, <manuelj6@gmail.com> <alberthm12@gmail.com>   *
 * wrote this file. As long as you retain this notice you can do whatever you    *
 * want with this stuff. If we meet some day, and you think this stuff is worth  *
 * it, you can buy us a beer in return                                           *
 * ------------------------------------------------------------------------------*/
 
/* ------------------------------------------------------------------------------* 
 *  File:         Datacollection                                                 *
 *  Desined by:   Luis Felipe Echeverri                                          *
 *                Alberth Moreno                                                 *
 * 				  Juan Manuel Gomez                                              *
 *				                                                                 *
 *  Description:  This code implements a simple datacollection device, with      *
 *                possibility to change sampletime through configuration system  *
 *-------------------------------------------------------------------------------*/  

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <time.h>
#include <kaa.h>
#include <platform/kaa_client.h>
#include <kaa_error.h>
#include <kaa_configuration_manager.h>
#include <kaa_logging.h>
#include <gen/kaa_logging_gen.h>
#include <platform/kaa_client.h>
#include <utilities/kaa_log.h>
#include <platform-impl/common/ext_log_upload_strategies.h>

/*-------------------------------------------------------------------------------*
 * libraries and macros for Profiling example                                    *
 *-------------------------------------------------------------------------------*/ 
#include <extensions/profile/kaa_profile.h>
#include <kaa/gen/kaa_profile_gen.h> 
/*-------------------------------------------------------------------------------*
 * libraries for notification                                                    *
 *-------------------------------------------------------------------------------*/ 
#include <extensions/notification/kaa_notification_manager.h>
#include <kaa/platform/ext_notification_receiver.h>

#define KAA_EXAMPLE_PROFILE_ID "Sampleid"
#define KAA_EXAMPLE_OS_VERSION "1.0"
#define KAA_EXAMPLE_BUILD_INFO "3cbaf67e"
#define KAA_EXAMPLE_COMPANY_NAME "UCO"
#define KAA_EXAMPLE_COMPANY_LOCATION "Rionegro"
#define KAA_EXAMPLE_AREA "Electronica"
#define KAA_EXAMPLE_LINE 1
#define KAA_EXAMPLE_DEVICE_POSITION 1 

static int32_t sample_period;
extern kaa_error_t ext_unlimited_log_storage_create(void **log_storage_context_p, kaa_logger_t *logger);

/*-------------------------------------------------------------------------------*
 * Macros                                                                        *
 *-------------------------------------------------------------------------------*/  
#define KAA_DEMO_UNUSED(x)(void)(x);
#define KAA_DEMO_RETURN_IF_ERROR(error, message)\
    if ((error)) {\
        printf(message ", error code %d\n", (error));\
        return (error);\
    }
/*-------------------------------------------------------------------------------*
 * copy_string function                                                          *
 *-------------------------------------------------------------------------------*/ 
void copy_string(char d[], char s[]) {
   int c = 0;
 
   while (s[c] != '\0') {
      d[c] = s[c];
      c++;
   }
   d[c] = '\0';
}
/*-------------------------------------------------------------------------------*
 * on_configuration_updated: This function receives new configuration data.      *
 *-------------------------------------------------------------------------------*/
static kaa_error_t on_configuration_updated(void *context, const kaa_root_configuration_t *conf)
{
    (void) context;
    printf("Received configuration data. New sample period: %d seconds\n", conf->sample_period);
    sample_period = conf->sample_period;
    return KAA_ERR_NONE;
}
/*-------------------------------------------------------------------------------*
 * on_notification function                                                      *  
 *-------------------------------------------------------------------------------*/ 
void on_notification(void *context, uint64_t *topic_id, kaa_notification_t *notification)
{
  uint8_t *ptr=NULL;
	KAA_DEMO_UNUSED(context);
  printf("\nReceived notification on topic:");
  printf("\nIdHigh: %d", (uint32_t)(((*topic_id)>>32) & 0xFFFFFFFF));
  printf("\nIdLow:  %d", (uint32_t)((*topic_id) & 0xFFFFFFFF));
  printf("\nMessage:");
  printf(notification->message->data);
}

/*-------------------------------------------------------------------------------*
 * Read the availaible topic                                                     *
 *-------------------------------------------------------------------------------*/ 
 void on_topic_list_uploaded(void *context, kaa_list_t *topics)
{
	printf("\non topic list uploaded");
	KAA_DEMO_UNUSED(context);
    kaa_list_node_t *it = kaa_list_begin(topics);
    while (it) {
         kaa_topic_t *topic = (kaa_topic_t *) kaa_list_get_data(it);
         if (topic->subscription_type == OPTIONAL_SUBSCRIPTION){
			     printf("\ntype: OPTIONAL\n");
		     }
         else{ 
			     printf("\ntype: MANDATORY\n");
	       } 
	       printf("\ntype: ");
         printf(topic->name);
         printf("\nIdHigh: %d,", (uint32_t)(((topic->id)>>32) & 0xFFFFFFFF));
         printf("\nIdLow:  %d, \n", (uint32_t)((topic->id) & 0xFFFFFFFF));
         it = kaa_list_next(it);
    }
}
/*-------------------------------------------------------------------------------*
 * get_temperature_sample: this function returns ADC value in voltage range      * 
 *                         from 0 to 3.3, is used for emulates temperature       * 
 *                         the ADC is 10bits                                     *
 *-------------------------------------------------------------------------------*/
static float get_temperature_sample(void)
{
    /* For the sake of example, ADC data is used */   
    uint16_t adc = system_adc_read();
    printf("ADC value: %d \n", adc);
    return (((float)(adc))*3.3/1024)+21;
}
/*-------------------------------------------------------------------------------*
 * periodically_callback: called by Kaa SDK once per second.                     *
 *-------------------------------------------------------------------------------*/
static void periodically_callback(void *context)
{
    static uint8_t current_time = 0;
    current_time++;
    /* Respect sample period */
    if ( current_time >= sample_period) {
    current_time=0;
        float temperature = get_temperature_sample();
        printf("Sampled temperature is close to: %d\n", (int)temperature);
        kaa_user_log_record_t *log_record = kaa_logging_data_collection_schema_create();
        log_record->temperature = temperature;
        kaa_logging_add_record(kaa_client_get_context(context)->log_collector, log_record, NULL);
    }
}

int main(void)
{
    /* Prepare Kaa client. */
    uint32_t topic_listener_id = 0;
    uint32_t notification_listener_id = 0;
    kaa_client_t *kaa_client = NULL;
    kaa_error_t error = kaa_client_create(&kaa_client, NULL);
    if (error) {
        return EXIT_FAILURE;
    }
/*-------------------------------------------------------------------------------* 
 * Create/update profile                                                         *                 
 *-------------------------------------------------------------------------------*/                   
	   kaa_profile_client_side_counter_profile_t *profile = kaa_profile_client_side_counter_profile_create(); //check kaa/src/kaa/gen/
     profile->company_location = kaa_string_move_create(KAA_EXAMPLE_COMPANY_LOCATION, NULL);
     profile->company_name = kaa_string_move_create(KAA_EXAMPLE_COMPANY_NAME, NULL);
	   profile->area = kaa_string_move_create(KAA_EXAMPLE_AREA, NULL);
     profile->line = KAA_EXAMPLE_LINE;	 
	   profile->position = KAA_EXAMPLE_DEVICE_POSITION;	
     profile->id = kaa_string_move_create(KAA_EXAMPLE_PROFILE_ID, NULL);
     profile->os = ENUM_OS_FreeRTOS;
     profile->os_version = kaa_string_move_create(KAA_EXAMPLE_OS_VERSION, NULL);
     profile->build = kaa_string_move_create(KAA_EXAMPLE_BUILD_INFO, NULL); 
     profile->type =ENUM_DEVICE_TYPE_Register;
     error = kaa_profile_manager_update_profile(kaa_client_get_context(kaa_client)->profile_manager, profile);
     /* Check error code */
     profile->destroy(profile);	
/*-------------------------------------------------------------------------------*    
 * Configure notification manager.                                               *     
 *-------------------------------------------------------------------------------*/
      /* Get available topics*/
      kaa_list_t *topics_list = NULL;
      error = kaa_get_topics(kaa_client_get_context(kaa_client)->notification_manager, &topics_list);
      on_topic_list_uploaded(NULL, topics_list);
   
      /* Subscribe to updates on available topics*/
      kaa_topic_listener_t topic_listener = { &on_topic_list_uploaded, NULL };
  
      /* Add listener*/
       error= kaa_add_topic_list_listener(kaa_client_get_context(kaa_client)->notification_manager,  
                                                                                   &topic_listener, 
                                                                                   &topic_listener_id);
      /* Default notification listener*/
      kaa_notification_listener_t notification_listener = { &on_notification, NULL };
  
      /* Add listener*/
      error = kaa_add_notification_listener(kaa_client_get_context(kaa_client)->notification_manager, 
                                                                                    &notification_listener,
                                                                                    &notification_listener_id);
/*-------------------------------------------------------------------------------*
 *  Subscribe optional topic (you need know the topic id)                        *
 *  Assume we have some optional topic                                           *
 *-------------------------------------------------------------------------------*/ 
			  // (void)kaa_subscribe_to_topic(kaa_client_get_context(kaa_client)->notification_manager,&(topic->id),1);
/*-------------------------------------------------------------------------------*	
 * Configure configuration manager.                                              *
 *-------------------------------------------------------------------------------*/
      kaa_configuration_root_receiver_t receiver = {
          .context = NULL,
          .on_configuration_updated = on_configuration_updated
      };
      error = kaa_configuration_manager_set_root_receiver(
          kaa_client_get_context(kaa_client)->configuration_manager,
          &receiver);
      if (error) {
          return EXIT_FAILURE;
      }
      /* Obtain default configuration shipped within SDK. */
      const kaa_root_configuration_t *dflt = kaa_configuration_manager_get_configuration(
          kaa_client_get_context(kaa_client)->configuration_manager);
      printf("Default sample period: %d seconds\n", dflt->sample_period);
      sample_period = dflt->sample_period;
/*-------------------------------------------------------------------------------*
 * Configure data collection.                                                    *
 *-------------------------------------------------------------------------------*/
      void *log_storage_context         = NULL;
      void *log_upload_strategy_context = NULL;
      /* The internal memory log storage distributed with Kaa SDK. */
      error = ext_unlimited_log_storage_create(&log_storage_context,
          kaa_client_get_context(kaa_client)->logger);
      if (error) {
          return EXIT_FAILURE;
      }
      /* Create a strategy based on timeout. */
      error = ext_log_upload_strategy_create(
          kaa_client_get_context(kaa_client), &log_upload_strategy_context,
          KAA_LOG_UPLOAD_BY_TIMEOUT_STRATEGY);
      if (error) {
          return EXIT_FAILURE;
      }
      /* Strategy will upload logs every 5 seconds. */
      error = ext_log_upload_strategy_set_upload_timeout(log_upload_strategy_context, 5);
      if (error) {
          return EXIT_FAILURE;
      }
      /* Specify log bucket size constraints. */
      kaa_log_bucket_constraints_t bucket_sizes = {
           .max_bucket_size       = 32,   /* Bucket size in bytes. */
           .max_bucket_log_count  = 2,    /* Maximum log count in one bucket. */
      };
      /* Initialize the log storage and strategy (by default, they are not set). */
      error = kaa_logging_init(kaa_client_get_context(kaa_client)->log_collector,
          log_storage_context, log_upload_strategy_context, &bucket_sizes);
      if (error) {
          return EXIT_FAILURE;
      }
/*-------------------------------------------------------------------------------*
 * Start Kaa SDK's main loop.                                                    *
 * periodically_callback is called once per second.                              *
 *-------------------------------------------------------------------------------*/
    error = kaa_client_start(kaa_client, periodically_callback, kaa_client, 1);
    /* Should get here only after Kaa stops. */
    kaa_client_destroy(kaa_client);
    
    if (error) {
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}