// SPDX-License-Identifier: GPL-2.0+
//
// Small command line utility to control a CM6206 based USB sound card
// Copyright (C) 2019 Tommy Vestermark (tovsurf@vestermark.dk)
//
// Bulding:
// $ gcc cm6206ctl.c -l hidapi-libusb -o cm6206ctl
//
// Dependencies:
// - libhidapi-dev

#include <assert.h>
#include <err.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <hidapi/hidapi.h>

// USB ID's for C-Media Electronics CM6202
#define USB_VENDOR_ID   0x0d8c
#define USB_PRODUCT_ID  0x0102
#define NUM_REGS        6

// Default values for registers after reset
static const uint16_t REG_DEFAULT[NUM_REGS] = {
    0x2000,
    0x3002,
    0x6004,
    0x147f,     // Assuming 48 pin package is most normal
    0x0000,
    0x3000
};

//////// CM6206 USB read/write functions

int cm6206_read(hid_device *dev, uint8_t regnum, uint16_t *value) {
    uint8_t buf[5] = {0x00, // USB Report ID
            0x30,           // 0x30 = read, 0x20 = write
            0x00,           // DATAL
            0x00,           // DATAH
            regnum          // Register address
    };

    if (hid_write(dev, buf, sizeof(buf)) != sizeof(buf))
        return -1;

    if (hid_read(dev, buf, sizeof(buf)) != 3)
        return -2;

    if (buf[0] & 0xe0 != 0x20)    // No register data in the input report
        return -3;

    *value = (((uint16_t)buf[2]) << 8) | buf[1];
    return 0;
}

int cm6206_write(hid_device *dev, uint8_t regnum, uint16_t value) {
    uint8_t buf[5] = {0x00, // USB Report ID
            0x20,           // 0x30 = read, 0x20 = write
            (value & 0xff), // DATAL
            (value >> 8),   // DATAH
            regnum          // Register address
    };

    if (hid_write(dev, buf, sizeof(buf)) != sizeof(buf))
        return -1;

    return 0;
}

inline uint16_t newvalue(uint16_t oldvalue, uint16_t newmask, uint16_t newbits) {
    return (oldvalue & ~newmask) | (newbits & newmask);
}


/////// Printout of registers functions

#define ANSI_HEADER "\e[33m"    // Orange
#define ANSI_BOLD   "\e[1m"
#define ANSI_RESET  "\e[0m"
#define ANSI_TAB    "\e[44G"    // Column number

// Tuple for list of value/label pairs. Last tuple in list must have value -1 and a default label
typedef struct { int val; const char *label; } ValLabel;

// Print a header for the provided register
void print_reg_header(unsigned regnum, uint16_t regval) {
    const char *HILIGHT = (regval == REG_DEFAULT[regnum]) ? ANSI_RESET: ANSI_BOLD;
    printf("%s== REG%u ==%s\n", ANSI_HEADER, regnum, ANSI_RESET);
    printf("%sRaw value: 0x%04X%s       (Reset value: 0x%04X)\n", HILIGHT, regval, ANSI_RESET, REG_DEFAULT[regnum]);
}

// Print value of bit in register with provided label
void print_reg_bit_special(unsigned regnum, uint16_t regval, unsigned bit, const char *label, const char *valuetxt) {
    int isdefault = (regval>>bit & 1) == (REG_DEFAULT[regnum]>>bit & 1);
    const char *HILIGHT = (isdefault ? "" : ANSI_BOLD);
    printf("%s[%02u] %s%s %s%s\n", HILIGHT, bit, label, ANSI_TAB, valuetxt, ANSI_RESET);
}

// Print value of bit in register with provided on/off labels
void print_reg_bit_txt(unsigned regnum, uint16_t regval, unsigned bit, const char *label, const char *ontxt, const char* offtxt) {
    char strbuf[128];
    const char *statetxt = ((regval>>bit & 1) ? ontxt : offtxt);
    sprintf(strbuf, "%s", statetxt);
    if(0) {     // Verbose values
        sprintf(strbuf+strlen(strbuf), "\t{1=\"%s\", 0=\"%s\"}", ontxt, offtxt);
    }
    print_reg_bit_special(regnum, regval, bit, label, strbuf);
}

// Print value of bit in register with default on/off labels
void print_reg_bit_def(unsigned regnum, uint16_t regval, unsigned bit, const char *label) {
    print_reg_bit_txt(regnum, regval, bit, label, "Yes", "No");
}

// Print value of bit in register with provided label
void print_reg_bit_range(unsigned regnum, uint16_t regval, unsigned firstbit, unsigned numbits, const char *label, const char *valuetxt) {
    assert(numbits > 1);
    assert(firstbit + numbits <= 16);
    unsigned mask = 0xFFFF >> (16-numbits);
    int isdefault = (regval>>firstbit & mask) == (REG_DEFAULT[regnum]>>firstbit & mask);
    const char *HILIGHT = (isdefault ? "" : ANSI_BOLD);
    printf("%s[%02u:%02u] %s%s %s%s\n", HILIGHT, firstbit+numbits-1, firstbit, label, ANSI_TAB, valuetxt, ANSI_RESET);
}

// Print value of bit in register with provided labels
void print_reg_bit_range_label(unsigned regnum, uint16_t regval, unsigned firstbit, unsigned numbits, const char *label, const ValLabel *labels) {
    char strbuf[128];
    const char *valuetxt = NULL;
    int n=0;
    unsigned mask = 0xFFFF >> (16-numbits);
    while (labels[n].val >= 0) {
        if(labels[n].val == (regval>>firstbit & mask)) { // Does values match
            valuetxt = labels[n].label;
        }
        n++;
    }
    if(!valuetxt)   valuetxt = labels[n].label;     // Choose -1 as default label
    sprintf(strbuf, "%s", valuetxt);
    if(1) {     // Verbose values
        sprintf(strbuf+strlen(strbuf), "\t{");
        n=0;
        while (labels[n].val >= 0) {
            sprintf(strbuf+strlen(strbuf), "%u=\"%s\", ", labels[n].val, labels[n].label);
            n++;
        }
        sprintf(strbuf+(strlen(strbuf)-2), "}");
    }
    print_reg_bit_range(regnum, regval, firstbit, numbits, label, strbuf);
}





void print_cm6202_reg0(uint16_t val) {
    char *s = NULL;
    char sbuf[5];
    static const ValLabel SPDIF_OUT_HZ[] = {
        {0, "44.1 kHz"},    // Marked as reserved, but seems to work!
        {2, "48 kHz"},
        {3, "32 kHz"},      // Marked as reserved, but seems to work!
        {6, "96 kHz"},
        {-1, "Reserved"}
    };
    print_reg_header(0, val);
    print_reg_bit_txt(0, val, 15, "DMA Master", "SPDIF Out", "DAC");
    print_reg_bit_range_label(0, val, 12, 3, "SPDIF Out sample rate", SPDIF_OUT_HZ);
    sprintf(sbuf, "%u", (val>>4 & 0xFF));
    print_reg_bit_range(0, val, 4, 8, "Category code", sbuf);
    print_reg_bit_txt(0, val, 3, "Emphasis", "CD_Type", "None");
    print_reg_bit_txt(0, val, 2, "Copyright", "Not Asserted", "Asserted");
    print_reg_bit_txt(0, val, 1, "Non-audio", "non-PCM (e.g. AC3)", "PCM");
    print_reg_bit_txt(0, val, 0, "Professional/Consumer", "Professional", "Consumer");
}

void print_cm6202_reg1(uint16_t val) {
    print_reg_header(1, val);
    print_reg_bit_special(1, val, 15, "<Reserved>", "");
    print_reg_bit_txt(1, val, 14, "SEL Clk (test)", "22.58 MHz", "24.576 MHz");
    print_reg_bit_def(1, val, 13, "PLL binary search Enable");
    print_reg_bit_def(1, val, 12, "Soft Mute Enable");
    print_reg_bit_def(1, val, 11, "GPIO4 Out Status");
    print_reg_bit_def(1, val, 10, "GPIO4 Out Enable");
    print_reg_bit_def(1, val, 9, "GPIO3 Out Status");
    print_reg_bit_def(1, val, 8, "GPIO3 Out Enable");
    print_reg_bit_def(1, val, 7, "GPIO2 Out Status");
    print_reg_bit_def(1, val, 6, "GPIO2 Out Enable");
    print_reg_bit_def(1, val, 5, "GPIO1 Out Status");
    print_reg_bit_def(1, val, 4, "GPIO1 Out Enable");
    print_reg_bit_def(1, val, 3, "SPDIF Out Valid");
    print_reg_bit_def(1, val, 2, "SPDIF Loop-back Enable");
    print_reg_bit_def(1, val, 1, "SPDIF Out Disable");
    print_reg_bit_def(1, val, 0, "SPDIF In Mix Enable");
}

void print_cm6202_reg2(uint16_t val) {
    char *s = NULL;
    print_reg_header(2, val);
    print_reg_bit_def(2, val, 15, "Driver On");
    ValLabel HEADPHONE_SOURCES[] = {
        {0, "Side channels"},
        {1, "Surround channels"},
        {2, "Center and Subwoofer"},
        {3, "Front channels"},
        {-1,  "<Reserved>"}
    };
    print_reg_bit_range_label(2, val, 13, 2, "Headphone Source", HEADPHONE_SOURCES);
    print_reg_bit_def(2, val, 12, "Mute Headphone Right");
    print_reg_bit_def(2, val, 11, "Mute Headphone Left");
    print_reg_bit_def(2, val, 10, "Mute Rear Surround Right");
    print_reg_bit_def(2, val, 9, "Mute Rear Surround Left");
    print_reg_bit_def(2, val, 8, "Mute Side Surround Right");
    print_reg_bit_def(2, val, 7, "Mute Side Surround Left");
    print_reg_bit_def(2, val, 6, "Mute Subwoofer");
    print_reg_bit_def(2, val, 5, "Mute Center");
    print_reg_bit_def(2, val, 4, "Mute Front Right");
    print_reg_bit_def(2, val, 3, "Mute Front Left");
    print_reg_bit_def(2, val, 2, "BTL mode enable");
    ValLabel MCU_CLK_FREQS[] = {
        {0, "1.5 MHz"},
        {1, "3 MHz"},
        {-1, "<Reserved>"}
    };
    print_reg_bit_range_label(2, val, 0, 2, "MCU Clock Frequency", MCU_CLK_FREQS);
}

void print_cm6202_reg3(uint16_t val) {
    char *s = NULL;
    char sbuf[5];
    print_reg_header(3, val);
    print_reg_bit_range(3, val, 14, 2, "<Reserved>", "");
    sprintf(sbuf, "%u", (val>>11 & 7));
    print_reg_bit_range(3, val, 11, 2, "Sensitivity to FLY tuner volume", sbuf);
    print_reg_bit_txt(3, val, 10, "Microphone bias voltage", "2.25 V", "4.5 V");
    print_reg_bit_txt(3, val, 9, "Mix MIC/Line In to", "All 8 Channels", "Front Out Only");
    static const ValLabel SPDIF_IN_HZ[] = {
        {0, "44.1 kHz"},    // Marked as reserved, but seems to work!
        {2, "48 kHz"},
        {3, "32 kHz"},      // Marked as reserved, but seems to work!
        {-1, "Reserved"}
    };
    print_reg_bit_range_label(3, val, 7, 2, "SPDIF In sample rate", SPDIF_IN_HZ);
    print_reg_bit_txt(3, val, 6, "Package size", "48 pins", "100 pins");
    print_reg_bit_def(3, val, 5, "Front Out Enable");
    print_reg_bit_def(3, val, 4, "Rear Out Enable");
    print_reg_bit_def(3, val, 3, "Center Out Enable");
    print_reg_bit_def(3, val, 2, "Line Out Enable");
    print_reg_bit_def(3, val, 1, "Headphone Out Enable");
    print_reg_bit_def(3, val, 0, "SPDIF In can be recorded");
}

void print_cm6202_reg4(uint16_t val) {
    char *s = NULL;
    print_reg_header(4, val);
    print_reg_bit_def(4, val, 15, "GPIO12 Out Status");
    print_reg_bit_def(4, val, 14, "GPIO12 Out Enable");
    print_reg_bit_def(4, val, 13, "GPIO11 Out Status");
    print_reg_bit_def(4, val, 12, "GPIO11 Out Enable");
    print_reg_bit_def(4, val, 11, "GPIO10 Out Status");
    print_reg_bit_def(4, val, 10, "GPIO10 Out Enable");
    print_reg_bit_def(4, val, 9, "GPIO9 Out Status");
    print_reg_bit_def(4, val, 8, "GPIO9 Out Enable");
    print_reg_bit_def(4, val, 7, "GPIO8 Out Status");
    print_reg_bit_def(4, val, 6, "GPIO8 Out Enable");
    print_reg_bit_def(4, val, 5, "GPIO7 Out Status");
    print_reg_bit_def(4, val, 4, "GPIO7 Out Enable");
    print_reg_bit_def(4, val, 3, "GPIO6 Out Status");
    print_reg_bit_def(4, val, 2, "GPIO6 Out Enable");
    print_reg_bit_def(4, val, 1, "GPIO5 Out Enable");
    print_reg_bit_def(4, val, 0, "GPIO5 Out Status");
}

void print_cm6202_reg5(uint16_t val) {
    char *s = NULL;
    print_reg_header(5, val);
    print_reg_bit_range(5, val, 14, 2, "<Reserved>", "");
    print_reg_bit_def(5, val, 13, "DAC Not Reset");
    print_reg_bit_def(5, val, 12, "ADC Not Reset");
    print_reg_bit_def(5, val, 11, "ADC to SPDIF Out");
    ValLabel SPDIF_OUT_CHANNELS[] = {
        {0, "Front"},
        {1, "Side"},
        {2, "Center"},
        {3, "Rear"},
        {-1,  "<Reserved>"}
    };
    print_reg_bit_range_label(5, val, 9, 2, "SPDIF Out select", SPDIF_OUT_CHANNELS);
    print_reg_bit_txt(5, val, 8, "USB/CODEC Mode", "CODEC", "USB");
    print_reg_bit_def(5, val, 7, "DAC high pass filter");
    print_reg_bit_def(5, val, 6, "Loopback ADC to Rear DAC");
    print_reg_bit_def(5, val, 5, "Loopback ADC to Center DAC");
    print_reg_bit_def(5, val, 4, "Loopback ADC to Side DAC");
    print_reg_bit_def(5, val, 3, "Loopback ADC to Front DAC");
    ValLabel AD_FILTER_SOURCES[] = {
        {0, "Normal"},
        {4, "Front"},
        {5, "Side"},
        {6, "Center"},
        {7, "Rear"},
        {-1,  "<Reserved>"}
    };
    print_reg_bit_range_label(5, val, 0, 3, "Input source to AD digital filter", AD_FILTER_SOURCES);
}

////////

int main(void) {
    hid_device *hid_dev;
    uint16_t regbuf[NUM_REGS] = {0};

    if ( !(hid_dev = hid_open(USB_VENDOR_ID, USB_PRODUCT_ID, NULL)) )
        err(1, "Could not open USB device (hid_open: %ls)", hid_error(hid_dev));

    for(int n=0; n<NUM_REGS; n++) {
        if (cm6206_read(hid_dev, n, &regbuf[n]) < 0)
            err(2, "read: %ls, reg: %d", hid_error(hid_dev), n);
        // printf("REG%d=%04X\n", n, regbuf[n]);
    }

    print_cm6202_reg0(regbuf[0]);
    print_cm6202_reg1(regbuf[1]);
    print_cm6202_reg2(regbuf[2]);
    print_cm6202_reg3(regbuf[3]);
    print_cm6202_reg4(regbuf[4]);
    print_cm6202_reg5(regbuf[5]);


///// Write!! Set SPDIF sample rate
//    if (cm106_write(hid_dev, 0, newvalue(reg[0], 0x7000, 0x2000) < 0)
//        err(3, "write: %ls", hid_error(hid_dev));


///// Write!! Set SPDIF as master
//    if (cm106_write(hid_dev, 0, newvalue(reg[0], 0x8000, 0x8000)) < 0)
//        err(3, "write: %ls", hid_error(hid_dev));


///// Write!! Set DAC as master
//    if (cm106_write(hid_dev, 0, newvalue(reg[0], 0x8000, 0x0000)) < 0)
//        err(3, "write: %ls", hid_error(hid_dev));

    hid_close(hid_dev);
    hid_exit();
    return 0;
}
