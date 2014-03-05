/*
 * STEM Du Type II
 *
 * Gakken GMC-4/FM microcomputer clone
 *
 *
 *
 *
 * Based on the Arduino based project/sketch by nshdot
 *   url: http://home.nshdot.com/gmc-no
 *   last update: 2010/06/20 
 */

#include "pitches.h"
#include <EEPROM.h>


// virtual machine

byte mem[256];
byte pc = 0;
boolean flag = true;
byte *data = mem+0x50;   // data memory pointer
boolean locked = false;  // data is read-only?

#define regA  mem[0x6F]  // register A
#define regB  mem[0x6C]  // register B
#define regY  mem[0x6E]  // register Y
#define regZ  mem[0x6D]  // register Z
#define regAA mem[0x69]  // register A'
#define regBB mem[0x67]  // register B'
#define regYY mem[0x68]  // register Y'
#define regZZ mem[0x66]  // register Z'
#define regS  mem[0x63]  // stack pointer
#define regM  mem[0x64]  // data segment selector
#define regK  mem[0x65]  // sound key

void mem_segment(byte m)
{
    regM = m;
    data = mem + (m<<4);
    locked = (m == 6);
}

void cpu_reset()
{
    pc = 0;
    flag = true;
}

void sys_reset()
{
    for(int i = 0; i < 256; i++) {
        mem[i] = 0xF;
    }
    if (!noext) {
        regM = 5;
    }
    data = mem + 0x50;
    locked = false;
    cpu_reset();
}

byte fetch()
{
    if (noext) {
        if (pc >= 0x60) pc = 0;
    }else{
         if ((pc&0xF0) == 0x60) pc = 0;
    }
    byte a = (mem[pc++]) & 0xF;
    if (pc == 0x60) pc = 0;
    return a;
}

void mem_push(byte a)
{
    byte sp = regS;
    mem[0xF0+sp] = a & 0xF;
    sp--;
    sp &= 0xF;
    regS = sp;
}

byte mem_pop()
{
    byte sp = regS;
    sp++;
    sp &= 0xF;
    regS = sp;
    return mem[0xF0+sp] & 0xF;
}

void sound(int n, int t=0)
{
    static const int freq[] = {
        NOTE_A3,NOTE_AS3,
        NOTE_B3,
        NOTE_C4,NOTE_CS4,
        NOTE_D4,NOTE_DS4,
        NOTE_E4,
        NOTE_F4,NOTE_FS4,
        NOTE_G4,NOTE_GS4,
        NOTE_A4,NOTE_AS4,
        NOTE_B4,
        NOTE_C5,NOTE_CS5,
        NOTE_D5,NOTE_DS5,
        NOTE_E5,
        NOTE_F5,NOTE_FS5,
        NOTE_G5,NOTE_GS5,
        NOTE_A5,NOTE_AS5,
        NOTE_B5,
        NOTE_C6,NOTE_CS6,
        NOTE_D6,NOTE_DS6,
        NOTE_E6,
        NOTE_F6,NOTE_FS6,
        NOTE_G6,NOTE_GS6,
        NOTE_A6,NOTE_AS6,
        NOTE_B6
    };
    static int scale[14] = {
        0,2,3,5,7,8,10,12,14,15,17,19,20,22
    };
    if (n >= 1 && n <= 14) {
        n = scale[n-1];
        if ((!noext) && regK > 1 && regK <= 14) {
            n += regK-1;
        }
        n = constrain(n,0,38);
        soundf(freq[n],t);
    }else{
        soundf(0,t);
    }
}

// storage

#define SAVE_LOAD_WAIT 3    // slow down on purpose

void mem_send()
{
    led_binary(0x7F);
    Serial.begin(9600);
    int i = 0;
    for(; i < 256; i++) {
        if (keypad_read() >= KEY_RESET) break;
        led_7seg(mem[i]);
        Serial.print((mem[i]&0xF),HEX);
        led_binary((int)map(i,0,255,6,0),false);
#ifdef SAVE_LOAD_WAIT
        dev_wait(SAVE_LOAD_WAIT);
#endif
    }
    if (keypad_buf == KEY_CLEAR) return;
    Serial.write("\r\n");
    led_binary(0);
    if (keypad_buf != KEY_RESET) {
        if (i == 256) cmd_ends();
        else          cmd_errs();
    }
    led_7seg(0,true);
}

void mem_receive()
{
    led_binary(0);
    Serial.begin(9600);
    int c;
    int e = '\0';
    int buf1 = -1;
    int buf2 = -1;
    int i = 0;
    int t = 0;
    while(i < 256) {
        if (keypad_read() >= KEY_RESET) break;
        c = Serial.read();
        if (c < 0) {
            if (t >= 20 && i > 0)  break;
            if (t >= 100)  break;
            led_7seg((1<<(t%6)),true);
            t++;
            dev_wait(100);
        }else{
            t = 0;
            if (e != '\0') {
                if (c == e)  e = '\0';
                c = -1;
            }else{
                if (c >= '0' && c <= '9') {
                    c = c - '0';
                }else if (c >= 'A' && c <= 'F') {
                    c = 0xA + c - 'A';
                }else if (c >= 'a' && c <= 'f') {
                    c = 0xA + c - 'a';
                }else if (c == ':') {
                    if (buf2 >= 0 && buf1 >= 0) {
                        i = (buf2 << 4) | buf1;
                        buf2 = buf1 = -1;
                    }
                    c = -1;
                }else if (c == ';') {
                    e = '\n';
                    c = -1;
                }else if (c == '<') {
                    e = '>';
                    c = -1;
                }else{
                    c = -1;
                }
            }
            if (c >= 0) {
                led_7seg((byte)c);
            }else{
                led_7seg(B1000000,true);
            }
            if (buf2 >= 0) {
                mem[i++] = (byte)(buf2 & 0xF);
                led_binary((int)map(i,0,255,6,0),true);
            }
            buf2 = buf1;
            buf1 = c;
        }
    }
    if (keypad_buf == KEY_CLEAR) return;
    if (buf2 >= 0 && i < 256) {
        mem[i++] = (byte)(buf2 & 0xF);
    }
    if (buf1 >= 0 && i < 256) {
        mem[i++] = (byte)(buf1 & 0xF);
    }
    if (i > 0 && i < 256) {
        led_binary((int)map(i,0,255,6,0),true);
    }
    led_7seg(0,true);
    if (keypad_buf != KEY_RESET) {
        if (i != 0) cmd_ends();
        else        cmd_errs();
    }
    led_binary(0);
}

void mem_save(int slot)
{
    int a = slot<<7;
    int d;
    int i = 0;
    led_binary(0x7F);
    while(i < 256) {
        if (keypad_read() >= KEY_RESET) break;
        d  = (mem[i++]&0xF);
        led_7seg(d);
        d |= (mem[i++]&0xF)<<4;
        EEPROM.write(a++,d);
        led_binary((int)map(i,0,255,6,0),false);
#ifdef SAVE_LOAD_WAIT
        dev_wait(SAVE_LOAD_WAIT*2);
#endif
    }
    if (keypad_buf == KEY_CLEAR) return;
    led_7seg(0,true);
    if (keypad_buf != KEY_RESET) {
        if (i == 256) cmd_ends();
        else          cmd_errs();
    }
    led_binary(0);
}

void mem_load(int slot)
{
    int a = slot<<7;
    int d;
    int i = 0;
    led_binary(0);
    while(i < 256) {
        if (keypad_read() >= KEY_RESET) break;
        d = EEPROM.read(a++);
        mem[i++] =  d    &0xF;
        mem[i++] = (d>>4)&0xF;
        led_7seg(d&0xF);
        led_binary((int)map(i,0,255,6,0),true);
#ifdef SAVE_LOAD_WAIT
        dev_wait(SAVE_LOAD_WAIT*2);
#endif
    }
    if (keypad_buf == KEY_CLEAR) return;
    led_7seg(0,true);
    if (keypad_buf != KEY_RESET) {
        if (i == 256) cmd_ends();
        else          cmd_errs();
    }
    led_binary(0);
}

// cpu

void cmd_ka()
{
    int k = keypad_read();
    if (k >= 16) k = -1;
    if (k >= 0) {
        regA = (byte)k;
        flag = false;
    }else{
        flag = true;
    }
}

void cmd_ao()
{
    led_7seg(regA);
    flag = true;
}

void cmd_ch()
{
    byte t = regA;
    regA   = regB;
    regB   = t;
    t      = regY;
    regY   = regZ;
    regZ   = t;
    flag = true;
}

void cmd_cy()
{
    byte t = regA;
    regA   = regY;
    regY   = t;
    flag = true;
}

void cmd_am()
{
    if (!locked) {
        data[regY] = regA;
    }
    flag = true;
}

void cmd_ma()
{
    regA = data[regY];
    flag = true;
}

void cmd_mp()
{
    byte a = regA + data[regY];
    regA = a&0x0F;
    flag = (a >= 0x10);
}

void cmd_mm()
{
    byte a = regA - data[regY];
    regA = a&0x0F;
    flag = (a >= 0x10);
}

void cmd_tia()
{
    regA = fetch();
    flag = true;
}

void cmd_aia()
{
    byte a = regA + fetch();
    regA = a&0x0F;
    flag = (a >= 0x10);
}

void cmd_tiy()
{
    regY = fetch();
    flag = true;
}

void cmd_aiy()
{
    byte a = regY + fetch();
    regY = a&0x0F;
    flag = (a >= 0x10);
}

void cmd_cia()
{
    byte a = fetch();
    flag = (regA != a);
}

void cmd_ciy()
{
    byte a = fetch();
    flag = (regY != a);
}

void cmd_rsto()
{
    led_7seg(0,true);
}

void cmd_setr()
{
    led_binary(regY,true);
    flag = true;
}

void cmd_rstr()
{
    led_binary(regY,false);
    flag = true;
}

void cmd_inpt()
{
    regA = input_read(false);
    flag = true;
}

void cmd_cmpl()
{
    regA ^= 0x0F;
    flag = true;
}

void cmd_chng()
{
    byte t = regA;
    regA   = regAA;
    regAA  = t;
    t      = regY;
    regY   = regYY;
    regYY  = t;
    t      = regB;
    regB   = regBB;
    regBB  = t;
    t      = regZ;
    regZ   = regZZ;
    regZZ  = t;
    flag = true;
}

void cmd_sift()
{
    byte a = regA;
    regA = (a>>1) & 0xF;
    flag = ((a&1) == 0);
}

void cmd_ends()
{
    soundf(NOTE_D6,-80);
    soundf(NOTE_E6,-80);
    soundf(NOTE_F6,-80);
    soundf(NOTE_G6,-80);
    soundf(NOTE_A6,-80);
    soundf(NOTE_B6,-80);
    soundf(0);
    flag = true;
}

void cmd_errs()
{
    for(int i = 0; i < 6; i++) {
        soundf(NOTE_G5,-20);
        soundf(NOTE_A5,-20);
        soundf(NOTE_B5,-20);
        soundf(NOTE_C6,-20);
        soundf(NOTE_D6,-20);
        soundf(NOTE_E6,-20);
    }
    soundf(0);
    flag = true;
}

void cmd_shts()
{
    soundf(NOTE_C5,150);
    soundf(0,150);
    flag = true;
}

void cmd_lons()
{
    soundf(NOTE_C5,450);
    soundf(0,150);
    flag = true;
}

void cmd_sund()
{
    sound(regA,300);
    sound(0,15);
    flag = true;
}

void cmd_timr()
{
    dev_wait((int)(regA+1)*100);
    flag = true;
}

// FFFF EEEE
// 7654 3210
void cmd_dspr()
{
    byte a = data[0xE] | (data[0xF] << 4);
    led_binary(a);
    flag = true;
}

void cmd_demm()
{
    if (!locked) {
        byte a = regA;
        byte y = regY;
        if (a > data[y]) {
            data[y] = (10 + data[y] - a) & 0xF;
            data[(y-1)&0xF] = 1;
        }else{
            data[y] -= a;
        }
    }
    regY = (regY-1) & 0xF;
    flag = true;
}

void cmd_demp()
{
    if (!locked) {
        byte a = regA;
        byte y = regY;
        do {
            a += data[y];
            if (a >= 10) {
                data[y] = (a-10) & 0xF;
                a = 1;
            }else{
                data[y] = a;
                a = 0;
            }
            y--;
            y &= 0xF;
        } while(a != 0);
    }
    regY = (regY-1) & 0xF;
    flag = true;
}

void cmd_cal()
{
    byte c = fetch();
    if (!flag) return;
    switch(c) {
        case 0x0:  cmd_rsto();  break;
        case 0x1:  cmd_setr();  break;
        case 0x2:  cmd_rstr();  break;
        case 0x3:  cmd_inpt();  break;
        case 0x4:  cmd_cmpl();  break;
        case 0x5:  cmd_chng();  break;
        case 0x6:  cmd_sift();  break;
        case 0x7:  cmd_ends();  break;
        case 0x8:  cmd_errs();  break;
        case 0x9:  cmd_shts();  break;
        case 0xA:  cmd_lons();  break;
        case 0xB:  cmd_sund();  break;
        case 0xC:  cmd_timr();  break;
        case 0xD:  cmd_dspr();  break;
        case 0xE:  cmd_demm();  break;
        case 0xF:  cmd_demp();  break;
    }
}

void cmd_init()
{
    regS = 0xF;
    regK = 0;
    mem_segment(5);
    led_binary(0);
    led_7seg(0,true);
    soundf(0);
    flag = true;
}

void cmd_push()
{
    mem_push(regA);
    flag = true;
}

void cmd_pop()
{
    regA = mem_pop();
    flag = true;
}

void cmd_inpa()
{
    regA = input_read(true);
    flag = true;
}

void cmd_not()
{
    flag = !flag;
}

void cmd_ram()
{
    mem_segment(regA);
    flag = true;
}

void cmd_lsft()
{
    byte a = regA;
    a <<= 1;
    if (!flag) a |= 1;
    regA = a&0xF;
    flag = (a < 16);
}

void cmd_cb()
{
    byte t = regA;
    regA   = regB;
    regB   = t;
    flag = true;
}

void cmd_and()
{
    regA &= data[regY];
    flag = (regA != 0);
}

void cmd_or()
{
    regA |= data[regY];
    flag = (regA != 0);
}

void cmd_xor()
{
    regA ^= data[regY];
    flag = (regA != 0);
}

void cmd_xsnd()
{
    if (regA == 0xF) {
        regK = regB;
    }else{
        sound(regA,(int)(regB+1)*60);
    }
    flag = true;
}

void cmd_tims()
{
    dev_wait((int)(regA+1)*10);
    flag = true;
}

// FFFF EEEE
// -gfe dcba
void cmd_dsp7()
{
    byte a = data[0xE] | (data[0xF] << 4);
    led_7seg(a,true);
    flag = true;
}

void cmd_sr()
{
    if (flag) {
        byte a = pc+3;
        mem_push((a>>4)&0xF);
        mem_push( a    &0xF);
    }
}

void cmd_ret()
{
    if (flag) {
        byte l = mem_pop();
        byte h = mem_pop();
        pc = (h<<4) | l;
    }
    flag = true;
}

void cmd_ext()
{
    byte c = fetch();
    switch(c) {
        case 0x0:  cmd_init();  break;
        case 0x1:  cmd_push();  break;
        case 0x2:  cmd_pop();   break;
        case 0x3:  cmd_inpa();  break;
        case 0x4:  cmd_not();   break;
        case 0x5:  cmd_ram();   break;
        case 0x6:  cmd_lsft();  break;
        case 0x7:  cmd_cb();    break;
        case 0x8:  cmd_and();   break;
        case 0x9:  cmd_or();    break;
        case 0xA:  cmd_xor();   break;
        case 0xB:  cmd_xsnd();  break;
        case 0xC:  cmd_tims();  break;
        case 0xD:  cmd_dsp7();  break;
        case 0xE:  cmd_sr();    break;
        case 0xF:  cmd_ret();   break;
    }
}

void cmd_jump()
{
    byte h = fetch();
    if (h == 6 && (!noext)) {
        cmd_ext();
        return;
    }
    byte l = fetch();
    if (flag) {
        pc = (h<<4) | l;
    }
    flag = true;
}

void cmd_exec()
{
    byte c = fetch();
    switch(c) {
        case 0x0:  cmd_ka();    break;
        case 0x1:  cmd_ao();    break;
        case 0x2:  cmd_ch();    break;
        case 0x3:  cmd_cy();    break;
        case 0x4:  cmd_am();    break;
        case 0x5:  cmd_ma();    break;
        case 0x6:  cmd_mp();    break;
        case 0x7:  cmd_mm();    break;
        case 0x8:  cmd_tia();   break;
        case 0x9:  cmd_aia();   break;
        case 0xA:  cmd_tiy();   break;
        case 0xB:  cmd_aiy();   break;
        case 0xC:  cmd_cia();   break;
        case 0xD:  cmd_ciy();   break;
        case 0xE:  cmd_cal();   break;
        case 0xF:  cmd_jump();  break;
    }
}

// program

void run_nop()
{
    while(keypad_read() < KEY_RESET) ;
    if (keypad_buf == KEY_CLEAR) return;
    keypad_beep();
}

void run_program(boolean incr, boolean adrs, boolean kbeep)
{
    int k = KEY_CLEAR;  // sentinel
    int k2;
    cpu_reset();
    while(true) {
        if (adrs) led_binary(pc);
WAIT_FOR_STEP:
        k2 = k;
        k = keypad_read();
        if (k >= KEY_RESET)  break;
        if (incr) {
            if (k2 == KEY_CLEAR) {
                ; // don't stop at first in step mode
            }else if (k == k2) {
                goto WAIT_FOR_STEP;
            }else if (k == KEY_RUN) {
                incr = false;
                adrs = false;
                kbeep = false;
            }else if (k == KEY_INCR) {
                if (kbeep) keypad_beep();
            }else{
                goto WAIT_FOR_STEP;
            }
        }else{
            if (k == KEY_INCR) {
                incr = true;
                adrs = true;
                kbeep = true;
                keypad_beep();
            }
        }
        cmd_exec();
        if (keypad_buf == KEY_CLEAR) break;
    }
}

void run_organ()
{
    int k = KEY_NONE;
    int k2;
    boolean c = true;
    if (!noext) {
        regK = 1;
    }
    while(true) {
        k2 = k;
        k = keypad_read();
        if (k >= KEY_RESET) break;
        if (k >= 16) k = -1;
        if (k < 0) {
            //led_7seg(0,true);
            sound(0);
            c = true;
        }else{
            if (noext) led_7seg(k);
            if (k != k2) c = true;
            if (k == 0) {
                if ((!noext) && c) {
                    if (regK > 1) regK--;
                    sound(1,1);
                    c = false;
                    led_7seg(regK);
                    led_binary(7,true);
                }
            }else if (k == 15) {
                if ((!noext) && c) {
                    if (regK < 0xE) regK++;
                    sound(0xE,1);
                    c = false;
                    led_7seg(regK);
                    led_binary(7,true);
                }
            }else if (c) {
                led_7seg(k);
                led_binary(7,false);
                sound(k);
                c = false;
            }
        }
    }
    sound(0);
}

void run_melody()
{
    byte a,b;
    byte saveK = regK;
    regK = 0;
    pc = 1;
    int t = (int)(1+mem[0])*15;
    while(keypad_read() < KEY_RESET) {
        if (pc == 0x65) {
            a = saveK;
            pc++;
        }else{
            a = mem[pc++];
        }
        if (pc == 0x65) {
            b = saveK;
            pc++;
        }else{
            b = mem[pc++];
        }
        if (a == 0xF) {
            if (b == 0) {
                pc = 1;
            }else if (b == 0xF) {
                pc -= 2;
            }else{
                regK = b;
            }
        }else{
            sound(a,(int)(1+b)*t);
        }
    }
    regK = saveK;
}

void run_quiz()
{
    byte note[16];
    byte s;
    byte i;
    byte j;
    int k2;
    int k;
    
    note[0] = 3;
    s = 0;
    i = j = 1;
    k = KEY_NONE;
    while(true) {
        k2 = k;
        k = keypad_read();
        if (k == KEY_CLEAR) break;
        if (s >= 16) {
            if (k == KEY_RESET) break;
            if (k != k2 && k == KEY_RUN) {
                led_7seg(0,true);
                s = 0;
                i = j = 1;
            }
        }else if (i <= s) {
            sound(note[i],300);
            if (keypad_buf == KEY_CLEAR) break;
            sound(0,100);
            if (keypad_buf == KEY_CLEAR) break;
            i++;
            if (i > s) {
                led_7seg(0,true);
                j = 0;
            }
        }else if (j <= s) {
            if (k == k2 || k == KEY_NONE || k > 0xF) {
                continue;
            }
            led_7seg(k);
            if (k == note[j]) {
                j++;
                sound(k,300);
                if (keypad_buf == KEY_CLEAR) break;
            }else if (k >= 1 && k <= 0xE) {
                cmd_errs();
                if (keypad_buf == KEY_CLEAR) break;
                led_7seg(s-1);
                s = 16;
            }
        }else{
            s++;
            if (s < 16) {
                note[s] = 1 + random(14);
                i = 0;
                sound(0,300);
                if (keypad_buf == KEY_CLEAR) break;
            }else{
                led_7seg(s-1);
                cmd_ends();
                if (keypad_buf == KEY_CLEAR) break;
            }
        }
    }
    keypad_click = false;
}

void run_mole()
{
    byte n;
    byte t;
    byte x;
    byte ts;
    byte score = 0xFE;
    int k = KEY_NONE;
    int k2;
    while(true) {
        k2 = k;
        k = keypad_read();
        if (k == KEY_CLEAR) break;
        if (score == 0xFF) {
            if (k == KEY_RESET) break;
            if (k != k2 && k == KEY_RUN) {
                led_7seg(0,true);
                score = 0xFE;
            }
        }else if (score == 0xFE) {
            if (k != k2 && (k&0xF3) == 0) {
                keypad_beep();
                switch(k) {
                    default:
                    case 0xC:  ts = 6;  break;
                    case 0x8:  ts = 7;  break;
                    case 0x4:  ts = 11;  break;
                    case 0x0:  ts = 15;  break;
                }
                n = 10;
                t = 0;
                score = 0;
                dev_wait(300);
                if (keypad_buf == KEY_CLEAR) break;
            }
        }else if (t > 0) {
            if (k != k2 && k >= 0 && k <= 6) {
                if (k == x) {
                    score++;
                    soundf(NOTE_C6,30);
                    if (keypad_buf == KEY_CLEAR) break;
                }
                t = 0;
                led_binary(x,false);
            }else{
                t--;
                if (t == 0) {
                    led_binary(x,false);
                }else{
                    dev_wait(120);
                    if (keypad_buf == KEY_CLEAR) break;
                }
            }
        }else if (n > 0) {
            dev_wait(600);
            if (keypad_buf == KEY_CLEAR) break;
            x = random(7);
            led_binary(x,true);
            n--;
            t = ts;
        }else{
            led_7seg(score);
            cmd_ends();
            score = 0xFF;
        }
    }
    keypad_click = false;
}

void run_tennis()
{
    byte x;
    boolean right;
    byte c;
    byte speed;
    byte hit;
    int sf;
    byte score = 0xFF;
    byte add;
    boolean pause = false;
    boolean vscom = false;
    int k = KEY_NONE;
    int k2;
    while(true) {
        k2 = k;
        k = keypad_read();
        if (k == KEY_CLEAR) break;
        if (k != k2 && k == 0xF) {
            vscom = !vscom;
            led_binary(7,vscom);
            soundf(NOTE_C6,30);
            if (keypad_buf == KEY_CLEAR) break;
        }
        if (pause) {
            if (k == KEY_RESET) break;
            if (k != k2 && k == KEY_RUN) {
                score = 0xFF;
                pause = false;
            }
            continue;
        }
        if (score == 0xFF) {
            x = 6;
            right = false;
            c = 0;
            speed = 0;
            hit = 7;
            score = 0;
            led_binary(0);
            led_binary(x,true);
            led_binary(7,vscom);
        }
        if (hit >= 7) {
            if (right) {
                if (vscom) {
                    hit = random(4);
                    if (hit > 0 && speed > random(10))  hit--;
                }else{
                    if ((k == 3 && k2 != 3) || (k == 1 && k2 == 0))  hit = x;
                }
            }else{
                if ((k == 0 && k2 != 0) || (k == 1 && k2 == 3))  hit = x;
            }
        }
        if (speed == 0) {
            if (hit < 7) {
                if (vscom && right) {
                    dev_wait((1+hit)*200);
                    if (keypad_buf == KEY_CLEAR) break;
                }
                speed = 4;
                c = 0;
                right = !right;
                hit = 7;
            }
            continue;
        }
        sf = 0;
        c++;
        if (c == speed) {
            c = 0;
            led_binary(x,false);
            sf = NOTE_C7;
            add = 0;
            if (right) {
                if (x > 0) {
                    x--;
                }else if (hit < 3) {
                    right = false;
                    if      (hit == 0) speed = 2;
                    else if (hit == 1) speed = 4;
                    else if (hit == 2) speed = 8;
                    sf = NOTE_C6;
                    hit = 7;
                }else{
                    add = 0x10;
                    hit = 7;
                }
            }else{
                if (x < 6) {
                    x++;
                }else if (hit > 3 && hit < 7) {
                    right = true;
                    if      (hit == 6) speed = 2;
                    else if (hit == 5) speed = 4;
                    else if (hit == 4) speed = 8;
                    sf = NOTE_C6;
                    hit = 7;
                }else{
                    add = 0x01;
                    hit = 7;
                }
            }
            led_binary(x,true);
            if (add != 0) {
                score += add;
                if (((score+add)&0x88) != 0) {
                    led_binary(score);
                    led_binary(7,vscom);
                    cmd_ends();
                    if (keypad_buf == KEY_CLEAR) break;
                    sf = 0;
                    pause = true;
                }else{
                    speed = 0;
                    cmd_errs();
                    if (keypad_buf == KEY_CLEAR) break;
                    sf = 0;
                }
            }
        }
        soundf(sf,30);
    }
    keypad_click = false;
}

void run_timer()
{
    byte m1 = mem[0];
    byte s2 = mem[1];
    byte s1 = mem[2];
    while(true) {
        if (keypad_read() == KEY_CLEAR) return;
        led_binary((m1<<4)|s2);
        led_7seg(s1);
        if (s1 > 0) {
            s1--;
        }else{
            s1 = 9;
            if (s2 > 0) {
                s2--;
            }else{
                s2 = 5;
                if (m1 > 0) {
                    m1--;
                }else{
                    break;
                }
            }
        }
        sound(1,5);
        sound(0,995);
    }
    cmd_ends();
    while(keypad_read() < KEY_RESET) ;
}

void run_morse()
{
    int k2;
    int k = KEY_NONE;
    byte a;
    int s,r;
    int t0 = (int)(1 + mem[0]) * 30;
    pc = 1;
    boolean ph = true;
    boolean pause = false;
    while(true) {
        k2 = k;
        k = keypad_read();
        if (k >= KEY_RESET)  break;
        if (pause) {
            if (k != k2 && k == KEY_RUN) {
                pause = false;
            }
            continue;
        }
        if (ph) {
            a = mem[pc++]&0xF;
            if (a >= 0xE) {
                if (a == 0xE) {
                    pause = true;
                }
                pc = 1;
                continue;
            }
        }else{
            a <<= 2;
        }
        ph = !ph;
        switch(a&B1100) {
            case B0000:  s = 1;  r = 1;  break;
            case B0100:  s = 3;  r = 1;  break;
            case B1000:  s = 0;  r = 2;  break;
            case B1100:  s = 0;  r = 6;  break;
        }
        dev_wait(r*t0,KEY_RESET);
        if (keypad_buf >= KEY_RESET) break;
        if (s > 0) {
            soundf(NOTE_C5);
            dev_wait(s*t0,KEY_RESET);
            soundf(0);
            if (keypad_buf >= KEY_RESET) break;
        }
    }
    sound(0);
}

int wait_number(byte s)
{
    boolean b = false;
    unsigned long t;
    unsigned long t1 = millis();
    unsigned long t2 = t1 + 10*1000;
    int k = KEY_NONE;
    int k2;
    do {
        t = millis();
        if (t >= t1) {
            t1 += 500;
            b = !b;
            if (b) led_7seg(s,true);
            else   led_7seg(0,true);
        }
        k2 = k;
        k = keypad_read();
        if (k >= KEY_RESET)  return -1;
        if ((k&0xF0) == 0) {
            keypad_beep();
            led_7seg((byte)k);
            dev_wait(500);
            if (keypad_read() >= KEY_RESET) return -1;
            led_7seg(0,true);
            dev_wait(100);
            if (keypad_read() >= KEY_RESET) return -1;
            return k;
        }
    } while(t < t2);
    return -1;
}

void run_save()
{
    if (noext) {
        run_program(false,true,false);
        return;
    }
    int i = wait_number(B0101101);  // 'S'
    if ((i&0xF0) == 0) {
        if (i <= 3) {
            mem_save(i);
        }else{
            mem_send();
        }
    }
}

void run_load()
{
    if (noext) {
        run_program(true,true,true);
        return;
    }
    int i = wait_number(B0111000);  // 'L'
    if ((i&0xF0) == 0) {
        if (i <= 3) {
            mem_load(i);
        }else{
            mem_receive();
        }
    }
}

void run_0()
{
    run_program(false,false,false);
}

void run_4()
{
    run_program(true,false,false);
}

void run_8()
{
    run_nop();
}

// ui

byte iadr;
byte idata;
byte ibuf;

void ui_reset()
{
    iadr = 0;
    idata = mem[iadr] & 0xF;
    ibuf = 0x00;
}

void key_data(byte k)
{
    ibuf <<= 4;
    ibuf &= 0xF0;
    idata = k & 0xF;
    ibuf |= idata;
}

void key_aset()
{
    iadr = ibuf;
    idata = mem[iadr] & 0xF;
}

void key_incr()
{
    mem[iadr] = idata & 0xF;
    iadr++;
    idata = mem[iadr] & 0xF;
}

void key_run()
{
    led_binary(0);
    led_7seg(0,true);
    randomSeed(millis()^analogRead(5));
    switch(idata) {
        case 0x0: run_0();                          break;
        case 0x1: run_program(false,false,false);   break;
        case 0x2: run_program(false,true, false);   break;
        case 0x3: run_save();                       break;
        case 0x4: run_4();                          break;
        case 0x5: run_program(true, false,false);   break;
        case 0x6: run_program(true, true, true );   break;
        case 0x7: run_load();                       break;
        case 0x8: run_8();                          break;
        case 0x9: run_organ();                      break;
        case 0xA: run_melody();                     break;
        case 0xB: run_quiz();                       break;
        case 0xC: run_mole();                       break;
        case 0xD: run_tennis();                     break;
        case 0xE: run_timer();                      break;
        case 0xF: run_morse();                      break;
    }
    if (keypad_buf == KEY_CLEAR) return;
    keypad_beep();
    key_reset();
}

void key_reset()
{
    iadr = 0;
    idata = mem[iadr] & 0xF;
}

void key_clear()
{
    soundf(0);
    led_binary(0);
    led_7seg(0,true);
    while(keypad_read() != KEY_NONE) ;
    dev_reset();
    sys_reset();
    ui_reset();
    delay(10);
}

void key_loop()
{
    int k = KEY_NONE;
    int k2;
    while(true) {
        led_binary(iadr);
        led_7seg(idata);
        k2 = k;
        k = keypad_read();
        if (k == KEY_CLEAR)  break;
        if (k != k2 && k != KEY_NONE) {
            keypad_beep();
            switch(k) {
                case KEY_ASET:  key_aset();         break;
                case KEY_INCR:  key_incr();         break;
                case KEY_RUN:   key_run();          break;
                case KEY_RESET: key_reset();        break;
                default:        key_data((byte)k);  break;
            }
            if (keypad_buf == KEY_CLEAR) break;
        }
    }
}

// main

void setup()
{
    int i;
    for(i = 0; i < 8; i++) {
        pinMode(P_BINARY+i,OUTPUT);
    }
    pinMode(P_SOUND,OUTPUT);
    for(i = 0; i < 7; i++) {
        pinMode(P_7SEG+i,OUTPUT);
    }
    pinMode(PD_KEYPAD,INPUT);
    pinMode(PD_INPT1,INPUT);
#if (P_SOUND != P_LSOUND)
    pinMode(P_SOUND,OUTPUT);
#endif
#if (PD_INPT2 != P_SOUND)
    pinMode(PD_INPT2,INPUT);
#endif
}

void loop()
{
    key_clear();
    key_loop();
}

/*
cmd mnemonic    flag    desc.
0   KA          0       keypad->A
                1       no keys
1   AO          1       A->7seg
2   CH          1       AY<->BZ
3   CY          1       A<->Y
4   AM          1       A->M[Y]
5   MA          1       M[Y]->A
6   M+          carry   A+M[Y]->A
7   M-          borrow  A-M[Y]->A
8?  TIA ?       1       (?)->A
9?  AIA ?       carry   A+(?)->A
A?  TIY ?       1       (?)->Y
B?  AIY ?       carry   Y+(?)->Y
C?  CIA ?       1       A!=(?)
                0       A==(?)
D?  CIY ?       1       Y!=(?)
                0       Y==(?)
E0  CAL RSTO    1       if (flag) 7seg off
E1  CAL SETR    1       if (flag) led(Y) on
E2  CAL RSTR    1       if (flag) led(Y) off
E3  CAL INPT    1       if (flag) input->A
E4  CAL CMPL    1       if (flag) ~A->A
E5  CAL CHNG    1       if (flag) ABYZ<->A'B'Y'Z'
E6  CAL SIFT    ~lsb    if (flag) A>>=1
E7  CAL ENDS    1       if (flag) sound("end")
E8  CAL ERRS    1       if (flag) sound("error")
E9  CAL SHTS    1       if (flag) sound("short")
EA  CAL LONS    1       if (flag) sound("long")
EB  CAL SUND    1       if (flag) sound(A)
EC  CAL TIMR    1       if (flag) sleep((A+1)*100mS)
ED  CAL DSPR    1       if (flag) M[F-E]->led(7-0)
EE  CAL DEM-    1       if (flag) dec(M[Y]-A)->M[Y],Y--
EF  CAL DEM+    1       if (flag) dec(M[Y]+A)->M[Y],Y--
F?? JUMP ??     1       if (flag) goto (??)
(extension)
F60 EXT INIT    1       F->SP,5->M,0->K
F61 EXT PUSH    1       A->stack
F62 EXT POP     1       stack->A
F63 EXT INPA    1       input(analog)->A
F64 EXT NOT     !flag   (!flag)->flag
F65 EXT RAM     1       A->data segment
F66 EXT LSFT    ~msb    A<<=1,A|=(!flag)
F67 EXT CB      1       A<->B
F68 EXT AND     (A!=0)  A&M[Y]->A
F69 EXT OR      (A!=0)  A|M[Y]->A
F6A EXT XOR     (A!=0)  A^M[Y]->A
F6B EXT XSND    1       sound(A,B) or B->key(if A=F)
F6C EXT TIMS    1       sleep((A+1)*10mS)
F6D EXT DSP7    1       M[F-E]->7seg(6-0)
F6E EXT SR      flag    if (flag) (PC+3)->stack
F6F EXT RET     1       if (flag) stack->PC
*/