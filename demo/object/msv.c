/**************************************************************************
*
* Copyright (C) 2012 Steve Karg <skarg@users.sourceforge.net>
*
* Permission is hereby granted, free of charge, to any person obtaining
* a copy of this software and associated documentation files (the
* "Software"), to deal in the Software without restriction, including
* without limitation the rights to use, copy, modify, merge, publish,
* distribute, sublicense, and/or sell copies of the Software, and to
* permit persons to whom the Software is furnished to do so, subject to
* the following conditions:
*
* The above copyright notice and this permission notice shall be included
* in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*
*********************************************************************/

/* Multi-state Value Objects */

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include "bacdef.h"
#include "bacdcode.h"
#include "bacenum.h"
#include "bacapp.h"
#include "config.h"     /* the custom stuff */
#include "rp.h"
#include "wp.h"
#include "msv.h"
#include "handlers.h"
#include "ucix.h"

/* number of demo objects */
#ifndef MAX_MULTI_STATE_VALUES
#define MAX_MULTI_STATE_VALUES 254
#endif

/* When all the priorities are level null, the present value returns */
/* the Relinquish Default value */
#define MULTI_STATE_RELINQUISH_DEFAULT 0

/* NULL part of the array */
#define MULTI_STATE_NULL (255)
/* how many states? 1 to 254 states - 0 is not allowed. */
//#ifndef MULTI_STATE_NUMBER_OF_STATES
//#define MULTI_STATE_NUMBER_OF_STATES (4)
//#endif

/* we choose to have a NULL level in our system represented by */
/* a particular value.  When the priorities are not in use, they */
/* will be relinquished (i.e. set to the NULL level). */
#define MULTI_STATE_LEVEL_NULL 255

MULTI_STATE_VALUE_DESCR MSV_Descr[MAX_MULTI_STATE_VALUES];

/* These three arrays are used by the ReadPropertyMultiple handler */
static const int Multistate_Value_Properties_Required[] = {
    PROP_OBJECT_IDENTIFIER,
    PROP_OBJECT_NAME,
    PROP_OBJECT_TYPE,
    PROP_PRESENT_VALUE,
    PROP_STATUS_FLAGS,
    PROP_EVENT_STATE,
    PROP_OUT_OF_SERVICE,
    PROP_NUMBER_OF_STATES,
    -1
};

static const int Multistate_Value_Properties_Optional[] = {
    PROP_DESCRIPTION,
    PROP_PRIORITY_ARRAY,
    PROP_RELINQUISH_DEFAULT,
    PROP_STATE_TEXT,
#if defined(INTRINSIC_REPORTING)
    PROP_ALARM_VALUES,
    PROP_TIME_DELAY,
    PROP_NOTIFICATION_CLASS,
    PROP_EVENT_ENABLE,
    PROP_ACKED_TRANSITIONS,
    PROP_NOTIFY_TYPE,
    PROP_EVENT_TIME_STAMPS,
#endif
    -1
};

static const int Multistate_Value_Properties_Proprietary[] = {
    -1
};

void Multistate_Value_Property_Lists(
    const int **pRequired,
    const int **pOptional,
    const int **pProprietary)
{
    if (pRequired)
        *pRequired = Multistate_Value_Properties_Required;
    if (pOptional)
        *pOptional = Multistate_Value_Properties_Optional;
    if (pProprietary)
        *pProprietary = Multistate_Value_Properties_Proprietary;

    return;
}

void Multistate_Value_Init(
    void)
{
    unsigned i, j, k;
    const char description[64] = "";
    const char *uciname;
    const uint8_t *uci_value;
    const char *ucidescription;
    const char *ucidescription_default;
    char *ucistate_default[254][64];
    int *number_of_states_default;
    char *ucialarmstate_default[254][64];
    int *number_of_alarm_states_default;
    const char int_to_string[64] = "";
    struct uci_context *ctx;
    fprintf(stderr, "Multistate_Value_Init\n");
    ctx = ucix_init("bacnet_mv");
    if(!ctx)
        fprintf(stderr,  "Failed to load config file");

    //ucidescription_default = ucix_get_option(ctx, "bacnet_mv", "default", "Description");

    ucidescription_default = ucix_get_option(ctx, "bacnet_mv", "default", "description");
    number_of_states_default = ucix_get_list(&ucistate_default, ctx, "bacnet_mv", "default", "state");
    number_of_alarm_states_default = ucix_get_list(&ucialarmstate_default, ctx, "bacnet_mv", "default", "alarmstate");
    fprintf(stderr, "Multistate %i \n",number_of_states_default);
    /* initialize all the analog output priority arrays to NULL */
    for (i = 0; i < MAX_MULTI_STATE_VALUES; i++) {
        memset(&MSV_Descr[i], 0x00, sizeof(MULTI_STATE_VALUE_DESCR));
        /* initialize all the analog output priority arrays to NULL */
        for (j = 0; j < BACNET_MAX_PRIORITY; j++) {
            MSV_Descr[i].Priority_Array[j] = MULTI_STATE_LEVEL_NULL;
        }
        sprintf(int_to_string, "%lu", (unsigned long) i);
        uciname = ucix_get_option(ctx, "bacnet_mv", int_to_string, "name");
        if (uciname != 0) {
            ucix_string_copy(&MSV_Descr[i].Object_Name, sizeof(MSV_Descr[i].Object_Name), uciname);
            ucidescription = ucix_get_option(ctx, "bacnet_mv",
                int_to_string, "description");
            //fprintf(stderr, "UCI Description %s \n",ucidescription);
            if (ucidescription != 0) {
                sprintf(description, "%s", ucidescription);
            } else if (ucidescription_default != 0) {
                sprintf(description, "%s %lu", ucidescription_default , (unsigned long) i);
            } else {
                sprintf(description, "MV%lu no uci section configured", (unsigned long) i);
            }
            ucix_string_copy(&MSV_Descr[i].Object_Description,
                sizeof(MSV_Descr[i].Object_Description), ucidescription);

            uci_value = ucix_get_option_int(ctx, "bacnet_mv", int_to_string, "value", 1);
            MSV_Descr[i].Priority_Array[15] = uci_value;
            //Present_Value[i] = uci_value;
        } else {
            sprintf(int_to_string, "MV%lu_not_configured", (unsigned long) i);
            //fprintf(stderr, "UCI Name %s \n",int_to_string);
            ucix_string_copy(&MSV_Descr[i].Object_Name, sizeof(MSV_Descr[i].Object_Name), int_to_string);
            if (ucidescription_default != 0) {
                sprintf(description, "%s %lu", ucidescription_default , (unsigned long) i);
            } else {
                sprintf(description, "MV%lu no uci section configured", (unsigned long) i);
            }
            ucix_string_copy(&MSV_Descr[i].Object_Description,
                sizeof(MSV_Descr[i].Object_Description), description);
        }

        MSV_Descr[i].number_of_states = number_of_states_default;
        MSV_Descr[i].Relinquish_Default = 1; //TODO read uci
        for (j = 0; j < number_of_states_default; j++) {
            if (ucistate_default[j]) {
                sprintf(MSV_Descr[i].State_Text[j], ucistate_default[j]);
            } else {
                sprintf(MSV_Descr[i].State_Text[j], "STATUS: %i", j);
            }
            for (k = 0; k < number_of_alarm_states_default; k++) {
                if (strcmp(ucistate_default[j],ucialarmstate_default[k]) == 0) {
                    MSV_Descr[i].Alarm_Values[j] = j+1;
                }
            }
#if 0
            switch (j) {
                case 0:
                    sprintf(MSV_Descr[i].State_Text[j], "UP");
                    break;
                case 1:
                    sprintf(MSV_Descr[i].State_Text[j], "DOWN");
                    MSV_Descr[i].Alarm_Values[j] = j+1;
                    break;
                case 2:
                    sprintf(MSV_Descr[i].State_Text[j], "UNREACHABLE");
                    MSV_Descr[i].Alarm_Values[j] = j+1;
                    break;
                case 3:
                    sprintf(MSV_Descr[i].State_Text[j], "FLAPING");
                    MSV_Descr[i].Alarm_Values[j] = j+1;
                    break;
                default:
                    sprintf(MSV_Descr[i].State_Text[j], "STATUS: %i", j);
                    break;
            }
#endif
        }
#if defined(INTRINSIC_REPORTING)
        MSV_Descr[i].Event_State = EVENT_STATE_NORMAL;
        /* notification class not connected */
        //MSV_Descr[i].Notification_Class = BACNET_MAX_INSTANCE;
        MSV_Descr[i].Notification_Class = 0;
        MSV_Descr[i].Event_Enable = 7;
        /* initialize Event time stamps using wildcards
           and set Acked_transitions */
        for (j = 0; j < MAX_BACNET_EVENT_TRANSITION; j++) {
            datetime_wildcard_set(&MSV_Descr[i].Event_Time_Stamps[j]);
            MSV_Descr[i].Acked_Transitions[j].bIsAcked = true;
        }

        /* Set handler for GetEventInformation function */
        handler_get_event_information_set(OBJECT_MULTI_STATE_VALUE,
            Multistate_Value_Event_Information);
        /* Set handler for AcknowledgeAlarm function */
        handler_alarm_ack_set(OBJECT_MULTI_STATE_VALUE, Multistate_Value_Alarm_Ack);
        /* Set handler for GetAlarmSummary Service */
        handler_get_alarm_summary_set(OBJECT_MULTI_STATE_VALUE,
            Multistate_Value_Alarm_Summary);
#endif
    }
    ucix_cleanup(ctx);

    return;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the index */
/* that correlates to the correct instance number */
unsigned Multistate_Value_Instance_To_Index(
    uint32_t object_instance)
{
    unsigned index = MAX_MULTI_STATE_VALUES;

    if (object_instance < MAX_MULTI_STATE_VALUES)
        index = object_instance;

    return index;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then you need to return the instance */
/* that correlates to the correct index */
uint32_t Multistate_Value_Index_To_Instance(
    unsigned index)
{
    return index;
}

/* we simply have 0-n object instances.  Yours might be */
/* more complex, and then count how many you have */
unsigned Multistate_Value_Count(
    void)
{
    return MAX_MULTI_STATE_VALUES;
}

bool Multistate_Value_Valid_Instance(
    uint32_t object_instance)
{
    unsigned index = 0; /* offset from instance lookup */

    index = Multistate_Value_Instance_To_Index(object_instance);
    if (index < MAX_MULTI_STATE_VALUES) {
        return true;
    }

    return false;
}

uint32_t Multistate_Value_Present_Value(
    uint32_t object_instance)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    uint32_t value = MULTI_STATE_RELINQUISH_DEFAULT;
    unsigned index = 0; /* offset from instance lookup */
    unsigned i = 0;

    index = Multistate_Value_Instance_To_Index(object_instance);
    if (index < MAX_MULTI_STATE_VALUES) {
        CurrentMSV = &MSV_Descr[index];
        /* When all the priorities are level null, the present value returns */
        /* the Relinquish Default value */
        value = CurrentMSV->Relinquish_Default;
        for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
            if (CurrentMSV->Priority_Array[i] != MULTI_STATE_LEVEL_NULL) {
                value = CurrentMSV->Priority_Array[i];
                break;
            }
        }
    }

    return value;
}

bool Multistate_Value_Present_Value_Set(
    uint32_t object_instance,
    uint32_t value,
    uint8_t priority)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    unsigned index = 0; /* offset from instance lookup */
    bool status = false;

    index = Multistate_Value_Instance_To_Index(object_instance);
    if (index < MAX_MULTI_STATE_VALUES) {
        CurrentMSV = &MSV_Descr[index];
        if ((value > 0) && (value <= CurrentMSV->number_of_states)) {
            CurrentMSV->Present_Value = (uint8_t) value;
            CurrentMSV->Priority_Array[priority - 1] = (uint8_t) value;
            status = true;
            //if (priority && (priority <= BACNET_MAX_PRIORITY) &&
                //(priority != 6 /* reserved */ )) {
                    //CurrentMSV->Priority_Array[priority - 1] = (uint8_t) value;
                    /* Note: you could set the physical output here to the next
                    highest priority, or to the relinquish default if no
                    priorities are set.
                    However, if Out of Service is TRUE, then don't set the
                    physical output.  This comment may apply to the
                    main loop (i.e. check out of service before changing output) */
                    //status = true;
             //}
        }
    }

    return status;
}

bool Multistate_Value_Out_Of_Service(
    uint32_t object_instance)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    unsigned index = 0; /* offset from instance lookup */
    bool value = false;

    index = Multistate_Value_Instance_To_Index(object_instance);
    if (index < MAX_MULTI_STATE_VALUES) {
        CurrentMSV = &MSV_Descr[index];
        value = CurrentMSV->Out_Of_Service;
    }

    return value;
}

void Multistate_Value_Out_Of_Service_Set(
    uint32_t object_instance,
    bool value)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    unsigned index = 0;

    index = Multistate_Value_Instance_To_Index(object_instance);
    if (index < MAX_MULTI_STATE_VALUES) {
        CurrentMSV = &MSV_Descr[index];
        //Out_Of_Service[index] = value;
        CurrentMSV->Out_Of_Service = value;
    }

    return;
}

static char *Multistate_Value_Description(
    uint32_t object_instance)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    unsigned index = 0; /* offset from instance lookup */
    char *pName = NULL; /* return value */

    index = Multistate_Value_Instance_To_Index(object_instance);
    if (index < MAX_MULTI_STATE_VALUES) {
        CurrentMSV = &MSV_Descr[index];
        pName = CurrentMSV->Object_Description;
    }

    return pName;
}

bool Multistate_Value_Description_Set(
    uint32_t object_instance,
    char *new_name)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    unsigned index = 0; /* offset from instance lookup */
    size_t i = 0;       /* loop counter */
    bool status = false;        /* return value */

    index = Multistate_Value_Instance_To_Index(object_instance);
    if (index < MAX_MULTI_STATE_VALUES) {
        CurrentMSV = &MSV_Descr[index];
        status = true;
        if (new_name) {
            for (i = 0; i < sizeof(CurrentMSV->Object_Description); i++) {
                CurrentMSV->Object_Description[i] = new_name[i];
                if (new_name[i] == 0) {
                    break;
                }
            }
        } else {
            for (i = 0; i < sizeof(CurrentMSV->Object_Description); i++) {
                CurrentMSV->Object_Description[i] = 0;
            }
        }
    }

    return status;
}

static bool Multistate_Value_Description_Write(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING *char_string,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    unsigned index = 0; /* offset from instance lookup */
    size_t length = 0;
    uint8_t encoding = 0;
    bool status = false;        /* return value */

    index = Multistate_Value_Instance_To_Index(object_instance);
    if (index < MAX_MULTI_STATE_VALUES) {
        CurrentMSV = &MSV_Descr[index];
        length = characterstring_length(char_string);
        if (length <= sizeof(CurrentMSV->Object_Description)) {
            encoding = characterstring_encoding(char_string);
            if (encoding == CHARACTER_UTF8) {
                status = characterstring_ansi_copy(
                    CurrentMSV->Object_Description,
                    sizeof(CurrentMSV->Object_Description),
                    char_string);
                if (!status) {
                    *error_class = ERROR_CLASS_PROPERTY;
                    *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                } else {
                    struct uci_context *ctx;
                    const char index_c[32] = "";
                    sprintf(index_c, "%u", index);
                    ctx = ucix_init("bacnet_mv");
                    if(ctx) {
                        ucix_add_option(ctx, "bacnet_mv", index_c, "description", char_string->value);
                        ucix_commit(ctx, "bacnet_mv");
                        ucix_cleanup(ctx);
                    } else {
                        fprintf(stderr,  "Failed to open config file bacnet_mv\n");
                    }
                }
            } else {
                *error_class = ERROR_CLASS_PROPERTY;
                *error_code = ERROR_CODE_CHARACTER_SET_NOT_SUPPORTED;
            }
        } else {
            *error_class = ERROR_CLASS_PROPERTY;
            *error_code = ERROR_CODE_NO_SPACE_TO_WRITE_PROPERTY;
        }
    }

    return status;
}


bool Multistate_Value_Object_Name(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING * object_name)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    unsigned index = 0; /* offset from instance lookup */
    bool status = false;

    index = Multistate_Value_Instance_To_Index(object_instance);

    if (index < MAX_MULTI_STATE_VALUES) {
        CurrentMSV = &MSV_Descr[index];
        status = characterstring_init_ansi(object_name, CurrentMSV->Object_Name);
    }

    return status;
}

/* note: the object name must be unique within this device */
bool Multistate_Value_Name_Set(
    uint32_t object_instance,
    char *new_name)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    unsigned index = 0; /* offset from instance lookup */
    size_t i = 0;       /* loop counter */
    bool status = false;        /* return value */

    index = Multistate_Value_Instance_To_Index(object_instance);
    if (index < MAX_MULTI_STATE_VALUES) {
        CurrentMSV = &MSV_Descr[index];
        status = true;
        /* FIXME: check to see if there is a matching name */
        if (new_name) {
            for (i = 0; i < sizeof(CurrentMSV->Object_Name); i++) {
                CurrentMSV->Object_Name[i] = new_name[i];
                if (new_name[i] == 0) {
                    break;
                }
            }
        } else {
            for (i = 0; i < sizeof(CurrentMSV->Object_Name); i++) {
                CurrentMSV->Object_Name[i] = 0;
            }
        }
    }

    return status;
}

static bool Multistate_Value_Object_Name_Write(
    uint32_t object_instance,
    BACNET_CHARACTER_STRING *char_string,
    BACNET_ERROR_CLASS *error_class,
    BACNET_ERROR_CODE *error_code)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    unsigned index = 0; /* offset from instance lookup */
    size_t length = 0;
    uint8_t encoding = 0;
    bool status = false;        /* return value */

    index = Multistate_Value_Instance_To_Index(object_instance);
    if (index < MAX_MULTI_STATE_VALUES) {
        CurrentMSV = &MSV_Descr[index];
        length = characterstring_length(char_string);
        if (length <= sizeof(CurrentMSV->Object_Name)) {
            encoding = characterstring_encoding(char_string);
            if (encoding == CHARACTER_UTF8) {
                status = characterstring_ansi_copy(
                    CurrentMSV->Object_Name,
                    sizeof(CurrentMSV->Object_Name),
                    char_string);
                if (!status) {
                    *error_class = ERROR_CLASS_PROPERTY;
                    *error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                } else {
                    struct uci_context *ctx;
                    const char index_c[32] = "";
                    sprintf(index_c, "%u", index);
                    ctx = ucix_init("bacnet_mv");
                    if(ctx) {
                        ucix_add_option(ctx, "bacnet_mv", index_c, "name", char_string->value);
                        ucix_commit(ctx, "bacnet_mv");
                        ucix_cleanup(ctx);
                    } else {
                        fprintf(stderr,  "Failed to open config file bacnet_mv\n");
                    }
                }
            } else {
                *error_class = ERROR_CLASS_PROPERTY;
                *error_code = ERROR_CODE_CHARACTER_SET_NOT_SUPPORTED;
            }
        } else {
            *error_class = ERROR_CLASS_PROPERTY;
            *error_code = ERROR_CODE_NO_SPACE_TO_WRITE_PROPERTY;
        }
    }

    return status;
}


static char *Multistate_Value_State_Text(
    uint32_t object_instance,
    uint32_t state_index)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    unsigned index = 0; /* offset from instance lookup */
    char *pName = NULL; /* return value */

    index = Multistate_Value_Instance_To_Index(object_instance);
    if (index < MAX_MULTI_STATE_VALUES)
        CurrentMSV = &MSV_Descr[index];
    else
        return BACNET_STATUS_ERROR;

    if ((index < MAX_MULTI_STATE_VALUES) && (state_index > 0) &&
        (state_index <= CurrentMSV->number_of_states)) {
            state_index--;
            pName = CurrentMSV->State_Text[state_index];
        }

    return pName;
}

/* note: the object name must be unique within this device */
bool Multistate_Value_State_Text_Set(
    uint32_t object_instance,
    uint32_t state_index,
    char *new_name)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    unsigned index = 0; /* offset from instance lookup */
    size_t i = 0;       /* loop counter */
    bool status = false;        /* return value */

    index = Multistate_Value_Instance_To_Index(object_instance);
    if (index < MAX_MULTI_STATE_VALUES)
        CurrentMSV = &MSV_Descr[index];
    else
        return BACNET_STATUS_ERROR;

    index = Multistate_Value_Instance_To_Index(object_instance);
    if ((index < MAX_MULTI_STATE_VALUES) && (state_index > 0) &&
        (state_index <= CurrentMSV->number_of_states)) {
        state_index--;
        status = true;
        if (new_name) {
            for (i = 0; i < sizeof(CurrentMSV->State_Text[state_index]); i++) {
                CurrentMSV->State_Text[state_index][i] = new_name[i];
                if (new_name[i] == 0) {
                    break;
                }
            }
        } else {
            for (i = 0; i < sizeof(CurrentMSV->State_Text[state_index]); i++) {
                CurrentMSV->State_Text[state_index][i] = 0;
            }
        }
    }

    return status;;
}


/* return apdu len, or BACNET_STATUS_ERROR on error */
int Multistate_Value_Read_Property(
    BACNET_READ_PROPERTY_DATA * rpdata)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    int len = 0;
    int apdu_len = 0;   /* return value */
    BACNET_BIT_STRING bit_string;
    BACNET_CHARACTER_STRING char_string;
    uint32_t present_value = 0;
    unsigned object_index = 0;
    unsigned i = 0;
    bool state = false;
    uint8_t *apdu = NULL;

    if ((rpdata == NULL) || (rpdata->application_data == NULL) ||
        (rpdata->application_data_len == 0)) {
        return 0;
    }
    apdu = rpdata->application_data;

    object_index = Multistate_Value_Instance_To_Index(rpdata->object_instance);
    if (object_index < MAX_MULTI_STATE_VALUES)
        CurrentMSV = &MSV_Descr[object_index];
    else
        return BACNET_STATUS_ERROR;

    switch (rpdata->object_property) {
        case PROP_OBJECT_IDENTIFIER:
            apdu_len =
                encode_application_object_id(&apdu[0],
                OBJECT_MULTI_STATE_VALUE, rpdata->object_instance);
            break;

        case PROP_OBJECT_NAME:
            Multistate_Value_Object_Name(rpdata->object_instance,
                &char_string);
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_DESCRIPTION:
            characterstring_init_ansi(&char_string,
                Multistate_Value_Description(rpdata->object_instance));
            apdu_len =
                encode_application_character_string(&apdu[0], &char_string);
            break;

        case PROP_OBJECT_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0],
                OBJECT_MULTI_STATE_VALUE);
            break;

        case PROP_PRESENT_VALUE:
            present_value =
                Multistate_Value_Present_Value(rpdata->object_instance);
            apdu_len = encode_application_unsigned(&apdu[0], present_value);
            break;

        case PROP_STATUS_FLAGS:
            /* note: see the details in the standard on how to use these */
            bitstring_init(&bit_string);
#if defined(INTRINSIC_REPORTING)
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM,
                CurrentMSV->Event_State ? true : false);
#else
            bitstring_set_bit(&bit_string, STATUS_FLAG_IN_ALARM, false);
#endif
            bitstring_set_bit(&bit_string, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&bit_string, STATUS_FLAG_OVERRIDDEN, false);
            if (Multistate_Value_Out_Of_Service(rpdata->object_instance)) {
                bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE,
                    true);
            } else {
                bitstring_set_bit(&bit_string, STATUS_FLAG_OUT_OF_SERVICE,
                    false);
            }
            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_EVENT_STATE:
            /* note: see the details in the standard on how to use this */
#if defined(INTRINSIC_REPORTING)
            apdu_len =
                encode_application_enumerated(&apdu[0],
                CurrentMSV->Event_State);
#else
            apdu_len =
                encode_application_enumerated(&apdu[0], EVENT_STATE_NORMAL);
#endif
            break;

        case PROP_OUT_OF_SERVICE:
            state = CurrentMSV->Out_Of_Service;
            apdu_len = encode_application_boolean(&apdu[0], state);
            break;

        case PROP_PRIORITY_ARRAY:
            /* Array element zero is the number of elements in the array */
            if (rpdata->array_index == 0) {
                apdu_len =
                    encode_application_unsigned(&apdu[0], BACNET_MAX_PRIORITY);
            /* if no index was specified, then try to encode the entire list */
            /* into one packet. */
            } else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                for (i = 0; i < BACNET_MAX_PRIORITY; i++) {
                    /* FIXME: check if we have room before adding it to APDU */
                    if (CurrentMSV->Priority_Array[i] == MULTI_STATE_LEVEL_NULL) {
                        len = encode_application_null(&apdu[apdu_len]);
                    } else {
                        present_value = CurrentMSV->Priority_Array[i];
                        len =
                            encode_application_unsigned(&apdu[apdu_len],
                            present_value);
                    }
                    /* add it if we have room */
                    if ((apdu_len + len) < MAX_APDU) {
                        apdu_len += len;
                    } else {
                        rpdata->error_class = ERROR_CLASS_SERVICES;
                        rpdata->error_code = ERROR_CODE_NO_SPACE_FOR_OBJECT;
                        apdu_len = BACNET_STATUS_ERROR;
                        break;
                    }
                }
            } else {
                if (rpdata->array_index <= BACNET_MAX_PRIORITY) {
                    if (CurrentMSV->Priority_Array[rpdata->array_index - 1]
                        == MULTI_STATE_LEVEL_NULL)
                        apdu_len = encode_application_null(&apdu[0]);
                    else {
                        present_value =
                            CurrentMSV->Priority_Array[rpdata->array_index - 1];
                        apdu_len =
                            encode_application_unsigned(&apdu[0],present_value);
                    }
                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = BACNET_STATUS_ERROR;
                }
            }
            break;

        case PROP_RELINQUISH_DEFAULT:
            present_value = CurrentMSV->Relinquish_Default;
            apdu_len = encode_application_unsigned(&apdu[0], present_value);
            break;

        case PROP_NUMBER_OF_STATES:
            apdu_len =
                encode_application_unsigned(&apdu[apdu_len],
                CurrentMSV->number_of_states);
            break;

        case PROP_STATE_TEXT:
            if (rpdata->array_index == 0) {
                /* Array element zero is the number of elements in the array */
                apdu_len =
                    encode_application_unsigned(&apdu[0],
                    CurrentMSV->number_of_states);
            } else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                /* if no index was specified, then try to encode the entire list */
                /* into one packet. */
                for (i = 0; i < CurrentMSV->number_of_states; i++) {
                    characterstring_init_ansi(&char_string,
                        CurrentMSV->State_Text[i]);

                    /* FIXME: this might go beyond MAX_APDU length! */
                    len =
                        encode_application_character_string(&apdu[apdu_len],
                        &char_string);
                    /* add it if we have room */
                    if ((apdu_len + len) < MAX_APDU) {
                        apdu_len += len;
                    } else {
                        rpdata->error_class = ERROR_CLASS_SERVICES;
                        rpdata->error_code = ERROR_CODE_NO_SPACE_FOR_OBJECT;
                        apdu_len = BACNET_STATUS_ERROR;
                        break;
                    }
                }
            } else {
                if (rpdata->array_index <= CurrentMSV->number_of_states) {
                    characterstring_init_ansi(&char_string,
                        Multistate_Value_State_Text(rpdata->object_instance,
                            rpdata->array_index));
                    apdu_len =
                        encode_application_character_string(&apdu[0],
                        &char_string);
                } else {
                    rpdata->error_class = ERROR_CLASS_PROPERTY;
                    rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                    apdu_len = BACNET_STATUS_ERROR;
                }
            }
            break;

#if defined(INTRINSIC_REPORTING)
        case PROP_ALARM_VALUES:
            for (i = 0; i < CurrentMSV->number_of_states; i++) {
            	    if (CurrentMSV->Alarm_Values[i]) {
                        len =
                            encode_application_unsigned(&apdu[apdu_len],
                                CurrentMSV->Alarm_Values[i]);
                        apdu_len += len;
            	    }
            }
            break;
        case PROP_TIME_DELAY:
            apdu_len =
                encode_application_unsigned(&apdu[0], CurrentMSV->Time_Delay);
            break;

        case PROP_NOTIFICATION_CLASS:
            apdu_len =
                encode_application_unsigned(&apdu[0],
                CurrentMSV->Notification_Class);
            break;

        case PROP_EVENT_ENABLE:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                (CurrentMSV->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ? true :
                false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                (CurrentMSV->Event_Enable & EVENT_ENABLE_TO_FAULT) ? true :
                false);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                (CurrentMSV->Event_Enable & EVENT_ENABLE_TO_NORMAL) ? true :
                false);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_ACKED_TRANSITIONS:
            bitstring_init(&bit_string);
            bitstring_set_bit(&bit_string, TRANSITION_TO_OFFNORMAL,
                CurrentMSV->
                Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
            bitstring_set_bit(&bit_string, TRANSITION_TO_FAULT,
                CurrentMSV->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked);
            bitstring_set_bit(&bit_string, TRANSITION_TO_NORMAL,
                CurrentMSV->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked);

            apdu_len = encode_application_bitstring(&apdu[0], &bit_string);
            break;

        case PROP_NOTIFY_TYPE:
            apdu_len =
                encode_application_enumerated(&apdu[0],
                CurrentMSV->Notify_Type ? NOTIFY_EVENT : NOTIFY_ALARM);
            break;

        case PROP_EVENT_TIME_STAMPS:
            /* Array element zero is the number of elements in the array */
            if (rpdata->array_index == 0)
                apdu_len =
                    encode_application_unsigned(&apdu[0],
                    MAX_BACNET_EVENT_TRANSITION);
            /* if no index was specified, then try to encode the entire list */
            /* into one packet. */
            else if (rpdata->array_index == BACNET_ARRAY_ALL) {
                for (i = 0; i < MAX_BACNET_EVENT_TRANSITION; i++) {;
                    len =
                        encode_opening_tag(&apdu[apdu_len],
                        TIME_STAMP_DATETIME);
                    len +=
                        encode_application_date(&apdu[apdu_len + len],
                        &CurrentMSV->Event_Time_Stamps[i].date);
                    len +=
                        encode_application_time(&apdu[apdu_len + len],
                        &CurrentMSV->Event_Time_Stamps[i].time);
                    len +=
                        encode_closing_tag(&apdu[apdu_len + len],
                        TIME_STAMP_DATETIME);

                    /* add it if we have room */
                    if ((apdu_len + len) < MAX_APDU)
                        apdu_len += len;
                    else {
                        rpdata->error_class = ERROR_CLASS_SERVICES;
                        rpdata->error_code = ERROR_CODE_NO_SPACE_FOR_OBJECT;
                        apdu_len = BACNET_STATUS_ERROR;
                        break;
                    }
                }
            } else if (rpdata->array_index <= MAX_BACNET_EVENT_TRANSITION) {
                apdu_len =
                    encode_opening_tag(&apdu[apdu_len], TIME_STAMP_DATETIME);
                apdu_len +=
                    encode_application_date(&apdu[apdu_len],
                    &CurrentMSV->Event_Time_Stamps[rpdata->array_index].date);
                apdu_len +=
                    encode_application_time(&apdu[apdu_len],
                    &CurrentMSV->Event_Time_Stamps[rpdata->array_index].time);
                apdu_len +=
                    encode_closing_tag(&apdu[apdu_len], TIME_STAMP_DATETIME);
            } else {
                rpdata->error_class = ERROR_CLASS_PROPERTY;
                rpdata->error_code = ERROR_CODE_INVALID_ARRAY_INDEX;
                apdu_len = BACNET_STATUS_ERROR;
            }
            break;
#endif



      default:
            rpdata->error_class = ERROR_CLASS_PROPERTY;
            rpdata->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            apdu_len = BACNET_STATUS_ERROR;
            break;
    }
    /*  only array properties can have array options */
    if ((apdu_len >= 0) && (rpdata->object_property != PROP_PRIORITY_ARRAY) &&
        (rpdata->object_property != PROP_EVENT_TIME_STAMPS) &&
        (rpdata->array_index != BACNET_ARRAY_ALL)) {
        rpdata->error_class = ERROR_CLASS_PROPERTY;
        rpdata->error_code = ERROR_CODE_PROPERTY_IS_NOT_AN_ARRAY;
        apdu_len = BACNET_STATUS_ERROR;
    }

    return apdu_len;
}

/* returns true if successful */
bool Multistate_Value_Write_Property(
    BACNET_WRITE_PROPERTY_DATA * wp_data)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    bool status = false;        /* return value */
    unsigned int object_index = 0;
    int object_type = 0;
    uint32_t object_instance = 0;
    unsigned int priority = 0;
    uint8_t level = MULTI_STATE_LEVEL_NULL;
    int len = 0;
    BACNET_APPLICATION_DATA_VALUE value;

    /* decode the some of the request */
    len =
        bacapp_decode_application_data(wp_data->application_data,
        wp_data->application_data_len, &value);
    /* FIXME: len < application_data_len: more data? */
    if (len < 0) {
        /* error while decoding - a value larger than we can handle */
        wp_data->error_class = ERROR_CLASS_PROPERTY;
        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
        return false;
    }

    object_index = Multistate_Value_Instance_To_Index(wp_data->object_instance);
    if (object_index < MAX_MULTI_STATE_VALUES)
        CurrentMSV = &MSV_Descr[object_index];
    else
        return false;

    switch (wp_data->object_property) {
        case PROP_OBJECT_NAME:
            if (value.tag == BACNET_APPLICATION_TAG_CHARACTER_STRING) {
                /* All the object names in a device must be unique */
                if (Device_Valid_Object_Name(&value.type.Character_String,
                    &object_type, &object_instance)) {
                    if ((object_type == wp_data->object_type) &&
                        (object_instance == wp_data->object_instance)) {
                        /* writing same name to same object */
                        status = true;
                    } else {
                        status = false;
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_DUPLICATE_NAME;
                    }
                } else {
                    status = Multistate_Value_Object_Name_Write(
                        wp_data->object_instance,
                        &value.type.Character_String,
                        &wp_data->error_class,
                        &wp_data->error_code);
                }
            } else {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
            }
            break;
        case PROP_DESCRIPTION:
            if (value.tag == BACNET_APPLICATION_TAG_CHARACTER_STRING) {
                status = Multistate_Value_Description_Write(
                    wp_data->object_instance,
                    &value.type.Character_String,
                    &wp_data->error_class,
                    &wp_data->error_code);
            } else {
                wp_data->error_class = ERROR_CLASS_PROPERTY;
                wp_data->error_code = ERROR_CODE_INVALID_DATA_TYPE;
            }
            break;
        case PROP_PRESENT_VALUE:
            if (value.tag == BACNET_APPLICATION_TAG_UNSIGNED_INT) {
                /* Command priority 6 is reserved for use by Minimum On/Off
                   algorithm and may not be used for other purposes in any
                   object. */
                if (Multistate_Value_Present_Value_Set(wp_data->object_instance,
                        value.type.Unsigned_Int, wp_data->priority)) {
                    status = true;
                } else if (wp_data->priority == 6) {
                    /* Command priority 6 is reserved for use by Minimum On/Off
                       algorithm and may not be used for other purposes in any
                       object. */
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                }
            } else {
                status =
                    WPValidateArgType(&value, BACNET_APPLICATION_TAG_NULL,
                    &wp_data->error_class, &wp_data->error_code);
                if (status) {
                    level = MULTI_STATE_LEVEL_NULL;
                    priority = wp_data->priority;
                    if (priority && (priority <= BACNET_MAX_PRIORITY)) {
                        priority--;
                        CurrentMSV->Priority_Array[priority] = level;
                        /* Note: you could set the physical output here to the next
                           highest priority, or to the relinquish default if no
                           priorities are set.
                           However, if Out of Service is TRUE, then don't set the
                           physical output.  This comment may apply to the
                           main loop (i.e. check out of service before changing output) */
                    } else {
                        status = false;
                        wp_data->error_class = ERROR_CLASS_PROPERTY;
                        wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                    }
                }
            }
            break;

        case PROP_OUT_OF_SERVICE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_BOOLEAN,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                Multistate_Value_Out_Of_Service_Set(wp_data->object_instance,
                    value.type.Boolean);
            }
            break;

        case PROP_RELINQUISH_DEFAULT:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                &wp_data->error_class, &wp_data->error_code);
            if (status) {
                CurrentMSV->Relinquish_Default = value.type.Unsigned_Int;
            }
            break;

#if defined(INTRINSIC_REPORTING)
        case PROP_TIME_DELAY:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                CurrentMSV->Time_Delay = value.type.Unsigned_Int;
                CurrentMSV->Remaining_Time_Delay = CurrentMSV->Time_Delay;
            }
            break;

        case PROP_NOTIFICATION_CLASS:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_UNSIGNED_INT,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                CurrentMSV->Notification_Class = value.type.Unsigned_Int;
            }
            break;

        case PROP_EVENT_ENABLE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_BIT_STRING,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                if (value.type.Bit_String.bits_used == 3) {
                    CurrentMSV->Event_Enable = value.type.Bit_String.value[0];
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                    status = false;
                }
            }
            break;

        case PROP_NOTIFY_TYPE:
            status =
                WPValidateArgType(&value, BACNET_APPLICATION_TAG_ENUMERATED,
                &wp_data->error_class, &wp_data->error_code);

            if (status) {
                if (value.type.Bit_String.bits_used > NOTIFY_EVENT) {
                    CurrentMSV->Event_Enable = value.type.Enumerated;
                } else {
                    wp_data->error_class = ERROR_CLASS_PROPERTY;
                    wp_data->error_code = ERROR_CODE_VALUE_OUT_OF_RANGE;
                    status = false;
                }
            }
            break;
#endif
        case PROP_OBJECT_IDENTIFIER:
        case PROP_OBJECT_TYPE:
        case PROP_STATUS_FLAGS:
        case PROP_EVENT_STATE:
        case PROP_NUMBER_OF_STATES:
        case PROP_PRIORITY_ARRAY:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        case PROP_STATE_TEXT:
#if defined(INTRINSIC_REPORTING)
        case PROP_ACKED_TRANSITIONS:
        case PROP_EVENT_TIME_STAMPS:
#endif
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_WRITE_ACCESS_DENIED;
            break;
        default:
            wp_data->error_class = ERROR_CLASS_PROPERTY;
            wp_data->error_code = ERROR_CODE_UNKNOWN_PROPERTY;
            break;
    }

    return status;
}

void Multistate_Value_Intrinsic_Reporting(
    uint32_t object_instance)
{
#if defined(INTRINSIC_REPORTING)
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    BACNET_EVENT_NOTIFICATION_DATA event_data;
    BACNET_CHARACTER_STRING msgText;
    unsigned int object_index;
    uint8_t FromState = 0;
    uint8_t ToState;
    uint8_t PresentVal = 0;
    bool SendNotify = false;
    bool tonormal = true;
    int i;


    object_index = Multistate_Value_Instance_To_Index(object_instance);
    if (object_index < MAX_MULTI_STATE_VALUES)
        CurrentMSV = &MSV_Descr[object_index];
    else
        return;

    if (CurrentMSV->Ack_notify_data.bSendAckNotify) {
        /* clean bSendAckNotify flag */
        CurrentMSV->Ack_notify_data.bSendAckNotify = false;
        /* copy toState */
        ToState = CurrentMSV->Ack_notify_data.EventState;

#if PRINT_ENABLED
        fprintf(stderr, "Send Acknotification for (%i,%d).\n",
            bactext_object_type_name(OBJECT_MULTI_STATE_VALUE), object_instance);
#endif /* PRINT_ENABLED */

        characterstring_init_ansi(&msgText, "AckNotification");

        /* Notify Type */
        event_data.notifyType = NOTIFY_ACK_NOTIFICATION;

        /* Send EventNotification. */
        SendNotify = true;
    } else {
        /* actual Present_Value */
        PresentVal = Multistate_Value_Present_Value(object_instance);
        FromState = CurrentMSV->Event_State;
        switch (CurrentMSV->Event_State) {
            case EVENT_STATE_NORMAL:
                /* A TO-OFFNORMAL event is generated under these conditions:
                   (a) the Present_Value must exceed the High_Limit for a minimum
                   period of time, specified in the Time_Delay property, and
                   (b) the HighLimitEnable flag must be set in the Limit_Enable property, and
                   (c) the TO-OFFNORMAL flag must be set in the Event_Enable property. */
                for ( i = 0; i < CurrentMSV->number_of_states; i++) {
            	    if (CurrentMSV->Alarm_Values[i]) {
                        if ((PresentVal == CurrentMSV->Alarm_Values[i]) &&
                            ((CurrentMSV->Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ==
                            EVENT_ENABLE_TO_OFFNORMAL)) {
                                if (!CurrentMSV->Remaining_Time_Delay)
                                    CurrentMSV->Event_State = EVENT_STATE_FAULT;
                                else
                                    CurrentMSV->Remaining_Time_Delay--;
                                break;
            	        }
                    }
                }
                /* value of the object is still in the same event state */
                CurrentMSV->Remaining_Time_Delay = CurrentMSV->Time_Delay;
                break;

            case EVENT_STATE_FAULT:
                for ( i = 0; i < CurrentMSV->number_of_states; i++) {
            	    if (CurrentMSV->Alarm_Values[i]) {
                        if (PresentVal == CurrentMSV->Alarm_Values[i]) {
                               tonormal = false;
            	        }
                    }
                }
                if ((tonormal) &&
                    ((CurrentMSV->Event_Enable & EVENT_ENABLE_TO_NORMAL) ==
                    EVENT_ENABLE_TO_NORMAL)) {
                    if (!CurrentMSV->Remaining_Time_Delay)
                        CurrentMSV->Event_State = EVENT_STATE_NORMAL;
                    else
                        CurrentMSV->Remaining_Time_Delay--;
                    break;
                }
                /* value of the object is still in the same event state */
                CurrentMSV->Remaining_Time_Delay = CurrentMSV->Time_Delay;
                break;

            default:
                return; /* shouldn't happen */
        }       /* switch (FromState) */

        ToState = CurrentMSV->Event_State;

        if (FromState != ToState) {
            /* Event_State has changed.
               Need to fill only the basic parameters of this type of event.
               Other parameters will be filled in common function. */

            switch (ToState) {
                case EVENT_STATE_FAULT:
                    //ExceededLimit = CurrentMSV->High_Limit;
                    characterstring_init_ansi(&msgText, "Goes to EVENT_STATE_FAULT");
                    break;

                case EVENT_STATE_NORMAL:
                    if (FromState == EVENT_STATE_FAULT) {
                        //ExceededLimit = CurrentMSV->High_Limit;
                        characterstring_init_ansi(&msgText,
                            "Back to normal state from EVENT_STATE_FAULT");
                    }
                    break;

                default:
                    //ExceededLimit = 0;
                    break;
            }   /* switch (ToState) */

#if PRINT_ENABLED
            fprintf(stderr, "Event_State for (%i,%d) goes from %i to %i.\n",
                bactext_object_type_name(OBJECT_MULTI_STATE_VALUE), object_instance,
                bactext_event_state_name(FromState),
                bactext_event_state_name(ToState));
#endif /* PRINT_ENABLED */

            /* Notify Type */
            event_data.notifyType = CurrentMSV->Notify_Type;

            /* Send EventNotification. */
            SendNotify = true;
        }
    }


    if (SendNotify) {
        /* Event Object Identifier */
        event_data.eventObjectIdentifier.type = OBJECT_MULTI_STATE_VALUE;
        event_data.eventObjectIdentifier.instance = object_instance;

        /* Time Stamp */
        event_data.timeStamp.tag = TIME_STAMP_DATETIME;
        Device_getCurrentDateTime(&event_data.timeStamp.value.dateTime);

        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            /* fill Event_Time_Stamps */
            switch (ToState) {
                case EVENT_STATE_FAULT:
                    CurrentMSV->Event_Time_Stamps[TRANSITION_TO_FAULT] =
                        event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_NORMAL:
                    CurrentMSV->Event_Time_Stamps[TRANSITION_TO_NORMAL] =
                        event_data.timeStamp.value.dateTime;
                    break;
            }
        }

        /* Notification Class */
        event_data.notificationClass = CurrentMSV->Notification_Class;

        /* Event Type */
        event_data.eventType = EVENT_OUT_OF_RANGE;

        /* Message Text */
        event_data.messageText = &msgText;

        /* Notify Type */
        /* filled before */

        /* From State */
        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION)
            event_data.fromState = FromState;

        /* To State */
        event_data.toState = CurrentMSV->Event_State;

        /* Event Values */
        if (event_data.notifyType != NOTIFY_ACK_NOTIFICATION) {
            /* Value that exceeded a limit. */
            event_data.notificationParams.outOfRange.exceedingValue =
                PresentVal;
            /* Status_Flags of the referenced object. */
            bitstring_init(&event_data.notificationParams.
                outOfRange.statusFlags);
            bitstring_set_bit(&event_data.notificationParams.
                outOfRange.statusFlags, STATUS_FLAG_IN_ALARM,
                CurrentMSV->Event_State ? true : false);
            bitstring_set_bit(&event_data.notificationParams.
                outOfRange.statusFlags, STATUS_FLAG_FAULT, false);
            bitstring_set_bit(&event_data.notificationParams.
                outOfRange.statusFlags, STATUS_FLAG_OVERRIDDEN, false);
            bitstring_set_bit(&event_data.notificationParams.
                outOfRange.statusFlags, STATUS_FLAG_OUT_OF_SERVICE,
                CurrentMSV->Out_Of_Service);
        }

        /* add data from notification class */
        Notification_Class_common_reporting_function(&event_data);

        /* Ack required */
        if ((event_data.notifyType != NOTIFY_ACK_NOTIFICATION) &&
            (event_data.ackRequired == true)) {
            switch (event_data.toState) {
                case EVENT_STATE_HIGH_LIMIT:
                case EVENT_STATE_LOW_LIMIT:
                case EVENT_STATE_OFFNORMAL:
                    CurrentMSV->
                        Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked =
                        false;
                    CurrentMSV->
                        Acked_Transitions[TRANSITION_TO_OFFNORMAL].Time_Stamp =
                        event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_FAULT:
                    CurrentMSV->
                        Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked =
                        false;
                    CurrentMSV->
                        Acked_Transitions[TRANSITION_TO_FAULT].Time_Stamp =
                        event_data.timeStamp.value.dateTime;
                    break;

                case EVENT_STATE_NORMAL:
                    CurrentMSV->
                        Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked =
                        false;
                    CurrentMSV->
                        Acked_Transitions[TRANSITION_TO_NORMAL].Time_Stamp =
                        event_data.timeStamp.value.dateTime;
                    break;
            }
        }
    }
#endif /* defined(INTRINSIC_REPORTING) */
}


#if defined(INTRINSIC_REPORTING)
int Multistate_Value_Event_Information(
    unsigned index,
    BACNET_GET_EVENT_INFORMATION_DATA * getevent_data)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    bool IsNotAckedTransitions;
    bool IsActiveEvent;
    int i;


    /* check index */
    if (index < MAX_MULTI_STATE_VALUES) {
        CurrentMSV = &MSV_Descr[index];
        /* Event_State not equal to NORMAL */
        IsActiveEvent = (MSV_Descr[index].Event_State != EVENT_STATE_NORMAL);

        /* Acked_Transitions property, which has at least one of the bits
           (TO-OFFNORMAL, TO-FAULT, TONORMAL) set to FALSE. */
        IsNotAckedTransitions =
            (MSV_Descr[index].
            Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked ==
            false) | (MSV_Descr[index].
            Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked ==
            false) | (MSV_Descr[index].
            Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked == false);
    } else
        return -1;      /* end of list  */

    if ((IsActiveEvent) || (IsNotAckedTransitions)) {
        /* Object Identifier */
        getevent_data->objectIdentifier.type = OBJECT_MULTI_STATE_VALUE;
        getevent_data->objectIdentifier.instance =
            Multistate_Value_Index_To_Instance(index);
        /* Event State */
        getevent_data->eventState = MSV_Descr[index].Event_State;
        /* Acknowledged Transitions */
        bitstring_init(&getevent_data->acknowledgedTransitions);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_OFFNORMAL,
            MSV_Descr[index].
            Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_FAULT,
            MSV_Descr[index].Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked);
        bitstring_set_bit(&getevent_data->acknowledgedTransitions,
            TRANSITION_TO_NORMAL,
            MSV_Descr[index].Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked);
        /* Event Time Stamps */
        for (i = 0; i < 3; i++) {
            getevent_data->eventTimeStamps[i].tag = TIME_STAMP_DATETIME;
            getevent_data->eventTimeStamps[i].value.dateTime =
                MSV_Descr[index].Event_Time_Stamps[i];
        }
        /* Notify Type */
        getevent_data->notifyType = MSV_Descr[index].Notify_Type;
        /* Event Enable */
        bitstring_init(&getevent_data->eventEnable);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_OFFNORMAL,
            (MSV_Descr[index].Event_Enable & EVENT_ENABLE_TO_OFFNORMAL) ? true :
            false);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_FAULT,
            (MSV_Descr[index].Event_Enable & EVENT_ENABLE_TO_FAULT) ? true :
            false);
        bitstring_set_bit(&getevent_data->eventEnable, TRANSITION_TO_NORMAL,
            (MSV_Descr[index].Event_Enable & EVENT_ENABLE_TO_NORMAL) ? true :
            false);
        /* Event Priorities */
        Notification_Class_Get_Priorities(MSV_Descr[index].Notification_Class,
            getevent_data->eventPriorities);

        return 1;       /* active event */
    } else
        return 0;       /* no active event at this index */
}

int Multistate_Value_Alarm_Ack(
    BACNET_ALARM_ACK_DATA * alarmack_data,
    BACNET_ERROR_CODE * error_code)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    unsigned int object_index;


    object_index =
        Multistate_Value_Instance_To_Index(alarmack_data->
        eventObjectIdentifier.instance);

    if (object_index < MAX_MULTI_STATE_VALUES)
        CurrentMSV = &MSV_Descr[object_index];
    else {
        *error_code = ERROR_CODE_UNKNOWN_OBJECT;
        return -1;
    }

    switch (alarmack_data->eventStateAcked) {
        case EVENT_STATE_OFFNORMAL:
            if (CurrentMSV->
                Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked == false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&CurrentMSV->Acked_Transitions
                        [TRANSITION_TO_OFFNORMAL].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                CurrentMSV->
                    Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked = true;
            } else {
                *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                return -1;
            }
            break;

        case EVENT_STATE_FAULT:
            if (CurrentMSV->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked ==
                false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&CurrentMSV->Acked_Transitions
                        [TRANSITION_TO_NORMAL].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                CurrentMSV->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked =
                    true;
            } else {
                *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                return -1;
            }
            break;

        case EVENT_STATE_NORMAL:
            if (CurrentMSV->Acked_Transitions[TRANSITION_TO_FAULT].bIsAcked ==
                false) {
                if (alarmack_data->eventTimeStamp.tag != TIME_STAMP_DATETIME) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }
                if (datetime_compare(&CurrentMSV->Acked_Transitions
                        [TRANSITION_TO_FAULT].Time_Stamp,
                        &alarmack_data->eventTimeStamp.value.dateTime) > 0) {
                    *error_code = ERROR_CODE_INVALID_TIME_STAMP;
                    return -1;
                }

                /* Clean transitions flag. */
                CurrentMSV->Acked_Transitions[TRANSITION_TO_NORMAL].bIsAcked =
                    true;
            } else {
                *error_code = ERROR_CODE_INVALID_EVENT_STATE;
                return -1;
            }
            break;

        default:
            return -2;
    }

    /* Need to send AckNotification. */
    CurrentMSV->Ack_notify_data.bSendAckNotify = true;
    CurrentMSV->Ack_notify_data.EventState = alarmack_data->eventStateAcked;

    /* Return OK */
    return 1;
}

int Multistate_Value_Alarm_Summary(
    unsigned index,
    BACNET_GET_ALARM_SUMMARY_DATA * getalarm_data)
{
    MULTI_STATE_VALUE_DESCR *CurrentMSV;
    /* check index */
    if (index < MAX_MULTI_STATE_VALUES) {
        CurrentMSV = &MSV_Descr[index];
        /* Event_State is not equal to NORMAL  and
           Notify_Type property value is ALARM */
        if ((CurrentMSV->Event_State != EVENT_STATE_NORMAL) &&
            (CurrentMSV->Notify_Type == NOTIFY_ALARM)) {
            /* Object Identifier */
            getalarm_data->objectIdentifier.type = OBJECT_MULTI_STATE_VALUE;
            getalarm_data->objectIdentifier.instance =
                Multistate_Value_Index_To_Instance(index);
            /* Alarm State */
            getalarm_data->alarmState = CurrentMSV->Event_State;
            /* Acknowledged Transitions */
            bitstring_init(&getalarm_data->acknowledgedTransitions);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_OFFNORMAL,
                CurrentMSV->
                Acked_Transitions[TRANSITION_TO_OFFNORMAL].bIsAcked);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_FAULT,
                CurrentMSV->Acked_Transitions[TRANSITION_TO_FAULT].
                bIsAcked);
            bitstring_set_bit(&getalarm_data->acknowledgedTransitions,
                TRANSITION_TO_NORMAL,
                CurrentMSV->Acked_Transitions[TRANSITION_TO_NORMAL].
                bIsAcked);

            return 1;   /* active alarm */
        } else
            return 0;   /* no active alarm at this index */
    } else
        return -1;      /* end of list  */
}
#endif /* defined(INTRINSIC_REPORTING) */



#ifdef TEST
#include <assert.h>
#include <string.h>
#include "ctest.h"

bool WPValidateArgType(
    BACNET_APPLICATION_DATA_VALUE * pValue,
    uint8_t ucExpectedTag,
    BACNET_ERROR_CLASS * pErrorClass,
    BACNET_ERROR_CODE * pErrorCode)
{
    pValue = pValue;
    ucExpectedTag = ucExpectedTag;
    pErrorClass = pErrorClass;
    pErrorCode = pErrorCode;

    return false;
}

void testMultistateInput(
    Test * pTest)
{
    BACNET_READ_PROPERTY_DATA rpdata;
    uint8_t apdu[MAX_APDU] = { 0 };
    int len = 0;
    uint32_t len_value = 0;
    uint8_t tag_number = 0;
    uint16_t decoded_type = 0;
    uint32_t decoded_instance = 0;

    Multistate_Value_Init();
    rpdata.application_data = &apdu[0];
    rpdata.application_data_len = sizeof(apdu);
    rpdata.object_type = OBJECT_MULTI_STATE_VALUE;
    rpdata.object_instance = 1;
    rpdata.object_property = PROP_OBJECT_IDENTIFIER;
    rpdata.array_index = BACNET_ARRAY_ALL;
    len = Multistate_Value_Read_Property(&rpdata);
    ct_test(pTest, len != 0);
    len = decode_tag_number_and_value(&apdu[0], &tag_number, &len_value);
    ct_test(pTest, tag_number == BACNET_APPLICATION_TAG_OBJECT_ID);
    len = decode_object_id(&apdu[len], &decoded_type, &decoded_instance);
    ct_test(pTest, decoded_type == rpdata.object_type);
    ct_test(pTest, decoded_instance == rpdata.object_instance);

    return;
}

#ifdef TEST_MULTI_STATE_VALUE
int main(
    void)
{
    Test *pTest;
    bool rc;

    pTest = ct_create("BACnet Multi-state Input", NULL);
    /* individual tests */
    rc = ct_addTestFunction(pTest, testMultistateInput);
    assert(rc);

    ct_setStream(pTest, stdout);
    ct_run(pTest);
    (void) ct_report(pTest);
    ct_destroy(pTest);

    return 0;
}
#endif
#endif /* TEST */