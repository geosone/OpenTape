/* Synchronous native tape controls inside the established QUILIB screen.
 * Editing/canceling never opens a device. Each confirmed operation goes through
 * the same checked SDK path as the CLI, without retries or implicit probes.
 * A reported position is a snapshot, not a bookmark or recovery operation. */
#include <stdio.h>
#include <string.h>
#include <commfunc.h>
#include <input.h>
#include <libcolor.h>
#include <screen.h>
#include <ui.h>
#include <video.h>
#include <windowfn.h>
#include "opentape/native_tape.h"
#include "opentape/native_ui.h"
#include "opentape/dos_mouse.h"
#include "opentape/messages.h"

#define UI_LOCATE 256u
#define UI_ALERTS 257u
enum tape_phase { T_IDLE,T_ALERTS,T_LAYOUT,T_POSITION,T_SPACE_KIND,T_SPACE_COUNT,T_BLOCK,T_PARTITION,T_CONFIRM,T_RESULT,T_BUSY,T_NOTICE };
struct tape_ui {
    const char *alias,*notice;
    enum tape_phase phase;
    unsigned action,error,completion,session,reply_known,partition_flag,input_overflow;
    unsigned diagnostic;
    unsigned char host,target,key,response,asc,ascq;
    int result;
    char block_text[21],partition_text[4],count_text[9];
    oa_s32 count;
    oa_u32 block,high,partition;
#ifdef OT_NATIVE_TAPE_WIDE
    struct ot_tape_position64_v1 position;
    struct ot_tape_locate64_v1 locate;
#endif
#ifdef OT_NATIVE_TAPE_PARTITIONS
    struct ot_tape_partitions_v1 layout;
#endif
#ifdef OT_NATIVE_TAPE_ALERTS
    struct ot_tape_alerts_v1 alerts;
#endif
    struct ot_tape_control_v1 control;
};
/* All native replies share the checked diagnostic prefix, but remain distinct
 * packet types. Copy fields explicitly rather than aliasing packed structures. */
#define KEEP_REPLY(s,p) do { \
    (s)->completion=(p).completion_flags; (s)->session=(p).session_state; \
    (s)->diagnostic=(p).diagnostic_class; (s)->host=(p).host_status; \
    (s)->target=(p).target_status; (s)->key=(p).sense_key; \
    (s)->response=(p).sense_response; (s)->asc=(p).asc; (s)->ascq=(p).ascq; \
} while(0)
static const char *action_name(unsigned action)
{
    if(action==OT_TAPE_CONTROL_SPACE_RECORDS) return OT_MSG_NUI_SPACE_RECORDS;
    if(action==OT_TAPE_CONTROL_SPACE_FILEMARKS) return OT_MSG_NUI_SPACE_MARKS;
    if(action==OT_TAPE_CONTROL_SPACE_EOD) return OT_MSG_NUI_SPACE_EOD;
#ifdef OT_NATIVE_TAPE_EXTENDED_SPACE
    if(action==OT_TAPE_CONTROL_SPACE_SEQ_FILEMARKS) return OT_MSG_NUI_SPACE_SEQ_MARKS;
    if(action==OT_TAPE_CONTROL_SPACE_SETMARKS) return OT_MSG_NUI_SPACE_SETMARKS;
    if(action==OT_TAPE_CONTROL_SPACE_SEQ_SETMARKS) return OT_MSG_NUI_SPACE_SEQ_SETS;
#endif
    if(action==UI_ALERTS) return OT_MSG_NUI_ALERTS;
    if(action==UI_LOCATE) return OT_MSG_NUI_LOCATE;
    if(action==OT_TAPE_CONTROL_LOAD) return OT_MSG_NUI_LOAD;
    if(action==OT_TAPE_CONTROL_UNLOAD) return OT_MSG_NUI_UNLOAD;
#ifdef OT_NATIVE_TAPE_REMOVAL
    if(action==OT_TAPE_CONTROL_LOCK) return OT_MSG_NUI_LOCK;
    if(action==OT_TAPE_CONTROL_UNLOCK) return OT_MSG_NUI_UNLOCK;
#endif
    return OT_MSG_NUI_REWIND;
}
#ifdef OT_NATIVE_TAPE_ALERTS
/* Numeric alert codes stay complete even with all 64 flags active. Four rows
 * of at most 47 characters fit the established data window without clipping. */
static void draw_alerts(const struct tape_ui *s,ot_native_ui_line line)
{
    char text[128],number[5];
    unsigned i,reported=0,active=0,row=11,length=0,n;
    for(i=1;i<=64;++i) {
        int state=ot_tape_alert_state(&s->alerts,(oa_u16)i);
        if(state>=0) ++reported;
        if(state==1) ++active;
    }
    sprintf(text,OT_MSG_NUI_ALERTS_COUNTS,reported,active); line(0,9,text,DataColor);
    if(!reported) line(0,11,OT_MSG_NATIVE_ALERTS_UNKNOWN,DataYellowColor);
    else if(!active) line(0,11,OT_MSG_NATIVE_ALERTS_NONE,DataColor);
    else {
        line(0,10,OT_MSG_NUI_ALERTS_NUMBERS,DataColor); text[0]=0;
        for(i=1;i<=64;++i) if(ot_tape_alert_state(&s->alerts,(oa_u16)i)==1) {
            sprintf(number,"%u",i); n=(unsigned)strlen(number);
            if(length && length+1+n>47u) { line(0,row++,text,DataYellowColor); length=0; text[0]=0; }
            if(length) text[length++]=' ';
            memcpy(text+length,number,n+1); length+=n;
        }
        if(length) line(0,row,text,DataYellowColor);
    }
    line(0,18,OT_MSG_NATIVE_ALERTS_ACK,DataColor);
}
#endif
/* Every line is bounded by the application's existing window renderer. Full
 * 20-digit values get their own row so narrow panels never truncate them. */
static void draw(const struct tape_ui *s,ot_native_ui_line line)
{
    char text[128];
    unsigned i;
#ifdef OT_NATIVE_TAPE_WIDE
    char number[21];
#endif
    paintBackgroundScreen();
    ot_dos_mouse_clear();
    line(0,1,OT_MSG_APP_TITLE,DataYellowColor);
    line(0,2,OT_MSG_NUI_TITLE,DataColor);
    sprintf(text,OT_MSG_UI_SELECTED,s->alias); line(0,4,text,DataYellowColor);
    line(0,5,OT_MSG_NUI_SCOPE,DataColor);
    if(s->phase==T_SPACE_KIND) {
        line(0,7,OT_MSG_NUI_SPACE_CHOOSE,DataYellowColor);
        line(0,9,OT_MSG_NUI_SPACE_ONE,DataColor);
        line(0,10,OT_MSG_NUI_SPACE_TWO,DataColor);
        line(0,11,OT_MSG_NUI_SPACE_THREE,DataColor);
#ifdef OT_NATIVE_TAPE_EXTENDED_SPACE
        line(0,12,OT_MSG_NUI_SPACE_FOUR,DataColor);
        line(0,13,OT_MSG_NUI_SPACE_FIVE,DataColor);
        line(0,14,OT_MSG_NUI_SPACE_SIX,DataColor);
        line(0,17,OT_MSG_NUI_SPACE_KEYS_EXT,DataYellowColor);
#else
        line(0,17,OT_MSG_NUI_SPACE_KEYS,DataYellowColor);
#endif
    } else if(s->phase==T_BLOCK || s->phase==T_PARTITION || s->phase==T_SPACE_COUNT || s->phase==T_CONFIRM) {
        line(0,7,action_name(s->action),DataYellowColor);
        if(s->action==UI_LOCATE) {
            line(0,8,OT_MSG_NUI_BLOCK,DataColor);
            line(0,9,s->block_text,DataYellowColor);
            if(s->phase!=T_BLOCK) {
                line(0,11,OT_MSG_NUI_PARTITION,DataColor);
                line(0,12,*s->partition_text ? s->partition_text : OT_MSG_NUI_CURRENT,DataYellowColor);
            }
        }
        if(ot_native_space_operation((oa_u16)s->action) && s->action!=OT_TAPE_CONTROL_SPACE_EOD) {
            line(0,8,OT_MSG_NUI_SPACE_RANGE,DataColor);
            line(0,9,s->count_text,DataYellowColor);
            if(s->phase==T_CONFIRM) line(0,12,s->count<0 ? OT_MSG_NUI_SPACE_BACKWARD :
                s->count>0 ? OT_MSG_NUI_SPACE_FORWARD : OT_MSG_NUI_SPACE_ZERO,DataColor);
        }
        if(s->action==UI_ALERTS) line(0,13,OT_MSG_NATIVE_ALERTS_ACK,DataYellowColor);
        if(s->phase==T_CONFIRM) {
            line(0,17,OT_MSG_NUI_CONFIRM,DataYellowColor);
            line(0,18,OT_MSG_UI_REWIND_KEYS,DataYellowColor);
        } else line(0,17,s->phase==T_SPACE_COUNT ? OT_MSG_NUI_SPACE_EDIT :
            s->phase==T_BLOCK ? OT_MSG_NUI_EDIT_BLOCK : OT_MSG_NUI_EDIT_PART,DataYellowColor);
        if(s->notice) line(0,s->phase==T_CONFIRM ? 19 : 16,s->notice,DataYellowColor);
    } else if(s->phase==T_POSITION || s->phase==T_RESULT || s->phase==T_LAYOUT || s->phase==T_ALERTS) {
        sprintf(text,OT_MSG_NUI_RESULT,s->result,s->error); line(0,7,text,DataYellowColor);
#ifdef OT_NATIVE_TAPE_WIDE
        if(s->phase==T_POSITION && s->reply_known) {
            if(s->position.valid_fields & 1UL) {
                sprintf(text,OT_MSG_NUI_PART_VALUE,(unsigned long)s->position.partition); line(0,9,text,DataColor);
                line(0,10,OT_MSG_NUI_BLOCK,DataColor);
                ot_tape_u64_format(s->position.block,s->position.block_high,number,sizeof(number));
                line(0,11,number,DataYellowColor);
            } else line(0,9,OT_MSG_NUI_UNKNOWN,DataYellowColor);
            line(0,12,OT_MSG_NUI_FILE,DataColor);
            if(s->position.valid_fields & 2UL) {
                ot_tape_u64_format(s->position.file,s->position.file_high,number,sizeof(number));
                line(0,13,number,DataColor);
            } else line(0,13,OT_MSG_NATIVE_UNKNOWN,DataColor);
            if(s->position.probe_flags & 1UL) line(0,18,OT_MSG_NUI_UNVERIFIED,DataYellowColor);
        }
#endif
#ifdef OT_NATIVE_TAPE_PARTITIONS
        if(s->phase==T_LAYOUT && s->reply_known) {
            if(s->layout.valid_fields & OT_TAPE_LAYOUT_VALID) {
                sprintf(text,OT_MSG_NUI_LAYOUT_COUNTS,(unsigned long)s->layout.maximum_additional,(unsigned long)s->layout.defined_additional);
                line(0,9,text,DataColor);
                sprintf(text,OT_MSG_NUI_LAYOUT_RAW,(unsigned long)s->layout.partition_flags,(unsigned long)s->layout.format_recognition,(unsigned long)s->layout.partition_units);
                line(0,11,text,DataColor);
                sprintf(text,OT_MSG_NUI_LAYOUT_SIZES,(unsigned long)s->layout.size_descriptors);
                line(0,13,text,DataColor);
                line(0,18,OT_MSG_NATIVE_LAYOUT_PENDING,DataYellowColor);
            }
            if(s->layout.probe_flags & 1UL) line(0,18,OT_MSG_NUI_UNVERIFIED,DataYellowColor);
        }
#endif
#ifdef OT_NATIVE_TAPE_ALERTS
        if(s->phase==T_ALERTS && s->reply_known) {
            if(s->alerts.valid_fields & OT_TAPE_ALERTS_VALID) draw_alerts(s,line);
            if(s->alerts.probe_flags & 1UL) line(0,18,OT_MSG_NUI_UNVERIFIED,DataYellowColor);
        }
#endif
        if(s->phase==T_RESULT) {
            line(0,9,action_name(s->action),DataColor);
            if(s->reply_known && (s->completion & OT_TAPE_COMPLETION_DONE))
                line(0,11,s->result==OT_NATIVE_CLOSE ? OT_MSG_NUI_CLOSE_FAILED : OT_MSG_NUI_DONE,DataYellowColor);
            if(ot_native_space_operation((oa_u16)s->action) && s->action!=OT_TAPE_CONTROL_SPACE_EOD) {
                sprintf(text,OT_MSG_NUI_SPACE_COUNT,(long)s->count); line(0,12,text,DataColor);
            }
            if(s->completion & OT_TAPE_POSITION_UNKNOWN) line(0,13,OT_MSG_UI_NATIVE_POSITION,DataYellowColor);
        }
        if(s->reply_known) {
            sprintf(text,OT_MSG_NUI_SESSION,s->session); line(0,15,text,DataColor);
            if(s->session & OT_TAPE_SESSION_NATIVE_FAILED) line(0,16,OT_MSG_NUI_FAILED,DataYellowColor);
            if(s->diagnostic) {
                sprintf(text,OT_MSG_NUI_DIAG,s->diagnostic,(unsigned)s->host,(unsigned)s->target,(unsigned)s->key);
                line(0,17,text,DataYellowColor);
                if(s->response) {
                    sprintf(text,OT_MSG_NUI_ASC,(unsigned)s->response,(unsigned)s->asc,(unsigned)s->ascq);
                    line(0,18,text,DataColor);
                }
            }
        }
    } else if(s->phase==T_BUSY) line(0,9,OT_MSG_NUI_BUSY,DataYellowColor);
    else if(s->notice) line(0,9,s->notice,DataYellowColor);
    else line(0,9,OT_MSG_NUI_IDLE,DataColor);
    line(1,1,OT_MSG_HELP_TITLE,QuickBackColor);
    ot_dos_ui_action(line,1u,3,OT_MSG_NUI_HELP_POSITION,QuickBackColor,'p');
    ot_dos_ui_action(line,1u,4,OT_MSG_NUI_HELP_LOCATE,QuickBackColor,'g');
    ot_dos_ui_action(line,1u,5,OT_MSG_NUI_HELP_LAYOUT,QuickBackColor,'i');
    ot_dos_ui_action(line,1u,6,OT_MSG_NUI_HELP_LOAD,QuickBackColor,'l');
    ot_dos_ui_action(line,1u,7,OT_MSG_NUI_HELP_UNLOAD,QuickBackColor,'u');
    ot_dos_ui_action(line,1u,8,OT_MSG_NUI_HELP_SPACE,QuickBackColor,'s');
    ot_dos_ui_action(line,1u,9,OT_MSG_NUI_HELP_LOCK,QuickBackColor,'k');
    ot_dos_ui_action(line,1u,10,OT_MSG_NUI_HELP_UNLOCK,QuickBackColor,'o');
    ot_dos_ui_action(line,1u,11,OT_MSG_NUI_ALERTS_HELP,QuickBackColor,'a');
    ot_dos_ui_action(line,1u,12,OT_MSG_UI_HELP_REWIND,QuickBackColor,'r');
    ot_dos_ui_action(line,1u,14,OT_MSG_NUI_BACK,QuickBackColor,KEY_ESCAPE);
    if(s->phase==T_CONFIRM || s->phase==T_BLOCK || s->phase==T_PARTITION ||
       s->phase==T_SPACE_COUNT || s->phase==T_SPACE_KIND || s->phase==T_BUSY) {
        ot_dos_mouse_clear();
        if(s->phase==T_CONFIRM) ot_dos_mouse_pair(0u,18,OT_MSG_UI_REWIND_KEYS,OT_UI_CONFIRM_KEY,KEY_ESCAPE);
        else if(s->phase==T_BLOCK || s->phase==T_PARTITION || s->phase==T_SPACE_COUNT) {
            line(0,19,OT_MSG_MOUSE_EDIT,DataColor);
            ot_dos_mouse_pair(0u,19,OT_MSG_MOUSE_EDIT,KEY_ENTER,KEY_ESCAPE);
        } else if(s->phase==T_SPACE_KIND) {
            for(i=0u;i<3u;++i) ot_dos_mouse_bind(0u,9+(int)i,0,44u,'1'+i);
#ifdef OT_NATIVE_TAPE_EXTENDED_SPACE
            for(i=3u;i<6u;++i) ot_dos_mouse_bind(0u,9+(int)i,0,44u,'1'+i);
#endif
            ot_dos_ui_action(line,0u,19,OT_MSG_MOUSE_BACK,DataColor,KEY_ESCAPE);
        }
    }
    line(1,16,OT_MSG_UI_HELP_READY,QuickBackColor);
}
static void execute(struct tape_ui *s,ot_native_ui_line line)
{
    s->phase=T_BUSY; s->reply_known=0; s->completion=s->session=s->error=0;
    draw(s,line);
#ifdef OT_NATIVE_TAPE_ALERTS
    if(s->action==UI_ALERTS) {
        s->result=ot_dos_native_alerts(s->alias,OT_TAPE_ALERTS_ACKNOWLEDGE,&s->alerts,&s->error);
        s->reply_known=ot_tape_alerts_validate(&s->alerts,sizeof(s->alerts))==0;
        if(s->reply_known) KEEP_REPLY(s,s->alerts);
        s->phase=T_ALERTS; return;
    }
#endif
#ifdef OT_NATIVE_TAPE_WIDE
    if(s->action==UI_LOCATE) {
        s->result=ot_dos_native_locate64(s->alias,(oa_u16)s->partition_flag,s->partition,s->block,s->high,&s->locate,&s->error);
        s->reply_known=ot_tape_locate64_validate(&s->locate,sizeof(s->locate))==0;
        if(s->reply_known) KEEP_REPLY(s,s->locate);
    } else
#endif
    {
        if(ot_native_space_operation((oa_u16)s->action))
            s->result=ot_dos_native_space(s->alias,(oa_u16)s->action,s->count,&s->control,&s->error);
        else s->result=ot_dos_native_execute(s->alias,(oa_u16)s->action,&s->control,&s->error);
        s->reply_known=ot_tape_control_validate(&s->control,sizeof(s->control))==0;
        if(s->reply_known) KEEP_REPLY(s,s->control);
    }
    s->phase=T_RESULT;
}
/* Accept only decimal digits and backspace while editing. Confirm consumes
 * exactly one key; cancel keys are never reinterpreted as another command.
 * A rejected printable character or excess digit latches an input error until
 * backspace/cancel, so a shortened valid-looking target cannot be submitted. */
void ot_dos_native_controls_ui(const char *alias,ot_native_ui_line line)
{
    struct tape_ui s;
    INPUTEVENT event;
    unsigned key,length,i,value;
    memset(&s,0,sizeof(s)); s.alias=alias; draw(&s,line);
    for(;;) {
        if(!ot_dos_mouse_input(&event)) continue;
        key=event.key;
        if(s.phase==T_CONFIRM) {
            if(key==OT_UI_CONFIRM_KEY || key==OT_UI_CONFIRM_KEY-'a'+'A') execute(&s,line);
            else { s.phase=T_IDLE; s.notice=OT_MSG_NUI_CANCELLED; }
            draw(&s,line); continue;
        }
        if(s.phase==T_SPACE_KIND) {
            if(key==KEY_ESCAPE) { s.phase=T_IDLE; s.notice=OT_MSG_NUI_CANCELLED; }
            else if(key>='1' && key<='6') {
                if(key<='3') s.action=OT_TAPE_CONTROL_SPACE_RECORDS+(key-'1');
                else {
#ifdef OT_NATIVE_TAPE_EXTENDED_SPACE
                    s.action=OT_TAPE_CONTROL_SPACE_SEQ_FILEMARKS+(key-'4');
#else
                    s.phase=T_NOTICE; s.notice=OT_MSG_NUI_MISSING; draw(&s,line); continue;
#endif
                }
                s.count=0; s.count_text[0]=0; s.notice=0; s.input_overflow=0;
                s.phase=key=='3' ? T_CONFIRM : T_SPACE_COUNT;
            }
            draw(&s,line); continue;
        }
        if(s.phase==T_BLOCK || s.phase==T_PARTITION || s.phase==T_SPACE_COUNT) {
            char *edit=s.phase==T_SPACE_COUNT ? s.count_text :
                s.phase==T_BLOCK ? s.block_text : s.partition_text;
            length=(unsigned)strlen(edit);
            s.notice=s.input_overflow ? OT_MSG_NUI_INVALID : 0;
            if(key==KEY_ESCAPE) { s.phase=T_IDLE; s.notice=OT_MSG_NUI_CANCELLED; }
            else if(key==KEY_BKSP) { if(length) edit[length-1]=0; s.input_overflow=0; s.notice=0; }
            else if(key=='-' && s.phase==T_SPACE_COUNT && length==0) { edit[0]='-'; edit[1]=0; }
            else if(key>='0' && key<='9') {
                if(length<(s.phase==T_SPACE_COUNT ? 8u : s.phase==T_BLOCK ? 20u : 3u)) { edit[length]=(char)key; edit[length+1]=0; }
                else { s.input_overflow=1; s.notice=OT_MSG_NUI_INVALID; }
            } else if(key==KEY_ENTER && !s.input_overflow) {
                if(s.phase==T_SPACE_COUNT) {
                    if(ot_native_space_count_parse(edit,&s.count)!=0) s.notice=OT_MSG_NUI_INVALID;
                    else s.phase=T_CONFIRM;
                } else
#ifdef OT_NATIVE_TAPE_WIDE
                if(s.phase==T_BLOCK) {
                    if(ot_tape_u64_parse(edit,&s.block,&s.high)!=0) s.notice=OT_MSG_NUI_INVALID;
                    else s.phase=T_PARTITION;
                } else
#endif
                {
                    value=0;
                    for(i=0;i<length;++i) value=value*10u+(unsigned)(edit[i]-'0');
                    if(value>255u) s.notice=OT_MSG_NUI_INVALID;
                    else { s.partition=value; s.partition_flag=length!=0; s.phase=T_CONFIRM; }
                }
            } else if(key>=32u && key<=126u) {
                s.input_overflow=1; s.notice=OT_MSG_NUI_INVALID;
            }
            draw(&s,line); continue;
        }
        if(key==KEY_ESCAPE) return;
        s.notice=0;
        if(key=='a' || key=='A') {
#ifdef OT_NATIVE_TAPE_ALERTS
            s.action=UI_ALERTS; s.phase=T_CONFIRM;
#else
            s.phase=T_NOTICE; s.notice=OT_MSG_NUI_MISSING;
#endif
        } else if(key=='s' || key=='S') {
            s.phase=T_SPACE_KIND;
        } else if(key=='i' || key=='I') {
#ifdef OT_NATIVE_TAPE_PARTITIONS
            s.phase=T_BUSY; s.reply_known=0; draw(&s,line);
            s.result=ot_dos_native_partitions(alias,&s.layout,&s.error);
            s.reply_known=ot_tape_partitions_validate(&s.layout,sizeof(s.layout))==0;
            if(s.reply_known) KEEP_REPLY(&s,s.layout);
            s.phase=T_LAYOUT;
#else
            s.phase=T_NOTICE; s.notice=OT_MSG_NUI_MISSING;
#endif
        } else if(key=='p' || key=='P') {
#ifdef OT_NATIVE_TAPE_WIDE
            s.phase=T_BUSY; s.reply_known=0; draw(&s,line);
            s.result=ot_dos_native_position64(alias,&s.position,&s.error);
            s.reply_known=ot_tape_position64_validate(&s.position,sizeof(s.position))==0;
            if(s.reply_known) KEEP_REPLY(&s,s.position);
            s.phase=T_POSITION;
#else
            s.phase=T_NOTICE; s.notice=OT_MSG_NUI_MISSING;
#endif
        } else if(key=='g' || key=='G') {
#ifdef OT_NATIVE_TAPE_WIDE
            s.action=UI_LOCATE; s.phase=T_BLOCK; s.input_overflow=0; s.block_text[0]=s.partition_text[0]=0;
#else
            s.phase=T_NOTICE; s.notice=OT_MSG_NUI_MISSING;
#endif
        } else {
            if(key=='r' || key=='R') s.action=OT_TAPE_CONTROL_REWIND;
            else if(key=='l' || key=='L') s.action=OT_TAPE_CONTROL_LOAD;
            else if(key=='u' || key=='U') s.action=OT_TAPE_CONTROL_UNLOAD;
#ifdef OT_NATIVE_TAPE_REMOVAL
            else if(key=='k' || key=='K') s.action=OT_TAPE_CONTROL_LOCK;
            else if(key=='o' || key=='O') s.action=OT_TAPE_CONTROL_UNLOCK;
#else
            else if(key=='k' || key=='K' || key=='o' || key=='O') {
                s.phase=T_NOTICE; s.notice=OT_MSG_NUI_MISSING; draw(&s,line); continue;
            }
#endif
            else continue;
            s.block_text[0]=s.partition_text[0]=0; s.phase=T_CONFIRM;
        }
        draw(&s,line);
    }
}
