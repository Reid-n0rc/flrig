// ----------------------------------------------------------------------------
// Copyright (C) 2026
//              David Freese, W1HKJ
//
// This file is part of flrig.
//
// flrig is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or
// (at your option) any later version.
//
// flrig is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://www.gnu.org/licenses/>.
// ----------------------------------------------------------------------------

#include <sstream>
#include "yaesu/FTX_1.h"
#include "debug.h"
#include "support.h"
#include "trace.h"

#define FTX_1_WAIT_TIME 200

enum mFTX_1 { // mode index
   mLSB, mUSB, mCW_U, mFM,  mAM, mRTTYL, mCW_L, mDATAL, mRTTYU, mDATAFM, mFMN, mDATAU, mAMN, mPSK, mDATAFMN, mC4FMDN, mC4FMVW };
//   0    1,      2,   3,    4,    5,     6,      7,       8,      9,     10,    11,    12,   13,     14,       15,      16

static const char FTX_1name_[] = "FTX-1";

static std::vector<std::string>FTX_1modes_;
static const char *vFTX_1modes_[] = {
"LSB",     "USB",     "CW-U",    "FM",      "AM", 
"RTTY-L",  "CW-L",    "DATA-L",  "RTTY-U",  "DATA-FM", 
"FM-N",    "DATA-U",  "AM-N",    "PSK",     "DATA-FM-N", 
"C4FM-DN", "C4FM-VW" };

static const char FTX_1_mode_chr[] =  {
 '1', '2', '3', '4', '5', 
 '6', '7', '8', '9', 'A',
 'B', 'C', 'D', 'E', 'F',
 'H', 'I' };

static const char FTX_1_mode_type[] = {
 'L', 'U', 'U', 'U', 'U', 
 'L', 'L', 'L', 'U', 'U', 
 'U', 'U', 'U', 'U', 'U',
 'U', 'U' };

static const int FTX_1_def_bw[] = {
       17,   17,   5,   0,    0,   10,     5,    16,     10,       0,    0,     16,    0,   16,        0,       0,       0 };
//   mLSB, mUSB, mCW, mFM,  mAM, mRTTYL, mCW_L, mDATAL, mRTTYU, mDATAFM, mFMN, mDATAU, mAMN, mPSK, mDATAFMN, mC4FMDN, mC4FMVW

static std::vector<std::string>FTX_1_widths_SSB;
static const char *vFTX_1_widths_SSB[] = {
 "300",  "400",  "600",  "850", "1100",
"1200", "1500", "1650", "1800", "1950",
"2100", "2250", "2400", "2450", "2500", 
"2600", "2700", "2800", "2900", "3000",
"3200", "3500", "4000" };
static int FTX_1_wvals_SSB[] = {
 1,  2,  3,  4,  5,  
 6,  7,  8,  9, 10,
11, 12, 13, 14, 15, 
16, 17, 18, 19, 20,
21, 22, 23, WVALS_LIMIT};

static std::vector<std::string>FTX_1_widths_DATA;
static const char *vFTX_1_widths_DATA[] = {
   "50",  "100",  "150",  "200",  "250",  
  "300",  "350",  "400",  "450",  "500",
  "600",  "800", "1200", "1400", "1700", 
 "2000", "2400", "3000", "3200", "3500",
 "4000" };
static int FTX_1_wvals_DATA[] = {
 1,  2,  3,  4,  5,  
 6,  7,  8,  9, 10,
11, 12, 13, 14, 15, 
16, 17, 18, 19, 20,
21, WVALS_LIMIT};

static std::vector<std::string>FTX_1_widths_CW;
static const char *vFTX_1_widths_CW[] = {
   "50",  "100",  "150",  "200",  "250",  
  "300",  "350",  "400",  "450",  "500",
  "600",  "800", "1200", "1400", "1700", 
 "2000", "2400", "3000", "3200", "3500",
 "4000" };
static int FTX_1_wvals_CW[] = {
 1,  2,  3,  4,  5,
 6,  7,  8,  9, 10,
11, 12, 13, 14, 15, 
16, 17, 18, 19, 20, 
21, WVALS_LIMIT};

// Single bandwidth modes, CAT manual Table 5
// AM, AM-N
static std::vector<std::string>FTX_1_widths_AM;
static const char *vFTX_1_widths_AM[]  = { "6000" };
static const int FTX_1_wvals_AM[] = { 0, WVALS_LIMIT };

// FM-N, DATA-FM-N
static std::vector<std::string>FTX_1_widths_FMN;
static const char *vFTX_1_widths_FMN[]  = { "9000" };
static const int FTX_1_wvals_FMN[] = { 0, WVALS_LIMIT };

// FM, DATA-FM, C4FM
static std::vector<std::string>FTX_1_widths_FM;
static const char *vFTX_1_widths_FM[]  = { "16000" };
static const int FTX_1_wvals_FM[] = { 0, WVALS_LIMIT };

// SD (CW break-in delay) and VD (VOX delay) codes 00 - 33, in msec
static const int FTX_1_DELAY_MSEC[] = {
	  30,   50,  100,  150,  200,  250,  300,  400,  500,  600,
	 700,  800,  900, 1000, 1100, 1200, 1300, 1400, 1500, 1600,
	1700, 1800, 1900, 2000, 2100, 2200, 2300, 2400, 2500, 2600,
	2700, 2800, 2900, 3000 };
static const int FTX_1_DELAY_CODES =
	sizeof(FTX_1_DELAY_MSEC) / sizeof(*FTX_1_DELAY_MSEC);

/// SD / VD code closest to msec
static int delay_code(int msec) {
	int best = 0;
	for (int code = 1; code < FTX_1_DELAY_CODES; code++) {
		if (abs(FTX_1_DELAY_MSEC[code] - msec) <
			abs(FTX_1_DELAY_MSEC[best] - msec)) {
			best = code;
		}
	}
	return best;
}

/// msec for an SD / VD code, or -1 if the code is out of range
static int delay_msec(int code) {
	if (code < 0 || code >= FTX_1_DELAY_CODES) {
		return -1;
	}
	return FTX_1_DELAY_MSEC[code];
}

//----------------------------------------------------------------------
static std::vector<std::string>FTX_1_att_labels;
static const char *vFTX_1_att_labels[] = { "ATT", "12 dB" };

static std::vector<std::string>FTX_1_pre_labels;
static const char *vFTX_1_pre_labels[] = { "Amp", "IPO" };
//----------------------------------------------------------------------

static GUI rig_widgets[]= {
	{ (Fl_Widget *)btnVol,        2, 125,  50 }, // 0
	{ (Fl_Widget *)sldrVOLUME,   54, 125, 368 }, // 1
	{ (Fl_Widget *)sldrRFGAIN,   54, 145, 156 }, // 2
	{ (Fl_Widget *)sldrSQUELCH, 266, 145, 156 }, // 3

	{ (Fl_Widget *)sldrMICGAIN,  54, 165, 156 }, // 4
	{ (Fl_Widget *)btnNotch,    214, 165,  50 }, // 5
	{ (Fl_Widget *)sldrNOTCH,   266, 165, 156 }, // 6

	{ (Fl_Widget *)btnNR,         2, 185,  50 }, // 7
	{ (Fl_Widget *)sldrNR,       54, 185, 156 }, // 8
	{ (Fl_Widget *)btnIFsh,     214, 185,  50 }, // 9
	{ (Fl_Widget *)sldrIFSHIFT, 266, 185, 156 }, // 10

	{ (Fl_Widget *)sldrPOWER,    54, 205, 368 }, // 11

	{ (Fl_Widget *)NULL,          0,   0,   0 }
};

RIG_FTX_1::RIG_FTX_1() {
// base class values
	IDstr = "ID";
	name_ = FTX_1name_;
	modes_ = FTX_1modes_;
	bandwidths_ = FTX_1_widths_SSB;
	bw_vals_ = FTX_1_wvals_SSB;

	widgets = rig_widgets;

	serial_baudrate = BR38400;
	stopbits = 1;
	serial_retries = 2;

	serial_write_delay = 0;
	serial_post_write_delay = 5;

	serial_timeout = 50;
	serial_rtscts = true;
	serial_rtsplus = false;
	serial_dtrplus = false;
	serial_catptt = true;
	serial_rtsptt = false;
	serial_dtrptt = false;

	A.imode = B.imode = modeB = modeA = def_mode = 1;
	A.iBW = B.iBW = bwA = bwB = def_bw = 12;
	A.freq = B.freq = freqA = freqB = def_freq = 14070000ULL;

	has_compression =
	has_compON =
	has_a2b =
	has_ext_tuner =
	has_xcvr_auto_on_off =
	has_split =
//	has_split_AB =
	has_noise_reduction =
	has_noise_reduction_control =
	has_extras =
	has_vox_onoff =
	has_vox_gain =
	has_vox_hang =
	has_vox_on_dataport =

	has_vfo_adj =

	has_cw_wpm =
	has_cw_keyer =
	has_cw_vol =
	has_cw_spot =
	has_cw_spot_tone =
	has_cw_qsk =
	has_cw_delay =
	has_cw_weight =
	has_cw_zero_in =

	has_band_selection =

	can_change_alt_vfo =
	has_smeter =
	has_alc_control =
	has_swr_control =
	has_power_out =
	has_power_control =
	has_volume_control =
	has_rf_control =
	has_sql_control =
	has_micgain_control =
	has_mode_control =
	has_nb_level =
	has_noise_control =
	has_bandwidth_control =
	has_notch_control =
	has_auto_notch =
	has_attenuator_control =
	has_preamp_control =
	has_ifshift_control =
	has_ptt_control =
	has_tune_control = true;

// derived specific
	atten_level = 1;
	preamp_level = 1;
	notch_on = false;
	m_60m_indx = 0;

	precision = 1;
	ndigits = 9;

}

void RIG_FTX_1::initialize()
{
	VECTOR (FTX_1modes_, vFTX_1modes_);
	VECTOR (FTX_1_widths_SSB, vFTX_1_widths_SSB);
	VECTOR (FTX_1_widths_DATA, vFTX_1_widths_DATA);
	VECTOR (FTX_1_widths_CW, vFTX_1_widths_CW);
	VECTOR (FTX_1_widths_AM, vFTX_1_widths_AM);
	VECTOR (FTX_1_widths_FMN, vFTX_1_widths_FMN);
	VECTOR (FTX_1_widths_FM, vFTX_1_widths_FM);

	VECTOR (FTX_1_att_labels, vFTX_1_att_labels);
	att_labels_ = FTX_1_att_labels;

	VECTOR (FTX_1_pre_labels, vFTX_1_pre_labels);
	pre_labels_ = FTX_1_pre_labels;

	modes_ = FTX_1modes_;
	bandwidths_ = FTX_1_widths_SSB;
	bw_vals_ = FTX_1_wvals_SSB;

	rig_widgets[0].W = btnVol;
	rig_widgets[1].W = sldrVOLUME;
	rig_widgets[2].W = sldrRFGAIN;
	rig_widgets[3].W = sldrSQUELCH;
	rig_widgets[4].W = sldrMICGAIN;
	rig_widgets[5].W = btnNotch;
	rig_widgets[6].W = sldrNOTCH;
	rig_widgets[7].W = btnNR;
	rig_widgets[8].W = sldrNR;
	rig_widgets[9].W = btnIFsh;
	rig_widgets[10].W = sldrIFSHIFT;
	rig_widgets[11].W = sldrPOWER;

// set progStatus defaults
	if (progStatus.notch_val < 10) progStatus.notch_val = 1500;
	if (progStatus.noise_reduction_val < 1) progStatus.noise_reduction_val = 1;
	if (progStatus.power_level < 5) progStatus.power_level = 5;
// first-time-thru, or reset
	if (progStatus.cw_qsk < 15) {
		progStatus.cw_qsk = 15;
		progStatus.cw_spot_tone = 700;
		progStatus.cw_weight = 3.0;
		progStatus.cw_wpm = 18;
		progStatus.vox_on_dataport = false;
		progStatus.vox_gain = 50;
		progStatus.vox_hang = 500;
	}
// Disable Auto Information mode
//	sendCommand("AI0;");

	op_yaesu_select60->deactivate();

}

void RIG_FTX_1::post_initialize()
{
}

bool RIG_FTX_1::check ()
{
	cmd = "ID;";
	get_trace(1, __func__);
	wait_char(';', 7, 500, __func__, ASC);
	gett("");
	if (replystr.rfind("ID") != std::string::npos) return true;
	return false;
}

/// The FTX-1 has two receivers.  flrig VFO A is the MAIN side and VFO B is
/// the SUB side.  VS selects which side is in use; FA/FB, MD0/MD1 and
/// SH0/SH1 always address MAIN and SUB directly.
char RIG_FTX_1::active_side() {
	if (inuse == onB) {
		return '1';
	}
	return '0';
}

unsigned long long RIG_FTX_1::get_vfoA ()
{
	cmd = rsp = "FA";
	cmd += ';';

	get_trace(1, __func__);
	wait_char(';',12, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind(rsp);
	if (p == std::string::npos) return freqA;
	p += 2;
	unsigned long long f = 0;
	for (int n = 0; n < ndigits; n++)
		f = f * 10 + replystr[p + n] - '0';
	freqA = f;
	return freqA;
}

void RIG_FTX_1::set_vfoA (unsigned long long freq)
{
	freqA = freq;
	cmd = "FA000000000;";
	for (int i = 0; i < ndigits; i++) {
		cmd[ndigits + 1 - i] += freq % 10;
		freq /= 10;
	}

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

unsigned long long RIG_FTX_1::get_vfoB ()
{
	cmd = rsp = "FB";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';',12, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind(rsp);
	if (p == std::string::npos) return freqB;
	p += 2;
	unsigned long long f = 0;
	for (int n = 0; n < ndigits; n++)
		f = f * 10 + replystr[p + n] - '0';
	freqB = f;
	return freqB;
}


void RIG_FTX_1::set_vfoB (unsigned long long freq)
{
	freqB = freq;
	cmd = "FB000000000;";
	for (int i = 0; i < ndigits; i++) {
		cmd[ndigits + 1 - i] += freq % 10;
		freq /= 10;
	}

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/// Read which side the radio is using, so a MAIN/SUB change made on the
/// front panel is followed by flrig.
int RIG_FTX_1::get_vfoAorB() {
	cmd = rsp = "VS";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 4, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos != std::string::npos && pos + 2 < replystr.length()) {
		if (replystr[pos + 2] == '1') {
			inuse = onB;
		} else {
			inuse = onA;
		}
	}
	return inuse;
}

void RIG_FTX_1::selectA()
{
	cmd = "VS0;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

	inuse = onA;
}

void RIG_FTX_1::selectB()
{
	cmd = "VS1;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

	inuse = onB;
}


void RIG_FTX_1::A2B()
{
	cmd = "AB;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::B2A()
{
	cmd = "BA;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::swapAB()
{
	cmd = "SV;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

bool RIG_FTX_1::can_split()
{
	return true;
}

/// Split is done by moving the transmitter to the other side with FT.
/// ST1 is not used: on the FTX-1 it locks the SUB side in transmit.
void RIG_FTX_1::set_split(bool val)
{
	split = val;
	bool tx_on_sub = (inuse == onA);
	if (!val) {
		tx_on_sub = !tx_on_sub;
	}
	if (tx_on_sub) {
		cmd = "FT1;";
	} else {
		cmd = "FT0;";
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int RIG_FTX_1::get_split()
{
	cmd = rsp = "FT";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 4, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 2 >= replystr.length()) {
		return split;
	}
	split = (replystr[pos + 2] != active_side());
	return split;
}

int RIG_FTX_1::get_smeter()
{
	cmd = "SM0;";
	cmd[2] = active_side();
	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("SM");
	if (p == std::string::npos) return 0;
	if (p + 6 >= replystr.length()) return 0;
	int mtr = atoi(&replystr[p+3]);
	mtr = mtr / 2.56;
	return mtr;
}

int RIG_FTX_1::get_swr()
{
	cmd = "RM6;";
	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("RM");
	if (p == std::string::npos) return 0;
	if (p + 6 >= replystr.length()) return 0;
	int mtr = atoi(&replystr[p+3]);
	return (int)ceil(mtr / 2.56);
}

int RIG_FTX_1::get_alc()
{
	cmd = "RM4;";
	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("RM");
	if (p == std::string::npos) return 0;
	if (p + 6 >= replystr.length()) return 0;
	int mtr = atoi(&replystr[p+3]);
	return (int)ceil(mtr / 2.56);
}

static meterpair pwrtbl[] = {
{0, 0},
{35, 5.0},
{59, 10.0},
{72, 15.0},
{86, 20.0},
{96, 25.0},
{107, 30.0},
{119, 35.0},
{129, 40.0},
{141, 45.0},
{150, 50.0},
{155, 55.0},
{161, 60.0},
{166, 65.0},
{173, 70.0},
{179, 75.0},
{184, 80.0},
{191, 85.0},
{196, 90.0},
{202, 95.0},
{208, 100.0}
};

int RIG_FTX_1::get_power_out()
{
	cmd = "RM5;";
	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("RM5");
	if (p == std::string::npos) return 0;
	if (p + 6 >= replystr.length()) return 0;

	int mtr = atoi(&replystr[p+3]);

//	mtr = (.06 * mtr) + (.002 * mtr * mtr);

	size_t i = 0;
	for (i = 0; i < sizeof(pwrtbl) / sizeof(*pwrtbl) - 1; i++)
		if (mtr >= pwrtbl[i].mtr && mtr < pwrtbl[i+1].mtr)
			break;
	int val = (int)ceil(
				 pwrtbl[i].val + 
				(pwrtbl[i+1].val - pwrtbl[i].val) * (mtr - pwrtbl[i].mtr) / (pwrtbl[i+1].mtr - pwrtbl[i].mtr));
	if (val > 100) val = 100;

	return val;
}

// Transceiver power level

static 	char SPA1 = '0';

void RIG_FTX_1::get_pc_min_max_step(double &min, double &max, double &step)
{
	if (SPA1 == '1') { // FTX-1 field head
		min = 5; max = 10; step = 1;
	} else { // SPA-1
		min = 5; max = 100; step = 1;
	}
}

double RIG_FTX_1::get_power_control()
{
	cmd = "PC;";
	get_trace(1, __func__);
	wait_char(';', 7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("PC");
	if (p == std::string::npos) return progStatus.power_level;

	int pc_ctl = 0;
	sscanf(&replystr[p], "PC%c%d;", &SPA1, &pc_ctl);

	return pc_ctl;
}

void RIG_FTX_1::set_power_control(double val)
{
	char strcmd[8];
	snprintf(strcmd, sizeof (strcmd), "PC%c%03d;",
		SPA1, (int)val);
	cmd = strcmd;
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

// Volume control return 0 ... 100
int RIG_FTX_1::get_volume_control()
{
	cmd = "AG0;";
	cmd[2] = active_side();
	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("AG");
	if (p == std::string::npos) return progStatus.volume;
	if (p + 6 >= replystr.length()) return progStatus.volume;
	int val = round(atoi(&replystr[p+3]) / 2.55);
	if (val > 100) val = 100;
	return ceil(val);
}

void RIG_FTX_1::set_volume_control(int val)
{
	int ivol = (int)(val * 2.55);
	cmd = "AG0000;";
	cmd[2] = active_side();
	for (int i = 5; i > 2; i--) {
		cmd[i] += ivol % 10;
		ivol /= 10;
	}

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

// Tranceiver PTT on/off
void RIG_FTX_1::set_PTT_control(int val)
{
	cmd = val ? "TX1;" : "TX0;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
	ptt_ = val;
}

int RIG_FTX_1::get_PTT()
{
	cmd = "TX;";

	get_trace(1, __func__);
	wait_char(';', 4, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("TX");
	if (p == std::string::npos) return ptt_;
	ptt_ =  (replystr[p+2] != '0' ? 1 : 0);

	return ptt_;
}


// internal or external tune mode
void RIG_FTX_1::tune_rig(int)
{
	cmd = "AC012;";
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int RIG_FTX_1::get_tune()
{
	cmd = "AC;";
	get_trace(1, __func__);
	wait_char(';', 5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("AC");
	if (p == std::string::npos) return 0;
	int val = replystr[p+4] - '0';
	return !(val < 2);
}

void RIG_FTX_1::set_attenuator(int val)
{
	if (val) cmd = "RA01;";
	else     cmd = "RA00;";

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int RIG_FTX_1::get_attenuator()
{
	cmd = "RA0;";
	get_trace(1, __func__);
	wait_char(';', 5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("RA");
	if (p == std::string::npos) return progStatus.attenuator;
	if (p + 3 >= replystr.length()) return progStatus.attenuator;
	atten_level = replystr[p+3] - '0';

	return atten_level;
}

void RIG_FTX_1::set_preamp(int val)
{
	if (val) cmd = "PA00;";
	else     cmd = "PA01;";
	preamp_level = val;
	preamp_state = (preamp_level == 0);

	set_trace(1, __func__);
	sendCommand (cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int RIG_FTX_1::get_preamp()
{
	cmd = "PA0;";
	get_trace(1, __func__);
	wait_char(';',5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("PA0");
	if (p == std::string::npos) return 0;

	preamp_state = preamp_level = (replystr[p+3] == '0');
	return preamp_level;
}

/// true for modes where the FTX-1 has one fixed bandwidth and SH does
/// not apply
bool RIG_FTX_1::fixed_width(int mode) {
	switch (mode) {
		case mAM:
		case mAMN:
		case mFM:
		case mFMN:
		case mDATAFM:
		case mDATAFMN:
		case mC4FMDN:
		case mC4FMVW:
			return true;
		default:
			return false;
	}
}

int RIG_FTX_1::adjust_bandwidth(int val)
{
	bandwidths_ = bwtable(val);
	switch (val) {
		case mCW_U:
		case mCW_L:
		case mRTTYL:
		case mRTTYU:
			bw_vals_ = FTX_1_wvals_CW;
			break;
		case mDATAL:
		case mDATAU:
		case mPSK:
			bw_vals_ = FTX_1_wvals_DATA;
			break;
		case mAM:
		case mAMN:
			bw_vals_ = FTX_1_wvals_AM;
			break;
		case mFMN:
		case mDATAFMN:
			bw_vals_ = FTX_1_wvals_FMN;
			break;
		case mFM:
		case mDATAFM:
		case mC4FMDN:
		case mC4FMVW:
			bw_vals_ = FTX_1_wvals_FM;
			break;
		default:
			bw_vals_ = FTX_1_wvals_SSB;
	}
	return FTX_1_def_bw[val];
}

int RIG_FTX_1::def_bandwidth(int val)
{
	return FTX_1_def_bw[val];
}

std::vector<std::string>& RIG_FTX_1::bwtable(int n)
{
	switch (n) {
		case mCW_U:
		case mCW_L:
		case mRTTYL:
		case mRTTYU:
			return FTX_1_widths_CW;
		case mDATAL:
		case mDATAU:
		case mPSK:
			return FTX_1_widths_DATA;
		case mAM:
		case mAMN:
			return FTX_1_widths_AM;
		case mFMN:
		case mDATAFMN:
			return FTX_1_widths_FMN;
		case mFM:
		case mDATAFM:
		case mC4FMDN:
		case mC4FMVW:
			return FTX_1_widths_FM;
		default:
			return FTX_1_widths_SSB;
	}
}

/// Set the width for one side.  NARROW is turned off first because with
/// NARROW on the FTX-1 uses the menu NAR WIDTH and the SH setting does not
/// hold.
void RIG_FTX_1::set_width(char side, int bw_index) {
	cmd = "NA00;";
	cmd[2] = side;
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

	cmd = "SH00";
	cmd[2] = side;
	cmd += '0' + bw_index / 10;
	cmd += '0' + bw_index % 10;
	cmd += ';';
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::set_modeA(int val)
{
	modeA = val;

	adjust_bandwidth(modeA);

	cmd = "MD0";
	cmd += FTX_1_mode_chr[val];
	cmd += ';';

	set_trace(1, "set_modeA()");
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, "SET mode A", cmd, replystr);

}

int RIG_FTX_1::get_modeA()
{
	cmd = "MD0;";
	get_trace(1, __func__);
	wait_char(';',5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("MD");
	if (p != std::string::npos) {
		if (p + 3 < replystr.length()) {
			int md = 0;
			switch (replystr[p+3]) {
				case '1': md = mLSB; break;
				case '2': md = mUSB; break;
				case '3': md = mCW_U; break;
				case '4': md = mFM; break;
				case '5': md = mAM; break;
				case '6': md = mRTTYL; break;
				case '7': md = mCW_L; break;
				case '8': md = mDATAL; break;
				case '9': md = mRTTYU; break;
				case 'A': md = mDATAFM; break;
				case 'B': md = mFMN; break;
				case 'C': md = mDATAU; break;
				case 'D': md = mAMN; break;
				case 'E': md = mPSK; break;
				case 'F': md = mDATAFMN; break;
				case 'H': md = mC4FMDN; break;
				case 'I': md = mC4FMVW; break;
			}
			modeA = md;
		}
	}

	adjust_bandwidth(modeA);

	return modeA;
}

void RIG_FTX_1::set_modeB(int val)
{
	modeB = val;

	adjust_bandwidth(modeB);

	cmd = "MD1";
	cmd += FTX_1_mode_chr[val];
	cmd += ';';

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

}

int RIG_FTX_1::get_modeB()
{

	cmd = "MD1;";
	get_trace(1, __func__);
	wait_char(';',5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("MD");
	if (p != std::string::npos) {
		if (p + 3 < replystr.length()) {
			int md = 0;
			switch (replystr[p+3]) {
				case '1': md = mLSB; break;
				case '2': md = mUSB; break;
				case '3': md = mCW_U; break;
				case '4': md = mFM; break;
				case '5': md = mAM; break;
				case '6': md = mRTTYL; break;
				case '7': md = mCW_L; break;
				case '8': md = mDATAL; break;
				case '9': md = mRTTYU; break;
				case 'A': md = mDATAFM; break;
				case 'B': md = mFMN; break;
				case 'C': md = mDATAU; break;
				case 'D': md = mAMN; break;
				case 'E': md = mPSK; break;
				case 'F': md = mDATAFMN; break;
				case 'H': md = mC4FMDN; break;
				case 'I': md = mC4FMVW; break;
			}
			modeB = md;
		}
	}

	adjust_bandwidth(modeB);
	return modeB;
}

void RIG_FTX_1::set_bwA(int val)
{
	bwA = val;
	if (fixed_width(modeA)) {
		return;
	}
	set_width('0', bw_vals_[val]);
}

int RIG_FTX_1::get_bwA()
{
	size_t p;

	if (fixed_width(modeA)) {
		bwA = 0;
		return bwA;
	}

	cmd = "SH0;";

	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

// response should be similar to
//   0123456
//   SH00nn;
//   nn = filter number 

	p = replystr.rfind("SH00");
	if (p == std::string::npos) return bwA;
	if (p + 6 >= replystr.length()) return bwA;

	int filnbr = 0;
	sscanf(&replystr[p], "SH00%d;", &filnbr);

	const int *idx = bw_vals_;
	int i = 0;
	while (*idx != WVALS_LIMIT) {
		if (*idx == filnbr) break;
		idx++;
		i++;
	}
	if (*idx == WVALS_LIMIT) i--;
	bwA = i;

	return bwA;
}

void RIG_FTX_1::set_bwB(int val)
{
	bwB = val;
	if (fixed_width(modeB)) {
		return;
	}
	set_width('1', bw_vals_[val]);
}

int RIG_FTX_1::get_bwB()
{
	size_t p;

	if (fixed_width(modeB)) {
		bwB = 0;
		return bwB;
	}
	cmd = "SH1;";
	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	p = replystr.rfind("SH10");
	if (p == std::string::npos) return bwB;
	if (p + 6 >= replystr.length()) return bwB;

	int filnbr = 0;
	sscanf(&replystr[p], "SH10%d;", &filnbr);

	const int *idx = bw_vals_;
	int i = 0;
	while (*idx != WVALS_LIMIT) {
		if (*idx == filnbr) break;
		idx++;
		i++;
	}
	if (*idx == WVALS_LIMIT) i--;
	bwB = i;

	return bwB;
}

int RIG_FTX_1::get_modetype(int n)
{
	return FTX_1_mode_type[n];
}

void RIG_FTX_1::set_if_shift(int val)
{
	char cmdstr[20];  // "IS00+0000;";
	if (inuse == onA)
		snprintf( cmdstr, sizeof(cmdstr), "IS00%+05d;", val );
	else
		snprintf(cmdstr, sizeof(cmdstr), "IS10%+05d;", val );

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

bool RIG_FTX_1::get_if_shift(int &val)
{
	if (inuse == onA)
		cmd = "IS0;";
	else
		cmd = "IS1;";
	get_trace(1, __func__);
	wait_char(';',10, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("IS");

	char sub = '0';
	int sh = progStatus.shift_val;
	sscanf(&replystr[p], "IS%c0%d;", &sub, &sh);

	val = sh;
	return true;
}

void RIG_FTX_1::set_notch(bool on, int val)
{
// set notch frequency
	if (on) {
		cmd = "BP00001;";
		cmd[2] = active_side();
		set_trace(1, __func__);
		sendCommand(cmd);
		sett("");
		showresp(WARN, ASC, __func__, cmd, replystr);
		cmd = "BP01000;";
		cmd[2] = active_side();
		if (val % 10 >= 5) val += 10;
		val /= 10;
		for (int i = 3; i > 0; i--) {
			cmd[3 + i] += val % 10;
			val /=10;
		}
		set_trace(1, "set notch value");
		sendCommand(cmd);
		sett("");
		showresp(WARN, ASC, "SET notch val", cmd, replystr);
		return;
	}

// set notch off
	cmd = "BP00000;";
	cmd[2] = active_side();
	set_trace(1, "set_notch OFF");
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, "SET notch off", cmd, replystr);
}

bool  RIG_FTX_1::get_notch(int &val)
{
	bool ison = false;
	cmd = "BP00;";
	cmd[2] = active_side();
	get_trace(1, __func__);
	wait_char(';',8, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("BP");
	if (p == std::string::npos) return ison;

	if (replystr[p+6] == '1') { // manual notch enabled
		ison = true;
		val = progStatus.notch_val;
		cmd = "BP01";
		cmd[2] = active_side();
		get_trace(1, "get notch value()");
		wait_char(';',8, FTX_1_WAIT_TIME, "get notch val", ASC);
		gett("");
		p = replystr.rfind("BP");
		if (p == std::string::npos)
			val = 10;
		else
			val = fm_decimal(replystr.substr(p+4), 3) * 10;
	}
	return ison;
}

void RIG_FTX_1::set_auto_notch(int v)
{
	cmd.assign("BC").append(1, active_side());
	cmd.append(v ? "1" : "0").append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int  RIG_FTX_1::get_auto_notch()
{
	cmd = "BC0;";
	cmd[2] = active_side();
	get_trace(1, __func__);
	wait_char(';',5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");
	size_t p = replystr.rfind("BC");
	if (p == std::string::npos) return 0;
	if (replystr[p+3] == '1') return 1;
	return 0;
}

void RIG_FTX_1::set_noise(bool b)
{
	if (b) cmd = "NR01;";
	else   cmd = "NR00;";
	set_trace(1, __func__);
	sendCommand (cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int RIG_FTX_1::get_noise()
{
	cmd = "NR0;";
	get_trace(1, __func__);
	wait_char(';',5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("NR0");
	if (p == std::string::npos) return 0;
	return replystr[p+3] - '0';
}

void RIG_FTX_1::set_nb_level(int val) 
{
	cmd = "RL000;";
	for (int i = 4; i > 2; i--) {
		cmd[i] += val % 10;
		val /= 10;
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int RIG_FTX_1::get_nb_level() 
{ 
	cmd = "RL0;";
	get_trace(1, __func__);
	wait_char(';', 7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");
	size_t p = replystr.rfind("RL0");
	if (p == std::string::npos) return 0;

	int val = atoi(&replystr[p+3]);
	return val;
}

void RIG_FTX_1::set_noise_reduction(int val)
{
	if (val) cmd = "NB01;";
	else     cmd = "NB00;";
	set_trace(1, __func__);
	sendCommand (cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int  RIG_FTX_1::get_noise_reduction()
{
	cmd = "NB0;";
	get_trace(1, __func__);
	wait_char(';', 5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("NB0");
	if (p == std::string::npos) return 0;
	return replystr[p+3] - '0';
}

void RIG_FTX_1::set_noise_reduction_val(int val)
{
	cmd = "NL0000;";
	for (int i = 5; i > 2; i--) {
		cmd[i] += val % 10;
		val /= 10;
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int  RIG_FTX_1::get_noise_reduction_val()
{
	cmd = "NL0;";
	get_trace(1, __func__);
	wait_char(';', 7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");
	size_t p = replystr.rfind("NL0");
	if (p == std::string::npos) return 0;

	int val = atoi(&replystr[p+3]);
	return val;
}

// val 0 .. 100
void RIG_FTX_1::set_mic_gain(int val)
{
	cmd = "MG000;";
	for (int i = 4; i > 1; i--) {
		cmd[i] = val % 10 + '0';
		val /= 10;
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int RIG_FTX_1::get_mic_gain()
{
	cmd = "MG;";
	get_trace(1, __func__);
	wait_char(';',6, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("MG");
	if (p == std::string::npos) return progStatus.mic_gain;
	int val = atoi(&replystr[p+2]);
	if (val > 100) val = 100;
	return ceil(val);
}

void RIG_FTX_1::set_rf_gain(int val)
{
	cmd = "RG0000;";
	cmd[2] = active_side();
	for (int i = 5; i > 2; i--) {
		cmd[i] = val % 10 + '0';
		val /= 10;
	}

	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int  RIG_FTX_1::get_rf_gain()
{
	int rfval = 0;
	cmd = "RG0;";
	cmd[2] = active_side();
	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("RG");
	if (p == std::string::npos) return progStatus.rfgain;
	for (int i = 3; i < 6; i++) {
		rfval *= 10;
		rfval += replystr[p+i] - '0';
	}
	return ceil(rfval);
}

void RIG_FTX_1::set_squelch(int val)
{
	char cmdstr[12];
	snprintf(cmdstr, sizeof(cmdstr), "%c%03d",
		(inuse == onA ? '0' : '1'), val * 255 / 100);
	cmd.assign("SQ").append(cmdstr).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int  RIG_FTX_1::get_squelch()
{
	if (inuse == onA)
		cmd = "SQ0;";
	else
		cmd = "SQ1;";
	get_trace(1, __func__);
	wait_char(';',7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("SQ");
	char sub_side = '0';
	int level = 0;
	sscanf(&replystr[p], "SQ%c%d;", &sub_side, &level);

	return level * 100 / 255;
}


// VX     VOX on/off
// VG     VOX GAIN 000 - 100
// VD     VOX DELAY, 2 digit code (see FTX_1_DELAY_MSEC)
// EX030510 VOX SELECT 0: MIC, 1: USB, 2: Bluetooth
// VG and VD apply to the input chosen by VOX SELECT.
// The FTX-1 has no anti-VOX setting available over CAT.

void RIG_FTX_1::set_vox_onoff()
{
	cmd = "VX0;";
	if (progStatus.vox_onoff) cmd[2] = '1';
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::set_vox_gain()
{
	cmd.assign("VG").append(to_decimal(progStatus.vox_gain, 3)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::set_vox_hang()
{
	cmd.assign("VD").append(to_decimal(delay_code(progStatus.vox_hang), 2));
	cmd.append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::set_vox_on_dataport()
{
	cmd = "EX0305100;";
	if (progStatus.vox_on_dataport) {
		cmd[8] = '1';
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::set_cw_wpm()
{
	cmd = "KS";
	if (progStatus.cw_wpm > 60) progStatus.cw_wpm = 60;
	if (progStatus.cw_wpm < 4) progStatus.cw_wpm = 4;
	cmd.append(to_decimal(progStatus.cw_wpm, 3)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

void RIG_FTX_1::set_cw_vol()
{
	if (progStatus.cw_vol == 0)
		cmd = "ML0000;";
	else {
		char cmdstr[20];
		snprintf(cmdstr, 19, "ML1%03d;", progStatus.cw_vol);
		cmd = cmdstr;
	}
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int  RIG_FTX_1::get_cw_vol()
{
	cmd = "ML1;";

	get_trace(1, __func__);
	wait_char(';', 7, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");
	size_t p = replystr.rfind("ML");
	if (p == std::string::npos) return progStatus.cw_vol;

	char mon = 0;
	int vol = 0;
	sscanf( &replystr[p], "ML%c%d;", &mon, &vol );
	if (mon == 0) vol = 0;
	progStatus.cw_vol = vol;
	return vol;
}

void RIG_FTX_1::enable_keyer()
{
	if (progStatus.enable_keyer)
		cmd = "KR1;";
	else
		cmd = "KR0;";
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int RIG_FTX_1::get_keyer()
{
	cmd = "KR;";
	get_trace(1, __func__);
	wait_char(';', 4, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");
	size_t p = replystr.rfind("KR");
	if (p != std::string::npos) return replystr[p+2] - '0';
	return 0;
}

bool RIG_FTX_1::set_cw_spot()
{
	if (vfo->imode == mCW_U || vfo->imode == mCW_L) {
		cmd = "CS0;";
		if (progStatus.spot_onoff) cmd[2] = '1';
		set_trace(1, __func__);
		sendCommand(cmd);
		sett("");
		showresp(WARN, ASC, __func__, cmd, replystr);
		return true;
	} else
		return false;
}

// EX020203 CW WEIGHT 25 - 45 (2.5 - 4.5)
void RIG_FTX_1::set_cw_weight()
{
	int weight = round(progStatus.cw_weight * 10);
	cmd.assign("EX020203").append(to_decimal(weight, 2)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

// EX020117 QSK DELAY TIME 0: 15, 1: 20, 2: 25, 3: 30 msec
void RIG_FTX_1::set_cw_qsk()
{
	int qsk_code = progStatus.cw_qsk / 5 - 3;
	if (qsk_code < 0) {
		qsk_code = 0;
	}
	if (qsk_code > 3) {
		qsk_code = 3;
	}
	cmd.assign("EX020117").append(to_decimal(qsk_code, 1)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int  RIG_FTX_1::get_cw_qsk()
{
	cmd = rsp = "EX020117";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 10, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 8 >= replystr.length()) {
		return progStatus.cw_qsk;
	}
	int qsk_code = replystr[pos + 8] - '0';
	if (qsk_code >= 0 && qsk_code <= 3) {
		progStatus.cw_qsk = (qsk_code + 3) * 5;
	}
	return progStatus.cw_qsk;
}

// SD CW break-in delay, 2 digit code
void RIG_FTX_1::set_cw_delay()
{
	cmd.assign("SD").append(to_decimal(delay_code(progStatus.cw_delay), 2));
	cmd.append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

int  RIG_FTX_1::get_cw_delay()
{
	cmd = rsp = "SD";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 5, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 4 >= replystr.length()) {
		return progStatus.cw_delay;
	}
	int msec = delay_msec(atoi(replystr.substr(pos + 2, 2).c_str()));
	if (msec > 0) {
		progStatus.cw_delay = msec;
	}
	return progStatus.cw_delay;
}

// 00: 300 Hz to 75: 1050 Hz (10Hz steps)
void RIG_FTX_1::set_cw_spot_tone()
{
	int n = progStatus.cw_spot_tone / 10 - 30;
	if (n < 0) n = 0;
	if (n > 75) n = 75;
	cmd.assign("KP").append(to_decimal(n, 2)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

/*
 * auto on/off not working well needs much work
*/

void RIG_FTX_1::set_xcvr_auto_on()
{
	if (!progStatus.xcvr_auto_on) return;

// This command requires dummy data be initially sent. Then after one
// second and before two seconds the command is sent.

	cmd = "PS1;"; // use as the dummy data
	sendCommand(cmd);
	update_progress(0);
	for (int i = 0; i < 1200; i += 100) {
		MilliSleep(100);
		update_progress(100 * i / 6000);
		Fl::awake();
	}

	cmd = "PS;";
	get_trace(1, "xcvr ON? ()");
	wait_char(';',4, 500, "Test: Is Rig ON", ASC);
	gett("");

	if (replystr.find("PS1;") != std::string::npos) {
		update_progress(0);
		return;
	}

	cmd = "PS1;";
	set_trace(1, "set xcvr ON()");
	sendCommand(cmd);
	sett("");

	for (int i = 1500; i < 6000; i += 100) {
		MilliSleep(100);
		update_progress(100 * i / 6000);
		Fl::awake();
	}
}

void RIG_FTX_1::set_xcvr_auto_off()
{
	if (!progStatus.xcvr_auto_off) return;

	cmd = "PS0;";
	set_trace(1, "set_xcvr_OFF()");
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, "SET xcvr auto on/off", cmd, replystr);
}


void RIG_FTX_1::set_compression(int on, int val)
{
	cmd = "PL";
	cmd.append(to_decimal(val, 3)).append(";");
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);

	// Can only send PR command in SSB mode.  Other modes will cause 891 to
	// return ?; in response to sending this
	int curMode = rigbase::isOnA() ? modeA : modeB;
	if ( curMode == mLSB || curMode == mUSB ) {
		if (on)
			cmd = "PR01;";
		else
			cmd = "PR00;";
		set_trace(2, "set Comp", cmd.c_str());
		set_trace(1, "set_comp_level");
		sendCommand(cmd);
		sett("");
		showresp(WARN, ASC, "set Comp", cmd, replystr);
	}
}

void RIG_FTX_1::get_compression(int &on, int &val)
{ 
	on = 0; val = 0;

	cmd = "PL;";
	get_trace(1, __func__);
	wait_char(';',6, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t p = replystr.rfind("PL");
	if (p == std::string::npos) return;
	val = atoi(&replystr[p+2]);
	if (val > 100) val = 100;
	val = ceil(val);

	// Can only send PR command in SSB mode.  Other modes will cause 891 to
	// return ?; in response to sending this
	int curMode = rigbase::isOnA() ? modeA : modeB;
	if ( curMode == mLSB || curMode == mUSB ) {
		cmd = "PR0;";
		get_trace(1, "get comp level");
		wait_char(';', 7, FTX_1_WAIT_TIME, "get PR level", ASC);
		gett("");
		size_t p = replystr.rfind("PR0");
		if (p == std::string::npos) return;

		on = replystr[p+3] - '0';
	}
	
	std::stringstream s;
	s << "get_compression: " << (on ? "ON" : "OFF") << "(" << on << "), comp PL=" << val;
	get_trace(1, s.str().c_str());
}


void RIG_FTX_1::get_band_selection(int v)
{
	if (v < 3) v = v - 1;
	cmd.assign("BS").append(to_decimal(v, 2)).append(";");
	get_trace(1, __func__);
	sendCommand(cmd);
	gett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
	get_trace(2, "get band", cmd.c_str());
}

// EX030113 REF FREQ ADJ -25 - +25, always signed: +05, -12
void RIG_FTX_1::setVfoAdj(double v)
{
	char cmdstr[20];
	snprintf(cmdstr, sizeof(cmdstr), "EX030113%+03d;", (int)v);
	cmd = cmdstr;
	set_trace(1, __func__);
	sendCommand(cmd);
	sett("");
	showresp(WARN, ASC, __func__, cmd, replystr);
}

double RIG_FTX_1::getVfoAdj()
{
	cmd = rsp = "EX030113";
	cmd += ';';
	get_trace(1, __func__);
	wait_char(';', 12, FTX_1_WAIT_TIME, __func__, ASC);
	gett("");

	size_t pos = replystr.rfind(rsp);
	if (pos == std::string::npos || pos + 11 >= replystr.length()) {
		return progStatus.vfo_adj;
	}
	return (double)(atoi(&replystr[pos + 8]));
}

void RIG_FTX_1::get_vfoadj_min_max_step(double &min, double &max, double &step)
{
	min = -25;
	max = 25;
	step = 1;
}

//----------------------------------------------------------------------
// AGC control
//----------------------------------------------------------------------

int  RIG_FTX_1::get_agc()
{
	cmd = "GT0;";
	cmd[2] = active_side();
	wait_char(';', 6, FTX_1_WAIT_TIME, __func__, ASC);
	gett(__func__);

	size_t p = replystr.rfind("GT");
	if (p == std::string::npos) return agcval;

	switch (replystr[3]) {
		default:
		case '0': agcval = 0; break;
		case '1': agcval = 1; break;
		case '2': agcval = 2; break;
		case '3': agcval = 3; break;
		case '4': case '5':
		case '6': agcval = 4; break;
	}
	return agcval;
}

int RIG_FTX_1::incr_agc()
{
static const char ch[] = {'0', '1', '2', '3', '4'};
	agcval++;
	if (agcval > 4) agcval = 0;
	cmd = "GT00;";
	cmd[2] = active_side();
	cmd[3] = ch[agcval];

	sendCommand(cmd);
	showresp(WARN, ASC, __func__, cmd, replystr);
	sett(__func__);

	return agcval;
}


static const char *agcstr[] = {"AGC", "FST", "MED", "SLO", "AUT"};
const char *RIG_FTX_1::agc_label()
{
	if (agcval < 0 || agcval > 4) return "AGC";
	return agcstr[agcval];
}

int  RIG_FTX_1::agc_val()
{
	return (agcval);
}
 
void  RIG_FTX_1::zero_in()
{
	sendCommand("ZI;");
}

const char * RIG_FTX_1::get_bwname_(int bw, int md) {
// read bw based on mode
	try {
		std::string sbw = bwtable(md).at(bw);
		return (bwtable(md).at(bw)).c_str();
	} catch (const std::exception& e) {
		LOG_ERROR("%s", e.what());
	}
	return "";
}

