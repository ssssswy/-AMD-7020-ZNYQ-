#include "touch_input.h"
#include "xparameters.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static uint8_t registers[65536];
static unsigned selected, fail_read, fail_ack, acks, clicks, downs, ups;
static XTime now;
void XTime_GetTime(XTime *t) { *t = now; }
void usleep(unsigned n) { (void)n; }
XIicPs_Config *XIicPs_LookupConfig(unsigned id) { static XIicPs_Config c;
#ifdef SDT
 assert(id==XPAR_XIICPS_0_BASEADDR);
#else
 assert(id==XPAR_XIICPS_0_DEVICE_ID);
#endif
 return &c; }
XGpioPs_Config *XGpioPs_LookupConfig(unsigned id) { static XGpioPs_Config c;
#ifdef SDT
 assert(id==XPAR_XGPIOPS_0_BASEADDR);
#else
 assert(id==XPAR_XGPIOPS_0_DEVICE_ID);
#endif
 return &c; }
int XIicPs_CfgInitialize(XIicPs *i,XIicPs_Config *c,uintptr_t a) { (void)i;(void)c;(void)a;return 0; }
int XGpioPs_CfgInitialize(XGpioPs *i,XGpioPs_Config *c,uintptr_t a) { (void)i;(void)c;(void)a;return 0; }
void XIicPs_SetSClk(XIicPs *i,unsigned n) { (void)i;(void)n; }
int XIicPs_BusIsBusy(XIicPs *i) { (void)i;return 0; }
void XIicPs_SetOptions(XIicPs *i,unsigned n) { (void)i;(void)n; }
void XIicPs_ClearOptions(XIicPs *i,unsigned n) { (void)i;(void)n; }
void XGpioPs_SetDirectionPin(XGpioPs *g,unsigned p,unsigned v) { (void)g;(void)p;(void)v; }
void XGpioPs_SetOutputEnablePin(XGpioPs *g,unsigned p,unsigned v) { (void)g;(void)p;(void)v; }
void XGpioPs_WritePin(XGpioPs *g,unsigned p,unsigned v) { (void)g;(void)p;(void)v; }
int XIicPs_MasterSendPolled(XIicPs *i,uint8_t *d,unsigned n,unsigned addr)
{
    unsigned bytes = addr == 0x38U ? 1U : 2U;
    (void)i;
    selected = bytes == 2U ? ((unsigned)d[0]<<8)|d[1] : d[0];
    if (n > bytes) {
        if (selected == 0x814eU) {
            if (fail_ack) return -1;
            ++acks;
        }
        memcpy(&registers[selected], &d[bytes], n-bytes);
    }
    return 0;
}
int XIicPs_MasterRecvPolled(XIicPs *i,uint8_t *d,unsigned n,unsigned addr)
{
    (void)i;(void)addr;
    if (selected == fail_read) return -1;
    memcpy(d, &registers[selected], n);
    return 0;
}
static void setup(tracegpu_touch_context *c)
{
    memset(c,0,sizeof(*c));
    memset(registers,0,sizeof(registers));
    c->ps.initialized = 1U;
    c->ps.frame.online = 1U;
    c->ps.frame.controller = TRACEGPU_TOUCH_CONTROLLER_GT;
    c->ps.frame.address = 0x14U;
    c->accepted_frame = c->ps.frame;
    c->button_armed = 1U;
    now = clicks = downs = ups = acks = fail_ack = 0U;
    fail_read = 0xffffU;
}
static void report(unsigned count,unsigned x)
{
    unsigned i;
    registers[0x814e] = (uint8_t)(0x80U|count);
    for (i=0;i<count && i<5U;++i) {
        unsigned p=0x8150U+8U*i;
        registers[p]=(uint8_t)(x+30U*i);
        registers[p+1]=(uint8_t)((x+30U*i)>>8);
        registers[p+2]=100U;registers[p+3]=0U;
    }
}
static tracegpu_touch_event_type step(tracegpu_touch_context *c,unsigned dt)
{
    tracegpu_touch_sample s;
    tracegpu_touch_event_type e;
    now+=dt;
    e=tracegpu_touch_poll(c,&s);
    downs+=e==TRACEGPU_TOUCH_DOWN; ups+=e==TRACEGPU_TOUCH_UP;
    clicks+=(unsigned)tracegpu_touch_button_press(c,e,&s);
    assert(s.valid==(tracegpu_touch_frame_get(c)->count!=0U));
    return e;
}
int main(void)
{
    tracegpu_touch_context c;
    unsigned i, sequence, old_acks;
    setup(&c);
    report(1U,650U); assert(step(&c,5U)==TRACEGPU_TOUCH_DOWN);
    sequence=c.ps.frame.sequence;old_acks=acks;
    assert(step(&c,5U)==TRACEGPU_TOUCH_NONE);
    assert(c.ps.frame.count==1U && c.ps.frame.point[0].active);
    assert(c.ps.frame.sequence==sequence && acks==old_acks && !c.ps.frame.i2c_errors);
    /* 1000 alternating ready/not-ready reports during one held contact. */
    for(i=0;i<1000U;++i) {
        report(1U,650U);step(&c,5U);step(&c,5U);
    }
    assert(clicks==1U && downs==1U && ups==0U);
    report(0U,0U);step(&c,5U);step(&c,40U);
    report(1U,651U);assert(step(&c,5U)==TRACEGPU_TOUCH_MOVE);
    assert(clicks==1U && !ups);
    /* Read, invalid count and ACK failure must not replace last coordinates. */
    sequence=c.ps.frame.sequence;
    fail_read=0x8158U;report(2U,700U);step(&c,5U);
    assert(c.ps.frame.count==1U && c.ps.frame.point[0].x==651U);
    fail_read=0xffffU;report(6U,700U);step(&c,5U);
    fail_ack=1U;report(1U,750U);step(&c,5U);fail_ack=0U;
    assert(c.ps.frame.sequence==sequence && c.ps.frame.i2c_errors==3U && !ups);
    report(5U,200U);assert(step(&c,5U)==TRACEGPU_TOUCH_MOVE);
    step(&c,5U);assert(tracegpu_touch_frame_get(&c)->count==5U && clicks==1U);
    report(2U,200U);step(&c,5U);step(&c,5U);
    assert(tracegpu_touch_frame_get(&c)->count==2U);
    /* A single fresh zero frame followed by no data must still release. */
    report(0U,0U);step(&c,5U);step(&c,59U);
    assert(!ups);assert(step(&c,1U)==TRACEGPU_TOUCH_UP);
    step(&c,100U);assert(ups==1U);
    report(1U,650U);assert(step(&c,5U)==TRACEGPU_TOUCH_DOWN);
    assert(clicks==2U);
    /* Multitouch DOWN consumes the contact without clicking settings. */
    setup(&c);report(2U,650U);step(&c,5U);
    report(1U,650U);step(&c,5U);assert(clicks==0U);
    report(0U,0U);step(&c,5U);step(&c,60U);
    report(1U,650U);step(&c,5U);assert(clicks==1U);
    /* FT lift and reserved records cannot remain active; mapping unchanged. */
    setup(&c);c.ps.frame.controller=TRACEGPU_TOUCH_CONTROLLER_FT;c.ps.frame.address=0x38U;
    registers[2]=3U;
    registers[3]=0x40U;registers[9]=0xc0U;
    registers[15]=0x80U;registers[16]=100U;registers[17]=0U;registers[18]=200U;
    assert(step(&c,5U)==TRACEGPU_TOUCH_DOWN);
    assert(c.ps.frame.count==1U && c.previous.x==200U && c.previous.y==100U);
    puts("PASS touch_input: 1000 ready/no-data cycles -> one click; release debounce; single-zero release; I2C/ACK rollback; five points; multi-touch button exclusion; FT events");
    return 0;
}
