/***********************************************************************
 * SPACE TRADER 1.2.0 - Field.c
 * Genesis port: field operations rerouted to ui_field_buf / label table.
 ***********************************************************************/
#include "external.h"
#include "ui.h"

/* On Genesis there is no real widget system.
 * SetField stores the initial value in a static buffer.
 * GetField reads back from ui_field_buf (which GEN_FrmDoDialog populates
 * after the user enters a number via the spinner). */

static char _field_storage[128];

void* SetField( FormPtr frm, int Nr, char* Value, int Size, Boolean Focus )
{
    (void)frm; (void)Nr; (void)Size; (void)Focus;
    strncpy(_field_storage, Value, sizeof(_field_storage) - 1);
    _field_storage[sizeof(_field_storage)-1] = '\0';
    /* Copy into ui_field_buf so GEN_FldGetTextPtr returns it */
    strncpy(ui_field_buf, Value, sizeof(ui_field_buf) - 1);
    ui_field_buf[sizeof(ui_field_buf)-1] = '\0';
    return (void*)_field_storage;
}

void GetField( FormPtr frm, int Nr, char* Value, void* AmountH )
{
    (void)frm; (void)Nr; (void)AmountH;
    /* The spinner in GEN_FrmDoDialog wrote its result into ui_field_buf */
    strncpy(Value, ui_field_buf, 127);
    Value[127] = '\0';
    /* Reset the buffer */
    ui_field_buf[0] = '\0';
}

void SetTriggerList( FormPtr frm, int Nr, int Index )
{
    (void)frm;
    GEN_LstSetSelection(Nr, Index);
}

void SetControlLabel( FormPtr frm, int Nr, char* Label )
{
    (void)frm; (void)Nr;
    GEN_CtlSetLabel((void*)((long)Nr), Label);
}

int GetTriggerList( FormPtr frm, int Nr )
{
    (void)frm;
    return GEN_LstGetSelection(Nr);
}

void SetCheckBox( FormPtr frm, int Nr, Boolean Value )
{
    (void)frm;
    GEN_CtlSetValue(Nr, Value ? 1 : 0);
}

Boolean GetCheckBox( FormPtr frm, int Nr )
{
    (void)frm;
    return GEN_CtlGetValue(Nr) != 0;
}
